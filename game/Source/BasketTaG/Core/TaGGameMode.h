#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TaGGameMode.generated.h"

class ATaGBall;
class ATaGCharacter;
class ATaGGameState;

/** Match statistics, printed by the autotest (RULES_V1 section 7: measure points per possession). */
struct FTaGMatchStats
{
	int32 Possessions[2] = { 0, 0 };
	int32 Points[2] = { 0, 0 };
	int32 Shots[2] = { 0, 0 };
	int32 Makes[2] = { 0, 0 };
	int32 Perfect[2] = { 0, 0 };
	int32 Blocks[2] = { 0, 0 };
	int32 Steals[2] = { 0, 0 };
	int32 ShotClockViolations[2] = { 0, 0 };
	float ExpectedMakes[2] = { 0.f, 0.f };
};

/**
 * Server-only rules for P1: FIBA 3x3 half court, 1 point inside / 2 outside, first to 11,
 * 12 s shot clock, clear the ball beyond the arc after a change of possession.
 * Command-line test modes: -TaGShotTest, -TaGAutoTest, -TaGSnap.
 */
UCLASS()
class BASKETTAG_API ATaGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATaGGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	// Queries
	bool IsLive() const { return bLive; }
	bool NeedsClear(int32 Team) const;
	float GetShotClockLeft() const;

	// Events from the ball and the players
	void ResolveShot(ATaGCharacter* Shooter, float Charge);
	bool ResolveSteal(ATaGCharacter* Stealer);
	void OnBasket(ATaGCharacter* Shooter, int32 Points, bool bValid);
	void OnShotMissed(bool bTouchedRim);
	void OnBlocked(ATaGCharacter* Blocker);
	void OnBallPickedUp(ATaGCharacter* Picker);
	void OnOutOfBounds(int32 LastTouchTeam);
	void OnStuckBall(int32 LastTouchTeam);
	void RestartMatch();

protected:
	UPROPERTY()
	TObjectPtr<ATaGBall> Ball;

	UPROPERTY()
	TArray<TObjectPtr<ATaGCharacter>> Players; // index = team

	bool bLive = false;
	bool bShotInFlight = false;
	float DeadTimeLeft = 0.f;
	int32 NextPossessionTeam = 0;
	FTaGMatchStats Stats;

	bool bAutoTest = false;
	bool bShotTest = false;
	bool bSnap = false;
	float SnapTimer = 0.f;
	int32 SnapStage = 0;
	float MatchTime = 0.f;

	ATaGGameState* GS() const;
	ATaGCharacter* SpawnBot(int32 Team, float Skill, const FVector& Feet);
	void RegisterPlayer(ATaGCharacter* C, int32 Team);
	void ScheduleTurnover(int32 ToTeam, float Delay);
	void StartPossession(int32 Team);
	void SetNeedsClear(int32 Team, bool bValue);
	void Popup(const FString& Text, int32 Team);
	void EndMatch(int32 Winner);
	void LogStats() const;
	void RunShotTest();
	void TickSnap(float Dt);
};
