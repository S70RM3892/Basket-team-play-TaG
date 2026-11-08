#include "Character/TaGRig.h"
#include "Core/TaGRules.h"
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
	OutlineMats.Reset();
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
	UStaticMesh* FootL = M(TEXT("Foot_L"));
	UStaticMesh* FootR = M(TEXT("Foot_R"));
	bHasFeet = FootL && FootR;

	const float S = HeightScale;
	HipSock[0] = SocketLoc(Pelvis, TEXT("hip_l"), FVector(0.f, -11.f * S, -2.f * S));
	HipSock[1] = SocketLoc(Pelvis, TEXT("hip_r"), FVector(0.f, 11.f * S, -2.f * S));
	KneeSock[0] = SocketLoc(ThighL, TEXT("knee"), FVector(0.f, 0.f, -45.f * S));
	KneeSock[1] = SocketLoc(ThighR, TEXT("knee"), FVector(0.f, 0.f, -45.f * S));
	if (bHasFeet)
	{
		// Separate feet: the shin ends at the ankle and the foot keeps the sole level on the floor.
		ShinEndSock[0] = SocketLoc(ShinL, TEXT("ankle"), FVector(0.f, 0.f, -40.f * S));
		ShinEndSock[1] = SocketLoc(ShinR, TEXT("ankle"), FVector(0.f, 0.f, -40.f * S));
		SoleSock[0] = SocketLoc(FootL, TEXT("sole"), FVector(5.f * S, 0.f, -7.f * S));
		SoleSock[1] = SocketLoc(FootR, TEXT("sole"), FVector(5.f * S, 0.f, -7.f * S));
		ToeSock[0] = SocketLoc(FootL, TEXT("toe"), SoleSock[0] + FVector(12.f * S, 0.f, 0.f));
		ToeSock[1] = SocketLoc(FootR, TEXT("toe"), SoleSock[1] + FVector(12.f * S, 0.f, 0.f));
	}
	else
	{
		// One-piece shin and foot: the chain ends at the sole.
		ShinEndSock[0] = SocketLoc(ShinL, TEXT("sole"), FVector(0.f, 0.f, -46.f * S));
		ShinEndSock[1] = SocketLoc(ShinR, TEXT("sole"), FVector(0.f, 0.f, -46.f * S));
		SoleSock[0] = SoleSock[1] = ToeSock[0] = ToeSock[1] = FVector::ZeroVector;
	}
	PelvisRestZ = -(HipSock[0].Z + KneeSock[0].Z + ShinEndSock[0].Z + SoleSock[0].Z);

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
	Joints[HipL] = MakeJoint(Joints[Hips], TEXT("RigHipL"), HipSock[0]);
	Joints[HipR] = MakeJoint(Joints[Hips], TEXT("RigHipR"), HipSock[1]);
	MakePart(Joints[HipL], ThighL);
	MakePart(Joints[HipR], ThighR);
	Joints[KneeL] = MakeJoint(Joints[HipL], TEXT("RigKneeL"), KneeSock[0]);
	Joints[KneeR] = MakeJoint(Joints[HipR], TEXT("RigKneeR"), KneeSock[1]);
	UStaticMeshComponent* ShinCL = MakePart(Joints[KneeL], ShinL);
	UStaticMeshComponent* ShinCR = MakePart(Joints[KneeR], ShinR);
	Joints[AnkleL] = MakeJoint(Joints[KneeL], TEXT("RigAnkleL"), ShinEndSock[0]);
	Joints[AnkleR] = MakeJoint(Joints[KneeR], TEXT("RigAnkleR"), ShinEndSock[1]);
	if (bHasFeet)
	{
		MakePart(Joints[AnkleL], FootL);
		MakePart(Joints[AnkleR], FootR);
	}

	// Glow, team and outline slots
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
	// Other TaG bands (machine joints, a jelly bell rim) follow the wrist glow
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
		if (C && C->GetMaterialIndex(TEXT("Outline")) != INDEX_NONE)
		{
			OutlineMats.Add(TaGArt::PaintSlot(C, TEXT("Outline"), FLinearColor(0.05f, 0.05f, 0.07f), 0.f));
		}
	}
	FinishLegSetup();
}

void UTaGRigComponent::BuildFallback(const FTaGHeroDef& Hero)
{
	// Engine shapes sized from the body numbers of the hero (M body = Godot rig.gd proportions in cm).
	const float S = HeightScale;
	const float W = Hero.BodyWidth / 50.f;
	UStaticMesh* Cyl = TaGArt::EngineCylinder();
	UStaticMesh* Sph = TaGArt::EngineSphere();
	UStaticMesh* Cube = TaGArt::EngineCube();
	bHasFeet = true;
	HipSock[0] = FVector(0.f, -11.f * S * W, -2.f * S);
	HipSock[1] = FVector(0.f, 11.f * S * W, -2.f * S);
	KneeSock[0] = KneeSock[1] = FVector(0.f, 0.f, -45.f * S);
	ShinEndSock[0] = ShinEndSock[1] = FVector(0.f, 0.f, -44.f * S);
	SoleSock[0] = SoleSock[1] = FVector(5.f * S, 0.f, -3.5f * S);
	ToeSock[0] = ToeSock[1] = FVector(18.f * S, 0.f, -3.5f * S);
	PelvisRestZ = -(HipSock[0].Z + KneeSock[0].Z + ShinEndSock[0].Z + SoleSock[0].Z);
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

	Joints[HipL] = MakeJoint(Joints[Hips], TEXT("RigHipL"), HipSock[0]);
	Joints[HipR] = MakeJoint(Joints[Hips], TEXT("RigHipR"), HipSock[1]);
	Limb(Joints[HipL], 45.f * S, 16.f * S * W, Q * 0.5f);
	Limb(Joints[HipR], 45.f * S, 16.f * S * W, Q * 0.5f);
	Joints[KneeL] = MakeJoint(Joints[HipL], TEXT("RigKneeL"), KneeSock[0]);
	Joints[KneeR] = MakeJoint(Joints[HipR], TEXT("RigKneeR"), KneeSock[1]);
	Limb(Joints[KneeL], 44.f * S, 13.f * S * W, P);
	Limb(Joints[KneeR], 44.f * S, 13.f * S * W, P);
	Joints[AnkleL] = MakeJoint(Joints[KneeL], TEXT("RigAnkleL"), ShinEndSock[0]);
	Joints[AnkleR] = MakeJoint(Joints[KneeR], TEXT("RigAnkleR"), ShinEndSock[1]);
	for (USceneComponent* A : { Joints[AnkleL], Joints[AnkleR] })
	{
		TaGArt::PaintSlot(MakePart(A, Cube, FTransform(FRotator::ZeroRotator, FVector(8.f * S, 0.f, 0.f), FVector(0.26f * S, 0.11f * S * W, 0.07f * S))), NAME_None, FLinearColor(0.92f, 0.93f, 0.95f));
	}
	AnkleGlow.Add(Band(Joints[KneeL], -38.f * S, 13.f * S * W));
	AnkleGlow.Add(Band(Joints[KneeR], -38.f * S, 13.f * S * W));
	FinishLegSetup();
}

void UTaGRigComponent::FinishLegSetup()
{
	ThighLen = FMath::Max(KneeSock[0].Size(), 5.f);
	ShinLen = FMath::Max(ShinEndSock[0].Size(), 5.f);
	for (int32 i = 0; i < 2; ++i)
	{
		RestSole[i] = FVector(0.f, 0.f, PelvisRestZ) + HipSock[i] + KneeSock[i] + ShinEndSock[i] + SoleSock[i];
	}
	bFeetValid = false;
	bWasOnFloor = true;
	LegIKWeight = 1.f;
	HipDrop = 0.f;
	GaitPhase = 0.f;
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
	// Outlines carry the team colour (WORLD.md 7: TaG glow and outline tell ally from enemy)
	for (UMaterialInstanceDynamic* M : OutlineMats)
	{
		if (M)
		{
			M->SetVectorParameterValue(TEXT("Color"), Color * 0.9f);
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

// ================================================================ Legs: planted-foot gait

float UTaGRigComponent::FootForward(int32 Side, float Duty) const
{
	const float P = FMath::Frac(GaitPhase + (Side == 0 ? 0.f : 0.5f));
	if (P < Duty)
	{
		return 1.f - 2.f * P / FMath::Max(Duty, 0.01f);
	}
	const float Q = (P - Duty) / FMath::Max(1.f - Duty, 0.01f);
	return -1.f + 2.f * FMath::SmoothStep(0.f, 1.f, Q);
}

FVector UTaGRigComponent::ChainEndTarget(int32 Side) const
{
	const FFoot& F = Feet[Side & 1];
	const FVector SoleW = F.Current + FVector(0.f, 0.f, F.Lift);
	if (!bHasFeet)
	{
		return SoleW; // the shin mesh ends at the sole
	}
	const FQuat FootW = FRotator(F.Pitch, F.CurYaw, 0.f).Quaternion();
	if (F.bOnToe)
	{
		// Heel up: the foot rolls over the toe, which stays where a flat foot would put it.
		const FQuat Flat = FRotator(0.f, F.CurYaw, 0.f).Quaternion();
		const FVector ToeW = SoleW + Flat.RotateVector(ToeSock[Side & 1] - SoleSock[Side & 1]);
		return ToeW - FootW.RotateVector(ToeSock[Side & 1]);
	}
	return SoleW - FootW.RotateVector(SoleSock[Side & 1]);
}

void UTaGRigComponent::UpdateFeet(float Dt, const FVector& Vel, bool bOnFloor, float Duty, float Sweep, float Lift, float Widen, float OutYaw, float FreqOverride)
{
	const bool bFitted = FreqOverride >= 0.f; // a run-up times the steps itself
	const FTransform RigXf = GetComponentTransform();
	const float GroundZ = RigXf.GetLocation().Z;
	const float BodyYaw = RigXf.Rotator().Yaw;
	const float LegLen = ThighLen + ShinLen;
	auto Rest = [&](int32 i)
	{
		FVector W = RigXf.TransformPositionNoScale(FVector(RestSole[i].X, RestSole[i].Y + (i == 0 ? -Widen : Widen), 0.f));
		W.Z = GroundZ;
		return W;
	};
	auto RestYaw = [&](int32 i) { return BodyYaw + (i == 0 ? -OutYaw : OutYaw); };
	auto Reset = [&]()
	{
		for (int32 i = 0; i < 2; ++i)
		{
			FFoot& F = Feet[i];
			F.bPlanted = true;
			F.PlantPos = F.Current = F.SwingFrom = Rest(i);
			F.PlantYaw = F.CurYaw = F.SwingFromYaw = RestYaw(i);
			F.Lift = 0.f;
			F.Pitch = 0.f;
			F.bOnToe = false;
		}
		GaitPhase = 0.f;
		bFeetValid = true;
		bSettling = false;
	};

	if (!bOnFloor)
	{
		bWasOnFloor = false;
		bFeetValid = false;
		return;
	}
	if (!bFeetValid || !bWasOnFloor)
	{
		const bool bLanded = !bWasOnFloor;
		Reset();
		if (bLanded)
		{
			LandSquash = 1.f;
			StepCount++;
		}
	}
	bWasOnFloor = true;
	for (int32 i = 0; i < 2; ++i)
	{
		if (FVector::Dist2D(Feet[i].Current, Rest(i)) > 1.6f * LegLen)
		{
			Reset(); // teleported (a new possession)
			break;
		}
	}

	FVector V = Vel;
	V.Z = 0.f;
	const float Spd = V.Size();
	const FVector Dir = Spd > 1.f ? V / Spd : FVector::ZeroVector;

	// Standing still with the feet out of place (stopped mid-stride, turned on the spot): a few small steps.
	if (bFitted)
	{
		bSettling = false;
	}
	else if (Spd < 25.f && Feet[0].bPlanted && Feet[1].bPlanted)
	{
		float Err = 0.f;
		for (int32 i = 0; i < 2; ++i)
		{
			Err = FMath::Max(Err, FVector::Dist2D(Feet[i].PlantPos, Rest(i)) + 0.25f * FMath::Abs(FMath::FindDeltaAngleDegrees(Feet[i].PlantYaw, RestYaw(i))));
		}
		if (Err > 0.13f * LegLen)
		{
			bSettling = true;
		}
		else if (Err < 0.05f * LegLen)
		{
			bSettling = false;
		}
	}
	else if (Spd >= 25.f)
	{
		bSettling = false;
	}
	const bool bAnySwing = !Feet[0].bPlanted || !Feet[1].bPlanted;
	float SEff = Spd;
	if (bSettling || bAnySwing)
	{
		SEff = FMath::Max(SEff, 0.9f * LegLen);
	}
	// The stance foot sweeps back exactly as fast as the body moves forward: cycles/s = speed * duty / sweep.
	float Freq = 0.f;
	float UseSweep = Sweep;
	if (bFitted)
	{
		// The stance foot still sweeps back as fast as the body moves: the stride follows the fitted cadence
		Freq = FreqOverride;
		UseSweep = Freq > 0.05f ? FMath::Max(Spd * Duty / Freq, 2.f) : Sweep;
	}
	else if (Spd >= 25.f || bSettling || bAnySwing)
	{
		Freq = SEff * Duty / Sweep;
		if (Freq > 3.6f)
		{
			Freq = 3.6f; // very fast (drives): longer strides instead of even faster feet
			UseSweep = SEff * Duty / Freq;
		}
	}
	// A planted foot the body has run away from (sharp turns, shoves) is picked up at once.
	for (int32 i = 0; i < 2 && Freq > 0.f && !bFitted; ++i)
	{
		if (Feet[i].bPlanted && FVector::Dist2D(Feet[i].PlantPos, Rest(i)) > 0.7f * LegLen)
		{
			GaitPhase = FMath::Frac(Duty - (i == 0 ? 0.f : 0.5f));
			break;
		}
	}
	GaitPhase = FMath::Frac(GaitPhase + Freq * Dt);

	const float RunK = FMath::Clamp(Spd / (2.f * LegLen), 0.f, 1.f);
	for (int32 i = 0; i < 2; ++i)
	{
		FFoot& F = Feet[i];
		if (Freq <= 0.f)
		{
			F.Lift = FMath::FInterpTo(F.Lift, 0.f, Dt, 12.f);
			F.Pitch = FMath::FInterpTo(F.Pitch, 0.f, Dt, 12.f);
			F.bOnToe = false;
			continue;
		}
		const float P = FMath::Frac(GaitPhase + (i == 0 ? 0.f : 0.5f));
		const bool bStance = P < Duty;
		if (bStance && !F.bPlanted)
		{
			F.bPlanted = true;
			F.PlantPos = FVector(F.Current.X, F.Current.Y, GroundZ);
			F.PlantYaw = F.CurYaw;
			StepCount++;
			LastStepSide = i;
		}
		else if (!bStance && F.bPlanted)
		{
			F.bPlanted = false;
			F.SwingFrom = F.PlantPos;
			F.SwingFromYaw = F.PlantYaw;
		}
		if (F.bPlanted)
		{
			F.Current = F.PlantPos;
			F.CurYaw = F.PlantYaw;
			F.Lift = 0.f;
			// Running: the heel comes up late in the stance and the foot rolls onto the toe.
			const float Late = FMath::Clamp((P / Duty - 0.72f) / 0.28f, 0.f, 1.f) * RunK;
			F.bOnToe = bHasFeet && Late > 0.01f;
			F.Pitch = -26.f * Late;
		}
		else
		{
			const float Q = (P - Duty) / (1.f - Duty);
			const float TimeLeft = (1.f - Q) * (1.f - Duty) / Freq;
			// Land half a sweep ahead of where the hip will be at touchdown.
			const FVector Land = Rest(i) + V * TimeLeft + Dir * (UseSweep * 0.5f);
			const float E = Q * Q * Q * (Q * (Q * 6.f - 15.f) + 10.f);
			F.Current = FMath::Lerp(F.SwingFrom, Land, E);
			F.Current.Z = GroundZ;
			F.CurYaw = F.SwingFromYaw + FMath::FindDeltaAngleDegrees(F.SwingFromYaw, RestYaw(i)) * E;
			F.Lift = Lift * FMath::Sin(UE_PI * Q);
			F.bOnToe = false;
			F.Pitch = (-32.f * FMath::Max(0.f, 1.f - Q / 0.35f) + 10.f * FMath::SmoothStep(0.6f, 1.f, Q)) * RunK;
		}
	}
}

void UTaGRigComponent::SolveLeg(int32 Side, const FQuat& FKThigh, const FQuat& FKKnee)
{
	USceneComponent* HipJ = Joints[Side == 0 ? HipL : HipR];
	USceneComponent* KneeJ = Joints[Side == 0 ? KneeL : KneeR];
	USceneComponent* AnkJ = Joints[Side == 0 ? AnkleL : AnkleR];
	if (!HipJ || !KneeJ)
	{
		return;
	}
	const float W = bFeetValid ? LegIKWeight : 0.f;
	if (W <= 0.001f)
	{
		HipJ->SetRelativeRotation(FKThigh);
		KneeJ->SetRelativeRotation(FKKnee);
		if (AnkJ)
		{
			AnkJ->SetRelativeRotation(FQuat::Identity);
		}
		return;
	}
	const FFoot& F = Feet[Side];
	const FTransform HipsXf = Joints[Hips]->GetComponentTransform();
	const FVector Target = HipsXf.InverseTransformPositionNoScale(ChainEndTarget(Side)) - HipSock[Side];
	const float A = KneeSock[Side].Size();
	const float B = ShinEndSock[Side].Size();
	const float D = FMath::Clamp(Target.Size(), FMath::Abs(A - B) + 1.f, (A + B) * 0.999f);
	const FVector N = Target.GetSafeNormal(UE_SMALL_NUMBER, FVector(0.f, 0.f, -1.f));
	// The knee points where the foot points, a little outward.
	const FQuat FootYawInHips = HipsXf.GetRotation().Inverse() * FRotator(0.f, F.CurYaw, 0.f).Quaternion();
	const FVector Pole = FootYawInHips.RotateVector(FVector(1.f, Side == 0 ? -0.12f : 0.12f, 0.f));
	FVector H = FVector::CrossProduct(Pole, N).GetSafeNormal();
	if (H.IsNearlyZero())
	{
		H = FVector(0.f, 1.f, 0.f);
	}
	const FVector M = FVector::CrossProduct(N, H);
	const float CosA = FMath::Clamp((A * A + D * D - B * B) / (2.f * A * D), -1.f, 1.f);
	const float Alpha = FMath::Acos(CosA);
	const FVector T = N * FMath::Cos(Alpha) + M * FMath::Sin(Alpha); // hip -> knee
	FQuat QThigh = FRotationMatrix::MakeFromZY(-T, H).ToQuat();
	// The knee socket may sit a little off the joint axis: turn it onto T.
	const float PsiKnee = FMath::Atan2(KneeSock[Side].X, -KneeSock[Side].Z);
	QThigh = QThigh * FRotator(-FMath::RadiansToDegrees(PsiKnee), 0.f, 0.f).Quaternion();
	// Knee hinge (local pitch): swing the end of the shin onto the target.
	const FVector InThigh = QThigh.Inverse().RotateVector(Target) - KneeSock[Side];
	const float PsiWant = FMath::Atan2(InThigh.X, -InThigh.Z);
	const float PsiRest = FMath::Atan2(ShinEndSock[Side].X, -ShinEndSock[Side].Z);
	const FQuat QKnee = FRotator(FMath::RadiansToDegrees(PsiWant - PsiRest), 0.f, 0.f).Quaternion();
	HipJ->SetRelativeRotation(FQuat::Slerp(FKThigh, QThigh, W));
	KneeJ->SetRelativeRotation(FQuat::Slerp(FKKnee, QKnee, W));
	if (AnkJ)
	{
		// The foot keeps its own world orientation: flat on the floor, or toe-off / landing pitch in the air.
		const FQuat ShinW = KneeJ->GetComponentQuat();
		const FQuat Want = ShinW.Inverse() * FRotator(F.Pitch, F.CurYaw, 0.f).Quaternion();
		AnkJ->SetRelativeRotation(FQuat::Slerp(FQuat::Identity, Want, W));
	}
}

// ================================================================ Pose

void UTaGRigComponent::Animate(const FTaGPoseInput& In)
{
	if (Joints.Num() != JointCount || !Joints[Hips])
	{
		return;
	}
	// Long frames (hitches) still advance the gait by the real time, or the planted feet fall behind the body.
	const float Dt = FMath::Clamp(In.Dt, 0.f, 0.25f);
	IdleT += Dt;

	// ---------------- Travel: how fast, and which way relative to the body
	const FTransform RigXf = GetComponentTransform();
	FVector Vel = In.Velocity;
	Vel.Z = 0.f;
	const float Spd = Vel.Size();
	const float LegLen = FMath::Max(ThighLen + ShinLen, 10.f);
	const float SRel = Spd / LegLen; // leg lengths per second
	const float Run = FMath::Clamp((SRel - 1.f) / 3.5f, 0.f, 1.f);
	if (Spd > 15.f)
	{
		const FVector L = RigXf.InverseTransformVectorNoScale(Vel / Spd);
		const FVector2D Want = FVector2D(L.X, L.Y).GetSafeNormal();
		if (FVector2D::DotProduct(Want, MoveDirLocal) < -0.3f && FMath::Abs(Want.Y) > 0.7f && FMath::Abs(MoveDirLocal.Y) > 0.7f)
		{
			// A sideways reversal flips at once: blending would pass through forward for a few frames and turn the
			// level defensive slide into a bouncing run. Forward/back reversals keep the blend (their feet slip less).
			MoveDirLocal = Want;
		}
		else
		{
			const FVector2D Blend = MoveDirLocal + (Want - MoveDirLocal) * FMath::Clamp(Dt * 12.f, 0.f, 1.f);
			MoveDirLocal = Blend.IsNearlyZero() ? Want : Blend.GetSafeNormal();
		}
	}
	const float FwdW = FMath::Max(MoveDirLocal.X, 0.f);
	const float BackW = FMath::Max(-MoveDirLocal.X, 0.f);
	const float SideW = FMath::Abs(MoveDirLocal.Y);
	const float WSum = FwdW + BackW + SideW + 1e-3f;
	auto Mix = [&](float F, float B, float Sd) { return (FwdW * F + BackW * B + SideW * Sd) / WSum; };
	const float SideMix = SideW / WSum;
	const float Moving = FMath::Clamp(Spd / (0.5f * LegLen), 0.f, 1.f);

	// Stride: longer with speed; shorter when backpedalling, shortest sideways (a shuffle: the feet never cross).
	const float StepLen = LegLen * FMath::Min(0.4f + 0.15f * SRel, 1.2f) * Mix(1.f, 0.72f, 0.55f);
	const float Duty = FMath::Clamp(FMath::Lerp(0.62f, 0.38f, Run) + 0.1f * SideMix, 0.36f, 0.72f);
	const float Sweep = FMath::Max(2.f * Duty * StepLen, 2.f);
	const float Lift = LegLen * FMath::Lerp(0.07f, 0.2f, Run) * Mix(1.f, 0.6f, 0.45f);
	float Widen = LegLen * (0.03f + 0.07f * SideMix * Moving);
	if (In.Role == 2)
	{
		Widen += LegLen * 0.09f; // defensive stance: wide base
	}
	// ---------------- Run-up into a layup or dunk: the steps are timed so that the take-off foot (the one opposite the
	// finishing hand) lifts exactly when the body leaves the floor. The gait phase runs at the rate that gets there,
	// choosing the number of steps that keeps the cadence closest to the natural one for the speed.
	float FreqOverride = -1.f;
	if (In.RunUpLeft >= 0.f && In.bOnFloor && bFeetValid)
	{
		const float LiftOff = (In.bLeftHand ? 0.5f : 0.f) + Duty - 0.01f; // the take-off foot's last stance phase
		if (!bRunUp)
		{
			bRunUp = true;
			const float Natural = FMath::Clamp(FMath::Max(Spd, 0.9f * LegLen) * Duty / Sweep, 1.2f, 3.6f);
			float Best = 1e9f;
			RunUpPhaseLeft = FMath::Frac(LiftOff - GaitPhase) + 1.f;
			for (int32 n = 0; n < 4; ++n)
			{
				const float Left = FMath::Frac(LiftOff - GaitPhase) + n;
				const float F = Left / FMath::Max(In.RunUpLeft, 0.05f);
				if (Left < 0.1f || F > 4.5f)
				{
					continue;
				}
				// A foot lands each time the phase passes a half cycle; the layup's rhythm is "one, two, up"
				const int32 Downs = FMath::FloorToInt((GaitPhase + Left) * 2.f) - FMath::FloorToInt(GaitPhase * 2.f);
				const float Cost = FMath::Abs(F - Natural) + (Downs < 2 ? 0.6f * Natural : 0.f);
				if (Cost < Best)
				{
					Best = Cost;
					RunUpPhaseLeft = Left;
				}
			}
		}
		// Hold just before the lift-off if the body is still on the floor at the planned time (a late update)
		FreqOverride = In.RunUpLeft > 1e-3f ? FMath::Clamp(RunUpPhaseLeft / FMath::Max(In.RunUpLeft, Dt), 0.f, 8.f) : 0.f;
		const float Step = FMath::Min(FreqOverride * Dt, RunUpPhaseLeft);
		RunUpPhaseLeft -= Step;
		FreqOverride = Dt > 0.f ? Step / Dt : 0.f;
	}
	else
	{
		bRunUp = false;
	}
	UpdateFeet(Dt, Vel, In.bOnFloor, Duty, Sweep, Lift, Widen, 8.f, FreqOverride);
	const float FxL = FootForward(0, Duty) * Moving;
	const float FxR = FootForward(1, Duty) * Moving;
	const float Amp = FMath::Clamp(Spd / 500.f, 0.f, 1.f);
	const float ArmAmp = 0.65f * Amp * Mix(1.f, 0.6f, 0.15f);

	float HipsY = 0.95f;
	float Lean = (0.06f + 0.16f * Run) * (FwdW - 0.6f * BackW) / WSum * Moving;
	float SideLean = 0.1f * Run * MoveDirLocal.Y;
	float Spin = 0.f;
	float Tilt = 0.f;
	// Arms swing against the legs (left arm back when the left foot is forward); out for balance when shuffling.
	float ShL = -FxL * ArmAmp + 0.1f, ShR = -FxR * ArmAmp + 0.1f;
	float ShzL = -0.12f - 0.3f * SideMix * Moving, ShzR = 0.12f + 0.3f * SideMix * Moving;
	float ElL = 0.5f + 0.4f * Run, ElR = 0.5f + 0.4f * Run;
	// FK legs: only used while the IK is off (air, lunges, spins, falls)
	float HipL_ = 0.f, HipR_ = 0.f;
	float KnL = -0.15f, KnR = -0.15f;

	const ETaGAction St = In.Action;
	const bool bNormal = St == ETaGAction::None || St == ETaGAction::Gathered || St == ETaGAction::PassThrow;
	if (In.Role == 2 && bNormal)
	{
		// Defensive stance: hips down, arms wide
		HipsY -= 0.1f;
		Lean = 0.25f;
		ShL = 0.5f; ShR = 0.5f; ShzL = -0.9f; ShzR = 0.9f; ElL = 0.4f; ElR = 0.4f;
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
		if (Ty == ETaGShotType::Layup || Ty == ETaGShotType::Euro || Ty == ETaGShotType::Dunk || Ty == ETaGShotType::AlleyOop
			|| Ty == ETaGShotType::Floater)
		{
			// One-foot finishes. S = the finishing side, G = the other; Out > 0 swings the finishing arm outward
			const bool bLeft = In.bLeftHand;
			float& ShS = bLeft ? ShL : ShR; float& ElS = bLeft ? ElL : ElR; float& ShzS = bLeft ? ShzL : ShzR;
			float& ShG = bLeft ? ShR : ShL; float& ElG = bLeft ? ElR : ElL; float& ShzG = bLeft ? ShzR : ShzL;
			const float Out = bLeft ? -1.f : 1.f;
			const bool bTwo = Ty == ETaGShotType::Dunk || Ty == ETaGShotType::AlleyOop;
			if (In.bOnFloor)
			{
				// The gather: the ball in both hands at the chest on the finishing side, elbows in
				ShS = 0.7f; ElS = 1.75f; ShzS = 0.f;
				ShG = 0.95f; ElG = 1.7f; ShzG = 0.35f * Out; // the other hand reaches across to the ball
				Lean = 0.22f;
			}
			else
			{
				const float Rise = In.FinishRise;
				if (bTwo)
				{
					// Both hands take it up; an alley-oop is caught overhead and slammed down
					ShS = ShG = FMath::Lerp(1.6f, 2.95f, Rise);
					ElS = ElG = FMath::Lerp(1.3f, 0.15f, Rise);
					ShzS = 0.08f * Out; ShzG = -0.08f * Out;
					if (Ty == ETaGShotType::AlleyOop)
					{
						const float Down = FMath::Clamp(In.ActionT / 0.13f, 0.f, 1.f);
						ShS = ShG = FMath::Lerp(2.95f, 1.7f, Down);
						ElS = ElG = FMath::Lerp(0.1f, 0.35f, Down);
					}
				}
				else
				{
					// One hand carries it up and lays it off the fingertips; the other arm stays out in front
					ShS = FMath::Lerp(1.3f, 2.9f, Rise); ElS = FMath::Lerp(1.5f, 0.1f, Rise); ShzS = 0.06f * Out;
					ShG = 1.25f; ElG = 1.35f; ShzG = -0.25f * Out;
				}
				// The finishing side's knee drives up (the swing leg adds height to a one-foot take-off); the take-off
				// leg trails below
				float& HipS = bLeft ? HipL_ : HipR_; float& KnS = bLeft ? KnL : KnR;
				float& HipT = bLeft ? HipR_ : HipL_; float& KnT = bLeft ? KnR : KnL;
				HipS = 1.5f; KnS = -1.7f; HipT = -0.12f; KnT = -0.3f;
				Lean = FMath::Lerp(0.16f, 0.02f, Rise);
			}
		}
		else
		{
			ShL = FMath::Lerp(0.7f, 2.7f, K); ShR = FMath::Lerp(0.8f, 2.85f, K);
			ElL = FMath::Lerp(1.7f, 0.6f, K); ElR = FMath::Lerp(1.9f, 0.15f, K);
			ShzL = -0.15f; ShzR = 0.1f;
			if (In.bOnFloor && K < 0.5f)
			{
				HipsY = 0.85f; // gather: sink before the jump
			}
			else if (!In.bOnFloor)
			{
				KnL = -0.3f; KnR = -0.3f; HipL_ = 0.1f; HipR_ = 0.1f;
			}
			Lean = Ty == ETaGShotType::Fade ? -0.38f : (Ty == ETaGShotType::StepBack ? -0.15f : 0.f);
		}
		SideLean = 0.f;
		break;
	}
	case ETaGAction::ContestStart:
	case ETaGAction::Contest:
		ShL = 3.f; ShR = 3.f; ElL = 0.05f; ElR = 0.05f; ShzL = -0.15f; ShzR = 0.15f; Lean = 0.f;
		break;
	case ETaGAction::TackleStart:
	case ETaGAction::ArtStart:
		HipsY = 0.78f; Lean = 0.55f;
		HipL_ = 0.7f; HipR_ = 0.7f; KnL = -1.1f; KnR = -1.1f;
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
		HipsY = 0.86f; Lean = 0.5f;
		break;
	case ETaGAction::Drive:
		Lean = 0.45f + 0.1f * Run;
		break;
	case ETaGAction::DodgeStart:
	case ETaGAction::Dodge:
		// Spin move
		Spin = 2.f * UE_PI * FMath::Clamp(In.ActionT / 0.36f, 0.f, 1.f) * In.DodgeSide;
		Lean = 0.2f;
		KnL = -0.6f; KnR = -0.3f; HipL_ = 0.4f; HipR_ = -0.1f;
		break;
	case ETaGAction::DodgeRecover:
		Lean = 0.25f; ShL = 0.2f; ShR = 0.2f;
		break;
	case ETaGAction::Stagger:
		Lean = -0.3f; ShL = 1.4f; ShR = 1.4f; ShzL = -1.f; ShzR = 1.f;
		KnL = -0.5f; KnR = -0.2f; HipL_ = 0.3f; HipR_ = -0.2f;
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
	case ETaGAction::Leap:
		// Up at the lob: both arms reach for it, the right knee drives (a running one-foot take-off)
		ShL = 2.85f; ShR = 2.85f; ElL = 0.2f; ElR = 0.2f; ShzL = -0.12f; ShzR = 0.12f;
		HipR_ = 1.4f; KnR = -1.6f; HipL_ = -0.1f; KnL = -0.35f;
		Lean = -0.05f; SideLean = 0.f;
		break;
	default:
		break;
	}
	if (In.bPretellTackle && bNormal)
	{
		HipsY = 0.82f; Lean = 0.5f; HipL_ = 0.6f; HipR_ = 0.6f; KnL = -1.f; KnR = -1.f;
	}
	else if (In.bPretellContest && bNormal)
	{
		ShL = 2.2f; ShR = 2.2f;
	}
	else if (In.bPretellDrive && bNormal)
	{
		HipsY = 0.86f; Lean = 0.65f; ShL = 0.7f; ShR = -0.5f;
	}
	else if (In.bPretellShot && bNormal)
	{
		ShL = 1.7f; ShR = 1.7f; ElL = 0.9f; ElR = 0.9f; HipsY = 0.9f;
	}
	if (In.bBraced && bNormal)
	{
		HipsY = 0.8f; Lean = 0.35f; ShL = 1.2f; ShR = 1.2f; ElL = 1.9f; ElR = 1.9f; ShzL = 0.2f; ShzR = -0.2f;
	}
	// Idle: a slow breath when standing
	Lean += 0.015f * FMath::Sin(IdleT * 1.7f) * (1.f - Moving);

	// Legs follow the IK on the floor; lunges, spins, falls and jumps use the posed (FK) legs.
	bool bLegIK = In.bOnFloor;
	switch (St)
	{
	case ETaGAction::Down:
	case ETaGAction::Stagger:
	case ETaGAction::DodgeStart:
	case ETaGAction::Dodge:
	case ETaGAction::Tackle:
	case ETaGAction::ArtActive:
		bLegIK = false;
		break;
	default:
		break;
	}
	LegIKWeight = FMath::FInterpConstantTo(LegIKWeight, bLegIK ? 1.f : 0.f, Dt, 7.f);

	// ---------------- Body motion locked to the steps
	// Walking vaults over a stiff stance leg (inverted pendulum): the pelvis is lowest in double support and highest at
	// mid-stance (Saunders, Inman & Eberhart 1953; Cavagna, Heglund & Taylor 1977). Running bounces on a compressing
	// leg (spring-mass): lowest at mid-stance, highest in flight (Blickhan 1989; McMahon & Cheng 1990). People change
	// gait near a Froude number v^2/(gL) of 0.5 (Alexander 1989). Walking moves the pelvis about 4-5 cm up and down and
	// running about 6-10 cm; the body sways over the stance foot, the pelvis turns about 4 degrees with the leading leg
	// and drops on the swing side (Perry & Burnfield 2010). The speed also pulses within each step instead of gliding.
	const float Froude = Spd * Spd / (980.f * LegLen);
	const float RunGait = FMath::SmoothStep(0.38f, 0.62f, Froude);
	const float Pace = FMath::Clamp(Froude / 0.3f, 0.f, 1.f);   // a stroll barely bobs
	const float TwoPi = 2.f * UE_PI;
	const float StepS = FMath::Frac(2.f * GaitPhase);           // 0 at each touchdown
	const float WalkLow = Duty - 0.5f;                           // middle of the double support, in steps
	const float RunLow = Duty;                                   // mid-stance, in steps
	const float WalkDip = 0.5f + 0.5f * FMath::Cos(TwoPi * (StepS - WalkLow));
	const float RunDip = 0.5f + 0.5f * FMath::Cos(TwoPi * (StepS - RunLow));
	GaitWeight = FMath::FInterpTo(GaitWeight, In.bOnFloor ? Moving * LegIKWeight : 0.f, Dt, 8.f);
	const float Fore = 1.f - SideMix;                            // sway, surge, turn and tilt belong to forward/back travel
	const float SideK = FMath::SmoothStep(0.6f, 0.95f, SideMix); // a defensive slide keeps the head level
	const float BobZ = -LegLen * GaitWeight * (1.f - 0.6f * SideK) * FMath::Lerp(0.015f + 0.05f * Pace * WalkDip, 0.035f + 0.085f * RunDip, RunGait);
	const float StanceL = FMath::Cos(TwoPi * (GaitPhase - 0.5f * Duty)); // +1 at mid-stance on the left foot, -1 on the right
	const float SwayY = -LegLen * GaitWeight * Fore * FMath::Lerp(0.022f, 0.01f, RunGait) * StanceL;
	// Walking is fastest in double support, running slowest at mid-stance: a small surge along the travel
	const float Surge = LegLen * GaitWeight * Fore * FMath::Lerp(0.012f * Pace * FMath::Sin(TwoPi * (StepS - WalkLow)),
		-0.008f * FMath::Sin(TwoPi * (StepS - RunLow)), RunGait);
	const FVector2D PelvisXY = MoveDirLocal * Surge + FVector2D(0.f, SwayY);
	const float PelvisYaw = 2.f * (FxL - FxR) * GaitWeight * Fore;                          // up to 4 degrees each way
	// A run-up drops the body on the second-last stride, keeps it low into the plant, and the take-off support lifts it
	// again (Hay & Nohara 1990)
	float RunUpDip = 0.f;
	if (In.RunUpLeft >= 0.f && In.bOnFloor)
	{
		const float Stance = FTaGRules::RunUpStanceT;
		const float D = 0.07f * LegLen;
		RunUpDip = In.RunUpLeft > Stance
			? -D * (1.f - FMath::SmoothStep(Stance, Stance + 0.3f, In.RunUpLeft))
			: FMath::Lerp(-D, 0.25f * D, 1.f - In.RunUpLeft / Stance);
	}
	const float PelvisRoll = FMath::Lerp(4.f, 3.f, RunGait) * GaitWeight * Fore * StanceL;  // the swing side drops

	const float R = FMath::Clamp(18.f * Dt, 0.f, 1.f);
	CurHipsY = FMath::Lerp(CurHipsY, HipsY, R);
	CurLean = LerpAngle(CurLean, Lean, R);
	CurSideLean = LerpAngle(CurSideLean, SideLean, R);
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

	// ---------------- Upper body
	Joints[Pivot]->SetRelativeRotation(FRotator(Deg(CurTilt), Deg(CurSpin), 0.f));
	const float Twist = -0.08f * (FxL - FxR) * Mix(1.f, 0.8f, 0.2f); // shoulders turn against the stride
	// The spine takes back most of the pelvic tilt so the shoulders stay level
	Joints[Spine]->SetRelativeRotation(FRotator(-Deg(CurLean), Deg(Twist), Deg(CurSideLean) - 0.85f * PelvisRoll));
	if (Joints[Neck])
	{
		// The head stays nearly level while the body leans
		Joints[Neck]->SetRelativeRotation(FRotator(Deg(CurLean) * 0.5f, -Deg(Twist) * 0.7f, -Deg(CurSideLean) * 0.6f));
	}
	// Shz follows rig.gd: positive swings a hanging arm toward the body's right. UE's roll swings it toward the left
	// (-Y), so the sign flips here; without it both arms swung inward and crossed in front of the body.
	Joints[ShoulderL]->SetRelativeRotation(FRotator(Deg(Cur[0]), 0.f, -Deg(Cur[2])));
	Joints[ShoulderR]->SetRelativeRotation(FRotator(Deg(Cur[1]), 0.f, -Deg(Cur[3])));
	Joints[ElbowL]->SetRelativeRotation(FRotator(Deg(Cur[4]), 0.f, 0.f));
	Joints[ElbowR]->SetRelativeRotation(FRotator(Deg(Cur[5]), 0.f, 0.f));

	// ---------------- Pelvis height: the pose, lowered so the legs reach the planted feet, squashed on landing
	LandSquash = FMath::Max(0.f, LandSquash - Dt * 5.f);
	const float PoseZ = PelvisRestZ + (CurHipsY - 0.95f) * 100.f * HeightScale - 0.08f * LegLen * LandSquash + BobZ + RunUpDip;
	const FRotator PelvisRot(0.f, PelvisYaw, PelvisRoll);
	Joints[Hips]->SetRelativeLocationAndRotation(FVector(PelvisXY.X, PelvisXY.Y, PoseZ - HipDrop * LegIKWeight), PelvisRot);
	float Need = 0.f;
	if (bFeetValid && LegIKWeight > 0.f)
	{
		const FTransform HipsXf = Joints[Hips]->GetComponentTransform();
		const float Reach = 0.985f * (ThighLen + ShinLen);
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector HipW = HipsXf.TransformPosition(HipSock[i]);
			const FVector EndW = ChainEndTarget(i);
			const float Hd = FVector::Dist2D(HipW, EndW);
			const float Up = FMath::Sqrt(FMath::Max(Reach * Reach - Hd * Hd, 1.f));
			Need = FMath::Max(Need, (HipW.Z + HipDrop * LegIKWeight) - EndW.Z - Up);
		}
	}
	HipDrop = Need > HipDrop ? FMath::FInterpTo(HipDrop, Need, Dt, 25.f) : FMath::FInterpTo(HipDrop, Need, Dt, 10.f);
	HipDrop = FMath::Clamp(HipDrop, 0.f, 0.3f * LegLen);
	Joints[Hips]->SetRelativeLocationAndRotation(FVector(PelvisXY.X, PelvisXY.Y, PoseZ - HipDrop * LegIKWeight), PelvisRot);

	// ---------------- What the eyes do (first-person camera): the step-locked pelvis motion, the landing dip, half of a
	// crouch, and a slow breath when standing
	const float Crouch = bNormal ? FMath::Min((CurHipsY - 0.95f) * 100.f * HeightScale, 0.f) : 0.f;
	// The reach dip (HipDrop) stays out: it bends the legs to keep stretched feet planted, the head does not follow it
	const float EyeZ = BobZ + RunUpDip - 0.06f * LegLen * LandSquash + 0.5f * Crouch
		+ 0.25f * HeightScale * FMath::Sin(IdleT * 1.7f) * (1.f - Moving);
	HeadMotion = FVector(PelvisXY.X, PelvisXY.Y, FMath::Clamp(EyeZ, -14.f * HeightScale, 1.f * HeightScale));
	DbgBob = BobZ; DbgCrouch = Crouch; DbgSide = SideMix; DbgRun = RunGait; DbgFroude = Froude; DbgSpd = Spd; DbgLeg = LegLen;

	// ---------------- Legs
	SolveLeg(0, FRotator(Deg(Cur[6]), 0.f, 0.f).Quaternion(), FRotator(Deg(CurKnee[0]), 0.f, 0.f).Quaternion());
	SolveLeg(1, FRotator(Deg(Cur[7]), 0.f, 0.f).Quaternion(), FRotator(Deg(CurKnee[1]), 0.f, 0.f).Quaternion());
}

float UTaGRigComponent::GetFootError(int32 Side) const
{
	if (!bFeetValid || LegIKWeight < 0.99f)
	{
		return 0.f;
	}
	const USceneComponent* End = Joints[Side == 0 ? AnkleL : AnkleR];
	if (!End)
	{
		return 0.f;
	}
	return FVector::Dist(End->GetComponentLocation(), ChainEndTarget(Side));
}

FString UTaGRigComponent::DebugHead() const
{
	return FString::Printf(TEXT("eye=%.1f bob=%.1f crouch=%.1f squash=%.2f gw=%.2f side=%.2f run=%.2f fr=%.2f spd=%.0f leg=%.0f"),
		HeadMotion.Z, DbgBob, DbgCrouch, LandSquash, GaitWeight, DbgSide, DbgRun, DbgFroude, DbgSpd, DbgLeg);
}

FString UTaGRigComponent::DebugFeet() const
{
	FString Out = FString::Printf(TEXT("phase=%.2f drop=%.1f ik=%.2f"), GaitPhase, HipDrop, LegIKWeight);
	if (Joints.Num() == JointCount && Joints[Hips])
	{
		const FTransform HipsXf = Joints[Hips]->GetComponentTransform();
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector HipW = HipsXf.TransformPosition(HipSock[i]);
			const FVector EndW = ChainEndTarget(i);
			Out += FString::Printf(TEXT(" | %s planted=%d lift=%.1f hd=%.1f dz=%.1f len=%.1f reach=%.1f"), i == 0 ? TEXT("L") : TEXT("R"),
				Feet[i].bPlanted ? 1 : 0, Feet[i].Lift, FVector::Dist2D(HipW, EndW), HipW.Z - EndW.Z, FVector::Dist(HipW, EndW), ThighLen + ShinLen);
		}
	}
	return Out;
}
