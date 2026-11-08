#include "Character/TaGCharacter.h"
#include "BasketTaG.h"
#include "Abilities/TaGAbilities.h"
#include "Abilities/TaGTags.h"
#include "Ball/TaGBall.h"
#include "Character/TaGPlayerController.h"
#include "Core/TaGGameMode.h"
#include "Core/TaGTypes.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float CapsuleHalfHeight = 96.f;
	constexpr float MouseDegPerCount = 0.12f;
	constexpr float StickYawRate = 200.f;
	constexpr float StickPitchRate = 140.f;

	UStaticMeshComponent* MakePart(ACharacter* Owner, const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Mat,
		const FVector& RelLoc, const FVector& Scale)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Owner->GetCapsuleComponent());
		C->SetStaticMesh(Mesh);
		C->SetMaterial(0, Mat);
		C->SetRelativeLocation(RelLoc);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetOwnerNoSee(true); // first person: you don't see your own body
		return C;
	}
}

ATaGCharacter::ATaGCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, CapsuleHalfHeight);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Body: simple silhouette that matches the capsule (hitbox == what you see, ROSTER.md)
	BodyMesh = MakePart(this, TEXT("Body"), CylinderMesh.Object, ShapeMat.Object, FVector(0.f, 0.f, -12.f), FVector(0.62f, 0.62f, 1.55f));
	HeadMesh = MakePart(this, TEXT("Head"), SphereMesh.Object, ShapeMat.Object, FVector(0.f, 0.f, 79.f), FVector(0.26f));
	RingMesh = MakePart(this, TEXT("TaGRing"), CylinderMesh.Object, ShapeMat.Object, FVector(0.f, 0.f, 9.f), FVector(0.74f, 0.74f, 0.05f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, TaG::EyeHeight - CapsuleHalfHeight));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(95.f);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = TaG::WalkSpeed;
	Move->MaxAcceleration = TaG::Accel;
	Move->BrakingDecelerationWalking = TaG::Accel;
	Move->GroundFriction = 8.f;
	Move->JumpZVelocity = TaG::JumpVelocity;
	Move->GravityScale = 1.f;
	Move->AirControl = 0.25f;
	Move->bOrientRotationToMovement = false;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AutoPossessAI = EAutoPossessAI::Disabled;
}

void ATaGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATaGCharacter, Team);
	DOREPLIFETIME_CONDITION(ATaGCharacter, bCharging, COND_SkipOwner);
	DOREPLIFETIME(ATaGCharacter, RecoverTimeLeft);
}

UAbilitySystemComponent* ATaGCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ATaGCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyTeamColor(); // team 0 never triggers OnRep_Team on clients
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

void ATaGCharacter::InitAbilities()
{
	if (!HasAuthority() || bAbilitiesGiven)
	{
		return;
	}
	bAbilitiesGiven = true;
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(UTaGAbility_Shoot::StaticClass(), 1, static_cast<int32>(ETaGAbilityInput::Shoot), this));
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(UTaGAbility_Block::StaticClass(), 1, static_cast<int32>(ETaGAbilityInput::Block), this));
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(UTaGAbility_Steal::StaticClass(), 1, static_cast<int32>(ETaGAbilityInput::Steal), this));
}

void ATaGCharacter::SetTeam(int32 InTeam)
{
	Team = InTeam;
	ApplyTeamColor();
}

void ATaGCharacter::OnRep_Team()
{
	ApplyTeamColor();
}

void ATaGCharacter::ApplyTeamColor()
{
	// Team 0 = blue (you), team 1 = red (opponent). The TaG ring glows cyan.
	const FLinearColor BodyColor = Team == 0 ? FLinearColor(0.08f, 0.25f, 0.85f) : FLinearColor(0.85f, 0.12f, 0.1f);
	const FLinearColor HeadColor(0.85f, 0.65f, 0.5f);
	const FLinearColor RingColor(0.1f, 0.9f, 1.f);
	auto Paint = [](UStaticMeshComponent* C, const FLinearColor& Color)
	{
		if (C)
		{
			if (UMaterialInstanceDynamic* M = C->CreateAndSetMaterialInstanceDynamic(0))
			{
				M->SetVectorParameterValue(TEXT("Color"), Color);
			}
		}
	};
	Paint(BodyMesh, BodyColor);
	Paint(HeadMesh, HeadColor);
	Paint(RingMesh, RingColor);
}

FVector ATaGCharacter::GetFeetLocation() const
{
	return GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

FVector ATaGCharacter::GetEyeLocation() const
{
	return GetFeetLocation() + FVector(0.f, 0.f, TaG::EyeHeight);
}

FVector ATaGCharacter::GetBlockHandLocation() const
{
	return GetEyeLocation() + GetBaseAimRotation().Vector() * TaG::BlockHandReach + FVector(0.f, 0.f, 25.f);
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

ATaGCharacter* ATaGCharacter::FindOpponent() const
{
	ATaGCharacter* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->GetTeam() != Team)
		{
			const float D = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
			if (D < BestDist)
			{
				BestDist = D;
				Best = *It;
			}
		}
	}
	return Best;
}

// ---------------------------------------------------------------- Shoot

bool ATaGCharacter::CanStartShot() const
{
	return !bCharging && !IsRecovering() && HasBall() && GetCharacterMovement()->IsMovingOnGround();
}

void ATaGCharacter::BeginShot()
{
	if (bCharging)
	{
		return;
	}
	bCharging = true;
	ChargeTime = 0.f;
	bAutoJumped = false;
	AbilitySystem->AddLooseGameplayTag(TaGTags::State_Charging);
	UpdateMoveSpeed();
}

void ATaGCharacter::ReleaseShot()
{
	if (!bCharging)
	{
		return;
	}
	bCharging = false;
	AbilitySystem->RemoveLooseGameplayTag(TaGTags::State_Charging);
	UpdateMoveSpeed();
	if (HasAuthority())
	{
		ResolveShotOnServer(ChargeTime);
	}
}

void ATaGCharacter::ResolveShotOnServer(float Charge)
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		GM->ResolveShot(this, Charge);
	}
}

// ---------------------------------------------------------------- Block / Steal

bool ATaGCharacter::CanBlock() const
{
	return !bCharging && !IsRecovering() && !HasBall() && GetWorld()->GetTimeSeconds() >= NextBlockTime;
}

void ATaGCharacter::StartBlock()
{
	BlockTimeLeft = TaG::BlockWindow;
	NextBlockTime = GetWorld()->GetTimeSeconds() + TaG::BlockCooldown;
}

bool ATaGCharacter::CanSteal() const
{
	return !bCharging && !IsRecovering() && !HasBall() && GetWorld()->GetTimeSeconds() >= NextStealTime;
}

void ATaGCharacter::StartSteal()
{
	NextStealTime = GetWorld()->GetTimeSeconds() + TaG::StealCooldown;
	if (HasAuthority())
	{
		if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
		{
			if (!GM->ResolveSteal(this))
			{
				RecoverTimeLeft = TaG::StealWhiffRecover;
				AbilitySystem->AddLooseGameplayTag(TaGTags::State_Recovering);
			}
		}
	}
}

// ---------------------------------------------------------------- Tick

void ATaGCharacter::UpdateMoveSpeed()
{
	float Speed = TaG::WalkSpeed;
	if (bCharging)
	{
		Speed = TaG::ShootMoveSpeed;
	}
	else if (IsRecovering())
	{
		Speed = 200.f;
	}
	else if (HasBall())
	{
		Speed = TaG::DribbleSpeed;
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void ATaGCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bStopJumpNextTick)
	{
		bStopJumpNextTick = false;
		StopJumping();
	}

	BlockTimeLeft = FMath::Max(0.f, BlockTimeLeft - DeltaSeconds);
	if (RecoverTimeLeft > 0.f)
	{
		RecoverTimeLeft = FMath::Max(0.f, RecoverTimeLeft - DeltaSeconds);
		if (RecoverTimeLeft <= 0.f && AbilitySystem->HasMatchingGameplayTag(TaGTags::State_Recovering))
		{
			AbilitySystem->RemoveLooseGameplayTag(TaGTags::State_Recovering);
		}
	}

	if (bCharging)
	{
		ChargeTime += DeltaSeconds;
		if (!bAutoJumped && ChargeTime >= TaG::ShotAutoJumpTime)
		{
			// Gather, then rise: the apex lands inside the perfect window.
			bAutoJumped = true;
			Jump();
			bStopJumpNextTick = true;
		}
		if (IsLocallyControlled() && IsPlayerControlled())
		{
			// Lock-on: the view is pulled to the rim while charging.
			if (AController* C = GetController())
			{
				const FRotator Want = (TaG::RimCenter() - GetEyeLocation()).Rotation();
				C->SetControlRotation(FMath::RInterpTo(C->GetControlRotation(), Want, DeltaSeconds, 10.f));
			}
		}
		if (ChargeTime >= TaG::ShotAutoRelease && (HasAuthority() || IsLocallyControlled()))
		{
			ReleaseShot();
		}
	}
	else if (!HasAuthority() && !IsLocallyControlled())
	{
		ChargeTime = 0.f;
	}

	UpdateMoveSpeed();
}

void ATaGCharacter::PlaceAt(const FVector& FeetPos, const FVector& LookAt)
{
	if (bCharging)
	{
		bCharging = false;
		AbilitySystem->RemoveLooseGameplayTag(TaGTags::State_Charging);
	}
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

// ---------------------------------------------------------------- Input

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
	EIC->BindAction(A.Shoot, ETriggerEvent::Started, this, &ATaGCharacter::OnShootPressed);
	EIC->BindAction(A.Shoot, ETriggerEvent::Completed, this, &ATaGCharacter::OnShootReleased);
	EIC->BindAction(A.Block, ETriggerEvent::Started, this, &ATaGCharacter::OnBlockPressed);
	EIC->BindAction(A.Steal, ETriggerEvent::Started, this, &ATaGCharacter::OnStealPressed);
	EIC->BindAction(A.Restart, ETriggerEvent::Started, this, &ATaGCharacter::OnRestartPressed);
}

void ATaGCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	if (AController* C = GetController())
	{
		const FRotator Yaw(0.f, C->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), V.Y);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), V.X);
	}
}

void ATaGCharacter::AddLook(float YawDeg, float PitchDeg)
{
	if (bCharging)
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
	AddLook(V.X * MouseDegPerCount, V.Y * MouseDegPerCount);
}

void ATaGCharacter::OnLookStick(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	const float Dt = GetWorld()->GetDeltaSeconds();
	// Squared response for fine aim near the center
	AddLook(V.X * FMath::Abs(V.X) * StickYawRate * Dt, V.Y * FMath::Abs(V.Y) * StickPitchRate * Dt);
}

void ATaGCharacter::OnJumpPressed()
{
	if (!bCharging)
	{
		Jump();
	}
}

void ATaGCharacter::OnJumpReleased()
{
	StopJumping();
}

void ATaGCharacter::OnShootPressed()
{
	AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(ETaGAbilityInput::Shoot));
}

void ATaGCharacter::OnShootReleased()
{
	AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(ETaGAbilityInput::Shoot));
}

void ATaGCharacter::OnBlockPressed()
{
	AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(ETaGAbilityInput::Block));
	AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(ETaGAbilityInput::Block));
}

void ATaGCharacter::OnStealPressed()
{
	AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(ETaGAbilityInput::Steal));
	AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(ETaGAbilityInput::Steal));
}

void ATaGCharacter::OnRestartPressed()
{
	if (ATaGPlayerController* PC = Cast<ATaGPlayerController>(GetController()))
	{
		PC->ServerRequestRestart();
	}
}
