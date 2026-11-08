#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Core/TaGHeroData.h"
#include "TaGBotController.generated.h"

class ATaGCharacter;
class ATaGBall;
enum class ETaGInput : uint8;

/**
 * Bot: the Godot prototype's 1on1 brain (habitual mixed strategy + reactions, weak bots telegraph their defense)
 * extended to team play: spacing spots, cuts, passes to the open teammate, man-to-man defense with help,
 * rebounds, and each hero's arts. No navigation mesh: it steers the pawn directly. Runs on the server.
 */
UCLASS()
class BASKETTAG_API ATaGBotController : public AAIController
{
	GENERATED_BODY()

public:
	ATaGBotController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;

	/** 0 = practice .. 1 = demon. Affects timing spread, reactions, read frequency and telegraphs. */
	float Skill = 0.5f;

	/** The habit shown at the end of a match (prototype v0.2: "answer check"). */
	FString HabitText() const;

	/**
	 * Tutorial lessons (TaGTutorial.h) replace the brain with a script: stand aside, or show one clear tell
	 * (red contest, orange tackle, cyan drive, gold shot) and then do it, so the player can answer it.
	 */
	enum class EScript : uint8 { Normal, Idle, ContestOnClose, TackleOnClose, DriveAttack, ShootAttack };
	void SetScript(EScript InScript);

	/** Tests: cut to the rim now. */
	void TestCut();

protected:
	EScript Script = EScript::Normal;
	float ScriptT = 0.f;
	int32 ScriptPhase = 0;
	float ScriptSide = 1.f;
	void TickScript(ATaGCharacter* Me, ATaGBall* Ball, float Dt);

	enum class EHand : uint8 { Tackle, Contest, Wait, Steal, Drive, Dodge, Shoot };

	void Offense(ATaGCharacter* Me, ATaGBall* Ball, float Dt);
	void OffBall(ATaGCharacter* Me, ATaGCharacter* Handler, float Dt);
	void DefendBall(ATaGCharacter* Me, ATaGCharacter* Handler, float Dt);
	void DefendOffBall(ATaGCharacter* Me, ATaGCharacter* Man, ATaGCharacter* Handler, float Dt);
	void Loose(ATaGCharacter* Me, ATaGBall* Ball, float Dt);

	bool TryPass(ATaGCharacter* Me, bool bPressured);
	/** A teammate running at the rim with nobody at the catch spot: throw it up for an alley-oop. */
	bool TryAlleyOop(ATaGCharacter* Me, float Dt);
	void PlanShot(ATaGCharacter* Me, float Nd, float DefDist);
	void StartShot(ATaGCharacter* Me);
	void Commit(ATaGCharacter* Me, EHand Hand);
	void Do(ATaGCharacter* Me, EHand Hand);
	bool Press(ATaGCharacter* Me, ETaGInput Input);
	void Steer(ATaGCharacter* Me, const FVector& Target, float Accuracy = 40.f, float Scale = 1.f);
	void Wish(ATaGCharacter* Me, const FVector& Dir);
	void Face(ATaGCharacter* Me, const FVector& Point, float Dt, float Rate);
	ATaGCharacter* MyMan(ATaGCharacter* Me) const;
	int32 TeamIndex(ATaGCharacter* Me) const;
	float OpenNess(const ATaGCharacter* Who) const;
	EHand Pick(const TMap<EHand, float>& Weights) const;
	bool UseArtIf(ATaGCharacter* Me, int32 Index, bool bCondition);
	/** Data-driven arts (TaGHeroData): use one whose AI hint matches the situation, at RatePerSec. */
	bool TryArts(ATaGCharacter* Me, ETaGArtAI Situation, float RatePerSec, float Dt);

	EHand DefFav = EHand::Tackle;
	EHand OffFav = EHand::Drive;
	float ThinkT = 0.f;
	float Strafe = 1.f;
	float ReleaseAt = 0.8f;
	float ContestHold = 0.f;
	float FadeWait = 0.f;
	bool bDashShotRolled = false;
	uint8 ReactedTo = 255;
	bool bBlockDecided = false;
	float CutTimer = 3.f;
	float CuttingFor = 0.f;
	float HoldTime = 0.f;
	bool bHadBall = false;
	bool bTetherUsed = false;
	bool bOopDefDecided = false;
	bool bAttackRim = false;   // this possession: run an open lane all the way in for a layup
	float LeapSlack = 0.f;     // this bot jumps for a lob a little early or late (less so when skilled)

	struct FQueued
	{
		float T;
		EHand Hand;
	};
	TArray<FQueued> Queue;
};
