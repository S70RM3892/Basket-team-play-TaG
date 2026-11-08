#include "Core/TaGRules.h"
#include "Core/TaGTypes.h"
#include "Character/TaGCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

float FTaGRules::TimingQuality(float Charge)
{
	if (Charge >= TaG::ShotPerfectLo && Charge <= TaG::ShotPerfectHi)
	{
		return 1.12f;
	}
	if (Charge >= TaG::ShotGoodLo && Charge <= TaG::ShotGoodHi)
	{
		return 1.f;
	}
	if (Charge < TaG::ShotGoodLo)
	{
		return FMath::Lerp(0.7f, 1.f, FMath::Clamp(Charge / TaG::ShotGoodLo, 0.f, 1.f));
	}
	return FMath::Clamp(FMath::Lerp(1.f, 0.7f, (Charge - TaG::ShotGoodHi) / 0.36f), 0.65f, 1.f);
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

FTaGShotEval FTaGRules::EvaluateShot(const ATaGCharacter* Shooter, float Charge)
{
	FTaGShotEval E;
	if (!Shooter)
	{
		return E;
	}
	const FVector Feet = Shooter->GetFeetLocation();
	E.Distance = TaG::DistToRim(Feet);
	E.Points = TaG::IsBeyondArc(Feet) ? 2 : 1;
	E.Timing = TimingQuality(Charge);
	E.bPerfect = E.Timing > 1.05f;

	// Closest defender decides how contested the shot is.
	if (const ATaGCharacter* Def = Shooter->FindOpponent())
	{
		const FVector DefFeet = Def->GetFeetLocation();
		const FVector2D ToShooter(Feet.X - DefFeet.X, Feet.Y - DefFeet.Y);
		const float D = ToShooter.Size();
		const FVector DefFwd = Def->GetActorForwardVector();
		const float Facing = D > 1.f ? FVector2D::DotProduct(FVector2D(DefFwd.X, DefFwd.Y).GetSafeNormal(), ToShooter / D) : 1.f;
		const bool bDefActive = !Def->IsRecovering();
		const bool bDefUp = Def->GetCharacterMovement() && Def->GetCharacterMovement()->IsFalling();

		if (bDefActive && bDefUp && D < TaG::ContestRange && Facing > 0.3f)
		{
			E.Contest = 0.45f;   // a hand in the face
			E.bContested = true;
		}
		else if (!bDefActive)
		{
			E.Contest = 1.f;
		}
		else if (D < 110.f)
		{
			E.Contest = 0.75f;
		}
		else if (D < 180.f)
		{
			E.Contest = 0.88f;
		}
		else if (D < 230.f)
		{
			E.Contest = 0.96f;
		}
	}

	E.Chance = FMath::Clamp(BaseChance(E.Distance) * E.Timing * E.Contest, 0.02f, 0.97f);
	return E;
}
