#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ball/TaGBallSim.h"
#include "TaGBall.generated.h"

class ATaGCharacter;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class ETaGBallState : uint8
{
	Dead,    // between possessions
	Held,
	Flight,  // a shot on its way to the rim
	Pass,    // a pass on its way to a teammate
	Loose    // rebound, block, deflection
};

/** Network snapshot of the ball. Clients rebuild the flight from it and simulate locally (no jitter). */
USTRUCT()
struct FTaGBallNet
{
	GENERATED_BODY()

	UPROPERTY() ETaGBallState State = ETaGBallState::Dead;
	UPROPERTY() TObjectPtr<ATaGCharacter> Holder;
	UPROPERTY() TObjectPtr<ATaGCharacter> Receiver;
	UPROPERTY() FVector_NetQuantize10 Pos = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantize10 Vel = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantize10 Target = FVector::ZeroVector;
	UPROPERTY() float ServerTime = 0.f;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() bool bThread = false;
	UPROPERTY() uint8 Seq = 0;
};

/**
 * The ball ("core ball" in the lore: it glows brighter as it travels through teammates' hands).
 * The server owns its state; flight uses FTaGBallSim instead of engine physics (TECH.md section 3).
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
	void LaunchShot(const FVector& From, const FVector& Velocity, ATaGCharacter* Shooter, int32 Points, bool bValid, float BlockMult);
	void LaunchPass(ATaGCharacter* Passer, ATaGCharacter* Receiver, bool bThread);
	void MakeLoose(const FVector& From, const FVector& Velocity, int32 TouchTeam);
	void SetDead();
	void SetChain(int32 InChain) { Chain = InChain; }

	ETaGBallState GetBallState() const { return Net.State; }
	ATaGCharacter* GetHolder() const { return Net.Holder; }
	ATaGCharacter* GetShooter() const { return Shooter.Get(); }
	ATaGCharacter* GetPassReceiver() const { return Net.Receiver; }
	float GetFlightTime() const { return FlightTime; }
	int32 GetLastTouchTeam() const { return LastTouchTeam; }
	int32 GetShotTeam() const { return ShotTeam; }
	FVector GetSimVelocity() const { return Sim.Vel; }
	int32 GetChain() const { return Chain; }

	/** Where the ball sits in the holder's hands this frame (cosmetic). */
	FVector HoldPoint(const ATaGCharacter* Who) const;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlowMat;

	UPROPERTY(ReplicatedUsing = OnRep_Net) FTaGBallNet Net;
	UPROPERTY(Replicated) int32 LastTouchTeam = -1;
	UPROPERTY(Replicated) int32 ShotTeam = -1;
	UPROPERTY(Replicated) int32 Chain = 0;

	UFUNCTION() void OnRep_Net();

	TWeakObjectPtr<ATaGCharacter> Shooter;
	TWeakObjectPtr<ATaGCharacter> Passer;
	FTaGBallSim Sim;
	FVector PassFrom = FVector::ZeroVector;
	float FlightTime = 0.f;
	float LooseTime = 0.f;
	float NoPickupTime = 0.f;
	float SyncTimer = 0.f;
	int32 ShotPoints = 1;
	bool bShotValid = true;
	bool bTouchedRim = false;
	float ShotBlockMult = 1.f;
	TSet<TWeakObjectPtr<ATaGCharacter>> BlockRolled;
	TSet<TWeakObjectPtr<ATaGCharacter>> PassRolled;
	float SoundCooldown = 0.f;

	// Cosmetic trail behind shots and passes: a small ring of segments reused over and over (no spawning).
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TrailSegs;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> TrailMats;
	TArray<float> TrailAge;
	TArray<FVector> TrailA;
	TArray<FVector> TrailB;
	int32 TrailNext = 0;
	float TrailTimer = 0.f;
	FVector TrailLast = FVector::ZeroVector;
	bool bTrailActive = false;
	void TickTrail(float Dt);

	void PushNet();
	void TickAuthority(float Dt);
	void TickPass(float Dt, bool bAuthority);
	void TryPickup();
	void CheckBlocks();
	void PlayContactSounds(uint8 Contacts);
	FVector PassPosition(float T) const;
};
