#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/TaGHeroData.h"
#include "TaGPlayerState.generated.h"

USTRUCT()
struct FTaGPlayerStats
{
	GENERATED_BODY()

	UPROPERTY() int32 Points = 0;
	UPROPERTY() int32 Shots = 0;
	UPROPERTY() int32 Makes = 0;
	UPROPERTY() int32 Assists = 0;
	UPROPERTY() int32 Steals = 0;
	UPROPERTY() int32 Blocks = 0;
	UPROPERTY() int32 Rebounds = 0;
	UPROPERTY() int32 ReadWins = 0;
	UPROPERTY() int32 ReadLosses = 0;
};

/** Per-player choices (team, hero) and match statistics. Bots get one too. */
UCLASS()
class BASKETTAG_API ATaGPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ATaGPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

	UPROPERTY(Replicated) int32 Team = -1;
	UPROPERTY(Replicated) ETaGHero Hero = ETaGHero::Rei;
	UPROPERTY(Replicated) bool bIsBotPlayer = false;
	UPROPERTY(Replicated) bool bHeroLocked = false;
	UPROPERTY(Replicated) FTaGPlayerStats Stats;

	void ResetStats() { Stats = FTaGPlayerStats(); }
};
