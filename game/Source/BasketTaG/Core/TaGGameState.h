#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TaGGameState.generated.h"

class ATaGCourt;

/** A short message on screen ("Block!", "Perfect release!"). Local only. */
struct FTaGPopup
{
	FText Text;
	FLinearColor Color;
	float TimeLeft = 0.f;
};

/** State everyone sees: score, shot clock, possession and the 3x3 clear rule. */
UCLASS()
class BASKETTAG_API ATaGGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ATaGGameState();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated) int32 Score0 = 0;
	UPROPERTY(Replicated) int32 Score1 = 0;
	UPROPERTY(Replicated) float ShotClock = 12.f;
	UPROPERTY(Replicated) int32 PossessionTeam = 0;
	UPROPERTY(Replicated) bool bNeedsClear0 = false;
	UPROPERTY(Replicated) bool bNeedsClear1 = false;
	UPROPERTY(Replicated) bool bMatchOver = false;
	UPROPERTY(Replicated) int32 WinnerTeam = -1;

	int32 GetScore(int32 Team) const { return Team == 0 ? Score0 : Score1; }
	bool NeedsClear(int32 Team) const { return Team == 0 ? bNeedsClear0 : bNeedsClear1; }

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPopup(const FText& Text, FLinearColor Color);

	const TArray<FTaGPopup>& GetPopups() const { return Popups; }

protected:
	TArray<FTaGPopup> Popups;

	UPROPERTY()
	TObjectPtr<ATaGCourt> Court;
};
