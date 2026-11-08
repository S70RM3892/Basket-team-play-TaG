#include "Core/TaGGameMode.h"
#include "BasketTaG.h"
#include "Ball/TaGBall.h"
#include "Ball/TaGBallSim.h"
#include "Character/TaGBotController.h"
#include "Character/TaGCharacter.h"
#include "Character/TaGPlayerController.h"
#include "Core/TaGGameState.h"
#include "Core/TaGRules.h"
#include "Core/TaGTypes.h"
#include "UI/TaGHUD.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	// Spots between possessions: the offense checks the ball at the top, the defense waits inside the arc.
	const FVector OffenseCheckSpot(TaG::RimX + TaG::ArcRadius + 120.f, 0.f, 0.f);
	const FVector DefenseCheckSpot(TaG::RimX + TaG::ArcRadius - 120.f, 0.f, 0.f);
	const FVector SideSpot(TaG::RimX + 300.f, 380.f, 0.f);

	FLinearColor TeamColor(int32 Team)
	{
		if (Team == 0)
		{
			return FLinearColor(0.35f, 0.65f, 1.f);
		}
		if (Team == 1)
		{
			return FLinearColor(1.f, 0.4f, 0.35f);
		}
		return FLinearColor(1.f, 0.95f, 0.5f);
	}

	FString TeamName(int32 Team)
	{
		return Team == 0 ? TEXT("あなた") : TEXT("BOT");
	}
}

ATaGGameMode::ATaGGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ATaGCharacter::StaticClass();
	PlayerControllerClass = ATaGPlayerController::StaticClass();
	GameStateClass = ATaGGameState::StaticClass();
	HUDClass = ATaGHUD::StaticClass();
	Players.SetNum(2);
}

ATaGGameState* ATaGGameMode::GS() const
{
	return GetGameState<ATaGGameState>();
}

void ATaGGameMode::BeginPlay()
{
	Super::BeginPlay();

	const TCHAR* Cmd = FCommandLine::Get();
	bShotTest = FParse::Param(Cmd, TEXT("TaGShotTest"));
	bAutoTest = FParse::Param(Cmd, TEXT("TaGAutoTest"));
	bSnap = FParse::Param(Cmd, TEXT("TaGSnap"));

	if (bShotTest)
	{
		RunShotTest();
		FPlatformMisc::RequestExit(false);
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Ball = GetWorld()->SpawnActor<ATaGBall>(ATaGBall::StaticClass(), FTransform(FVector(TaG::RimX + 400.f, 0.f, 100.f)), Params);

	SpawnBot(1, bAutoTest ? 0.6f : 0.5f, DefenseCheckSpot);
	if (bAutoTest)
	{
		SpawnBot(0, 0.6f, OffenseCheckSpot);
	}
	ScheduleTurnover(0, 1.0f);
}

void ATaGGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer || NewPlayer->GetPawn())
	{
		return;
	}
	if (bAutoTest || bShotTest)
	{
		return; // spectator only
	}
	// No player starts on the generated map: spawn at the top of the key facing the rim.
	const FVector Feet = OffenseCheckSpot;
	const FRotator Rot = (TaG::RimCenter() - Feet).Rotation();
	RestartPlayerAtTransform(NewPlayer, FTransform(FRotator(0.f, Rot.Yaw, 0.f), Feet + FVector(0.f, 0.f, 100.f)));
	if (ATaGCharacter* C = Cast<ATaGCharacter>(NewPlayer->GetPawn()))
	{
		RegisterPlayer(C, 0);
	}
}

ATaGCharacter* ATaGGameMode::SpawnBot(int32 Team, float Skill, const FVector& Feet)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FRotator Rot(0.f, (TaG::RimCenter() - Feet).Rotation().Yaw, 0.f);
	ATaGCharacter* C = GetWorld()->SpawnActor<ATaGCharacter>(ATaGCharacter::StaticClass(), Feet + FVector(0.f, 0.f, 100.f), Rot, Params);
	ATaGBotController* AI = GetWorld()->SpawnActor<ATaGBotController>(ATaGBotController::StaticClass(), Feet, Rot, Params);
	if (C && AI)
	{
		AI->Skill = Skill;
		AI->Possess(C);
		RegisterPlayer(C, Team);
	}
	return C;
}

void ATaGGameMode::RegisterPlayer(ATaGCharacter* C, int32 Team)
{
	C->SetTeam(Team);
	Players[Team] = C;
}

bool ATaGGameMode::NeedsClear(int32 Team) const
{
	return GS() && GS()->NeedsClear(Team);
}

float ATaGGameMode::GetShotClockLeft() const
{
	return GS() ? GS()->ShotClock : TaG::ShotClock;
}

void ATaGGameMode::SetNeedsClear(int32 Team, bool bValue)
{
	if (ATaGGameState* S = GS())
	{
		(Team == 0 ? S->bNeedsClear0 : S->bNeedsClear1) = bValue;
	}
}

void ATaGGameMode::Popup(const FString& Text, int32 Team)
{
	if (ATaGGameState* S = GS())
	{
		S->MulticastPopup(FText::FromString(Text), TeamColor(Team));
	}
	if (bAutoTest)
	{
		UE_LOG(LogTaG, Verbose, TEXT("[%6.1f] %s"), MatchTime, *Text);
	}
}

// ---------------------------------------------------------------- Flow

void ATaGGameMode::ScheduleTurnover(int32 ToTeam, float Delay)
{
	bLive = false;
	bShotInFlight = false;
	NextPossessionTeam = ToTeam;
	DeadTimeLeft = Delay;
	if (Ball)
	{
		Ball->SetDead();
	}
}

void ATaGGameMode::StartPossession(int32 Team)
{
	ATaGGameState* S = GS();
	ATaGCharacter* Off = Players[Team];
	ATaGCharacter* Def = Players[1 - Team];
	if (!S || !Ball || !Off)
	{
		DeadTimeLeft = 0.5f; // a player is not spawned yet; try again shortly
		return;
	}
	Off->PlaceAt(OffenseCheckSpot, TaG::RimCenter());
	if (Def)
	{
		Def->PlaceAt(DefenseCheckSpot, Off->GetActorLocation());
	}
	Ball->GiveTo(Off);
	S->PossessionTeam = Team;
	S->ShotClock = TaG::ShotClock;
	SetNeedsClear(0, false);
	SetNeedsClear(1, false);
	Stats.Possessions[Team]++;
	bLive = true;
	bShotInFlight = false;
}

void ATaGGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ATaGGameState* S = GS();
	if (!S || bShotTest)
	{
		return;
	}
	MatchTime += DeltaSeconds;

	if (bSnap)
	{
		TickSnap(DeltaSeconds);
	}

	if (S->bMatchOver)
	{
		DeadTimeLeft -= DeltaSeconds;
		if (DeadTimeLeft <= 0.f)
		{
			if (bAutoTest)
			{
				FPlatformMisc::RequestExit(false);
				return;
			}
			RestartMatch();
		}
		return;
	}

	if (!bLive)
	{
		DeadTimeLeft -= DeltaSeconds;
		if (DeadTimeLeft <= 0.f)
		{
			StartPossession(NextPossessionTeam);
		}
		return;
	}

	if (bAutoTest && MatchTime > 1200.f)
	{
		UE_LOG(LogTaG, Warning, TEXT("AUTOTEST timeout"));
		LogStats();
		FPlatformMisc::RequestExit(false);
		return;
	}

	// Shot clock runs while the ball is alive and not on its way to the rim.
	if (!bShotInFlight)
	{
		S->ShotClock -= DeltaSeconds;
		if (S->ShotClock <= 0.f)
		{
			const int32 Team = S->PossessionTeam;
			Stats.ShotClockViolations[Team]++;
			Popup(TEXT("ショットクロック・バイオレーション"), 1 - Team);
			ScheduleTurnover(1 - Team, 1.0f);
			return;
		}
	}

	// 3x3 clear rule: the ball must be taken beyond the arc after a change of possession.
	if (Ball && Ball->GetBallState() == ETaGBallState::Held)
	{
		if (ATaGCharacter* H = Ball->GetHolder())
		{
			if (NeedsClear(H->GetTeam()) && TaG::IsBeyondArc(H->GetFeetLocation()))
			{
				SetNeedsClear(H->GetTeam(), false);
				Popup(TEXT("クリア"), H->GetTeam());
			}
		}
	}
}

// ---------------------------------------------------------------- Shots

void ATaGGameMode::ResolveShot(ATaGCharacter* Shooter, float Charge)
{
	if (!bLive || !Ball || !Shooter || Ball->GetHolder() != Shooter)
	{
		return;
	}
	const int32 Team = Shooter->GetTeam();
	const FTaGShotEval E = FTaGRules::EvaluateShot(Shooter, Charge);
	const bool bMake = FMath::FRand() < E.Chance;
	const bool bValid = !NeedsClear(Team);

	const FVector Feet = Shooter->GetFeetLocation();
	const FVector From = FTaGBallSim::ReleasePoint(Feet);
	FVector Target = TaG::RimCenter() + FVector(0.f, 0.f, 2.f);
	if (!bMake)
	{
		// A miss is aimed at the rim's edge; the rim and board decide where the rebound goes.
		const float A = FMath::FRandRange(0.f, 2.f * UE_PI);
		Target += FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::FRandRange(20.f, 32.f);
	}
	const float Angle = FTaGBallSim::ShotAngleForDistance(E.Distance);
	const FVector Vel = FTaGBallSim::SolveLaunch(From, Target, Angle);
	Ball->LaunchShot(From, Vel, Shooter, E.Points, bValid);
	bShotInFlight = true;

	Stats.Shots[Team]++;
	Stats.ExpectedMakes[Team] += E.Chance;
	if (E.bPerfect)
	{
		Stats.Perfect[Team]++;
		Popup(TEXT("パーフェクト・リリース！"), -1);
	}
	if (E.bContested)
	{
		Popup(FString::Printf(TEXT("跳んで妨害！ %s"), *TeamName(1 - Team)), 1 - Team);
	}
	UE_LOG(LogTaG, Verbose, TEXT("Shot team=%d dist=%.0f pts=%d charge=%.2f p=%.2f make=%d"), Team, E.Distance, E.Points, Charge, E.Chance, bMake ? 1 : 0);
}

void ATaGGameMode::OnBasket(ATaGCharacter* Shooter, int32 Points, bool bValid)
{
	ATaGGameState* S = GS();
	const int32 Team = Shooter ? Shooter->GetTeam() : (S ? S->PossessionTeam : 0);
	bShotInFlight = false;
	if (!S)
	{
		return;
	}
	if (!bValid)
	{
		Popup(TEXT("クリア前のシュートは無効"), 1 - Team);
		ScheduleTurnover(1 - Team, 1.2f);
		return;
	}
	(Team == 0 ? S->Score0 : S->Score1) += Points;
	Stats.Points[Team] += Points;
	Stats.Makes[Team]++;
	Popup(FString::Printf(TEXT("%s +%d"), *TeamName(Team), Points), Team);

	if (S->GetScore(Team) >= TaG::TargetScore)
	{
		EndMatch(Team);
		return;
	}
	ScheduleTurnover(1 - Team, 1.2f);
}

void ATaGGameMode::OnShotMissed(bool bTouchedRim)
{
	bShotInFlight = false;
	if (bTouchedRim)
	{
		if (ATaGGameState* S = GS())
		{
			S->ShotClock = TaG::ShotClock; // 3x3: the clock resets when the ball touches the rim
		}
	}
}

void ATaGGameMode::OnBlocked(ATaGCharacter* Blocker)
{
	bShotInFlight = false;
	if (Blocker)
	{
		Stats.Blocks[Blocker->GetTeam()]++;
		Popup(FString::Printf(TEXT("ブロック！ %s"), *TeamName(Blocker->GetTeam())), Blocker->GetTeam());
	}
}

void ATaGGameMode::OnBallPickedUp(ATaGCharacter* Picker)
{
	ATaGGameState* S = GS();
	if (!S || !Picker)
	{
		return;
	}
	const int32 Team = Picker->GetTeam();
	if (Team != S->PossessionTeam)
	{
		// Defensive rebound or steal: new possession, must clear first.
		S->PossessionTeam = Team;
		S->ShotClock = TaG::ShotClock;
		Stats.Possessions[Team]++;
		const bool bInside = !TaG::IsBeyondArc(Picker->GetFeetLocation());
		SetNeedsClear(Team, bInside);
		SetNeedsClear(1 - Team, false);
		if (bInside)
		{
			Popup(TEXT("アークの外へクリア！"), Team);
		}
	}
}

bool ATaGGameMode::ResolveSteal(ATaGCharacter* Stealer)
{
	if (!bLive || !Ball || !Stealer || Ball->GetBallState() != ETaGBallState::Held)
	{
		return false;
	}
	ATaGCharacter* Handler = Ball->GetHolder();
	if (!Handler || Handler->GetTeam() == Stealer->GetTeam())
	{
		return false;
	}
	FVector To = Handler->GetActorLocation() - Stealer->GetActorLocation();
	To.Z = 0.f;
	const float D = To.Size();
	const FVector Fwd = Stealer->GetActorForwardVector();
	if (D > TaG::StealRange || FVector::DotProduct(Fwd, To.GetSafeNormal()) < 0.4f)
	{
		return false;
	}
	// A reach-in mostly fails; a whiff costs the defender a moment (StealWhiffRecover).
	const float Chance = Handler->IsChargingShot() ? 0.12f : 0.25f;
	if (FMath::FRand() >= Chance)
	{
		return false;
	}
	const FVector Push = (-To.GetSafeNormal()) * 250.f + FVector(0.f, 0.f, 200.f);
	Ball->MakeLoose(Ball->GetActorLocation(), Push, Stealer->GetTeam());
	Handler->ReleaseShot(); // cancels a charge (no-op otherwise); the ball is gone so nothing is shot
	Stats.Steals[Stealer->GetTeam()]++;
	Popup(FString::Printf(TEXT("スティール！ %s"), *TeamName(Stealer->GetTeam())), Stealer->GetTeam());
	return true;
}

void ATaGGameMode::OnOutOfBounds(int32 LastTouchTeam)
{
	const int32 To = LastTouchTeam == 0 ? 1 : 0;
	Popup(TEXT("アウト・オブ・バウンズ"), To);
	ScheduleTurnover(To, 1.0f);
}

void ATaGGameMode::OnStuckBall(int32 LastTouchTeam)
{
	const int32 To = LastTouchTeam == 0 ? 1 : 0;
	ScheduleTurnover(To, 0.5f);
}

void ATaGGameMode::EndMatch(int32 Winner)
{
	ATaGGameState* S = GS();
	bLive = false;
	if (Ball)
	{
		Ball->SetDead();
	}
	S->bMatchOver = true;
	S->WinnerTeam = Winner;
	Popup(FString::Printf(TEXT("%s の勝ち！  %d - %d"), *TeamName(Winner), S->Score0, S->Score1), Winner);
	DeadTimeLeft = bAutoTest ? 0.1f : 5.f;
	LogStats();
}

void ATaGGameMode::RestartMatch()
{
	ATaGGameState* S = GS();
	if (!S)
	{
		return;
	}
	S->Score0 = 0;
	S->Score1 = 0;
	S->bMatchOver = false;
	S->WinnerTeam = -1;
	Stats = FTaGMatchStats();
	ScheduleTurnover(0, 1.0f);
}

void ATaGGameMode::LogStats() const
{
	const ATaGGameState* S = GS();
	for (int32 T = 0; T < 2; ++T)
	{
		const float PPP = Stats.Possessions[T] > 0 ? float(Stats.Points[T]) / Stats.Possessions[T] : 0.f;
		const float FG = Stats.Shots[T] > 0 ? float(Stats.Makes[T]) / Stats.Shots[T] : 0.f;
		const float XFG = Stats.Shots[T] > 0 ? Stats.ExpectedMakes[T] / Stats.Shots[T] : 0.f;
		UE_LOG(LogTaG, Display, TEXT("STATS team=%d score=%d poss=%d ppp=%.2f shots=%d makes=%d fg=%.2f expected_fg=%.2f perfect=%d blocks=%d steals=%d clock_viol=%d"),
			T, S ? S->GetScore(T) : -1, Stats.Possessions[T], PPP, Stats.Shots[T], Stats.Makes[T], FG, XFG,
			Stats.Perfect[T], Stats.Blocks[T], Stats.Steals[T], Stats.ShotClockViolations[T]);
	}
	UE_LOG(LogTaG, Display, TEXT("STATS match_time=%.1f"), MatchTime);
}

// ---------------------------------------------------------------- Test modes

void ATaGGameMode::RunShotTest()
{
	// Planned makes must physically drop through the hoop from every distance and angle (like the Godot --shottest).
	const float Distances[] = { 80.f, 150.f, 250.f, 350.f, 450.f, 550.f, 650.f, 700.f, 800.f, 900.f };
	const float Bearings[] = { -80.f, -45.f, 0.f, 45.f, 80.f };
	int32 MakeOk = 0, MakeTotal = 0, MissIn = 0, MissTotal = 0;
	for (const float Dist : Distances)
	{
		int32 RowOk = 0, RowTotal = 0;
		for (const float Bearing : Bearings)
		{
			const float B = FMath::DegreesToRadians(Bearing);
			FVector Feet(TaG::RimX + FMath::Cos(B) * Dist, FMath::Sin(B) * Dist, 0.f);
			if (Feet.X < 30.f)
			{
				continue;
			}
			for (int32 Jump = 0; Jump < 2; ++Jump)
			{
				Feet.Z = Jump ? 60.f : 0.f;
				for (int32 Kind = 0; Kind < 2; ++Kind)
				{
					const bool bMake = Kind == 0;
					const int32 Trials = bMake ? 1 : 12;
					for (int32 T = 0; T < Trials; ++T)
					{
						const FVector From = FTaGBallSim::ReleasePoint(Feet);
						FVector Target = TaG::RimCenter() + FVector(0.f, 0.f, 2.f);
						if (!bMake)
						{
							const float A = 2.f * UE_PI * T / Trials;
							Target += FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (T % 2 ? 32.f : 20.f);
						}
						FTaGBallSim Sim;
						Sim.Pos = From;
						Sim.Vel = FTaGBallSim::SolveLaunch(From, Target, FTaGBallSim::ShotAngleForDistance(TaG::DistToRim(Feet)));
						bool bIn = false;
						for (float Time = 0.f; Time < 5.f && !bIn; Time += 1.f / 60.f)
						{
							bool bScored = false;
							Sim.Step(1.f / 60.f, bScored);
							bIn |= bScored;
						}
						if (bMake)
						{
							MakeTotal++;
							RowTotal++;
							if (bIn)
							{
								MakeOk++;
								RowOk++;
							}
							else
							{
								UE_LOG(LogTaG, Display, TEXT("SHOTTEST fail dist=%.0f bearing=%.0f jump=%d from=(%.0f,%.0f,%.0f) angle=%.0f"),
									Dist, Bearing, Jump, From.X, From.Y, From.Z, FTaGBallSim::ShotAngleForDistance(TaG::DistToRim(Feet)));
							}
						}
						else
						{
							MissTotal++;
							MissIn += bIn ? 1 : 0;
						}
					}
				}
			}
		}
		UE_LOG(LogTaG, Display, TEXT("SHOTTEST dist=%.1fm planned_makes_in=%d/%d"), Dist / 100.f, RowOk, RowTotal);
	}
	UE_LOG(LogTaG, Display, TEXT("SHOTTEST total planned_makes_in=%d/%d (%.0f%%)  planned_misses_that_went_in=%d/%d (%.0f%%)"),
		MakeOk, MakeTotal, 100.f * MakeOk / FMath::Max(MakeTotal, 1), MissIn, MissTotal, 100.f * MissIn / FMath::Max(MissTotal, 1));
}

void ATaGGameMode::TickSnap(float Dt)
{
	// -TaGSnap: take one screenshot after the scene settles, then quit.
	SnapTimer += Dt;
	if (SnapStage == 0 && SnapTimer > 8.f)
	{
		SnapStage = 1;
		const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("TaGSnap.png");
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogTaG, Display, TEXT("SNAP requested %s"), *File);
	}
	else if (SnapStage == 1 && SnapTimer > 11.f)
	{
		SnapStage = 2;
		FPlatformMisc::RequestExit(false);
	}
}
