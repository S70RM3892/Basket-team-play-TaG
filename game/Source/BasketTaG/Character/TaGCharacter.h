#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Core/TaGHeroData.h"
#include "Core/TaGRules.h"
#include "TaGCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTaGRigComponent;
class ATaGBall;
class ATaGPlayerState;
struct FInputActionValue;

/** What the player is doing. One state at a time; the rock-paper-scissors hands are states. */
UENUM(BlueprintType)
enum class ETaGAction : uint8
{
	None,
	Shoot,
	DriveStart,
	Drive,
	Gathered,
	DodgeStart,
	Dodge,
	DodgeRecover,
	TackleStart,
	Tackle,
	TackleRecover,
	ContestStart,
	Contest,
	Step,
	StealRecover,
	Stagger,
	Down,
	PassThrow,
	ArtStart,
	ArtActive,
	ArtRecover
};

/** Cooldown slots. */
UENUM(BlueprintType)
enum class ETaGSlot : uint8
{
	Drive,
	Dodge,
	Tackle,
	Contest,
	Step,
	Steal,
	Pass,
	Art1,
	Art2,
	Count UMETA(Hidden)
};

/** Input channels (one Gameplay Ability each; the meaning depends on offense / defense). */
UENUM(BlueprintType)
enum class ETaGInput : uint8
{
	None,
	Primary,     // LB / Q : drive (offense) / tackle (defense) / cut (off ball)
	Secondary,   // RB / E : dodge (offense) / contest, hold (defense)
	Step,        // B / Shift : spacing step
	PassSteal,   // X / F / right mouse : pass (offense) / steal (defense)
	Shoot,       // RT / left mouse : hold and release
	Art1,        // LT / C
	Art2,        // Y / V
	Count UMETA(Hidden)
};

USTRUCT()
struct FTaGActionRep
{
	GENERATED_BODY()

	UPROPERTY() ETaGAction Action = ETaGAction::None;
	UPROPERTY() uint8 Variant = 0;
	UPROPERTY() float ServerStart = 0.f;
	UPROPERTY() FVector_NetQuantizeNormal Dir = FVector::ForwardVector;
	UPROPERTY() float Length = 0.f;
	UPROPERTY() uint8 Counter = 0;
};

/**
 * A player (human or bot). Movement is the Character Movement Component (server-authoritative with client prediction);
 * the hands and arts are Gameplay Abilities that call the Input* functions below. The action state machine runs on
 * the owning client (prediction) and on the server (authority); the server alone resolves the rock-paper-scissors
 * outcomes, steals, passes and shots. Bots call the same Input* functions on the server.
 */
UCLASS()
class BASKETTAG_API ATaGCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATaGCharacter(const FObjectInitializer& ObjectInitializer);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------ Setup
	void SetupHero(ETaGHero InHero, int32 InTeam);
	ETaGHero GetHero() const { return Hero; }
	const FTaGHeroDef& GetHeroDef() const;
	int32 GetTeam() const { return Team; }
	ATaGPlayerState* GetTaGPlayerState() const;
	bool IsBot() const;
	FString GetDisplayName() const;

	// ------------------------------------------------ Queries
	FVector GetFeetLocation() const;
	FVector GetEyeLocation() const;
	FVector GetAimDirection() const;
	FVector GetBlockHandLocation() const;
	FVector GetBallHoldLocation(float Time) const;
	ATaGBall* FindBall() const;
	bool HasBall() const;
	/** 0 loose / no ball, 1 holding, 2 guarding (opponent holds). */
	int32 GetRole() const;
	ETaGAction GetAction() const { return Action; }
	float GetActionTime() const { return ActionT; }
	bool IsChargingShot() const { return Action == ETaGAction::Shoot; }
	float GetShotCharge() const { return ShotCharge; }
	ETaGShotType GetShotType() const { return ShotType; }
	bool IsContesting() const { return Action == ETaGAction::Contest; }
	bool IsContestStarting() const { return Action == ETaGAction::ContestStart; }
	bool IsSonicContest() const { return Action == ETaGAction::Contest && ActionVariant == static_cast<uint8>(ETaGArt::SonicWave); }
	bool IsDriving() const;
	bool IsDodging() const { return Action == ETaGAction::Dodge || Action == ETaGAction::DodgeStart; }
	bool IsDown() const { return Action == ETaGAction::Down; }
	bool IsActiveDefender() const;
	bool CanTouchBall() const;
	bool IsInZone() const;
	bool IsScoped() const;
	bool IsCatchAndShoot() const;
	/** Just caught a pass (catch and shoot window, everyone). */
	bool IsFreshCatch() const;
	bool IsTetheredWith(const ATaGCharacter* Other) const;
	bool IsForcedMove() const;
	bool IsPretelling(uint8& OutKind) const;
	float GetCooldownLeft(ETaGSlot Slot) const;
	float GetCooldownFraction(ETaGSlot Slot) const;
	float GetGuard() const { return Guard; }
	float GetMaxGuard() const;
	ATaGCharacter* FindNearestOpponent(float MaxDist = 1e9f) const;
	ATaGCharacter* FindBestPassTarget(const FVector& AimDir) const;
	float ServerNow() const;

	// ------------------------------------------------ Movement (read by UTaGMovementComponent)
	float GetDesiredMaxSpeed() const;
	float GetDesiredAcceleration() const;
	float GetDesiredBraking() const;

	// ------------------------------------------------ Inputs (abilities for humans, called directly by bots)
	bool CanUseInput(ETaGInput Input) const;
	void InputPressed(ETaGInput Input);
	void InputReleased(ETaGInput Input);
	void InputJump();
	/** Bots: say which way they want to go (used for drive / dodge / step directions and shot types). */
	void SetBotWishDir(const FVector& Dir) { BotWish = Dir.GetSafeNormal2D(); }
	/** Bots: weaker bots show their defensive choice for a moment first (prototype v0.2). */
	void SetPretell(uint8 Kind, float Seconds);

	// ------------------------------------------------ Server: outcomes (called by the game mode / ball / arts)
	void KnockDown(const FVector& Push);
	void Stagger(float Seconds, const FVector& Push = FVector::ZeroVector);
	void ForceRecover(float Seconds);
	void Revive();
	void GiveZone();
	void ApplyGuardDamage(float Amount);
	void ApplySlow(float SpeedFactor, float ActionFactor, float Seconds);
	void AddSpeedBoost(float Seconds);
	void ResetHandCooldowns();
	void OnCaughtPass();
	void OnBallGained();
	void OnBallLost();
	void ClearForNewPossession(const FVector& FeetPos, const FVector& LookAt);
	void SetTether(ATaGCharacter* Partner, float Seconds);
	/** Called when an incoming tackle met this player's dodge. */
	void MarkEvaded() { bDodgeEvaded = true; }
	/** A tackle met the shooter mid-shot: the shot goes up with contact (a draw in the RPS table). */
	void ForceContactShot();
	float GetDriveCancelTime() const { return DriveCancelT; }
	void ConsumeZone() { ZoneEnd = 0.f; }
	bool HadBallOnDrive() const { return bHadBallOnDrive; }
	bool UsedAfterimage() const { return Action == ETaGAction::Dodge && ActionVariant == static_cast<uint8>(ETaGArt::Afterimage); }

	/** Bots: turn the view toward a point. */
	void FaceTowards(const FVector& Target, float DeltaSeconds, float Speed);

	// ------------------------------------------------ Camera
	void SetThirdPerson(bool bTPS);
	bool IsThirdPerson() const { return bThirdPerson; }
	/** Local camera shake (hits, read losses); decays on its own. */
	void AddCameraKick(float Strength) { KickAmp = FMath::Max(KickAmp, Strength); }

	// ------------------------------------------------ Cosmetic events (server -> everyone)
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFx(uint8 Kind, FVector_NetQuantize A, FVector_NetQuantize B, ATaGCharacter* Other);

	/** Shot type chosen for the current shot (also replicated as the action variant). */
	ETaGShotType ClassifyShot() const;

protected:
	// ------------------------------------------------ Components
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTaGRigComponent> Rig;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FPArmL;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FPArmR;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ContestField;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAbilitySystemComponent> AbilitySystem;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FPGlowL;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FPGlowR;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ContestMat;

	// ------------------------------------------------ Replicated state
	UPROPERTY(ReplicatedUsing = OnRep_HeroTeam) ETaGHero Hero = ETaGHero::Rei;
	UPROPERTY(ReplicatedUsing = OnRep_HeroTeam) int32 Team = 0;
	UPROPERTY(ReplicatedUsing = OnRep_NetAction) FTaGActionRep NetAction;
	UPROPERTY(ReplicatedUsing = OnRep_Cooldowns) TArray<float> CooldownEndRep;
	UPROPERTY(Replicated) float Guard = 100.f;
	UPROPERTY(Replicated) float ZoneEnd = 0.f;
	UPROPERTY(Replicated) float ScopeEnd = 0.f;
	UPROPERTY(Replicated) float SpotUpEnd = 0.f;
	UPROPERTY(Replicated) float CatchShootEnd = 0.f;
	UPROPERTY(Replicated) float LastCatchTime = -100.f;
	UPROPERTY(Replicated) float SpeedBoostEnd = 0.f;
	UPROPERTY(Replicated) float SlowEnd = 0.f;
	UPROPERTY(Replicated) float SlowSpeedFactor = 1.f;
	UPROPERTY(Replicated) float SlowActionFactor = 1.f;
	UPROPERTY(Replicated) float TetherEnd = 0.f;
	UPROPERTY(Replicated) TObjectPtr<ATaGCharacter> TetherPartner;
	UPROPERTY(Replicated) uint8 PretellKind = 0;     // 1 tackle, 2 contest, 3 drive, 4 shot (tutorial)
	UPROPERTY(Replicated) float PretellEnd = 0.f;

	UFUNCTION() void OnRep_HeroTeam();
	UFUNCTION() void OnRep_NetAction();
	UFUNCTION() void OnRep_Cooldowns();

	// ------------------------------------------------ Local state
	ETaGAction Action = ETaGAction::None;
	uint8 ActionVariant = 0;
	float ActionT = 0.f;
	float ActionLength = 0.f;
	FVector ActionDir = FVector::ForwardVector;
	float PredictUntil = 0.f;
	float CooldownEnd[static_cast<int32>(ETaGSlot::Count)] = {};
	float CooldownLocalSet[static_cast<int32>(ETaGSlot::Count)] = {};
	float CooldownTotal[static_cast<int32>(ETaGSlot::Count)] = {};

	ETaGShotType ShotType = ETaGShotType::Jumper;
	float ShotCharge = 0.f;
	bool bShotJumped = false;
	bool bShotContact = false;
	float ShotSide = 1.f;
	float SinceDrive = 9.f;
	float DriveCancelT = 0.f;      // just after a drive, a shot still counts as the drive (loses to tackle)
	bool bDriveBeatDone = false;
	bool bTackleResolved = false;
	bool bDodgeEvaded = false;
	bool bHadBallOnDrive = false;
	bool bContestHeld = false;
	bool bStopJumpNextTick = false;
	float DribbleT = 0.f;
	float LastGuardHit = -100.f;
	FVector BotWish = FVector::ZeroVector;
	FVector LastMoveInput = FVector::ZeroVector;
	TSet<TWeakObjectPtr<ATaGCharacter>> ArtHits;
	bool bThirdPerson = false;
	bool bAbilitiesGiven = false;
	bool bVisualsBuilt = false;
	float LandCheckZ = 0.f;
	float KickAmp = 0.f;
	float KickT = 0.f;

	/** The local player's own state changes (knocked down, staggered): rumble and camera kick. */
	void OnLocalActionChanged(ETaGAction NewAction);

	// ------------------------------------------------ Internals
	void SetAction(ETaGAction NewAction, uint8 Variant = 0, const FVector& Dir = FVector::ForwardVector, float Length = 0.f);
	void StartCooldown(ETaGSlot Slot, float Seconds);
	bool IsReady(ETaGSlot Slot) const;
	FVector WishDir() const;
	float ActionFactor() const;
	void TickAction(float Dt);
	void TickServerChecks(float Dt);
	void TickVisuals(float Dt);
	void ApplyHeroBody();
	void BuildVisuals();
	void InitAbilities();

	void StartDrive(bool bWithBall);
	void StartDodge(bool bAfterimage);
	void StartTackle();
	void StartContest();
	void ReleaseContest();
	void StartStep();
	void TrySteal();
	void TryPass(bool bThread);
	void BeginShot();
	void ReleaseShot();
	void UseArt(int32 Index);
	bool CanUseArt(int32 Index) const;
	void ExecuteArt(ETaGArt Art);
	void ArtStartFinished();
	void FinishBlink();
	void FirePull();
	FVector ShotMotion(float& OutSpeed) const;

	// Input handlers
	void OnMove(const FInputActionValue& Value);
	void OnLookMouse(const FInputActionValue& Value);
	void OnLookStick(const FInputActionValue& Value);
	void OnJumpPressed();
	void OnJumpReleased();
	void OnAbilityPressed(ETaGInput Input);
	void OnAbilityReleased(ETaGInput Input);
	void OnPausePressed();
	void OnCameraToggle();
	void AddLook(float YawDeg, float PitchDeg);
};
