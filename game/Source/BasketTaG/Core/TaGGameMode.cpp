#include "Core/TaGGameMode.h"
#include "BasketTaG.h"
#include "Ball/TaGBall.h"
#include "Ball/TaGBallSim.h"
#include "Character/TaGArtActors.h"
#include "Character/TaGBotController.h"
#include "Character/TaGCharacter.h"
#include "Character/TaGPlayerController.h"
#include "Core/TaGGameState.h"
#include "Core/TaGPlayerState.h"
#include "Core/TaGRules.h"
#include "Core/TaGStage.h"
#include "Core/TaGTutorial.h"
#include "Core/TaGTypes.h"
#include "UI/TaGHUD.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	float BotSkillValue(int32 Level)
	{
		switch (Level)
		{
		case 0: return 0.25f;
		case 2: return 0.75f;
		case 3: return 0.95f;
		default: return 0.5f;
		}
	}

	FString TeamLabel(int32 Team)
	{
		return Team == 0 ? TEXT("チームA") : TEXT("チームB");
	}
}

ATaGGameMode::ATaGGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	DefaultPawnClass = nullptr; // pawns are spawned by StartMatch
	PlayerControllerClass = ATaGPlayerController::StaticClass();
	GameStateClass = ATaGGameState::StaticClass();
	PlayerStateClass = ATaGPlayerState::StaticClass();
	HUDClass = ATaGHUD::StaticClass();
	bStartPlayersAsSpectators = false;
}

ATaGGameState* ATaGGameMode::GS() const
{
	return GetGameState<ATaGGameState>();
}

ATaGPlayerState* ATaGGameMode::PS(ATaGCharacter* C) const
{
	return C ? C->GetTaGPlayerState() : nullptr;
}

void ATaGGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	bWantLobbyOnStart = UGameplayStatics::HasOption(Options, TEXT("Lobby"));
	bListenServer = UGameplayStatics::HasOption(Options, TEXT("listen"));

	const TCHAR* Cmd = FCommandLine::Get();
	bShotTest = FParse::Param(Cmd, TEXT("TaGShotTest"));
	FString AutoValue;
	if (FParse::Value(Cmd, TEXT("TaGAutoTest="), AutoValue))
	{
		bAutoTest = true;
		AutoTestSize = FMath::Clamp(FCString::Atoi(*AutoValue), 1, 3);
	}
	else if (FParse::Param(Cmd, TEXT("TaGAutoTest")))
	{
		bAutoTest = true;
		AutoTestSize = 1;
	}
	FParse::Value(Cmd, TEXT("TaGSnap="), SnapMode);
}

void ATaGGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bShotTest)
	{
		RunShotTest();
		FPlatformMisc::RequestExit(false);
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Ball = GetWorld()->SpawnActor<ATaGBall>(ATaGBall::StaticClass(), FTransform(FVector(TaG::RimX + 400.f, 0.f, 100.f)), Params);

	ATaGGameState* S = GS();
	if (bAutoTest)
	{
		S->TeamSize = AutoTestSize;
		S->BotSkill = 2;
		StartMatch();
		return;
	}
	if (!SnapMode.IsEmpty())
	{
		FString SizeValue;
		int32 Size = 3;
		if (FParse::Value(FCommandLine::Get(), TEXT("TaGSize="), SizeValue))
		{
			Size = FMath::Clamp(FCString::Atoi(*SizeValue), 1, 3);
		}
		if (SnapMode != TEXT("Title") && SnapMode != TEXT("Settings") && SnapMode != TEXT("Trail") && !SnapMode.StartsWith(TEXT("Tutorial")))
		{
			EnterLobby(Size, 1);
		}
		return; // Play / TPS snaps start the match once the local player exists (TickSnap)
	}
	if (bWantLobbyOnStart)
	{
		int32 Size = UGameplayStatics::GetIntOption(OptionsString, TEXT("TeamSize"), 3);
		int32 Bot = UGameplayStatics::GetIntOption(OptionsString, TEXT("Bot"), 1);
		EnterLobby(FMath::Clamp(Size, 1, 3), FMath::Clamp(Bot, 0, 3));
		S->bOnline = bListenServer;
	}
	else
	{
		S->Phase = ETaGPhase::Title;
	}
}

void ATaGGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (ATaGPlayerState* P = NewPlayer ? NewPlayer->GetPlayerState<ATaGPlayerState>() : nullptr)
	{
		const int32 Index = GameState ? GameState->PlayerArray.Num() : 1;
		P->SetPlayerName(Index <= 1 ? TEXT("ホスト") : FString::Printf(TEXT("プレイヤー%d"), Index));
		P->Hero = ETaGHero::Rei;
		P->Team = -1;
	}
	if (GS() && GS()->Phase == ETaGPhase::Lobby)
	{
		AssignTeams();
	}
}

void ATaGGameMode::Logout(AController* Exiting)
{
	// A friend left mid-match: a bot takes over their player.
	if (APawn* Pawn = Exiting ? Exiting->GetPawn() : nullptr)
	{
		if (ATaGCharacter* C = Cast<ATaGCharacter>(Pawn))
		{
			if (GS() && GS()->Phase == ETaGPhase::Playing)
			{
				Exiting->UnPossess();
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ATaGBotController* AI = GetWorld()->SpawnActor<ATaGBotController>(ATaGBotController::StaticClass(), Params))
				{
					AI->Skill = BotSkillValue(GS()->BotSkill);
					AI->Possess(C);
					if (ATaGPlayerState* P = AI->GetPlayerState<ATaGPlayerState>())
					{
						P->bIsBotPlayer = true;
						P->Team = C->GetTeam();
						P->Hero = C->GetHero();
						P->SetPlayerName(TEXT("BOT"));
					}
					Bots.Add(AI);
				}
			}
		}
	}
	Super::Logout(Exiting);
}

void ATaGGameMode::RestartPlayer(AController* NewPlayer)
{
	// Pawns are spawned by StartMatch with the hero chosen in the lobby.
}

// ================================================================ Lobby

void ATaGGameMode::EnterLobby(int32 TeamSize, int32 BotSkill)
{
	ATaGGameState* S = GS();
	ClearPlayers();
	bLive = false;
	S->Phase = ETaGPhase::Lobby;
	S->bTutorial = false;
	S->TeamSize = FMath::Clamp(TeamSize, 1, 3);
	S->BotSkill = FMath::Clamp(BotSkill, 0, 3);
	S->Score0 = 0;
	S->Score1 = 0;
	S->WinnerTeam = -1;
	AssignTeams();
	if (ATaGStage* Stage = S->GetStage())
	{
		Stage->SetCityLights(0, 0, false);
	}
}

void ATaGGameMode::SetLobbyOptions(int32 TeamSize, int32 BotSkill)
{
	ATaGGameState* S = GS();
	if (S->Phase != ETaGPhase::Lobby)
	{
		return;
	}
	S->TeamSize = FMath::Clamp(TeamSize, 1, 3);
	S->BotSkill = FMath::Clamp(BotSkill, 0, 3);
	AssignTeams();
}

void ATaGGameMode::SetPlayerHero(APlayerController* PC, ETaGHero Hero)
{
	if (ATaGPlayerState* P = PC ? PC->GetPlayerState<ATaGPlayerState>() : nullptr)
	{
		P->Hero = Hero;
		P->bHeroLocked = true;
	}
}

void ATaGGameMode::SetPlayerTeam(APlayerController* PC, int32 Team)
{
	ATaGPlayerState* P = PC ? PC->GetPlayerState<ATaGPlayerState>() : nullptr;
	if (!P || GS()->Phase != ETaGPhase::Lobby || Team < 0 || Team > 1)
	{
		return;
	}
	int32 Count = 0;
	for (APlayerState* Other : GameState->PlayerArray)
	{
		const ATaGPlayerState* O = Cast<ATaGPlayerState>(Other);
		if (O && O != P && !O->bIsBotPlayer && O->Team == Team)
		{
			Count++;
		}
	}
	if (Count < GS()->TeamSize)
	{
		P->Team = Team;
	}
}

void ATaGGameMode::AssignTeams()
{
	// Humans alternate A, B, A, B... (the host is on A). Extra humans watch.
	const int32 Size = GS()->TeamSize;
	int32 Counts[2] = { 0, 0 };
	int32 Next = 0;
	for (APlayerState* P : GameState->PlayerArray)
	{
		ATaGPlayerState* T = Cast<ATaGPlayerState>(P);
		if (!T || T->bIsBotPlayer)
		{
			continue;
		}
		int32 Team = (T->Team == 0 || T->Team == 1) && Counts[T->Team] < Size ? T->Team : -1;
		if (Team < 0)
		{
			Team = Counts[Next] < Size ? Next : (Counts[1 - Next] < Size ? 1 - Next : -1);
		}
		T->Team = Team;
		if (Team >= 0)
		{
			Counts[Team]++;
			Next = Counts[0] <= Counts[1] ? 0 : 1;
		}
	}
}

void ATaGGameMode::ClearPlayers()
{
	for (ATaGCharacter* C : Players)
	{
		if (C)
		{
			if (AController* Ctrl = C->GetController())
			{
				Ctrl->UnPossess();
			}
			C->Destroy();
		}
	}
	Players.Reset();
	for (ATaGBotController* B : Bots)
	{
		if (B)
		{
			B->Destroy();
		}
	}
	Bots.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA(ATaGWallActor::StaticClass()) || It->IsA(ATaGFieldActor::StaticClass()))
		{
			It->Destroy();
		}
	}
	if (Ball)
	{
		Ball->SetDead();
	}
}

ATaGCharacter* ATaGGameMode::SpawnPlayerCharacter(AController* C, ETaGHero Hero, int32 Team, const FVector& Feet)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FRotator Rot(0.f, (TaG::RimCenter() - Feet).Rotation().Yaw, 0.f);
	const FTaGHeroDef& H = TaGHeroes::Get(Hero);
	ATaGCharacter* Ch = GetWorld()->SpawnActor<ATaGCharacter>(ATaGCharacter::StaticClass(), Feet + FVector(0.f, 0.f, H.CapsuleHalfHeight + 5.f), Rot, Params);
	if (!Ch)
	{
		return nullptr;
	}
	Ch->SetupHero(Hero, Team);
	if (C)
	{
		C->Possess(Ch);
		C->SetControlRotation(Rot);
	}
	Players.Add(Ch);
	return Ch;
}

void ATaGGameMode::StartMatch()
{
	ATaGGameState* S = GS();
	ClearPlayers();
	S->Phase = ETaGPhase::Playing;
	S->Score0 = 0;
	S->Score1 = 0;
	S->WinnerTeam = -1;
	S->MatchTime = 0.f;
	S->TeamStats0 = FTaGTeamStats();
	S->TeamStats1 = FTaGTeamStats();
	if (ATaGStage* Stage = S->GetStage())
	{
		Stage->SetCityLights(0, 0, false);
	}
	const int32 Size = S->TeamSize;
	if (!bAutoTest)
	{
		AssignTeams();
	}

	for (int32 Team = 0; Team < 2; ++Team)
	{
		TArray<ETaGHero> Used;
		int32 Slot = 0;
		// Humans first
		for (APlayerState* P : GameState->PlayerArray)
		{
			ATaGPlayerState* T = Cast<ATaGPlayerState>(P);
			if (!T || T->bIsBotPlayer || T->Team != Team || Slot >= Size)
			{
				continue;
			}
			APlayerController* PC = Cast<APlayerController>(T->GetOwner());
			if (!PC)
			{
				continue;
			}
			T->ResetStats();
			SpawnPlayerCharacter(PC, T->Hero, Team, TaG::OffenseSpot(Slot) + FVector(Team == 0 ? 0.f : -150.f, 0.f, 0.f));
			Used.Add(T->Hero);
			Slot++;
		}
		// Bots fill the empty slots
		for (; Slot < Size; ++Slot)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ATaGBotController* AI = GetWorld()->SpawnActor<ATaGBotController>(ATaGBotController::StaticClass(), Params);
			if (!AI)
			{
				continue;
			}
			AI->Skill = BotSkillValue(S->BotSkill);
			// The tutorial bot is Minamo: none of her arts replace the plain hands the lessons teach.
			const ETaGHero Hero = S->bTutorial ? ETaGHero::Minamo : TaGHeroes::Random(Used);
			Used.Add(Hero);
			if (ATaGPlayerState* P = AI->GetPlayerState<ATaGPlayerState>())
			{
				P->bIsBotPlayer = true;
				P->Team = Team;
				P->Hero = Hero;
				P->SetPlayerName(TEXT("BOT"));
				P->ResetStats();
			}
			SpawnPlayerCharacter(AI, Hero, Team, TaG::OffenseSpot(Slot) + FVector(Team == 0 ? 0.f : -150.f, 0.f, 0.f));
			Bots.Add(AI);
		}
	}
	UE_LOG(LogTaG, Log, TEXT("Match start: %don%d, %d characters"), Size, Size, Players.Num());
	ScheduleTurnover(0, 1.5f);
}

void ATaGGameMode::RestartMatch()
{
	StartMatch();
}

void ATaGGameMode::BackToLobby()
{
	EnterLobby(GS()->TeamSize, GS()->BotSkill);
}

// ================================================================ Queries

bool ATaGGameMode::NeedsClear(int32 Team) const
{
	return GS() && GS()->NeedsClear(Team);
}

float ATaGGameMode::GetShotClockLeft() const
{
	return GS() ? GS()->ShotClock : TaG::ShotClock;
}

TArray<ATaGCharacter*> ATaGGameMode::GetTeamPlayers(int32 Team) const
{
	TArray<ATaGCharacter*> Out;
	for (ATaGCharacter* C : Players)
	{
		if (C && C->GetTeam() == Team)
		{
			Out.Add(C);
		}
	}
	return Out;
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
	if (bAutoTest)
	{
		UE_LOG(LogTaG, Verbose, TEXT("[%6.1f] %s"), GS() ? GS()->MatchTime : 0.f, *Text);
		return;
	}
	if (ATaGGameState* S = GS())
	{
		S->MulticastPopup(FText::FromString(Text), Team);
	}
}

void ATaGGameMode::Sfx(FName Sound, const FVector& Where, float Volume)
{
	if (!bAutoTest && GS())
	{
		GS()->MulticastSfx(Sound, Where, Volume);
	}
}

// ================================================================ Flow

void ATaGGameMode::ScheduleTurnover(int32 ToTeam, float Delay)
{
	bLive = false;
	bShotInFlight = false;
	if (GS() && GS()->bTutorial)
	{
		ToTeam = TutorialAttackTeam(); // every lesson restarts with the side that practices
	}
	NextPossessionTeam = ToTeam;
	DeadTimeLeft = Delay;
	bRestartAfterDead = true;
	if (Ball)
	{
		Ball->SetDead();
	}
}

void ATaGGameMode::StartPossession(int32 Team)
{
	ATaGGameState* S = GS();
	TArray<ATaGCharacter*> Off = GetTeamPlayers(Team);
	TArray<ATaGCharacter*> Def = GetTeamPlayers(1 - Team);
	if (!S || !Ball || Off.Num() == 0)
	{
		DeadTimeLeft = 0.5f;
		bRestartAfterDead = true;
		return;
	}
	// The handler: a human if there is one (so you get the ball), otherwise the first player.
	Off.Sort([](const ATaGCharacter& A, const ATaGCharacter& B) { return A.IsPlayerControlled() && !B.IsPlayerControlled(); });
	TArray<int32> Spots = { 0, 1, 2 };
	if (Off.Num() == 2 && FMath::RandBool())
	{
		Spots[1] = 2;
	}
	for (int32 i = 0; i < Off.Num(); ++i)
	{
		Off[i]->ClearForNewPossession(TaG::OffenseSpot(Spots[i]), TaG::RimCenter());
	}
	for (int32 i = 0; i < Def.Num(); ++i)
	{
		const FVector Mark = TaG::OffenseSpot(Spots[FMath::Min(i, Off.Num() - 1)]);
		const FVector ToRim = (TaG::RimFloor() - Mark).GetSafeNormal2D();
		Def[i]->ClearForNewPossession(Mark + ToRim * 170.f, Mark);
	}
	Ball->GiveTo(Off[0]);
	Ball->SetChain(0);
	S->PossessionTeam = Team;
	S->ShotClock = TaG::ShotClock;
	SetNeedsClear(0, false);
	SetNeedsClear(1, false);
	S->Stats(Team).Possessions++;
	PossessionPasses = 0;
	LastPasser = nullptr;
	LastCatcher = nullptr;
	bLive = true;
	bShotInFlight = false;
	Sfx(TEXT("Whistle"), TaG::RimCenter(), 0.4f);
}

void ATaGGameMode::EndPossession(int32 Team, int32 Points)
{
	ATaGGameState* S = GS();
	if (!S || Team < 0)
	{
		return;
	}
	FTaGTeamStats& T = S->Stats(Team);
	if (PossessionPasses > 0)
	{
		T.TeamPossessions++;
		T.TeamPoints += Points;
	}
	else
	{
		T.IsoPossessions++;
		T.IsoPoints += Points;
	}
	PossessionPasses = 0;
}

void ATaGGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickSlowmo();
	ATaGGameState* S = GS();
	if (!S || bShotTest)
	{
		return;
	}
	if (!SnapMode.IsEmpty())
	{
		TickSnap(DeltaSeconds);
	}
	// -TaGTutorialTest: the tutorial from the title (the player controller answers the lessons)
	if (FParse::Param(FCommandLine::Get(), TEXT("TaGTutorialTest")) && S->Phase == ETaGPhase::Title && SnapTimer < 100.f)
	{
		SnapTimer += DeltaSeconds;
		if (SnapTimer > 1.f)
		{
			SnapTimer = 100.f;
			StartTutorial(GetWorld()->GetFirstPlayerController());
		}
	}
	// -TaGInputTest: a 1on1 against a practice bot with the hero from -TaGHero
	if (FParse::Param(FCommandLine::Get(), TEXT("TaGInputTest")) && S->Phase != ETaGPhase::Playing && SnapTimer < 100.f)
	{
		SnapTimer += DeltaSeconds;
		if (S->Phase == ETaGPhase::Title)
		{
			EnterLobby(1, 0);
		}
		else if (S->Phase == ETaGPhase::Lobby && SnapTimer > 1.f)
		{
			FString HeroKey;
			if (FParse::Value(FCommandLine::Get(), TEXT("TaGHero="), HeroKey))
			{
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
				{
					SetPlayerHero(PC, TaGHeroes::FromKey(HeroKey));
				}
			}
			StartMatch();
			SnapTimer = 100.f;
		}
	}

	// -TaGNetHost: network smoke test. Start once a client joined, stop after a while.
	if (FParse::Param(FCommandLine::Get(), TEXT("TaGNetHost")))
	{
		int32 Humans = 0;
		for (APlayerState* P : GameState->PlayerArray)
		{
			const ATaGPlayerState* T = Cast<ATaGPlayerState>(P);
			Humans += (T && !T->bIsBotPlayer) ? 1 : 0;
		}
		if (S->Phase == ETaGPhase::Lobby && Humans >= 2)
		{
			UE_LOG(LogTaG, Display, TEXT("NETTEST host: %d humans, starting"), Humans);
			StartMatch();
		}
		if (S->Phase == ETaGPhase::Playing && S->MatchTime > 45.f)
		{
			UE_LOG(LogTaG, Display, TEXT("NETTEST host done: score %d-%d"), S->Score0, S->Score1);
			LogStats();
			FPlatformMisc::RequestExit(false);
		}
	}

	if (S->Phase != ETaGPhase::Playing)
	{
		return;
	}
	S->MatchTime += DeltaSeconds;
	if (S->bTutorial)
	{
		TickTutorial(DeltaSeconds);
	}

	if (!bLive)
	{
		if (bRestartAfterDead)
		{
			DeadTimeLeft -= DeltaSeconds;
			if (DeadTimeLeft <= 0.f)
			{
				bRestartAfterDead = false;
				StartPossession(NextPossessionTeam);
			}
		}
		return;
	}

	if (bAutoTest && S->MatchTime > 1500.f)
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
			S->Stats(Team).ShotClockViolations++;
			S->Stats(Team).Turnovers++;
			EndPossession(Team, 0);
			Popup(TEXT("ショットクロック・バイオレーション"), 1 - Team);
			Sfx(TEXT("Buzzer"), TaG::RimCenter(), 0.8f);
			ScheduleTurnover(1 - Team, 1.2f);
			return;
		}
	}

	// 3x3 clear rule
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

void ATaGGameMode::TickSlowmo()
{
	if (HitstopUntil <= 0.0)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	AWorldSettings* WS = GetWorldSettings();
	if (!WS)
	{
		return;
	}
	if (Now < HitstopUntil)
	{
		WS->SetTimeDilation(0.05f);
	}
	else if (Now < SlowmoUntil)
	{
		WS->SetTimeDilation(0.35f);
	}
	else
	{
		WS->SetTimeDilation(1.f);
		HitstopUntil = 0.0;
	}
}

// ================================================================ Rock-paper-scissors

void ATaGGameMode::ReadWin(ATaGCharacter* Winner, ATaGCharacter* Loser, const FString& Rule)
{
	if (!Winner)
	{
		return;
	}
	Winner->GiveZone();
	ATaGGameState* S = GS();
	S->Stats(Winner->GetTeam()).ReadWins++;
	if (ATaGPlayerState* P = PS(Winner)) { P->Stats.ReadWins++; }
	if (ATaGPlayerState* P = PS(Loser)) { P->Stats.ReadLosses++; }
	if (S->bTutorial)
	{
		TutorialOnReadWin(Winner, Rule);
	}
	if (bAutoTest)
	{
		UE_LOG(LogTaG, Verbose, TEXT("READ %s by team %d"), *Rule, Winner->GetTeam());
		return;
	}
	S->MulticastReadWin(Winner->GetTeam(), Winner, Loser, FText::FromString(Rule));
	const double Now = FPlatformTime::Seconds();
	HitstopUntil = Now + 0.09;
	SlowmoUntil = Now + 0.09 + 0.38;
}

void ATaGGameMode::ResolveTackle(ATaGCharacter* T, ATaGCharacter* O, bool bCharge)
{
	if (!T || !O || !bLive)
	{
		return;
	}
	const FVector Fwd = T->GetActorForwardVector();
	const float WeightRatio = T->GetHeroDef().Weight / FMath::Max(O->GetHeroDef().Weight, 0.3f);
	Sfx(TEXT("Tackle"), O->GetActorLocation(), 1.f);

	// Evade > tackle: the tackler is stuck for a long time
	if (O->IsDodging())
	{
		O->MarkEvaded();
		T->ForceRecover(O->UsedAfterimage() ? 1.4f : (bCharge ? 1.2f : 1.f));
		ReadWin(O, T, bCharge ? TEXT("かわし ＞ ショルダーチャージ") : TEXT("かわし ＞ タックル"));
		return;
	}
	if (O->IsDown())
	{
		return;
	}
	if (!bCharge)
	{
		T->ForceRecover(0.3f);
	}
	// Shot vs tackle: a draw, the shot goes up with contact
	if (O->IsChargingShot() && O->GetDriveCancelTime() <= 0.f)
	{
		O->ForceContactShot();
		Popup(TEXT("当たられながら打った（あいこ）"), -1);
		return;
	}
	const bool bHadBall = O->HasBall();
	const bool bWasDrive = O->IsDriving();
	if (!bWasDrive)
	{
		// A tackle on someone who is not driving only knocks them off balance (the tackle is for drives).
		const float Push = (bCharge ? 260.f : 250.f) * FMath::Clamp(WeightRatio, 0.4f, 2.5f);
		O->Stagger(bCharge ? 0.8f : 0.35f, Fwd * Push);
		O->ApplyGuardDamage(TaG::ContactGuardDamage * FMath::Clamp(WeightRatio, 0.5f, 2.f) * (bCharge ? 1.5f : 1.f));
		Popup(bCharge ? TEXT("押し返した！") : TEXT("当たっただけ（タックルは突破に刺す技）"), bCharge ? T->GetTeam() : -1);
		return;
	}
	// Tackle > drive: down, and the ball stays with the tackler
	O->KnockDown(Fwd);
	if (bHadBall)
	{
		GS()->Stats(O->GetTeam()).Turnovers++;
		Ball->GiveTo(T);
		OnBallPickedUp(T, false);
	}
	ReadWin(T, O, bCharge ? TEXT("ショルダーチャージ ＞ 突破") : TEXT("タックル ＞ 突破"));
}

void ATaGGameMode::OnDriveBeatsContest(ATaGCharacter* D, ATaGCharacter* O)
{
	if (!D || !O)
	{
		return;
	}
	O->Stagger(1.2f);
	ReadWin(D, O, O->IsSonicContest() ? TEXT("突破 ＞ 音波") : TEXT("突破 ＞ シュート妨害"));
}

void ATaGGameMode::OnDriveEnd(ATaGCharacter* D)
{
	if (D && GS() && GS()->bTutorial)
	{
		const int32 Step = GS()->TutorialStep;
		if (D->IsPlayerControlled() && D->HadBallOnDrive() && Step == TaGTutorial::Drive)
		{
			TutorialProgress(1);
			if (TutorialAdvanceT <= 0.f)
			{
				TutorialResetT = 1.f; // back to the top for the next try (no gather-and-shoot needed)
			}
		}
		else if (D->IsPlayerControlled() && D->HadBallOnDrive() && Step == TaGTutorial::DriveVsContest)
		{
			TutorialRetry(TEXT("相手が手を上げて赤く光っている間に突破しよう"), 1.f);
		}
		else if (!D->IsPlayerControlled() && Step == TaGTutorial::TackleVsDrive)
		{
			TutorialRetry(TEXT("抜かれた！ 水色に光ったらすぐタックル"));
		}
	}
	if (!D || !D->HadBallOnDrive() || !D->HasBall())
	{
		return;
	}
	ATaGCharacter* O = D->FindNearestOpponent(400.f);
	if (!O)
	{
		return;
	}
	const float DRim = TaG::DistToRim(D->GetFeetLocation());
	const float ORim = TaG::DistToRim(O->GetFeetLocation());
	const float Dist = FVector::Dist2D(D->GetActorLocation(), O->GetActorLocation());
	if (Dist < 200.f && ORim < DRim)
	{
		Popup(TEXT("止められた（あいこ）"), -1);
	}
	else if (ORim > DRim + 40.f)
	{
		Popup(TEXT("抜いた！"), D->GetTeam());
	}
}

void ATaGGameMode::OnDodgeWhiff(ATaGCharacter* D)
{
	// Spacing > evade: a defender who stayed patient takes the ball from the whiffed spin.
	if (D && GS() && GS()->bTutorial && D->IsPlayerControlled() && GS()->TutorialStep == TaGTutorial::DodgeVsTackle)
	{
		D->ResetHandCooldowns(); // the lesson needs the next dodge ready for the tackle
		Popup(TEXT("オレンジに光ってからかわそう"), -1);
	}
	if (!D || !D->HasBall())
	{
		return;
	}
	ATaGCharacter* O = D->FindNearestOpponent(400.f);
	if (!O)
	{
		return;
	}
	const float Dist = FVector::Dist2D(D->GetActorLocation(), O->GetActorLocation());
	const bool bWaiting = O->GetAction() == ETaGAction::None || O->GetAction() == ETaGAction::Step;
	if (Dist < TaG::SpacingReadRange && bWaiting)
	{
		ReadWin(O, D, TEXT("間合い ＞ かわし"));
		GS()->Stats(O->GetTeam()).Steals++;
		GS()->Stats(D->GetTeam()).Turnovers++;
		if (ATaGPlayerState* P = PS(O)) { P->Stats.Steals++; }
		Ball->GiveTo(O);
		OnBallPickedUp(O, false);
	}
	else if (Dist < 300.f)
	{
		Popup(TEXT("かわしが空振り（相手は遠かった）"), -1);
	}
}

bool ATaGGameMode::ResolveSteal(ATaGCharacter* S)
{
	if (!bLive || !Ball || !S || Ball->GetBallState() != ETaGBallState::Held)
	{
		return false;
	}
	ATaGCharacter* H = Ball->GetHolder();
	if (!H || H->GetTeam() == S->GetTeam())
	{
		return false;
	}
	FVector To = H->GetActorLocation() - S->GetActorLocation();
	To.Z = 0.f;
	FVector Look = S->GetAimDirection();
	Look.Z = 0.f;
	const float Range = TaG::StealRange * S->GetHeroDef().ReachMul;
	if (To.Size() > Range || FVector::DotProduct(To.GetSafeNormal(), Look.GetSafeNormal()) < 0.7f)
	{
		return false;
	}
	float Chance = 0.15f;
	if (H->GetAction() == ETaGAction::DodgeRecover)
	{
		Chance = 1.f; // the spin whiffed: the ball is wide open
	}
	else if (H->IsChargingShot() || H->IsDodging())
	{
		Chance = 0.f;
	}
	// Small handlers dribble low: big defenders have trouble reaching (ROSTER.md section 2)
	if (S->GetHeroDef().Size == ETaGSize::XL && H->GetHeroDef().Size == ETaGSize::S)
	{
		Chance *= 0.5f;
	}
	if (FMath::FRand() >= Chance)
	{
		return false;
	}
	GS()->Stats(S->GetTeam()).Steals++;
	GS()->Stats(H->GetTeam()).Turnovers++;
	if (ATaGPlayerState* P = PS(S)) { P->Stats.Steals++; }
	Popup(FString::Printf(TEXT("スティール！ %s"), S->GetHeroDef().Name), S->GetTeam());
	Sfx(TEXT("Steal"), H->GetActorLocation(), 1.f);
	Ball->GiveTo(S);
	OnBallPickedUp(S, false);
	return true;
}

void ATaGGameMode::OnPullHit(ATaGCharacter* M, ATaGCharacter* O)
{
	if (!M || !O)
	{
		return;
	}
	if (O->IsDodging())
	{
		O->MarkEvaded();
		M->ForceRecover(1.f);
		ReadWin(O, M, TEXT("かわし ＞ 引き寄せ"));
		return;
	}
	const FVector ToM = (M->GetActorLocation() - O->GetActorLocation()).GetSafeNormal2D();
	if (O->IsDriving())
	{
		const bool bHadBall = O->HasBall();
		O->KnockDown(ToM);
		if (bHadBall)
		{
			GS()->Stats(O->GetTeam()).Turnovers++;
			Ball->MakeLoose(Ball->GetActorLocation(), ToM * 300.f + FVector(0.f, 0.f, 250.f), M->GetTeam());
		}
		ReadWin(M, O, TEXT("引き寄せ ＞ 突破"));
		return;
	}
	O->Stagger(0.5f, ToM * 700.f + FVector(0.f, 0.f, 120.f));
	O->ApplyGuardDamage(25.f);
	Popup(FString::Printf(TEXT("引き寄せ！ %s"), M->GetHeroDef().Name), M->GetTeam());
}

void ATaGGameMode::OnGuardBreak(ATaGCharacter* V)
{
	if (!V)
	{
		return;
	}
	Popup(TEXT("ガードブレイク！"), 1 - V->GetTeam());
	if (Ball && Ball->GetHolder() == V)
	{
		GS()->Stats(V->GetTeam()).Turnovers++;
		Ball->MakeLoose(Ball->GetActorLocation(), FVector(0.f, 0.f, 250.f), 1 - V->GetTeam());
	}
}

void ATaGGameMode::OnArtUsed(ATaGCharacter* U, ETaGArt Art)
{
	if (!U)
	{
		return;
	}
	GS()->Stats(U->GetTeam()).ArtsUsed++;
	const FTaGArtDef& A = TaGHeroes::GetArt(Art);
	Popup(FString::Printf(TEXT("%s：%s〔%s〕"), U->GetHeroDef().Name, A.Name, TaGHeroes::HandName(A.Hand)), U->GetTeam());
	if (GS()->bTutorial && U->IsPlayerControlled() && GS()->TutorialStep == TaGTutorial::Art)
	{
		TutorialProgress(1);
	}
}

// ================================================================ Ball events

void ATaGGameMode::ResolveShot(ATaGCharacter* Shooter, float Charge, bool bContact)
{
	if (!bLive || !Ball || !Shooter || Ball->GetHolder() != Shooter)
	{
		return;
	}
	ATaGGameState* S = GS();
	const int32 Team = Shooter->GetTeam();
	const FTaGShotEval E = FTaGRules::EvaluateShot(Shooter, Charge, bContact);
	const FTaGShotTypeDef& St = FTaGRules::ShotType(E.Type);

	if (E.bContested && E.Contester.IsValid())
	{
		ReadWin(E.Contester.Get(), Shooter, E.Contester->IsSonicContest() ? TEXT("音波 ＞ シュート") : TEXT("シュート妨害 ＞ シュート"));
	}
	else if (E.bWalled)
	{
		Popup(TEXT("鉄の壁に阻まれた"), 1 - Team);
	}
	else if (E.bOpen)
	{
		Popup(TEXT("ノーマーク！  シュート ＞ 間合い"), Team);
	}
	if (E.Type != ETaGShotType::Jumper)
	{
		Popup(FString::Printf(TEXT("%s！"), St.Name), Team);
	}
	if (E.bPerfect)
	{
		S->Stats(Team).Perfects++;
		Popup(TEXT("パーフェクト・リリース！"), -1);
	}
	if (S->bTutorial)
	{
		if (Shooter->IsPlayerControlled())
		{
			if (S->TutorialStep == TaGTutorial::Shoot)
			{
				TutorialProgress(1);
			}
			else if (S->TutorialStep == TaGTutorial::Perfect)
			{
				TutorialShots++;
				if (E.bPerfect)
				{
					TutorialProgress(1);
				}
				else if (TutorialShots >= 5)
				{
					Popup(TEXT("タイミングはつかめた。次へ進もう"), -1);
					TutorialProgress(S->TutorialGoal);
				}
			}
		}
		else if (S->TutorialStep == TaGTutorial::ContestVsShot && !E.bContested)
		{
			TutorialRetry(TEXT("打たれた。金色に光ったらシュート妨害を押し続けよう"), 2.2f);
		}
	}

	const bool bMake = FMath::FRand() < E.Chance;
	const bool bValid = !NeedsClear(Team);
	const FVector Feet = Shooter->GetFeetLocation();
	const float Release = Shooter->GetHeroDef().EyeHeight + 60.f;
	FVector From = FTaGBallSim::ReleasePoint(Feet, Release);
	FVector Target = TaG::RimCenter() + FVector(0.f, 0.f, 2.f);
	if (!bMake)
	{
		const float A = FMath::FRandRange(0.f, 2.f * UE_PI);
		Target += FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::FRandRange(20.f, 32.f);
	}
	float Angle = FTaGBallSim::ShotAngleForDistance(E.Distance, From.Z);
	if (St.Angle > 0.f && E.Distance >= 120.f)
	{
		Angle = FMath::Max(Angle, St.Angle);
	}
	FVector Vel = FTaGBallSim::SolveLaunch(From, Target, Angle);
	if (E.Type == ETaGShotType::Dunk && E.Distance < 220.f)
	{
		// Dunk: hammer it down through the rim
		FVector Away(Feet.X - TaG::RimX, Feet.Y, 0.f);
		Away = Away.Size() > 5.f ? Away.GetSafeNormal() : FVector::ForwardVector;
		From = TaG::RimCenter() + Away * 30.f + FVector(0.f, 0.f, 32.f);
		const FVector Aim = bMake ? TaG::RimCenter() - FVector(0.f, 0.f, 10.f) : TaG::RimCenter() + Away * 24.f + FVector(0.f, 0.f, 2.f);
		Vel = (Aim - From) / 0.14f;
		// A defender in the air under the rim can stuff it
		for (ATaGCharacter* D : GetTeamPlayers(1 - Team))
		{
			const bool bAir = D->GetCharacterMovement() && D->GetCharacterMovement()->IsFalling();
			if (bAir && TaG::DistToRim(D->GetFeetLocation()) < 150.f)
			{
				const ATaGBotController* AI = Cast<ATaGBotController>(D->GetController());
				const float Chance = AI ? 0.3f + 0.4f * AI->Skill : 1.f;
				if (FMath::FRand() < Chance)
				{
					Ball->LaunchShot(From, Vel, Shooter, E.Points, bValid, St.Block);
					Ball->MakeLoose(From, Away * 400.f + FVector(0.f, 0.f, 200.f), D->GetTeam());
					S->Stats(Team).Shots++;
					S->Stats(Team).ExpectedMakes += E.Chance;
					OnBlocked(D);
					Popup(TEXT("ダンクをブロック！"), D->GetTeam());
					return;
				}
			}
		}
	}
	// Tall release (Hayate) is harder to block
	const float BlockMult = St.Block * FMath::Clamp(175.f / Shooter->GetHeroDef().EyeHeight, 0.6f, 1.3f);
	Ball->LaunchShot(From, Vel, Shooter, E.Points, bValid, BlockMult);
	bShotInFlight = true;
	bLastShotTeamPlay = PossessionPasses > 0;
	S->Stats(Team).Shots++;
	S->Stats(Team).ExpectedMakes += E.Chance;
	if (ATaGPlayerState* P = PS(Shooter)) { P->Stats.Shots++; }
	Shooter->ConsumeZone();
	UE_LOG(LogTaG, Verbose, TEXT("Shot team=%d type=%d dist=%.0f pts=%d charge=%.2f p=%.2f make=%d"), Team, static_cast<int32>(E.Type), E.Distance, E.Points, Charge, E.Chance, bMake ? 1 : 0);
}

void ATaGGameMode::LaunchPass(ATaGCharacter* Passer, ATaGCharacter* Receiver, bool bThread)
{
	if (!Ball || !Passer || !Receiver || Ball->GetHolder() != Passer)
	{
		return;
	}
	Ball->LaunchPass(Passer, Receiver, bThread);
	LastPasser = Passer;
	GS()->Stats(Passer->GetTeam()).Passes++;
	if (bThread)
	{
		Passer->MulticastFx(5, Passer->GetActorLocation(), Receiver->GetActorLocation(), Receiver);
	}
}

void ATaGGameMode::OnPassCaught(ATaGCharacter* Receiver)
{
	if (!Receiver)
	{
		return;
	}
	PossessionPasses++;
	LastCatcher = Receiver;
	LastCatchTime = GetWorld()->GetTimeSeconds();
	Ball->SetChain(FMath::Min(PossessionPasses, 5));
	Receiver->OnCaughtPass();
}

void ATaGGameMode::OnPassIntercepted(ATaGCharacter* D)
{
	if (!D)
	{
		return;
	}
	ATaGGameState* S = GS();
	S->Stats(D->GetTeam()).Interceptions++;
	S->Stats(D->GetTeam()).Steals++;
	S->Stats(1 - D->GetTeam()).Turnovers++;
	if (ATaGPlayerState* P = PS(D)) { P->Stats.Steals++; }
	Popup(FString::Printf(TEXT("パスカット！ %s"), D->GetHeroDef().Name), D->GetTeam());
	OnBallPickedUp(D, false);
}

void ATaGGameMode::OnBasket(ATaGCharacter* Shooter, int32 Points, bool bValid)
{
	ATaGGameState* S = GS();
	const int32 Team = Shooter ? Shooter->GetTeam() : S->PossessionTeam;
	bShotInFlight = false;
	if (!bValid)
	{
		EndPossession(Team, 0);
		Popup(TEXT("クリア前のシュートは無効"), 1 - Team);
		ScheduleTurnover(1 - Team, 1.2f);
		return;
	}
	(Team == 0 ? S->Score0 : S->Score1) += Points;
	S->Stats(Team).Points += Points;
	S->Stats(Team).Makes++;
	if (ATaGPlayerState* P = PS(Shooter))
	{
		P->Stats.Points += Points;
		P->Stats.Makes++;
	}
	// Assist: the scorer caught a pass in the last 4 seconds
	if (Shooter && LastCatcher.Get() == Shooter && LastPasser.IsValid() && LastPasser->GetTeam() == Team
		&& GetWorld()->GetTimeSeconds() - LastCatchTime < 4.f)
	{
		if (ATaGPlayerState* P = PS(LastPasser.Get())) { P->Stats.Assists++; }
		Popup(FString::Printf(TEXT("アシスト %s"), LastPasser->GetHeroDef().Name), Team);
	}
	EndPossession(Team, Points);
	Popup(FString::Printf(TEXT("%s +%d  %s"), Shooter ? Shooter->GetHeroDef().Name : TEXT(""), Points, Points == 2 ? TEXT("（2点）") : TEXT("")), Team);
	if (!bAutoTest)
	{
		S->MulticastBasket(Team, Points);
	}
	if (S->GetScore(Team) >= TaG::TargetScore && !S->bTutorial)
	{
		EndMatch(Team);
		return;
	}
	ScheduleTurnover(1 - Team, 1.4f);
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
		GS()->Stats(Blocker->GetTeam()).Blocks++;
		if (ATaGPlayerState* P = PS(Blocker)) { P->Stats.Blocks++; }
		Popup(FString::Printf(TEXT("ブロック！ %s"), Blocker->GetHeroDef().Name), Blocker->GetTeam());
	}
}

void ATaGGameMode::OnBallPickedUp(ATaGCharacter* Picker, bool bFromPass)
{
	ATaGGameState* S = GS();
	if (!S || !Picker)
	{
		return;
	}
	const int32 Team = Picker->GetTeam();
	if (ATaGPlayerState* P = PS(Picker)) { P->Stats.Rebounds++; }
	if (Team != S->PossessionTeam)
	{
		// Defensive rebound, steal or interception: a new possession that must clear first.
		EndPossession(S->PossessionTeam, 0);
		S->PossessionTeam = Team;
		S->ShotClock = TaG::ShotClock;
		S->Stats(Team).Possessions++;
		PossessionPasses = 0;
		LastPasser = nullptr;
		LastCatcher = nullptr;
		Ball->SetChain(0);
		const bool bInside = !S->bTutorial && !TaG::IsBeyondArc(Picker->GetFeetLocation());
		SetNeedsClear(Team, bInside);
		SetNeedsClear(1 - Team, false);
		if (bInside)
		{
			Popup(TEXT("アークの外へクリア！"), Team);
		}
	}
}

void ATaGGameMode::OnOutOfBounds(int32 LastTouchTeam)
{
	const int32 To = LastTouchTeam == 0 ? 1 : 0;
	const int32 Possession = GS()->PossessionTeam;
	if (Possession != To)
	{
		EndPossession(Possession, 0);
		GS()->Stats(Possession).Turnovers++;
	}
	Popup(TEXT("アウト・オブ・バウンズ"), To);
	Sfx(TEXT("Whistle"), TaG::RimCenter(), 0.7f);
	ScheduleTurnover(To, 1.0f);
}

void ATaGGameMode::OnStuckBall(int32 LastTouchTeam)
{
	const int32 To = LastTouchTeam == 0 ? 1 : 0;
	EndPossession(GS()->PossessionTeam, 0);
	ScheduleTurnover(To, 0.5f);
}

void ATaGGameMode::EndMatch(int32 Winner)
{
	ATaGGameState* S = GS();
	bLive = false;
	bRestartAfterDead = false;
	if (Ball)
	{
		Ball->SetDead();
	}
	S->WinnerTeam = Winner;
	S->Phase = ETaGPhase::Results;
	Sfx(TEXT("Buzzer"), TaG::RimCenter(), 1.f);
	LogStats();
	if (bAutoTest)
	{
		FPlatformMisc::RequestExit(false);
	}
}

void ATaGGameMode::LogStats() const
{
	const ATaGGameState* S = GS();
	if (!S)
	{
		return;
	}
	for (int32 T = 0; T < 2; ++T)
	{
		const FTaGTeamStats& St = S->Stats(T);
		const float PPP = St.Possessions > 0 ? float(St.Points) / St.Possessions : 0.f;
		const float Iso = St.IsoPossessions > 0 ? float(St.IsoPoints) / St.IsoPossessions : 0.f;
		const float TeamPlay = St.TeamPossessions > 0 ? float(St.TeamPoints) / St.TeamPossessions : 0.f;
		const float FG = St.Shots > 0 ? float(St.Makes) / St.Shots : 0.f;
		const float XFG = St.Shots > 0 ? St.ExpectedMakes / St.Shots : 0.f;
		UE_LOG(LogTaG, Display, TEXT("STATS team=%d score=%d poss=%d ppp=%.2f iso_ppp=%.2f(%d) team_ppp=%.2f(%d) shots=%d fg=%.2f xfg=%.2f reads=%d steals=%d blocks=%d intercepts=%d tov=%d passes=%d arts=%d perfect=%d clock=%d"),
			T, S->GetScore(T), St.Possessions, PPP, Iso, St.IsoPossessions, TeamPlay, St.TeamPossessions, St.Shots, FG, XFG,
			St.ReadWins, St.Steals, St.Blocks, St.Interceptions, St.Turnovers, St.Passes, St.ArtsUsed, St.Perfects, St.ShotClockViolations);
	}
	for (ATaGCharacter* C : Players)
	{
		if (const ATaGPlayerState* P = C ? C->GetTaGPlayerState() : nullptr)
		{
			UE_LOG(LogTaG, Display, TEXT("PLAYER team=%d hero=%s pts=%d shots=%d ast=%d stl=%d blk=%d reb=%d reads=%d/%d"),
				C->GetTeam(), C->GetHeroDef().Key, P->Stats.Points, P->Stats.Shots, P->Stats.Assists, P->Stats.Steals, P->Stats.Blocks, P->Stats.Rebounds, P->Stats.ReadWins, P->Stats.ReadLosses);
		}
	}
	UE_LOG(LogTaG, Display, TEXT("STATS match_time=%.1f size=%d"), S->MatchTime, S->TeamSize);
}

// ================================================================ Tutorial

bool ATaGGameMode::IsTutorial() const
{
	return GS() && GS()->bTutorial;
}

void ATaGGameMode::StartTutorial(APlayerController* PC)
{
	ATaGGameState* S = GS();
	if (!S || !PC)
	{
		return;
	}
	EnterLobby(1, 0);
	ATaGPlayerState* Me = PC->GetPlayerState<ATaGPlayerState>();
	for (APlayerState* P : GameState->PlayerArray)
	{
		ATaGPlayerState* T = Cast<ATaGPlayerState>(P);
		if (T && !T->bIsBotPlayer)
		{
			T->Team = T == Me ? 0 : -1; // a 1on1 lesson
		}
	}
	if (Me)
	{
		Me->Hero = ETaGHero::Rei; // the art lesson talks about Rei's arts
	}
	S->bTutorial = true;
	S->TutorialStep = TaGTutorial::Move;
	S->TutorialProgress = 0;
	S->TutorialGoal = TaGTutorial::Get(TaGTutorial::Move).Goal;
	TutorialAdvanceT = 0.f;
	TutorialResetT = 0.f;
	TutorialShots = 0;
	TutorialMoved = 0.f;
	bTutorialHasLastPos = false;
	StartMatch();
	ApplyTutorialScripts();
}

int32 ATaGGameMode::TutorialAttackTeam() const
{
	// The learner is always team A (0); the scripted bot is team B.
	return GS() && !TaGTutorial::Get(GS()->TutorialStep).bPlayerAttacks ? 1 : 0;
}

ATaGCharacter* ATaGGameMode::TutorialPlayer() const
{
	for (ATaGCharacter* C : Players)
	{
		if (C && C->IsPlayerControlled())
		{
			return C;
		}
	}
	return nullptr;
}

void ATaGGameMode::ApplyTutorialScripts()
{
	using EScript = ATaGBotController::EScript;
	EScript Script = EScript::Idle;
	switch (GS()->TutorialStep)
	{
	case TaGTutorial::DriveVsContest: Script = EScript::ContestOnClose; break;
	case TaGTutorial::DodgeVsTackle: Script = EScript::TackleOnClose; break;
	case TaGTutorial::TackleVsDrive: Script = EScript::DriveAttack; break;
	case TaGTutorial::ContestVsShot: Script = EScript::ShootAttack; break;
	default: break;
	}
	for (ATaGBotController* B : Bots)
	{
		if (B)
		{
			B->SetScript(Script);
		}
	}
}

void ATaGGameMode::TickTutorial(float Dt)
{
	ATaGGameState* S = GS();
	S->ShotClock = TaG::ShotClock; // no shot clock while learning
	if (S->TutorialStep >= TaGTutorial::Done)
	{
		return;
	}
	if (TutorialAdvanceT > 0.f)
	{
		TutorialAdvanceT -= Dt;
		if (TutorialAdvanceT <= 0.f)
		{
			TutorialAdvance(false);
		}
		return;
	}
	if (TutorialResetT > 0.f)
	{
		TutorialResetT -= Dt;
		if (TutorialResetT <= 0.f)
		{
			for (ATaGCharacter* C : Players)
			{
				if (C)
				{
					C->ResetHandCooldowns();
				}
			}
			ScheduleTurnover(TutorialAttackTeam(), 0.4f);
		}
		return;
	}
	ATaGCharacter* Me = TutorialPlayer();
	if (!Me || !bLive || !Ball)
	{
		bTutorialHasLastPos = false;
		return;
	}
	// Lesson 1: walk a few meters
	if (S->TutorialStep == TaGTutorial::Move)
	{
		const FVector P = Me->GetActorLocation();
		if (bTutorialHasLastPos)
		{
			TutorialMoved += FVector::Dist2D(P, TutorialLastPos);
		}
		TutorialLastPos = P;
		bTutorialHasLastPos = true;
		const int32 Meters = FMath::Min(FMath::FloorToInt(TutorialMoved / 100.f), S->TutorialGoal);
		if (Meters > S->TutorialProgress)
		{
			TutorialProgress(Meters - S->TutorialProgress);
		}
	}
	// The other side got the ball (rebound, steal, tackle): give it back to the side that practices.
	const int32 Want = TutorialAttackTeam();
	const ATaGCharacter* Holder = Ball->GetHolder();
	if (Ball->GetBallState() == ETaGBallState::Held && Holder && Holder->GetTeam() != Want)
	{
		TutorialResetT = 0.8f;
	}
	else if (Want != Me->GetTeam() && Ball->GetBallState() == ETaGBallState::Loose)
	{
		TutorialResetT = 1.f; // defense lessons: the bot's attempt is over once the ball is loose
	}
}

void ATaGGameMode::TutorialProgress(int32 Amount)
{
	ATaGGameState* S = GS();
	if (!S || !S->bTutorial || TutorialAdvanceT > 0.f || S->TutorialStep >= TaGTutorial::Done)
	{
		return;
	}
	S->TutorialProgress = FMath::Min(S->TutorialProgress + Amount, S->TutorialGoal);
	if (S->TutorialProgress >= S->TutorialGoal)
	{
		TutorialAdvanceT = 1.1f;
		TutorialResetT = 0.f;
		Popup(TEXT("できた！"), 0);
		if (ATaGCharacter* Me = TutorialPlayer())
		{
			Sfx(TEXT("ReadWin"), Me->GetActorLocation(), 0.7f);
		}
	}
}

void ATaGGameMode::TutorialOnReadWin(ATaGCharacter* Winner, const FString& Rule)
{
	if (!Winner || !Winner->IsPlayerControlled())
	{
		return;
	}
	bool bLesson = false;
	switch (GS()->TutorialStep)
	{
	case TaGTutorial::DriveVsContest: bLesson = Rule.StartsWith(TEXT("突破")); break;
	case TaGTutorial::DodgeVsTackle: bLesson = Rule.StartsWith(TEXT("かわし")); break;
	case TaGTutorial::TackleVsDrive: bLesson = Rule.StartsWith(TEXT("タックル")); break;
	case TaGTutorial::ContestVsShot: bLesson = Rule.StartsWith(TEXT("シュート妨害")); break;
	default: break;
	}
	if (bLesson)
	{
		TutorialProgress(1);
	}
}

void ATaGGameMode::TutorialRetry(const FString& Hint, float Delay)
{
	if (!IsTutorial() || TutorialAdvanceT > 0.f || TutorialResetT > 0.f)
	{
		return;
	}
	Popup(Hint, -1);
	TutorialResetT = Delay;
}

void ATaGGameMode::TutorialAdvance(bool bSkip)
{
	ATaGGameState* S = GS();
	if (!S || !S->bTutorial || S->Phase != ETaGPhase::Playing || S->TutorialStep >= TaGTutorial::Done)
	{
		return;
	}
	TutorialAdvanceT = 0.f;
	TutorialResetT = 0.f;
	TutorialShots = 0;
	S->TutorialStep++;
	S->TutorialProgress = 0;
	S->TutorialGoal = TaGTutorial::Get(S->TutorialStep).Goal;
	for (ATaGCharacter* C : Players)
	{
		if (C)
		{
			C->ResetHandCooldowns();
		}
	}
	ApplyTutorialScripts();
	UE_LOG(LogTaG, Display, TEXT("TUTORIAL step=%d %s%s"), S->TutorialStep, TaGTutorial::Get(S->TutorialStep).Title, bSkip ? TEXT(" (skipped)") : TEXT(""));
	if (S->TutorialStep >= TaGTutorial::Done)
	{
		bLive = false;
		bRestartAfterDead = false;
		if (Ball)
		{
			Ball->SetDead();
		}
		Sfx(TEXT("Buzzer"), TaG::RimCenter(), 0.6f);
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			ATaGPlayerController* PC = Cast<ATaGPlayerController>(It->Get());
			if (PC && PC->IsLocalController())
			{
				PC->bTutorialDone = true;
				PC->SaveConfig();
			}
		}
		return;
	}
	ScheduleTurnover(TutorialAttackTeam(), bSkip ? 0.4f : 1.f);
}

// ================================================================ Test modes

void ATaGGameMode::RunShotTest()
{
	// Planned makes must physically drop through the hoop from every distance and angle (like the Godot --shottest).
	const float Distances[] = { 80.f, 150.f, 250.f, 350.f, 450.f, 550.f, 650.f, 700.f, 800.f, 900.f };
	const float Bearings[] = { -80.f, -45.f, 0.f, 45.f, 80.f };
	const float Heights[] = { 205.f, 235.f, 250.f, 310.f };   // S, M, tall M, XL release heights
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
			for (const float Height : Heights)
			{
				for (int32 Jump = 0; Jump < 2; ++Jump)
				{
					Feet.Z = Jump ? 60.f : 0.f;
					for (int32 Kind = 0; Kind < 2; ++Kind)
					{
						const bool bMake = Kind == 0;
						const int32 Trials = bMake ? 1 : 8;
						for (int32 T = 0; T < Trials; ++T)
						{
							const FVector From = FTaGBallSim::ReleasePoint(Feet, Height);
							FVector Target = TaG::RimCenter() + FVector(0.f, 0.f, 2.f);
							if (!bMake)
							{
								const float A = 2.f * UE_PI * T / Trials;
								Target += FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (T % 2 ? 32.f : 20.f);
							}
							FTaGBallSim Sim;
							Sim.Pos = From;
							Sim.Vel = FTaGBallSim::SolveLaunch(From, Target, FTaGBallSim::ShotAngleForDistance(TaG::DistToRim(Feet), From.Z));
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
									UE_LOG(LogTaG, Display, TEXT("SHOTTEST fail dist=%.0f bearing=%.0f height=%.0f jump=%d"), Dist, Bearing, Height, Jump);
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
		}
		UE_LOG(LogTaG, Display, TEXT("SHOTTEST dist=%.1fm planned_makes_in=%d/%d"), Dist / 100.f, RowOk, RowTotal);
	}
	UE_LOG(LogTaG, Display, TEXT("SHOTTEST total planned_makes_in=%d/%d (%.0f%%)  planned_misses_that_went_in=%d/%d (%.0f%%)"),
		MakeOk, MakeTotal, 100.f * MakeOk / FMath::Max(MakeTotal, 1), MissIn, MissTotal, 100.f * MissIn / FMath::Max(MissTotal, 1));
}

void ATaGGameMode::TickSnap(float Dt)
{
	// -TaGSnap=<Title|Lobby|Play|TPS|Results>: take one screenshot after the scene settles, then quit.
	SnapTimer += Dt;
	ATaGGameState* S = GS();
	if (SnapStage == 0 && SnapMode == TEXT("Defense") && SnapTimer > 4.f && SnapTimer - Dt <= 4.f)
	{
		ScheduleTurnover(1, 0.2f); // the bots attack, you defend
	}
	APlayerController* SnapPC = GetWorld()->GetFirstPlayerController();
	if (SnapStage == 0 && SnapMode.StartsWith(TEXT("Tutorial")) && SnapTimer > 2.f && S->Phase == ETaGPhase::Title && SnapPC)
	{
		StartTutorial(SnapPC);
		// -TaGLesson=N shows a later lesson (TutorialDone: the finished screen)
		int32 Lesson = SnapMode == TEXT("TutorialDone") ? TaGTutorial::Done : 0;
		FParse::Value(FCommandLine::Get(), TEXT("TaGLesson="), Lesson);
		for (int32 i = 0; i < Lesson; ++i)
		{
			TutorialAdvance(true);
		}
	}
	// -TaGSnap=Trail: a shot from the top in the shooting lesson, photographed mid-flight (the ball trail)
	if (SnapStage == 0 && SnapMode == TEXT("Trail") && SnapTimer > 2.f && S->Phase == ETaGPhase::Title && SnapPC)
	{
		StartTutorial(SnapPC);
		TutorialAdvance(true);
	}
	if (SnapStage == 0 && SnapMode == TEXT("Trail") && SnapTimer > 6.f && SnapTimer - Dt <= 6.f)
	{
		if (ATaGCharacter* Shooter = TutorialPlayer())
		{
			Shooter->SetThirdPerson(true); // the whole arc in view
			ResolveShot(Shooter, 0.75f, false);
		}
	}
	if (SnapStage == 0 && SnapMode == TEXT("Settings") && SnapTimer > 2.f)
	{
		if (ATaGPlayerController* TPC = Cast<ATaGPlayerController>(SnapPC))
		{
			TPC->SetSettingsOpen(true);
		}
	}
	if (SnapStage == 0 && (SnapMode == TEXT("Play") || SnapMode == TEXT("TPS") || SnapMode == TEXT("Results") || SnapMode == TEXT("Defense")) && SnapTimer > 2.f && S->Phase == ETaGPhase::Lobby)
	{
		FString HeroKey;
		if (FParse::Value(FCommandLine::Get(), TEXT("TaGHero="), HeroKey))
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				SetPlayerHero(PC, TaGHeroes::FromKey(HeroKey));
			}
		}
		StartMatch();
	}
	if (SnapStage == 0 && SnapMode == TEXT("TPS") && SnapTimer > 4.f)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (ATaGCharacter* C = Cast<ATaGCharacter>(PC->GetPawn()))
			{
				C->SetThirdPerson(true);
			}
		}
	}
	if (SnapStage == 0 && SnapMode == TEXT("Results") && SnapTimer > 5.f && S->Phase == ETaGPhase::Playing)
	{
		S->Score0 = 11;
		S->Score1 = 7;
		EndMatch(0);
	}
	float ShotAt = SnapMode == TEXT("Play") || SnapMode == TEXT("TPS") || SnapMode == TEXT("Defense") || SnapMode == TEXT("Tutorial") ? 9.5f : 8.f;
	if (SnapMode == TEXT("Trail"))
	{
		ShotAt = 6.55f;
	}
	if (SnapStage == 0 && SnapTimer > ShotAt)
	{
		SnapStage = 1;
		const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("TaGSnap_%s.png"), *SnapMode);
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogTaG, Display, TEXT("SNAP requested %s"), *File);
	}
	else if (SnapStage == 1 && SnapTimer > ShotAt + 3.f)
	{
		SnapStage = 2;
		FPlatformMisc::RequestExit(false);
	}
}
