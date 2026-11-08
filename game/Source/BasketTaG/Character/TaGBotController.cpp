#include "Character/TaGBotController.h"
#include "Ball/TaGBall.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGGameMode.h"
#include "Core/TaGGameState.h"
#include "Core/TaGTypes.h"
#include "GameFramework/CharacterMovementComponent.h"

ATaGBotController::ATaGBotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true;
	bSetControlRotationFromPawnOrientation = false; // the bot steers its view itself (FaceTowards)
}

void ATaGBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	bHasSpot = false;
}

void ATaGBotController::SteerTo(ATaGCharacter* Me, const FVector& Target, float Accuracy)
{
	FVector To = Target - Me->GetFeetLocation();
	To.Z = 0.f;
	const float D = To.Size();
	if (D > Accuracy)
	{
		Me->AddMovementInput(To / D, FMath::Clamp(D / 120.f, 0.35f, 1.f));
	}
}

void ATaGBotController::PickOffenseSpot(ATaGCharacter* Me)
{
	const bool bDeep = FMath::FRand() < 0.4f;
	const float Dist = bDeep ? FMath::FRandRange(TaG::ArcRadius + 40.f, TaG::ArcRadius + 140.f) : FMath::FRandRange(180.f, TaG::ArcRadius - 60.f);
	const float Angle = FMath::DegreesToRadians(FMath::FRandRange(-70.f, 70.f));
	OffenseSpot = FVector(TaG::RimX + FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
	OffenseSpot.X = FMath::Clamp(OffenseSpot.X, 60.f, TaG::CourtDepth - 80.f);
	OffenseSpot.Y = FMath::Clamp(OffenseSpot.Y, -TaG::CourtHalfWidth + 80.f, TaG::CourtHalfWidth - 80.f);
	bHasSpot = true;
	HoldTime = 0.f;
	ShootAfter = FMath::FRandRange(0.9f, 2.6f);
	// Release timing: aim for the perfect window, spread shrinks with skill.
	const float Spread = FMath::Lerp(0.16f, 0.05f, Skill);
	ReleaseAt = FMath::Clamp(0.74f + FMath::FRandRange(-Spread, Spread), 0.45f, 1.2f);
}

void ATaGBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ATaGCharacter* Me = Cast<ATaGCharacter>(GetPawn());
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	if (!Me || !GM || !GM->IsLive())
	{
		return;
	}
	ATaGBall* Ball = Me->FindBall();
	if (!Ball)
	{
		return;
	}

	switch (Ball->GetBallState())
	{
	case ETaGBallState::Held:
		if (Ball->GetHolder() == Me)
		{
			TickOffense(Me, DeltaSeconds);
		}
		else if (Ball->GetHolder())
		{
			bHadBall = false;
			bBlockDecided = false;
			TickDefense(Me, Ball->GetHolder(), DeltaSeconds);
		}
		break;
	case ETaGBallState::Flight:
	case ETaGBallState::Loose:
		bHadBall = false;
		if (Ball->GetBallState() == ETaGBallState::Flight && Ball->GetShooter() && Ball->GetShooter()->GetTeam() != Me->GetTeam())
		{
			// One read per shot: swipe at a rising ball that comes close (decided once, not every frame).
			if (!bBlockDecided && FVector::Dist(Me->GetEyeLocation(), Ball->GetActorLocation()) < 150.f)
			{
				bBlockDecided = true;
				if (Me->CanBlock() && FMath::FRand() < 0.1f + 0.25f * Skill)
				{
					Me->FaceTowards(Ball->GetActorLocation(), 1.f, 1000.f);
					Me->StartBlock();
				}
			}
		}
		else
		{
			bBlockDecided = false;
		}
		TickChase(Me, Ball->GetActorLocation(), DeltaSeconds);
		break;
	default:
		bHadBall = false;
		bBlockDecided = false;
		Me->FaceTowards(TaG::RimCenter(), DeltaSeconds, 4.f);
		break;
	}
}

void ATaGBotController::TickOffense(ATaGCharacter* Me, float Dt)
{
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	if (!bHadBall)
	{
		bHadBall = true;
		bHasSpot = false;
	}

	if (Me->IsChargingShot())
	{
		Me->FaceTowards(TaG::RimCenter(), Dt, 12.f);
		if (Me->GetChargeTime() >= ReleaseAt)
		{
			Me->ReleaseShot();
		}
		return;
	}

	// 3x3: after a steal or defensive rebound the ball must go back beyond the arc first.
	if (GM->NeedsClear(Me->GetTeam()))
	{
		FVector Out(Me->GetFeetLocation().X - TaG::RimX, Me->GetFeetLocation().Y, 0.f);
		Out = Out.GetSafeNormal() * (TaG::ArcRadius + 90.f);
		const FVector Target(TaG::RimX + Out.X, Out.Y, 0.f);
		Me->FaceTowards(Target + FVector(0, 0, TaG::EyeHeight), Dt, 6.f);
		SteerTo(Me, Target);
		bHasSpot = false;
		return;
	}

	if (!bHasSpot)
	{
		PickOffenseSpot(Me);
	}
	HoldTime += Dt;
	Me->FaceTowards(TaG::RimCenter(), Dt, 6.f);
	SteerTo(Me, OffenseSpot);

	float DefDist = 9999.f;
	if (const ATaGCharacter* Def = Me->FindOpponent())
	{
		DefDist = FVector::Dist2D(Def->GetFeetLocation(), Me->GetFeetLocation());
	}
	const float RimDist = TaG::DistToRim(Me->GetFeetLocation());
	const bool bOpen = DefDist > 220.f && RimDist < TaG::ArcRadius + 150.f && HoldTime > 0.5f;
	const bool bClock = GM->GetShotClockLeft() < 2.5f;
	if ((HoldTime >= ShootAfter || bOpen || bClock) && Me->CanStartShot())
	{
		Me->BeginShot();
	}
}

void ATaGBotController::TickDefense(ATaGCharacter* Me, ATaGCharacter* Handler, float Dt)
{
	const FVector HFeet = Handler->GetFeetLocation();
	FVector ToRim(TaG::RimX - HFeet.X, -HFeet.Y, 0.f);
	ToRim = ToRim.GetSafeNormal();
	// Stay between the handler and the rim, a little more than an arm's length off.
	const FVector Spot = HFeet + ToRim * 130.f;
	SteerTo(Me, Spot, 25.f);
	Me->FaceTowards(Handler->GetActorLocation(), Dt, 10.f);

	const float D = FVector::Dist2D(HFeet, Me->GetFeetLocation());
	if (Handler->IsChargingShot())
	{
		// Contest: read the shot and rise with the shooter
		if (!bReactedToShot && Handler->GetChargeTime() > FMath::Lerp(0.45f, 0.3f, Skill))
		{
			bReactedToShot = true;
			if (D < TaG::ContestRange && FMath::FRand() < 0.35f + 0.45f * Skill)
			{
				Me->Jump();
			}
		}
	}
	else
	{
		bReactedToShot = false;
		Me->StopJumping();
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextStealTry && D < TaG::StealRange - 20.f && Me->CanSteal())
		{
			NextStealTry = Now + 1.5f;
			if (FMath::FRand() < 0.15f + 0.25f * Skill)
			{
				Me->StartSteal();
			}
		}
	}
}

void ATaGBotController::TickChase(ATaGCharacter* Me, const FVector& BallPos, float Dt)
{
	SteerTo(Me, FVector(BallPos.X, BallPos.Y, 0.f), 10.f);
	Me->FaceTowards(BallPos, Dt, 8.f);
	const FVector Feet = Me->GetFeetLocation();
	if (FVector::Dist2D(BallPos, Feet) < 90.f && BallPos.Z > Feet.Z + TaG::StandingReach - 10.f && BallPos.Z < Feet.Z + TaG::StandingReach + 80.f)
	{
		Me->Jump();
	}
	else
	{
		Me->StopJumping();
	}
}
