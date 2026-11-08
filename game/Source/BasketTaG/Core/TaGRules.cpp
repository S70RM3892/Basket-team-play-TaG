#include "Core/TaGRules.h"
#include "Core/TaGTypes.h"
#include "Character/TaGCharacter.h"
#include "Character/TaGArtActors.h"
#include "Ball/TaGBall.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	// Values from prototype/scripts/athlete.gd SHOT_TYPES (jump speeds converted to cm/s).
	const FTaGShotTypeDef GShotTypes[] = {
		{ TEXT("ジャンプショット"), 1.00f, 1.00f, 1.00f, 1.0f, 0.f, 0.40f, 340.f },
		{ TEXT("ステップバック"), 1.28f, 0.95f, 1.00f, 0.8f, 0.f, 0.60f, 340.f },
		{ TEXT("フェイダウェイ"), 0.40f, 0.86f, 0.45f, 0.4f, 56.f, -1.f, 0.f },
		{ TEXT("レイアップ"), 1.30f, 1.05f, 1.00f, 1.3f, 0.f, 0.55f, 400.f },
		{ TEXT("ユーロステップ"), 1.35f, 1.00f, 0.50f, 0.6f, 0.f, 0.60f, 400.f },
		{ TEXT("フローター"), 0.95f, 0.95f, 0.60f, 0.3f, 66.f, 0.40f, 300.f },
		{ TEXT("ダンク"), 0.80f, 1.00f, 0.70f, 1.5f, 0.f, 0.15f, 440.f },
	};
}

const FTaGShotTypeDef& FTaGRules::ShotType(ETaGShotType Type)
{
	return GShotTypes[FMath::Clamp(static_cast<int32>(Type), 0, 6)];
}

float FTaGRules::TimingQuality(float Charge, float WindowScale)
{
	const float Mid = (TaG::ShotPerfectLo + TaG::ShotPerfectHi) * 0.5f;
	const float PerfHalf = (TaG::ShotPerfectHi - TaG::ShotPerfectLo) * 0.5f * WindowScale;
	const float GoodLo = TaG::ShotGoodLo;
	const float GoodHi = TaG::ShotGoodHi;
	if (FMath::Abs(Charge - Mid) <= PerfHalf)
	{
		return 1.12f;
	}
	if (Charge >= GoodLo && Charge <= GoodHi)
	{
		return 1.f;
	}
	if (Charge < GoodLo)
	{
		return FMath::Lerp(0.7f, 1.f, FMath::Clamp(Charge / GoodLo, 0.f, 1.f));
	}
	return FMath::Clamp(FMath::Lerp(1.f, 0.7f, (Charge - GoodHi) / 0.36f), 0.65f, 1.f);
}

float FTaGRules::BaseChance(float DistCm)
{
	const float M = DistCm / 100.f;
	if (M < 1.3f)
	{
		return 0.92f;
	}
	if (M < 6.75f)
	{
		return FMath::Lerp(0.78f, 0.60f, (M - 1.3f) / 5.45f);
	}
	if (M < 9.5f)
	{
		return FMath::Lerp(0.55f, 0.35f, (M - 6.75f) / 2.75f);
	}
	return 0.1f;
}

FTaGShotEval FTaGRules::EvaluateShot(const ATaGCharacter* Shooter, float Charge, bool bContact)
{
	FTaGShotEval E;
	if (!Shooter)
	{
		return E;
	}
	const FVector Feet = Shooter->GetFeetLocation();
	E.Type = Shooter->GetShotType();
	const FTaGShotTypeDef& St = ShotType(E.Type);
	E.Distance = TaG::DistToRim(Feet);
	E.Points = TaG::IsBeyondArc(Feet) ? 2 : 1;

	// Distance and type
	float Base = BaseChance(E.Distance);
	if (E.Type == ETaGShotType::Dunk)
	{
		Base = 0.96f;
	}
	else
	{
		Base *= St.Mult;
	}
	if (Shooter->IsScoped() && E.Distance >= 500.f)
	{
		Base += 0.12f; // Hayate: Scope
	}

	// Timing (Scope widens the perfect window; catch-and-shoot never punishes timing)
	E.Timing = TimingQuality(Charge, Shooter->IsScoped() ? 1.6f : 1.f);
	if (Shooter->IsCatchAndShoot() || Shooter->IsFreshCatch())
	{
		E.Timing = FMath::Max(E.Timing, 1.f); // catch and shoot: rhythm from the pass
	}
	// The core ball charges as it passes through teammates' hands (WORLD.md section 3): team play pays.
	if (const ATaGBall* Ball = Shooter->FindBall())
	{
		Base *= 1.f + 0.04f * FMath::Min(Ball->GetChain(), 3);
	}
	E.bPerfect = E.Timing > 1.05f;

	// Defense: the most effective defender decides.
	const bool bZone = Shooter->IsInZone();
	float Contest = 1.f;
	float NearestDist = TNumericLimits<float>::Max();
	for (TActorIterator<ATaGCharacter> It(Shooter->GetWorld()); It; ++It)
	{
		const ATaGCharacter* Def = *It;
		if (!Def || Def->GetTeam() == Shooter->GetTeam())
		{
			continue;
		}
		const FVector DefFeet = Def->GetFeetLocation();
		const FVector2D ToShooter(Feet.X - DefFeet.X, Feet.Y - DefFeet.Y);
		const float D = ToShooter.Size();
		NearestDist = FMath::Min(NearestDist, D);
		const FVector Fwd = Def->GetActorForwardVector();
		const float Facing = D > 1.f ? FVector2D::DotProduct(FVector2D(Fwd.X, Fwd.Y).GetSafeNormal(), ToShooter / D) : 1.f;

		float C = 1.f;
		bool bThisContest = false;
		if (Def->IsContesting() && Def->IsSonicContest() && D < 400.f)
		{
			C = 0.5f;           // Hibiki's sonic wave: area contest
			bThisContest = true;
		}
		else if (Def->IsContesting() && D < TaG::ContestRange && Facing > 0.2f)
		{
			C = 0.3f;           // contest > shot
			bThisContest = true;
		}
		else if (Def->IsContestStarting() && D < TaG::ContestRange)
		{
			C = 0.6f;
		}
		else if (!Def->IsActiveDefender() || bZone)
		{
			C = 1.f;
		}
		else
		{
			if (D < 110.f) { C = 0.75f; }
			else if (D < 180.f) { C = 0.88f; }
			else if (D < 230.f) { C = 0.96f; }
			// Shot types that resist closeouts (fadeaway, euro step, floater)
			C = 1.f - (1.f - C) * St.Prox;
		}
		if (C < Contest)
		{
			Contest = C;
			if (bThisContest)
			{
				E.bContested = true;
				E.Contester = const_cast<ATaGCharacter*>(Def);
			}
		}
	}

	// Gantetsu's iron wall between the shooter and the rim
	for (TActorIterator<ATaGWallActor> It(Shooter->GetWorld()); It; ++It)
	{
		if (It->GetTeam() != Shooter->GetTeam() && It->BlocksShotFrom(Feet))
		{
			if (0.5f < Contest)
			{
				Contest = 0.5f;
			}
			E.bWalled = true;
		}
	}

	E.bOpen = !E.bContested && !E.bWalled && NearestDist > 230.f && NearestDist < 9000.f;
	E.Contest = Contest * (bContact ? TaG::ContactShotMult : 1.f);
	const float Zone = bZone ? 1.15f : 1.f;
	E.Chance = FMath::Clamp(Base * E.Timing * E.Contest * Zone, 0.02f, 0.97f);
	return E;
}
