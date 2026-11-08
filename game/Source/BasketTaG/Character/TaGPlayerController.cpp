#include "Character/TaGPlayerController.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGGameMode.h"
#include "Core/TaGGameState.h"
#include "Core/TaGRules.h"
#include "Core/TaGTypes.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	UInputAction* MakeAction(APlayerController* Owner, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = Owner->CreateDefaultSubobject<UInputAction>(Name);
		A->ValueType = Type;
		return A;
	}

	void MapAxisKey(UInputMappingContext* IMC, UInputAction* Action, const FKey& Key, bool bSwizzleToY, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = IMC->MapKey(Action, Key);
		if (bSwizzleToY)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(IMC);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			M.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			M.Modifiers.Add(NewObject<UInputModifierNegate>(IMC));
		}
	}

	void MapStick(UInputMappingContext* IMC, UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& M = IMC->MapKey(Action, Key);
		UInputModifierDeadZone* DZ = NewObject<UInputModifierDeadZone>(IMC);
		DZ->LowerThreshold = 0.18f;
		M.Modifiers.Add(DZ);
	}
}

ATaGPlayerController::ATaGPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	// Actions exist from construction so the pawn can bind them whenever it is possessed.
	Actions.Move = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	Actions.LookMouse = MakeAction(this, TEXT("IA_LookMouse"), EInputActionValueType::Axis2D);
	Actions.LookStick = MakeAction(this, TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
	Actions.Jump = MakeAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	Actions.Primary = MakeAction(this, TEXT("IA_Primary"), EInputActionValueType::Boolean);
	Actions.Secondary = MakeAction(this, TEXT("IA_Secondary"), EInputActionValueType::Boolean);
	Actions.Step = MakeAction(this, TEXT("IA_Step"), EInputActionValueType::Boolean);
	Actions.PassSteal = MakeAction(this, TEXT("IA_PassSteal"), EInputActionValueType::Boolean);
	Actions.Shoot = MakeAction(this, TEXT("IA_Shoot"), EInputActionValueType::Boolean);
	Actions.Art1 = MakeAction(this, TEXT("IA_Art1"), EInputActionValueType::Boolean);
	Actions.Art2 = MakeAction(this, TEXT("IA_Art2"), EInputActionValueType::Boolean);
	Actions.Pause = MakeAction(this, TEXT("IA_Pause"), EInputActionValueType::Boolean);
	Actions.CameraToggle = MakeAction(this, TEXT("IA_CameraToggle"), EInputActionValueType::Boolean);
}

void ATaGPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_TaG"));
	// Move / look
	MapAxisKey(Mapping, Actions.Move, EKeys::W, true, false);
	MapAxisKey(Mapping, Actions.Move, EKeys::S, true, true);
	MapAxisKey(Mapping, Actions.Move, EKeys::D, false, false);
	MapAxisKey(Mapping, Actions.Move, EKeys::A, false, true);
	MapStick(Mapping, Actions.Move, EKeys::Gamepad_Left2D);
	Mapping->MapKey(Actions.LookMouse, EKeys::Mouse2D);
	MapStick(Mapping, Actions.LookStick, EKeys::Gamepad_Right2D);
	// Buttons: controller layout follows the Godot prototype (LB/RB hands, B step, X steal), plus pass and arts.
	Mapping->MapKey(Actions.Jump, EKeys::SpaceBar);
	Mapping->MapKey(Actions.Jump, EKeys::Gamepad_FaceButton_Bottom);
	Mapping->MapKey(Actions.Shoot, EKeys::LeftMouseButton);
	Mapping->MapKey(Actions.Shoot, EKeys::Gamepad_RightTrigger);
	Mapping->MapKey(Actions.Primary, EKeys::Q);
	Mapping->MapKey(Actions.Primary, EKeys::Gamepad_LeftShoulder);
	Mapping->MapKey(Actions.Secondary, EKeys::E);
	Mapping->MapKey(Actions.Secondary, EKeys::Gamepad_RightShoulder);
	Mapping->MapKey(Actions.Step, EKeys::LeftShift);
	Mapping->MapKey(Actions.Step, EKeys::Gamepad_FaceButton_Right);
	Mapping->MapKey(Actions.PassSteal, EKeys::F);
	Mapping->MapKey(Actions.PassSteal, EKeys::RightMouseButton);
	Mapping->MapKey(Actions.PassSteal, EKeys::Gamepad_FaceButton_Left);
	Mapping->MapKey(Actions.Art1, EKeys::C);
	Mapping->MapKey(Actions.Art1, EKeys::Gamepad_LeftTrigger);
	Mapping->MapKey(Actions.Art2, EKeys::V);
	Mapping->MapKey(Actions.Art2, EKeys::Gamepad_FaceButton_Top);
	Mapping->MapKey(Actions.Pause, EKeys::Escape);
	Mapping->MapKey(Actions.Pause, EKeys::Gamepad_Special_Right);
	Mapping->MapKey(Actions.CameraToggle, EKeys::T);
	Mapping->MapKey(Actions.CameraToggle, EKeys::Gamepad_Special_Left);

	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Sub->AddMappingContext(Mapping, 0);
	}
	// No engine debug text over the game (asset compile status, shadow warnings) in development builds.
	ConsoleCommand(TEXT("DisableAllScreenMessages"));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	OrbitCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FVector(1400.f, 900.f, 600.f)), Params);
	if (OrbitCamera && OrbitCamera->GetCameraComponent())
	{
		OrbitCamera->GetCameraComponent()->SetFieldOfView(70.f);
		OrbitCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	SetUIMode(true);
}

bool ATaGPlayerController::IsHost() const
{
	return HasAuthority() && IsLocalController();
}

void ATaGPlayerController::SetLookSensitivity(float Value)
{
	LookSensitivity = FMath::Clamp(Value, 0.2f, 3.f);
	SaveConfig();
}

void ATaGPlayerController::SetUIMode(bool bUI)
{
	if (!IsLocalController())
	{
		return;
	}
	bUIMode = bUI;
	if (bUI)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		bShowMouseCursor = true;
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

void ATaGPlayerController::TogglePauseMenu()
{
	const ATaGGameState* GS = GetWorld()->GetGameState<ATaGGameState>();
	if (!GS || GS->Phase != ETaGPhase::Playing)
	{
		return;
	}
	bPauseMenu = !bPauseMenu;
	if (GetNetMode() == NM_Standalone)
	{
		UGameplayStatics::SetGamePaused(this, bPauseMenu);
	}
}

void ATaGPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void ATaGPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

void ATaGPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsLocalController())
	{
		return;
	}
	const ATaGGameState* GS = GetWorld()->GetGameState<ATaGGameState>();
	const bool bPlaying = GS && GS->Phase == ETaGPhase::Playing;
	if (!bPlaying && bPauseMenu)
	{
		bPauseMenu = false;
		if (GetNetMode() == NM_Standalone)
		{
			UGameplayStatics::SetGamePaused(this, false);
		}
	}
	// -TaGNetClient: network smoke test log (what this client sees)
	if (GetNetMode() == NM_Client && FParse::Param(FCommandLine::Get(), TEXT("TaGNetClient")))
	{
		NetLogTimer += DeltaSeconds;
		if (NetLogTimer > 5.f)
		{
			NetLogTimer = 0.f;
			int32 Chars = 0;
			for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It) { Chars++; }
			const ATaGCharacter* Me = Cast<ATaGCharacter>(GetPawn());
			UE_LOG(LogTemp, Display, TEXT("NETTEST client: phase=%d score=%d-%d clock=%.1f chars=%d myhero=%s myteam=%d"),
				GS ? static_cast<int32>(GS->Phase) : -1, GS ? GS->Score0 : -1, GS ? GS->Score1 : -1, GS ? GS->ShotClock : -1.f, Chars,
				Me ? Me->GetHeroDef().Key : TEXT("-"), Me ? Me->GetTeam() : -1);
		}
		NetTestTime += DeltaSeconds;
		if (NetTestTime > 70.f)
		{
			FPlatformMisc::RequestExit(false);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("TaGInputTest")))
	{
		TickInputTest(DeltaSeconds);
	}
	const bool bWantUI = !bPlaying || bPauseMenu || !GetPawn();
	if (bWantUI != bUIMode)
	{
		SetUIMode(bWantUI);
	}
	UpdateSpectatorCamera(DeltaSeconds);
}

void ATaGPlayerController::UpdateSpectatorCamera(float Dt)
{
	if (!OrbitCamera)
	{
		return;
	}
	if (GetPawn())
	{
		if (GetViewTarget() != GetPawn())
		{
			SetViewTarget(GetPawn());
		}
		return;
	}
	// No player: slow orbit over the rooftop so the city stays in view.
	OrbitAngle += Dt * 4.f;
	const float A = FMath::DegreesToRadians(OrbitAngle);
	const FVector Center(550.f, 0.f, 150.f);
	const FVector Pos = Center + FVector(FMath::Cos(A) * 1500.f, FMath::Sin(A) * 1500.f, 520.f);
	OrbitCamera->SetActorLocationAndRotation(Pos, (Center - Pos).Rotation());
	if (GetViewTarget() != OrbitCamera)
	{
		SetViewTarget(OrbitCamera);
	}
}

// ================================================================ Input test

void ATaGPlayerController::TickInputTest(float Dt)
{
	// -TaGInputTest: feed the real input path (Enhanced Input -> ability -> character) with injected presses.
	ATaGCharacter* Me = Cast<ATaGCharacter>(GetPawn());
	const ATaGGameState* GS = GetWorld()->GetGameState<ATaGGameState>();
	const ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>();
	if (!Me || !GS || GS->Phase != ETaGPhase::Playing || !GM || (!GM->IsLive() && InputTestStep == 0))
	{
		return;
	}
	UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Sub)
	{
		return;
	}
	InputTestT += FApp::GetDeltaTime();
	auto ActionName = [Me]() { return StaticEnum<ETaGAction>()->GetNameStringByValue(static_cast<int64>(Me->GetAction())); };
	struct FStep { float At; int32 Id; };
	const FStep Steps[] = { { 0.5f, 0 }, { 0.75f, 1 }, { 2.0f, 2 }, { 2.75f, 3 }, { 3.2f, 4 }, { 4.0f, 5 }, { 4.3f, 6 }, { 5.0f, 7 }, { 5.3f, 8 }, { 6.0f, 9 }, { 6.3f, 10 }, { 7.0f, 11 } };
	while (InputTestStep < UE_ARRAY_COUNT(Steps) && InputTestT >= Steps[InputTestStep].At)
	{
		switch (Steps[InputTestStep].Id)
		{
		case 0:
			UE_LOG(LogTemp, Display, TEXT("INPUTTEST start: hero=%s hasball=%d action=%s"), Me->GetHeroDef().Key, Me->HasBall() ? 1 : 0, *ActionName());
			Sub->InjectInputForAction(Actions.Primary, FInputActionValue(true), {}, {});
			break;
		case 1:
			UE_LOG(LogTemp, Display, TEXT("INPUTTEST primary -> action=%s drive_cd=%.1f"), *ActionName(), Me->GetCooldownLeft(ETaGSlot::Drive));
			break;
		case 2:
			UE_LOG(LogTemp, Display, TEXT("INPUTTEST before shot: hasball=%d action=%s"), Me->HasBall() ? 1 : 0, *ActionName());
			Sub->StartContinuousInputInjectionForAction(Actions.Shoot, FInputActionValue(true), {}, {});
			break;
		case 3:
			UE_LOG(LogTemp, Display, TEXT("INPUTTEST charging=%d charge=%.2f held=%.2f type=%s"), Me->IsChargingShot() ? 1 : 0, Me->GetShotCharge(), Me->GetActionTime(),
				FTaGRules::ShotType(Me->GetShotType()).Name);
			Sub->StopContinuousInputInjectionForAction(Actions.Shoot);
			break;
		case 4:
			UE_LOG(LogTemp, Display, TEXT("INPUTTEST after release: shots=%d action=%s hasball=%d"), GS->Stats(Me->GetTeam()).Shots, *ActionName(), Me->HasBall() ? 1 : 0);
			break;
		case 5:
			Sub->InjectInputForAction(Actions.Art1, FInputActionValue(true), {}, {});
			break;
		case 6:
			UE_LOG(LogTemp, Display, TEXT("INPUTTEST art1 -> scoped=%d action=%s art1_cd=%.1f"), Me->IsScoped() ? 1 : 0, *ActionName(), Me->GetCooldownLeft(ETaGSlot::Art1));
			break;
		case 7:
			Sub->InjectInputForAction(Actions.Pause, FInputActionValue(true), {}, {});
			break;
		case 8:
			UE_LOG(LogTemp, Display, TEXT("INPUTTEST pause -> open=%d"), bPauseMenu ? 1 : 0);
			if (bPauseMenu)
			{
				TogglePauseMenu();
			}
			break;
		case 9:
			Sub->InjectInputForAction(Actions.CameraToggle, FInputActionValue(true), {}, {});
			break;
		case 10:
			UE_LOG(LogTemp, Display, TEXT("INPUTTEST camera -> third_person=%d"), Me->IsThirdPerson() ? 1 : 0);
			break;
		default:
			UE_LOG(LogTemp, Display, TEXT("INPUTTEST done"));
			FPlatformMisc::RequestExit(false);
			break;
		}
		InputTestStep++;
	}
}

// ================================================================ Title

void ATaGPlayerController::PlayOffline()
{
	ServerEnterLobby(3, 1);
}

void ATaGPlayerController::HostOnline()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, TEXT("listen?Lobby=1?TeamSize=3?Bot=1"));
}

void ATaGPlayerController::JoinOnline(const FString& Address)
{
	if (Address.IsEmpty())
	{
		return;
	}
	LastJoinAddress = Address;
	SaveConfig();
	ClientTravel(Address, ETravelType::TRAVEL_Absolute);
}

void ATaGPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ATaGPlayerController::LeaveToTitle()
{
	UGameplayStatics::SetGamePaused(this, false);
	bPauseMenu = false;
	if (GetNetMode() == NM_Client)
	{
		ConsoleCommand(TEXT("disconnect"));
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true);
}

// ================================================================ Server RPCs

void ATaGPlayerController::ServerEnterLobby_Implementation(int32 TeamSize, int32 BotSkill)
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		if (IsLocalController())
		{
			GM->EnterLobby(TeamSize, BotSkill);
		}
	}
}

void ATaGPlayerController::ServerSetLobbyOptions_Implementation(int32 TeamSize, int32 BotSkill)
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		if (IsLocalController())
		{
			GM->SetLobbyOptions(TeamSize, BotSkill);
		}
	}
}

void ATaGPlayerController::ServerSetHero_Implementation(ETaGHero Hero)
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		GM->SetPlayerHero(this, Hero);
	}
}

void ATaGPlayerController::ServerSetTeam_Implementation(int32 Team)
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		GM->SetPlayerTeam(this, Team);
	}
}

void ATaGPlayerController::ServerStartMatch_Implementation()
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		if (IsLocalController())
		{
			GM->StartMatch();
		}
	}
}

void ATaGPlayerController::ServerRematch_Implementation()
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		if (IsLocalController())
		{
			GM->RestartMatch();
		}
	}
}

void ATaGPlayerController::ServerBackToLobby_Implementation()
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		if (IsLocalController())
		{
			GM->BackToLobby();
		}
	}
}
