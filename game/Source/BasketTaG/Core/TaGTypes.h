#pragma once

#include "CoreMinimal.h"

/**
 * Court, tempo and rule constants (units: cm, seconds).
 * Court: FIBA 3x3 half court. Baseline is X = 0 and the court runs toward +X; Y is the sideline axis.
 * Tempo and the rock-paper-scissors timings are carried over from the Godot prototype v0.4 ("real" tempo),
 * which the user played and tuned (prototype/scripts/athlete.gd).
 */
namespace TaG
{
	// ---------------------------------------------------------------- Court
	constexpr float CourtDepth = 1100.f;
	constexpr float CourtHalfWidth = 750.f;
	constexpr float RimX = 157.5f;
	constexpr float RimZ = 305.f;
	constexpr float RimRadius = 22.5f;
	constexpr float RimTube = 1.0f;
	constexpr float BoardFrontX = 120.f;
	constexpr float BoardThickness = 3.f;
	constexpr float BoardHalfWidth = 90.f;
	constexpr float BoardBottomZ = 290.f;
	constexpr float BoardTopZ = 395.f;
	constexpr float ArcRadius = 675.f;
	constexpr float CornerLineY = 660.f;
	constexpr float LaneHalfWidth = 245.f;
	constexpr float FreeThrowX = 580.f;

	// ---------------------------------------------------------------- Ball
	constexpr float BallRadius = 12.f;
	constexpr float Gravity = 980.f;

	// ---------------------------------------------------------------- Movement (M-size body; heroes scale these)
	constexpr float WalkSpeed = 480.f;
	constexpr float DribbleSpeed = 440.f;
	constexpr float Accel = 1400.f;
	constexpr float JumpVelocity = 380.f;
	constexpr float ContestMoveSpeed = 120.f;
	constexpr float StaggerMoveSpeed = 100.f;

	// ---------------------------------------------------------------- Rock-paper-scissors hands (RULES_V1 section 3)
	// Startup ~0.15 s (faster than human reaction -> you read, not react); whiff recovery 0.4-0.6 s (punishable).
	constexpr float DriveStartT = 0.15f;
	constexpr float DriveT = 0.45f;
	constexpr float DriveSpeed = 750.f;
	constexpr float DriveCD = 4.f;
	constexpr float CutCD = 2.f;          // drive without the ball (cut)
	constexpr float DodgeStartT = 0.08f;
	constexpr float DodgeT = 0.4f;
	constexpr float DodgeSpeed = 350.f;
	constexpr float DodgeRecoverT = 0.45f;
	constexpr float DodgeCD = 5.f;
	constexpr float TackleStartT = 0.15f;
	constexpr float TackleT = 0.25f;
	constexpr float TackleSpeed = 650.f;
	constexpr float TackleRecoverT = 0.5f;
	constexpr float TackleWhiffExtra = 0.35f;
	constexpr float TackleCD = 5.f;
	constexpr float TackleRange = 115.f;
	constexpr float ContestStartT = 0.15f;
	constexpr float ContestMaxT = 1.2f;
	constexpr float ContestCD = 5.f;
	constexpr float ContestRange = 240.f;
	constexpr float StepT = 0.25f;
	constexpr float StepSpeed = 550.f;
	constexpr float StepCD = 2.5f;
	constexpr float StealCD = 1.2f;
	constexpr float StealRange = 180.f;
	constexpr float StealRecoverT = 0.45f;
	constexpr float DownT = 1.2f;
	constexpr float StaggerT = 0.7f;
	constexpr float ZoneT = 2.f;           // reward after a read win: faster, next shot easier
	constexpr float DriveBeatsContestRange = 230.f;
	constexpr float SpacingReadRange = 220.f;

	// Gather rule (ABILITY_DESIGN section 4): a movement ability with the ball ends the dribble;
	// two steps (~0.6 s) to pass or shoot, then only a pivot.
	constexpr float GatherStepsT = 0.6f;
	constexpr float GatherStepSpeed = 300.f;

	// ---------------------------------------------------------------- Passing
	constexpr float PassSpeed = 1500.f;
	constexpr float ThreadPassSpeed = 2600.f;
	constexpr float PassCD = 0.35f;
	constexpr float PassInterceptRadius = 55.f;
	constexpr float ThreadInterceptRadius = 40.f;
	constexpr float CatchRadius = 140.f;

	// ---------------------------------------------------------------- Shooting (charge is normalized: 1.0 = full meter)
	constexpr float ShotGoodLo = 0.52f;
	constexpr float ShotGoodHi = 0.94f;
	constexpr float ShotPerfectLo = 0.68f;
	constexpr float ShotPerfectHi = 0.80f;
	constexpr float ShotAutoRelease = 1.30f;
	// Seconds for a Meter=1.0 shot to fill to 1.0 (athlete.gd: charge += delta / meter). With the jumper's auto jump
	// at 0.40 s and 340 cm/s, the apex comes at ~0.75 s = charge 0.75, inside the perfect window.
	constexpr float ShotMeterSeconds = 1.0f;
	constexpr float ContactShotMult = 0.55f;

	// ---------------------------------------------------------------- Guard / down
	constexpr float GuardRegenDelay = 2.f;
	constexpr float GuardRegenPerSec = 15.f;
	constexpr float ContactGuardDamage = 35.f;
	constexpr float ReviveTouchExtra = 45.f;

	// ---------------------------------------------------------------- Rules (FIBA 3x3)
	constexpr int32 TargetScore = 11;
	constexpr float ShotClock = 12.f;

	// ---------------------------------------------------------------- Colors (WORLD.md section 7: ally teal, enemy red-orange)
	inline FLinearColor AllyColor() { return FLinearColor(0.10f, 0.95f, 0.82f); }
	inline FLinearColor EnemyColor() { return FLinearColor(1.f, 0.36f, 0.10f); }
	inline FLinearColor GoldColor() { return FLinearColor(1.f, 0.85f, 0.25f); }

	// ---------------------------------------------------------------- Helpers
	inline FVector RimCenter() { return FVector(RimX, 0.f, RimZ); }
	inline FVector RimFloor() { return FVector(RimX, 0.f, 0.f); }
	inline float DistToRim(const FVector& P) { return FVector2D(P.X - RimX, P.Y).Size(); }

	/** True when a shot from this floor position is worth 2 points (outside the arc or in the corner). */
	inline bool IsBeyondArc(const FVector& P)
	{
		if (FMath::Abs(P.Y) >= CornerLineY && P.X < RimX + 150.f)
		{
			return true;
		}
		return DistToRim(P) > ArcRadius;
	}

	inline bool IsOutOfBounds(const FVector& P)
	{
		return P.X < 0.f || P.X > CourtDepth || FMath::Abs(P.Y) > CourtHalfWidth;
	}

	/** Offense spacing spots (floor positions) used by bots and possession setups. Index 0 is the top. */
	inline FVector OffenseSpot(int32 Index)
	{
		switch (Index % 7)
		{
		case 0: return FVector(RimX + 780.f, 0.f, 0.f);       // top
		case 1: return FVector(RimX + 520.f, -470.f, 0.f);    // left wing
		case 2: return FVector(RimX + 520.f, 470.f, 0.f);     // right wing
		case 3: return FVector(90.f, -640.f, 0.f);            // left corner
		case 4: return FVector(90.f, 640.f, 0.f);             // right corner
		case 5: return FVector(RimX + 420.f, -250.f, 0.f);    // left elbow
		default: return FVector(RimX + 420.f, 250.f, 0.f);    // right elbow
		}
	}
}
