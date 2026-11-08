#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ball/TaGBallSim.h"
#include "TaGBall.generated.h"

class ATaGCharacter;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ETaGBallState : uint8
{
	Dead,    // between possessions
	Held,
	Flight,  // a shot on its way up / to the rim
	Loose,   // rebound, block, steal, bad bounce
};

/**
 * The ball. The server owns its state; flight uses FTaGBallSim instead of engine physics (TECH.md section 3).
 */
UCLASS()
class BASKETTAG_API ATaGBall : public AActor
{
	GENERATED_BODY()

public:
	ATaGBall();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Server API ---
	void GiveTo(ATaGCharacter* NewHolder);
	void LaunchShot(const FVector& From, const FVector& Velocity, ATaGCharacter* Shooter, int32 Points, bool bValid);
	void MakeLoose(const FVector& From, const FVector& Velocity, int32 TouchTeam);
	void SetDead();

	ETaGBallState GetBallState() const { return State; }
	ATaGCharacter* GetHolder() const { return Holder; }
	ATaGCharacter* GetShooter() const { return Shooter; }
	float GetFlightTime() const { return FlightTime; }
	int32 GetLastTouchTeam() const { return LastTouchTeam; }

	/** Where the ball sits for a holder this frame (dribble bob and shot pocket are cosmetic). */
	static FVector HoldPoint(const ATaGCharacter* Who, float Time);

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Replicated)
	ETaGBallState State = ETaGBallState::Dead;

	UPROPERTY(Replicated)
	TObjectPtr<ATaGCharacter> Holder;

	UPROPERTY(Replicated)
	TObjectPtr<ATaGCharacter> Shooter;

	UPROPERTY(Replicated)
	int32 LastTouchTeam = -1;

	FTaGBallSim Sim;
	float FlightTime = 0.f;
	float LooseTime = 0.f;
	float NoPickupTime = 0.f;
	int32 ShotPoints = 1;
	bool bShotValid = true;
	bool bTouchedRim = false;

	void TickAuthority(float Dt);
	void TryPickup();
	void CheckBlocks();
};
