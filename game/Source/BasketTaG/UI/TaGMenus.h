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

/** A bar split into equal segments (the guard: one per 25), filled up to Value of Max. */
class STaGSegmentBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STaGSegmentBar) : _Value(0.f), _Max(100.f), _Segment(25.f), _Color(FLinearColor::White), _Size(FVector2D(200.f, 12.f)) {}
		SLATE_ATTRIBUTE(float, Value)
		SLATE_ATTRIBUTE(float, Max)
		SLATE_ARGUMENT(float, Segment)
		SLATE_ATTRIBUTE(FLinearColor, Color)
		SLATE_ARGUMENT(FVector2D, Size)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return Size; }

private:
	TAttribute<float> Value;
	TAttribute<float> Max;
	float Segment = 25.f;
	TAttribute<FLinearColor> Color;
	FVector2D Size = FVector2D(200.f, 12.f);
};

struct FSlateBrush;
struct FTaGHeroDef;
enum class ETaGArt : uint8;
enum class ETaGRole : uint8;

namespace TaGUI
{
	FSlateFontInfo Font(float Size, bool bBold = true);
	/** Portrait rendered by tools/blender (/Game/Art/UI/T_Portrait_<Key>); nullptr until the art exists. */
	const FSlateBrush* PortraitBrush(const FTaGHeroDef& Hero);
	/** HUD icon /Game/Art/UI/Icons/T_Icon_<Name> (white, tinted by the widget that shows it); nullptr if missing. */
	const FSlateBrush* IconBrush(const FString& Name);
	/** An art's icon: its own for the twelve hand-written arts, its kind's for the data-driven ones. */
	FString ArtIconName(ETaGArt Art);
	FLinearColor RoleColor(ETaGRole Role);
	const TCHAR* RoleIconName(ETaGRole Role);
	TSharedRef<SWidget> MakeMatchHUD(TWeakObjectPtr<ATaGPlayerController> PC);
	TSharedRef<SWidget> MakeTitle(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
	TSharedRef<SWidget> MakeLobby(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
	TSharedRef<SWidget> MakeResults(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
	TSharedRef<SWidget> MakePause(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
	TSharedRef<SWidget> MakeSettings(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
	TSharedRef<SWidget> MakeTutorialDone(TWeakObjectPtr<ATaGPlayerController> PC, TSharedPtr<SWidget>& OutFocus);
}
