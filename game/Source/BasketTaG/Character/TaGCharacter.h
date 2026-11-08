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
	/** Server: open the catch-and-shoot window (curved passes, lantern marks). */
	void GrantCatchAndShoot(float Seconds) { CatchShootEnd = ServerNow() + Seconds; }
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

	// ------------------------------------------------ Art status effects (TaGCharacterArts.cpp)
	bool IsInvulnerable() const { return ServerNow() < InvulnEnd; }      // shell: tackles, steals and pulls bounce off
	bool IsPhasing() const { return ServerNow() < PhaseEnd; }            // intangible: tackles and steals miss
	bool IsRooted() const { return ServerNow() < RootEnd; }              // can't move
	bool IsRootedWall() const { return ServerNow() < RootWallEnd; }      // Kokemaru: unmovable, contests shots around
	float GetRootWallRadius() const { return RootWallRadius; }
	bool HasCounter() const { return ServerNow() < CounterEnd; }
	bool IsShotDebuffed() const { return ServerNow() < ShotDebuffEnd; }
	bool HasInterceptBoost() const { return ServerNow() < InterceptEnd; }
	/** Steal / block reach multiplier (blinded by sparks: 0.5). */
	float GetReachFactor() const { return ServerNow() < BlindEnd ? 0.5f : 1.f; }
	/** The shot modifier waiting for the next shot (ETaGArtFx::None if none). */
	ETaGArtFx GetShotMod() const;
	void ConsumeShotMod();
	/** The art behind the current tackle (Arrest, ShieldBash) or None. */
	ETaGArt GetTackleArt() const { return TackleArt; }
	/** Server: a tackle reached this player while a counter was up. Returns the counter used (None if none). */
	ETaGArtFx ConsumeCounter();
	/** Server: catch with a lantern / data-share mark: returns the chain bonus (0 if none). */
	int32 ConsumeMark();
	/** Server: an art of another player lands on this one (or on a teammate for team buffs). */
	void ApplyArtStatus(ETaGArtFx Fx, float Duration, float Power);
	/** The contest-stance art held right now (Gulp) or nullptr. */
	const FTaGArtDef* GetContestArt() const;
	bool IsShielding() const { return ServerNow() < ShieldEnd; }

	/** Bots: turn the view toward a point. */
	void FaceTowards(const FVector& Target, float DeltaSeconds, float Speed);

	// ------------------------------------------------ Camera
	void SetThirdPerson(bool bTPS);
	/** The first-person camera's step motion in world space (things held in view move with it). */
	FVector GetViewBobOffset() const { return ViewBobWorld; }
	bool IsThirdPerson() const { return bThirdPerson; }
	UTaGRigComponent* GetRig() const { return Rig; }
	/** Local camera shake (hits, read losses); decays on its own. */
	void AddCameraKick(float Strength) { KickAmp = FMath::Max(KickAmp, Strength); }

	// ------------------------------------------------ Cosmetic events (server -> everyone)
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFx(uint8 Kind, FVector_NetQuantize A, FVector_NetQuantize B, ATaGCharacter* Other);

	/** A generic art's visual: Stage 0 cast (A = user), 1 hit (B = target), 2 bounced / read (B = target). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastArtFx(uint8 ArtId, uint8 Stage, FVector_NetQuantize A, FVector_NetQuantize B);

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
	/** Dark disc on the floor (TaGArt::MakeBlobShadow): the toon materials receive no shadows. */
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BlobShadow;
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
	// Art status effects
	UPROPERTY(Replicated) float InvulnEnd = 0.f;
	UPROPERTY(Replicated) float PhaseEnd = 0.f;
	UPROPERTY(Replicated) float RootEnd = 0.f;
	UPROPERTY(Replicated) float RootWallEnd = 0.f;
	UPROPERTY(Replicated) float RootWallRadius = 0.f;
	UPROPERTY(Replicated) float CounterEnd = 0.f;
	UPROPERTY(Replicated) uint8 CounterKind = 0;
	UPROPERTY(Replicated) float ShotModEnd = 0.f;
	UPROPERTY(Replicated) uint8 ShotMod = 0;
	UPROPERTY(Replicated) float BlindEnd = 0.f;
	UPROPERTY(Replicated) float ShotDebuffEnd = 0.f;
	UPROPERTY(Replicated) float InterceptEnd = 0.f;
	UPROPERTY(Replicated) float MarkEnd = 0.f;
	UPROPERTY(Replicated) uint8 MarkPower = 0;
	UPROPERTY(Replicated) float RegenEnd = 0.f;
	UPROPERTY(Replicated) float ShieldEnd = 0.f;

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
	int32 LastStepCount = 0;
	float BoomBaseZ = 81.f;               // first-person eye height above the capsule centre, before the step bob
	FVector PrevVisVel = FVector::ZeroVector;
	float SqueakCD = 0.f;
	int32 FootSfx = 0;
	bool bWasOnFloorVis = true;
	float VisYaw = 0.f;                   // where the drawn body faces; turns toward the actor's yaw over a few frames
	bool bVisYawInit = false;
	FVector ViewBobWorld = FVector::ZeroVector; // what the first-person camera moves with the steps, world space
	float AmbientT = 0.f;
	float ZoneFxT = 0.f;
	/** Particles that make each hero recognisable at a glance (sparks, fireflies, frost, steam...). */
	void TickHeroAmbience(float Dt);

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
	void FinishBlink(float Distance = 450.f);
	void FirePull();

	// Generic arts (TaGCharacterArts.cpp)
	ETaGArt TackleArt = ETaGArt::None;
	uint8 PendingPassStyle = 0;   // 2 curve, 3 lob (set around TryPass by the pass arts)
	float SkateTrailT = 0.f;
	bool bPhaseCollision = false;
	bool CanUseGenericArt(const FTaGArtDef& Def) const;
	void ExecuteGenericArt(const FTaGArtDef& Def);
	/** ArtStart finished its startup for a generic art: fire it. */
	void FireGenericArt(const FTaGArtDef& Def);
	void StartArtDrive(const FTaGArtDef& Def);
	/** Server: the art's effect on one opponent (handles dodges, shells and drives as reads). */
	void HitWithArt(ATaGCharacter* Target, const FTaGArtDef& Def, const FVector& Push);
	/** Server, every tick: drive trails (sparks, ice), shields, phase collision. */
	void TickArtStatus(float Dt);
	TArray<ATaGCharacter*> TeamInRadius(float Radius, bool bIncludeSelf) const;
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
