#include "Ball/TaGBall.h"
#include "BasketTaG.h"
#include "Character/TaGArtActors.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGArtLib.h"
#include "Core/TaGAudio.h"
#include "Core/TaGGameMode.h"
#include "Core/TaGGameState.h"
#include "Core/TaGRules.h"
#include "Core/TaGTypes.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

ATaGBall::ATaGBall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // after the characters moved
	bReplicates = true;
	SetReplicatingMovement(false); // clients simulate from FTaGBallNet
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(60.f);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
}

void ATaGBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATaGBall, Net);
	DOREPLIFETIME(ATaGBall, LastTouchTeam);
	DOREPLIFETIME(ATaGBall, ShotTeam);
	DOREPLIFETIME(ATaGBall, Chain);
}

void ATaGBall::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* Art = TaGArt::LoadMesh(TEXT("/Game/Art/Ball/SM_Ball")))
	{
		Mesh->SetStaticMesh(Art);
		Mesh->SetWorldScale3D(FVector(1.f));
		if (Mesh->GetMaterialIndex(TEXT("Glow")) != INDEX_NONE)
		{
			GlowMat = TaGArt::PaintSlot(Mesh, TEXT("Glow"), FLinearColor(1.f, 0.6f, 0.2f), 0.f);
		}
	}
	else
	{
		Mesh->SetStaticMesh(TaGArt::EngineSphere());
		Mesh->SetWorldScale3D(FVector(TaG::BallRadius * 2.f / 100.f));
		GlowMat = TaGArt::PaintSlot(Mesh, NAME_None, FLinearColor(0.95f, 0.38f, 0.08f), 0.f);
	}
}

// ================================================================ Server API

void ATaGBall::PushNet()
{
	Net.Pos = Sim.Pos;
	Net.Vel = Sim.Vel;
	if (const AGameStateBase* GS = GetWorld()->GetGameState())
	{
		Net.ServerTime = GS->GetServerWorldTimeSeconds();
	}
	Net.Seq++;
	SyncTimer = 0.f;
}

void ATaGBall::GiveTo(ATaGCharacter* NewHolder)
{
	if (ATaGCharacter* Old = Net.Holder)
	{
		if (Old != NewHolder)
		{
			Old->OnBallLost();
		}
	}
	Net.State = NewHolder ? ETaGBallState::Held : ETaGBallState::Dead;
	Net.Holder = NewHolder;
	Net.Receiver = nullptr;
	Shooter = nullptr;
	if (NewHolder)
	{
		LastTouchTeam = NewHolder->GetTeam();
		NewHolder->OnBallGained();
		Sim.Pos = HoldPoint(NewHolder);
		SetActorLocation(Sim.Pos);
	}
	Sim.Vel = FVector::ZeroVector;
	PushNet();
}

void ATaGBall::LaunchShot(const FVector& From, const FVector& Velocity, ATaGCharacter* InShooter, int32 Points, bool bValid, float BlockMult)
{
	if (ATaGCharacter* Old = Net.Holder)
	{
		Old->OnBallLost();
	}
	Net.State = ETaGBallState::Flight;
	Net.Holder = nullptr;
	Net.Receiver = nullptr;
	Shooter = InShooter;
	ShotTeam = InShooter ? InShooter->GetTeam() : -1;
	LastTouchTeam = ShotTeam;
	Sim.Pos = From;
	Sim.Vel = Velocity;
	FlightTime = 0.f;
	LooseTime = 0.f;
	NoPickupTime = 0.f;
	ShotPoints = Points;
	bShotValid = bValid;
	bTouchedRim = false;
	ShotBlockMult = BlockMult;
	BlockRolled.Reset();
	SetActorLocation(From);
	PushNet();
	TaGAudio::PlayAt(this, TEXT("ShotRelease"), From, 0.6f);
}

void ATaGBall::LaunchPass(ATaGCharacter* InPasser, ATaGCharacter* Receiver, bool bThread)
{
	if (!InPasser || !Receiver)
	{
		return;
	}
	InPasser->OnBallLost();
	Passer = InPasser;
	PassFrom = HoldPoint(InPasser);
	// Lead the receiver a little
	const float Speed = bThread ? TaG::ThreadPassSpeed : TaG::PassSpeed;
	FVector Target = Receiver->GetFeetLocation() + FVector(0.f, 0.f, Receiver->GetHeroDef().EyeHeight * 0.7f);
	const float Dist0 = FVector::Dist(PassFrom, Target);
	Target += Receiver->GetVelocity() * FMath::Clamp(Dist0 / Speed, 0.f, 0.6f) * 0.8f;
	const float Duration = FMath::Max(FVector::Dist(PassFrom, Target) / Speed, 0.12f);

	PassRolled.Reset();
	Net.State = ETaGBallState::Pass;
	Net.Holder = nullptr;
	Net.Receiver = Receiver;
	Net.Target = Target;
	Net.Duration = Duration;
	Net.bThread = bThread;
	LastTouchTeam = InPasser->GetTeam();
	FlightTime = 0.f;
	Sim.Pos = PassFrom;
	Sim.Vel = (Target - PassFrom) / Duration;
	SetActorLocation(PassFrom);
	PushNet();
	Net.Pos = PassFrom;
}

void ATaGBall::MakeLoose(const FVector& From, const FVector& Velocity, int32 TouchTeam)
{
	if (ATaGCharacter* Old = Net.Holder)
	{
		Old->OnBallLost();
	}
	Net.State = ETaGBallState::Loose;
	Net.Holder = nullptr;
	Net.Receiver = nullptr;
	Shooter = nullptr;
	LastTouchTeam = TouchTeam;
	Sim.Pos = From;
	Sim.Vel = Velocity;
	LooseTime = 0.f;
	NoPickupTime = 0.25f;
	SetActorLocation(From);
	PushNet();
}

void ATaGBall::SetDead()
{
	if (ATaGCharacter* Old = Net.Holder)
	{
		Old->OnBallLost();
	}
	Net.State = ETaGBallState::Dead;
	Net.Holder = nullptr;
	Net.Receiver = nullptr;
	Shooter = nullptr;
	// A dead ball rests on the floor where it was
	const FVector At = GetActorLocation();
	Sim.Pos = FVector(At.X, At.Y, TaG::BallRadius);
	Sim.Vel = FVector::ZeroVector;
	SetActorLocation(Sim.Pos);
	PushNet();
}

// ================================================================ Positions

FVector ATaGBall::HoldPoint(const ATaGCharacter* Who) const
{
	if (!Who)
	{
		return GetActorLocation();
	}
	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	// First person: the holder's own ball sits in the lower right of their view (cosmetic, local only).
	if (Who->IsLocallyControlled() && Who->IsPlayerControlled() && !Who->IsThirdPerson() && GetNetMode() != NM_DedicatedServer)
	{
		const FRotator View = Who->GetControlRotation();
		const FVector Fwd = View.Vector();
		const FVector Right = FRotationMatrix(View).GetUnitAxis(EAxis::Y);
		const FVector Up = FRotationMatrix(View).GetUnitAxis(EAxis::Z);
		FVector P = Who->GetEyeLocation() + Fwd * 80.f;
		if (Who->IsChargingShot())
		{
			P -= Up * 12.f;
		}
		else
		{
			const bool bDribbling = Who->GetAction() == ETaGAction::None && Who->GetVelocity().Size2D() > 60.f;
			const float Bob = bDribbling ? FMath::Abs(FMath::Sin(Time * 7.5f)) : 1.f;
			P += Right * 32.f - Up * FMath::Lerp(75.f, 38.f, Bob);
		}
		return P;
	}
	return Who->GetBallHoldLocation(Time);
}

FVector ATaGBall::PassPosition(float T) const
{
	const float S = FMath::Clamp(T / FMath::Max(Net.Duration, 0.01f), 0.f, 1.f);
	const FVector From = FVector(Net.Pos);
	const FVector To = FVector(Net.Target);
	const float Arc = Net.bThread ? 8.f : FMath::Clamp(FVector::Dist2D(From, To) * 0.06f, 15.f, 70.f);
	return FMath::Lerp(From, To, S) + FVector(0.f, 0.f, 4.f * Arc * S * (1.f - S));
}

// ================================================================ Tick

void ATaGBall::OnRep_Net()
{
	// Rebuild the local simulation from the snapshot, advanced to "now".
	Sim.Pos = Net.Pos;
	Sim.Vel = Net.Vel;
	const AGameStateBase* GS = GetWorld()->GetGameState();
	const float Elapsed = GS ? FMath::Clamp(static_cast<float>(GS->GetServerWorldTimeSeconds()) - Net.ServerTime, 0.f, 0.5f) : 0.f;
	FlightTime = Elapsed;
	if (Net.State == ETaGBallState::Dead)
	{
		SetActorLocation(Sim.Pos);
	}
	if (Net.State == ETaGBallState::Flight || Net.State == ETaGBallState::Loose)
	{
		bool bScored = false;
		Sim.Step(Elapsed, bScored);
	}
}

void ATaGBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SoundCooldown = FMath::Max(0.f, SoundCooldown - DeltaSeconds);

	if (HasAuthority())
	{
		TickAuthority(DeltaSeconds);
	}
	else
	{
		switch (Net.State)
		{
		case ETaGBallState::Held:
			break;
		case ETaGBallState::Pass:
			FlightTime += DeltaSeconds;
			SetActorLocation(PassPosition(FlightTime));
			break;
		case ETaGBallState::Flight:
		case ETaGBallState::Loose:
		{
			bool bScored = false;
			const uint8 Contacts = Sim.Step(DeltaSeconds, bScored);
			PlayContactSounds(Contacts);
			SetActorLocation(Sim.Pos);
			break;
		}
		default:
			break;
		}
	}

	if (Net.State == ETaGBallState::Held && Net.Holder)
	{
		const FVector P = HoldPoint(Net.Holder);
		SetActorLocation(P);
		// Dribble bounce sound
		const float T = GetWorld()->GetTimeSeconds() * 7.5f;
		if (Net.Holder->GetAction() == ETaGAction::None && Net.Holder->GetVelocity().Size2D() > 60.f && FMath::Abs(FMath::Sin(T)) < 0.12f && SoundCooldown <= 0.f)
		{
			SoundCooldown = 0.25f;
			TaGAudio::PlayAt(this, TEXT("Bounce"), Net.Holder->GetFeetLocation(), 0.35f);
		}
	}

	// Cosmetic spin and the core glow (brighter with each pass in the chain)
	if (Net.State == ETaGBallState::Flight || Net.State == ETaGBallState::Loose || Net.State == ETaGBallState::Pass)
	{
		Mesh->AddLocalRotation(FRotator(-720.f * DeltaSeconds, 0.f, 0.f));
	}
	if (GlowMat)
	{
		const float Pulse = 0.6f + 0.4f * FMath::Sin(GetWorld()->GetTimeSeconds() * (4.f + Chain * 2.f));
		TaGArt::SetGlow(GlowMat, FLinearColor(1.f, 0.55f + 0.1f * Chain, 0.15f + 0.15f * Chain), Chain > 0 ? (1.5f + Chain * 2.5f) * Pulse : 0.f);
	}
}

void ATaGBall::PlayContactSounds(uint8 Contacts)
{
	if (Contacts == 0 || SoundCooldown > 0.f)
	{
		return;
	}
	SoundCooldown = 0.08f;
	if (Contacts & FTaGBallSim::Rim)
	{
		TaGAudio::PlayAt(this, TEXT("Rim"), Sim.Pos, FMath::Clamp(Sim.Vel.Size() / 600.f, 0.3f, 1.f));
	}
	else if (Contacts & FTaGBallSim::Board)
	{
		TaGAudio::PlayAt(this, TEXT("Board"), Sim.Pos, FMath::Clamp(Sim.Vel.Size() / 600.f, 0.3f, 1.f));
	}
	else if (Contacts & FTaGBallSim::Floor)
	{
		TaGAudio::PlayAt(this, TEXT("Bounce"), Sim.Pos, FMath::Clamp(FMath::Abs(Sim.Vel.Z) / 500.f, 0.15f, 0.8f));
	}
}

void ATaGBall::TickAuthority(float Dt)
{
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	SyncTimer += Dt;

	switch (Net.State)
	{
	case ETaGBallState::Held:
	case ETaGBallState::Dead:
		return;

	case ETaGBallState::Pass:
		TickPass(Dt, true);
		return;

	case ETaGBallState::Flight:
	case ETaGBallState::Loose:
	{
		bool bScored = false;
		const uint8 Contacts = Sim.Step(Dt, bScored);
		PlayContactSounds(Contacts);
		SetActorLocation(Sim.Pos);
		NoPickupTime = FMath::Max(0.f, NoPickupTime - Dt);
		if (Contacts != 0)
		{
			PushNet(); // bounces diverge quickly: resync clients on every contact
		}

		// Walls deflect balls
		for (TActorIterator<ATaGWallActor> It(GetWorld()); It; ++It)
		{
			if (It->BlocksSegment(Sim.Pos - Sim.Vel * Dt, Sim.Pos))
			{
				const FVector N = It->GetActorForwardVector() * FMath::Sign(FVector::DotProduct(Sim.Pos - It->GetActorLocation(), It->GetActorForwardVector()));
				Sim.Vel = (Sim.Vel - 2.f * FVector::DotProduct(Sim.Vel, N) * N) * 0.5f;
				Sim.Pos += N * 15.f;
				if (Net.State == ETaGBallState::Flight)
				{
					Net.State = ETaGBallState::Loose;
					LooseTime = 0.f;
					Shooter = nullptr;
					if (GM) { GM->OnShotMissed(false); }
				}
				PushNet();
				TaGAudio::PlayAt(this, TEXT("Board"), Sim.Pos, 0.8f);
				break;
			}
		}

		if (Net.State == ETaGBallState::Flight)
		{
			FlightTime += Dt;
			if (Contacts & FTaGBallSim::Rim)
			{
				bTouchedRim = true;
			}
			if (bScored)
			{
				ATaGCharacter* Scorer = Shooter.Get();
				TaGAudio::PlayAt(this, TEXT("Swish"), TaG::RimCenter(), 1.f);
				SetDead();
				if (GM)
				{
					GM->OnBasket(Scorer, ShotPoints, bShotValid);
				}
				return;
			}
			CheckBlocks();
			if (Net.State != ETaGBallState::Flight)
			{
				return;
			}
			// Once the shot is on its way down below the rim (or hit the floor) it is a rebound.
			if ((FlightTime > 0.35f && Sim.Vel.Z < 0.f && Sim.Pos.Z < TaG::RimZ - 45.f) || (Contacts & FTaGBallSim::Floor))
			{
				Net.State = ETaGBallState::Loose;
				LooseTime = 0.f;
				Shooter = nullptr;
				PushNet();
				if (GM)
				{
					GM->OnShotMissed(bTouchedRim);
				}
			}
		}
		else
		{
			LooseTime += Dt;
			if (SyncTimer > 0.3f)
			{
				PushNet();
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
		if (Net.State == ETaGBallState::Loose)
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

void ATaGBall::TickPass(float Dt, bool bAuthority)
{
	FlightTime += Dt;
	const FVector Prev = GetActorLocation();
	const FVector P = PassPosition(FlightTime);
	SetActorLocation(P);
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	ATaGCharacter* Receiver = Net.Receiver;
	const int32 PassTeam = LastTouchTeam;

	// Interceptions: a defender's hand on the lane (thread passes only on the thread itself)
	if (FlightTime > 0.08f)
	{
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			ATaGCharacter* D = *It;
			if (D->GetTeam() == PassTeam || !D->CanTouchBall() || !D->IsActiveDefender())
			{
				continue;
			}
			if (PassRolled.Contains(D))
			{
				continue;
			}
			bool bHit = false;
			bool bDeflect = false;
			if (Net.bThread)
			{
				const FVector Body = D->GetActorLocation();
				const FVector C = FMath::ClosestPointOnSegment(Body, Prev, P);
				bHit = FVector::Dist2D(Body, C) < TaG::ThreadInterceptRadius && FMath::Abs(Body.Z - C.Z) < D->GetHeroDef().CapsuleHalfHeight;
			}
			else
			{
				const FVector Hand = D->GetBlockHandLocation();
				const FVector Body = D->GetActorLocation();
				const float Reach = TaG::PassInterceptRadius * D->GetHeroDef().ReachMul;
				const FVector CH = FMath::ClosestPointOnSegment(Hand, Prev, P);
				const FVector CB = FMath::ClosestPointOnSegment(Body, Prev, P);
				if (FVector::Dist(Hand, CH) < Reach)
				{
					// A hand in the lane: one try per defender per pass
					PassRolled.Add(D);
					bHit = FMath::FRand() < 0.5f * D->GetHeroDef().ReachMul;
				}
				else if (FVector::Dist2D(Body, CB) < D->GetHeroDef().CapsuleRadius + TaG::BallRadius && FMath::Abs(Body.Z - CB.Z) < D->GetHeroDef().CapsuleHalfHeight)
				{
					bDeflect = true; // off the body: a loose ball, not a clean steal
				}
			}
			if (bDeflect)
			{
				MakeLoose(P, (P - Prev).GetSafeNormal() * -150.f + FVector(0.f, 0.f, 200.f), D->GetTeam());
				TaGAudio::PlayAt(this, TEXT("Block"), P, 0.8f);
				return;
			}
			if (bHit)
			{
				GiveTo(D);
				TaGAudio::PlayAt(this, TEXT("Steal"), P, 1.f);
				if (GM)
				{
					GM->OnPassIntercepted(D);
				}
				return;
			}
		}
		// Walls stop passes
		for (TActorIterator<ATaGWallActor> It(GetWorld()); It; ++It)
		{
			if (It->GetTeam() != PassTeam && It->BlocksSegment(Prev, P))
			{
				MakeLoose(Prev, (Prev - P).GetSafeNormal() * 200.f + FVector(0.f, 0.f, 150.f), It->GetTeam());
				TaGAudio::PlayAt(this, TEXT("Board"), P, 0.9f);
				return;
			}
		}
	}

	if (FlightTime >= Net.Duration)
	{
		if (Receiver && Receiver->CanTouchBall() && FVector::Dist(Receiver->GetActorLocation(), P) < TaG::CatchRadius + Receiver->GetHeroDef().CapsuleHalfHeight)
		{
			GiveTo(Receiver);
			TaGAudio::PlayAt(this, TEXT("Catch"), P, 0.8f);
			if (GM)
			{
				GM->OnPassCaught(Receiver);
			}
		}
		else
		{
			// Missed the receiver: it rolls on
			MakeLoose(P, (P - Prev) / FMath::Max(Dt, 0.001f) * 0.6f, PassTeam);
			NoPickupTime = 0.f;
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
		if (!C || !C->CanTouchBall() || C->GetAction() == ETaGAction::Stagger)
		{
			continue;
		}
		const FVector Feet = C->GetFeetLocation();
		const float Flat = FVector2D(Sim.Pos.X - Feet.X, Sim.Pos.Y - Feet.Y).Size();
		const float Reach = (C->GetHeroDef().EyeHeight + 75.f) * FMath::Sqrt(C->GetHeroDef().ReachMul) + (Feet.Z > 5.f ? 30.f : 0.f);
		if (Flat < 50.f + C->GetHeroDef().CapsuleRadius && Sim.Pos.Z < Feet.Z + Reach && Flat < BestDist)
		{
			Best = C;
			BestDist = Flat;
		}
	}
	if (Best)
	{
		GiveTo(Best);
		TaGAudio::PlayAt(this, TEXT("Catch"), Sim.Pos, 0.6f);
		if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
		{
			GM->OnBallPickedUp(Best, false);
		}
	}
}

void ATaGBall::CheckBlocks()
{
	// A defender in the air whose hands meet the rising ball (the shot type decides how blockable it is).
	if (FlightTime > 0.6f)
	{
		return;
	}
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		ATaGCharacter* C = *It;
		if (!C || C->GetTeam() == ShotTeam || BlockRolled.Contains(C) || !C->CanTouchBall())
		{
			continue;
		}
		const bool bAirborne = C->GetCharacterMovement() && C->GetCharacterMovement()->IsFalling();
		if (!bAirborne && !C->IsContesting())
		{
			continue;
		}
		const FVector Hand = C->GetBlockHandLocation();
		if (FVector::Dist(Hand, Sim.Pos) < 45.f * C->GetHeroDef().ReachMul)
		{
			BlockRolled.Add(C);
			const float Chance = FMath::Clamp(0.85f * ShotBlockMult, 0.f, 1.f);
			if (FMath::FRand() < Chance)
			{
				FVector Away(Sim.Pos.X - TaG::RimX, Sim.Pos.Y, 0.f);
				Away = Away.GetSafeNormal() * 420.f;
				MakeLoose(Sim.Pos, Away + FVector(0.f, 0.f, -150.f), C->GetTeam());
				TaGAudio::PlayAt(this, TEXT("Block"), Sim.Pos, 1.f);
				if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
				{
					GM->OnBlocked(C);
				}
				return;
			}
		}
	}
}
