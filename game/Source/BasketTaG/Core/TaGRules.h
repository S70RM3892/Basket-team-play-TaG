#pragma once

#include "CoreMinimal.h"
#include "Core/TaGHeroData.h"
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
	Dunk,
	AlleyOop    // a lob caught in the air and finished before landing (no meter)
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
	bool bArtContest = false;  // the defender that matters contests with an art (the bodies do not count)
	float ReachGap = 0.f;      // that defender's reach (with the jump) minus the shooter's, cm
	bool bReached = false;     // a defender close enough to touch the shot (the gap above means something)
	float Distance = 0.f;
	ETaGShotType Type = ETaGShotType::Jumper;
	TWeakObjectPtr<ATaGCharacter> Contester;
};

/**
 * The run-up into a layup or a dunk: the gather, two steps (FIBA Official Basketball Rules, Art. 25, which also allows
 * the gather step) and a one-foot take-off. The last step plants the take-off foot ahead of the body: the plant checks
 * the run and turns part of it into lift. Long jumpers lower the body on the second-last stride and stay low until the
 * take-off support raises it (Hay & Nohara 1990); a one-foot take-off gains height from the swing leg driven up
 * (Vint & Hinrichs 1996). The take-off time and the speeds are fitted to the distance, so the top of the jump comes in
 * front of the rim from wherever, and however fast, the shot was started.
 */
struct FTaGRunUp
{
	bool bValid = false;
	float TakeoffT = 0.5f;       // gather -> the take-off foot leaves the floor (s)
	float ApproachSpeed = 400.f; // cm/s until the plant
	float TakeoffSpeed = 220.f;  // horizontal cm/s carried into the air
	float JumpVZ = 400.f;        // cm/s
	bool bLeftHand = false;      // the finishing hand; the take-off foot is the other one
};

/** An alley-oop: a lob that meets a cutter's hands at the top of a running jump in front of the rim. */
struct FTaGAlleyOop
{
	bool bValid = false;
	FVector Catch = FVector::ZeroVector;   // where the ball meets the hands
	float FlightT = 0.8f;                  // the lob's time in the air (s)
	float LeapVZ = 440.f;                  // take-off speed that brings the hands to the catch height at the top
	bool bSlam = true;                     // high enough to throw it down; otherwise laid in off the catch
};

/** Shot model shared by the server (to decide), the HUD (to preview) and the bots. Ported from the Godot
 *  prototype's shot_chance(): the aim locks on, so skill is timing plus where and against whom you shoot. */
struct BASKETTAG_API FTaGRules
{
	static const FTaGShotTypeDef& ShotType(ETaGShotType Type);
	static float TimingQuality(float Charge, float WindowScale = 1.f);
	static float BaseChance(float DistCm);
	static FTaGShotEval EvaluateShot(const ATaGCharacter* Shooter, float Charge, bool bContact);

	/** Seconds the take-off foot stays down in a run-up (the plant). */
	static constexpr float RunUpStanceT = 0.16f;
	/** A run-up's meter reaches this at the top of the jump (the middle of the perfect window). */
	static constexpr float RunUpApexCharge = 0.74f;
	/** How far out a layup starts (cm): a standing step-in 3.2 m, running in at full speed about 4.2 m. */
	static float LayupRange(float SpeedToRim);
	static FTaGRunUp PlanRunUp(const FTaGHeroDef& Hero, ETaGShotType Type, const FVector& Feet, const FVector& Velocity, float StickLateral);
	/** Where a one-handed layup leaves the fingers: overhead and in front, on the finishing hand's side. */
	static FVector LayupRelease(const FVector& Feet, const FVector& Forward, float EyeHeight, bool bLeftHand);
	/** Hands overhead with the arms up: standing reach is about 1.33 x eye height. */
	static float StandingReach(const FTaGHeroDef& Hero) { return Hero.EyeHeight * 1.33f; }
	/** A lob for a teammate running at the rim, or bValid = false when the cut does not allow one. */
	static FTaGAlleyOop PlanAlleyOop(const ATaGCharacter* Passer, const ATaGCharacter* Receiver);
};
