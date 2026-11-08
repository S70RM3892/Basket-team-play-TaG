#include "Ball/TaGBall.h"
#include "BasketTaG.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGGameMode.h"
#include "Core/TaGTypes.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATaGBall::ATaGBall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // after the characters moved
	bReplicates = true;
	SetReplicatingMovement(true);
	bAlwaysRelevant = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetWorldScale3D(FVector(TaG::BallRadius * 2.f / 100.f));
	Mesh->SetCastShadow(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
	if (ShapeMat.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMat.Object);
	}
}

void ATaGBall::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInstanceDynamic* M = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.38f, 0.08f));
	}
}

void ATaGBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATaGBall, State);
	DOREPLIFETIME(ATaGBall, Holder);
	DOREPLIFETIME(ATaGBall, Shooter);
	DOREPLIFETIME(ATaGBall, LastTouchTeam);
}

FVector ATaGBall::HoldPoint(const ATaGCharacter* Who, float Time)
{
	if (!Who)
	{
		return FVector::ZeroVector;
	}
	const FVector Feet = Who->GetFeetLocation();
	const FVector Fwd = Who->GetActorForwardVector();
	const FVector Right = Who->GetActorRightVector();
	if (Who->IsChargingShot())
	{
		// Set point above the forehead
		return Feet + Fwd * 25.f + FVector(0.f, 0.f, TaG::EyeHeight + 35.f);
	}
	const float Speed = Who->GetVelocity().Size2D();
	const FVector Pocket = Feet + Fwd * 40.f + Right * 22.f;
	if (Speed > 60.f && Who->GetCharacterMovement() && Who->GetCharacterMovement()->IsMovingOnGround())
	{
		// Dribble: one bounce every 0.5 s between the floor and the waist
		const float Bob = FMath::Abs(FMath::Sin(Time * UE_PI / 0.5f));
		return Pocket + FVector(0.f, 0.f, TaG::BallRadius + Bob * 85.f);
	}
	return Pocket + FVector(0.f, 0.f, 105.f);
}

void ATaGBall::GiveTo(ATaGCharacter* NewHolder)
{
	Holder = NewHolder;
	Shooter = nullptr;
	State = NewHolder ? ETaGBallState::Held : ETaGBallState::Dead;
	if (NewHolder)
	{
		LastTouchTeam = NewHolder->GetTeam();
		SetActorLocation(HoldPoint(NewHolder, GetWorld()->GetTimeSeconds()));
	}
	Sim.Vel = FVector::ZeroVector;
}

void ATaGBall::LaunchShot(const FVector& From, const FVector& Velocity, ATaGCharacter* InShooter, int32 Points, bool bValid)
{
	Holder = nullptr;
	Shooter = InShooter;
	State = ETaGBallState::Flight;
	LastTouchTeam = InShooter ? InShooter->GetTeam() : -1;
	Sim.Pos = From;
	Sim.Vel = Velocity;
	FlightTime = 0.f;
	LooseTime = 0.f;
	NoPickupTime = 0.f;
	ShotPoints = Points;
	bShotValid = bValid;
	bTouchedRim = false;
	SetActorLocation(From);
}

void ATaGBall::MakeLoose(const FVector& From, const FVector& Velocity, int32 TouchTeam)
{
	Holder = nullptr;
	Shooter = nullptr;
	State = ETaGBallState::Loose;
	LastTouchTeam = TouchTeam;
	Sim.Pos = From;
	Sim.Vel = Velocity;
	LooseTime = 0.f;
	NoPickupTime = 0.25f;
	SetActorLocation(From);
}

void ATaGBall::SetDead()
{
	Holder = nullptr;
	Shooter = nullptr;
	State = ETaGBallState::Dead;
	Sim.Vel = FVector::ZeroVector;
}

void ATaGBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		TickAuthority(DeltaSeconds);
	}
	else if (State == ETaGBallState::Held && Holder)
	{
		// Clients place the held ball themselves so the dribble does not wait for the network.
		SetActorLocation(HoldPoint(Holder, GetWorld()->GetTimeSeconds()));
	}

	// First person: the holder's own ball sits in the lower right of their view. Purely cosmetic and
	// local; everyone else (and the rules) use HoldPoint.
	if (State == ETaGBallState::Held && Holder && Holder->IsLocallyControlled() && Holder->IsPlayerControlled())
	{
		const FRotator View = Holder->GetControlRotation();
		const FVector Fwd = View.Vector();
		const FVector Right = FRotationMatrix(View).GetUnitAxis(EAxis::Y);
		const FVector Up = FRotationMatrix(View).GetUnitAxis(EAxis::Z);
		FVector P = Holder->GetEyeLocation() + Fwd * 80.f;
		if (Holder->IsChargingShot())
		{
			P -= Up * 12.f; // set point, just under the aim
		}
		else
		{
			const float Speed = Holder->GetVelocity().Size2D();
			const float Bob = Speed > 60.f ? FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * UE_PI / 0.5f)) : 1.f;
			P += Right * 32.f - Up * FMath::Lerp(75.f, 38.f, Bob);
		}
		SetActorLocation(P);
	}

	// Cosmetic spin
	if (State == ETaGBallState::Flight || State == ETaGBallState::Loose)
	{
		Mesh->AddLocalRotation(FRotator(-720.f * DeltaSeconds, 0.f, 0.f));
	}
}

void ATaGBall::TickAuthority(float Dt)
{
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();

	switch (State)
	{
	case ETaGBallState::Held:
		if (Holder)
		{
			SetActorLocation(HoldPoint(Holder, GetWorld()->GetTimeSeconds()));
		}
		return;

	case ETaGBallState::Dead:
		return;

	case ETaGBallState::Flight:
	case ETaGBallState::Loose:
	{
		bool bScored = false;
		const uint8 Contacts = Sim.Step(Dt, bScored);
		SetActorLocation(Sim.Pos);
		NoPickupTime = FMath::Max(0.f, NoPickupTime - Dt);

		if (State == ETaGBallState::Flight)
		{
			FlightTime += Dt;
			if (Contacts & FTaGBallSim::Rim)
			{
				bTouchedRim = true;
			}
			if (bScored)
			{
				ATaGCharacter* Scorer = Shooter;
				SetDead();
				if (GM)
				{
					GM->OnBasket(Scorer, ShotPoints, bShotValid);
				}
				return;
			}
			CheckBlocks();
			if (State != ETaGBallState::Flight)
			{
				return;
			}
			// Once the shot is on its way down below the rim (or hit the floor) it becomes a rebound.
			if ((FlightTime > 0.35f && Sim.Vel.Z < 0.f && Sim.Pos.Z < TaG::RimZ - 45.f) || (Contacts & FTaGBallSim::Floor))
			{
				State = ETaGBallState::Loose;
				LooseTime = 0.f;
				Shooter = nullptr;
				if (GM)
				{
					GM->OnShotMissed(bTouchedRim);
				}
			}
		}
		else
		{
			LooseTime += Dt;
			if (bScored && GM)
			{
				// A loose ball that drops in (tip-in off a block, etc.) does not count in P1.
			}
		}

		if (TaG::IsOutOfBounds(Sim.Pos) && Sim.Pos.Z < 60.f)
		{
			const int32 Team = LastTouchTeam;
			SetDead();
			if (GM)
			{
				GM->OnOutOfBounds(Team);
			}
			return;
		}
		if (State == ETaGBallState::Loose)
		{
			if (LooseTime > 8.f && GM)
			{
				SetDead();
				GM->OnStuckBall(LastTouchTeam);
				return;
			}
			TryPickup();
		}
		return;
	}
	}
}

void ATaGBall::TryPickup()
{
	if (NoPickupTime > 0.f)
	{
		return;
	}
	ATaGCharacter* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		ATaGCharacter* C = *It;
		if (!C || C->IsRecovering())
		{
			continue;
		}
		const FVector Feet = C->GetFeetLocation();
		const float Flat = FVector2D(Sim.Pos.X - Feet.X, Sim.Pos.Y - Feet.Y).Size();
		const float Reach = TaG::StandingReach + (Feet.Z > 5.f ? 30.f : 0.f);
		if (Flat < 70.f && Sim.Pos.Z < Feet.Z + Reach && Flat < BestDist)
		{
			Best = C;
			BestDist = Flat;
		}
	}
	if (Best)
	{
		GiveTo(Best);
		if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
		{
			GM->OnBallPickedUp(Best);
		}
	}
}

void ATaGBall::CheckBlocks()
{
	// Aimed block: a defender who swiped recently and whose hand meets the rising ball.
	if (FlightTime > 0.6f)
	{
		return;
	}
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		ATaGCharacter* C = *It;
		if (!C || !C->IsBlocking() || (Shooter && C->GetTeam() == Shooter->GetTeam()))
		{
			continue;
		}
		const FVector Hand = C->GetBlockHandLocation();
		if (FVector::Dist(Hand, Sim.Pos) < TaG::BlockHitRadius)
		{
			FVector Away(Sim.Pos.X - TaG::RimX, Sim.Pos.Y, 0.f);
			Away = Away.GetSafeNormal() * 420.f;
			MakeLoose(Sim.Pos, Away + FVector(0.f, 0.f, -150.f), C->GetTeam());
			if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
			{
				GM->OnBlocked(C);
			}
			return;
		}
	}
}
