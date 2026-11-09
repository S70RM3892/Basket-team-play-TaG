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
#include "Core/TaGReplay.h"
#include "Core/TaGRules.h"
#include "Core/TaGStage.h"
#include "Core/TaGTutorial.h"
#include "Core/TaGTypes.h"
#include "UI/TaGHUD.h"
#include "Character/TaGRig.h"
#include "AIController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
		if (SnapMode != TEXT("Title") && SnapMode != TEXT("Settings") && SnapMode != TEXT("Trail") && SnapMode != TEXT("Gait") && !SnapMode.StartsWith(TEXT("Tutorial")))
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
			ETaGHero Hero = S->bTutorial ? ETaGHero::Minamo : TaGHeroes::Random(Used);
			// Tests: -TaGTeamHeroes=A,B,C,D,E,F puts these heroes on the court in order (team A first)
			FString List;
			if (FParse::Value(FCommandLine::Get(), TEXT("TaGTeamHeroes="), List, false))
			{
				TArray<FString> Keys;
				List.ParseIntoArray(Keys, TEXT(","));
				const int32 Index = Team * Size + Slot;
				if (Keys.IsValidIndex(Index))
				{
					Hero = TaGHeroes::FromKey(Keys[Index], Hero);
				}
			}
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
	// A real match opens on the two lineups; tests and the tutorial start right away
	const bool bIntro = !bAutoTest && !S->bTutorial && !FParse::Param(FCommandLine::Get(), TEXT("TaGInputTest"))
		&& (SnapMode.IsEmpty() || SnapMode == TEXT("Lineup"));
	S->IntroSeconds = bIntro ? 2.3f : 0.f;
	ScheduleTurnover(0, bIntro ? 2.6f : 1.5f);
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
	bHighlightShowing = false;
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

	// A ball handler whose foot touches the line or the floor beyond it is out. A body in the air keeps the status of
	// the spot it jumped from: a player who took off inside may save a ball over the line (and is out on landing
	// there), one who took off outside and catches the ball in the air is out at once. A shooter who took off inside
	// and lands outside is not: the ball left the hand in the air.
	if (Ball && Ball->GetBallState() == ETaGBallState::Held)
	{
		ATaGCharacter* H = Ball->GetHolder();
		if (H && TaG::IsPlayerOutOfBounds(H->GetShotSpot()))
		{
			OnHandlerOutOfBounds(H);
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
		UE_LOG(LogTaG, Display, TEXT("READ %s by team %d"), *Rule, Winner->GetTeam());
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

	// Shell: the tackle bounces off and the tackler is stuck (evade > tackle)
	if (O->IsInvulnerable())
	{
		T->ForceRecover(1.2f);
		ReadWin(O, T, TEXT("殻ごもり ＞ タックル"));
		return;
	}
	// Mist and scattering swarms: it goes straight through (evade > tackle)
	if (O->IsPhasing())
	{
		T->ForceRecover(1.f);
		ReadWin(O, T, TEXT("すり抜け ＞ タックル"));
		return;
	}
	// Counters: Amagi slips aside, Kawara's tile armour throws the tackler back
	const ETaGArtFx Counter = O->ConsumeCounter();
	if (Counter == ETaGArtFx::CounterSlip)
	{
		O->MarkEvaded();
		T->ForceRecover(1.f);
		O->LaunchCharacter(O->GetActorRightVector() * (FMath::RandBool() ? 420.f : -420.f), true, false);
		ReadWin(O, T, TEXT("予測回避 ＞ タックル"));
		return;
	}
	if (Counter == ETaGArtFx::CounterTile)
	{
		T->Stagger(0.8f, -Fwd * 300.f);
		ReadWin(O, T, TEXT("瓦鎧 ＞ タックル"));
		return;
	}
	// A rooted giant does not move: the tackler bounces off
	if (O->IsRootedWall())
	{
		T->Stagger(0.6f, -Fwd * 260.f);
		Popup(FString::Printf(TEXT("%s は根を張っていて動かない"), O->GetHeroDef().Name), O->GetTeam());
		return;
	}

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
		// Keibi's arrest pins the player in place instead of a shove
		if (T->GetTackleArt() == ETaGArt::Arrest)
		{
			O->ApplyArtStatus(ETaGArtFx::Root, 1.f, 0.f);
			O->ApplyGuardDamage(TaG::ContactGuardDamage * 0.5f);
			Popup(FString::Printf(TEXT("確保！ %s が止まった"), O->GetHeroDef().Name), T->GetTeam());
			return;
		}
		// A tackle on someone who is not driving only knocks them off balance (the tackle is for drives).
		const bool bBash = T->GetTackleArt() == ETaGArt::ShieldBash;
		const float Push = (bCharge ? 260.f : 250.f) * FMath::Clamp(WeightRatio, 0.4f, 2.5f) * (bBash ? 1.5f : 1.f);
		O->Stagger(bCharge || bBash ? 0.7f : 0.35f, Fwd * Push);
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

void ATaGGameMode::OnShotFake(ATaGCharacter* Shooter, bool bIntoDrive)
{
	// The read itself (drive > contest) is decided by the drive passing the raised hands; this only tells it
	bool bBit = false;
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() != Shooter->GetTeam() && (It->IsContesting() || It->IsContestStarting())
			&& FVector::Dist2D(It->GetActorLocation(), Shooter->GetActorLocation()) < 320.f)
		{
			bBit = true;
			break;
		}
	}
	if (bAutoTest)
	{
		UE_LOG(LogTaG, Display, TEXT("FAKE %s into %s bit=%d"), Shooter->GetHeroDef().Key, bIntoDrive ? TEXT("drive") : TEXT("pass"), bBit ? 1 : 0);
	}
	Popup(bBit ? TEXT("シュートフェイク！ 手が上がった") : TEXT("シュートフェイク"), Shooter->GetTeam());
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
		// The defender did not bite and is still in front: the handler loses the dribble's edge
		// (no drive for a moment, guard chipped). The ball is only lost to an actual reach (steal).
		ReadWin(O, D, TEXT("間合い ＞ かわし"));
		D->LockDrive(1.5f);
		D->ApplyGuardDamage(TaG::ContactGuardDamage * 0.5f);
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
	if (H->IsInvulnerable() || H->IsPhasing())
	{
		return false; // shells and mist cannot be stripped
	}
	const float Range = TaG::StealRange * S->GetHeroDef().ReachMul * S->GetReachFactor() * (S->HasInterceptBoost() ? 1.3f : 1.f);
	if (To.Size() > Range || FVector::DotProduct(To.GetSafeNormal(), Look.GetSafeNormal()) < 0.7f)
	{
		return false;
	}
	float Chance = 0.15f;
	if (H->GetAction() == ETaGAction::DodgeRecover)
	{
		Chance = 0.6f; // the spin whiffed: the ball is exposed
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
	if (O->IsInvulnerable())
	{
		Popup(FString::Printf(TEXT("%s は殻で弾いた"), O->GetHeroDef().Name), O->GetTeam());
		return;
	}
	if (O->IsDodging() || O->IsPhasing())
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
	if (bAutoTest)
	{
		UE_LOG(LogTaG, Display, TEXT("ARTUSE %s %s"), U->GetHeroDef().Key, A.Name);
	}
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

	const bool bMake = bForceMakes || FMath::FRand() < E.Chance;
	if (bAutoTest)
	{
		// One line per shot: how much the bodies decided (the reach gap of the defender that mattered, cm)
		UE_LOG(LogTaG, Display, TEXT("SHOT team=%d hero=%s type=%d dist=%.0f chance=%.2f contest=%.2f reached=%d gap=%.0f art=%d contested=%d make=%d"),
			Team, Shooter->GetHeroDef().Key, (int32)E.Type, E.Distance, E.Chance, E.Contest, E.bReached ? 1 : 0, E.ReachGap,
			E.bArtContest ? 1 : 0, E.bContested ? 1 : 0, bMake ? 1 : 0);
	}
	const bool bValid = !NeedsClear(Team);
	const FVector Feet = Shooter->GetFeetLocation();
	const ETaGArtFx ShotMod = Shooter->GetShotMod();
	// Nikoichi: the kid on the shoulders lets it go from much higher
	const float Release = Shooter->GetHeroDef().EyeHeight + 60.f + (ShotMod == ETaGArtFx::ShotShoulder ? 70.f : 0.f);
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
	const bool bSlam = (E.Type == ETaGShotType::Dunk || (E.Type == ETaGShotType::AlleyOop && Shooter->IsOopSlam())) && E.Distance < 220.f;
	bLastShotOop = E.Type == ETaGShotType::AlleyOop;
	bLastLayupBank = false;
	LayupsTaken += E.Type == ETaGShotType::Layup ? 1 : 0;
	DunksTaken += E.Type == ETaGShotType::Dunk ? 1 : 0;
	if (!bSlam && (E.Type == ETaGShotType::Layup || E.Type == ETaGShotType::Euro || E.Type == ETaGShotType::AlleyOop))
	{
		// One hand at the top of the jump: off the glass from the angles, a finger roll over the front rim otherwise
		From = E.Type == ETaGShotType::AlleyOop ? Shooter->GetReachHands()
			: FTaGRules::LayupRelease(Feet, Shooter->GetActorForwardVector(), Shooter->GetHeroDef().EyeHeight, Shooter->IsFinishingLeft());
		Vel = FTaGBallSim::SolveLayup(From, Target, bMake, bLastLayupBank);
		for (int32 Try = 0; !bMake && Try < 4 && FTaGBallSim::Scores(From, Vel); ++Try)
		{
			const float A = FMath::FRandRange(0.f, 2.f * UE_PI);
			Target = TaG::RimCenter() + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::FRandRange(26.f, 36.f) + FVector(0.f, 0.f, 2.f);
			Vel = FTaGBallSim::SolveLayup(From, Target, false, bLastLayupBank);
		}
		LayupBanks += bLastLayupBank ? 1 : 0;
	}
	if (bSlam && !bAutoTest)
	{
		S->MulticastWorldFx(4, TaG::RimCenter(), Team); // the rim takes the hit
	}
	if (bSlam || E.Type == ETaGShotType::AlleyOop)
	{
		LastSlamTime = GetWorld()->GetTimeSeconds();
		LastSlamKind = E.Type == ETaGShotType::AlleyOop ? 2 : 1;
	}
	if (bSlam)
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
	float BlockMult = St.Block * FMath::Clamp(175.f / Shooter->GetHeroDef().EyeHeight, 0.6f, 1.3f);
	if (ShotMod == ETaGArtFx::ShotHigh)
	{
		BlockMult = 0.f; // Tsurube: the high arc cannot be blocked
	}
	else if (ShotMod == ETaGArtFx::ShotShoulder)
	{
		BlockMult *= 0.3f;
	}
	if (E.bPerfect && !bAutoTest)
	{
		S->MulticastWorldFx(1, From, Team); // a golden flash where the ball leaves the hand
	}
	Ball->LaunchShot(From, Vel, Shooter, E.Points, bValid, BlockMult);
	bShotInFlight = true;
	bLastShotTeamPlay = PossessionPasses > 0;
	S->Stats(Team).Shots++;
	S->Stats(Team).ExpectedMakes += E.Chance;
	if (ATaGPlayerState* P = PS(Shooter)) { P->Stats.Shots++; }
	Shooter->ConsumeZone();
	Shooter->ConsumeShotMod();
	UE_LOG(LogTaG, Verbose, TEXT("Shot team=%d type=%d dist=%.0f pts=%d charge=%.2f p=%.2f make=%d"), Team, static_cast<int32>(E.Type), E.Distance, E.Points, Charge, E.Chance, bMake ? 1 : 0);
}

void ATaGGameMode::LaunchPass(ATaGCharacter* Passer, ATaGCharacter* Receiver, uint8 Style)
{
	if (!Ball || !Passer || !Receiver || Ball->GetHolder() != Passer)
	{
		return;
	}
	// A pass to a teammate running at the rim goes up as an alley-oop: a lob to where the hands will be at the top of
	// the leap (plain passes and lobs; threads and curves keep their own flight)
	if (Style == 0 || Style == 3)
	{
		const FTaGAlleyOop Oop = FTaGRules::PlanAlleyOop(Passer, Receiver);
		if (Oop.bValid)
		{
			Ball->LaunchAlleyOop(Passer, Receiver, Oop);
			LastPasser = Passer;
			GS()->Stats(Passer->GetTeam()).Passes++;
			OopThrown++;
			Popup(TEXT("アリウープのパス！"), Passer->GetTeam());
			return;
		}
	}
	Ball->LaunchPass(Passer, Receiver, Style);
	LastPasser = Passer;
	GS()->Stats(Passer->GetTeam()).Passes++;
	if (Style == 1)
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
	// Lantern / data-share marks add to the chain; marked and curved catches can shoot at once
	const int32 Bonus = Receiver->ConsumeMark();
	Ball->SetChain(FMath::Min(PossessionPasses + Bonus, 5));
	Receiver->OnCaughtPass();
	if (Bonus > 0 || Ball->GetPassStyle() == 2)
	{
		Receiver->GrantCatchAndShoot(1.5f);
		if (Bonus > 0)
		{
			Popup(FString::Printf(TEXT("灯りが乗った！ チェイン+%d"), Bonus), Receiver->GetTeam());
		}
	}
}

void ATaGGameMode::OnAlleyOopCaught(ATaGCharacter* Receiver)
{
	if (!Receiver)
	{
		return;
	}
	OopCaught++;
	if (SnapMode == TEXT("AlleyOop"))
	{
		TestCatchAt = TestT;
		TestCatchErr = Ball ? Ball->GetLastCatchError() : -1.f;
		TestCatchZ = Ball ? Ball->GetLastCatchPos().Z : 0.f;
		TestCatchDt = Receiver->ServerNow() - Receiver->GetAlleyOopTime();
	}
	Receiver->StartAlleyOopFinish();
}

void ATaGGameMode::SkipHighlight()
{
	if (bHighlightShowing && !bLive && bRestartAfterDead)
	{
		DeadTimeLeft = FMath::Min(DeadTimeLeft, 0.4f);
	}
	bHighlightShowing = false;
}

void ATaGGameMode::OnAlleyOopStopped(ATaGCharacter* D)
{
	OopStopped++;
	ATaGGameState* S = GS();
	if (!S || !D)
	{
		return;
	}
	S->Stats(D->GetTeam()).Blocks++;
	if (ATaGPlayerState* P = PS(D)) { P->Stats.Blocks++; }
	if (!bAutoTest && Ball)
	{
		S->MulticastWorldFx(2, Ball->GetActorLocation(), D->GetTeam());
	}
	Popup(FString::Printf(TEXT("アリウープを阻止！ %s"), D->GetHeroDef().Name), D->GetTeam());
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
	bTestScored = true;
	if (bValid && bLastShotOop)
	{
		OopMade++;
	}
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
	// A slam or an alley-oop that went in is replayed during the dead ball (alley-oops always, slams at most once a
	// minute so the game keeps its pace), if the host has replays on
	const float Now = GetWorld()->GetTimeSeconds();
	const ATaGPlayerController* HostPC = Cast<ATaGPlayerController>(GetWorld()->GetFirstPlayerController());
	const bool bReplayTest = FParse::Param(FCommandLine::Get(), TEXT("TaGReplay"));
	const bool bReplays = !bAutoTest && !S->bTutorial && (SnapMode.IsEmpty() || bReplayTest) && (!HostPC || HostPC->bReplays);
	if (bReplays && Now - LastSlamTime < 1.5f && (LastSlamKind == 2 || S->MatchTime - LastReplayAt > 60.f))
	{
		const float From = LastSlamTime - 1.7f;
		const float To = Now + 0.45f;
		S->MulticastHighlight(From, To, LastSlamKind, FText::FromString(LastSlamKind == 2 ? TEXT("アリウープ") : TEXT("ダンク")), Shooter);
		LastReplayAt = S->MatchTime;
		LastSlamTime = -100.f;
		bHighlightShowing = true;
		ScheduleTurnover(1 - Team, ATaGReplay::HoldSeconds(To - From, ATaGGameState::HighlightDelay, ATaGGameState::HighlightSpeed));
		return;
	}
	ScheduleTurnover(1 - Team, 1.4f);
}

void ATaGGameMode::OnShotMissed(bool bTouchedRim)
{
	bShotInFlight = false;
	bTestMissed = true;
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
		if (!bAutoTest && Ball)
		{
			GS()->MulticastWorldFx(2, Ball->GetActorLocation(), Blocker->GetTeam());
		}
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

void ATaGGameMode::OnHandlerOutOfBounds(ATaGCharacter* Handler)
{
	ATaGGameState* S = GS();
	if (!S || !Handler)
	{
		return;
	}
	const int32 Team = Handler->GetTeam();
	const FVector Feet = Handler->GetFeetLocation();
	UE_LOG(LogTaG, Display, TEXT("OUTOFBOUNDS team=%d hero=%s feet=(%.0f, %.0f) line=%.0fcm"), Team, Handler->GetHeroDef().Key, Feet.X, Feet.Y, TaG::DistToBoundary(Feet));
	S->Stats(Team).Turnovers++;
	EndPossession(Team, 0);
	Popup(TEXT("アウト・オブ・バウンズ（ラインを踏んだ）"), 1 - Team);
	Sfx(TEXT("Whistle"), Handler->GetFeetLocation(), 0.6f);
	if (Ball)
	{
		Ball->SetDead();
	}
	ScheduleTurnover(1 - Team, 1.2f);
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
	UE_LOG(LogTaG, Display, TEXT("STATS finishes layups=%d bank=%d dunks=%d alleyoop_thrown=%d caught=%d stopped=%d made=%d"),
		LayupsTaken, LayupBanks, DunksTaken, OopThrown, OopCaught, OopStopped, OopMade);
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

	// Layups from the hand at the top of the jump (FTaGRules::LayupRelease): every angle around the rim, small to XL
	// bodies, low to high jumps, the top of the jump close to or farther from the rim
	int32 LayOk = 0, LayTotal = 0, LayBank = 0, LayMissIn = 0, LayMissTotal = 0;
	for (const float Eye : { 140.f, 175.f, 210.f, 250.f })
	{
		for (float Bearing = -85.f; Bearing <= 85.f; Bearing += 17.f)
		{
			for (const FVector2D ApexLift : { FVector2D(60.f, 30.f), FVector2D(85.f, 60.f), FVector2D(110.f, 95.f), FVector2D(85.f, 95.f) })
			{
				const float B = FMath::DegreesToRadians(Bearing);
				const FVector Feet(TaG::RimX + FMath::Cos(B) * ApexLift.X, FMath::Sin(B) * ApexLift.X, ApexLift.Y);
				const FVector From = FTaGRules::LayupRelease(Feet, FVector(TaG::RimX - Feet.X, -Feet.Y, 0.f), Eye, Bearing > 0.f);
				const float Z = From.Z;
				const float Reach = FVector2D(From.X - TaG::RimX, From.Y).Size();
				bool bBank = false;
				const FVector V = FTaGBallSim::SolveLayup(From, TaG::RimCenter() + FVector(0.f, 0.f, 2.f), true, bBank);
				const bool bIn = FTaGBallSim::Scores(From, V);
				LayTotal++;
				LayOk += bIn ? 1 : 0;
				LayBank += bBank ? 1 : 0;
				if (!bIn)
				{
					UE_LOG(LogTaG, Display, TEXT("SHOTTEST layup fail reach=%.0f bearing=%.0f z=%.0f"), Reach, Bearing, Z);
				}
				for (int32 T = 0; T < 4; ++T)
				{
					const float A = 2.f * UE_PI * T / 4.f;
					const FVector Off = FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 26.f;
					bool bMissBank = false;
					const FVector MV = FTaGBallSim::SolveLayup(From, TaG::RimCenter() + FVector(0.f, 0.f, 2.f) + Off, false, bMissBank);
					LayMissTotal++;
					LayMissIn += FTaGBallSim::Scores(From, MV) ? 1 : 0;
				}
			}
		}
	}
	UE_LOG(LogTaG, Display, TEXT("SHOTTEST layups planned_makes_in=%d/%d (bank %d)  planned_misses_that_went_in=%d/%d"), LayOk, LayTotal, LayBank, LayMissIn, LayMissTotal);
}

// ================================================================ Tests: layups and alley-oops

void ATaGGameMode::TestShotAt(const FString& Name, bool bUI)
{
	const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("TaGSnap_%s_%s.png"), *SnapMode, *Name);
	FScreenshotRequest::RequestScreenshot(File, bUI, false);
	UE_LOG(LogTaG, Display, TEXT("SNAP requested %s"), *File);
}

void ATaGGameMode::TestSideCamera(const FVector& From, const FVector& To)
{
	UWorld* World = GetWorld();
	ACameraActor* Cam = Cast<ACameraActor>(TestCam);
	if (!Cam)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		TestCam = Cam;
		if (!Cam)
		{
			return;
		}
		Cam->GetCameraComponent()->SetFieldOfView(55.f);
		Cam->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	const FVector Dir = FVector(To - From).GetSafeNormal2D();
	FVector Perp(-Dir.Y, Dir.X, 0.f);
	if (Perp.X < 0.f)
	{
		Perp = -Perp; // from the court side
	}
	const FVector Mid = FMath::Lerp(From, To, 0.62f);
	Cam->SetActorLocation(Mid + Perp * 720.f + FVector(0.f, 0.f, 175.f));
	Cam->SetActorRotation((Mid + FVector(0.f, 0.f, 150.f) - Cam->GetActorLocation()).Rotation());
	if (ATaGPlayerController* PC = Cast<ATaGPlayerController>(World->GetFirstPlayerController()))
	{
		PC->bExternalView = true;
		PC->SetViewTarget(Cam);
	}
}

void ATaGGameMode::TickLayupTest(float Dt)
{
	// -TaGSnap=Layup (1on1, the bot stands aside): the player runs at the rim from five spots and finishes; the log has
	// the planned and real take-off, which foot left the floor, the steps after the gather, the foot slip, the top of
	// the jump against the release, and whether it went off the glass
	SnapTimer += Dt;
	ATaGGameState* S = GS();
	ATaGPlayerController* PC = Cast<ATaGPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!S || !PC)
	{
		return;
	}
	if (S->Phase == ETaGPhase::Lobby && SnapTimer > 1.5f)
	{
		FString HeroKey;
		if (FParse::Value(FCommandLine::Get(), TEXT("TaGHero="), HeroKey))
		{
			SetPlayerHero(PC, TaGHeroes::FromKey(HeroKey));
		}
		StartMatch();
		return;
	}
	ATaGCharacter* Me = Cast<ATaGCharacter>(PC->GetPawn());
	if (S->Phase != ETaGPhase::Playing || !Me || !Ball)
	{
		return;
	}
	if (TestStage == 0)
	{
		if (!bLive)
		{
			return;
		}
		for (ATaGBotController* B : Bots)
		{
			if (B)
			{
				B->SetScript(ATaGBotController::EScript::Idle);
			}
		}
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveAllViewportWidgets();
		}
		bForceMakes = true;
		TestStage = 1;
		TestCase = 0;
		TestT = -1.f;
	}
	struct FCase { const TCHAR* Name; FVector Start; bool bDrive; bool bSide; };
	static const FCase Cases[] = {
		{ TEXT("RightWing"), FVector(TaG::RimX + 424.f, -424.f, 0.f), false, true },
		{ TEXT("LeftWing"), FVector(TaG::RimX + 424.f, 424.f, 0.f), false, false },
		{ TEXT("Middle"), FVector(TaG::RimX + 600.f, 0.f, 0.f), false, false },
		{ TEXT("Baseline"), FVector(TaG::RimX + 30.f, 600.f, 0.f), false, true },
		{ TEXT("DriveDunk"), FVector(TaG::RimX + 650.f, -160.f, 0.f), true, true },
	};
	constexpr int32 NumCases = UE_ARRAY_COUNT(Cases);
	if (TestCase >= NumCases)
	{
		UE_LOG(LogTaG, Display, TEXT("LAYUPTEST done hero=%s%s"), Me->GetHeroDef().Key, *TestSummary);
		FPlatformMisc::RequestExit(false);
		TestCase = 1000;
		return;
	}
	const FCase& C = Cases[TestCase];
	UTaGRigComponent* Rig = Me->GetRig();
	if (TestT < 0.f)
	{
		Me->ClearForNewPossession(C.Start, TaG::RimCenter());
		Ball->GiveTo(Me);
		bLive = true;
		bRestartAfterDead = false;
		S->ShotClock = TaG::ShotClock;
		TestT = 0.f;
		TestT0 = -1.f;
		TestTakeoff = -1.f;
		TestApexT = -1.f;
		TestApexZ = 0.f;
		TestRelease = -1.f;
		TestFootErr = 0.f;
		TestPlanted = -1;
		TestTakeoffFoot = -1;
		TestStepLog.Reset();
		TestShot = 0;
		bTestScored = false;
		bTestMissed = false;
		bTestPressed = false;
		bTestDriven = false;
		TestDoneT = -1.f;
		if (C.bSide)
		{
			Me->SetThirdPerson(true);
			TestSideCamera(C.Start, TaG::RimFloor());
		}
		else
		{
			Me->SetThirdPerson(false);
			PC->bExternalView = false;
			PC->SetViewTarget(Me);
		}
		return;
	}
	TestT += Dt;
	const FVector Feet = Me->GetFeetLocation();
	const FVector ToRim = FVector(TaG::RimX - Feet.X, -Feet.Y, 0.f).GetSafeNormal();
	const bool bGround = Me->GetCharacterMovement()->IsMovingOnGround();
	if (!bTestPressed)
	{
		// Run at the rim like a player, then start the shot inside the range
		if (TestT > 0.3f)
		{
			Me->SetTestStick(ToRim);
			if (!Me->IsForcedMove())
			{
				Me->AddMovementInput(ToRim, 1.f);
			}
		}
		const float SpeedIn = FMath::Max(0.f, FVector::DotProduct(Me->GetVelocity(), ToRim));
		const float Dist = TaG::DistToRim(Feet);
		if (C.bDrive && !bTestDriven && TestT > 0.6f)
		{
			bTestDriven = true;
			Me->InputPressed(ETaGInput::Primary);
		}
		const bool bInRange = C.bDrive
			? (Dist < 250.f && (Me->GetAction() == ETaGAction::Drive || Me->GetAction() == ETaGAction::Gathered))
			: (TestT > 0.8f && Dist < FTaGRules::LayupRange(SpeedIn) - 20.f);
		if (bInRange)
		{
			Me->InputPressed(ETaGInput::Shoot);
			bTestPressed = Me->GetAction() == ETaGAction::Shoot;
			if (bTestPressed)
			{
				TestT0 = TestT;
				TestSteps0 = Rig ? Rig->GetStepCount() : 0;
				TestLastStepCount = TestSteps0;
				const FTaGRunUp& R = Me->GetRunUp();
				UE_LOG(LogTaG, Display, TEXT("LAYUPTEST %s start type=%s dist=%.0f speed_in=%.0f plan takeoff=%.2fs approach=%.0f keep=%.0f vz=%.0f hand=%s"),
					C.Name, FTaGRules::ShotType(Me->GetShotType()).Name, Dist, SpeedIn, R.TakeoffT, R.ApproachSpeed, R.TakeoffSpeed, R.JumpVZ,
					R.bLeftHand ? TEXT("L") : TEXT("R"));
			}
		}
		if (TestT > 6.f)
		{
			UE_LOG(LogTaG, Warning, TEXT("LAYUPTEST %s never got into range"), C.Name);
			TestCase++;
			TestT = -1.f;
		}
		return;
	}
	const float T = TestT - TestT0;
	Me->SetTestStick(ToRim);
	if (Rig && TestTakeoff < 0.f)
	{
		if (Rig->GetStepCount() != TestLastStepCount)
		{
			TestLastStepCount = Rig->GetStepCount();
			TestStepLog += FString::Printf(TEXT(" %s@%.2f"), Rig->GetLastStepSide() == 0 ? TEXT("L") : TEXT("R"), T);
		}
		const bool bL = Rig->IsFootPlanted(0);
		const bool bR = Rig->IsFootPlanted(1);
		if (bL != bR)
		{
			TestPlanted = bL ? 0 : 1;
		}
		if (bGround)
		{
			TestFootErr = FMath::Max(TestFootErr, FMath::Max(Rig->GetFootError(0), Rig->GetFootError(1)));
		}
	}
	if (TestTakeoff < 0.f && !bGround)
	{
		TestTakeoff = T;
		TestTakeoffFoot = TestPlanted;
	}
	if (TestTakeoff >= 0.f && !bGround && TestRelease < 0.f && Feet.Z > TestApexZ)
	{
		TestApexZ = Feet.Z;
		TestApexT = T;
	}
	// Let go at the top: the meter's perfect middle
	if (TestRelease < 0.f && Me->GetAction() == ETaGAction::Shoot && Me->GetShotCharge() >= FTaGRules::RunUpApexCharge)
	{
		Me->InputReleased(ETaGInput::Shoot);
	}
	if (TestRelease < 0.f && Me->GetAction() != ETaGAction::Shoot)
	{
		TestRelease = T;
		TestReleaseZ = Feet.Z;
		TestHandOff = Ball->GetHandOff();
	}
	// The gather, the plant, the take-off, the top, the ball on its way
	const FTaGRunUp& R = Me->GetRunUp();
	const float TUp = R.JumpVZ / TaG::Gravity;
	const float Shots[5] = { 0.04f, R.TakeoffT - FTaGRules::RunUpStanceT, R.TakeoffT + 0.03f, R.TakeoffT + TUp, R.TakeoffT + TUp + 0.3f };
	if (TestShot < 5 && T >= Shots[TestShot])
	{
		TestShotAt(FString::Printf(TEXT("%s_%d"), C.Name, TestShot), false);
		TestShot++;
	}
	if (TestDoneT < 0.f && TestRelease >= 0.f && (bTestScored || bTestMissed || T > TestRelease + 3.f))
	{
		TestDoneT = TestT;
		const FString Line = FString::Printf(TEXT(" | %s takeoff=%.2f/%.2f foot=%s hand=%s steps=[%s ] foot_err=%.1fcm apex=%.0fcm@%.2f release=%.2f@%.0fcm handoff=%.0fcm bank=%d %s"),
			C.Name, TestTakeoff, R.TakeoffT, TestTakeoffFoot == 0 ? TEXT("L") : (TestTakeoffFoot == 1 ? TEXT("R") : TEXT("?")),
			R.bLeftHand ? TEXT("L") : TEXT("R"), *TestStepLog, TestFootErr, TestApexZ, TestApexT, TestRelease, TestReleaseZ, TestHandOff,
			bLastLayupBank ? 1 : 0, bTestScored ? TEXT("IN") : TEXT("OUT"));
		UE_LOG(LogTaG, Display, TEXT("LAYUPTEST%s"), *Line);
		TestSummary += Line;
	}
	if (TestDoneT >= 0.f && TestT - TestDoneT > 1.2f)
	{
		TestCase++;
		TestT = -1.f;
	}
}

void ATaGGameMode::TickOopTest(float Dt)
{
	// -TaGSnap=AlleyOop (2on2, the defenders stand aside): the player throws it up to a cutting bot (first person, then
	// from the side), and a bot throws it up to the player cutting to the rim, who jumps when the prompt says so
	SnapTimer += Dt;
	ATaGGameState* S = GS();
	ATaGPlayerController* PC = Cast<ATaGPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!S || !PC)
	{
		return;
	}
	if (S->Phase == ETaGPhase::Lobby && SnapTimer > 1.5f)
	{
		FString HeroKey;
		if (FParse::Value(FCommandLine::Get(), TEXT("TaGHero="), HeroKey))
		{
			SetPlayerHero(PC, TaGHeroes::FromKey(HeroKey));
		}
		StartMatch();
		return;
	}
	ATaGCharacter* Me = Cast<ATaGCharacter>(PC->GetPawn());
	if (S->Phase != ETaGPhase::Playing || !Me || !Ball)
	{
		return;
	}
	ATaGCharacter* Mate = nullptr;
	for (ATaGCharacter* P : GetTeamPlayers(Me->GetTeam()))
	{
		if (P != Me)
		{
			Mate = P;
		}
	}
	ATaGBotController* MateAI = Mate ? Cast<ATaGBotController>(Mate->GetController()) : nullptr;
	if (!Mate || !MateAI)
	{
		return;
	}
	if (TestStage == 0)
	{
		if (!bLive)
		{
			return;
		}
		for (ATaGBotController* B : Bots)
		{
			ATaGCharacter* P = B ? Cast<ATaGCharacter>(B->GetPawn()) : nullptr;
			if (P && P->GetTeam() != Me->GetTeam())
			{
				B->SetScript(ATaGBotController::EScript::Idle);
			}
		}
		MateAI->Skill = 1.f;
		bForceMakes = true;
		TestStage = 1;
		TestCase = 0;
		TestT = -1.f;
	}
	struct FCase { const TCHAR* Name; bool bMeReceives; bool bSide; };
	static const FCase Cases[] = {
		{ TEXT("Pass"), false, false },
		{ TEXT("PassSide"), false, true },
		{ TEXT("Receive"), true, false },
	};
	constexpr int32 NumCases = UE_ARRAY_COUNT(Cases);
	if (TestCase >= NumCases)
	{
		UE_LOG(LogTaG, Display, TEXT("OOPTEST done%s"), *TestSummary);
		FPlatformMisc::RequestExit(false);
		TestCase = 1000;
		return;
	}
	const FCase& C = Cases[TestCase];
	ATaGCharacter* Cutter = C.bMeReceives ? Me : Mate;
	ATaGCharacter* Thrower = C.bMeReceives ? Mate : Me;
	if (TestT < 0.f)
	{
		const FVector Top(TaG::RimX + 680.f, 40.f, 0.f);
		const FVector Wing(TaG::RimX + 470.f, C.bMeReceives ? 430.f : -430.f, 0.f);
		Thrower->ClearForNewPossession(Top, Wing);
		Cutter->ClearForNewPossession(Wing, TaG::RimCenter());
		if (C.bMeReceives)
		{
			Ball->SetDead();
		}
		else
		{
			Ball->GiveTo(Me);
		}
		bLive = true;
		bRestartAfterDead = false;
		S->ShotClock = TaG::ShotClock;
		TestT = 0.f;
		TestT0 = -1.f;
		TestShot = 0;
		bTestScored = false;
		bTestMissed = false;
		bTestPressed = false;
		bTestDriven = false;
		TestLeapLate = 99.f;
		TestCatchAt = -1.f;
		TestCatchErr = -1.f;
		TestCatchZ = 0.f;
		TestCatchDt = 99.f;
		TestDoneT = -1.f;
		if (C.bSide)
		{
			Me->SetThirdPerson(true);
			TestSideCamera(Wing, TaG::RimFloor());
		}
		else
		{
			Me->SetThirdPerson(false);
			PC->bExternalView = false;
			PC->SetViewTarget(Me);
		}
		return;
	}
	TestT += Dt;
	const bool bLob = Ball->GetBallState() == ETaGBallState::Pass && Ball->GetPassStyle() == 4;
	if (!C.bMeReceives)
	{
		// The bot cuts; the player throws it up as soon as the cut allows a lob, then watches the rim
		if (!bTestDriven && TestT > 0.4f)
		{
			bTestDriven = true;
			MateAI->TestCut();
		}
		if (!bTestPressed && TestT > 0.5f && FTaGRules::PlanAlleyOop(Me, Mate).bValid)
		{
			const FRotator Face = (Mate->GetActorLocation() - Me->GetEyeLocation()).Rotation();
			PC->SetControlRotation(FRotator(0.f, Face.Yaw, 0.f));
			Me->InputPressed(ETaGInput::PassSteal);
			bTestPressed = true;
		}
		if (bTestPressed && TestT0 >= 0.f && !C.bSide)
		{
			const FRotator Want = (Mate->GetAlleyOopPoint() - Me->GetEyeLocation()).Rotation();
			PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), FRotator(FMath::Clamp(Want.Pitch, -30.f, 30.f), Want.Yaw, 0.f), Dt, 5.f));
		}
	}
	else
	{
		// The player cuts from the wing; the bot at the top gets the ball once the cut is on and throws it up
		const FVector Feet = Me->GetFeetLocation();
		const FVector Goal = Me->HasAlleyOop() ? Me->GetAlleyOopPoint() : TaG::RimFloor();
		const FVector ToGoal = FVector(Goal.X - Feet.X, Goal.Y - Feet.Y, 0.f).GetSafeNormal();
		if (TestT > 0.3f && Me->GetCharacterMovement()->IsMovingOnGround() && TestCatchAt < 0.f)
		{
			// Running in, the player looks where the ball will be met
			if (Me->HasAlleyOop() && Me->GetAction() != ETaGAction::Leap)
			{
				PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), FRotator(10.f, ToGoal.Rotation().Yaw, 0.f), Dt, 8.f));
			}
			Me->SetTestStick(ToGoal);
			if (!Me->IsForcedMove())
			{
				Me->AddMovementInput(ToGoal, 1.f);
			}
			if (!bTestDriven && TestT > 0.45f)
			{
				bTestDriven = true;
				Me->InputPressed(ETaGInput::Primary); // a cut
			}
		}
		if (TestT > 0.6f && !bTestPressed)
		{
			bTestPressed = true;
			Ball->GiveTo(Mate);
		}
		// Jump when the prompt says now
		if (Me->HasAlleyOop() && Me->GetAlleyOopJumpIn() <= 0.f && Me->GetCharacterMovement()->IsMovingOnGround())
		{
			Me->InputJump();
		}
	}
	if (bLob && TestT0 < 0.f)
	{
		TestT0 = TestT;
		const FVector CP = Cutter->GetAlleyOopPoint();
		UE_LOG(LogTaG, Display, TEXT("OOPTEST %s thrown by %s to %s: flight=%.2fs catch=(%.0f,%.0f,%.0f) leap_vz=%.0f slam=%d"),
			C.Name, Thrower->GetHeroDef().Key, Cutter->GetHeroDef().Key, Cutter->GetAlleyOopTime() - Cutter->ServerNow(), CP.X, CP.Y, CP.Z,
			Cutter->GetCharacterMovement()->JumpZVelocity, Cutter->IsOopSlam() ? 1 : 0);
	}
	if (TestLeapLate > 90.f && Cutter->GetAction() == ETaGAction::Leap && Cutter->HasAlleyOop())
	{
		TestLeapLate = -Cutter->GetAlleyOopJumpIn();
	}
	// Screenshots: the lob up, the leap, the catch, the slam, through the net
	if (TestT0 >= 0.f)
	{
		const bool bUI = C.bMeReceives;
		if (TestShot == 0 && TestT > TestT0 + 0.25f)
		{
			TestShotAt(FString::Printf(TEXT("%s_0"), C.Name), bUI);
			TestShot++;
		}
		else if (TestShot == 1 && Cutter->GetAction() == ETaGAction::Leap && Cutter->GetVelocity().Z < 120.f)
		{
			TestShotAt(FString::Printf(TEXT("%s_1"), C.Name), bUI);
			TestShot++;
		}
		else if (TestShot == 2 && TestCatchAt >= 0.f)
		{
			TestShotAt(FString::Printf(TEXT("%s_2"), C.Name), bUI);
			TestShot++;
		}
		else if (TestShot == 3 && TestCatchAt >= 0.f && TestT > TestCatchAt + 0.16f)
		{
			TestShotAt(FString::Printf(TEXT("%s_3"), C.Name), bUI);
			TestShot++;
		}
		else if (TestShot == 4 && TestCatchAt >= 0.f && TestT > TestCatchAt + 0.4f)
		{
			TestShotAt(FString::Printf(TEXT("%s_4"), C.Name), bUI);
			TestShot++;
		}
	}
	const bool bOver = bTestScored || bTestMissed || (TestT0 >= 0.f && !bLob && TestCatchAt < 0.f && Ball->GetBallState() != ETaGBallState::Held)
		|| TestT > 9.f;
	if (TestDoneT < 0.f && bOver && (TestT0 >= 0.f || TestT > 9.f))
	{
		TestDoneT = TestT;
		const FString Line = FString::Printf(TEXT(" | %s receiver=%s thrown=%d leap_late=%.2fs caught=%d catch_err=%.0fcm catch_z=%.0fcm catch_dt=%.2fs %s"),
			C.Name, Cutter->GetHeroDef().Key, TestT0 >= 0.f ? 1 : 0, TestLeapLate > 90.f ? -1.f : TestLeapLate, TestCatchAt >= 0.f ? 1 : 0,
			TestCatchErr, TestCatchZ, TestCatchDt, bTestScored ? TEXT("IN") : TEXT("NO"));
		UE_LOG(LogTaG, Display, TEXT("OOPTEST%s"), *Line);
		TestSummary += Line;
	}
	// -TaGReplay: wait through the highlight replay of a basket and photograph it
	const bool bReplayTest = FParse::Param(FCommandLine::Get(), TEXT("TaGReplay"));
	if (bReplayTest && TestDoneT >= 0.f && bTestScored)
	{
		const float Since = TestT - TestDoneT;
		const float Marks[3] = { 1.6f, 2.6f, 3.6f };
		for (int32 m = 0; m < 3; ++m)
		{
			if (Since >= Marks[m] && Since - Dt < Marks[m])
			{
				TestShotAt(FString::Printf(TEXT("%s_Replay%d"), C.Name, m), true);
				UE_LOG(LogTaG, Display, TEXT("OOPTEST %s replay=%d"), C.Name, ATaGReplay::IsPlaying(GetWorld()) ? 1 : 0);
			}
		}
	}
	if (TestDoneT >= 0.f && TestT - TestDoneT > (bReplayTest && bTestScored ? 6.5f : 1.2f))
	{
		TestCase++;
		TestT = -1.f;
	}
}

void ATaGGameMode::TickGait(float Dt)
{
	SnapTimer += Dt;
	UWorld* World = GetWorld();
	const ETaGHero Heroes[4] = { ETaGHero::Rei, ETaGHero::Hayate, ETaGHero::Minamo, ETaGHero::Gantetsu };
	const FVector Starts[4] = { FVector(700.f, -200.f, 0.f), FVector(500.f, 0.f, 0.f), FVector(300.f, 250.f, 0.f), FVector(800.f, 300.f, 0.f) };
	if (GaitChars.Num() == 0 && SnapTimer > 1.f)
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveAllViewportWidgets(); // no menus over the test scene
		}
		for (int32 i = 0; i < 4; ++i)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AAIController* AI = World->SpawnActor<AAIController>(AAIController::StaticClass(), Params);
			ATaGCharacter* C = SpawnPlayerCharacter(AI, Heroes[i], i % 2, Starts[i]);
			GaitChars.Add(C);
		}
		ACameraActor* Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FVector(560.f, -980.f, 150.f)));
		if (Cam)
		{
			Cam->SetActorRotation((FVector(560.f, 40.f, 95.f) - Cam->GetActorLocation()).Rotation());
			Cam->GetCameraComponent()->SetFieldOfView(58.f);
			Cam->GetCameraComponent()->bConstrainAspectRatio = false;
			if (ATaGPlayerController* PC = Cast<ATaGPlayerController>(World->GetFirstPlayerController()))
			{
				PC->bExternalView = true;
				PC->SetViewTarget(Cam);
			}
		}
		GaitCam = Cam;
		return;
	}
	if (GaitChars.Num() == 0)
	{
		return;
	}
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (GaitCam && PC->GetViewTarget() != GaitCam)
		{
			PC->SetViewTarget(GaitCam);
		}
	}
	const float T = SnapTimer - 1.f;
	if (FApp::GetDeltaTime() >= 0.05f)
	{
		GaitHitchT = T;
	}
	for (int32 i = 0; i < GaitChars.Num(); ++i)
	{
		ATaGCharacter* C = GaitChars[i];
		AController* Ctrl = C ? C->GetController() : nullptr;
		if (!Ctrl)
		{
			continue;
		}
		FVector Move = FVector::ZeroVector;
		float Yaw = 0.f;
		switch (i)
		{
		case 0: // runs forward around a circle, facing where it goes
		{
			const float A = T * 1.4f;
			Move = FVector(-FMath::Sin(A), FMath::Cos(A), 0.f);
			Yaw = Move.Rotation().Yaw;
			break;
		}
		case 1: // backpedals and comes forward again, always facing +X
			Move = FVector(FMath::Fmod(T, 3.f) < 1.5f ? -1.f : 1.f, 0.f, 0.f);
			break;
		case 2: // defensive side shuffle, facing +X
			Move = FVector(0.f, FMath::Fmod(T, 2.4f) < 1.2f ? 1.f : -1.f, 0.f);
			break;
		default: // walks (a light stick) diagonally: forward-left, then back-right
			Move = FMath::Fmod(T, 3.f) < 1.5f ? FVector(0.7f, -0.7f, 0.f) : FVector(-0.7f, 0.7f, 0.f);
			break;
		}
		if (T > 7.f)
		{
			Move = FVector::ZeroVector; // stop: the feet should settle under the hips
		}
		const float StickIn = i == 3 ? 0.45f : 1.f;
		// An AI controller copies the pawn's facing back into its control rotation every tick, so a plain
		// SetControlRotation does nothing: give it a focal point in the wanted direction instead
		if (AAIController* AIC = Cast<AAIController>(Ctrl))
		{
			AIC->SetFocalPoint(C->GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * 10000.f);
		}
		Ctrl->SetControlRotation(FRotator(0.f, Yaw, 0.f));
		C->AddMovementInput(Move, StickIn);
		// Start: time to 90 % of the speed the stick asks for; stop: time from letting go to standing
		const float Sp = C->GetVelocity().Size2D();
		if (GaitStartT[i] < 0.f && T > 0.f && Sp >= 0.9f * StickIn * C->GetDesiredMaxSpeed())
		{
			GaitStartT[i] = T;
		}
		if (GaitStopT[i] < 0.f && T > 7.f && Sp < 15.f)
		{
			GaitStopT[i] = T - 7.f;
		}
		if (UTaGRigComponent* Rig = C->GetRig())
		{
			// Measure steady walking only: frames right after a hitch (screenshots) are skipped.
			if (T > 1.f && FApp::GetDeltaTime() < 0.05f && T - GaitHitchT > 0.4f)
			{
				const float E = FMath::Max(Rig->GetFootError(0), Rig->GetFootError(1));
				if (E > 8.f && E > GaitErr[i] + 2.f)
				{
					UE_LOG(LogTaG, Display, TEXT("GAITTEST walker=%d t=%.2f err=%.1f %s"), i, T, E, *Rig->DebugFeet());
				}
				GaitErr[i] = FMath::Max(GaitErr[i], E);
				if (T > 2.f && T < 7.f)
				{
					if (FMath::Frac(T * 4.f) < Dt * 4.f)
					{
						UE_LOG(LogTaG, Display, TEXT("GAITHEAD walker=%d t=%.2f %s"), i, T, *Rig->DebugHead());
					}
					const float Hz = Rig->GetHeadMotion().Z;
					GaitBobMin[i] = FMath::Min(GaitBobMin[i], Hz);
					GaitBobMax[i] = FMath::Max(GaitBobMax[i], Hz);
				}
			}
			GaitSteps[i] = Rig->GetStepCount();
		}
	}
	// A burst of frames while they move, then one after they stopped
	const float Shots[7] = { 5.0f, 5.12f, 5.24f, 5.36f, 5.48f, 5.6f, 9.2f };
	if (GaitShot < 7 && T >= Shots[GaitShot])
	{
		const FString File = FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("TaGSnap_Gait_%d.png"), GaitShot);
		FScreenshotRequest::RequestScreenshot(File, false, false);
		GaitShot++;
	}
	if (T > 10.f)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			UE_LOG(LogTaG, Display, TEXT("GAITTEST walker=%d hero=%s steps=%d max_foot_error=%.2fcm head_bob=%.1fcm start90=%.2fs stop=%.2fs"), i,
				TaGHeroes::Get(Heroes[i]).Key, GaitSteps[i], GaitErr[i], GaitBobMax[i] - GaitBobMin[i], GaitStartT[i], GaitStopT[i]);
		}
		FPlatformMisc::RequestExit(false);
	}
}

void ATaGGameMode::TickSnap(float Dt)
{
	// -TaGSnap=<Title|Lobby|Play|TPS|Results>: take one screenshot after the scene settles, then quit.
	if (SnapMode == TEXT("Gait"))
	{
		TickGait(Dt);
		return;
	}
	if (SnapMode == TEXT("Layup"))
	{
		TickLayupTest(Dt);
		return;
	}
	if (SnapMode == TEXT("AlleyOop"))
	{
		TickOopTest(Dt);
		return;
	}
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
	if (SnapStage == 0 && (SnapMode == TEXT("Play") || SnapMode == TEXT("TPS") || SnapMode == TEXT("Results") || SnapMode == TEXT("Defense") || SnapMode == TEXT("Lineup") || SnapMode == TEXT("Threat")) && SnapTimer > 2.f && S->Phase == ETaGPhase::Lobby)
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
	if (SnapMode == TEXT("Lineup"))
	{
		ShotAt = 3.4f; // about 1.3 s into the match, while the lineups are up
	}
	if (SnapMode == TEXT("Threat"))
	{
		ShotAt = 9.5f;
		if (SnapStage == 0 && SnapTimer > ShotAt - 0.15f && SnapTimer - Dt <= ShotAt - 0.15f && SnapPC)
		{
			ATaGCharacter* Me = Cast<ATaGCharacter>(SnapPC->GetPawn());
			ATaGCharacter* Opp = Me ? Me->FindNearestOpponent() : nullptr;
			if (Me && Opp)
			{
				FString Side = TEXT("Behind");
				FParse::Value(FCommandLine::Get(), TEXT("TaGThreatSide="), Side);
				const FVector Off = Side == TEXT("Right") ? Me->GetActorRightVector() * 260.f : -Me->GetActorForwardVector() * 260.f;
				const FVector At = Me->GetActorLocation() + Off;
				Opp->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
				Opp->SetActorRotation((Me->GetActorLocation() - At).GetSafeNormal2D().Rotation());
				Opp->InputPressed(ETaGInput::Primary);
				UE_LOG(LogTaG, Display, TEXT("SNAP threat from %s: action=%d"), *Side, static_cast<int32>(Opp->GetAction()));
			}
		}
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
