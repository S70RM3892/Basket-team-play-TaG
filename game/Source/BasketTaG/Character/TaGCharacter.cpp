#include "Character/TaGCharacter.h"
#include "BasketTaG.h"
#include "Abilities/TaGAbilities.h"
#include "Abilities/TaGTags.h"
#include "Ball/TaGBall.h"
#include "Character/TaGArtActors.h"
#include "Character/TaGMovementComponent.h"
#include "Character/TaGPlayerController.h"
#include "Character/TaGRig.h"
#include "Core/TaGArtLib.h"
#include "Core/TaGAudio.h"
#include "Core/TaGFx.h"
#include "Core/TaGGameMode.h"
#include "Core/TaGGameState.h"
#include "Core/TaGPlayerState.h"
#include "Core/TaGTypes.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr float MouseDegPerCount = 0.12f;
	constexpr float StickYawRate = 210.f;
	constexpr float StickPitchRate = 150.f;
	constexpr int32 NumSlots = static_cast<int32>(ETaGSlot::Count);

	enum EFx : uint8
	{
		FxBlink = 1,
		FxAfterimage,
		FxPullBeam,
		FxTether,
		FxThread,
		FxRevive,
		FxArtCue
	};

	bool IsImposed(ETaGAction A)
	{
		switch (A)
		{
		case ETaGAction::Down:
		case ETaGAction::Stagger:
		case ETaGAction::StealRecover:
		case ETaGAction::TackleRecover:
		case ETaGAction::DodgeRecover:
		case ETaGAction::ArtRecover:
			return true;
		default:
			return false;
		}
	}

	FName ActionSound(ETaGAction A)
	{
		switch (A)
		{
		case ETaGAction::DriveStart: return TEXT("Drive");
		case ETaGAction::DodgeStart: return TEXT("Dodge");
		case ETaGAction::TackleStart: return TEXT("TackleCue");
		case ETaGAction::ContestStart: return TEXT("ContestHum");
		case ETaGAction::Step: return TEXT("Step");
		case ETaGAction::Down: return TEXT("Down");
		case ETaGAction::ArtStart: return TEXT("ArtCue");
		case ETaGAction::PassThrow: return TEXT("Pass");
		default: return NAME_None;
		}
	}
}

ATaGCharacter::ATaGCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTaGMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(25.f, 94.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 175.f - 94.f));
	CameraBoom->TargetArmLength = 0.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(95.f);

	Rig = CreateDefaultSubobject<UTaGRigComponent>(TEXT("Rig"));
	Rig->SetupAttachment(GetCapsuleComponent());
	Rig->SetRelativeLocation(FVector(0.f, 0.f, -94.f));

	FPArmL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FPArmL"));
	FPArmR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FPArmR"));
	for (UStaticMeshComponent* A : { FPArmL.Get(), FPArmR.Get() })
	{
		A->SetupAttachment(Camera);
		A->SetOnlyOwnerSee(true);
		A->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		A->SetCastShadow(false);
	}

	ContestField = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContestField"));
	ContestField->SetupAttachment(GetCapsuleComponent());
	ContestField->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ContestField->SetCastShadow(false);
	ContestField->SetVisibility(false);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->JumpZVelocity = TaG::JumpVelocity;
	Move->GravityScale = 1.f;
	Move->AirControl = 0.25f;
	Move->GroundFriction = 8.f;
	Move->bOrientRotationToMovement = false;
	Move->NavAgentProps.bCanCrouch = false;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AutoPossessAI = EAutoPossessAI::Disabled;
	CooldownEndRep.SetNum(NumSlots);
}

void ATaGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATaGCharacter, Hero);
	DOREPLIFETIME(ATaGCharacter, Team);
	DOREPLIFETIME(ATaGCharacter, NetAction);
	DOREPLIFETIME(ATaGCharacter, CooldownEndRep);
	DOREPLIFETIME(ATaGCharacter, Guard);
	DOREPLIFETIME(ATaGCharacter, ZoneEnd);
	DOREPLIFETIME(ATaGCharacter, ScopeEnd);
	DOREPLIFETIME(ATaGCharacter, SpotUpEnd);
	DOREPLIFETIME(ATaGCharacter, CatchShootEnd);
	DOREPLIFETIME(ATaGCharacter, LastCatchTime);
	DOREPLIFETIME(ATaGCharacter, SpeedBoostEnd);
	DOREPLIFETIME(ATaGCharacter, SlowEnd);
	DOREPLIFETIME(ATaGCharacter, SlowSpeedFactor);
	DOREPLIFETIME(ATaGCharacter, SlowActionFactor);
	DOREPLIFETIME(ATaGCharacter, TetherEnd);
	DOREPLIFETIME(ATaGCharacter, TetherPartner);
	DOREPLIFETIME(ATaGCharacter, PretellKind);
	DOREPLIFETIME(ATaGCharacter, PretellEnd);
	DOREPLIFETIME(ATaGCharacter, InvulnEnd);
	DOREPLIFETIME(ATaGCharacter, PhaseEnd);
	DOREPLIFETIME(ATaGCharacter, RootEnd);
	DOREPLIFETIME(ATaGCharacter, RootWallEnd);
	DOREPLIFETIME(ATaGCharacter, RootWallRadius);
	DOREPLIFETIME(ATaGCharacter, CounterEnd);
	DOREPLIFETIME(ATaGCharacter, CounterKind);
	DOREPLIFETIME(ATaGCharacter, ShotModEnd);
	DOREPLIFETIME(ATaGCharacter, ShotMod);
	DOREPLIFETIME(ATaGCharacter, BlindEnd);
	DOREPLIFETIME(ATaGCharacter, ShotDebuffEnd);
	DOREPLIFETIME(ATaGCharacter, InterceptEnd);
	DOREPLIFETIME(ATaGCharacter, MarkEnd);
	DOREPLIFETIME(ATaGCharacter, MarkPower);
	DOREPLIFETIME(ATaGCharacter, RegenEnd);
	DOREPLIFETIME(ATaGCharacter, ShieldEnd);
}

UAbilitySystemComponent* ATaGCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

const FTaGHeroDef& ATaGCharacter::GetHeroDef() const
{
	return TaGHeroes::Get(Hero);
}

ATaGPlayerState* ATaGCharacter::GetTaGPlayerState() const
{
	return GetPlayerState<ATaGPlayerState>();
}

bool ATaGCharacter::IsBot() const
{
	return !IsPlayerControlled() && (HasAuthority() || (GetTaGPlayerState() && GetTaGPlayerState()->bIsBotPlayer));
}

FString ATaGCharacter::GetDisplayName() const
{
	const ATaGPlayerState* PS = GetTaGPlayerState();
	if (PS && !PS->bIsBotPlayer)
	{
		return FString::Printf(TEXT("%s（%s）"), *PS->GetPlayerName(), GetHeroDef().Name);
	}
	return GetHeroDef().Name;
}

float ATaGCharacter::ServerNow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	const AGameStateBase* GS = World->GetGameState();
	return GS ? static_cast<float>(GS->GetServerWorldTimeSeconds()) : World->GetTimeSeconds();
}

// ================================================================ Setup

void ATaGCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (CooldownEndRep.Num() != NumSlots)
	{
		CooldownEndRep.SetNum(NumSlots);
	}
	ApplyHeroBody();
	BuildVisuals();
}

void ATaGCharacter::SetupHero(ETaGHero InHero, int32 InTeam)
{
	Hero = InHero;
	Team = InTeam;
	Guard = GetHeroDef().Guard;
	ApplyHeroBody();
	BuildVisuals();
}

void ATaGCharacter::OnRep_HeroTeam()
{
	ApplyHeroBody();
	BuildVisuals();
}

void ATaGCharacter::ApplyHeroBody()
{
	const FTaGHeroDef& H = GetHeroDef();
	UCapsuleComponent* Cap = GetCapsuleComponent();
	const float OldHalf = Cap->GetScaledCapsuleHalfHeight();
	if (!FMath::IsNearlyEqual(OldHalf, H.CapsuleHalfHeight) || !FMath::IsNearlyEqual(Cap->GetScaledCapsuleRadius(), H.CapsuleRadius))
	{
		Cap->SetCapsuleSize(H.CapsuleRadius, H.CapsuleHalfHeight);
		if (HasAuthority())
		{
			AddActorWorldOffset(FVector(0.f, 0.f, H.CapsuleHalfHeight - OldHalf));
		}
	}
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, H.EyeHeight - H.CapsuleHalfHeight));
	Rig->SetRelativeLocation(FVector(0.f, 0.f, -H.CapsuleHalfHeight));
	GetCharacterMovement()->JumpZVelocity = TaG::JumpVelocity * H.JumpMul;
	ContestField->SetRelativeLocation(FVector(90.f, 0.f, H.EyeHeight * 0.75f - H.CapsuleHalfHeight));
	ContestField->SetRelativeScale3D(FVector(0.06f, 2.f, 2.3f * H.EyeHeight / 175.f));
}

void ATaGCharacter::BuildVisuals()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FTaGHeroDef& H = GetHeroDef();
	Rig->Build(H);
	Rig->SetOwnerNoSeeAll(!bThirdPerson);

	// First-person forearms: the hero's own lower arms (Gantetsu's are huge) in front of the camera.
	const FString Base = FString::Printf(TEXT("/Game/Art/Heroes/%s/SM_%s_"), H.Key, H.Key);
	UStaticMesh* LowerL = TaGArt::LoadMesh(Base + TEXT("LowerArm_L"));
	UStaticMesh* LowerR = TaGArt::LoadMesh(Base + TEXT("LowerArm_R"));
	const float S = H.EyeHeight / 175.f;
	// A new hero brings new meshes: drop the material overrides painted for the previous one
	FPArmL->EmptyOverrideMaterials();
	FPArmR->EmptyOverrideMaterials();
	if (LowerL && LowerR)
	{
		FPArmL->SetStaticMesh(LowerL);
		FPArmR->SetStaticMesh(LowerR);
		FPArmL->SetRelativeScale3D(FVector(1.f));
		FPArmR->SetRelativeScale3D(FVector(1.f));
		FPGlowL = FPArmL->GetMaterialIndex(TEXT("TaG")) != INDEX_NONE ? TaGArt::PaintSlot(FPArmL, TEXT("TaG"), FLinearColor::Gray) : nullptr;
		FPGlowR = FPArmR->GetMaterialIndex(TEXT("TaG")) != INDEX_NONE ? TaGArt::PaintSlot(FPArmR, TEXT("TaG"), FLinearColor::Gray) : nullptr;
	}
	else
	{
		for (UStaticMeshComponent* A : { FPArmL.Get(), FPArmR.Get() })
		{
			A->SetStaticMesh(TaGArt::EngineCylinder());
			A->SetRelativeScale3D(FVector(0.1f * S, 0.1f * S, 0.32f * S));
		}
		FPGlowL = TaGArt::PaintSlot(FPArmL, NAME_None, H.SecondaryColor);
		FPGlowR = TaGArt::PaintSlot(FPArmR, NAME_None, H.SecondaryColor);
	}
	FPArmL->SetVisibility(false);
	FPArmR->SetVisibility(false);

	ContestField->SetStaticMesh(TaGArt::EngineCube());
	ContestMat = TaGArt::PaintSlot(ContestField, NAME_None, FLinearColor(1.f, 0.2f, 0.15f), 3.f, true, 0.18f);
	if (!BlobShadow)
	{
		BlobShadow = TaGArt::MakeBlobShadow(this);
	}
	bVisualsBuilt = true;
}

void ATaGCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	AbilitySystem->InitAbilityActorInfo(this, this);
	InitAbilities();
}

void ATaGCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	AbilitySystem->InitAbilityActorInfo(this, this);
}

void ATaGCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	AbilitySystem->InitAbilityActorInfo(this, this);
}

void ATaGCharacter::InitAbilities()
{
	if (!HasAuthority() || bAbilitiesGiven)
	{
		return;
	}
	bAbilitiesGiven = true;
	for (uint8 i = 1; i < static_cast<uint8>(ETaGInput::Count); ++i)
	{
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(UTaGInputAbility::StaticClass(), 1, i, this));
	}
}

// ================================================================ Queries

FVector ATaGCharacter::GetFeetLocation() const
{
	return GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

FVector ATaGCharacter::GetEyeLocation() const
{
	return GetFeetLocation() + FVector(0.f, 0.f, GetHeroDef().EyeHeight);
}

FVector ATaGCharacter::GetAimDirection() const
{
	return GetBaseAimRotation().Vector();
}

FVector ATaGCharacter::GetBlockHandLocation() const
{
	const FTaGHeroDef& H = GetHeroDef();
	if (IsPlayerControlled())
	{
		return GetEyeLocation() + GetAimDirection() * 55.f * H.ReachMul + FVector(0.f, 0.f, 25.f);
	}
	return GetEyeLocation() + GetActorForwardVector() * 30.f + FVector(0.f, 0.f, 55.f * H.ReachMul);
}

FVector ATaGCharacter::GetBallHoldLocation(float Time) const
{
	const float S = GetHeroDef().EyeHeight / 175.f;
	const FVector Feet = GetFeetLocation();
	const FVector Fwd = GetActorForwardVector();
	const FVector Right = GetActorRightVector();
	if (Action == ETaGAction::Shoot)
	{
		return Feet + Fwd * 30.f * S + FVector(0.f, 0.f, 205.f * S);
	}
	if (Action == ETaGAction::Gathered || Action == ETaGAction::PassThrow)
	{
		return Feet + Fwd * 40.f * S + FVector(0.f, 0.f, 120.f * S);
	}
	const float Bob = FMath::Abs(FMath::Sin(Time * 7.5f));
	FVector P = Feet + Fwd * 42.f * S + Right * 22.f * S;
	P.Z = Feet.Z + TaG::BallRadius + 1.f + Bob * 90.f * S;
	return P;
}

ATaGBall* ATaGCharacter::FindBall() const
{
	TActorIterator<ATaGBall> It(GetWorld());
	return It ? *It : nullptr;
}

bool ATaGCharacter::HasBall() const
{
	const ATaGBall* Ball = FindBall();
	return Ball && Ball->GetBallState() == ETaGBallState::Held && Ball->GetHolder() == this;
}

int32 ATaGCharacter::GetRole() const
{
	const ATaGBall* Ball = FindBall();
	if (!Ball || Ball->GetBallState() != ETaGBallState::Held || !Ball->GetHolder())
	{
		return 0;
	}
	if (Ball->GetHolder() == this)
	{
		return 1;
	}
	return Ball->GetHolder()->GetTeam() != Team ? 2 : 0;
}

bool ATaGCharacter::IsDriving() const
{
	return Action == ETaGAction::Drive || Action == ETaGAction::DriveStart
		|| (Action == ETaGAction::Shoot && DriveCancelT > 0.f)
		|| (Action == ETaGAction::ArtStart && ActionVariant == static_cast<uint8>(ETaGArt::Blink));
}

bool ATaGCharacter::IsActiveDefender() const
{
	switch (Action)
	{
	case ETaGAction::Stagger:
	case ETaGAction::Down:
	case ETaGAction::TackleRecover:
	case ETaGAction::StealRecover:
	case ETaGAction::ArtRecover:
		return false;
	default:
		return true;
	}
}

bool ATaGCharacter::CanTouchBall() const
{
	return Action != ETaGAction::Down;
}

bool ATaGCharacter::IsInZone() const { return ServerNow() < ZoneEnd; }
bool ATaGCharacter::IsScoped() const { return ServerNow() < ScopeEnd; }
bool ATaGCharacter::IsCatchAndShoot() const { return ServerNow() < CatchShootEnd; }
bool ATaGCharacter::IsFreshCatch() const { return ServerNow() - LastCatchTime < 1.2f; }

bool ATaGCharacter::IsTetheredWith(const ATaGCharacter* Other) const
{
	if (!Other)
	{
		return false;
	}
	const float Now = ServerNow();
	return (TetherPartner == Other && Now < TetherEnd) || (Other->TetherPartner == this && Now < Other->TetherEnd);
}

bool ATaGCharacter::IsForcedMove() const
{
	switch (Action)
	{
	case ETaGAction::Drive:
	case ETaGAction::Dodge:
	case ETaGAction::Tackle:
	case ETaGAction::Step:
	case ETaGAction::ArtActive:
		return true;
	case ETaGAction::Gathered:
		return ActionT < TaG::GatherStepsT;
	case ETaGAction::Shoot:
	{
		float Speed = 0.f;
		ShotMotion(Speed);
		return true;
	}
	default:
		return false;
	}
}

bool ATaGCharacter::IsPretelling(uint8& OutKind) const
{
	OutKind = PretellKind;
	return PretellKind != 0 && ServerNow() < PretellEnd;
}

void ATaGCharacter::SetPretell(uint8 Kind, float Seconds)
{
	PretellKind = Kind;
	PretellEnd = ServerNow() + Seconds;
}

float ATaGCharacter::GetCooldownLeft(ETaGSlot Slot) const
{
	return FMath::Max(0.f, CooldownEnd[static_cast<int32>(Slot)] - ServerNow());
}

float ATaGCharacter::GetCooldownFraction(ETaGSlot Slot) const
{
	const int32 i = static_cast<int32>(Slot);
	const float Total = FMath::Max(CooldownTotal[i], 0.01f);
	return FMath::Clamp(GetCooldownLeft(Slot) / Total, 0.f, 1.f);
}

float ATaGCharacter::GetMaxGuard() const
{
	return GetHeroDef().Guard;
}

bool ATaGCharacter::IsReady(ETaGSlot Slot) const
{
	return ServerNow() >= CooldownEnd[static_cast<int32>(Slot)];
}

void ATaGCharacter::StartCooldown(ETaGSlot Slot, float Seconds)
{
	const int32 i = static_cast<int32>(Slot);
	CooldownEnd[i] = ServerNow() + Seconds;
	CooldownTotal[i] = Seconds;
	CooldownLocalSet[i] = GetWorld()->GetTimeSeconds();
	if (HasAuthority())
	{
		if (CooldownEndRep.Num() != NumSlots)
		{
			CooldownEndRep.SetNum(NumSlots);
		}
		CooldownEndRep[i] = CooldownEnd[i];
	}
}

void ATaGCharacter::OnRep_Cooldowns()
{
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < FMath::Min(NumSlots, CooldownEndRep.Num()); ++i)
	{
		if (IsLocallyControlled() && Now - CooldownLocalSet[i] < 0.5f)
		{
			continue;
		}
		if (!FMath::IsNearlyEqual(CooldownEnd[i], CooldownEndRep[i]))
		{
			CooldownTotal[i] = FMath::Max(CooldownTotal[i], CooldownEndRep[i] - ServerNow());
			CooldownEnd[i] = CooldownEndRep[i];
		}
	}
}

ATaGCharacter* ATaGCharacter::FindNearestOpponent(float MaxDist) const
{
	ATaGCharacter* Best = nullptr;
	float BestD = MaxDist;
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->GetTeam() != Team)
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
			if (D < BestD)
			{
				BestD = D;
				Best = *It;
			}
		}
	}
	return Best;
}

ATaGCharacter* ATaGCharacter::FindBestPassTarget(const FVector& AimDir) const
{
	const FVector Aim = AimDir.GetSafeNormal2D();
	ATaGCharacter* Best = nullptr;
	float BestScore = -1e9f;
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->GetTeam() != Team || It->IsDown())
		{
			continue;
		}
		const FVector To = (It->GetActorLocation() - GetActorLocation());
		const float Dist = To.Size2D();
		const float Score = FVector::DotProduct(Aim, To.GetSafeNormal2D()) * 2.f - Dist / 1500.f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = *It;
		}
	}
	return Best;
}

// ================================================================ Movement

float ATaGCharacter::ActionFactor() const
{
	return ServerNow() < SlowEnd ? SlowActionFactor : 1.f;
}

float ATaGCharacter::GetDesiredMaxSpeed() const
{
	const FTaGHeroDef& H = GetHeroDef();
	const float Now = ServerNow();
	float Base = (HasBall() ? TaG::DribbleSpeed : TaG::WalkSpeed) * H.SpeedMul;
	if (Now < ZoneEnd) { Base *= 1.25f; }
	if (Now < TetherEnd) { Base *= 1.2f; }
	if (Now < SpeedBoostEnd) { Base *= 1.25f; }
	if (Now < SlowEnd) { Base *= SlowSpeedFactor; }
	if (Now < ShieldEnd) { Base *= 0.6f; }
	if (Now < RootEnd && Action != ETaGAction::Down)
	{
		return 0.f; // rooted (bound, shelled, rooted wall)
	}

	switch (Action)
	{
	case ETaGAction::Drive:
		if (ActionVariant > static_cast<uint8>(ETaGArt::Resync) && ActionVariant < static_cast<uint8>(ETaGArt::Count))
		{
			const FTaGArtDef& Art = TaGHeroes::GetArt(static_cast<ETaGArt>(ActionVariant));
			if (Art.Kind == ETaGArtKind::Leap)
			{
				return 1200.f; // the launch carries it, no braking mid-air
			}
			return TaG::DriveSpeed * H.SpeedMul * ActionFactor() * Art.Power;
		}
		return TaG::DriveSpeed * H.SpeedMul * ActionFactor();
	case ETaGAction::Dodge:
		return TaG::DodgeSpeed * 1.05f * ActionFactor();
	case ETaGAction::Tackle:
		return TaG::TackleSpeed * FMath::Sqrt(H.SpeedMul);
	case ETaGAction::ArtActive:
		return 800.f;
	case ETaGAction::Step:
		return TaG::StepSpeed * H.SpeedMul;
	case ETaGAction::Shoot:
	{
		float Speed = 0.f;
		ShotMotion(Speed);
		return Speed;
	}
	case ETaGAction::Gathered:
		return ActionT < TaG::GatherStepsT ? TaG::GatherStepSpeed : 0.f;
	case ETaGAction::ContestStart:
	case ETaGAction::Contest:
		return TaG::ContestMoveSpeed;
	case ETaGAction::Stagger:
		return TaG::StaggerMoveSpeed;
	case ETaGAction::PassThrow:
		return Base * 0.6f;
	case ETaGAction::None:
		return Base;
	default:
		return 0.f; // startups and recoveries: you are stuck
	}
}

float ATaGCharacter::GetDesiredAcceleration() const
{
	if (IsForcedMove())
	{
		return 8000.f;
	}
	const FTaGHeroDef& H = GetHeroDef();
	return TaG::Accel * H.AccelMul * (IsInZone() ? 1.2f : 1.f);
}

float ATaGCharacter::GetDesiredBraking() const
{
	if (Action != ETaGAction::None)
	{
		return 2400.f;
	}
	return TaG::Accel * GetHeroDef().AccelMul;
}

FVector ATaGCharacter::WishDir() const
{
	if (IsLocallyControlled() && IsPlayerControlled())
	{
		return LastMoveInput;
	}
	if (!IsPlayerControlled())
	{
		return BotWish;
	}
	return GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
}

FVector ATaGCharacter::ShotMotion(float& OutSpeed) const
{
	// Ported from athlete.gd _shot_motion (m/s -> cm/s)
	OutSpeed = 0.f;
	const FVector Feet = GetFeetLocation();
	FVector ToRim(TaG::RimX - Feet.X, -Feet.Y, 0.f);
	const float Dist = ToRim.Size();
	const FVector Tr = Dist > 1.f ? ToRim / Dist : GetActorForwardVector();
	const float T = ActionT;
	switch (ShotType)
	{
	case ETaGShotType::Layup:
		if (Dist < 70.f) { return FVector::ZeroVector; }
		OutSpeed = T < 0.55f ? 360.f : 160.f;
		return Tr;
	case ETaGShotType::Euro:
		if (T < 0.25f)
		{
			const FVector V = GetActorRightVector() * ShotSide * 360.f + Tr * 120.f;
			OutSpeed = V.Size();
			return V.GetSafeNormal();
		}
		if (Dist < 70.f) { return FVector::ZeroVector; }
		OutSpeed = T < 0.6f ? 360.f : 120.f;
		return Tr;
	case ETaGShotType::Dunk:
		if (Dist < 55.f) { return FVector::ZeroVector; }
		OutSpeed = 500.f;
		return Tr;
	case ETaGShotType::StepBack:
		if (T < 0.35f)
		{
			OutSpeed = 380.f;
			return -Tr;
		}
		return FVector::ZeroVector;
	case ETaGShotType::Fade:
		OutSpeed = 180.f;
		return -Tr;
	case ETaGShotType::Floater:
		if (Dist < 100.f) { return FVector::ZeroVector; }
		OutSpeed = 200.f;
		return Tr;
	default:
		return FVector::ZeroVector;
	}
}

// ================================================================ Action state

void ATaGCharacter::SetAction(ETaGAction NewAction, uint8 Variant, const FVector& Dir, float Length)
{
	const bool bChanged = NewAction != Action;
	Action = NewAction;
	ActionVariant = Variant;
	ActionT = 0.f;
	ActionDir = Dir.IsNearlyZero() ? GetActorForwardVector() : Dir.GetSafeNormal2D();
	ActionLength = Length;
	if (HasAuthority())
	{
		NetAction.Action = NewAction;
		NetAction.Variant = Variant;
		NetAction.ServerStart = ServerNow();
		NetAction.Dir = ActionDir;
		NetAction.Length = Length;
		NetAction.Counter++;
	}
	else if (IsLocallyControlled())
	{
		PredictUntil = GetWorld()->GetTimeSeconds() + 0.6f;
	}
	if (bChanged && GetNetMode() != NM_DedicatedServer)
	{
		const FName Sound = ActionSound(NewAction);
		if (!Sound.IsNone())
		{
			TaGAudio::PlayAt(this, Sound, GetActorLocation(), NewAction == ETaGAction::TackleStart ? 1.f : 0.7f);
		}
		OnLocalActionChanged(NewAction);
		if (NewAction == ETaGAction::Down)
		{
			TaGFx::Dust(this, GetFeetLocation(), 2.6f);
			TaGFx::Burst(this, GetEyeLocation(), FLinearColor(1.f, 0.9f, 0.4f), 8, 120.f, 0.8f, 7.f, -50.f);
		}
	}
}

void ATaGCharacter::OnLocalActionChanged(ETaGAction NewAction)
{
	if (!IsLocallyControlled() || !IsPlayerControlled())
	{
		return;
	}
	ATaGPlayerController* PC = Cast<ATaGPlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	switch (NewAction)
	{
	case ETaGAction::Down:
		PC->Feedback(1.f, 0.35f, 1.3f);
		break;
	case ETaGAction::Stagger:
		PC->Feedback(0.5f, 0.15f, 0.6f);
		break;
	default:
		break;
	}
}

void ATaGCharacter::OnRep_NetAction()
{
	const bool bImposed = IsImposed(NetAction.Action);
	if (IsLocallyControlled() && GetWorld()->GetTimeSeconds() < PredictUntil && !bImposed)
	{
		if (NetAction.Action == Action)
		{
			ActionVariant = NetAction.Variant;
			if (Action == ETaGAction::Shoot)
			{
				ShotType = static_cast<ETaGShotType>(NetAction.Variant);
			}
		}
		return;
	}
	const ETaGAction Old = Action;
	Action = NetAction.Action;
	ActionVariant = NetAction.Variant;
	ActionDir = FVector(NetAction.Dir);
	ActionLength = NetAction.Length;
	ActionT = FMath::Max(0.f, ServerNow() - NetAction.ServerStart);
	if (Action == ETaGAction::Shoot)
	{
		ShotType = static_cast<ETaGShotType>(ActionVariant);
		const FTaGShotTypeDef& St = FTaGRules::ShotType(ShotType);
		ShotCharge = ActionT / (St.Meter * TaG::ShotMeterSeconds);
		bShotJumped = St.JumpAt < 0.f || ActionT > St.JumpAt;
	}
	if (Old != Action && GetNetMode() != NM_DedicatedServer)
	{
		const FName Sound = ActionSound(Action);
		if (!Sound.IsNone())
		{
			TaGAudio::PlayAt(this, Sound, GetActorLocation(), 0.7f);
		}
		OnLocalActionChanged(Action);
		if (Action == ETaGAction::Down)
		{
			TaGFx::Dust(this, GetFeetLocation(), 2.6f);
			TaGFx::Burst(this, GetEyeLocation(), FLinearColor(1.f, 0.9f, 0.4f), 8, 120.f, 0.8f, 7.f, -50.f);
		}
	}
}

void ATaGCharacter::TickAction(float Dt)
{
	ActionT += Dt;
	SinceDrive += Dt;
	DriveCancelT = FMath::Max(0.f, DriveCancelT - Dt);
	const bool bAuth = HasAuthority();
	const bool bSim = bAuth || IsLocallyControlled();
	ATaGGameMode* GM = bAuth ? GetWorld()->GetAuthGameMode<ATaGGameMode>() : nullptr;
	const float AF = ActionFactor();

	switch (Action)
	{
	case ETaGAction::DriveStart:
		if (ActionT >= TaG::DriveStartT)
		{
			FVector Dir = WishDir();
			if (Dir.IsNearlyZero())
			{
				Dir = GetActorForwardVector();
			}
			bDriveBeatDone = false;
			// Art drives (sparks, skate, double beat) keep their art as the variant and stretch the length.
			const bool bArtDrive = ActionVariant > static_cast<uint8>(ETaGArt::Resync) && ActionVariant < static_cast<uint8>(ETaGArt::Count);
			const float LengthMul = bArtDrive ? FMath::Max(TaGHeroes::GetArt(static_cast<ETaGArt>(ActionVariant)).Duration, 0.5f) : 1.f;
			SetAction(ETaGAction::Drive, bArtDrive ? ActionVariant : 0, Dir, TaG::DriveT * AF * LengthMul);
		}
		break;
	case ETaGAction::Drive:
		SinceDrive = 0.f;
		if (ActionT >= (ActionLength > 0.f ? ActionLength : TaG::DriveT))
		{
			if (bAuth && GM)
			{
				GM->OnDriveEnd(this);
			}
			// Ushio: the double beat makes the drive ready again at once
			if (ActionVariant == static_cast<uint8>(ETaGArt::DoubleBeat))
			{
				CooldownEnd[static_cast<int32>(ETaGSlot::Drive)] = 0.f;
				if (bAuth && CooldownEndRep.IsValidIndex(static_cast<int32>(ETaGSlot::Drive)))
				{
					CooldownEndRep[static_cast<int32>(ETaGSlot::Drive)] = 0.f;
				}
			}
			if (bHadBallOnDrive && HasBall())
			{
				SetAction(ETaGAction::Gathered, 0, ActionDir); // gather rule: two steps, then pass or shoot
			}
			else
			{
				SetAction(ETaGAction::None);
			}
		}
		break;
	case ETaGAction::Gathered:
		if (!HasBall() && bSim)
		{
			SetAction(ETaGAction::None);
		}
		break;
	case ETaGAction::DodgeStart:
		if (ActionT >= TaG::DodgeStartT)
		{
			const FVector W = WishDir();
			float Side = FVector::DotProduct(W, GetActorRightVector());
			Side = FMath::Abs(Side) > 0.3f ? FMath::Sign(Side) : 1.f;
			const FVector Dir = (GetActorRightVector() * Side + GetActorForwardVector() * 0.3f).GetSafeNormal();
			bDodgeEvaded = false;
			ShotSide = Side;
			SetAction(ETaGAction::Dodge, ActionVariant, Dir, TaG::DodgeT * AF);
		}
		break;
	case ETaGAction::Dodge:
		if (ActionT >= TaG::DodgeT * AF)
		{
			if (bDodgeEvaded)
			{
				SetAction(ETaGAction::None);
			}
			else
			{
				const bool bAfter = ActionVariant == static_cast<uint8>(ETaGArt::Afterimage);
				SetAction(ETaGAction::DodgeRecover, 0, FVector::ZeroVector, bAfter ? 0.3f : TaG::DodgeRecoverT);
				if (bAuth && GM)
				{
					GM->OnDodgeWhiff(this);
				}
			}
		}
		break;
	case ETaGAction::TackleStart:
		if (ActionT >= TaG::TackleStartT)
		{
			bTackleResolved = false;
			SetAction(ETaGAction::Tackle, ActionVariant, GetActorForwardVector(), TaG::TackleRecoverT);
		}
		break;
	case ETaGAction::Tackle:
		if (ActionT >= TaG::TackleT)
		{
			TackleArt = ETaGArt::None;
			SetAction(ETaGAction::TackleRecover, 0, FVector::ZeroVector, bTackleResolved ? 0.3f : TaG::TackleRecoverT);
		}
		break;
	case ETaGAction::ContestStart:
		if (ActionT >= TaG::ContestStartT)
		{
			SetAction(ETaGAction::Contest, ActionVariant, FVector::ZeroVector, ActionLength);
		}
		break;
	case ETaGAction::Contest:
	{
		const bool bSonic = IsSonicContest();
		const bool bStance = GetContestArt() != nullptr;
		bool bRelease = bSonic ? ActionT >= 1.2f : ((!bContestHeld && ActionT > 0.25f) || ActionT >= TaG::ContestMaxT);
		if (bStance)
		{
			bRelease = ActionT >= ActionLength; // Ankou: the gulp is held for its full length
		}
		else if (!bSonic && GetRole() != 2 && ActionT > 0.5f)
		{
			bRelease = true;
		}
		if (bRelease && bSim)
		{
			if (!bSonic && !bStance)
			{
				StartCooldown(ETaGSlot::Contest, TaG::ContestCD);
			}
			SetAction(ETaGAction::None);
		}
		break;
	}
	case ETaGAction::Step:
		if (ActionT >= TaG::StepT)
		{
			SetAction(ETaGAction::None);
		}
		break;
	case ETaGAction::StealRecover:
		if (ActionT >= TaG::StealRecoverT)
		{
			SetAction(ETaGAction::None);
		}
		break;
	case ETaGAction::TackleRecover:
	case ETaGAction::DodgeRecover:
	case ETaGAction::Stagger:
	case ETaGAction::ArtRecover:
		if (ActionT >= ActionLength && bSim)
		{
			SetAction(ETaGAction::None);
		}
		break;
	case ETaGAction::Down:
		if (ActionT >= TaG::DownT && bAuth)
		{
			Guard = GetMaxGuard();
			SetAction(ETaGAction::None);
		}
		break;
	case ETaGAction::PassThrow:
		if (ActionT >= 0.15f)
		{
			SetAction(ETaGAction::None);
		}
		break;
	case ETaGAction::Shoot:
	{
		const FTaGShotTypeDef& St = FTaGRules::ShotType(ShotType);
		const float MeterSpeed = (IsCatchAndShoot() ? 1.6f : 1.f) * (GetShotMod() == ETaGArtFx::ShotQuick ? 1.7f : 1.f);
		ShotCharge += Dt * MeterSpeed / (St.Meter * TaG::ShotMeterSeconds);
		if (!bShotJumped && St.JumpAt >= 0.f && ActionT >= St.JumpAt && GetCharacterMovement()->IsMovingOnGround())
		{
			bShotJumped = true;
			GetCharacterMovement()->JumpZVelocity = St.JumpVel * FMath::Sqrt(GetHeroDef().JumpMul);
			Jump();
			bStopJumpNextTick = true;
		}
		if (bSim && !HasBall())
		{
			SetAction(ETaGAction::None);
		}
		else if (bSim && ShotCharge >= TaG::ShotAutoRelease)
		{
			ReleaseShot();
		}
		break;
	}
	case ETaGAction::ArtStart:
		if (bSim)
		{
			ArtStartFinished();
		}
		break;
	case ETaGAction::ArtActive:
		if (ActionT >= 0.35f && bSim)
		{
			SetAction(ETaGAction::ArtRecover, 0, FVector::ZeroVector, ActionLength > 0.f ? ActionLength : 0.5f);
		}
		break;
	default:
		break;
	}
}

void ATaGCharacter::TickServerChecks(float Dt)
{
	ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	if (!GM || !GM->IsLive())
	{
		return;
	}
	const float Now = ServerNow();

	// Drive > contest: a drive that passes a contesting defender leaves them stuck.
	if ((Action == ETaGAction::Drive || (Action == ETaGAction::ArtStart && ActionVariant == static_cast<uint8>(ETaGArt::Blink))) && !bDriveBeatDone)
	{
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetTeam() != Team && (It->IsContesting() || It->IsContestStarting())
				&& FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < TaG::DriveBeatsContestRange)
			{
				bDriveBeatDone = true;
				GM->OnDriveBeatsContest(this, *It);
				break;
			}
		}
	}

	// Tackle hits
	if (Action == ETaGAction::Tackle && !bTackleResolved)
	{
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (*It == this || It->GetTeam() == Team)
			{
				continue;
			}
			FVector To = It->GetActorLocation() - GetActorLocation();
			To.Z = 0.f;
			// A dodge that slid out of the tackle's path still beat it (evade > tackle): reach the dodger a bit farther.
			const float Bash = TackleArt == ETaGArt::ShieldBash ? 60.f : 0.f;
			const float Reach = TaG::TackleRange + It->GetCapsuleComponent()->GetScaledCapsuleRadius() - 25.f + (It->IsDodging() ? 90.f : 0.f) + Bash;
			if (To.Size() < Reach && (To.Size() < 40.f || FVector::DotProduct(To.GetSafeNormal(), GetActorForwardVector()) > 0.2f))
			{
				bTackleResolved = true;
				GM->ResolveTackle(this, *It, false);
				break;
			}
		}
	}

	// Gantetsu's shoulder charge hits everyone in front once
	if (Action == ETaGAction::ArtActive && ActionVariant == static_cast<uint8>(ETaGArt::ShoulderCharge))
	{
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (*It == this || It->GetTeam() == Team || ArtHits.Contains(*It))
			{
				continue;
			}
			FVector To = It->GetActorLocation() - GetActorLocation();
			To.Z = 0.f;
			if (To.Size() < 150.f + It->GetCapsuleComponent()->GetScaledCapsuleRadius() && FVector::DotProduct(To.GetSafeNormal(), GetActorForwardVector()) > 0.1f)
			{
				ArtHits.Add(*It);
				GM->ResolveTackle(this, *It, true);
				if (Action != ETaGAction::ArtActive)
				{
					break; // the charge was evaded
				}
			}
		}
	}

	// Down: a teammate's touch resyncs the TaG (instant revive)
	if (Action == ETaGAction::Down && ActionT > 0.2f)
	{
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != this && It->GetTeam() == Team && !It->IsDown()
				&& FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < GetCapsuleComponent()->GetScaledCapsuleRadius() + It->GetCapsuleComponent()->GetScaledCapsuleRadius() + TaG::ReviveTouchExtra)
			{
				Revive();
				GM->Popup(FString::Printf(TEXT("リシンク！ %s が起こした"), It->GetHeroDef().Name), Team);
				break;
			}
		}
	}

	// Guard regenerates after a short delay
	if (Now - LastGuardHit > TaG::GuardRegenDelay && Guard < GetMaxGuard())
	{
		Guard = FMath::Min(GetMaxGuard(), Guard + TaG::GuardRegenPerSec * Dt);
	}
	if (PretellKind != 0 && Now >= PretellEnd)
	{
		PretellKind = 0;
	}
}

void ATaGCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bStopJumpNextTick)
	{
		bStopJumpNextTick = false;
		StopJumping();
		GetCharacterMovement()->JumpZVelocity = TaG::JumpVelocity * GetHeroDef().JumpMul;
	}

	TickAction(DeltaSeconds);
	if (HasAuthority())
	{
		TickServerChecks(DeltaSeconds);
	}
	TickArtStatus(DeltaSeconds);

	// Forced movement (drive, dodge, tackle, step, shot motions) on the machine that owns the input.
	if (IsLocallyControlled() && IsForcedMove())
	{
		FVector Dir = ActionDir;
		if (Action == ETaGAction::Shoot)
		{
			float Speed = 0.f;
			Dir = ShotMotion(Speed);
		}
		if (!Dir.IsNearlyZero())
		{
			AddMovementInput(Dir, 1.f);
		}
	}

	DribbleT += DeltaSeconds;
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickVisuals(DeltaSeconds);
	}
}

// ================================================================ Inputs

bool ATaGCharacter::CanUseInput(ETaGInput Input) const
{
	const ATaGGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ATaGGameMode>() : nullptr;
	if (GM && !GM->IsLive())
	{
		return false;
	}
	const int32 MyRole = GetRole();
	const bool bFree = Action == ETaGAction::None;
	switch (Input)
	{
	case ETaGInput::Primary:
		if (!bFree) { return false; }
		if (MyRole == 1) { return IsReady(ETaGSlot::Drive); }
		if (MyRole == 2) { return IsReady(ETaGSlot::Tackle); }
		return IsReady(ETaGSlot::Drive);
	case ETaGInput::Secondary:
		if (!bFree) { return false; }
		if (MyRole == 1) { return IsReady(ETaGSlot::Dodge); }
		if (MyRole == 2) { return IsReady(ETaGSlot::Contest); }
		return false;
	case ETaGInput::Step:
		return bFree && MyRole != 1 && IsReady(ETaGSlot::Step);
	case ETaGInput::PassSteal:
		if (MyRole == 1)
		{
			return (bFree || Action == ETaGAction::Gathered || Action == ETaGAction::Drive) && IsReady(ETaGSlot::Pass) && FindBestPassTarget(GetAimDirection()) != nullptr;
		}
		if (MyRole == 2) { return bFree && IsReady(ETaGSlot::Steal); }
		return false;
	case ETaGInput::Shoot:
		return MyRole == 1 && (bFree || Action == ETaGAction::Drive || Action == ETaGAction::Gathered || Action == ETaGAction::DriveStart);
	case ETaGInput::Art1:
		return CanUseArt(1);
	case ETaGInput::Art2:
		return CanUseArt(2);
	default:
		return false;
	}
}

void ATaGCharacter::InputPressed(ETaGInput Input)
{
	if (!CanUseInput(Input))
	{
		return;
	}
	const int32 MyRole = GetRole();
	switch (Input)
	{
	case ETaGInput::Primary:
		if (MyRole == 2) { StartTackle(); }
		else { StartDrive(MyRole == 1); }
		break;
	case ETaGInput::Secondary:
		if (MyRole == 1) { StartDodge(false); }
		else if (MyRole == 2) { StartContest(); }
		break;
	case ETaGInput::Step:
		StartStep();
		break;
	case ETaGInput::PassSteal:
		if (MyRole == 1) { TryPass(false); }
		else if (MyRole == 2) { TrySteal(); }
		break;
	case ETaGInput::Shoot:
		if (Action == ETaGAction::Drive || Action == ETaGAction::DriveStart || Action == ETaGAction::Gathered)
		{
			DriveCancelT = 0.2f; // a shot straight out of a drive still loses to a tackle for a moment
		}
		BeginShot();
		break;
	case ETaGInput::Art1:
		UseArt(1);
		break;
	case ETaGInput::Art2:
		UseArt(2);
		break;
	default:
		break;
	}
}

void ATaGCharacter::InputReleased(ETaGInput Input)
{
	if (Input == ETaGInput::Secondary)
	{
		ReleaseContest();
	}
	else if (Input == ETaGInput::Shoot)
	{
		ReleaseShot();
	}
}

void ATaGCharacter::InputJump()
{
	if ((Action == ETaGAction::None || Action == ETaGAction::Gathered) && GetCharacterMovement()->IsMovingOnGround())
	{
		Jump();
		bStopJumpNextTick = true;
	}
}

void ATaGCharacter::StartDrive(bool bWithBall)
{
	StartCooldown(ETaGSlot::Drive, bWithBall ? TaG::DriveCD : TaG::CutCD);
	bHadBallOnDrive = bWithBall;
	bDriveBeatDone = false;
	SetAction(ETaGAction::DriveStart);
}

void ATaGCharacter::StartDodge(bool bAfterimage)
{
	if (!bAfterimage)
	{
		StartCooldown(ETaGSlot::Dodge, TaG::DodgeCD);
	}
	bDodgeEvaded = false;
	SetAction(ETaGAction::DodgeStart, bAfterimage ? static_cast<uint8>(ETaGArt::Afterimage) : 0);
	if (bAfterimage && HasAuthority())
	{
		MulticastFx(FxAfterimage, GetFeetLocation(), GetFeetLocation(), nullptr);
	}
}

void ATaGCharacter::StartTackle()
{
	StartCooldown(ETaGSlot::Tackle, TaG::TackleCD);
	SetAction(ETaGAction::TackleStart);
}

void ATaGCharacter::StartContest()
{
	bContestHeld = true;
	SetAction(ETaGAction::ContestStart);
}

void ATaGCharacter::ReleaseContest()
{
	bContestHeld = false;
}

void ATaGCharacter::StartStep()
{
	StartCooldown(ETaGSlot::Step, TaG::StepCD);
	FVector Dir = WishDir();
	if (Dir.IsNearlyZero())
	{
		Dir = -GetActorForwardVector();
	}
	SetAction(ETaGAction::Step, 0, Dir, TaG::StepT);
}

void ATaGCharacter::TrySteal()
{
	StartCooldown(ETaGSlot::Steal, TaG::StealCD);
	if (HasAuthority())
	{
		ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
		if (!GM || !GM->ResolveSteal(this))
		{
			SetAction(ETaGAction::StealRecover);
		}
	}
}

void ATaGCharacter::TryPass(bool bThread)
{
	ATaGCharacter* Target = FindBestPassTarget(GetAimDirection());
	if (!Target)
	{
		return;
	}
	StartCooldown(ETaGSlot::Pass, TaG::PassCD);
	SetAction(ETaGAction::PassThrow);
	if (HasAuthority())
	{
		if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
		{
			const uint8 Style = PendingPassStyle != 0 ? PendingPassStyle : ((bThread || IsTetheredWith(Target)) ? 1 : 0);
			GM->LaunchPass(this, Target, Style);
		}
	}
}

ETaGShotType ATaGCharacter::ClassifyShot() const
{
	// Ported from athlete.gd _classify_shot: stick direction toward / away from the rim, jump and drive.
	const FVector Feet = GetFeetLocation();
	const FVector ToRim(TaG::RimX - Feet.X, -Feet.Y, 0.f);
	const float Nd = ToRim.Size() / 100.f;
	const FVector W = WishDir();
	const float Toward = (!W.IsNearlyZero() && ToRim.Size() > 1.f) ? FVector::DotProduct(W, ToRim.GetSafeNormal()) : 0.f;
	if (Action == ETaGAction::Drive || Action == ETaGAction::DriveStart || Action == ETaGAction::Gathered || SinceDrive < 0.5f)
	{
		if (Nd < 1.9f) { return ETaGShotType::Dunk; }
		if (Nd < 3.6f) { return ETaGShotType::Euro; }
	}
	if (!GetCharacterMovement()->IsMovingOnGround())
	{
		return Toward < -0.3f ? ETaGShotType::Fade : ETaGShotType::Jumper;
	}
	if (Toward > 0.5f)
	{
		if (Nd < 3.2f) { return ETaGShotType::Layup; }
		if (Nd < 5.5f) { return ETaGShotType::Floater; }
	}
	if (Toward < -0.5f)
	{
		return ETaGShotType::StepBack;
	}
	return ETaGShotType::Jumper;
}

void ATaGCharacter::BeginShot()
{
	ShotType = ClassifyShot();
	const FTaGShotTypeDef& St = FTaGRules::ShotType(ShotType);
	ShotCharge = 0.f;
	bShotContact = false;
	bShotJumped = St.JumpAt < 0.f;
	const float Lateral = FVector::DotProduct(WishDir(), GetActorRightVector());
	ShotSide = FMath::Abs(Lateral) > 0.25f ? FMath::Sign(Lateral) : 1.f;
	SetAction(ETaGAction::Shoot, static_cast<uint8>(ShotType));
}

void ATaGCharacter::ReleaseShot()
{
	if (Action != ETaGAction::Shoot)
	{
		return;
	}
	const float Charge = ShotCharge;
	const bool bContact = bShotContact;
	SetAction(ETaGAction::None);
	if (HasAuthority())
	{
		if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
		{
			GM->ResolveShot(this, Charge, bContact);
		}
	}
}

void ATaGCharacter::ForceContactShot()
{
	bShotContact = true;
	ReleaseShot();
}

// ================================================================ Arts

bool ATaGCharacter::CanUseArt(int32 Index) const
{
	const ETaGArt Art = Index == 1 ? GetHeroDef().Art1 : GetHeroDef().Art2;
	const ETaGSlot Slot = Index == 1 ? ETaGSlot::Art1 : ETaGSlot::Art2;
	if (Art == ETaGArt::None || !IsReady(Slot))
	{
		return false;
	}
	const FTaGArtDef& GenericDef = TaGHeroes::GetArt(Art);
	if (GenericDef.Kind != ETaGArtKind::Custom)
	{
		return CanUseGenericArt(GenericDef);
	}
	const bool bFree = Action == ETaGAction::None;
	const bool bBall = HasBall();
	switch (Art)
	{
	case ETaGArt::ShoulderCharge:
	case ETaGArt::Pull:
	case ETaGArt::SlowField:
	case ETaGArt::SonicWave:
	case ETaGArt::IronWall:
	case ETaGArt::SpotUp:
		return bFree && !bBall;
	case ETaGArt::Blink:
		return bFree;
	case ETaGArt::Afterimage:
		return bFree && bBall;
	case ETaGArt::Scope:
		return Action != ETaGAction::Down;
	case ETaGArt::ThreadPass:
		return bBall && (bFree || Action == ETaGAction::Gathered) && FindBestPassTarget(GetAimDirection()) != nullptr;
	case ETaGArt::Tether:
	{
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != this && It->GetTeam() == Team && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 1200.f)
			{
				return Action != ETaGAction::Down;
			}
		}
		return false;
	}
	case ETaGArt::Resync:
	{
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (*It != this && It->GetTeam() == Team)
			{
				return Action != ETaGAction::Down;
			}
		}
		return false;
	}
	default:
		return false;
	}
}

void ATaGCharacter::UseArt(int32 Index)
{
	if (!CanUseArt(Index))
	{
		return;
	}
	const ETaGArt Art = Index == 1 ? GetHeroDef().Art1 : GetHeroDef().Art2;
	StartCooldown(Index == 1 ? ETaGSlot::Art1 : ETaGSlot::Art2, TaGHeroes::GetArt(Art).Cooldown);
	if (HasAuthority())
	{
		if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
		{
			GM->OnArtUsed(this, Art);
		}
	}
	ExecuteArt(Art);
}

void ATaGCharacter::ExecuteArt(ETaGArt Art)
{
	const float Now = ServerNow();
	UWorld* World = GetWorld();
	const bool bAuth = HasAuthority();
	const uint8 V = static_cast<uint8>(Art);
	switch (Art)
	{
	case ETaGArt::ShoulderCharge:
		ArtHits.Reset();
		SetAction(ETaGAction::ArtStart, V);
		break;
	case ETaGArt::IronWall:
		SetAction(ETaGAction::ArtStart, V);
		if (bAuth)
		{
			const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
			const FVector P = GetFeetLocation() + Fwd * 220.f + FVector(0.f, 0.f, ATaGWallActor::Height * 0.5f + 1.f);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ATaGWallActor* W = World->SpawnActor<ATaGWallActor>(ATaGWallActor::StaticClass(), P, Fwd.Rotation(), Params))
			{
				W->Init(Team, 4.f);
			}
		}
		break;
	case ETaGArt::Pull:
		SetAction(ETaGAction::ArtStart, V);
		break;
	case ETaGArt::SlowField:
		if (bAuth)
		{
			const FVector F = GetFeetLocation();
			if (ATaGFieldActor* Field = World->SpawnActor<ATaGFieldActor>(ATaGFieldActor::StaticClass(), FVector(F.X, F.Y, 2.f), FRotator::ZeroRotator))
			{
				Field->Init(ETaGFieldKind::Slow, Team, 400.f, 3.5f, nullptr);
			}
		}
		break;
	case ETaGArt::Blink:
		SetAction(ETaGAction::ArtStart, V);
		break;
	case ETaGArt::Afterimage:
		StartDodge(true);
		break;
	case ETaGArt::Scope:
		ScopeEnd = Now + 3.f;
		break;
	case ETaGArt::SpotUp:
		SpotUpEnd = Now + 4.f;
		break;
	case ETaGArt::ThreadPass:
		TryPass(true);
		break;
	case ETaGArt::Tether:
		if (bAuth)
		{
			ATaGCharacter* Best = nullptr;
			float BestD = 1200.f;
			for (TActorIterator<ATaGCharacter> It(World); It; ++It)
			{
				const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
				if (*It != this && It->GetTeam() == Team && D < BestD)
				{
					BestD = D;
					Best = *It;
				}
			}
			if (Best)
			{
				SetTether(Best, 5.f);
				Best->SetTether(this, 5.f);
				MulticastFx(FxTether, GetActorLocation(), Best->GetActorLocation(), Best);
			}
		}
		break;
	case ETaGArt::SonicWave:
		SetAction(ETaGAction::ArtStart, V);
		break;
	case ETaGArt::Resync:
		if (bAuth)
		{
			ATaGCharacter* Downed = nullptr;
			ATaGCharacter* Near = nullptr;
			float NearD = 1000.f;
			for (TActorIterator<ATaGCharacter> It(World); It; ++It)
			{
				if (*It == this || It->GetTeam() != Team)
				{
					continue;
				}
				if (It->IsDown())
				{
					Downed = *It;
				}
				const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
				if (D < NearD)
				{
					NearD = D;
					Near = *It;
				}
			}
			ATaGGameMode* GM = World->GetAuthGameMode<ATaGGameMode>();
			if (Downed)
			{
				Downed->Revive();
				MulticastFx(FxRevive, GetActorLocation(), Downed->GetActorLocation(), Downed);
				if (GM) { GM->Popup(FString::Printf(TEXT("リシンク！ %s が遠くから起こした"), GetHeroDef().Name), Team); }
			}
			else if (Near)
			{
				Near->ResetHandCooldowns();
				MulticastFx(FxRevive, GetActorLocation(), Near->GetActorLocation(), Near);
				if (GM) { GM->Popup(FString::Printf(TEXT("リシンク！ %s の技がすぐ使える"), Near->GetHeroDef().Name), Team); }
			}
		}
		break;
	default:
		ExecuteGenericArt(TaGHeroes::GetArt(Art));
		break;
	}
}

void ATaGCharacter::ArtStartFinished()
{
	const ETaGArt Art = static_cast<ETaGArt>(ActionVariant);
	switch (Art)
	{
	case ETaGArt::ShoulderCharge:
		if (ActionT >= 0.2f)
		{
			SetAction(ETaGAction::ArtActive, ActionVariant, GetActorForwardVector(), 0.5f);
		}
		break;
	case ETaGArt::IronWall:
		if (ActionT >= 0.15f)
		{
			SetAction(ETaGAction::None);
		}
		break;
	case ETaGArt::Pull:
		if (ActionT >= 0.35f)
		{
			FirePull();
			if (Action == ETaGAction::ArtStart)
			{
				SetAction(ETaGAction::ArtRecover, 0, FVector::ZeroVector, 0.4f);
			}
		}
		break;
	case ETaGArt::Blink:
		if (ActionT >= 0.1f)
		{
			FinishBlink();
		}
		break;
	case ETaGArt::SonicWave:
		if (ActionT >= 0.3f)
		{
			SetAction(ETaGAction::Contest, ActionVariant, FVector::ZeroVector, 1.2f);
			if (HasAuthority())
			{
				if (ATaGFieldActor* Field = GetWorld()->SpawnActor<ATaGFieldActor>(ATaGFieldActor::StaticClass(), GetFeetLocation(), FRotator::ZeroRotator))
				{
					Field->Init(ETaGFieldKind::Sonic, Team, 400.f, 1.2f, this);
				}
			}
		}
		break;
	default:
	{
		// Generic arts: a short startup (readable), then the effect, then a recovery (punishable)
		const FTaGArtDef& Def = TaGHeroes::GetArt(Art);
		if (Def.Kind == ETaGArtKind::Dash && Def.Fx == ETaGArtFx::DashHop)
		{
			if (ActionT >= 0.12f)
			{
				FinishBlink(Def.Range);
			}
		}
		else if (Def.Kind == ETaGArtKind::Cone || Def.Kind == ETaGArtKind::Aura || Def.Kind == ETaGArtKind::Bolt)
		{
			if (ActionT >= Def.Startup)
			{
				FireGenericArt(Def);
				if (Action == ETaGAction::ArtStart)
				{
					SetAction(ETaGAction::ArtRecover, 0, FVector::ZeroVector, 0.35f);
				}
			}
		}
		else if (ActionT >= 0.15f)
		{
			SetAction(ETaGAction::None);
		}
		break;
	}
	}
}

void ATaGCharacter::FinishBlink(float Distance)
{
	const FVector Origin = GetActorLocation();
	FVector Dir = WishDir();
	if (Dir.IsNearlyZero())
	{
		Dir = GetActorForwardVector().GetSafeNormal2D();
	}
	// Step toward the target and stop at the first blocking surface.
	FVector Dest = Origin + Dir * Distance;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(TaGBlink), false, this);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius(), GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.9f);
	if (GetWorld()->SweepSingleByChannel(Hit, Origin, Dest, FQuat::Identity, ECC_Pawn, Shape, Q))
	{
		Dest = Hit.Location;
	}
	SetActorLocation(Dest, false, nullptr, ETeleportType::TeleportPhysics);
	bHadBallOnDrive = HasBall();
	bDriveBeatDone = false;
	SinceDrive = 0.f;
	// The blink counts as a drive for the rest of its motion (tackle beats it, it beats contest).
	SetAction(ETaGAction::Drive, 0, Dir, 0.15f);
	if (HasAuthority())
	{
		MulticastFx(FxBlink, Origin - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Dest, nullptr);
		// The path itself also beats contesting defenders it passes.
		if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
		{
			for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
			{
				if (It->GetTeam() != Team && (It->IsContesting() || It->IsContestStarting()))
				{
					const FVector P = It->GetActorLocation();
					const FVector Closest = FMath::ClosestPointOnSegment(P, Origin, Dest);
					if (FVector::Dist2D(P, Closest) < TaG::DriveBeatsContestRange)
					{
						bDriveBeatDone = true;
						GM->OnDriveBeatsContest(this, *It);
						break;
					}
				}
			}
		}
	}
}

void ATaGCharacter::FirePull()
{
	if (!HasAuthority())
	{
		return;
	}
	const FVector Eye = GetEyeLocation();
	FVector Dir = GetAimDirection();
	Dir.Z = FMath::Clamp(Dir.Z, -0.3f, 0.3f);
	Dir.Normalize();
	const FVector End = Eye + Dir * 800.f;
	ATaGCharacter* Best = nullptr;
	float BestT = 1e9f;
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->GetTeam() == Team || It->IsDown())
		{
			continue;
		}
		const FVector P = It->GetActorLocation();
		const FVector C = FMath::ClosestPointOnSegment(P, Eye, End);
		const float Lateral = FVector::Dist2D(P, C);
		const float Along = FVector::DotProduct(C - Eye, Dir);
		if (Lateral < 60.f + It->GetCapsuleComponent()->GetScaledCapsuleRadius() && Along > 0.f && Along < BestT)
		{
			BestT = Along;
			Best = *It;
		}
	}
	const FVector HitPoint = Best ? Best->GetActorLocation() : End;
	MulticastFx(FxPullBeam, Eye - FVector(0.f, 0.f, 30.f), HitPoint, Best);
	if (Best)
	{
		if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
		{
			GM->OnPullHit(this, Best);
		}
	}
}

// ================================================================ Server outcomes

void ATaGCharacter::KnockDown(const FVector& Push)
{
	if (!HasAuthority())
	{
		return;
	}
	if (Action == ETaGAction::Shoot)
	{
		bShotContact = true;
	}
	SetAction(ETaGAction::Down, 0, Push, TaG::DownT);
	LaunchCharacter(Push.GetSafeNormal2D() * 300.f + FVector(0.f, 0.f, 150.f), true, true);
	LastGuardHit = ServerNow();
}

void ATaGCharacter::Stagger(float Seconds, const FVector& Push)
{
	if (!HasAuthority() || Action == ETaGAction::Down)
	{
		return;
	}
	StartCooldown(ETaGSlot::Contest, TaG::ContestCD);
	SetAction(ETaGAction::Stagger, 0, Push, Seconds);
	if (!Push.IsNearlyZero())
	{
		LaunchCharacter(Push, true, false);
	}
}

void ATaGCharacter::ForceRecover(float Seconds)
{
	if (!HasAuthority() || Action == ETaGAction::Down)
	{
		return;
	}
	SetAction(ETaGAction::TackleRecover, 0, FVector::ZeroVector, Seconds);
}

void ATaGCharacter::Revive()
{
	if (HasAuthority() && Action == ETaGAction::Down)
	{
		Guard = GetMaxGuard();
		SetAction(ETaGAction::None);
		MulticastFx(FxRevive, GetActorLocation(), GetActorLocation(), this);
	}
}

void ATaGCharacter::GiveZone()
{
	ZoneEnd = ServerNow() + TaG::ZoneT;
}

void ATaGCharacter::ApplyGuardDamage(float Amount)
{
	if (!HasAuthority() || Action == ETaGAction::Down)
	{
		return;
	}
	Guard -= Amount;
	LastGuardHit = ServerNow();
	if (Guard <= 0.f)
	{
		Guard = GetMaxGuard();
		KnockDown(-GetActorForwardVector());
		if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
		{
			GM->OnGuardBreak(this);
		}
	}
}

void ATaGCharacter::ApplySlow(float SpeedFactor, float InActionFactor, float Seconds)
{
	SlowEnd = FMath::Max(SlowEnd, ServerNow() + Seconds);
	SlowSpeedFactor = SpeedFactor;
	SlowActionFactor = InActionFactor;
}

void ATaGCharacter::AddSpeedBoost(float Seconds)
{
	SpeedBoostEnd = FMath::Max(SpeedBoostEnd, ServerNow() + Seconds);
}

void ATaGCharacter::ResetHandCooldowns()
{
	for (const ETaGSlot S : { ETaGSlot::Drive, ETaGSlot::Dodge, ETaGSlot::Tackle, ETaGSlot::Contest, ETaGSlot::Step, ETaGSlot::Steal })
	{
		const int32 i = static_cast<int32>(S);
		CooldownEnd[i] = 0.f;
		if (HasAuthority() && CooldownEndRep.IsValidIndex(i))
		{
			CooldownEndRep[i] = 0.f;
		}
	}
}

void ATaGCharacter::OnCaughtPass()
{
	LastCatchTime = ServerNow();
	if (ServerNow() < SpotUpEnd && GetVelocity().Size2D() < 160.f)
	{
		CatchShootEnd = ServerNow() + 1.5f; // Hayate: spot-up catch and shoot
	}
}

void ATaGCharacter::OnBallGained()
{
	DribbleT = 0.f;
}

void ATaGCharacter::OnBallLost()
{
	if (Action == ETaGAction::Shoot || Action == ETaGAction::Gathered)
	{
		SetAction(ETaGAction::None);
	}
}

void ATaGCharacter::SetTether(ATaGCharacter* Partner, float Seconds)
{
	TetherPartner = Partner;
	TetherEnd = ServerNow() + Seconds;
}

void ATaGCharacter::ClearForNewPossession(const FVector& FeetPos, const FVector& LookAt)
{
	SetAction(ETaGAction::None);
	GetCharacterMovement()->StopMovementImmediately();
	const FVector Pos = FeetPos + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
	FRotator Rot = (LookAt - Pos).Rotation();
	Rot.Pitch = 0.f;
	Rot.Roll = 0.f;
	SetActorLocationAndRotation(Pos, Rot, false, nullptr, ETeleportType::TeleportPhysics);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetControlRotation(Rot);
		PC->ClientSetRotation(Rot, true);
	}
	else if (AController* C = GetController())
	{
		C->SetControlRotation(Rot);
	}
	if (Guard < GetMaxGuard())
	{
		Guard = GetMaxGuard();
	}
}

// ================================================================ Cosmetics

void ATaGCharacter::MulticastFx_Implementation(uint8 Kind, FVector_NetQuantize A, FVector_NetQuantize B, ATaGCharacter* Other)
{
	UWorld* World = GetWorld();
	const ATaGGameState* GS = World ? World->GetGameState<ATaGGameState>() : nullptr;
	const FLinearColor TeamC = GS ? GS->TeamColorFor(Team) : TaG::AllyColor();
	const FTaGHeroDef& H = GetHeroDef();
	switch (Kind)
	{
	case FxBlink:
	case FxAfterimage:
		ATaGGhostFx::Spawn(World, FVector(A), H.EyeHeight + 15.f, H.CapsuleRadius * 1.2f, FLinearColor(0.3f, 0.9f, 1.f), Kind == FxBlink ? 0.45f : 0.6f);
		TaGAudio::PlayAt(this, TEXT("Dodge"), FVector(A), 0.8f);
		break;
	case FxPullBeam:
		ATaGBeamFx::Spawn(World, FVector(A), FVector(B), FLinearColor(0.2f, 0.6f, 1.f), 0.35f, 12.f);
		TaGAudio::PlayAt(this, TEXT("ArtCue"), FVector(A), 0.9f);
		break;
	case FxTether:
		ATaGBeamFx::Spawn(World, FVector(A), FVector(B), TeamC, 5.f, 1.5f, this, Other);
		TaGAudio::PlayAt(this, TEXT("ArtCue"), FVector(A), 0.7f);
		break;
	case FxThread:
		ATaGBeamFx::Spawn(World, FVector(A), FVector(B), FLinearColor(1.f, 0.85f, 0.35f), 0.45f, 1.2f, this, Other);
		break;
	case FxRevive:
		ATaGBeamFx::Spawn(World, FVector(A), FVector(B), TeamC, 0.5f, 4.f);
		TaGAudio::PlayAt(this, TEXT("Revive"), FVector(B), 0.9f);
		break;
	default:
		break;
	}
}

void ATaGCharacter::FaceTowards(const FVector& Target, float DeltaSeconds, float Speed)
{
	if (AController* C = GetController())
	{
		FRotator Want = (Target - GetEyeLocation()).Rotation();
		Want.Roll = 0.f;
		C->SetControlRotation(FMath::RInterpTo(C->GetControlRotation(), Want, DeltaSeconds, Speed));
	}
}

void ATaGCharacter::SetThirdPerson(bool bTPS)
{
	bThirdPerson = bTPS;
	Rig->SetOwnerNoSeeAll(!bTPS);
}

void ATaGCharacter::TickVisuals(float Dt)
{
	if (!bVisualsBuilt)
	{
		return;
	}
	const ATaGGameState* GS = GetWorld()->GetGameState<ATaGGameState>();
	const FLinearColor TeamC = GS ? GS->TeamColorFor(Team) : TaG::AllyColor();
	const int32 MyRole = GetRole();
	const bool bOnFloor = GetCharacterMovement()->IsMovingOnGround();
	{
		const FVector Feet = GetFeetLocation();
		TaGArt::PlaceBlobShadow(BlobShadow, Feet, GetHeroDef().CapsuleRadius * 2.f + 14.f, Feet.Z, 0.36f);
	}

	// --- Body pose
	FTaGPoseInput In;
	const FVector V = GetVelocity();
	In.Speed = V.Size2D();
	In.Velocity = V;
	In.ForwardSpeed = FVector::DotProduct(V, GetActorForwardVector());
	In.bOnFloor = bOnFloor;
	In.Role = MyRole;
	In.Action = Action;
	In.ActionT = ActionT;
	In.ShotType = ShotType;
	In.ShotCharge = ShotCharge;
	In.DribbleT = DribbleT;
	In.DodgeSide = FVector::DotProduct(ActionDir, GetActorRightVector()) >= 0.f ? 1.f : -1.f;
	uint8 Pretell = 0;
	const bool bPretelling = IsPretelling(Pretell);
	if (bPretelling)
	{
		In.bPretellTackle = Pretell == 1;
		In.bPretellContest = Pretell == 2;
		In.bPretellDrive = Pretell == 3;
		In.bPretellShot = Pretell == 4;
	}
	In.bBraced = IsInvulnerable() || IsRootedWall();
	In.Dt = Dt;
	Rig->Animate(In);
	Rig->SetTeamColor(TeamC);

	// Footfalls kick up dust when running; landings make a bigger puff
	const int32 Steps = Rig->GetStepCount();
	if (Steps != LastStepCount)
	{
		const float Run = FMath::Clamp((In.Speed - 250.f) / 450.f, 0.f, 1.f);
		const float Weight = FMath::Clamp(GetHeroDef().Weight, 0.5f, 3.f);
		if (Run > 0.f || Steps - LastStepCount > 1)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				if (Rig->IsFootPlanted(Side))
				{
					TaGFx::Dust(this, Rig->GetFootWorld(Side), (0.5f + Run) * FMath::Sqrt(Weight) * 0.6f);
				}
			}
		}
		LastStepCount = Steps;
	}
	// The zone: gold motes rising around the body
	if (IsInZone())
	{
		ZoneFxT -= Dt;
		if (ZoneFxT <= 0.f)
		{
			ZoneFxT = 0.05f;
			TaGFx::Aura(this, GetFeetLocation(), GetHeroDef().EyeHeight, GetHeroDef().CapsuleRadius + 12.f, TaG::GoldColor(), 2);
		}
	}
	// Drives leave streaks in the hero colour
	if (Action == ETaGAction::Drive && V.Size2D() > 400.f)
	{
		TaGFx::Burst(this, GetActorLocation() - V.GetSafeNormal() * 40.f, GetHeroDef().SecondaryColor, 2, 60.f, 0.25f, 9.f, 0.f, ETaGFxShape::Spark, -V * 0.3f);
	}
	TickHeroAmbience(Dt);

	// --- TaG glow: what you are doing (the tell), and whether each hand is ready (visible cooldowns)
	FLinearColor Tell = TeamC;
	switch (Action)
	{
	case ETaGAction::TackleStart:
	case ETaGAction::Tackle:
		Tell = FLinearColor(1.f, 0.45f, 0.1f);
		break;
	case ETaGAction::DriveStart:
	case ETaGAction::Drive:
		Tell = FLinearColor(0.4f, 0.8f, 1.f);
		break;
	case ETaGAction::DodgeStart:
	case ETaGAction::Dodge:
		Tell = FLinearColor::White;
		break;
	case ETaGAction::ContestStart:
	case ETaGAction::Contest:
		Tell = FLinearColor(1.f, 0.2f, 0.2f);
		break;
	case ETaGAction::ArtStart:
	case ETaGAction::ArtActive:
		Tell = FLinearColor(0.8f, 0.5f, 1.f);
		break;
	default:
		if (In.bPretellTackle) { Tell = FLinearColor(1.f, 0.45f, 0.1f); }
		else if (In.bPretellContest) { Tell = FLinearColor(1.f, 0.2f, 0.2f); }
		else if (In.bPretellDrive) { Tell = FLinearColor(0.4f, 0.8f, 1.f); }
		else if (In.bPretellShot) { Tell = TaG::GoldColor(); }
		else if (IsInZone()) { Tell = TaG::GoldColor(); }
		break;
	}
	const bool bFirstReady = MyRole == 2 ? IsReady(ETaGSlot::Tackle) : IsReady(ETaGSlot::Drive);
	const bool bSecondReady = MyRole == 2 ? IsReady(ETaGSlot::Contest) : IsReady(ETaGSlot::Dodge);
	// A tell glows bright on both arms so it reads from across the court.
	Rig->SetGlow(Tell, bPretelling ? 10.f : (bFirstReady ? 6.f : 0.2f), bPretelling ? 10.f : (bSecondReady ? 6.f : 0.2f));

	// --- Contest field (directional) in front of the defender
	const bool bField = (Action == ETaGAction::Contest || Action == ETaGAction::ContestStart) && !IsSonicContest();
	ContestField->SetVisibility(bField);

	// --- Camera and first-person arms (owner only)
	if (IsLocallyControlled() && IsPlayerControlled())
	{
		const FTaGHeroDef& H = GetHeroDef();
		const float S = H.EyeHeight / 175.f;
		float TargetZ = H.EyeHeight - H.CapsuleHalfHeight;
		if (Action == ETaGAction::Down) { TargetZ -= H.EyeHeight * 0.6f; }
		else if (Action == ETaGAction::TackleStart) { TargetZ -= 25.f; }
		const FVector BoomLoc = CameraBoom->GetRelativeLocation();
		CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, FMath::FInterpTo(BoomLoc.Z, TargetZ, Dt, 12.f)));
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, bThirdPerson ? 330.f * FMath::Max(1.f, S) : 0.f, Dt, 10.f);
		CameraBoom->SocketOffset = bThirdPerson ? FVector(0.f, 60.f, 50.f) : FVector::ZeroVector;
		const ATaGPlayerController* MyPC = Cast<ATaGPlayerController>(GetController());
		float Fov = (MyPC ? MyPC->FieldOfView : 95.f) + (IsInZone() ? 6.f : 0.f);
		if (IsScoped() && !bThirdPerson) { Fov = 58.f; }
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, Fov, Dt, 10.f));
		// Camera kick: a short, decaying shake on hits and lost reads (real time, so slow motion does not stretch it).
		const float RealDt = FApp::GetDeltaTime();
		KickT += RealDt;
		KickAmp *= FMath::Exp(-RealDt * 7.f);
		const float ShakePitch = FMath::Sin(KickT * 47.f) * KickAmp * 1.6f;
		const float ShakeYaw = FMath::Sin(KickT * 39.f + 1.3f) * KickAmp * 1.1f;
		const FRotator CamRot = Camera->GetRelativeRotation();
		Camera->SetRelativeRotation(FRotator(ShakePitch, ShakeYaw, FMath::FInterpTo(CamRot.Roll, Action == ETaGAction::Down && !bThirdPerson ? 20.f : 0.f, Dt, 8.f)));

		const bool bUp = Action == ETaGAction::Contest || Action == ETaGAction::ContestStart || (!bOnFloor && MyRole != 1);
		const bool bShow = !bThirdPerson && (MyRole != 1 || Action == ETaGAction::Shoot) && Action != ETaGAction::Down;
		FPArmL->SetVisibility(bShow);
		FPArmR->SetVisibility(bShow);
		// Forearms hang from the elbow (-Z); pitch them forward and a little up so the hands sit in the
		// lower part of the view (vertical half-FOV is only ~31 degrees at 95 horizontal).
		const float Lift = bUp ? 30.f : 0.f;
		const float Pitch = bUp ? 165.f : 98.f;
		const float Reach = FMath::Clamp(S, 0.75f, 1.3f);
		const bool bArtArms = FPArmL->GetStaticMesh() != TaGArt::EngineCylinder();
		for (int32 Side = 0; Side < 2; ++Side)
		{
			UStaticMeshComponent* Arm = Side == 0 ? FPArmL.Get() : FPArmR.Get();
			const float Y = (Side == 0 ? -24.f : 24.f) * Reach;
			const FVector Elbow(18.f, Y, -24.f * Reach + Lift);
			const FRotator Rot(Pitch, 0.f, Side == 0 ? -6.f : 6.f);
			// Art meshes pivot at the elbow; the engine cylinder is centered, so shift it half a forearm along its axis.
			const float Len = 32.f * S;
			const FVector Loc = bArtArms ? Elbow : Elbow + Rot.RotateVector(FVector(0.f, 0.f, -Len * 0.5f));
			Arm->SetRelativeLocationAndRotation(Loc, Rot);
		}
		TaGArt::SetGlow(FPGlowL, Tell, bFirstReady ? 6.f : 0.2f);
		TaGArt::SetGlow(FPGlowR, Tell, bSecondReady ? 6.f : 0.2f);
	}
}

// ================================================================ Input binding

void ATaGCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	ATaGPlayerController* PC = Cast<ATaGPlayerController>(GetController());
	if (!EIC || !PC)
	{
		UE_LOG(LogTaG, Error, TEXT("TaGCharacter needs an EnhancedInputComponent and a TaGPlayerController"));
		return;
	}
	const FTaGInputActions& A = PC->GetInputActions();
	EIC->BindAction(A.Move, ETriggerEvent::Triggered, this, &ATaGCharacter::OnMove);
	EIC->BindAction(A.LookMouse, ETriggerEvent::Triggered, this, &ATaGCharacter::OnLookMouse);
	EIC->BindAction(A.LookStick, ETriggerEvent::Triggered, this, &ATaGCharacter::OnLookStick);
	EIC->BindAction(A.Jump, ETriggerEvent::Started, this, &ATaGCharacter::OnJumpPressed);
	EIC->BindAction(A.Jump, ETriggerEvent::Completed, this, &ATaGCharacter::OnJumpReleased);
	EIC->BindAction(A.Pause, ETriggerEvent::Started, this, &ATaGCharacter::OnPausePressed);
	EIC->BindAction(A.CameraToggle, ETriggerEvent::Started, this, &ATaGCharacter::OnCameraToggle);
	const TPair<UInputAction*, ETaGInput> Abilities[] = {
		{ A.Primary, ETaGInput::Primary }, { A.Secondary, ETaGInput::Secondary }, { A.Step, ETaGInput::Step },
		{ A.PassSteal, ETaGInput::PassSteal }, { A.Shoot, ETaGInput::Shoot }, { A.Art1, ETaGInput::Art1 }, { A.Art2, ETaGInput::Art2 } };
	for (const auto& Pair : Abilities)
	{
		EIC->BindAction(Pair.Key, ETriggerEvent::Started, this, &ATaGCharacter::OnAbilityPressed, Pair.Value);
		EIC->BindAction(Pair.Key, ETriggerEvent::Completed, this, &ATaGCharacter::OnAbilityReleased, Pair.Value);
	}
}

void ATaGCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	if (AController* C = GetController())
	{
		const FRotator Yaw(0.f, C->GetControlRotation().Yaw, 0.f);
		const FVector World = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * V.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * V.X;
		LastMoveInput = World.Size() > 0.15f ? World.GetSafeNormal2D() : FVector::ZeroVector;
		if (IsForcedMove() || Action == ETaGAction::Down)
		{
			return;
		}
		AddMovementInput(World, 1.f);
	}
}

void ATaGCharacter::AddLook(float YawDeg, float PitchDeg)
{
	if (Action == ETaGAction::Shoot)
	{
		return; // lock-on owns the view while charging
	}
	if (AController* C = GetController())
	{
		FRotator R = C->GetControlRotation();
		R.Yaw += YawDeg;
		R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch + PitchDeg), -80.f, 80.f);
		C->SetControlRotation(R);
	}
}

void ATaGCharacter::OnLookMouse(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	const ATaGPlayerController* PC = Cast<ATaGPlayerController>(GetController());
	const float Sens = PC ? PC->GetLookSensitivity() : 1.f;
	const float Invert = PC && PC->bInvertMouseY ? -1.f : 1.f;
	AddLook(V.X * MouseDegPerCount * Sens, V.Y * MouseDegPerCount * Sens * Invert);
}

void ATaGCharacter::OnLookStick(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	const float Dt = FApp::GetDeltaTime();
	const ATaGPlayerController* PC = Cast<ATaGPlayerController>(GetController());
	const float Sens = PC ? PC->GetLookSensitivity() : 1.f;
	// Stick up arrives as +Y; bInvertStickY (the default after the first playtest) turns that into looking down.
	const float Invert = !PC || PC->bInvertStickY ? -1.f : 1.f;
	AddLook(V.X * FMath::Abs(V.X) * StickYawRate * Dt * Sens, V.Y * FMath::Abs(V.Y) * StickPitchRate * Dt * Sens * Invert);
}

void ATaGCharacter::OnJumpPressed()
{
	InputJump();
}

void ATaGCharacter::OnJumpReleased()
{
	StopJumping();
}

void ATaGCharacter::OnAbilityPressed(ETaGInput Input)
{
	AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(Input));
}

void ATaGCharacter::OnAbilityReleased(ETaGInput Input)
{
	AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(Input));
}

void ATaGCharacter::OnPausePressed()
{
	if (ATaGPlayerController* PC = Cast<ATaGPlayerController>(GetController()))
	{
		PC->TogglePauseMenu();
	}
}

void ATaGCharacter::OnCameraToggle()
{
	SetThirdPerson(!bThirdPerson);
}
