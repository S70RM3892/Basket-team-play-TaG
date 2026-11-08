#pragma once

#include "CoreMinimal.h"

/**
 * Ball flight without the engine's general physics (TECH.md section 3): a fixed-step parabola with
 * simple collision against the floor, the rim (a torus) and the backboard (a box).
 * Pure functions, so the server, prediction and the --TaGShotTest check all run the same code.
 */
struct FTaGBallSim
{
	FVector Pos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;

	enum EContact : uint8
	{
		None = 0,
		Floor = 1 << 0,
		Rim = 1 << 1,
		Board = 1 << 2,
	};

	/** Advances the ball by Dt using sub-steps. Returns the contacts hit (EContact bits) and sets bOutScored
	 *  when the ball passed down through the rim plane inside the hoop. */
	uint8 Step(float Dt, bool& bOutScored);

	/** Launch velocity that carries a ball from From to Target with the given launch angle (degrees). */
	static FVector SolveLaunch(const FVector& From, const FVector& Target, float AngleDeg);

	/** Launch angle used by P1 shots for a given horizontal distance to the rim (cm). */
	static float ShotAngleForDistance(float Dist);
	static float ShotAngleForDistance(float Dist, float ReleaseZ);

	/** Where a shooter releases the ball: above the head, nudged out from under the rim and the board. */
	static FVector ReleasePoint(const FVector& FeetPos, float ReleaseHeight = 235.f);

	/**
	 * A layup from the hand near rim height: off the glass from the angles (bank shots pay most from the wings,
	 * Silverberg, Tran & Adams 2011), otherwise a soft finger roll over the front of the rim. With bMustScore every
	 * candidate is simulated and the first one that drops through is used.
	 */
	static FVector SolveLayup(const FVector& From, const FVector& Target, bool bMustScore, bool& bOutBank);

	/** Bank shot: the launch that meets the board's face after FlightToBoard seconds and rebounds through Target. */
	static bool SolveBank(const FVector& From, const FVector& Target, float FlightToBoard, FVector& OutVel);

	/** Simulates a launch: does it drop through the hoop within MaxT seconds? */
	static bool Scores(const FVector& From, const FVector& Vel, float MaxT = 2.5f);

private:
	uint8 SubStep(float Dt, bool& bOutScored);
};
