#include "Character/TaGPlayerController.h"
#include "Core/TaGHeroDisplay.h"
#include "Core/TaGReplay.h"
#include "Core/TaGTypes.h"
#include "Core/TaGPlayerState.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGAudio.h"
#include "Core/TaGGameMode.h"
#include "Core/TaGGameState.h"
#include "Core/TaGRules.h"
#include "Core/TaGTutorial.h"
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
	ATaGReplay::Get(GetWorld()); // start recording for highlight replays

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
	ApplySettings();

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

void ATaGPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalController())
	{
		// Leaving a match mid-rumble must not leave the pad (or the phone) buzzing.
		PlayDynamicForceFeedback(0.f, 0.f, true, true, true, true, EDynamicForceFeedbackAction::Stop);
	}
	Super::EndPlay(EndPlayReason);
}

void ATaGPlayerController::SetLookSensitivity(float Value)
{
	LookSensitivity = Value;
	ApplySettings();
}

void ATaGPlayerController::ApplySettings()
{
	LookSensitivity = FMath::Clamp(FMath::RoundToFloat(LookSensitivity * 10.f) / 10.f, 0.2f, 3.f);
	FieldOfView = FMath::Clamp(FieldOfView, 80.f, 110.f);
	ViewBob = FMath::Clamp(FMath::RoundToFloat(ViewBob * 4.f) / 4.f, 0.f, 1.5f);
	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.f, 1.f);
	if (IsLocalController())
	{
		TaGAudio::SetMasterVolume(this, MasterVolume);
		TaGAudio::SetMusicVolume(MusicVolume);
	}
	SaveConfig();
}

void ATaGPlayerController::SetSettingsOpen(bool bOpen)
{
	bSettingsOpen = bOpen;
}

void ATaGPlayerController::Feedback(float Intensity, float Duration, float CameraKick)
{
	if (!IsLocalController())
	{
		return;
	}
	if (bVibration && Intensity > 0.f)
	{
		PlayDynamicForceFeedback(FMath::Clamp(Intensity, 0.f, 1.f), Duration, true, true, true, true);
	}
	if (ATaGCharacter* Me = Cast<ATaGCharacter>(GetPawn()))
	{
		Me->AddCameraKick(CameraKick);
	}
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
	if (bSettingsOpen)
	{
		SetSettingsOpen(false); // Esc / Start / the pause button backs out of the settings first
		return;
	}
	const ATaGGameState* GS = GetWorld()->GetGameState<ATaGGameState>();
	if (!GS || GS->Phase != ETaGPhase::Playing || GS->IsTutorialFinished())
	{
		return;
	}
	bPauseMenu = !bPauseMenu;
	if (GetNetMode() == NM_Standalone)
	{
		UGameplayStatics::SetGamePaused(this, bPauseMenu);
	}
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
	// A tackle winding up at me: a cue sound and a short rumble, so it can be answered even from outside the view
	if (ATaGCharacter* Me = bPlaying ? Cast<ATaGCharacter>(GetPawn()) : nullptr)
	{
		bool bThreat = false;
		for (TActorIterator<ATaGCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsTackleThreatTo(Me))
			{
				bThreat = true;
				break;
			}
		}
		if (bThreat && !bThreatCued)
		{
			TaGAudio::Play2D(this, TEXT("TackleCue"), 0.9f);
			Feedback(0.3f, 0.12f, 0.f);
			UE_LOG(LogTemp, Display, TEXT("THREATCUE tackle at %s"), Me->GetHeroDef().Key);
		}
		bThreatCued = bThreat;
	}
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
	if (FParse::Param(FCommandLine::Get(), TEXT("TaGTutorialTest")))
	{
		TickTutorialTest(DeltaSeconds);
	}
	const bool bWantUI = !bPlaying || bPauseMenu || bSettingsOpen || !GetPawn() || (GS && GS->IsTutorialFinished());
	if (bWantUI != bUIMode)
	{
		SetUIMode(bWantUI);
	}
	UpdateSpectatorCamera(DeltaSeconds);
}

void ATaGPlayerController::UpdateSpectatorCamera(float Dt)
{
	if (!OrbitCamera || bExternalView)
	{
		return;
	}
	// Highlight replay: the broadcast camera
	if (ATaGReplay::IsPlaying(GetWorld()))
	{
		if (ATaGReplay* R = ATaGReplay::Get(GetWorld()))
		{
			if (GetViewTarget() != R)
			{
				SetViewTarget(R);
			}
		}
		return;
	}
	// Hero select: the chosen hero on the court, in front of its own camera
	const ATaGGameState* LobbyGS = GetWorld()->GetGameState<ATaGGameState>();
	const bool bLobby = LobbyGS && (LobbyGS->Phase == ETaGPhase::Lobby || LobbyGS->Phase == ETaGPhase::Title) && !GetPawn();
	if (bLobby)
	{
		if (!LobbyHero)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			LobbyHero = GetWorld()->SpawnActor<ATaGHeroDisplay>(ATaGHeroDisplay::StaticClass(), FTransform::Identity, Params);
		}
		if (LobbyHero)
		{
			const ATaGPlayerState* MyPS = GetPlayerState<ATaGPlayerState>();
			const ETaGHero Want = MyPS ? MyPS->Hero : ETaGHero::Rei;
			if (!LobbyHero->HasHero() || LobbyHero->GetHero() != Want)
			{
				LobbyHero->ShowHero(Want, TaG::AllyColor(), true);
				LobbyHero->FrameForSelect(FVector(470.f, 150.f, 0.f), 0.f);
			}
			if (GetViewTarget() != LobbyHero)
			{
				SetViewTarget(LobbyHero);
			}
		}
		return;
	}
	if (LobbyHero)
	{
		LobbyHero->Destroy();
		LobbyHero = nullptr;
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

// ================================================================ Tutorial test

void ATaGPlayerController::TickTutorialTest(float Dt)
{
	const ATaGGameState* GS = GetWorld()->GetGameState<ATaGGameState>();
	UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!GS || !GS->bTutorial || !Sub)
	{
		return;
	}
	const float RealDt = FApp::GetDeltaTime();
	TutTestClock += RealDt;
	if (GS->TutorialStep != TutTestStep)
	{
		UE_LOG(LogTemp, Display, TEXT("TUTORIALTEST lesson %d -> %d at %.1f s"), TutTestStep, GS->TutorialStep, TutTestClock);
		TutTestStep = GS->TutorialStep;
		TutTestT = 0.f;
		TutTestNext = 0.f;
		bTutTestShootHeld = false;
		bTutTestContestHeld = false;
		for (UInputAction* A : { Actions.Move.Get(), Actions.Shoot.Get(), Actions.Secondary.Get() })
		{
			Sub->StopContinuousInputInjectionForAction(A);
		}
		if (TutTestStep >= TaGTutorial::Done)
		{
			UE_LOG(LogTemp, Display, TEXT("TUTORIALTEST done in %.1f s, finished=%d"), TutTestClock, GS->IsTutorialFinished() ? 1 : 0);
			FPlatformMisc::RequestExit(false);
		}
		return;
	}
	if (TutTestClock > 300.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("TUTORIALTEST timeout in lesson %d (progress %d/%d)"), TutTestStep, GS->TutorialProgress, GS->TutorialGoal);
		FPlatformMisc::RequestExit(false);
		return;
	}
	ATaGCharacter* Me = Cast<ATaGCharacter>(GetPawn());
	if (!Me)
	{
		return;
	}
	TutTestT += RealDt;
	ATaGCharacter* Opp = Me->FindNearestOpponent();
	uint8 Kind = 0;
	const bool bTell = Opp && Opp->IsPretelling(Kind);
	const ETaGAction OA = Opp ? Opp->GetAction() : ETaGAction::None;
	const bool bFree = Me->GetAction() == ETaGAction::None;
	auto Tap = [Sub](UInputAction* A) { Sub->InjectInputForAction(A, FInputActionValue(true), {}, {}); };

	switch (TutTestStep)
	{
	case TaGTutorial::Move:
		Sub->StartContinuousInputInjectionForAction(Actions.Move, FInputActionValue(FVector2D(0.f, 1.f)), {}, {});
		break;
	case TaGTutorial::Shoot:
	case TaGTutorial::Perfect:
		// Hold, release at the top of the jump (the perfect window), let go of the button between shots.
		if (bTutTestShootHeld)
		{
			if ((Me->IsChargingShot() && Me->GetShotCharge() >= 0.74f) || (!Me->IsChargingShot() && TutTestT - TutTestHeldSince > 0.3f))
			{
				Sub->StopContinuousInputInjectionForAction(Actions.Shoot);
				bTutTestShootHeld = false;
			}
		}
		else if (Me->HasBall() && bFree && TutTestT > TutTestNext)
		{
			Sub->StartContinuousInputInjectionForAction(Actions.Shoot, FInputActionValue(true), {}, {});
			bTutTestShootHeld = true;
			TutTestHeldSince = TutTestT;
			TutTestNext = TutTestT + 2.f;
		}
		break;
	case TaGTutorial::Drive:
		if (Me->HasBall() && bFree && TutTestT > TutTestNext)
		{
			Tap(Actions.Primary);
			TutTestNext = TutTestT + 1.f;
		}
		break;
	case TaGTutorial::DriveVsContest:
		// Answer the contest with a drive once its hands are up.
		if (Opp && (Opp->IsContesting() || Opp->IsContestStarting()) && Me->HasBall() && bFree)
		{
			Tap(Actions.Primary);
		}
		break;
	case TaGTutorial::DodgeVsTackle:
		// Answer the tackle with a dodge as it comes (a human reacting to the orange tell lands about here).
		if (Opp && OA == ETaGAction::TackleStart && Me->HasBall() && bFree && TutTestT > TutTestNext)
		{
			Tap(Actions.Secondary);
			TutTestNext = TutTestT + 0.6f;
		}
		break;
	case TaGTutorial::TackleVsDrive:
		// Answer the drive with a tackle as it starts.
		if (Opp && (OA == ETaGAction::DriveStart || OA == ETaGAction::Drive) && bFree)
		{
			Tap(Actions.Primary);
		}
		break;
	case TaGTutorial::ContestVsShot:
		// Answer the shot with a held contest.
		if (!bTutTestContestHeld && Opp && ((bTell && Kind == 4) || Opp->IsChargingShot()) && bFree)
		{
			Sub->StartContinuousInputInjectionForAction(Actions.Secondary, FInputActionValue(true), {}, {});
			bTutTestContestHeld = true;
			TutTestHeldSince = TutTestT;
		}
		else if (bTutTestContestHeld && TutTestT - TutTestHeldSince > 1.4f)
		{
			Sub->StopContinuousInputInjectionForAction(Actions.Secondary);
			bTutTestContestHeld = false;
		}
		break;
	case TaGTutorial::Art:
		if (Me->HasBall() && bFree && TutTestT > 1.f && TutTestT > TutTestNext)
		{
			Tap(Actions.Art1);
			TutTestNext = TutTestT + 2.f;
		}
		break;
	default:
		break;
	}
}

// ================================================================ Title

void ATaGPlayerController::PlayOffline()
{
	ServerEnterLobby(3, 1);
}

void ATaGPlayerController::QuickPlay()
{
	ServerQuickPlay();
}

void ATaGPlayerController::StartTutorial()
{
	ServerStartTutorial();
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
	bSettingsOpen = false;
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

void ATaGPlayerController::ServerQuickPlay_Implementation()
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		if (IsLocalController())
		{
			GM->EnterLobby(3, 1);
			GM->StartMatch();
		}
	}
}

void ATaGPlayerController::ServerStartTutorial_Implementation()
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		if (IsLocalController())
		{
			GM->StartTutorial(this);
		}
	}
}

void ATaGPlayerController::ServerTutorialSkip_Implementation()
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		if (IsLocalController())
		{
			GM->TutorialAdvance(true);
		}
	}
}
