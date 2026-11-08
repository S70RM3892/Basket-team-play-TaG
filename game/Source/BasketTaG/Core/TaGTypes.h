#pragma once

#include "CoreMinimal.h"

/**
 * Court and tempo constants for P1 (units: cm, seconds).
 * Court: FIBA 3x3 half court. Baseline is X = 0 and the court runs toward +X; Y is the sideline axis.
 * Tempo values are carried over from the Godot prototype v0.4 ("real" tempo), see prototype/README.md.
 */
namespace TaG
{
	// Court
	constexpr float CourtDepth = 1100.f;    // baseline -> half-court line
	constexpr float CourtHalfWidth = 750.f; // 15 m wide
	constexpr float RimX = 157.5f;          // rim center, 1.575 m from the baseline
	constexpr float RimZ = 305.f;           // 3.05 m
	constexpr float RimRadius = 22.5f;      // inner diameter 45 cm
	constexpr float RimTube = 1.0f;
	constexpr float BoardFrontX = 120.f;    // board face, 1.2 m from the baseline
	constexpr float BoardThickness = 3.f;
	constexpr float BoardHalfWidth = 90.f;  // 1.8 m
	constexpr float BoardBottomZ = 290.f;
	constexpr float BoardTopZ = 395.f;      // 1.05 m tall
	constexpr float ArcRadius = 675.f;      // 3x3 two-point arc
	constexpr float CornerLineY = 660.f;    // straight part of the arc near the baseline
	constexpr float LaneHalfWidth = 245.f;  // 4.9 m paint
	constexpr float FreeThrowX = 580.f;

	// Ball
	constexpr float BallRadius = 12.f;
	constexpr float Gravity = 980.f;

	// Players (v0.4 "real" tempo)
	constexpr float WalkSpeed = 480.f;
	constexpr float DribbleSpeed = 440.f;
	constexpr float ShootMoveSpeed = 120.f;
	constexpr float Accel = 1400.f;
	constexpr float JumpVelocity = 380.f;   // about 0.7 m vertical
	constexpr float EyeHeight = 175.f;
	constexpr float StandingReach = 250.f;

	// Shot timing (seconds since the shot button went down)
	constexpr float ShotAutoJumpTime = 0.36f;  // gather, then jump; apex lands inside the perfect window
	constexpr float ShotGoodLo = 0.52f;
	constexpr float ShotGoodHi = 0.94f;
	constexpr float ShotPerfectLo = 0.68f;
	constexpr float ShotPerfectHi = 0.80f;
	constexpr float ShotAutoRelease = 1.30f;

	// Defense
	constexpr float BlockWindow = 0.25f;
	constexpr float BlockCooldown = 1.0f;
	constexpr float BlockHandReach = 55.f;   // hand distance in front of the eyes
	constexpr float BlockHitRadius = 45.f;
	constexpr float StealCooldown = 1.2f;
	constexpr float StealRange = 180.f;
	constexpr float StealWhiffRecover = 0.45f;
	constexpr float ContestRange = 240.f;

	// Rules (FIBA 3x3)
	constexpr int32 TargetScore = 11;
	constexpr float ShotClock = 12.f;

	inline FVector RimCenter() { return FVector(RimX, 0.f, RimZ); }
	inline FVector RimFloor() { return FVector(RimX, 0.f, 0.f); }

	/** Horizontal distance from the rim (cm). */
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
}
