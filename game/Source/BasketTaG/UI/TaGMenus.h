#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class ATaGPlayerController;
class SWidget;

/** Root of all TaG screens: catches Escape / Start for the pause menu even while a menu has focus. */
class STaGRoot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STaGRoot) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ATaGPlayerController>, PC)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	TWeakObjectPtr<ATaGPlayerController> PC;
};

/** Names over heads (ally teal / enemy orange), the ball carrier, downed players and your pass target. */
class STaGWorldMarkers : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STaGWorldMarkers) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ATaGPlayerController>, PC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }

private:
	TWeakObjectPtr<ATaGPlayerController> PC;
};

namespace TaGUI
{
	FSlateFontInfo Font(float Size, bool bBold = true);
	TSharedRef<SWidget> MakeMatchHUD(TWeakObjectPtr<ATaGPlayerController> PC);
	TSharedRef<SWidget> MakeTitle(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
	TSharedRef<SWidget> MakeLobby(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
	TSharedRef<SWidget> MakeResults(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
	TSharedRef<SWidget> MakePause(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
	TSharedRef<SWidget> MakeSettings(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
	TSharedRef<SWidget> MakeTutorialDone(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
}
