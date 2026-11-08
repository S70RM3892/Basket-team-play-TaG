#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "TaGCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UStaticMeshComponent;
class ATaGBall;
struct FInputActionValue;

/**
 * A player (human or bot). Movement is the engine's Character Movement Component (server-authoritative
 * with client prediction); shoot / block / steal are Gameplay Abilities that call into this class.
 */
UCLASS()
class BASKETTAG_API ATaGCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATaGCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Team / queries ---
	void SetTeam(int32 InTeam);
	int32 GetTeam() const { return Team; }
	FVector GetFeetLocation() const;
	FVector GetEyeLocation() const;
	FVector GetBlockHandLocation() const;
	bool HasBall() const;
	bool IsChargingShot() const { return bCharging; }
	float GetChargeTime() const { return ChargeTime; }
	bool IsBlocking() const { return BlockTimeLeft > 0.f; }
	bool IsRecovering() const { return RecoverTimeLeft > 0.f; }
	ATaGCharacter* FindOpponent() const;
	ATaGBall* FindBall() const;

	// --- Actions (called by abilities for humans, directly by bots) ---
	bool CanStartShot() const;
	void BeginShot();
	/** Ends the charge; the server resolves the shot. Safe to call more than once. */
	void ReleaseShot();
	bool CanBlock() const;
	void StartBlock();
	bool CanSteal() const;
	void StartSteal();

	/** Puts the player on a spot facing the rim (used between possessions). */
	void PlaceAt(const FVector& FeetPos, const FVector& LookAt);

	/** Bots and lock-on: turn toward a point. */
	void FaceTowards(const FVector& Target, float DeltaSeconds, float Speed);

protected:
	UPROPERTY(VisibleAnywhere, Category = "TaG")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "TaG")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, Category = "TaG")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "TaG")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, Category = "TaG")
	TObjectPtr<UStaticMeshComponent> RingMesh;

	UPROPERTY(ReplicatedUsing = OnRep_Team)
	int32 Team = 0;

	UPROPERTY(Replicated)
	bool bCharging = false;

	float ChargeTime = 0.f;
	bool bAutoJumped = false;
	float BlockTimeLeft = 0.f;
	float NextBlockTime = 0.f;
	float NextStealTime = 0.f;

	UPROPERTY(Replicated)
	float RecoverTimeLeft = 0.f;

	bool bAbilitiesGiven = false;

	UFUNCTION()
	void OnRep_Team();

	bool bStopJumpNextTick = false;

	void InitAbilities();
	void ApplyTeamColor();
	void UpdateMoveSpeed();
	void ResolveShotOnServer(float Charge);

	// Input
	void OnMove(const FInputActionValue& Value);
	void OnLookMouse(const FInputActionValue& Value);
	void OnLookStick(const FInputActionValue& Value);
	void OnJumpPressed();
	void OnJumpReleased();
	void OnShootPressed();
	void OnShootReleased();
	void OnBlockPressed();
	void OnStealPressed();
	void OnRestartPressed();
	void AddLook(float YawDeg, float PitchDeg);
};
