#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TaGBotController.generated.h"

class ATaGCharacter;

/**
 * P1 bot: plain basketball only (drive to a spot, shoot, guard between the handler and the rim,
 * contest, block, reach for steals, chase rebounds). No navigation mesh: it steers the pawn directly.
 */
UCLASS()
class BASKETTAG_API ATaGBotController : public AAIController
{
	GENERATED_BODY()

public:
	ATaGBotController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;

	/** 0 = practice .. 1 = strong. Affects release timing, contest reactions and steal attempts. */
	float Skill = 0.5f;

protected:
	void TickOffense(ATaGCharacter* Me, float Dt);
	void TickDefense(ATaGCharacter* Me, ATaGCharacter* Handler, float Dt);
	void TickChase(ATaGCharacter* Me, const FVector& BallPos, float Dt);
	void SteerTo(ATaGCharacter* Me, const FVector& Target, float Accuracy = 40.f);
	void PickOffenseSpot(ATaGCharacter* Me);

	FVector OffenseSpot = FVector::ZeroVector;
	bool bHasSpot = false;
	float HoldTime = 0.f;
	float ShootAfter = 1.5f;
	float ReleaseAt = 0.74f;
	bool bReactedToShot = false;
	float NextStealTry = 0.f;
	bool bHadBall = false;
	bool bBlockDecided = false;
};
