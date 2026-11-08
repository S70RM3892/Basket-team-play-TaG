// Generic hero arts: the 48 arts after the first twelve run on a handful of mechanics (ETaGArtKind) with numbers and
// colours from TaGHeroData.cpp. The first twelve keep their own code in TaGCharacter.cpp.
#include "Character/TaGCharacter.h"
#include "BasketTaG.h"
#include "Ball/TaGBall.h"
#include "Character/TaGArtActors.h"
#include "Character/TaGRig.h"
#include "Core/TaGAudio.h"
#include "Core/TaGFx.h"
#include "Core/TaGGameMode.h"
#include "Core/TaGGameState.h"
#include "Core/TaGTypes.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	bool IsTackleHand(const FTaGArtDef& Def)
	{
		return Def.Hand == ETaGHand::Tackle;
	}
}

// ================================================================ Status

ETaGArtFx ATaGCharacter::GetShotMod() const
{
	return ServerNow() < ShotModEnd ? static_cast<ETaGArtFx>(ShotMod) : ETaGArtFx::None;
}

void ATaGCharacter::ConsumeShotMod()
{
	ShotModEnd = 0.f;
	ShotMod = 0;
}

ETaGArtFx ATaGCharacter::ConsumeCounter()
{
	if (!HasCounter())
	{
		return ETaGArtFx::None;
	}
	const ETaGArtFx Fx = static_cast<ETaGArtFx>(CounterKind);
	CounterEnd = 0.f;
	return Fx;
}

int32 ATaGCharacter::ConsumeMark()
{
	if (ServerNow() >= MarkEnd)
	{
		return 0;
	}
	MarkEnd = 0.f;
	return MarkPower;
}

const FTaGArtDef* ATaGCharacter::GetContestArt() const
{
	if (Action == ETaGAction::Contest && ActionVariant > static_cast<uint8>(ETaGArt::Resync) && ActionVariant < static_cast<uint8>(ETaGArt::Count))
	{
		const FTaGArtDef& Def = TaGHeroes::GetArt(static_cast<ETaGArt>(ActionVariant));
		if (Def.Kind == ETaGArtKind::ContestStance)
		{
			return &Def;
		}
	}
	return nullptr;
}

TArray<ATaGCharacter*> ATaGCharacter::TeamInRadius(float Radius, bool bIncludeSelf) const
{
	TArray<ATaGCharacter*> Out;
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() != Team || (!bIncludeSelf && *It == this))
		{
			continue;
		}
		if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= Radius)
		{
			Out.Add(*It);
		}
	}
	return Out;
}

void ATaGCharacter::ApplyArtStatus(ETaGArtFx Fx, float Duration, float Power)
{
	if (!HasAuthority())
	{
		return;
	}
	const float Now = ServerNow();
	switch (Fx)
	{
	case ETaGArtFx::Root:
		RootEnd = FMath::Max(RootEnd, Now + Duration);
		break;
	case ETaGArtFx::Blind:
		BlindEnd = FMath::Max(BlindEnd, Now + Duration);
		break;
	case ETaGArtFx::ShotDebuff:
		ShotDebuffEnd = FMath::Max(ShotDebuffEnd, Now + Duration);
		break;
	case ETaGArtFx::Heal:
		Guard = GetMaxGuard();
		SlowEnd = 0.f;
		RootEnd = 0.f;
		if (IsDown())
		{
			Revive();
		}
		break;
	case ETaGArtFx::Reset:
		ResetHandCooldowns();
		Guard = FMath::Min(GetMaxGuard(), Guard + Power);
		break;
	case ETaGArtFx::Speed:
		AddSpeedBoost(Duration);
		Guard = FMath::Min(GetMaxGuard(), Guard + Power);
		break;
	case ETaGArtFx::Intercept:
		InterceptEnd = FMath::Max(InterceptEnd, Now + Duration);
		break;
	case ETaGArtFx::Mark:
		MarkEnd = Now + Duration;
		MarkPower = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Power), 1, 3));
		break;
	case ETaGArtFx::FieldFeast:
		RegenEnd = FMath::Max(RegenEnd, Now + Duration);
		break;
	default:
		break;
	}
}

// ================================================================ Use

bool ATaGCharacter::CanUseGenericArt(const FTaGArtDef& Def) const
{
	const bool bFree = Action == ETaGAction::None;
	const bool bBall = HasBall();
	if ((Def.Need & TaGArtNeed::Free) && !bFree)
	{
		return false;
	}
	if ((Def.Need & TaGArtNeed::Ball) && !bBall)
	{
		return false;
	}
	if ((Def.Need & TaGArtNeed::NoBall) && bBall)
	{
		return false;
	}
	if (Action == ETaGAction::Down || Action == ETaGAction::Stagger)
	{
		return false;
	}
	if (Def.Need & TaGArtNeed::Ally)
	{
		bool bAlly = false;
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It && !bAlly; ++It)
		{
			bAlly = *It != this && It->GetTeam() == Team;
		}
		if (!bAlly)
		{
			return false;
		}
	}
	switch (Def.Kind)
	{
	case ETaGArtKind::Pass:
		return bBall && (bFree || Action == ETaGAction::Gathered) && FindBestPassTarget(GetAimDirection()) != nullptr;
	case ETaGArtKind::ShotBuff:
		return bBall && (bFree || Action == ETaGAction::Gathered || Action == ETaGAction::Drive);
	case ETaGArtKind::Counter:
		return bFree || Action == ETaGAction::Gathered;
	case ETaGArtKind::TeamOne:
		return TeamInRadius(Def.Range, false).Num() > 0;
	case ETaGArtKind::ContestAt:
	{
		const ATaGBall* B = FindBall();
		const ATaGCharacter* H = B ? B->GetHolder() : nullptr;
		return bFree && H && H->GetTeam() != Team && FVector::Dist2D(H->GetActorLocation(), GetActorLocation()) <= Def.Range;
	}
	default:
		return true;
	}
}

void ATaGCharacter::StartArtDrive(const FTaGArtDef& Def)
{
	bHadBallOnDrive = HasBall();
	bDriveBeatDone = false;
	ArtHits.Reset();
	SkateTrailT = 0.f;
	SetAction(ETaGAction::DriveStart, static_cast<uint8>(Def.Id));
}

void ATaGCharacter::ExecuteGenericArt(const FTaGArtDef& Def)
{
	UWorld* World = GetWorld();
	const bool bAuth = HasAuthority();
	const float Now = ServerNow();
	const uint8 V = static_cast<uint8>(Def.Id);
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	if (bAuth)
	{
		MulticastArtFx(V, 0, GetActorLocation(), GetActorLocation() + Fwd * FMath::Max(Def.Range, 100.f));
	}
	switch (Def.Kind)
	{
	case ETaGArtKind::Dash:
		if (Def.Fx == ETaGArtFx::DashHop)
		{
			SetAction(ETaGAction::ArtStart, V); // a short blink after a tiny crouch
		}
		else
		{
			StartArtDrive(Def);
		}
		break;
	case ETaGArtKind::Leap:
	{
		FVector Dir = WishDir();
		if (Dir.IsNearlyZero())
		{
			Dir = Fwd;
		}
		const float Up = 430.f;
		const float Air = 2.f * Up / TaG::Gravity;
		bHadBallOnDrive = HasBall();
		bDriveBeatDone = false;
		SetAction(ETaGAction::Drive, V, Dir, Air);
		LaunchCharacter(Dir * (Def.Range / Air) + FVector(0.f, 0.f, Up), true, true);
		break;
	}
	case ETaGArtKind::Shell:
		if (bAuth)
		{
			InvulnEnd = Now + Def.Duration;
			RootEnd = Now + Def.Duration;
		}
		break;
	case ETaGArtKind::Phase:
		if (bAuth)
		{
			PhaseEnd = Now + Def.Duration;
			AddSpeedBoost(Def.Duration);
			MulticastFx(2, GetFeetLocation(), GetFeetLocation(), nullptr); // afterimage ghost where it started
		}
		break;
	case ETaGArtKind::Counter:
		if (bAuth)
		{
			CounterEnd = Now + Def.Duration;
			CounterKind = static_cast<uint8>(Def.Fx);
		}
		break;
	case ETaGArtKind::Decoy:
		StartDodge(true);
		if (bAuth)
		{
			for (TActorIterator<ATaGCharacter> It(World); It; ++It)
			{
				if (It->GetTeam() != Team && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < Def.Radius)
				{
					It->ApplySlow(1.f, 1.35f, Def.Duration);
					MulticastArtFx(V, 1, GetActorLocation(), It->GetActorLocation());
				}
			}
		}
		break;
	case ETaGArtKind::ShotBuff:
		if (bAuth)
		{
			ShotModEnd = Now + Def.Duration;
			ShotMod = static_cast<uint8>(Def.Fx);
		}
		break;
	case ETaGArtKind::Wall:
		SetAction(ETaGAction::ArtStart, V);
		if (bAuth)
		{
			const int32 N = FMath::Max(Def.Count, 1);
			for (int32 i = 0; i < N; ++i)
			{
				const float Ang = N == 1 ? 0.f : FMath::Lerp(-38.f, 38.f, float(i) / float(N - 1));
				const FVector Dir = Fwd.RotateAngleAxis(Ang, FVector::UpVector);
				const FVector P = GetFeetLocation() + Dir * Def.Range + FVector(0.f, 0.f, ATaGWallActor::Height * 0.5f + 1.f);
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ATaGWallActor* W = World->SpawnActor<ATaGWallActor>(ATaGWallActor::StaticClass(), P, Dir.Rotation(), Params))
				{
					W->SetActorScale3D(FVector(1.f, Def.Power, 1.f));
					W->Init(Team, Def.Duration, static_cast<uint8>(Def.Id));
				}
			}
		}
		break;
	case ETaGArtKind::ShieldFollow:
		if (bAuth)
		{
			ShieldEnd = Now + Def.Duration;
			const FVector P = GetActorLocation() + Fwd * Def.Range + FVector(0.f, 0.f, ATaGWallActor::Height * 0.5f - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 1.f);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ATaGWallActor* W = World->SpawnActor<ATaGWallActor>(ATaGWallActor::StaticClass(), P, Fwd.Rotation(), Params))
			{
				W->SetActorScale3D(FVector(0.6f, Def.Power, 0.85f));
				W->Init(Team, Def.Duration, V);
				W->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				GetCapsuleComponent()->IgnoreActorWhenMoving(W, true);
			}
		}
		break;
	case ETaGArtKind::Root:
		if (bAuth)
		{
			RootEnd = Now + Def.Duration;
			RootWallEnd = Now + Def.Duration;
			RootWallRadius = Def.Radius;
			if (ATaGFieldActor* Field = World->SpawnActor<ATaGFieldActor>(ATaGFieldActor::StaticClass(), GetFeetLocation(), FRotator::ZeroRotator))
			{
				Field->Init(ETaGFieldKind::Root, Team, Def.Radius, Def.Duration, this);
			}
		}
		break;
	case ETaGArtKind::Tackle:
		TackleArt = Def.Id;
		SetAction(ETaGAction::TackleStart, V);
		break;
	case ETaGArtKind::Cone:
	case ETaGArtKind::Aura:
	case ETaGArtKind::Bolt:
		SetAction(ETaGAction::ArtStart, V);
		break;
	case ETaGArtKind::Field:
		if (bAuth)
		{
			ETaGFieldKind FK = ETaGFieldKind::Slow;
			switch (Def.Fx)
			{
			case ETaGArtFx::FieldSpore: FK = ETaGFieldKind::Spore; break;
			case ETaGArtFx::FieldWind: FK = ETaGFieldKind::Wind; break;
			case ETaGArtFx::FieldFeast: FK = ETaGFieldKind::Feast; break;
			case ETaGArtFx::FieldMist: FK = ETaGFieldKind::Mist; break;
			case ETaGArtFx::FieldIce: FK = ETaGFieldKind::Ice; break;
			default: break;
			}
			const FVector F = GetFeetLocation();
			if (ATaGFieldActor* Field = World->SpawnActor<ATaGFieldActor>(ATaGFieldActor::StaticClass(), FVector(F.X, F.Y, F.Z + 2.f), FRotator::ZeroRotator))
			{
				Field->Init(FK, Team, Def.Radius, Def.Duration, nullptr);
			}
		}
		break;
	case ETaGArtKind::TeamAura:
		if (bAuth)
		{
			const bool bSelf = Def.Fx != ETaGArtFx::Reset;
			for (ATaGCharacter* Mate : TeamInRadius(Def.Radius, bSelf))
			{
				Mate->ApplyArtStatus(Def.Fx, Def.Duration, Def.Power);
				MulticastArtFx(V, 1, GetActorLocation(), Mate->GetActorLocation());
			}
		}
		break;
	case ETaGArtKind::TeamOne:
		if (bAuth)
		{
			ATaGCharacter* Best = nullptr;
			float BestD = Def.Range;
			for (ATaGCharacter* Mate : TeamInRadius(Def.Range, false))
			{
				// Lanterns go to whoever is most open; supplies to whoever is closest.
				const float D = FVector::Dist2D(Mate->GetActorLocation(), GetActorLocation());
				if (D <= BestD)
				{
					BestD = D;
					Best = Mate;
				}
			}
			if (Best)
			{
				Best->ApplyArtStatus(Def.Fx, Def.Duration, Def.Power);
				MulticastArtFx(V, 1, GetActorLocation(), Best->GetActorLocation());
			}
		}
		break;
	case ETaGArtKind::Pass:
		PendingPassStyle = Def.Fx == ETaGArtFx::PassLob ? 3 : 2;
		TryPass(false);
		PendingPassStyle = 0;
		break;
	case ETaGArtKind::ContestStance:
		bContestHeld = false;
		SetAction(ETaGAction::Contest, V, FVector::ZeroVector, Def.Duration);
		break;
	case ETaGArtKind::ContestAt:
		if (bAuth)
		{
			const ATaGBall* B = FindBall();
			ATaGCharacter* H = B ? B->GetHolder() : nullptr;
			if (H && H->GetTeam() != Team)
			{
				if (ATaGFieldActor* Field = World->SpawnActor<ATaGFieldActor>(ATaGFieldActor::StaticClass(), H->GetFeetLocation(), FRotator::ZeroRotator))
				{
					Field->Init(ETaGFieldKind::Swarm, Team, Def.Radius, Def.Duration, H);
				}
				MulticastArtFx(V, 1, GetActorLocation(), H->GetActorLocation());
			}
		}
		break;
	default:
		break;
	}
}

void ATaGCharacter::FireGenericArt(const FTaGArtDef& Def)
{
	if (!HasAuthority())
	{
		return;
	}
	UWorld* World = GetWorld();
	const FVector Me = GetActorLocation();
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	switch (Def.Kind)
	{
	case ETaGArtKind::Cone:
	{
		const float CosHalf = FMath::Cos(FMath::DegreesToRadians(Def.Radius));
		for (TActorIterator<ATaGCharacter> It(World); It; ++It)
		{
			if (It->GetTeam() == Team || It->IsDown())
			{
				continue;
			}
			FVector To = It->GetActorLocation() - Me;
			To.Z = 0.f;
			const float D = To.Size();
			const float Reach = Def.Range + It->GetCapsuleComponent()->GetScaledCapsuleRadius();
			if (D < Reach && (D < 60.f || FVector::DotProduct(To / FMath::Max(D, 1.f), Fwd) > CosHalf))
			{
				HitWithArt(*It, Def, Fwd * Def.Power);
			}
		}
		break;
	}
	case ETaGArtKind::Aura:
		for (TActorIterator<ATaGCharacter> It(World); It; ++It)
		{
			if (It->GetTeam() == Team || It->IsDown())
			{
				continue;
			}
			FVector To = It->GetActorLocation() - Me;
			To.Z = 0.f;
			if (To.Size() < Def.Radius + It->GetCapsuleComponent()->GetScaledCapsuleRadius())
			{
				HitWithArt(*It, Def, To.GetSafeNormal() * Def.Power);
			}
		}
		break;
	case ETaGArtKind::Bolt:
	{
		const FVector Eye = GetEyeLocation();
		FVector Dir = GetAimDirection();
		Dir.Z = FMath::Clamp(Dir.Z, -0.3f, 0.3f);
		Dir.Normalize();
		const FVector End = Eye + Dir * Def.Range;
		ATaGCharacter* Best = nullptr;
		float BestT = 1e9f;
		for (TActorIterator<ATaGCharacter> It(World); It; ++It)
		{
			if (*It == this || It->GetTeam() == Team || It->IsDown())
			{
				continue;
			}
			const FVector P = It->GetActorLocation();
			const FVector C = FMath::ClosestPointOnSegment(P, Eye, End);
			const float Along = FVector::DotProduct(C - Eye, Dir);
			if (FVector::Dist2D(P, C) < 55.f + It->GetCapsuleComponent()->GetScaledCapsuleRadius() && Along > 0.f && Along < BestT)
			{
				BestT = Along;
				Best = *It;
			}
		}
		MulticastArtFx(static_cast<uint8>(Def.Id), Best ? 1 : 0, Eye - FVector(0.f, 0.f, 25.f), Best ? Best->GetActorLocation() : End);
		if (Best)
		{
			HitWithArt(Best, Def, (Me - Best->GetActorLocation()).GetSafeNormal2D() * 150.f);
		}
		break;
	}
	default:
		break;
	}
}

void ATaGCharacter::HitWithArt(ATaGCharacter* T, const FTaGArtDef& Def, const FVector& Push)
{
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	if (!T || !GM || !HasAuthority())
	{
		return;
	}
	const uint8 V = static_cast<uint8>(Def.Id);
	// A shell shrugs everything off
	if (T->IsInvulnerable())
	{
		MulticastArtFx(V, 2, GetActorLocation(), T->GetActorLocation());
		GM->Popup(FString::Printf(TEXT("%s は殻で弾いた"), T->GetHeroDef().Name), T->GetTeam());
		return;
	}
	// Dodges and phasing beat the tackle-type arts (evade > tackle)
	if (IsTackleHand(Def) && (T->IsDodging() || T->IsPhasing()))
	{
		T->MarkEvaded();
		ForceRecover(0.9f);
		MulticastArtFx(V, 2, GetActorLocation(), T->GetActorLocation());
		GM->ReadWin(T, this, FString::Printf(TEXT("かわし ＞ %s"), Def.Name));
		return;
	}
	MulticastArtFx(V, 1, GetActorLocation(), T->GetActorLocation());
	const bool bDriving = T->IsDriving();
	const bool bHadBall = T->HasBall();
	ATaGBall* Ball = FindBall();
	switch (Def.Fx)
	{
	case ETaGArtFx::Stagger:
	case ETaGArtFx::Root:
		if (bDriving)
		{
			// Like a tackle: a drive caught by it goes down and drops the ball
			T->KnockDown(Push.IsNearlyZero() ? GetActorForwardVector() : Push.GetSafeNormal());
			if (bHadBall && Ball)
			{
				Ball->MakeLoose(Ball->GetActorLocation(), (GetActorLocation() - T->GetActorLocation()).GetSafeNormal2D() * 300.f + FVector(0.f, 0.f, 250.f), Team);
			}
			GM->ReadWin(this, T, FString::Printf(TEXT("%s ＞ 突破"), Def.Name));
		}
		else if (Def.Fx == ETaGArtFx::Root)
		{
			T->ApplyArtStatus(ETaGArtFx::Root, Def.Duration, 0.f);
			GM->Popup(FString::Printf(TEXT("%s！ %s が止まった"), Def.Name, T->GetHeroDef().Name), Team);
		}
		else
		{
			T->Stagger(Def.Duration, Push);
			T->ApplyGuardDamage(15.f);
			GM->Popup(FString::Printf(TEXT("%s！"), Def.Name), Team);
		}
		break;
	case ETaGArtFx::Push:
		T->Stagger(0.35f, Push + FVector(0.f, 0.f, 80.f));
		if (Def.Duration > 0.f)
		{
			T->ApplySlow(0.7f, 1.15f, Def.Duration);
		}
		break;
	case ETaGArtFx::GuardDamage:
		if (T->IsChargingShot())
		{
			T->ForceContactShot(); // shot vs. the discharge: it goes up with contact
		}
		T->ApplyGuardDamage(Def.Power);
		T->Stagger(Def.Duration, Push * 0.3f);
		break;
	case ETaGArtFx::BallSteal:
		if (bHadBall && Ball && !T->IsChargingShot())
		{
			const FVector From = Ball->GetActorLocation();
			const FVector ToMe = (GetActorLocation() - From).GetSafeNormal2D();
			Ball->MakeLoose(From, ToMe * 520.f + FVector(0.f, 0.f, 260.f), Team);
			GM->Popup(FString::Printf(TEXT("%s！ ボールを奪った"), Def.Name), Team);
		}
		else
		{
			T->Stagger(0.25f, Push * 0.4f);
		}
		break;
	case ETaGArtFx::Blind:
	case ETaGArtFx::ShotDebuff:
		T->ApplyArtStatus(Def.Fx, Def.Duration, 0.f);
		break;
	default:
		break;
	}
}

// ================================================================ Per tick

void ATaGCharacter::TickArtStatus(float Dt)
{
	const float Now = ServerNow();
	// Phasing: bodies pass through each other (owner and server move the capsule)
	const bool bPhase = Now < PhaseEnd;
	if (bPhase != bPhaseCollision)
	{
		bPhaseCollision = bPhase;
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, bPhase ? ECR_Ignore : ECR_Block);
	}
	if (!HasAuthority())
	{
		return;
	}
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	// Feast field: fast guard recovery
	if (Now < RegenEnd && Guard < GetMaxGuard())
	{
		Guard = FMath::Min(GetMaxGuard(), Guard + 40.f * Dt);
	}
	if (Action != ETaGAction::Drive || ActionVariant <= static_cast<uint8>(ETaGArt::Resync) || !GM || !GM->IsLive())
	{
		return;
	}
	const FTaGArtDef& Def = TaGHeroes::GetArt(static_cast<ETaGArt>(ActionVariant));
	if (Def.Fx == ETaGArtFx::DashSpark)
	{
		// Sparks: defenders brushed by the drive stagger (once each)
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetTeam() == Team || ArtHits.Contains(*It) || It->IsDown())
			{
				continue;
			}
			if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 115.f + It->GetCapsuleComponent()->GetScaledCapsuleRadius())
			{
				ArtHits.Add(*It);
				HitWithArt(*It, Def, (It->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() * 220.f);
			}
		}
	}
	else if (Def.Fx == ETaGArtFx::DashSkate)
	{
		// The skate leaves ice behind
		SkateTrailT -= Dt;
		if (SkateTrailT <= 0.f)
		{
			SkateTrailT = 0.1f;
			const FVector F = GetFeetLocation();
			if (ATaGFieldActor* Field = GetWorld()->SpawnActor<ATaGFieldActor>(ATaGFieldActor::StaticClass(), FVector(F.X, F.Y, F.Z + 2.f), FRotator::ZeroRotator))
			{
				Field->Init(ETaGFieldKind::Ice, Team, 120.f, 2.2f, nullptr);
			}
		}
	}
}

// ================================================================ Visuals

void ATaGCharacter::MulticastArtFx_Implementation(uint8 ArtId, uint8 Stage, FVector_NetQuantize A, FVector_NetQuantize B)
{
	if (GetNetMode() == NM_DedicatedServer || ArtId >= static_cast<uint8>(ETaGArt::Count))
	{
		return;
	}
	TaGFx::Art(this, TaGHeroes::GetArt(static_cast<ETaGArt>(ArtId)), Stage, FVector(A), FVector(B));
}

// ================================================================ Hero ambience

void ATaGCharacter::TickHeroAmbience(float Dt)
{
	if (GetNetMode() == NM_DedicatedServer || (IsLocallyControlled() && IsPlayerControlled() && !bThirdPerson))
	{
		return; // nothing in front of your own camera
	}
	AmbientT -= Dt;
	if (AmbientT > 0.f)
	{
		return;
	}
	const FTaGHeroDef& H = GetHeroDef();
	const FVector Feet = GetFeetLocation();
	const float Eye = H.EyeHeight;
	const FVector Hand = Rig ? Rig->GetHandWorld(FMath::RandBool()) : GetActorLocation();
	const FVector Body = Feet + FVector(0.f, 0.f, FMath::FRandRange(0.25f, 0.9f) * Eye);
	const FVector Back = GetActorLocation() - GetActorForwardVector() * H.CapsuleRadius;
	switch (H.Id)
	{
	case ETaGHero::Hibana: // sparks crackle at the fists
		AmbientT = 0.12f;
		TaGFx::Burst(this, Hand, FLinearColor(1.f, 0.75f, 0.25f), 2, 170.f, 0.3f, 5.f, 700.f, ETaGFxShape::Spark);
		break;
	case ETaGHero::Kotetsu: // the welding torch
		AmbientT = 0.14f;
		TaGFx::Burst(this, Hand, FLinearColor(0.5f, 0.85f, 1.f), 2, 140.f, 0.25f, 5.f, 600.f, ETaGFxShape::Spark);
		break;
	case ETaGHero::Hotaru: // fireflies drifting around
	case ETaGHero::Tsubame: // drone glints
		AmbientT = 0.16f;
		TaGFx::Aura(this, Feet, Eye, H.CapsuleRadius + 25.f, H.Id == ETaGHero::Hotaru ? FLinearColor(0.85f, 1.f, 0.35f) : FLinearColor(0.7f, 0.85f, 1.f), 1);
		break;
	case ETaGHero::Hibiki: // light bubbles from the jelly
		AmbientT = 0.2f;
		TaGFx::Burst(this, Body, FMath::RandBool() ? FLinearColor(0.7f, 0.45f, 1.f) : FLinearColor(0.35f, 1.f, 1.f), 1, 40.f, 0.9f, 7.f, -60.f);
		break;
	case ETaGHero::Mugura: // electric crackle along the body
	case ETaGHero::Ushio:
		AmbientT = 0.22f;
		TaGFx::Burst(this, Body, FLinearColor(0.45f, 0.95f, 1.f), 3, 230.f, 0.14f, 4.f, 0.f, ETaGFxShape::Spark);
		break;
	case ETaGHero::Tsurara: // frost motes
		AmbientT = 0.18f;
		TaGFx::Burst(this, Body + FMath::VRand() * 20.f, FLinearColor(0.85f, 0.95f, 1.f), 1, 25.f, 1.1f, 5.f, 25.f);
		break;
	case ETaGHero::Oboro: // drifting mist
		AmbientT = 0.18f;
		TaGFx::Burst(this, Feet + FVector(0.f, 0.f, FMath::FRandRange(10.f, 60.f)), FLinearColor(0.8f, 0.76f, 0.95f, 0.35f), 1, 35.f, 1.3f, 34.f, -20.f, ETaGFxShape::Puff);
		break;
	case ETaGHero::Kokemaru: // spores
		AmbientT = 0.25f;
		TaGFx::Burst(this, Body, FLinearColor(0.65f, 1.f, 0.7f), 1, 30.f, 1.4f, 5.f, -40.f);
		break;
	case ETaGHero::Tomoshibi: // light from the lantern head
	case ETaGHero::Ankou:     // the glowing lure
		AmbientT = 0.2f;
		TaGFx::Burst(this, Feet + FVector(0.f, 0.f, Eye + 18.f) + GetActorForwardVector() * (H.Id == ETaGHero::Ankou ? 30.f : 0.f), FLinearColor(1.f, 0.88f, 0.5f), 1, 30.f, 0.9f, 6.f, -30.f);
		break;
	case ETaGHero::Yatai: // steam from the pot
		AmbientT = 0.28f;
		TaGFx::Burst(this, Feet + FVector(0.f, 0.f, Eye + 30.f), FLinearColor(1.f, 1.f, 1.f, 0.3f), 1, 30.f, 1.4f, 30.f, -70.f, ETaGFxShape::Puff);
		break;
	case ETaGHero::Gantetsu: // embers from the back chimney
		AmbientT = 0.3f;
		TaGFx::Burst(this, Back + FVector(0.f, 0.f, Eye * 0.45f), FLinearColor(1.f, 0.5f, 0.15f), 1, 40.f, 1.f, 5.f, -90.f);
		break;
	case ETaGHero::Aoji: // water dripping from the gate
		AmbientT = 0.25f;
		TaGFx::Burst(this, Feet + FVector(0.f, 0.f, Eye * 0.6f) + FMath::VRand() * 30.f, FLinearColor(0.45f, 0.9f, 1.f), 1, 20.f, 0.7f, 5.f, 600.f);
		break;
	case ETaGHero::Rei: // shards breaking off the coat
		AmbientT = 0.35f;
		TaGFx::Burst(this, Feet + FVector(0.f, 0.f, 40.f), FLinearColor(0.3f, 0.95f, 1.f), 1, 50.f, 0.8f, 5.f, -30.f, ETaGFxShape::Spark);
		break;
	case ETaGHero::Kazami: // wind streaks
		AmbientT = 0.25f;
		TaGFx::Burst(this, Body, FLinearColor(0.9f, 0.97f, 1.f, 0.7f), 1, 260.f, 0.25f, 4.f, 0.f, ETaGFxShape::Spark, GetActorRightVector() * 200.f);
		break;
	default:
		AmbientT = 1.f;
		break;
	}
}
