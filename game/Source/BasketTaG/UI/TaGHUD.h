#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TaGHUD.generated.h"

class SWidget;

/**
 * P1 HUD built with Slate in C++ (no widget assets): score, shot clock, clear reminder,
 * the shot meter with its good / perfect windows and the live make chance, and popups.
 */
UCLASS()
class BASKETTAG_API ATaGHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	TSharedPtr<SWidget> Root;
};
