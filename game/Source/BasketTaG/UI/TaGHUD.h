#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Core/TaGGameState.h"
#include "TaGHUD.generated.h"

class SWidget;
class SBox;
class STaGRoot;

/**
 * All screens are Slate built in C++ (no widget assets): title, lobby / hero select, the match HUD
 * (score, clock, shot meter, hands and arts with cooldowns, read-win banner, player markers), pause and results.
 */
UCLASS()
class BASKETTAG_API ATaGHUD : public AHUD
{
	GENERATED_BODY()

public:
	ATaGHUD();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void RebuildMenu();

	TSharedPtr<STaGRoot> Root;
	TSharedPtr<SBox> MenuSlot;
	TSharedPtr<SWidget> FocusTarget;
	ETaGPhase ShownPhase = ETaGPhase::Title;
	bool bShownPause = false;
	bool bShownSettings = false;
	bool bShownTutorialEnd = false;
	bool bBuiltOnce = false;
	int32 ShownLobbyVersion = -1;
};
