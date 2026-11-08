#pragma once

#include "CoreMinimal.h"

class ATaGCharacter;

/** Result of evaluating a shot at a given charge time. */
struct FTaGShotEval
{
	float Chance = 0.f;   // 0..1
	float Timing = 1.f;   // timing multiplier (1.12 = perfect)
	float Contest = 1.f;  // defender multiplier
	int32 Points = 1;     // 3x3: 1 inside the arc, 2 outside
	bool bPerfect = false;
	bool bContested = false;
	float Distance = 0.f; // cm to the rim
};

/** Shot model shared by the server (to decide), the HUD (to preview) and the bots. Mirrors the
 *  Godot prototype's shot_chance(): the aim locks on, so skill is timing plus where and against whom you shoot. */
struct FTaGRules
{
	static float TimingQuality(float Charge);
	static float BaseChance(float DistCm);
	static FTaGShotEval EvaluateShot(const ATaGCharacter* Shooter, float Charge);
};
