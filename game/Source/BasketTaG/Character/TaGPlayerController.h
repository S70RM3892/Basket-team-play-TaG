#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TaGPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/** Input actions built in C++ (P1 keeps no input assets in the repository). */
USTRUCT()
struct FTaGInputActions
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UInputAction> Move;
	UPROPERTY() TObjectPtr<UInputAction> LookMouse;
	UPROPERTY() TObjectPtr<UInputAction> LookStick;
	UPROPERTY() TObjectPtr<UInputAction> Jump;
	UPROPERTY() TObjectPtr<UInputAction> Shoot;
	UPROPERTY() TObjectPtr<UInputAction> Block;
	UPROPERTY() TObjectPtr<UInputAction> Steal;
	UPROPERTY() TObjectPtr<UInputAction> Restart;
};

UCLASS()
class BASKETTAG_API ATaGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATaGPlayerController();

	const FTaGInputActions& GetInputActions() const { return Actions; }

	UFUNCTION(Server, Reliable)
	void ServerRequestRestart();

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	FTaGInputActions Actions;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> Mapping;
};
