#pragma once

#include "CoreMinimal.h"
#include "TaGRules.generated.h"

class ATaGCharacter;

/** Shot types come from combinations of basic inputs (prototype v0.3): no extra buttons. */
UENUM(BlueprintType)
enum class ETaGShotType : uint8
{
	Jumper,
	StepBack,
	Fade,
	Layup,
	Euro,
	Floater,
	Dunk
};

struct FTaGShotTypeDef
{
	const TCHAR* Name = TEXT("");
	float Meter = 1.f;     // seconds for the charge to reach 1.0
	float Mult = 1.f;      // make-chance multiplier
	float Prox = 1.f;      // how much a close defender hurts (lower = resists closeouts)
	float Block = 1.f;     // how blockable
	float Angle = 0.f;     // launch angle override (0 = by distance)
	float JumpAt = 0.4f;   // seconds after the start; < 0 = already airborne
	float JumpVel = 340.f; // auto-jump speed (cm/s); the apex lands near the perfect window
};

/** Result of evaluating a shot at a given charge. */
struct FTaGShotEval
{
	float Chance = 0.f;
	float Timing = 1.f;
	float Contest = 1.f;
	int32 Points = 1;
	bool bPerfect = false;
	bool bContested = false;   // a contest hand won the read (shot < contest)
	bool bOpen = false;        // nobody close (shot > spacing)
	bool bWalled = false;
	float Distance = 0.f;
	ETaGShotType Type = ETaGShotType::Jumper;
	TWeakObjectPtr<ATaGCharacter> Contester;
};

/** Shot model shared by the server (to decide), the HUD (to preview) and the bots. Ported from the Godot
 *  prototype's shot_chance(): the aim locks on, so skill is timing plus where and against whom you shoot. */
struct BASKETTAG_API FTaGRules
{
	static const FTaGShotTypeDef& ShotType(ETaGShotType Type);
	static float TimingQuality(float Charge, float WindowScale = 1.f);
	static float BaseChance(float DistCm);
	static FTaGShotEval EvaluateShot(const ATaGCharacter* Shooter, float Charge, bool bContact);
};
