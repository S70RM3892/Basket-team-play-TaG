#include "Core/TaGGameState.h"
#include "Core/TaGCourt.h"
#include "Net/UnrealNetwork.h"

ATaGGameState::ATaGGameState()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATaGGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATaGGameState, Score0);
	DOREPLIFETIME(ATaGGameState, Score1);
	DOREPLIFETIME(ATaGGameState, ShotClock);
	DOREPLIFETIME(ATaGGameState, PossessionTeam);
	DOREPLIFETIME(ATaGGameState, bNeedsClear0);
	DOREPLIFETIME(ATaGGameState, bNeedsClear1);
	DOREPLIFETIME(ATaGGameState, bMatchOver);
	DOREPLIFETIME(ATaGGameState, WinnerTeam);
}

void ATaGGameState::BeginPlay()
{
	Super::BeginPlay();
	// The court is static scenery: every machine builds its own copy (nothing to replicate).
	if (GetNetMode() != NM_DedicatedServer)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Court = GetWorld()->SpawnActor<ATaGCourt>(ATaGCourt::StaticClass(), FTransform::Identity, Params);
	}
}

void ATaGGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (int32 i = Popups.Num() - 1; i >= 0; --i)
	{
		Popups[i].TimeLeft -= DeltaSeconds;
		if (Popups[i].TimeLeft <= 0.f)
		{
			Popups.RemoveAt(i);
		}
	}
}

void ATaGGameState::MulticastPopup_Implementation(const FText& Text, FLinearColor Color)
{
	FTaGPopup P;
	P.Text = Text;
	P.Color = Color;
	P.TimeLeft = 1.6f;
	Popups.Insert(P, 0);
	if (Popups.Num() > 4)
	{
		Popups.SetNum(4);
	}
}
