#include "Core/TaGHeroDisplay.h"
#include "Character/TaGCharacter.h"
#include "Character/TaGRig.h"
#include "Core/TaGArtLib.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"

ATaGHeroDisplay::ATaGHeroDisplay()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Rig = CreateDefaultSubobject<UTaGRigComponent>(TEXT("Rig"));
	Rig->SetupAttachment(Root);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetFieldOfView(50.f);
	Camera->bConstrainAspectRatio = false;
}

void ATaGHeroDisplay::ShowHero(ETaGHero InHero, const FLinearColor& TeamColor, bool bCheer)
{
	Team = TeamColor;
	if (!bBuilt || InHero != Hero)
	{
		Hero = InHero;
		Rig->Build(TaGHeroes::Get(Hero));
		bBuilt = true;
		if (bCheer)
		{
			CheerT = 0.9f; // arms up for a moment when a hero steps forward
		}
	}
	Rig->SetTeamColor(Team);
	if (!Shadow)
	{
		Shadow = TaGArt::MakeBlobShadow(this);
	}
}

void ATaGHeroDisplay::SetPose(const FTaGPoseInput& In)
{
	Pose = In;
	bPose = true;
}

void ATaGHeroDisplay::AnimatePose(const FTaGPoseInput& In)
{
	if (!bBuilt)
	{
		return;
	}
	Pose = In;
	bPose = true;
	Rig->SetRelativeRotation(FRotator::ZeroRotator);
	Rig->Animate(In);
	Rig->SetGlow(Team, 6.f, 6.f);
	const FTaGHeroDef& H = TaGHeroes::Get(Hero);
	const FVector At = GetActorLocation();
	TaGArt::PlaceBlobShadow(Shadow, FVector(At.X, At.Y, 0.f), H.CapsuleRadius * 2.f + 14.f, At.Z, 0.36f);
}

void ATaGHeroDisplay::FrameForSelect(const FVector& Floor, float FacingYaw)
{
	SetActorLocationAndRotation(Floor, FRotator(0.f, FacingYaw, 0.f));
	BaseYaw = FacingYaw;
	// The camera stands in front of the body at chest height with a portrait lens. It looks past the body's right
	// side, so the body stands in the right half of the frame and the panel on the left keeps the court behind it.
	const FTaGHeroDef& H = TaGHeroes::Get(Hero);
	const float Tall = FMath::Max(H.EyeHeight, 140.f) + 15.f;
	const FRotator Face(0.f, FacingYaw, 0.f);
	const FVector Fwd = Face.Vector();
	const FVector Right = FRotationMatrix(Face).GetUnitAxis(EAxis::Y);
	const FVector Eye = Floor + Fwd * (260.f + Tall * 1.25f) + Right * (Tall * 0.28f) + FVector(0.f, 0.f, Tall * 0.62f);
	const FVector Look = Floor + Right * (Tall * 0.5f) + FVector(0.f, 0.f, Tall * 0.52f);
	Camera->SetWorldLocationAndRotation(Eye, (Look - Eye).Rotation());
}

void ATaGHeroDisplay::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		return;
	}
	IdleT += DeltaSeconds;
	if (bPose)
	{
		return; // a replay drives this body (AnimatePose) with its slowed time
	}
	FTaGPoseInput In;
	{
		In.Velocity = FVector::ZeroVector;
		In.Speed = 0.f;
		In.bOnFloor = true;
		In.Role = 0;
		In.Action = ETaGAction::None;
		In.Dt = DeltaSeconds;
		if (CheerT > 0.f)
		{
			CheerT -= DeltaSeconds;
			In.Action = ETaGAction::Contest; // both arms up
		}
		// A slow sway toward the camera and back, so the whole body reads
		Rig->SetRelativeRotation(FRotator(0.f, 16.f * FMath::Sin(IdleT * 0.55f), 0.f));
	}
	Rig->Animate(In);
	Rig->SetGlow(Team, 6.f, 6.f);
	const FTaGHeroDef& H = TaGHeroes::Get(Hero);
	const FVector At = GetActorLocation();
	TaGArt::PlaceBlobShadow(Shadow, FVector(At.X, At.Y, 0.f), H.CapsuleRadius * 2.f + 14.f, At.Z, 0.36f);
}
