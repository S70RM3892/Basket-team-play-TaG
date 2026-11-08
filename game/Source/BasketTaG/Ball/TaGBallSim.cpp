#include "Ball/TaGBallSim.h"
#include "Core/TaGTypes.h"

namespace
{
	constexpr float SubStepDt = 1.f / 240.f;
	constexpr float FloorRestitution = 0.75f;
	constexpr float FloorFriction = 0.85f;
	constexpr float RimRestitution = 0.55f;
	constexpr float BoardRestitution = 0.6f;
	constexpr float ScoreRadius = 20.f; // ball center must pass this close to the rim center

	/** Pushes the ball out along N and reflects the velocity component into the surface. */
	void Bounce(FVector& Pos, FVector& Vel, const FVector& N, float Depth, float Restitution, float TangentKeep)
	{
		Pos += N * Depth;
		const float Vn = FVector::DotProduct(Vel, N);
		if (Vn < 0.f)
		{
			const FVector Normal = N * Vn;
			const FVector Tangent = Vel - Normal;
			Vel = Tangent * TangentKeep - Normal * Restitution;
		}
	}
}

uint8 FTaGBallSim::Step(float Dt, bool& bOutScored)
{
	uint8 Contacts = None;
	bOutScored = false;
	float Remaining = Dt;
	while (Remaining > KINDA_SMALL_NUMBER)
	{
		const float H = FMath::Min(Remaining, SubStepDt);
		bool bScored = false;
		Contacts |= SubStep(H, bScored);
		bOutScored |= bScored;
		Remaining -= H;
	}
	return Contacts;
}

uint8 FTaGBallSim::SubStep(float Dt, bool& bOutScored)
{
	uint8 Contacts = None;
	const FVector Prev = Pos;
	Vel.Z -= TaG::Gravity * Dt;
	Pos += Vel * Dt;

	// Basket: crossed the rim plane downward close enough to the center.
	if (Prev.Z >= TaG::RimZ && Pos.Z < TaG::RimZ && Vel.Z < 0.f)
	{
		const float T = (Prev.Z - TaG::RimZ) / FMath::Max(Prev.Z - Pos.Z, KINDA_SMALL_NUMBER);
		const FVector Cross = FMath::Lerp(Prev, Pos, T);
		if (FVector2D(Cross.X - TaG::RimX, Cross.Y).Size() < ScoreRadius)
		{
			bOutScored = true;
		}
	}

	// Floor
	if (Pos.Z < TaG::BallRadius)
	{
		Bounce(Pos, Vel, FVector::UpVector, TaG::BallRadius - Pos.Z, FloorRestitution, FloorFriction);
		Contacts |= Floor;
	}

	// Rim: closest point on the rim circle, then a sphere-vs-tube test.
	{
		const FVector C = TaG::RimCenter();
		FVector Flat(Pos.X - C.X, Pos.Y - C.Y, 0.f);
		const float FlatLen = Flat.Size();
		const FVector Dir = FlatLen > KINDA_SMALL_NUMBER ? Flat / FlatLen : FVector::ForwardVector;
		const FVector Closest = C + Dir * TaG::RimRadius;
		const FVector D = Pos - Closest;
		const float Dist = D.Size();
		const float MinDist = TaG::BallRadius + TaG::RimTube;
		if (Dist < MinDist && Dist > KINDA_SMALL_NUMBER)
		{
			Bounce(Pos, Vel, D / Dist, MinDist - Dist, RimRestitution, 0.9f);
			Contacts |= Rim;
		}
	}

	// Backboard (axis-aligned box)
	{
		const FVector BoxMin(TaG::BoardFrontX - TaG::BoardThickness, -TaG::BoardHalfWidth, TaG::BoardBottomZ);
		const FVector BoxMax(TaG::BoardFrontX, TaG::BoardHalfWidth, TaG::BoardTopZ);
		const FVector Closest(
			FMath::Clamp(Pos.X, BoxMin.X, BoxMax.X),
			FMath::Clamp(Pos.Y, BoxMin.Y, BoxMax.Y),
			FMath::Clamp(Pos.Z, BoxMin.Z, BoxMax.Z));
		const FVector D = Pos - Closest;
		const float Dist = D.Size();
		if (Dist < TaG::BallRadius)
		{
			const FVector N = Dist > KINDA_SMALL_NUMBER ? D / Dist : FVector::ForwardVector;
			Bounce(Pos, Vel, N, TaG::BallRadius - Dist, BoardRestitution, 0.9f);
			Contacts |= Board;
		}
	}

	return Contacts;
}

FVector FTaGBallSim::SolveLaunch(const FVector& From, const FVector& Target, float AngleDeg)
{
	const FVector Diff = Target - From;
	const FVector2D Flat(Diff.X, Diff.Y);
	const float D = FMath::Max(Flat.Size(), 1.f);
	const FVector2D Dir = Flat / D;
	const float H = Diff.Z;

	// Raise the angle until the target is reachable (d * tan(a) must exceed the height to climb).
	float Angle = AngleDeg;
	for (int32 i = 0; i < 20; ++i)
	{
		const float A = FMath::DegreesToRadians(Angle);
		const float Denom = 2.f * FMath::Square(FMath::Cos(A)) * (D * FMath::Tan(A) - H);
		if (Denom > KINDA_SMALL_NUMBER)
		{
			const float Speed = FMath::Sqrt(TaG::Gravity * D * D / Denom);
			return FVector(Dir.X * Speed * FMath::Cos(A), Dir.Y * Speed * FMath::Cos(A), Speed * FMath::Sin(A));
		}
		Angle = FMath::Min(Angle + 3.f, 85.f);
	}
	return FVector(Dir.X * 300.f, Dir.Y * 300.f, 700.f);
}

float FTaGBallSim::ShotAngleForDistance(float Dist, float ReleaseZ)
{
	// A low release (small players, no jump) has to climb more: raise the arc so it clears the front of the rim.
	const float Climb = TaG::RimZ - ReleaseZ;
	const float Extra = FMath::Clamp((Climb - 60.f) / 4.f, 0.f, 12.f);
	return FMath::Min(ShotAngleForDistance(Dist) + Extra, 84.f);
}

float FTaGBallSim::ShotAngleForDistance(float Dist)
{
	// Close shots need a higher arc so a low release (no jump) still clears the front of the rim.
	if (Dist < 120.f)
	{
		return 80.f;
	}
	if (Dist < 200.f)
	{
		return 66.f;
	}
	if (Dist < 350.f)
	{
		return 58.f;
	}
	return 52.f;
}

FVector FTaGBallSim::ReleasePoint(const FVector& FeetPos, float ReleaseHeight)
{
	FVector Flat(FeetPos.X - TaG::RimX, FeetPos.Y, 0.f);
	const float Dist = Flat.Size();
	const FVector Dir = Dist > KINDA_SMALL_NUMBER ? Flat / Dist : FVector::ForwardVector;
	// The ball leaves the hands about 35 cm in front of the body, toward the rim.
	FVector From = FeetPos - Dir * 35.f;
	From.Z = FeetPos.Z + ReleaseHeight; // FeetPos already includes the jump

	// Nothing is released from behind the board; shots from under the rim are pushed out a little.
	From.X = FMath::Max(From.X, 145.f);
	FVector Off(From.X - TaG::RimX, From.Y, 0.f);
	if (Off.Size() < 75.f)
	{
		Off.X = FMath::Max(Off.X, 30.f);
		Off = Off.GetSafeNormal() * 75.f;
		From.X = TaG::RimX + Off.X;
		From.Y = Off.Y;
	}
	return From;
}
