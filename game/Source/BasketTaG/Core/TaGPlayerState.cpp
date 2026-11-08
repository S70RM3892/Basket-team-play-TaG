#include "Core/TaGPlayerState.h"
#include "Net/UnrealNetwork.h"

ATaGPlayerState::ATaGPlayerState()
{
	SetNetUpdateFrequency(10.f);
}

void ATaGPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATaGPlayerState, Team);
	DOREPLIFETIME(ATaGPlayerState, Hero);
	DOREPLIFETIME(ATaGPlayerState, bIsBotPlayer);
	DOREPLIFETIME(ATaGPlayerState, bHeroLocked);
	DOREPLIFETIME(ATaGPlayerState, Stats);
}

void ATaGPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (ATaGPlayerState* Other = Cast<ATaGPlayerState>(PlayerState))
	{
		Other->Team = Team;
		Other->Hero = Hero;
		Other->bIsBotPlayer = bIsBotPlayer;
	}
}
