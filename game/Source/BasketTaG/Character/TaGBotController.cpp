#include "Character/TaGBotController.h"
#include "Ball/TaGBall.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGGameMode.h"
#include "Core/TaGHeroData.h"
#include "Core/TaGRules.h"
#include "Core/TaGTypes.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	bool ResyncWorth(const ATaGCharacter* Me)
	{
		const ATaGCharacter* Near = nullptr;
		float NearD = 1000.f; // the same 10 m as the art (ATaGCharacter::ExecuteArt, Resync)
		for (TActorIterator<ATaGCharacter> It(Me->GetWorld()); It; ++It)
		{
			if (*It == Me || It->GetTeam() != Me->GetTeam())
			{
				continue;
			}
			if (It->IsDown())
			{
				return true;
			}
			const float D = FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation());
			if (D < NearD)
			{
				NearD = D;
				Near = *It;
			}
		}
		return Near && Near->GetCooldownLeft(ETaGSlot::Art1) > 4.f && Near->GetCooldownLeft(ETaGSlot::Art2) > 4.f;
	}

	const TCHAR* HandLabel(uint8 H)
	{
		switch (H)
		{
		case 0: return TEXT("タックル");
		case 1: return TEXT("シュート妨害");
		case 2: return TEXT("間合い（待つ）");
		case 4: return TEXT("突破");
		case 5: return TEXT("かわし");
		default: return TEXT("シュート");
		}
	}

	FVector Flat(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.f);
	}
}

ATaGBotController::ATaGBotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true;
	bSetControlRotationFromPawnOrientation = false; // the bot steers its view itself
	const EHand Defs[] = { EHand::Tackle, EHand::Contest, EHand::Wait };
	const EHand Offs[] = { EHand::Drive, EHand::Dodge, EHand::Shoot };
	DefFav = Defs[FMath::RandRange(0, 2)];
	OffFav = Offs[FMath::RandRange(0, 2)];
}

FString ATaGBotController::HabitText() const
{
	return FString::Printf(TEXT("守備では「%s」、攻撃では「%s」が好き"), HandLabel(static_cast<uint8>(DefFav)), HandLabel(static_cast<uint8>(OffFav)));
}

void ATaGBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Queue.Reset();
	LeapSlack = FMath::FRandRange(-0.06f, 0.06f);
}

void ATaGBotController::TestCut()
{
	ATaGCharacter* Me = Cast<ATaGCharacter>(GetPawn());
	if (!Me)
	{
		return;
	}
	CuttingFor = 1.6f;
	CutTimer = 9.f;
	Wish(Me, Flat(TaG::RimFloor() - Me->GetFeetLocation()));
	Press(Me, ETaGInput::Primary);
}

// ================================================================ Helpers

bool ATaGBotController::Press(ATaGCharacter* Me, ETaGInput Input)
{
	if (Me->CanUseInput(Input))
	{
		Me->InputPressed(Input);
		return true;
	}
	return false;
}

void ATaGBotController::Wish(ATaGCharacter* Me, const FVector& Dir)
{
	Me->SetBotWishDir(Flat(Dir));
}

void ATaGBotController::Steer(ATaGCharacter* Me, const FVector& Target, float Accuracy, float Scale)
{
	FVector Goal = Target;
	if (Me->HasBall())
	{
		// With the ball, never aim at the lines: a foot on them is out of bounds
		Goal.X = FMath::Clamp(Goal.X, 70.f, TaG::CourtDepth - 70.f);
		Goal.Y = FMath::Clamp(Goal.Y, -TaG::CourtHalfWidth + 70.f, TaG::CourtHalfWidth - 70.f);
	}
	FVector To = Flat(Goal - Me->GetFeetLocation());
	const float D = To.Size();
	if (D > Accuracy)
	{
		Wish(Me, To);
		if (!Me->IsForcedMove())
		{
			Me->AddMovementInput(To / D, FMath::Clamp(D / 120.f, 0.35f, 1.f) * Scale);
		}
	}
	else
	{
		Wish(Me, FVector::ZeroVector);
	}
}

void ATaGBotController::Face(ATaGCharacter* Me, const FVector& Point, float Dt, float Rate)
{
	Me->FaceTowards(Point, Dt, Rate);
}

int32 ATaGBotController::TeamIndex(ATaGCharacter* Me) const
{
	const ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	if (!GM)
	{
		return 0;
	}
	TArray<ATaGCharacter*> Team = GM->GetTeamPlayers(Me->GetTeam());
	return FMath::Max(0, Team.IndexOfByKey(Me));
}

ATaGCharacter* ATaGBotController::MyMan(ATaGCharacter* Me) const
{
	const ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	if (!GM)
	{
		return nullptr;
	}
	TArray<ATaGCharacter*> Opp = GM->GetTeamPlayers(1 - Me->GetTeam());
	if (Opp.Num() == 0)
	{
		return nullptr;
	}
	return Opp[FMath::Clamp(TeamIndex(Me), 0, Opp.Num() - 1)];
}

float ATaGBotController::OpenNess(const ATaGCharacter* Who) const
{
	float Best = 9999.f;
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() != Who->GetTeam() && It->IsActiveDefender())
		{
			Best = FMath::Min(Best, FVector::Dist2D(It->GetActorLocation(), Who->GetActorLocation()));
		}
	}
	return Best;
}

ATaGBotController::EHand ATaGBotController::Pick(const TMap<EHand, float>& Weights) const
{
	float Total = 0.f;
	for (const auto& W : Weights)
	{
		Total += FMath::Max(0.f, W.Value);
	}
	if (Total <= 0.f)
	{
		return EHand::Wait;
	}
	float R = FMath::FRand() * Total;
	for (const auto& W : Weights)
	{
		R -= FMath::Max(0.f, W.Value);
		if (R <= 0.f)
		{
			return W.Key;
		}
	}
	return EHand::Wait;
}

bool ATaGBotController::TryArts(ATaGCharacter* Me, ETaGArtAI Situation, float RatePerSec, float Dt)
{
	const FTaGHeroDef& H = Me->GetHeroDef();
	for (int32 i = 1; i <= 2; ++i)
	{
		const FTaGArtDef& A = TaGHeroes::GetArt(i == 1 ? H.Art1 : H.Art2);
		if (A.Kind == ETaGArtKind::Custom || A.AI != Situation)
		{
			continue;
		}
		// Support arts wait until they matter: heals for hurt or downed teammates
		if (A.Fx == ETaGArtFx::Heal)
		{
			bool bNeed = false;
			for (TActorIterator<ATaGCharacter> It(GetWorld()); It && !bNeed; ++It)
			{
				bNeed = It->GetTeam() == Me->GetTeam() && (It->IsDown() || It->GetGuard() < It->GetMaxGuard() * 0.75f)
					&& FVector::Dist2D(It->GetActorLocation(), Me->GetActorLocation()) < A.Radius;
			}
			if (!bNeed)
			{
				continue;
			}
		}
		const ETaGInput In = i == 1 ? ETaGInput::Art1 : ETaGInput::Art2;
		if (!Me->CanUseInput(In) || FMath::FRand() > RatePerSec * Dt)
		{
			continue;
		}
		return Press(Me, In);
	}
	return false;
}

bool ATaGBotController::UseArtIf(ATaGCharacter* Me, int32 Index, bool bCondition)
{
	if (!bCondition)
	{
		return false;
	}
	return Press(Me, Index == 1 ? ETaGInput::Art1 : ETaGInput::Art2);
}

void ATaGBotController::Commit(ATaGCharacter* Me, EHand Hand)
{
	// Weaker bots telegraph their defensive choice for a moment (readable habits, prototype v0.2).
	const float Pretell = FMath::Max(0.f, 0.5f - Skill * 0.65f);
	if (Pretell <= 0.02f)
	{
		Do(Me, Hand);
		return;
	}
	for (const FQueued& Q : Queue)
	{
		if (Q.Hand == Hand)
		{
			return;
		}
	}
	Me->SetPretell(Hand == EHand::Tackle ? 1 : (Hand == EHand::Contest ? 2 : 0), Pretell);
	Queue.Add({ Pretell, Hand });
}

void ATaGBotController::Do(ATaGCharacter* Me, EHand Hand)
{
	if (Me->GetAction() != ETaGAction::None)
	{
		return;
	}
	const FTaGHeroDef& H = Me->GetHeroDef();
	switch (Hand)
	{
	case EHand::Tackle:
		// Gantetsu prefers his shoulder charge, Minamo her pull, when ready
		if (H.Art1 == ETaGArt::ShoulderCharge && Me->CanUseInput(ETaGInput::Art1) && FMath::FRand() < 0.5f)
		{
			Press(Me, ETaGInput::Art1);
		}
		else
		{
			Press(Me, ETaGInput::Primary);
		}
		break;
	case EHand::Contest:
		if (H.Art1 == ETaGArt::SonicWave && Me->CanUseInput(ETaGInput::Art1) && FMath::FRand() < 0.5f)
		{
			Press(Me, ETaGInput::Art1);
		}
		else if (Press(Me, ETaGInput::Secondary))
		{
			// The tutorial bot holds its contest up long enough to be driven past.
			ContestHold = Script == EScript::Normal ? FMath::FRandRange(0.6f, 1.f) : 1.15f;
		}
		break;
	case EHand::Steal:
		Press(Me, ETaGInput::PassSteal);
		break;
	case EHand::Drive:
		if (H.Art1 == ETaGArt::Blink && Me->CanUseInput(ETaGInput::Art1) && FMath::FRand() < 0.6f)
		{
			Press(Me, ETaGInput::Art1);
		}
		else
		{
			Press(Me, ETaGInput::Primary);
		}
		break;
	case EHand::Dodge:
		if (H.Art2 == ETaGArt::Afterimage && Me->CanUseInput(ETaGInput::Art2) && FMath::FRand() < 0.6f)
		{
			Press(Me, ETaGInput::Art2);
		}
		else
		{
			Press(Me, ETaGInput::Secondary);
		}
		break;
	default:
		break;
	}
}

// ================================================================ Tick

void ATaGBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ATaGCharacter* Me = Cast<ATaGCharacter>(GetPawn());
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	if (!Me || !GM)
	{
		return;
	}
	ATaGBall* Ball = Me->FindBall();
	if (!GM->IsLive() || !Ball)
	{
		Wish(Me, FVector::ZeroVector);
		Face(Me, TaG::RimCenter(), DeltaSeconds, 4.f);
		if (ScriptPhase != 0 || ScriptT != 0.f)
		{
			// A lesson is being set up again: start the script over, from the other side this time.
			ScriptPhase = 0;
			ScriptT = 0.f;
			ScriptSide = -ScriptSide;
			Queue.Reset();
		}
		return;
	}

	// Queued (telegraphed) actions
	for (int32 i = Queue.Num() - 1; i >= 0; --i)
	{
		Queue[i].T -= DeltaSeconds;
		if (Queue[i].T <= 0.f)
		{
			const EHand H = Queue[i].Hand;
			Queue.RemoveAt(i);
			Do(Me, H);
		}
	}
	if (Ball->GetBallState() != ETaGBallState::Pass)
	{
		bOopDefDecided = false;
	}
	if (ContestHold > 0.f)
	{
		ContestHold -= DeltaSeconds;
		if (ContestHold <= 0.f)
		{
			Me->InputReleased(ETaGInput::Secondary);
		}
	}
	if (Script != EScript::Normal)
	{
		TickScript(Me, Ball, DeltaSeconds);
		return;
	}

	ATaGCharacter* Holder = Ball->GetHolder();
	switch (Ball->GetBallState())
	{
	case ETaGBallState::Held:
		if (Holder == Me)
		{
			Offense(Me, Ball, DeltaSeconds);
		}
		else if (Holder && Holder->GetTeam() == Me->GetTeam())
		{
			bHadBall = false;
			OffBall(Me, Holder, DeltaSeconds);
		}
		else if (Holder)
		{
			bHadBall = false;
			ATaGCharacter* Man = MyMan(Me);
			if (Man == Holder || !Man)
			{
				DefendBall(Me, Holder, DeltaSeconds);
			}
			else
			{
				DefendOffBall(Me, Man, Holder, DeltaSeconds);
			}
		}
		bBlockDecided = false;
		break;
	default:
		bHadBall = false;
		Loose(Me, Ball, DeltaSeconds);
		break;
	}
}

// ================================================================ Tutorial scripts

void ATaGBotController::SetScript(EScript InScript)
{
	Script = InScript;
	ScriptPhase = 0;
	ScriptT = 0.f;
	Queue.Reset();
	ContestHold = 0.f;
}

void ATaGBotController::TickScript(ATaGCharacter* Me, ATaGBall* Ball, float Dt)
{
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	ATaGCharacter* Opp = Me->FindNearestOpponent();
	const bool bHold = Ball->GetBallState() == ETaGBallState::Held && Ball->GetHolder() == Me;
	ScriptT += Dt;
	uint8 Pt = 0;

	switch (Script)
	{
	case EScript::Idle:
		// Out of the way in the corner, watching the ball.
		Steer(Me, FVector(90.f, -640.f, 0.f), 40.f);
		Face(Me, Ball->GetActorLocation(), Dt, 6.f);
		break;

	case EScript::ContestOnClose:
	case EScript::TackleOnClose:
	{
		if (!Opp)
		{
			break;
		}
		// Guard the handler between them and the rim, then show the tell and commit when they come close.
		const bool bContest = Script == EScript::ContestOnClose;
		const FVector Hp = Flat(Opp->GetFeetLocation());
		const FVector ToRim = Flat(TaG::RimFloor() - Hp);
		// 2 m off the handler: close enough to tackle, far enough that the whole body (and its tell) is in view.
		const FVector Gp = Hp + ToRim.GetSafeNormal() * FMath::Min(200.f, ToRim.Size() * 0.6f);
		if (Me->GetAction() == ETaGAction::None)
		{
			Steer(Me, Gp, 20.f);
		}
		Face(Me, Opp->GetActorLocation(), Dt, 12.f);
		const float D = FVector::Dist2D(Opp->GetActorLocation(), Me->GetActorLocation());
		if (ScriptPhase == 0 && ScriptT > 1.2f && Opp->HasBall() && D < (bContest ? 330.f : 260.f)
			&& Me->GetAction() == ETaGAction::None && !Me->IsPretelling(Pt))
		{
			Me->ResetHandCooldowns();
			// Short tells, so answering right when the color shows up is on time: the contest then stays up
			// for over a second (drive any time), the tackle lands ~0.2 s after its tell (dodge right away).
			const float Tell = bContest ? 0.2f : 0.35f;
			Me->SetPretell(bContest ? 2 : 1, Tell);
			Queue.Add({ Tell, bContest ? EHand::Contest : EHand::Tackle });
			ScriptPhase = 1;
			ScriptT = 0.f;
		}
		else if (ScriptPhase == 1 && ScriptT > 2.4f)
		{
			ScriptPhase = 0; // next attempt
			ScriptT = 0.f;
		}
		break;
	}

	case EScript::DriveAttack:
	{
		if (!bHold)
		{
			Wish(Me, FVector::ZeroVector);
			Face(Me, Ball->GetActorLocation(), Dt, 6.f);
			break;
		}
		// Drive past the defender on one side (alternating every attempt).
		const FVector ToRim = Flat(TaG::RimFloor() - Me->GetFeetLocation()).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(FVector::UpVector, ToRim) * ScriptSide;
		Wish(Me, (ToRim + Side * 0.55f).GetSafeNormal());
		Face(Me, TaG::RimCenter(), Dt, 8.f);
		if (ScriptPhase == 0 && ScriptT > 1.4f)
		{
			Me->ResetHandCooldowns();
			Me->SetPretell(3, 0.45f);
			Queue.Add({ 0.45f, EHand::Drive });
			ScriptPhase = 1;
			ScriptT = 0.f;
		}
		else if (ScriptPhase == 1 && ScriptT > 0.9f && Me->GetAction() == ETaGAction::None)
		{
			// The drive never started: the defender hit during the tell.
			ScriptPhase = 2;
			if (GM)
			{
				GM->TutorialRetry(TEXT("早すぎた。前のめりになって走り出すところにタックル"));
			}
		}
		break;
	}

	case EScript::ShootAttack:
	{
		if (!bHold)
		{
			Wish(Me, FVector::ZeroVector);
			Face(Me, Ball->GetActorLocation(), Dt, 6.f);
			break;
		}
		Face(Me, TaG::RimCenter(), Dt, 8.f);
		Wish(Me, FVector::ZeroVector);
		if (Me->IsChargingShot())
		{
			if (Me->GetShotCharge() >= 0.72f)
			{
				Me->InputReleased(ETaGInput::Shoot);
			}
			break;
		}
		if (ScriptPhase == 0 && ScriptT > 1.4f)
		{
			Me->SetPretell(4, 0.5f);
			ScriptPhase = 1;
			ScriptT = 0.f;
		}
		else if (ScriptPhase == 1 && ScriptT > 0.5f)
		{
			ScriptPhase = 2;
			Press(Me, ETaGInput::Shoot);
		}
		break;
	}

	default:
		break;
	}
}

// ================================================================ Offense

void ATaGBotController::Offense(ATaGCharacter* Me, ATaGBall* Ball, float Dt)
{
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	const FVector Feet = Me->GetFeetLocation();
	const FVector ToRim = Flat(TaG::RimFloor() - Feet);
	const float Nd = ToRim.Size() / 100.f;
	ATaGCharacter* Opp = Me->FindNearestOpponent();
	const FVector ToOpp = Opp ? Flat(Opp->GetActorLocation() - Me->GetActorLocation()) : FVector(9999.f, 0.f, 0.f);
	const float DOpp = ToOpp.Size();
	const bool bBetween = DOpp < 300.f && FVector::DotProduct(ToOpp.GetSafeNormal(), ToRim.GetSafeNormal()) > 0.45f;
	Face(Me, TaG::RimCenter(), Dt, 8.f);

	if (!bHadBall)
	{
		bHadBall = true;
		HoldTime = 0.f;
		bAttackRim = FMath::FRand() < 0.35f + 0.3f * Skill;
		bTetherUsed = false;
	}
	HoldTime += Dt;

	if (Me->IsChargingShot())
	{
		if (Me->GetShotCharge() >= ReleaseAt)
		{
			Me->InputReleased(ETaGInput::Shoot);
		}
		return;
	}
	if (FadeWait > 0.f)
	{
		FadeWait -= Dt;
		Wish(Me, -ToRim);
		if (FadeWait <= 0.f)
		{
			StartShot(Me);
		}
		return;
	}
	// Alley-oop: a teammate running at the rim and nobody waiting at the catch spot -> throw it up
	const ETaGAction MyAct = Me->GetAction();
	if ((MyAct == ETaGAction::None || MyAct == ETaGAction::Gathered || MyAct == ETaGAction::Drive) && !GM->NeedsClear(Me->GetTeam()) && TryAlleyOop(Me, Dt))
	{
		return;
	}
	if (Me->GetAction() == ETaGAction::Drive || Me->GetAction() == ETaGAction::Gathered)
	{
		// Out of a drive: dunk or euro step near the rim, otherwise pass or pull up (gather rule)
		if (!bDashShotRolled && (Nd < 3.6f || Me->GetAction() == ETaGAction::Gathered))
		{
			bDashShotRolled = true;
			if (Nd < 3.6f && FMath::FRand() < 0.65f)
			{
				Wish(Me, ToRim);
				StartShot(Me);
				return;
			}
			if (TryPass(Me, true))
			{
				return;
			}
			if (Me->GetAction() == ETaGAction::Gathered)
			{
				StartShot(Me);
			}
		}
		return;
	}
	bDashShotRolled = false;

	// Catch and shoot: open right after the catch -> let it fly (the pass created the advantage)
	if (Me->IsFreshCatch() && !GM->NeedsClear(Me->GetTeam()) && DOpp > 200.f && Nd < 7.8f && Me->GetAction() == ETaGAction::None)
	{
		Wish(Me, FVector::ZeroVector);
		StartShot(Me);
		return;
	}

	// 3x3: after a change of possession, clear the ball beyond the arc first (or pass to someone out there)
	if (GM->NeedsClear(Me->GetTeam()))
	{
		const FVector Out = TaG::RimFloor() - ToRim.GetSafeNormal() * (TaG::ArcRadius + 120.f);
		Steer(Me, Out);
		if (HoldTime > 0.6f && FMath::FRand() < Dt * 2.f)
		{
			TryPass(Me, false);
		}
		return;
	}

	// Hero arts on offense
	const FTaGHeroDef& H = Me->GetHeroDef();
	if (H.Art1 == ETaGArt::Scope && Nd > 5.f && DOpp > 180.f)
	{
		UseArtIf(Me, 1, FMath::FRand() < Dt * 2.f);
	}
	if (H.Art2 == ETaGArt::Tether && !bTetherUsed && FMath::FRand() < Dt)
	{
		bTetherUsed = UseArtIf(Me, 2, true);
	}

	FVector WishDir = ToRim.GetSafeNormal();
	if (bBetween)
	{
		WishDir = (WishDir + Me->GetActorRightVector() * Strafe * 0.9f).GetSafeNormal();
	}
	// Close to a line with the ball: lean back toward the middle of the court
	const float LineD = TaG::DistToBoundary(Feet);
	if (LineD < 110.f)
	{
		const FVector Inward = Flat(FVector(TaG::CourtDepth * 0.5f, 0.f, 0.f) - Feet).GetSafeNormal();
		WishDir = (WishDir + Inward * (1.6f * (1.f - LineD / 110.f))).GetSafeNormal();
	}
	Wish(Me, WishDir);
	if (!Me->IsForcedMove() && Me->GetAction() == ETaGAction::None)
	{
		Me->AddMovementInput(WishDir, 0.8f);
	}
	// Reaction: see the tackle start and spin away, also out of a drive (the startup beats reaction time, so only sometimes)
	const bool bCanSpin = Me->GetAction() == ETaGAction::None || Me->IsDriving();
	if (bCanSpin && Opp && Opp->GetAction() == ETaGAction::TackleStart && ReactedTo != static_cast<uint8>(ETaGAction::TackleStart))
	{
		ReactedTo = static_cast<uint8>(ETaGAction::TackleStart);
		if (FMath::FRand() < Skill * 0.3f)
		{
			Queue.Add({ (FMath::FRandRange(0.25f, 0.4f) - Skill * 0.08f) * 0.5f, EHand::Dodge });
		}
	}
	else if (!Opp || Opp->GetAction() != ETaGAction::TackleStart)
	{
		ReactedTo = 255;
	}
	if (Me->GetAction() != ETaGAction::None)
	{
		return;
	}

	// Data-driven arts: make room when crowded, slip a tackle that is coming
	uint8 OppTell = 0;
	const bool bTackleComing = Opp && (Opp->GetAction() == ETaGAction::TackleStart || (Opp->IsPretelling(OppTell) && OppTell == 1));
	if (bTackleComing && DOpp < 280.f && TryArts(Me, ETaGArtAI::OffenseEvade, 1000.f, Dt))
	{
		return;
	}
	if (DOpp < 220.f && TryArts(Me, ETaGArtAI::OffenseEvade, 0.3f, Dt))
	{
		return;
	}
	if (DOpp < 200.f && TryArts(Me, ETaGArtAI::OffenseSpace, 0.9f, Dt))
	{
		return;
	}
	if (!bBetween && Nd > 3.5f && Nd < 9.f && TryArts(Me, ETaGArtAI::OffenseDrive, 0.6f, Dt))
	{
		return;
	}
	if (GM->GetShotClockLeft() < 2.f || ToRim.Size() < 150.f)
	{
		PlanShot(Me, Nd, DOpp);
		return;
	}
	ThinkT -= Dt;
	if (ThinkT > 0.f)
	{
		return;
	}
	ThinkT = FMath::FRandRange(0.3f, 0.6f) / (0.6f + Skill * 0.6f);
	if (FMath::FRand() < 0.25f)
	{
		Strafe = -Strafe;
	}

	// Team play first: a teammate who is wide open, or help when pressured
	if (TryPass(Me, DOpp < 160.f))
	{
		return;
	}

	const bool bOpen = DOpp > 230.f || !bBetween;
	// An open lane: keep running and finish at the rim (a layup) instead of pulling up from the first open spot
	const float SpeedIn = FMath::Max(0.f, FVector::DotProduct(Me->GetVelocity(), ToRim.GetSafeNormal()));
	if (bAttackRim && !bBetween && Nd < 7.f && Nd * 100.f > FTaGRules::LayupRange(SpeedIn) - 20.f)
	{
		return;
	}
	if (bOpen && Nd < 8.f && FMath::FRand() < 0.75f)
	{
		PlanShot(Me, Nd, DOpp);
		return;
	}
	if (!bBetween)
	{
		return;
	}
	TMap<EHand, float> W;
	W.Add(EHand::Drive, 1.f);
	W.Add(EHand::Dodge, 0.5f);
	W.Add(EHand::Shoot, Nd < 7.5f ? 0.7f : 0.15f);
	W.Add(EHand::Wait, 0.6f);
	W[OffFav] *= 2.5f;
	if (Opp && (Opp->IsContesting() || Opp->IsContestStarting()))
	{
		W[EHand::Drive] = 3.f;
		W[EHand::Shoot] = 0.05f;
	}
	if (Opp && Opp->GetCooldownLeft(ETaGSlot::Tackle) > 0.6f)
	{
		W[EHand::Drive] += 2.f;
		W[EHand::Dodge] = 0.1f;
	}
	if (!Me->CanUseInput(ETaGInput::Primary)) { W[EHand::Drive] = 0.f; }
	if (!Me->CanUseInput(ETaGInput::Secondary)) { W[EHand::Dodge] = 0.f; }
	switch (Pick(W))
	{
	case EHand::Drive:
		if (FMath::FRand() > 0.5f || !TryArts(Me, ETaGArtAI::OffenseDrive, 1000.f, Dt))
		{
			Do(Me, EHand::Drive);
		}
		break;
	case EHand::Dodge:
		if (FMath::FRand() > 0.5f || !TryArts(Me, ETaGArtAI::OffenseEvade, 1000.f, Dt))
		{
			Do(Me, EHand::Dodge);
		}
		break;
	case EHand::Shoot:
		PlanShot(Me, Nd, DOpp);
		break;
	default:
		break;
	}
}

bool ATaGBotController::TryAlleyOop(ATaGCharacter* Me, float Dt)
{
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	if (!GM || Me->GetCooldownLeft(ETaGSlot::Pass) > 0.f)
	{
		return false;
	}
	for (ATaGCharacter* M : GM->GetTeamPlayers(Me->GetTeam()))
	{
		if (M == Me)
		{
			continue;
		}
		const FTaGAlleyOop Oop = FTaGRules::PlanAlleyOop(Me, M);
		if (!Oop.bValid)
		{
			continue;
		}
		// A defender waiting at the catch spot would meet the lob first
		bool bClear = true;
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It && bClear; ++It)
		{
			bClear = It->GetTeam() == Me->GetTeam() || It->IsDown() || FVector::Dist2D(It->GetActorLocation(), Oop.Catch) > 130.f;
		}
		// Bots read a cut a moment late
		if (!bClear || FMath::FRand() > Dt * (2.f + 4.f * Skill))
		{
			return false;
		}
		Face(Me, M->GetActorLocation(), 1.f, 1000.f);
		return Press(Me, ETaGInput::PassSteal);
	}
	return false;
}

bool ATaGBotController::TryPass(ATaGCharacter* Me, bool bPressured)
{
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	const TArray<ATaGCharacter*> Mates = GM->GetTeamPlayers(Me->GetTeam());
	if (Mates.Num() < 2)
	{
		return false;
	}
	const float MyOpen = OpenNess(Me);
	const float MyValue = FTaGRules::BaseChance(TaG::DistToRim(Me->GetFeetLocation())) * (TaG::IsBeyondArc(Me->GetFeetLocation()) ? 2.f : 1.f);
	ATaGCharacter* Best = nullptr;
	float BestScore = 0.f;
	bool bLaneRisky = false;
	for (ATaGCharacter* M : Mates)
	{
		if (M == Me || M->IsDown())
		{
			continue;
		}
		const float Open = OpenNess(M);
		const FVector Feet = M->GetFeetLocation();
		float Value = FTaGRules::BaseChance(TaG::DistToRim(Feet)) * (TaG::IsBeyondArc(Feet) ? 2.f : 1.f);
		if (GM->NeedsClear(Me->GetTeam()) && TaG::IsBeyondArc(Feet))
		{
			Value += 0.6f; // passing out clears the ball
		}
		if (M->IsSpottingUp())
		{
			Value *= 1.6f; // the shooter called for it
		}
		// Lane safety: a defender close to the pass line
		bool bRisky = false;
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetTeam() == Me->GetTeam())
			{
				continue;
			}
			const FVector C = FMath::ClosestPointOnSegment(It->GetActorLocation(), Me->GetActorLocation(), M->GetActorLocation());
			if (FVector::Dist2D(C, It->GetActorLocation()) < 90.f)
			{
				bRisky = true;
			}
		}
		const float Score = Value * FMath::Clamp(Open / 250.f, 0.f, 1.6f) * (bRisky ? 0.35f : 1.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = M;
			bLaneRisky = bRisky;
		}
	}
	if (!Best)
	{
		return false;
	}
	const float Mine = MyValue * FMath::Clamp(MyOpen / 250.f, 0.f, 1.6f);
	const bool bWorth = BestScore > Mine * (bPressured ? 0.75f : 1.1f) && OpenNess(Best) > (Best->IsSpottingUp() ? 100.f : 150.f);
	if (!bWorth || FMath::FRand() > 0.5f + 0.35f * Skill)
	{
		return false;
	}
	Face(Me, Best->GetActorLocation(), 1.f, 1000.f);
	// Tsumugi threads risky lanes
	if ((bLaneRisky || FMath::FRand() < 0.35f) && Me->GetHeroDef().Art1 == ETaGArt::ThreadPass && Me->CanUseInput(ETaGInput::Art1) && Press(Me, ETaGInput::Art1))
	{
		return true;
	}
	// Curves and lobs go over a risky lane
	if ((bLaneRisky || FMath::FRand() < 0.3f) && TryArts(Me, ETaGArtAI::OffensePass, 1000.f, 1.f))
	{
		return true;
	}
	return Press(Me, ETaGInput::PassSteal);
}

void ATaGBotController::PlanShot(ATaGCharacter* Me, float Nd, float DefDist)
{
	// Pick a shot type and enter its inputs (stick direction, jump) like a player would.
	const FVector ToRim = Flat(TaG::RimFloor() - Me->GetFeetLocation()).GetSafeNormal();
	int32 Plan = 0; // 0 auto, 1 drive-in, 2 stepback, 3 fade, 4 jumper
	const float SpeedIn = FMath::Max(0.f, FVector::DotProduct(Me->GetVelocity(), ToRim));
	if (Nd * 100.f < FTaGRules::LayupRange(SpeedIn))
	{
		Plan = 1; // close enough for a run-up layup
	}
	else if (DefDist < 180.f)
	{
		const float R = FMath::FRand() * 2.8f;
		Plan = R < 1.2f ? 2 : (R < 2.2f ? 3 : 4);
	}
	else if (Nd < 5.5f)
	{
		Plan = FMath::RandBool() ? 1 : 4;
	}
	else
	{
		Plan = 4;
	}
	switch (Plan)
	{
	case 1:
		Wish(Me, ToRim);
		StartShot(Me);
		break;
	case 2:
		Wish(Me, -ToRim);
		StartShot(Me);
		break;
	case 3:
		Me->InputJump();
		FadeWait = 0.12f;
		break;
	default:
		Wish(Me, FVector::ZeroVector);
		StartShot(Me);
		break;
	}
}

void ATaGBotController::StartShot(ATaGCharacter* Me)
{
	TryArts(Me, ETaGArtAI::OffenseShoot, 1000.f, 1.f); // shot buffs go up right before the shot
	ReleaseAt = FMath::Clamp(0.74f + FMath::FRandRange(-1.f, 1.f) * 0.22f * (1.15f - Skill), 0.35f, 1.3f);
	Press(Me, ETaGInput::Shoot);
}

void ATaGBotController::OffBall(ATaGCharacter* Me, ATaGCharacter* Handler, float Dt)
{
	const int32 Index = TeamIndex(Me);
	FVector Spot = TaG::OffenseSpot(1 + (Index % 4));
	if (FVector::Dist2D(Spot, Handler->GetFeetLocation()) < 250.f)
	{
		Spot = TaG::OffenseSpot(1 + ((Index + 2) % 6));
	}
	const FTaGHeroDef& H = Me->GetHeroDef();

	// Cuts: now and then, sprint to the rim (a cut beats a defender who ball-watches)
	CutTimer -= Dt;
	if (CuttingFor > 0.f)
	{
		CuttingFor -= Dt;
		Steer(Me, TaG::RimFloor() + FVector(80.f, 0.f, 0.f), 30.f);
		Face(Me, Handler->GetActorLocation(), Dt, 8.f);
		return;
	}
	if (CutTimer <= 0.f)
	{
		CutTimer = FMath::FRandRange(3.5f, 7.f);
		if (FMath::FRand() < 0.35f + 0.35f * Skill && OpenNess(Me) < 400.f)
		{
			CuttingFor = 1.2f;
			Wish(Me, Flat(TaG::RimFloor() - Me->GetFeetLocation()));
			Press(Me, ETaGInput::Primary); // a cut is a drive without the ball
			return;
		}
	}
	Steer(Me, Spot, 50.f);
	Face(Me, Handler->GetActorLocation(), Dt, 8.f);

	// Arts that help without the ball
	TryArts(Me, ETaGArtAI::OffBall, 0.35f, Dt);
	TryArts(Me, ETaGArtAI::SupportAllies, 0.25f, Dt);
	const bool bSettled = FVector::Dist2D(Spot, Me->GetFeetLocation()) < 80.f;
	if (H.Art2 == ETaGArt::SpotUp)
	{
		UseArtIf(Me, 2, bSettled && FMath::FRand() < Dt * 0.8f);
	}
	if (H.Art2 == ETaGArt::Tether)
	{
		UseArtIf(Me, 2, FVector::Dist2D(Handler->GetActorLocation(), Me->GetActorLocation()) < 900.f && FMath::FRand() < Dt * 0.3f);
	}
	if (H.Art2 == ETaGArt::Resync)
	{
		UseArtIf(Me, 2, ResyncWorth(Me) && FMath::FRand() < Dt * 2.f);
	}
	if (H.Art2 == ETaGArt::IronWall)
	{
		// A screen for the handler: wall between the handler's defender and the rim side
		ATaGCharacter* HD = Handler->FindNearestOpponent(200.f);
		if (HD && FVector::Dist2D(HD->GetActorLocation(), Me->GetActorLocation()) < 400.f && FMath::FRand() < Dt * 0.5f)
		{
			Face(Me, HD->GetActorLocation(), 1.f, 1000.f);
			UseArtIf(Me, 2, true);
		}
	}
}

// ================================================================ Defense

void ATaGBotController::DefendBall(ATaGCharacter* Me, ATaGCharacter* Hd, float Dt)
{
	const FVector Hp = Flat(Hd->GetFeetLocation());
	const FVector ToRim = Flat(TaG::RimFloor() - Hp);
	const float GuardD = ToRim.Size() < 850.f ? 150.f : 220.f;
	const FVector Gp = Hp + ToRim.GetSafeNormal() * FMath::Min(GuardD, ToRim.Size() * 0.6f);
	const FVector ToGp = Gp - Flat(Me->GetFeetLocation());
	Wish(Me, ToGp);
	if (!Me->IsForcedMove())
	{
		Me->AddMovementInput(ToGp.GetSafeNormal(), FMath::Clamp(ToGp.Size() / 60.f, 0.f, 1.f));
	}
	Face(Me, Hd->GetActorLocation(), Dt, 12.f);
	const float D = FVector::Dist2D(Hp, Me->GetFeetLocation());
	const FTaGHeroDef& H = Me->GetHeroDef();

	// Reactions to what the handler shows
	const uint8 HS = static_cast<uint8>(Hd->GetAction());
	if (HS != ReactedTo)
	{
		ReactedTo = HS;
		const float React = FMath::FRandRange(0.25f, 0.4f) - Skill * 0.08f;
		switch (Hd->GetAction())
		{
		case ETaGAction::Shoot:
			if (D < 260.f && Me->CanUseInput(ETaGInput::Secondary) && FMath::FRand() < 0.05f + Skill * 0.45f)
			{
				Queue.Add({ React, EHand::Contest });
			}
			else if (D < 190.f && FMath::FRand() < 0.05f + Skill * 0.25f)
			{
				Me->InputJump();
			}
			break;
		case ETaGAction::DriveStart:
			if (D < 300.f && FMath::FRand() < Skill * 0.35f)
			{
				Queue.Add({ React * 0.6f, EHand::Tackle });
			}
			break;
		case ETaGAction::DodgeRecover:
			if (D < 200.f)
			{
				Queue.Add({ 0.08f, EHand::Steal });
			}
			break;
		default:
			break;
		}
	}
	if (Hd->GetAction() == ETaGAction::Drive && Me->GetAction() == ETaGAction::None)
	{
		const float HRim = TaG::DistToRim(Hp);
		const float MeRim = TaG::DistToRim(Me->GetFeetLocation());
		if (HRim < MeRim + 30.f && FMath::FRand() < Skill)
		{
			Wish(Me, ToGp);
			Press(Me, ETaGInput::Step);
		}
		// Minamo's pull catches drives from range
		if (H.Art1 == ETaGArt::Pull && D > 250.f && D < 750.f)
		{
			Face(Me, Hd->GetActorLocation(), 1.f, 1000.f);
			UseArtIf(Me, 1, FMath::FRand() < 0.5f);
		}
	}

	// Data-driven arts by distance: up close, at the shot, from range, near the rim
	if (Hd->IsChargingShot() && D < 340.f && TryArts(Me, ETaGArtAI::DefendShooter, 1000.f, Dt))
	{
		return;
	}
	if (D < 240.f && Me->GetAction() == ETaGAction::None && TryArts(Me, ETaGArtAI::DefendHandler, 0.4f + Skill, Dt))
	{
		return;
	}
	if (D > 260.f && D < 780.f && Me->GetAction() == ETaGAction::None)
	{
		Face(Me, Hd->GetActorLocation(), 1.f, 1000.f);
		if (TryArts(Me, ETaGArtAI::DefendRanged, 0.5f + Skill * 0.5f, Dt))
		{
			return;
		}
	}
	if (TaG::DistToRim(Hp) < 480.f && D < 420.f && TryArts(Me, ETaGArtAI::DefendPaint, 0.7f, Dt))
	{
		return;
	}
	TryArts(Me, ETaGArtAI::SupportAllies, 0.15f, Dt);

	// Area arts near the paint
	if (H.Art2 == ETaGArt::SlowField && TaG::DistToRim(Hp) < 450.f && D < 350.f)
	{
		UseArtIf(Me, 2, FMath::FRand() < Dt * 1.5f);
	}
	if (H.Art2 == ETaGArt::IronWall && TaG::DistToRim(Hp) < 600.f && D < 300.f)
	{
		UseArtIf(Me, 2, FMath::FRand() < Dt * 0.8f);
	}

	uint8 Pt = 0;
	if (Me->GetAction() != ETaGAction::None || D > 240.f || Me->IsPretelling(Pt))
	{
		return;
	}
	ThinkT -= Dt;
	if (ThinkT > 0.f)
	{
		return;
	}
	ThinkT = FMath::FRandRange(0.4f, 0.8f) / (0.6f + Skill * 0.6f);
	TMap<EHand, float> W;
	W.Add(EHand::Tackle, 0.6f);
	W.Add(EHand::Contest, 0.6f);
	W.Add(EHand::Wait, 1.6f);
	W.Add(EHand::Steal, 0.1f);
	W[DefFav] *= 2.5f;
	if (!Me->CanUseInput(ETaGInput::Primary) && !(H.Art1 == ETaGArt::ShoulderCharge && Me->CanUseInput(ETaGInput::Art1))) { W[EHand::Tackle] = 0.f; }
	if (!Me->CanUseInput(ETaGInput::Secondary)) { W[EHand::Contest] = 0.f; }
	if (!Me->CanUseInput(ETaGInput::PassSteal)) { W[EHand::Steal] = 0.f; }
	if (TaG::DistToRim(Hp) > 750.f) { W[EHand::Contest] *= 0.3f; }
	switch (Pick(W))
	{
	case EHand::Tackle:
		Commit(Me, EHand::Tackle);
		break;
	case EHand::Contest:
		Commit(Me, EHand::Contest);
		break;
	case EHand::Steal:
		Do(Me, EHand::Steal);
		break;
	default:
		break;
	}
}

void ATaGBotController::DefendOffBall(ATaGCharacter* Me, ATaGCharacter* Man, ATaGCharacter* Hd, float Dt)
{
	// Deny: between my man and the rim, sagging toward the ball; help when the handler drives at the rim.
	const FVector ManP = Flat(Man->GetFeetLocation());
	const FVector Rim = TaG::RimFloor();
	FVector Spot = ManP + (Rim - ManP) * 0.28f + (Flat(Hd->GetFeetLocation()) - ManP) * 0.15f;
	const bool bHelp = Hd->IsDriving() && TaG::DistToRim(Hd->GetFeetLocation()) < 450.f && FVector::Dist2D(Hd->GetActorLocation(), Me->GetActorLocation()) < 450.f;
	if (bHelp)
	{
		Spot = Flat(Hd->GetFeetLocation()) + (Rim - Flat(Hd->GetFeetLocation())).GetSafeNormal() * 90.f;
		if (FVector::Dist2D(Hd->GetActorLocation(), Me->GetActorLocation()) < 200.f && FMath::FRand() < Skill * Dt * 4.f)
		{
			Do(Me, EHand::Tackle);
		}
	}
	Steer(Me, Spot, 30.f);
	Face(Me, Hd->GetActorLocation(), Dt, 8.f);
	TryArts(Me, ETaGArtAI::SupportAllies, 0.2f, Dt);
	if (TaG::DistToRim(Hd->GetFeetLocation()) < 480.f)
	{
		TryArts(Me, ETaGArtAI::DefendPaint, 0.3f, Dt);
	}
	const float DH = FVector::Dist2D(Hd->GetActorLocation(), Me->GetActorLocation());
	if (DH > 260.f && DH < 780.f && Me->GetAction() == ETaGAction::None)
	{
		TryArts(Me, ETaGArtAI::DefendRanged, 0.25f, Dt);
	}

	// Jump into the passing lane to my man now and then (handled by the ball as an interception)
	const FTaGHeroDef& H = Me->GetHeroDef();
	if (H.Art1 == ETaGArt::Pull && Hd->IsDriving())
	{
		const float D = FVector::Dist2D(Hd->GetActorLocation(), Me->GetActorLocation());
		if (D > 250.f && D < 750.f)
		{
			Face(Me, Hd->GetActorLocation(), 1.f, 1000.f);
			UseArtIf(Me, 1, FMath::FRand() < Dt * 2.f);
		}
	}
	if (H.Art2 == ETaGArt::Resync)
	{
		UseArtIf(Me, 2, ResyncWorth(Me) && FMath::FRand() < Dt * 2.f);
	}
}

// ================================================================ Loose ball / rebounds

void ATaGBotController::Loose(ATaGCharacter* Me, ATaGBall* Ball, float Dt)
{
	const FVector Bp = Ball->GetActorLocation();
	const FVector Rim = TaG::RimFloor();
	FVector Target = Flat(Bp);
	const ETaGBallState St = Ball->GetBallState();

	if (St == ETaGBallState::Flight)
	{
		// Crash the glass: around the rim
		Target = Rim + Flat(Me->GetFeetLocation() - Rim).GetSafeNormal() * 140.f;
		if (Ball->GetShotTeam() != Me->GetTeam() && !bBlockDecided && FVector::Dist(Me->GetEyeLocation(), Bp) < 160.f)
		{
			bBlockDecided = true;
			if (FMath::FRand() < 0.1f + 0.25f * Skill)
			{
				Face(Me, Bp, 1.f, 1000.f);
				Me->InputJump();
			}
		}
	}
	else if (St == ETaGBallState::Pass)
	{
		ATaGCharacter* Receiver = Ball->GetPassReceiver();
		if (Ball->GetPassStyle() == 4 && Receiver == Me && Me->HasAlleyOop())
		{
			// Going up for the lob: run at the catch spot and leave the floor so the hands meet it at the top
			const FVector C = Me->GetAlleyOopPoint();
			const FVector To = Flat(C - Me->GetFeetLocation());
			Wish(Me, To);
			if (!Me->IsForcedMove())
			{
				Me->AddMovementInput(To.GetSafeNormal(), 1.f);
			}
			Face(Me, C, Dt, 12.f);
			if (Me->GetAlleyOopJumpIn() <= LeapSlack * (1.2f - Skill) && Me->GetCharacterMovement()->IsMovingOnGround())
			{
				Me->InputJump();
			}
			return;
		}
		if (Ball->GetPassStyle() == 4 && Receiver && Receiver->GetTeam() != Me->GetTeam() && Receiver->HasAlleyOop())
		{
			// Rim protection: the defender nearest the catch spot goes up to meet the lob
			const FVector C = Receiver->GetAlleyOopPoint();
			const float MyD = FVector::Dist2D(Me->GetActorLocation(), C);
			bool bNearest = MyD < 600.f;
			for (TActorIterator<ATaGCharacter> It(GetWorld()); It && bNearest; ++It)
			{
				bNearest = *It == Me || It->GetTeam() != Me->GetTeam() || FVector::Dist2D(It->GetActorLocation(), C) >= MyD;
			}
			if (bNearest)
			{
				Steer(Me, Flat(C), 10.f);
				Face(Me, C, Dt, 12.f);
				const float Up = TaG::JumpVelocity * Me->GetHeroDef().JumpMul / TaG::Gravity;
				if (!bOopDefDecided && Receiver->GetAlleyOopTime() - Me->ServerNow() < Up + 0.04f && MyD < 170.f)
				{
					bOopDefDecided = true;
					if (FMath::FRand() < 0.25f + 0.45f * Skill)
					{
						Me->InputJump();
					}
				}
				return;
			}
		}
		// Read the pass: the receiver's defender steps toward the catch point
		if (Receiver && Receiver->GetTeam() != Me->GetTeam() && MyMan(Me) == Receiver)
		{
			Target = Flat(FMath::Lerp(Bp, Receiver->GetActorLocation(), 0.6f));
		}
		else
		{
			Target = Flat(Me->GetFeetLocation());
		}
	}
	else
	{
		bBlockDecided = false;
		// Only the closest player of each team chases; the rest hold position near the rim
		float MyD = FVector::Dist2D(Bp, Me->GetActorLocation());
		bool bClosest = true;
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != Me && It->GetTeam() == Me->GetTeam() && FVector::Dist2D(Bp, It->GetActorLocation()) < MyD - 50.f)
			{
				bClosest = false;
			}
		}
		if (!bClosest)
		{
			Target = Rim + Flat(Bp - Rim).GetSafeNormal() * 300.f + FVector(0.f, (TeamIndex(Me) % 2 ? 200.f : -200.f), 0.f);
		}
		if (Me->GetAction() == ETaGAction::None && bClosest)
		{
			if (MyD > 400.f && FMath::FRand() < 0.05f)
			{
				Press(Me, ETaGInput::Primary);
			}
			if (Bp.Z > Me->GetFeetLocation().Z + Me->GetHeroDef().EyeHeight * 1.1f && MyD < 120.f)
			{
				Me->InputJump();
			}
		}
	}
	Steer(Me, Target, 10.f);
	Face(Me, Bp, Dt, 10.f);
}
