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
		{ TEXT("アリウープ"), 0.50f, 1.00f, 0.70f, 1.5f, 0.f, -1.f, 0.f },
	};
}

const FTaGShotTypeDef& FTaGRules::ShotType(ETaGShotType Type)
{
	return GShotTypes[FMath::Clamp(static_cast<int32>(Type), 0, static_cast<int32>(UE_ARRAY_COUNT(GShotTypes)) - 1)];
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
	// Points come from where the shooter last touched the floor: a jump from outside the arc that is released
	// inside it is still worth 2, and the arc line itself is inside (FIBA 3x3)
	E.Points = TaG::IsBeyondArc(Shooter->GetShotSpot()) ? 2 : 1;
	// Bodies: the defender's standing reach (arm length included) against the shooter's. A long defender bothers a
	// short shooter far more than the other way round; the arts that contest (stances, waves, rooted walls) ignore it.
	const float ShooterReach = StandingReach(Shooter->GetHeroDef()) * FMath::Sqrt(Shooter->GetHeroDef().ReachMul) + (Shooter->GetShotMod() == ETaGArtFx::ShotShoulder ? 70.f : 0.f);

	// Distance and type
	float Base = BaseChance(E.Distance);
	if (E.Type == ETaGShotType::Dunk || (E.Type == ETaGShotType::AlleyOop && Shooter->IsOopSlam()))
	{
		Base = 0.96f;
	}
	else if (E.Type == ETaGShotType::AlleyOop)
	{
		Base = 0.93f; // laid in off the catch (a body that cannot reach over the rim)
	}
	else
	{
		Base *= St.Mult;
	}
	if (Shooter->IsScoped() && E.Distance >= 500.f)
	{
		Base += 0.12f; // Hayate: Scope
	}
	const ETaGArtFx Mod = Shooter->GetShotMod();
	if (Shooter->IsShotDebuffed())
	{
		Base *= 0.65f; // Tomoshibi lit them up: dazzled
	}
	if (E.Distance > 500.f && ATaGFieldActor::FindAt(Shooter->GetWorld(), ETaGFieldKind::Wind, Shooter->GetTeam(), Feet))
	{
		Base *= 1.15f; // Kazami: a tailwind carries long shots
	}

	// Timing (Scope and Amagi widen the perfect window; catch-and-shoot never punishes timing)
	const float Window = Mod == ETaGArtFx::ShotSure ? 2.2f : (Shooter->IsScoped() ? 1.6f : 1.f);
	E.Timing = TimingQuality(Charge, Window);
	if (Mod == ETaGArtFx::ShotSure)
	{
		E.Timing = FMath::Max(E.Timing, 1.f);
	}
	if (Shooter->IsCatchAndShoot() || Shooter->IsFreshCatch())
	{
		E.Timing = FMath::Max(E.Timing, 1.f); // catch and shoot: rhythm from the pass
	}
	if (E.Type == ETaGShotType::AlleyOop)
	{
		E.Timing = 1.f; // no meter: the timing was the jump
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
		bool bThisArt = false;
		// 1 for bodies of the same size; a 2.5 m giant contesting a small shooter about 1.3, the other way round 0.35.
		// A defender in the air reaches higher still.
		const float DefReach = StandingReach(Def->GetHeroDef()) * FMath::Sqrt(Def->GetHeroDef().ReachMul) * Def->GetReachFactor()
			+ FMath::Max(0.f, DefFeet.Z);
		const float Body = FMath::Clamp(1.f + (DefReach - ShooterReach) / 110.f, 0.35f, 1.3f);
		const FTaGArtDef* Stance = Def->GetContestArt();
		if (Stance && D < Stance->Range && Facing > 0.2f)
		{
			C = 0.22f;          // Ankou's gulp: a strong contest in front
			bThisContest = true;
			bThisArt = true;
		}
		else if (Def->IsRootedWall() && D < Def->GetRootWallRadius())
		{
			C = 0.45f;          // Kokemaru rooted: every shot around him is contested
			bThisContest = true;
			bThisArt = true;
		}
		else if (Def->IsContesting() && Def->IsSonicContest() && D < 400.f)
		{
			C = 0.5f;           // Hibiki's sonic wave: area contest
			bThisContest = true;
			bThisArt = true;
		}
		else if (Def->IsContesting() && D < TaG::ContestRange && Facing > 0.2f)
		{
			C = FMath::Max(0.12f, 1.f - 0.7f * Body); // contest > shot (0.3 between equal bodies)
			bThisContest = true;
		}
		else if (Def->IsContestStarting() && D < TaG::ContestRange)
		{
			C = FMath::Max(0.3f, 1.f - 0.4f * Body);
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
			// Shot types that resist closeouts (fadeaway, euro step, floater); the bodies scale the closeout too
			C = FMath::Clamp(1.f - (1.f - C) * St.Prox * Body, 0.6f, 1.f);
		}
		// Oboro's mist halves contests made from inside it; the high arc and the quick release resist them
		if (C < 1.f && ATaGFieldActor::FindAt(Shooter->GetWorld(), ETaGFieldKind::Mist, Shooter->GetTeam(), DefFeet))
		{
			C = 1.f - (1.f - C) * 0.5f;
		}
		if (Mod == ETaGArtFx::ShotHigh)
		{
			C = 1.f - (1.f - C) * 0.5f;
		}
		else if (Mod == ETaGArtFx::ShotQuick)
		{
			C = 1.f - (1.f - C) * 0.75f;
		}
		if (C < Contest)
		{
			Contest = C;
			// What the shot meter tells the shooter: who is bothering the shot and by how much taller they reach
			E.bReached = true;
			E.ReachGap = DefReach - ShooterReach;
			E.bArtContest = bThisArt;
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

	// Hotaru's swarm around the shooter
	if (const ATaGFieldActor* Swarm = ATaGFieldActor::FindAt(Shooter->GetWorld(), ETaGFieldKind::Swarm, 1 - Shooter->GetTeam(), Feet))
	{
		Contest = FMath::Min(Contest, 0.5f);
	}
	E.bOpen = !E.bContested && !E.bWalled && NearestDist > 230.f && NearestDist < 9000.f;
	if (E.bOpen && Mod == ETaGArtFx::ShotShoulder)
	{
		Base *= 1.1f;
	}
	E.Contest = Contest * (bContact ? TaG::ContactShotMult : 1.f);
	const float Zone = bZone ? 1.15f : 1.f;
	E.Chance = FMath::Clamp(Base * E.Timing * E.Contest * Zone, 0.02f, 0.97f);
	return E;
}

float FTaGRules::LayupRange(float SpeedToRim)
{
	return 320.f + FMath::Clamp(SpeedToRim - 200.f, 0.f, 250.f) * 0.4f;
}

FTaGRunUp FTaGRules::PlanRunUp(const FTaGHeroDef& Hero, ETaGShotType Type, const FVector& Feet, const FVector& Velocity, float StickLateral)
{
	FTaGRunUp R;
	const bool bDunk = Type == ETaGShotType::Dunk;
	if (Type != ETaGShotType::Layup && !bDunk)
	{
		return R;
	}
	R.bValid = true;
	const FVector2D ToRim(TaG::RimX - Feet.X, -Feet.Y);
	const float D0 = ToRim.Size();
	const FVector2D Dir = D0 > 1.f ? ToRim / D0 : FVector2D(-1.f, 0.f);
	R.JumpVZ = ShotType(Type).JumpVel * FMath::Sqrt(Hero.JumpMul);
	const float TUp = R.JumpVZ / TaG::Gravity;
	// A layup carries more of the run into the air; a dunk plants harder and goes up
	const float Keep = bDunk ? 0.35f : 0.55f;
	// Feet to the rim centre at the top of the jump (the finishing hand reaches about 40 cm ahead of the body)
	const float Apex = bDunk ? 55.f : 85.f;
	const float Path = FMath::Max(D0 - Apex, 0.f);
	const float Along = FMath::Max(0.f, FVector2D::DotProduct(FVector2D(Velocity.X, Velocity.Y), Dir));
	const float MaxSpeed = 650.f * Hero.SpeedMul;
	float Speed = FMath::Clamp(Along, 220.f, MaxSpeed);
	// Path = Speed * (T - Stance) + Speed * (1 + Keep) / 2 * Stance + Speed * Keep * TUp
	const float Offset = RunUpStanceT * (1.f - Keep) * 0.5f - Keep * TUp;
	const float TMin = bDunk ? 0.24f : 0.3f;
	const float TMax = 0.75f;
	float T = Path / Speed + Offset;
	if (T < TMin)
	{
		T = TMin;
		Speed = FMath::Max(Path / (TMin - Offset), 60.f);
	}
	else if (T > TMax)
	{
		Speed = FMath::Min(Path / (TMax - Offset), MaxSpeed);
		T = FMath::Clamp(Path / Speed + Offset, TMin, TMax);
	}
	R.TakeoffT = T;
	R.ApproachSpeed = Speed;
	R.TakeoffSpeed = Speed * Keep;
	// The outside hand. Facing the rim from the court (-X), the +Y half is on the left: finish with the left hand off
	// the right foot there, the right hand on the other side; down the middle the stick's lean decides.
	R.bLeftHand = FMath::Abs(Feet.Y) > 35.f ? Feet.Y > 0.f : StickLateral < -0.3f;
	return R;
}

FVector FTaGRules::LayupRelease(const FVector& Feet, const FVector& Forward, float EyeHeight, bool bLeftHand)
{
	const float S = EyeHeight / 175.f;
	FVector Fwd(Forward.X, Forward.Y, 0.f);
	Fwd = Fwd.IsNearlyZero() ? FVector(-1.f, 0.f, 0.f) : Fwd.GetSafeNormal();
	const FVector Right(-Fwd.Y, Fwd.X, 0.f);
	FVector P = Feet + Fwd * 38.f * S + Right * (bLeftHand ? -16.f : 16.f) * S;
	P.Z = Feet.Z + EyeHeight * 1.28f;
	// Never from inside the hoop's cylinder or behind the board; a hand below the rim needs room to lift it over the front
	const float MinOff = 36.f + FMath::Clamp(TaG::RimZ + 10.f - P.Z, 0.f, 60.f) * 0.5f;
	FVector2D Off(P.X - TaG::RimX, P.Y);
	if (Off.Size() < MinOff)
	{
		FVector2D Back(Feet.X - TaG::RimX, Feet.Y);
		Back = Back.IsNearlyZero() ? FVector2D(1.f, 0.f) : Back.GetSafeNormal();
		Off = Back * MinOff;
		P.X = TaG::RimX + Off.X;
		P.Y = Off.Y;
	}
	P.X = FMath::Max(P.X, TaG::BoardFrontX + TaG::BallRadius + 10.f);
	return P;
}

FTaGAlleyOop FTaGRules::PlanAlleyOop(const ATaGCharacter* Passer, const ATaGCharacter* Receiver)
{
	FTaGAlleyOop O;
	if (!Passer || !Receiver || Receiver == Passer || Receiver->GetTeam() != Passer->GetTeam())
	{
		return O;
	}
	const ETaGAction A = Receiver->GetAction();
	const bool bFree = A == ETaGAction::None || A == ETaGAction::Drive || A == ETaGAction::DriveStart || A == ETaGAction::Step;
	const UCharacterMovementComponent* Move = Receiver->GetCharacterMovement();
	if (!bFree || !Receiver->CanTouchBall() || Receiver->IsRooted() || !Move || !Move->IsMovingOnGround())
	{
		return O;
	}
	const FVector Feet = Receiver->GetFeetLocation();
	const FVector2D ToRim(TaG::RimX - Feet.X, -Feet.Y);
	const float D = ToRim.Size();
	if (D < 160.f || D > 760.f)
	{
		return O;
	}
	const FVector2D Dir = ToRim / D;
	const FVector2D V(Receiver->GetVelocity().X, Receiver->GetVelocity().Y);
	const float Along = FVector2D::DotProduct(V, Dir);
	if (Along < 250.f || Along < 0.6f * V.Size())
	{
		return O; // only for a player running at the rim
	}
	const FTaGHeroDef& H = Receiver->GetHeroDef();
	const float Reach = StandingReach(H);
	const float MaxVZ = ShotType(ETaGShotType::Dunk).JumpVel * FMath::Sqrt(H.JumpMul);
	const float Top = Reach + MaxVZ * MaxVZ / (2.f * TaG::Gravity); // the hands at the top of the best jump
	O.bSlam = Top >= TaG::RimZ + 22.f;
	const float CatchZ = FMath::Min(Top - 8.f, TaG::RimZ + 45.f);
	if (CatchZ < 250.f)
	{
		return O;
	}
	// Just enough jump to put the hands a little over the catch height at the top
	O.LeapVZ = FMath::Sqrt(2.f * TaG::Gravity * FMath::Max(CatchZ + 8.f - Reach, 10.f));
	const float TUp = O.LeapVZ / TaG::Gravity;
	// The ball meets the hands in front of the rim on the cutter's line
	const float Front = O.bSlam ? 55.f : 65.f;
	const FVector2D C2 = FVector2D(TaG::RimX, 0.f) - Dir * Front;
	O.Catch = FVector(FMath::Max(C2.X, TaG::RimX - 15.f), C2.Y, CatchZ);
	// Run on to the take-off spot, then a leap that keeps about 45 % of the run; the hands lead the body by ~22 cm
	const float Run = FMath::Clamp(FMath::Min(Along, 1.2f * TaG::WalkSpeed * H.SpeedMul), 300.f, 650.f * H.SpeedMul);
	const float Lead = 22.f + 0.45f * Run * TUp;
	const float ToTakeoff = (D - Front) - Lead;
	if (ToTakeoff < -40.f)
	{
		return O; // already too close for a lob
	}
	O.FlightT = FMath::Max(ToTakeoff, 0.f) / Run + 0.1f + TUp;
	if (O.FlightT < 0.55f || O.FlightT > 1.25f)
	{
		return O;
	}
	O.bValid = true;
	return O;
}
