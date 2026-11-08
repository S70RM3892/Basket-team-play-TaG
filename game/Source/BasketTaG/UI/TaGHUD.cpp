#include "UI/TaGHUD.h"
#include "Character/TaGPlayerController.h"
#include "UI/TaGMenus.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

ATaGHUD::ATaGHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ATaGHUD::BeginPlay()
{
	Super::BeginPlay();
	ATaGPlayerController* PC = Cast<ATaGPlayerController>(GetOwningPlayerController());
	if (!PC || !PC->IsLocalController() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	TWeakObjectPtr<ATaGPlayerController> WeakPC = PC;
	Root = SNew(STaGRoot).PC(WeakPC)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				TaGUI::MakeMatchHUD(WeakPC)
			]
			+ SOverlay::Slot()
			[
				SAssignNew(MenuSlot, SBox)
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(Root.ToSharedRef(), 10);
	bBuiltOnce = false;
}

void ATaGHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Root.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Root.ToSharedRef());
	}
	Root.Reset();
	MenuSlot.Reset();
	Super::EndPlay(EndPlayReason);
}

void ATaGHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Root.IsValid())
	{
		return;
	}
	const ATaGGameState* GS = GetWorld()->GetGameState<ATaGGameState>();
	const ATaGPlayerController* PC = Cast<ATaGPlayerController>(GetOwningPlayerController());
	if (!GS || !PC)
	{
		return;
	}
	const bool bPause = PC->IsPauseMenuOpen();
	const bool bSettings = PC->IsSettingsOpen();
	const bool bTutorialEnd = GS->IsTutorialFinished();
	if (!bBuiltOnce || GS->Phase != ShownPhase || bPause != bShownPause || bSettings != bShownSettings || bTutorialEnd != bShownTutorialEnd)
	{
		bBuiltOnce = true;
		ShownPhase = GS->Phase;
		bShownPause = bPause;
		bShownSettings = bSettings;
		bShownTutorialEnd = bTutorialEnd;
		RebuildMenu();
	}
}

void ATaGHUD::RebuildMenu()
{
	if (!MenuSlot.IsValid())
	{
		return;
	}
	TWeakObjectPtr<ATaGPlayerController> PC = Cast<ATaGPlayerController>(GetOwningPlayerController());
	FocusTarget.Reset();
	TSharedPtr<SWidget> Content;
	if (bShownSettings)
	{
		Content = TaGUI::MakeSettings(PC, FocusTarget);
	}
	else
	{
		switch (ShownPhase)
		{
		case ETaGPhase::Title:
			Content = TaGUI::MakeTitle(PC, FocusTarget);
			break;
		case ETaGPhase::Lobby:
			Content = TaGUI::MakeLobby(PC, FocusTarget);
			break;
		case ETaGPhase::Results:
			Content = TaGUI::MakeResults(PC, FocusTarget);
			break;
		default:
			if (bShownTutorialEnd)
			{
				Content = TaGUI::MakeTutorialDone(PC, FocusTarget);
			}
			else if (bShownPause)
			{
				Content = TaGUI::MakePause(PC, FocusTarget);
			}
			break;
		}
	}
	if (Content.IsValid())
	{
		MenuSlot->SetContent(Content.ToSharedRef());
		MenuSlot->SetVisibility(EVisibility::Visible);
		if (FocusTarget.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(FocusTarget, EFocusCause::SetDirectly);
		}
	}
	else
	{
		MenuSlot->SetContent(SNullWidget::NullWidget);
		MenuSlot->SetVisibility(EVisibility::Collapsed);
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
}
