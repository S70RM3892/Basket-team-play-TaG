#include "Character/TaGRig.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGArtLib.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	FVector SocketLoc(UStaticMesh* Mesh, const TCHAR* Name, const FVector& Fallback)
	{
		if (Mesh)
		{
			if (const UStaticMeshSocket* S = Mesh->FindSocket(FName(Name)))
			{
				return S->RelativeLocation;
			}
		}
		return Fallback;
	}

	float LerpAngle(float A, float B, float T)
	{
		return A + FMath::FindDeltaAngleRadians(A, B) * T;
	}

	constexpr float Deg(float Rad) { return Rad * 57.2957795f; }
}

UTaGRigComponent::UTaGRigComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTaGRigComponent::Clear()
{
	for (UStaticMeshComponent* P : Parts)
	{
		if (P)
		{
			P->DestroyComponent();
		}
	}
	for (USceneComponent* J : Joints)
	{
		if (J)
		{
			J->DestroyComponent();
		}
	}
	Parts.Reset();
	Joints.Reset();
	WristGlow.Reset();
	AnkleGlow.Reset();
	TeamMats.Reset();
}

USceneComponent* UTaGRigComponent::MakeJoint(USceneComponent* Parent, const TCHAR* Name, const FVector& Loc)
{
	USceneComponent* J = NewObject<USceneComponent>(GetOwner(), Name);
	J->SetupAttachment(Parent);
	J->SetRelativeLocation(Loc);
	J->RegisterComponent();
	return J;
}

UStaticMeshComponent* UTaGRigComponent::MakePart(USceneComponent* Joint, UStaticMesh* Mesh, const FTransform& Xf)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(GetOwner());
	C->SetupAttachment(Joint);
	C->SetStaticMesh(Mesh);
	C->SetRelativeTransform(Xf);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->RegisterComponent();
	Parts.Add(C);
	return C;
}

void UTaGRigComponent::Build(const FTaGHeroDef& Hero)
{
	Clear();
	Joints.SetNum(JointCount);
	HeightScale = Hero.EyeHeight / 175.f;

	const FString Base = FString::Printf(TEXT("/Game/Art/Heroes/%s/SM_%s_"), Hero.Key, Hero.Key);
	bArt = TaGArt::LoadMesh(Base + TEXT("Torso")) != nullptr && TaGArt::LoadMesh(Base + TEXT("Pelvis")) != nullptr;
	if (bArt)
	{
		BuildArt(Hero);
	}
	else
	{
		BuildFallback(Hero);
	}
	CurHipsY = 0.95f;
}

void UTaGRigComponent::BuildArt(const FTaGHeroDef& Hero)
{
	const FString Base = FString::Printf(TEXT("/Game/Art/Heroes/%s/SM_%s_"), Hero.Key, Hero.Key);
	auto M = [&](const TCHAR* Part) { return TaGArt::LoadMesh(Base + Part); };
	UStaticMesh* Pelvis = M(TEXT("Pelvis"));
	UStaticMesh* Torso = M(TEXT("Torso"));
	UStaticMesh* Head = M(TEXT("Head"));
	UStaticMesh* UpperL = M(TEXT("UpperArm_L"));
	UStaticMesh* UpperR = M(TEXT("UpperArm_R"));
	UStaticMesh* LowerL = M(TEXT("LowerArm_L"));
	UStaticMesh* LowerR = M(TEXT("LowerArm_R"));
	UStaticMesh* ThighL = M(TEXT("Thigh_L"));
	UStaticMesh* ThighR = M(TEXT("Thigh_R"));
	UStaticMesh* ShinL = M(TEXT("Shin_L"));
	UStaticMesh* ShinR = M(TEXT("Shin_R"));

	const float S = HeightScale;
	const FVector HipLLoc = SocketLoc(Pelvis, TEXT("hip_l"), FVector(0.f, -11.f * S, -2.f * S));
	const FVector KneeLoc = SocketLoc(ThighL, TEXT("knee"), FVector(0.f, 0.f, -45.f * S));
	const FVector SoleLoc = SocketLoc(ShinL, TEXT("sole"), FVector(0.f, 0.f, -46.f * S));
	PelvisRestZ = -(HipLLoc.Z + KneeLoc.Z + SoleLoc.Z);

	Joints[Pivot] = MakeJoint(this, TEXT("RigPivot"), FVector::ZeroVector);
	Joints[Hips] = MakeJoint(Joints[Pivot], TEXT("RigHips"), FVector(0.f, 0.f, PelvisRestZ));
	MakePart(Joints[Hips], Pelvis);
	Joints[Spine] = MakeJoint(Joints[Hips], TEXT("RigSpine"), SocketLoc(Pelvis, TEXT("spine"), FVector(0.f, 0.f, 5.f * S)));
	MakePart(Joints[Spine], Torso);
	Joints[Neck] = MakeJoint(Joints[Spine], TEXT("RigNeck"), SocketLoc(Torso, TEXT("neck"), FVector(0.f, 0.f, 66.f * S)));
	MakePart(Joints[Neck], Head);
	Joints[ShoulderL] = MakeJoint(Joints[Spine], TEXT("RigShoulderL"), SocketLoc(Torso, TEXT("shoulder_l"), FVector(0.f, -25.f * S, 55.f * S)));
	Joints[ShoulderR] = MakeJoint(Joints[Spine], TEXT("RigShoulderR"), SocketLoc(Torso, TEXT("shoulder_r"), FVector(0.f, 25.f * S, 55.f * S)));
	MakePart(Joints[ShoulderL], UpperL);
	MakePart(Joints[ShoulderR], UpperR);
	Joints[ElbowL] = MakeJoint(Joints[ShoulderL], TEXT("RigElbowL"), SocketLoc(UpperL, TEXT("elbow"), FVector(0.f, 0.f, -30.f * S)));
	Joints[ElbowR] = MakeJoint(Joints[ShoulderR], TEXT("RigElbowR"), SocketLoc(UpperR, TEXT("elbow"), FVector(0.f, 0.f, -30.f * S)));
	UStaticMeshComponent* ForeL = MakePart(Joints[ElbowL], LowerL);
	UStaticMeshComponent* ForeR = MakePart(Joints[ElbowR], LowerR);
	HandOffset[0] = SocketLoc(LowerL, TEXT("hand"), FVector(0.f, 0.f, -30.f * S));
	HandOffset[1] = SocketLoc(LowerR, TEXT("hand"), FVector(0.f, 0.f, -30.f * S));
	ForearmLen = -HandOffset[1].Z;
	Joints[HipL] = MakeJoint(Joints[Hips], TEXT("RigHipL"), HipLLoc);
	Joints[HipR] = MakeJoint(Joints[Hips], TEXT("RigHipR"), SocketLoc(Pelvis, TEXT("hip_r"), FVector(0.f, 11.f * S, -2.f * S)));
	MakePart(Joints[HipL], ThighL);
	MakePart(Joints[HipR], ThighR);
	Joints[KneeL] = MakeJoint(Joints[HipL], TEXT("RigKneeL"), KneeLoc);
	Joints[KneeR] = MakeJoint(Joints[HipR], TEXT("RigKneeR"), SocketLoc(ThighR, TEXT("knee"), FVector(0.f, 0.f, -45.f * S)));
	UStaticMeshComponent* ShinCL = MakePart(Joints[KneeL], ShinL);
	UStaticMeshComponent* ShinCR = MakePart(Joints[KneeR], ShinR);

	// Glow and team slots
	for (UStaticMeshComponent* C : { ForeL, ForeR })
	{
		if (C && C->GetMaterialIndex(TEXT("TaG")) != INDEX_NONE)
		{
			WristGlow.Add(TaGArt::PaintSlot(C, TEXT("TaG"), FLinearColor(0.1f, 0.1f, 0.1f), 0.f));
		}
	}
	for (UStaticMeshComponent* C : { ShinCL, ShinCR })
	{
		if (C && C->GetMaterialIndex(TEXT("TaG")) != INDEX_NONE)
		{
			AnkleGlow.Add(TaGArt::PaintSlot(C, TEXT("TaG"), FLinearColor(0.1f, 0.1f, 0.1f), 0.f));
		}
	}
	// Torso / head TaG bands (Gantetsu's joints, Hibiki's bell rim) follow the wrist glow
	for (UStaticMeshComponent* C : Parts)
	{
		if (C && C != ForeL && C != ForeR && C != ShinCL && C != ShinCR && C->GetMaterialIndex(TEXT("TaG")) != INDEX_NONE)
		{
			WristGlow.Add(TaGArt::PaintSlot(C, TEXT("TaG"), FLinearColor(0.1f, 0.1f, 0.1f), 0.f));
		}
		if (C && C->GetMaterialIndex(TEXT("Team")) != INDEX_NONE)
		{
			TeamMats.Add(TaGArt::PaintSlot(C, TEXT("Team"), FLinearColor::White, 0.f));
		}
	}
}

void UTaGRigComponent::BuildFallback(const FTaGHeroDef& Hero)
{
	// Engine shapes sized from the hero's body numbers (M body = Godot rig.gd proportions in cm).
	const float S = HeightScale;
	const float W = Hero.BodyWidth / 50.f;
	UStaticMesh* Cyl = TaGArt::EngineCylinder();
	UStaticMesh* Sph = TaGArt::EngineSphere();
	UStaticMesh* Cube = TaGArt::EngineCube();
	PelvisRestZ = 95.f * S;
	ForearmLen = 30.f * S;
	HandOffset[0] = HandOffset[1] = FVector(0.f, 0.f, -ForearmLen);

	auto Limb = [&](USceneComponent* J, float Len, float Thick, const FLinearColor& Color)
	{
		UStaticMeshComponent* C = MakePart(J, Cyl, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -Len * 0.5f), FVector(Thick / 100.f, Thick / 100.f, Len / 100.f)));
		TaGArt::PaintSlot(C, NAME_None, Color);
		return C;
	};
	auto Band = [&](USceneComponent* J, float Z, float Thick)
	{
		UStaticMeshComponent* C = MakePart(J, Cyl, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, Z), FVector(Thick * 1.25f / 100.f, Thick * 1.25f / 100.f, 0.045f * S)));
		return TaGArt::PaintSlot(C, NAME_None, FLinearColor(0.15f, 0.15f, 0.18f), 0.f);
	};

	const FLinearColor P = Hero.PrimaryColor;
	const FLinearColor Q = Hero.SecondaryColor;
	Joints[Pivot] = MakeJoint(this, TEXT("RigPivot"), FVector::ZeroVector);
	Joints[Hips] = MakeJoint(Joints[Pivot], TEXT("RigHips"), FVector(0.f, 0.f, PelvisRestZ));
	TaGArt::PaintSlot(MakePart(Joints[Hips], Cube, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.2f * S, 0.34f * W, 0.16f * S))), NAME_None, Q);
	Joints[Spine] = MakeJoint(Joints[Hips], TEXT("RigSpine"), FVector(0.f, 0.f, 5.f * S));
	UStaticMeshComponent* Chest = MakePart(Joints[Spine], Cyl, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 30.f * S), FVector(0.27f * S, 0.44f * W, 0.6f * S)));
	TeamMats.Add(TaGArt::PaintSlot(Chest, NAME_None, P));
	Joints[Neck] = MakeJoint(Joints[Spine], TEXT("RigNeck"), FVector(0.f, 0.f, 66.f * S));
	TaGArt::PaintSlot(MakePart(Joints[Neck], Sph, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 12.f * S), FVector(0.26f * S))), NAME_None, FLinearColor(0.85f, 0.66f, 0.52f));
	UStaticMeshComponent* Visor = MakePart(Joints[Neck], Cube, FTransform(FRotator::ZeroRotator, FVector(10.f * S, 0.f, 14.f * S), FVector(0.05f, 0.2f * S, 0.06f * S)));
	WristGlow.Add(TaGArt::PaintSlot(Visor, NAME_None, FLinearColor(0.05f, 0.05f, 0.08f), 0.f));

	Joints[ShoulderL] = MakeJoint(Joints[Spine], TEXT("RigShoulderL"), FVector(0.f, -25.f * S * W, 55.f * S));
	Joints[ShoulderR] = MakeJoint(Joints[Spine], TEXT("RigShoulderR"), FVector(0.f, 25.f * S * W, 55.f * S));
	Limb(Joints[ShoulderL], 30.f * S, 11.f * S * W, P);
	Limb(Joints[ShoulderR], 30.f * S, 11.f * S * W, P);
	Joints[ElbowL] = MakeJoint(Joints[ShoulderL], TEXT("RigElbowL"), FVector(0.f, 0.f, -30.f * S));
	Joints[ElbowR] = MakeJoint(Joints[ShoulderR], TEXT("RigElbowR"), FVector(0.f, 0.f, -30.f * S));
	Limb(Joints[ElbowL], 30.f * S, 10.f * S * W, Q);
	Limb(Joints[ElbowR], 30.f * S, 10.f * S * W, Q);
	WristGlow.Add(Band(Joints[ElbowL], -24.f * S, 10.f * S * W));
	WristGlow.Add(Band(Joints[ElbowR], -24.f * S, 10.f * S * W));

	Joints[HipL] = MakeJoint(Joints[Hips], TEXT("RigHipL"), FVector(0.f, -11.f * S * W, -2.f * S));
	Joints[HipR] = MakeJoint(Joints[Hips], TEXT("RigHipR"), FVector(0.f, 11.f * S * W, -2.f * S));
	Limb(Joints[HipL], 45.f * S, 16.f * S * W, Q * 0.5f);
	Limb(Joints[HipR], 45.f * S, 16.f * S * W, Q * 0.5f);
	Joints[KneeL] = MakeJoint(Joints[HipL], TEXT("RigKneeL"), FVector(0.f, 0.f, -45.f * S));
	Joints[KneeR] = MakeJoint(Joints[HipR], TEXT("RigKneeR"), FVector(0.f, 0.f, -45.f * S));
	Limb(Joints[KneeL], 44.f * S, 13.f * S * W, P);
	Limb(Joints[KneeR], 44.f * S, 13.f * S * W, P);
	for (USceneComponent* K : { Joints[KneeL], Joints[KneeR] })
	{
		TaGArt::PaintSlot(MakePart(K, Cube, FTransform(FRotator::ZeroRotator, FVector(5.f * S, 0.f, -44.f * S), FVector(0.26f * S, 0.11f * S * W, 0.07f * S))), NAME_None, FLinearColor(0.92f, 0.93f, 0.95f));
	}
	AnkleGlow.Add(Band(Joints[KneeL], -38.f * S, 13.f * S * W));
	AnkleGlow.Add(Band(Joints[KneeR], -38.f * S, 13.f * S * W));
}

void UTaGRigComponent::SetOwnerNoSeeAll(bool bNoSee)
{
	for (UStaticMeshComponent* C : Parts)
	{
		if (C)
		{
			C->SetOwnerNoSee(bNoSee);
		}
	}
}

void UTaGRigComponent::SetTeamColor(const FLinearColor& Color)
{
	for (UMaterialInstanceDynamic* M : TeamMats)
	{
		if (M)
		{
			M->SetVectorParameterValue(TEXT("Color"), bArt ? Color : (Color * 0.55f + FLinearColor(0.05f, 0.05f, 0.05f)));
		}
	}
}

void UTaGRigComponent::SetGlow(const FLinearColor& Color, float WristStrength, float AnkleStrength)
{
	auto Apply = [&](TArray<TObjectPtr<UMaterialInstanceDynamic>>& Mats, float Strength)
	{
		const FLinearColor Dim(0.12f, 0.12f, 0.14f);
		for (UMaterialInstanceDynamic* M : Mats)
		{
			if (M)
			{
				// The fallback shape material has no emissive input: brighten the base color instead.
				M->SetVectorParameterValue(TEXT("Color"), Strength > 0.5f ? Color : Dim);
				TaGArt::SetGlow(M, Color, Strength);
			}
		}
	};
	Apply(WristGlow, WristStrength);
	Apply(AnkleGlow, AnkleStrength);
}

FVector UTaGRigComponent::GetHandWorld(bool bRight) const
{
	const USceneComponent* E = Joints.IsValidIndex(bRight ? ElbowR : ElbowL) ? Joints[bRight ? ElbowR : ElbowL].Get() : nullptr;
	return E ? E->GetComponentTransform().TransformPosition(HandOffset[bRight ? 1 : 0]) : GetComponentLocation();
}

void UTaGRigComponent::Animate(const FTaGPoseInput& In)
{
	if (Joints.Num() != JointCount || !Joints[Hips])
	{
		return;
	}
	// Godot rig.gd works in m/s; speeds here are cm/s.
	const float Spd = In.Speed / 100.f;
	const float Fwd = In.ForwardSpeed / 100.f;
	const float Amp = FMath::Clamp(Spd / 5.f, 0.f, 1.f);
	if (Spd > 0.3f)
	{
		Phase += In.Dt * (5.f + Spd * 1.5f) * (Fwd >= -0.5f ? 1.f : -1.f);
	}
	const float Sw = FMath::Sin(Phase);

	float HipsY = 0.95f - 0.05f * FMath::Abs(Sw) * Amp;
	float Lean = 0.12f * Amp;
	float Spin = 0.f;
	float Tilt = 0.f;
	float ShL = -Sw * 0.6f * Amp + 0.1f, ShR = Sw * 0.6f * Amp + 0.1f;
	float ShzL = -0.12f, ShzR = 0.12f;
	float ElL = 0.5f, ElR = 0.5f;
	float HipL_ = Sw * 0.75f * Amp, HipR_ = -Sw * 0.75f * Amp;
	float KnL = -(0.15f + 0.9f * Amp * FMath::Max(0.f, -Sw));
	float KnR = -(0.15f + 0.9f * Amp * FMath::Max(0.f, Sw));

	const ETaGAction St = In.Action;
	const bool bNormal = St == ETaGAction::None || St == ETaGAction::Gathered || St == ETaGAction::PassThrow;
	if (In.Role == 2 && bNormal)
	{
		// Defensive stance: hips down, arms wide
		HipsY -= 0.1f; KnL -= 0.55f; KnR -= 0.55f; HipL_ += 0.35f; HipR_ += 0.35f;
		Lean = 0.25f; ShL = 0.5f; ShR = 0.5f; ShzL = -0.9f; ShzR = 0.9f; ElL = 0.4f; ElR = 0.4f;
	}
	if (In.Role == 1 && bNormal)
	{
		// Dribble: right hand follows the ball, left arm guards it
		const float Bob = FMath::Abs(FMath::Sin(In.DribbleT * 7.5f));
		ShR = 0.45f + 0.25f * Bob; ElR = 0.3f + 0.5f * Bob; ShzR = 0.25f;
		ShL = 0.9f; ElL = 0.9f; ShzL = -0.4f; Lean += 0.12f;
	}
	if (!In.bOnFloor && bNormal)
	{
		KnL = -1.f; KnR = -0.7f; HipL_ = 0.6f; HipR_ = 0.3f;
		if (In.Role != 1)
		{
			ShL = 2.95f; ShR = 2.95f; ElL = 0.1f; ElR = 0.1f; // hands up: a block
		}
	}

	switch (St)
	{
	case ETaGAction::Shoot:
	{
		const float K = FMath::Clamp(In.ShotCharge / 0.7f, 0.f, 1.f);
		const ETaGShotType Ty = In.ShotType;
		if (Ty == ETaGShotType::Layup || Ty == ETaGShotType::Euro || Ty == ETaGShotType::Dunk)
		{
			ShR = FMath::Lerp(1.2f, 3.f, K); ElR = FMath::Lerp(1.2f, 0.05f, K); ShL = 1.f; ElL = 0.8f;
			if (!In.bOnFloor)
			{
				HipL_ = 1.3f; KnL = -1.6f; HipR_ = 0.1f; KnR = -0.2f;
			}
			Lean = Ty == ETaGShotType::Dunk ? 0.3f : 0.15f;
		}
		else
		{
			ShL = FMath::Lerp(0.7f, 2.7f, K); ShR = FMath::Lerp(0.8f, 2.85f, K);
			ElL = FMath::Lerp(1.7f, 0.6f, K); ElR = FMath::Lerp(1.9f, 0.15f, K);
			ShzL = -0.15f; ShzR = 0.1f;
			if (In.bOnFloor && K < 0.5f)
			{
				KnL = -0.7f; KnR = -0.7f; HipL_ = 0.45f; HipR_ = 0.45f; HipsY = 0.85f;
			}
			else if (!In.bOnFloor)
			{
				KnL = -0.3f; KnR = -0.3f; HipL_ = 0.1f; HipR_ = 0.1f;
			}
			Lean = Ty == ETaGShotType::Fade ? -0.38f : (Ty == ETaGShotType::StepBack ? -0.15f : 0.f);
		}
		break;
	}
	case ETaGAction::ContestStart:
	case ETaGAction::Contest:
		ShL = 3.f; ShR = 3.f; ElL = 0.05f; ElR = 0.05f; ShzL = -0.15f; ShzR = 0.15f; Lean = 0.f;
		break;
	case ETaGAction::TackleStart:
	case ETaGAction::ArtStart:
		HipsY = 0.78f; KnL = -1.1f; KnR = -1.1f; HipL_ = 0.7f; HipR_ = 0.7f; Lean = 0.55f;
		ShL = 0.9f; ShR = 0.9f; ElL = 1.2f; ElR = 1.2f;
		break;
	case ETaGAction::Tackle:
	case ETaGAction::ArtActive:
		Lean = 0.85f; ShL = 1.5f; ShR = 1.5f; ElL = 0.3f; ElR = 0.3f; HipR_ = 0.9f; KnR = -0.8f; HipL_ = -0.4f;
		break;
	case ETaGAction::TackleRecover:
	case ETaGAction::ArtRecover:
		Lean = 0.6f; ShL = 0.4f; ShR = 0.4f;
		break;
	case ETaGAction::DriveStart:
	case ETaGAction::Drive:
		Lean = 0.45f;
		break;
	case ETaGAction::DodgeStart:
	case ETaGAction::Dodge:
		// Spin move
		Spin = 2.f * UE_PI * FMath::Clamp(In.ActionT / 0.36f, 0.f, 1.f) * In.DodgeSide;
		Lean = 0.2f;
		break;
	case ETaGAction::DodgeRecover:
		Lean = 0.25f; ShL = 0.2f; ShR = 0.2f;
		break;
	case ETaGAction::Stagger:
		Lean = -0.3f; ShL = 1.4f; ShR = 1.4f; ShzL = -1.f; ShzR = 1.f;
		break;
	case ETaGAction::StealRecover:
		ShR = 1.6f; ElR = 0.1f; Lean = 0.4f;
		break;
	case ETaGAction::Down:
		ShzL = -1.2f; ShzR = 1.2f; ShL = 0.4f; ShR = 0.4f; KnL = -0.3f; KnR = -0.6f;
		Tilt = 1.35f;
		break;
	case ETaGAction::Step:
		Lean = 0.2f;
		break;
	case ETaGAction::PassThrow:
		ShL = 1.5f; ShR = 1.5f; ElL = 0.2f; ElR = 0.2f; Lean = 0.2f;
		break;
	default:
		break;
	}
	if (In.bPretellTackle && bNormal)
	{
		HipsY = 0.82f; KnL = -1.f; KnR = -1.f; HipL_ = 0.6f; HipR_ = 0.6f; Lean = 0.5f;
	}
	else if (In.bPretellContest && bNormal)
	{
		ShL = 2.2f; ShR = 2.2f;
	}
	else if (In.bPretellDrive && bNormal)
	{
		HipsY = 0.86f; Lean = 0.65f; KnL = -0.8f; KnR = -0.4f; HipL_ = 0.6f; ShL = 0.7f; ShR = -0.5f;
	}
	else if (In.bPretellShot && bNormal)
	{
		ShL = 1.7f; ShR = 1.7f; ElL = 0.9f; ElR = 0.9f; KnL = -0.5f; KnR = -0.5f; HipsY = 0.9f;
	}

	const float R = FMath::Clamp(18.f * In.Dt, 0.f, 1.f);
	CurHipsY = FMath::Lerp(CurHipsY, HipsY, R);
	CurLean = LerpAngle(CurLean, Lean, R);
	CurTilt = LerpAngle(CurTilt, Tilt, R * 0.6f);
	const bool bSpinning = St == ETaGAction::Dodge || St == ETaGAction::DodgeStart;
	CurSpin = bSpinning ? Spin : LerpAngle(CurSpin, 0.f, R);
	const float Target[8] = { ShL, ShR, ShzL, ShzR, ElL, ElR, HipL_, HipR_ };
	for (int32 i = 0; i < 8; ++i)
	{
		Cur[i] = LerpAngle(Cur[i], Target[i], R);
	}
	CurKnee[0] = LerpAngle(CurKnee[0], KnL, R);
	CurKnee[1] = LerpAngle(CurKnee[1], KnR, R);

	Joints[Pivot]->SetRelativeRotation(FRotator(Deg(CurTilt), Deg(CurSpin), 0.f));
	Joints[Hips]->SetRelativeLocation(FVector(0.f, 0.f, PelvisRestZ + (CurHipsY - 0.95f) * 100.f * HeightScale));
	Joints[Spine]->SetRelativeRotation(FRotator(-Deg(CurLean), 0.f, 0.f));
	Joints[ShoulderL]->SetRelativeRotation(FRotator(Deg(Cur[0]), 0.f, Deg(Cur[2])));
	Joints[ShoulderR]->SetRelativeRotation(FRotator(Deg(Cur[1]), 0.f, Deg(Cur[3])));
	Joints[ElbowL]->SetRelativeRotation(FRotator(Deg(Cur[4]), 0.f, 0.f));
	Joints[ElbowR]->SetRelativeRotation(FRotator(Deg(Cur[5]), 0.f, 0.f));
	Joints[HipL]->SetRelativeRotation(FRotator(Deg(Cur[6]), 0.f, 0.f));
	Joints[HipR]->SetRelativeRotation(FRotator(Deg(Cur[7]), 0.f, 0.f));
	Joints[KneeL]->SetRelativeRotation(FRotator(Deg(CurKnee[0]), 0.f, 0.f));
	Joints[KneeR]->SetRelativeRotation(FRotator(Deg(CurKnee[1]), 0.f, 0.f));
}
