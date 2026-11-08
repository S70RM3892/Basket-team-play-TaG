#include "Core/TaGGameState.h"
#include "Core/TaGAudio.h"
#include "Core/TaGPlayerState.h"
#include "Core/TaGStage.h"
#include "Core/TaGTutorial.h"
#include "Core/TaGTypes.h"
#include "Character/TaGCharacter.h"
#include "Character/TaGPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ATaGGameState::ATaGGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ATaGGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATaGGameState, Phase);
	DOREPLIFETIME(ATaGGameState, TeamSize);
	DOREPLIFETIME(ATaGGameState, BotSkill);
	DOREPLIFETIME(ATaGGameState, bOnline);
	DOREPLIFETIME(ATaGGameState, Score0);
	DOREPLIFETIME(ATaGGameState, Score1);
	DOREPLIFETIME(ATaGGameState, ShotClock);
	DOREPLIFETIME(ATaGGameState, PossessionTeam);
	DOREPLIFETIME(ATaGGameState, bNeedsClear0);
	DOREPLIFETIME(ATaGGameState, bNeedsClear1);
	DOREPLIFETIME(ATaGGameState, WinnerTeam);
	DOREPLIFETIME(ATaGGameState, MatchTime);
	DOREPLIFETIME(ATaGGameState, TeamStats0);
	DOREPLIFETIME(ATaGGameState, TeamStats1);
	DOREPLIFETIME(ATaGGameState, bTutorial);
	DOREPLIFETIME(ATaGGameState, TutorialStep);
	DOREPLIFETIME(ATaGGameState, TutorialProgress);
	DOREPLIFETIME(ATaGGameState, TutorialGoal);
}

bool ATaGGameState::IsTutorialFinished() const
{
	return bTutorial && Phase == ETaGPhase::Playing && TutorialStep >= TaGTutorial::Done;
}

namespace
{
	ATaGPlayerController* LocalTaGPC(const UWorld* World)
	{
		return World ? Cast<ATaGPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}
}

void ATaGGameState::BeginPlay()
{
	Super::BeginPlay();
	// The stage is static scenery: every machine builds its own copy (nothing to replicate).
	if (GetNetMode() != NM_DedicatedServer)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Stage = GetWorld()->SpawnActor<ATaGStage>(ATaGStage::StaticClass(), FTransform::Identity, Params);
		TaGAudio::PlayAmbient(this);
	}
}

void ATaGGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// UI timers run on real time so the read-win slow motion does not stretch them.
	const float Dt = FApp::GetDeltaTime();
	for (int32 i = Popups.Num() - 1; i >= 0; --i)
	{
		Popups[i].TimeLeft -= Dt;
		if (Popups[i].TimeLeft <= 0.f)
		{
			Popups.RemoveAt(i);
		}
	}
	Banner.TimeLeft = FMath::Max(0.f, Banner.TimeLeft - Dt);
	FlashTime = FMath::Max(0.f, FlashTime - Dt);

	// Music follows the screen: the calm bay loop for menus and lessons, the Tetsuba beat for matches.
	if (GetNetMode() != NM_DedicatedServer)
	{
		const bool bMatch = (Phase == ETaGPhase::Playing || Phase == ETaGPhase::Results) && !bTutorial;
		TaGAudio::PlayMusic(this, bMatch ? FName(TEXT("MusicMatch")) : FName(TEXT("MusicMenu")));
	}
}

int32 ATaGGameState::GetLocalTeam() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const ATaGPlayerState* PS = PC->GetPlayerState<ATaGPlayerState>())
			{
				if (PS->Team >= 0)
				{
					return PS->Team;
				}
			}
		}
	}
	return 0;
}

FLinearColor ATaGGameState::TeamColorFor(int32 Team) const
{
	if (Team < 0)
	{
		return TaG::GoldColor();
	}
	return Team == GetLocalTeam() ? TaG::AllyColor() : TaG::EnemyColor();
}

void ATaGGameState::AddLocalPopup(const FText& Text, int32 Team)
{
	FTaGPopup P;
	P.Text = Text;
	P.Team = Team;
	P.TimeLeft = 1.7f;
	Popups.Insert(P, 0);
	if (Popups.Num() > 4)
	{
		Popups.SetNum(4);
	}
}

void ATaGGameState::MulticastPopup_Implementation(const FText& Text, int32 Team)
{
	AddLocalPopup(Text, Team);
}

void ATaGGameState::MulticastReadWin_Implementation(int32 InWinnerTeam, ATaGCharacter* Winner, ATaGCharacter* Loser, const FText& Rule)
{
	const int32 Local = GetLocalTeam();
	const bool bGood = InWinnerTeam == Local;
	Banner.Title = FText::FromString(bGood ? TEXT("読み勝ち！") : TEXT("読まれた…"));
	Banner.Rule = Rule;
	Banner.bGood = bGood;
	Banner.TimeLeft = 1.3f;
	FlashTime = 0.25f;
	FlashColor = bGood ? FLinearColor(0.3f, 1.f, 0.9f, 0.35f) : FLinearColor(1.f, 0.25f, 0.15f, 0.3f);
	TaGAudio::Play2D(this, bGood ? TEXT("ReadWin") : TEXT("ReadLose"), 0.9f);
	// The two players in the read feel it: a short tap for the winner, a heavy hit for the one who was read.
	if (ATaGPlayerController* PC = LocalTaGPC(GetWorld()))
	{
		const APawn* Mine = PC->GetPawn();
		if (Mine && Mine == Winner)
		{
			PC->Feedback(0.35f, 0.12f, 0.35f);
		}
		else if (Mine && Mine == Loser)
		{
			PC->Feedback(0.8f, 0.28f, 1.f);
		}
	}
}

void ATaGGameState::MulticastSfx_Implementation(FName Sound, FVector_NetQuantize Location, float Volume)
{
	TaGAudio::PlayAt(this, Sound, Location, Volume);
}

void ATaGGameState::MulticastBasket_Implementation(int32 Team, int32 Points)
{
	if (Stage)
	{
		Stage->OnBasket(Team, GetScore(Team));
	}
	if (ATaGPlayerController* PC = LocalTaGPC(GetWorld()))
	{
		if (PC->GetPawn() && Team == GetLocalTeam())
		{
			PC->Feedback(0.3f, 0.1f, 0.f);
		}
	}
}

void ATaGGameState::OnRep_Phase()
{
	if (Stage)
	{
		Stage->SetCityLights(Score0, Score1, false);
	}
}

void ATaGGameState::OnRep_Score()
{
	if (Stage)
	{
		Stage->SetCityLights(Score0, Score1, true);
	}
}
