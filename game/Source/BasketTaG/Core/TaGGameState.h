#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TaGGameState.generated.h"

class ATaGStage;
class ATaGCharacter;

UENUM(BlueprintType)
enum class ETaGPhase : uint8
{
	Title,    // local title screen (no match)
	Lobby,    // private lobby / hero select
	Playing,
	Results
};

/** A short message on screen ("Block!", "Perfect release!"). Local only. */
struct FTaGPopup
{
	FText Text;
	int32 Team = -1;      // -1 neutral; otherwise colored ally / enemy relative to the viewer
	float TimeLeft = 0.f;
};

/** The big read-win banner ("読み勝ち！" / "読まれた…"). Local only. */
struct FTaGBanner
{
	FText Title;
	FText Rule;
	bool bGood = true;
	float TimeLeft = 0.f;
};

/** Team totals shown on the results screen and printed by the autotest (RULES_V1 section 2.4 metrics). */
USTRUCT()
struct FTaGTeamStats
{
	GENERATED_BODY()

	UPROPERTY() int32 Possessions = 0;
	UPROPERTY() int32 Points = 0;
	UPROPERTY() int32 IsoPossessions = 0;   // ended without a pass
	UPROPERTY() int32 IsoPoints = 0;
	UPROPERTY() int32 TeamPossessions = 0;  // at least one pass
	UPROPERTY() int32 TeamPoints = 0;
	UPROPERTY() int32 Shots = 0;
	UPROPERTY() int32 Makes = 0;
	UPROPERTY() float ExpectedMakes = 0.f;
	UPROPERTY() int32 ReadWins = 0;
	UPROPERTY() int32 Steals = 0;
	UPROPERTY() int32 Blocks = 0;
	UPROPERTY() int32 Turnovers = 0;
	UPROPERTY() int32 Passes = 0;
	UPROPERTY() int32 Interceptions = 0;
	UPROPERTY() int32 ShotClockViolations = 0;
	UPROPERTY() int32 Perfects = 0;
	UPROPERTY() int32 ArtsUsed = 0;
};

/** State everyone sees: phase, settings, score, shot clock, possession and the 3x3 clear rule. */
UCLASS()
class BASKETTAG_API ATaGGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ATaGGameState();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Phase) ETaGPhase Phase = ETaGPhase::Title;
	UPROPERTY(Replicated) int32 TeamSize = 1;    // 1on1, 2on2, 3on3
	UPROPERTY(Replicated) int32 BotSkill = 1;    // 0 practice, 1 normal, 2 strong, 3 demon
	UPROPERTY(Replicated) bool bOnline = false;
	UPROPERTY(ReplicatedUsing = OnRep_Score) int32 Score0 = 0;
	UPROPERTY(ReplicatedUsing = OnRep_Score) int32 Score1 = 0;
	UPROPERTY(Replicated) float ShotClock = 12.f;
	UPROPERTY(Replicated) int32 PossessionTeam = 0;
	UPROPERTY(Replicated) bool bNeedsClear0 = false;
	UPROPERTY(Replicated) bool bNeedsClear1 = false;
	UPROPERTY(Replicated) int32 WinnerTeam = -1;
	UPROPERTY(Replicated) float MatchTime = 0.f;
	UPROPERTY(Replicated) FTaGTeamStats TeamStats0;
	UPROPERTY(Replicated) FTaGTeamStats TeamStats1;

	// --- Tutorial (TaGTutorial.h): the lesson being played and how far along it is
	UPROPERTY(Replicated) bool bTutorial = false;
	UPROPERTY(Replicated) int32 TutorialStep = 0;
	UPROPERTY(Replicated) int32 TutorialProgress = 0;
	UPROPERTY(Replicated) int32 TutorialGoal = 1;
	bool IsTutorialFinished() const;

	int32 GetScore(int32 Team) const { return Team == 0 ? Score0 : Score1; }
	bool NeedsClear(int32 Team) const { return Team == 0 ? bNeedsClear0 : bNeedsClear1; }
	FTaGTeamStats& Stats(int32 Team) { return Team == 0 ? TeamStats0 : TeamStats1; }
	const FTaGTeamStats& Stats(int32 Team) const { return Team == 0 ? TeamStats0 : TeamStats1; }

	/** Team of the local viewer (the first local player); 0 when spectating. */
	int32 GetLocalTeam() const;
	FLinearColor TeamColorFor(int32 Team) const;

	// --- Events (server -> everyone) ---
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPopup(const FText& Text, int32 Team);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastReadWin(int32 InWinnerTeam, ATaGCharacter* Winner, ATaGCharacter* Loser, const FText& Rule);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSfx(FName Sound, FVector_NetQuantize Location, float Volume);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBasket(int32 Team, int32 Points);

	const TArray<FTaGPopup>& GetPopups() const { return Popups; }
	const FTaGBanner& GetBanner() const { return Banner; }
	float GetFlash() const { return FlashTime; }
	FLinearColor GetFlashColor() const { return FlashColor; }
	ATaGStage* GetStage() const { return Stage; }

	/** Local-only helper for one-off popups (no network). */
	void AddLocalPopup(const FText& Text, int32 Team);

protected:
	UFUNCTION() void OnRep_Phase();
	UFUNCTION() void OnRep_Score();

	TArray<FTaGPopup> Popups;
	FTaGBanner Banner;
	float FlashTime = 0.f;
	FLinearColor FlashColor = FLinearColor::White;

	UPROPERTY()
	TObjectPtr<ATaGStage> Stage;
};
