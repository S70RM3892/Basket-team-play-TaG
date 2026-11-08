#include "Character/TaGPlayerController.h"
#include "Core/TaGGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

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

	void AddDeadZone(UInputMappingContext* IMC, UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& M = IMC->MapKey(Action, Key);
		UInputModifierDeadZone* DZ = NewObject<UInputModifierDeadZone>(IMC);
		DZ->LowerThreshold = 0.18f;
		M.Modifiers.Add(DZ);
	}
}

ATaGPlayerController::ATaGPlayerController()
{
	// Actions exist from construction so the pawn can bind them whenever it is possessed.
	Actions.Move = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	Actions.LookMouse = MakeAction(this, TEXT("IA_LookMouse"), EInputActionValueType::Axis2D);
	Actions.LookStick = MakeAction(this, TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
	Actions.Jump = MakeAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	Actions.Shoot = MakeAction(this, TEXT("IA_Shoot"), EInputActionValueType::Boolean);
	Actions.Block = MakeAction(this, TEXT("IA_Block"), EInputActionValueType::Boolean);
	Actions.Steal = MakeAction(this, TEXT("IA_Steal"), EInputActionValueType::Boolean);
	Actions.Restart = MakeAction(this, TEXT("IA_Restart"), EInputActionValueType::Boolean);
}

void ATaGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_TaG"));

	// Move: WASD / left stick
	MapAxisKey(Mapping, Actions.Move, EKeys::W, true, false);
	MapAxisKey(Mapping, Actions.Move, EKeys::S, true, true);
	MapAxisKey(Mapping, Actions.Move, EKeys::D, false, false);
	MapAxisKey(Mapping, Actions.Move, EKeys::A, false, true);
	AddDeadZone(Mapping, Actions.Move, EKeys::Gamepad_Left2D);

	// Look: mouse / right stick
	Mapping->MapKey(Actions.LookMouse, EKeys::Mouse2D);
	AddDeadZone(Mapping, Actions.LookStick, EKeys::Gamepad_Right2D);

	// Buttons (controller layout follows the Godot prototype; Y also shoots for devices with odd trigger mapping)
	Mapping->MapKey(Actions.Jump, EKeys::SpaceBar);
	Mapping->MapKey(Actions.Jump, EKeys::Gamepad_FaceButton_Bottom);
	Mapping->MapKey(Actions.Shoot, EKeys::LeftMouseButton);
	Mapping->MapKey(Actions.Shoot, EKeys::Gamepad_RightTrigger);
	Mapping->MapKey(Actions.Shoot, EKeys::Gamepad_FaceButton_Top);
	Mapping->MapKey(Actions.Block, EKeys::RightMouseButton);
	Mapping->MapKey(Actions.Block, EKeys::Gamepad_RightShoulder);
	Mapping->MapKey(Actions.Steal, EKeys::F);
	Mapping->MapKey(Actions.Steal, EKeys::Gamepad_FaceButton_Left);
	Mapping->MapKey(Actions.Restart, EKeys::R);
	Mapping->MapKey(Actions.Restart, EKeys::Gamepad_Special_Right);

	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Sub->AddMappingContext(Mapping, 0);
	}

	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;
}

void ATaGPlayerController::ServerRequestRestart_Implementation()
{
	if (ATaGGameMode* GM = GetWorld()->GetAuthGameMode<ATaGGameMode>())
	{
		GM->RestartMatch();
	}
}
