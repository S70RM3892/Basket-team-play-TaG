#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/TaGHeroData.h"
#include "TaGGameMode.generated.h"

class ATaGBall;
class ATaGCharacter;
class ATaGGameState;
class ATaGPlayerState;
class ATaGBotController;
class APlayerController;

/**
 * Server-only rules: FIBA 3x3 half court, 1 point inside / 2 outside, first to 11, 12 s shot clock,
 * clear the ball beyond the arc after a change of possession; the rock-paper-scissors outcomes (RULES_V1 section 3);
 * the private lobby (host + friends by IP, bots fill the empty slots).
 * Command-line test modes: -TaGShotTest, -TaGAutoTest[=team size], -TaGSnap=<shot>.
 */
UCLASS()
class BASKETTAG_API ATaGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATaGGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	// ------------------------------------------------ Lobby / flow (called from player controllers)
	void EnterLobby(int32 TeamSize, int32 BotSkill);
	void SetLobbyOptions(int32 TeamSize, int32 BotSkill);
	void SetPlayerHero(APlayerController* PC, ETaGHero Hero);
	void SetPlayerTeam(APlayerController* PC, int32 Team);
	void StartMatch();
	void RestartMatch();
	void BackToLobby();
	bool IsLive() const { return bLive; }
	bool IsTestMode() const { return bAutoTest || bShotTest; }

	// ------------------------------------------------ Queries
	bool NeedsClear(int32 Team) const;
	float GetShotClockLeft() const;
	TArray<ATaGCharacter*> GetTeamPlayers(int32 Team) const;

	// ------------------------------------------------ Outcomes (from the characters, the ball and the arts)
	void ResolveShot(ATaGCharacter* Shooter, float Charge, bool bContact);
	bool ResolveSteal(ATaGCharacter* Stealer);
	void ResolveTackle(ATaGCharacter* Tackler, ATaGCharacter* Target, bool bCharge);
	void OnDriveBeatsContest(ATaGCharacter* Driver, ATaGCharacter* Contester);
	void OnDriveEnd(ATaGCharacter* Driver);
	void OnDodgeWhiff(ATaGCharacter* Dodger);
	void OnPullHit(ATaGCharacter* Puller, ATaGCharacter* Target);
	void OnGuardBreak(ATaGCharacter* Victim);
	void OnArtUsed(ATaGCharacter* User, ETaGArt Art);
	void LaunchPass(ATaGCharacter* Passer, ATaGCharacter* Receiver, bool bThread);
	void OnPassCaught(ATaGCharacter* Receiver);
	void OnPassIntercepted(ATaGCharacter* Defender);
	void OnBasket(ATaGCharacter* Shooter, int32 Points, bool bValid);
	void OnShotMissed(bool bTouchedRim);
	void OnBlocked(ATaGCharacter* Blocker);
	void OnBallPickedUp(ATaGCharacter* Picker, bool bFromPass);
	void OnOutOfBounds(int32 LastTouchTeam);
	void OnStuckBall(int32 LastTouchTeam);
	void ReadWin(ATaGCharacter* Winner, ATaGCharacter* Loser, const FString& Rule);
	void Popup(const FString& Text, int32 Team);
	void Sfx(FName Sound, const FVector& Where, float Volume = 1.f);

protected:
	UPROPERTY() TObjectPtr<ATaGBall> Ball;
	UPROPERTY() TArray<TObjectPtr<ATaGCharacter>> Players;
	UPROPERTY() TArray<TObjectPtr<ATaGBotController>> Bots;

	bool bLive = false;
	bool bShotInFlight = false;
	float DeadTimeLeft = 0.f;
	int32 NextPossessionTeam = 0;
	bool bRestartAfterDead = false;

	// Possession tracking (assists, iso vs team play)
	int32 PossessionPasses = 0;
	TWeakObjectPtr<ATaGCharacter> LastPasser;
	float LastCatchTime = -100.f;
	TWeakObjectPtr<ATaGCharacter> LastCatcher;
	bool bLastShotTeamPlay = false;

	// Read-win slow motion (real time)
	double HitstopUntil = 0.0;
	double SlowmoUntil = 0.0;

	// Test modes
	bool bAutoTest = false;
	bool bShotTest = false;
	FString SnapMode;
	float SnapTimer = 0.f;
	int32 SnapStage = 0;
	int32 AutoTestSize = 1;
	bool bWantLobbyOnStart = false;
	bool bListenServer = false;

	ATaGGameState* GS() const;
	ATaGCharacter* SpawnPlayerCharacter(AController* C, ETaGHero Hero, int32 Team, const FVector& Feet);
	void ClearPlayers();
	void AssignTeams();
	void ScheduleTurnover(int32 ToTeam, float Delay);
	void StartPossession(int32 Team);
	void EndPossession(int32 Team, int32 Points);
	void SetNeedsClear(int32 Team, bool bValue);
	void EndMatch(int32 Winner);
	void LogStats() const;
	void RunShotTest();
	void TickSnap(float Dt);
	void TickSlowmo();
	ATaGPlayerState* PS(ATaGCharacter* C) const;
};
