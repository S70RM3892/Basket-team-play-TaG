#include "UI/TaGMenus.h"
#include "Ball/TaGBall.h"
#include "Character/TaGCharacter.h"
#include "Character/TaGPlayerController.h"
#include "Core/TaGGameState.h"
#include "Core/TaGHeroData.h"
#include "Core/TaGPlayerState.h"
#include "Core/TaGRules.h"
#include "Core/TaGTutorial.h"
#include "Core/TaGTypes.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "IPAddress.h"
#include "Rendering/DrawElements.h"
#include "SocketSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Engine/Texture2D.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "TaGUI"

namespace
{
	using FPCPtr = TWeakObjectPtr<ATaGPlayerController>;

	const FSlateBrush* White() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
	FText T(const FString& S) { return FText::FromString(S); }

	ATaGGameState* GetGS(const FPCPtr& PC)
	{
		return PC.IsValid() ? PC->GetWorld()->GetGameState<ATaGGameState>() : nullptr;
	}
	ATaGCharacter* GetMe(const FPCPtr& PC)
	{
		return PC.IsValid() ? Cast<ATaGCharacter>(PC->GetPawn()) : nullptr;
	}
	ATaGPlayerState* GetMyPS(const FPCPtr& PC)
	{
		return PC.IsValid() ? PC->GetPlayerState<ATaGPlayerState>() : nullptr;
	}

	const FLinearColor Panel(0.02f, 0.03f, 0.05f, 0.82f);
	const FLinearColor Teal = TaG::AllyColor();
	const FLinearColor Orange = TaG::EnemyColor();

	TSharedRef<SWidget> Button(const FText& Label, TFunction<void()> OnClick, bool bPrimary = false, TSharedPtr<SWidget>* OutFocus = nullptr,
		TFunction<bool()> IsSelected = nullptr, float Size = 20.f)
	{
		TSharedRef<SButton> B = SNew(SButton)
			.ButtonColorAndOpacity_Lambda([bPrimary, IsSelected]()
			{
				if (IsSelected && IsSelected())
				{
					return FSlateColor(FLinearColor(0.1f, 0.85f, 0.75f, 1.f));
				}
				return FSlateColor(bPrimary ? FLinearColor(1.f, 0.55f, 0.15f, 1.f) : FLinearColor(0.22f, 0.25f, 0.3f, 1.f));
			})
			.ContentPadding(FMargin(18.f, 8.f))
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([OnClick]() { if (OnClick) { OnClick(); } return FReply::Handled(); })
			[
				SNew(STextBlock).Font(TaGUI::Font(Size)).Text(Label).ColorAndOpacity(FLinearColor::White)
			];
		if (OutFocus && !OutFocus->IsValid())
		{
			*OutFocus = B;
		}
		return B;
	}

	FString LocalIPs()
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Sockets)
		{
			return TEXT("?");
		}
		bool bCanBindAll = false;
		TSharedRef<FInternetAddr> Addr = Sockets->GetLocalHostAddr(*GLog, bCanBindAll);
		return Addr->IsValid() ? Addr->ToString(false) : TEXT("?");
	}

	FString CooldownText(const ATaGCharacter* Me, ETaGSlot Slot)
	{
		const float Left = Me->GetCooldownLeft(Slot);
		return Left > 0.05f ? FString::Printf(TEXT("%.1f"), Left) : TEXT("OK");
	}

	/** A button whose label follows a value (settings toggles). */
	TSharedRef<SWidget> ValueButton(TFunction<FText()> Label, TFunction<void()> OnClick, float Width = 400.f, TSharedPtr<SWidget>* OutFocus = nullptr)
	{
		TSharedRef<SButton> B = SNew(SButton)
			.ButtonColorAndOpacity(FLinearColor(0.22f, 0.25f, 0.3f, 1.f))
			.ContentPadding(FMargin(14.f, 8.f))
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([OnClick]() { if (OnClick) { OnClick(); } return FReply::Handled(); })
			[
				SNew(SBox).WidthOverride(Width).HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(TaGUI::Font(18.f)).ColorAndOpacity(FLinearColor::White).Text_Lambda([Label]() { return Label ? Label() : FText::GetEmpty(); })
				]
			];
		if (OutFocus && !OutFocus->IsValid())
		{
			*OutFocus = B;
		}
		return B;
	}

	/** One settings line: name, then either [−] value [＋] or a single toggle button. */
	TSharedRef<SWidget> SettingRow(const TCHAR* Name, TSharedRef<SWidget> Control)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(250.f)
				[
					SNew(STextBlock).Font(TaGUI::Font(20.f)).Text(T(Name))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Control
			];
	}

	TSharedRef<SWidget> Stepper(TFunction<FText()> Value, TFunction<void()> OnMinus, TFunction<void()> OnPlus, TSharedPtr<SWidget>* OutFocus = nullptr)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Button(T(TEXT("−")), OnMinus, false, OutFocus, nullptr, 19.f)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(296.f).HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(TaGUI::Font(20.f)).Text_Lambda([Value]() { return Value ? Value() : FText::GetEmpty(); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Button(T(TEXT("＋")), OnPlus, false, nullptr, nullptr, 19.f)
			];
	}
}

FSlateFontInfo TaGUI::Font(float Size, bool bBold)
{
	FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	F.OutlineSettings.OutlineSize = Size >= 18.f ? 2 : 1;
	F.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
	return F;
}

// ================================================================ Root

void STaGRoot::Construct(const FArguments& InArgs)
{
	PC = InArgs._PC;
	ChildSlot[InArgs._Content.Widget];
}

FReply STaGRoot::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if ((Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right) && PC.IsValid())
	{
		PC->TogglePauseMenu();
		return FReply::Handled();
	}
	// B backs out of the settings and the pause menu (only reaches here while a menu has focus).
	if (Key == EKeys::Gamepad_FaceButton_Right && PC.IsValid() && (PC->IsSettingsOpen() || PC->IsPauseMenuOpen()))
	{
		PC->TogglePauseMenu();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// ================================================================ World markers

void STaGWorldMarkers::Construct(const FArguments& InArgs)
{
	PC = InArgs._PC;
}

int32 STaGWorldMarkers::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!PC.IsValid())
	{
		return LayerId;
	}
	const ATaGGameState* GS = PC->GetWorld()->GetGameState<ATaGGameState>();
	if (!GS || GS->Phase != ETaGPhase::Playing)
	{
		return LayerId;
	}
	const ATaGCharacter* Me = Cast<ATaGCharacter>(PC->GetPawn());
	const ATaGCharacter* PassTarget = (Me && Me->HasBall()) ? Me->FindBestPassTarget(Me->GetAimDirection()) : nullptr;
	const float Scale = FMath::Max(AllottedGeometry.Scale, 0.01f);
	const FSlateFontInfo NameFont = TaGUI::Font(13.f);
	const FSlateFontInfo SmallFont = TaGUI::Font(11.f);

	for (TActorIterator<ATaGCharacter> It(PC->GetWorld()); It; ++It)
	{
		const ATaGCharacter* C = *It;
		if (C == Me)
		{
			continue;
		}
		FVector2D Screen;
		const FVector Head = C->GetEyeLocation() + FVector(0.f, 0.f, 45.f);
		if (!PC->ProjectWorldLocationToScreen(Head, Screen, true))
		{
			continue;
		}
		const FVector2f Local(Screen.X / Scale, Screen.Y / Scale);
		const FLinearColor Color = GS->TeamColorFor(C->GetTeam());
		FString Label = C->GetHeroDef().Name;
		if (const ATaGPlayerState* P = C->GetTaGPlayerState())
		{
			if (!P->bIsBotPlayer)
			{
				Label = FString::Printf(TEXT("%s・%s"), *P->GetPlayerName(), C->GetHeroDef().Name);
			}
		}
		if (C->HasBall()) { Label = TEXT("● ") + Label; }
		const float Dist = Me ? FVector::Dist(Me->GetActorLocation(), C->GetActorLocation()) : 1000.f;
		const float Alpha = FMath::Clamp(1.4f - Dist / 2500.f, 0.45f, 1.f);
		const FVector2f Size(220.f, 20.f);
		FSlateDrawElement::MakeText(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Local - FVector2f(Label.Len() * 6.5f, 18.f))),
			Label, NameFont, ESlateDrawEffect::None, Color.CopyWithNewOpacity(Alpha));
		FString Sub;
		if (C->IsDown()) { Sub = TEXT("ダウン"); }
		else if (C == PassTarget) { Sub = TEXT("▼ パス先"); }
		else if (C->IsInZone()) { Sub = TEXT("ゾーン"); }
		if (!Sub.IsEmpty())
		{
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
				AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Local - FVector2f(Sub.Len() * 5.5f, 36.f))),
				Sub, SmallFont, ESlateDrawEffect::None, C == PassTarget ? TaG::GoldColor() : FLinearColor::White);
		}
	}
	return LayerId + 2;
}

// ================================================================ Match HUD

TSharedRef<SWidget> TaGUI::MakeMatchHUD(FPCPtr PC)
{
	auto Playing = [PC]()
	{
		const ATaGGameState* S = GetGS(PC);
		return S && (S->Phase == ETaGPhase::Playing || S->Phase == ETaGPhase::Results);
	};

	// Popups
	TSharedRef<SVerticalBox> PopupBox = SNew(SVerticalBox);
	for (int32 i = 0; i < 4; ++i)
	{
		PopupBox->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f)
		[
			SNew(STextBlock).Font(Font(i == 0 ? 28.f : 20.f))
			.Text_Lambda([PC, i]()
			{
				const ATaGGameState* S = GetGS(PC);
				return S && S->GetPopups().IsValidIndex(i) ? S->GetPopups()[i].Text : FText::GetEmpty();
			})
			.ColorAndOpacity_Lambda([PC, i]()
			{
				const ATaGGameState* S = GetGS(PC);
				if (S && S->GetPopups().IsValidIndex(i))
				{
					FLinearColor C = S->TeamColorFor(S->GetPopups()[i].Team);
					C.A = FMath::Clamp(S->GetPopups()[i].TimeLeft / 0.4f, 0.f, 1.f);
					return FSlateColor(C);
				}
				return FSlateColor(FLinearColor::Transparent);
			})
		];
	}

	// Hands and arts with cooldowns (bottom right)
	struct FRow { const TCHAR* Key; int32 Kind; };
	const FRow Rows[] = { { TEXT("LB / Q"), 0 }, { TEXT("RB / E"), 1 }, { TEXT("B / Shift"), 2 }, { TEXT("X / F"), 3 }, { TEXT("LT / C"), 4 }, { TEXT("Y / V"), 5 } };
	TSharedRef<SVerticalBox> HandBox = SNew(SVerticalBox);
	for (const FRow& R : Rows)
	{
		const int32 Kind = R.Kind;
		HandBox->AddSlot().AutoHeight().Padding(0.f, 1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(92.f)
				[
					SNew(STextBlock).Font(Font(12.f, false)).ColorAndOpacity(FLinearColor(0.75f, 0.78f, 0.82f)).Text(T(R.Key))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(270.f)
				[
					SNew(STextBlock).Font(Font(14.f))
					.Text_Lambda([PC, Kind]()
					{
						const ATaGCharacter* Me = GetMe(PC);
						if (!Me) { return FText::GetEmpty(); }
						const int32 Role = Me->GetRole();
						const FTaGHeroDef& H = Me->GetHeroDef();
						switch (Kind)
						{
						case 0: return T(FString::Printf(TEXT("%s  %s"), Role == 2 ? TEXT("タックル") : TEXT("突破"), *CooldownText(Me, Role == 2 ? ETaGSlot::Tackle : ETaGSlot::Drive)));
						case 1: return T(FString::Printf(TEXT("%s  %s"), Role == 2 ? TEXT("シュート妨害") : TEXT("かわし"), *CooldownText(Me, Role == 2 ? ETaGSlot::Contest : ETaGSlot::Dodge)));
						case 2: return T(FString::Printf(TEXT("間合い（ステップ）  %s"), *CooldownText(Me, ETaGSlot::Step)));
						case 3: return T(Role == 1 ? TEXT("パス") : FString::Printf(TEXT("スティール  %s"), *CooldownText(Me, ETaGSlot::Steal)));
						case 4: return T(FString::Printf(TEXT("%s〔%s〕 %s"), TaGHeroes::GetArt(H.Art1).Name, TaGHeroes::HandName(TaGHeroes::GetArt(H.Art1).Hand), *CooldownText(Me, ETaGSlot::Art1)));
						default: return T(FString::Printf(TEXT("%s〔%s〕 %s"), TaGHeroes::GetArt(H.Art2).Name, TaGHeroes::HandName(TaGHeroes::GetArt(H.Art2).Hand), *CooldownText(Me, ETaGSlot::Art2)));
						}
					})
					.ColorAndOpacity_Lambda([PC, Kind]()
					{
						const ATaGCharacter* Me = GetMe(PC);
						if (!Me) { return FSlateColor(FLinearColor::White); }
						const int32 Role = Me->GetRole();
						ETaGSlot Slot = ETaGSlot::Step;
						switch (Kind)
						{
						case 0: Slot = Role == 2 ? ETaGSlot::Tackle : ETaGSlot::Drive; break;
						case 1: Slot = Role == 2 ? ETaGSlot::Contest : ETaGSlot::Dodge; break;
						case 3: Slot = Role == 1 ? ETaGSlot::Pass : ETaGSlot::Steal; break;
						case 4: Slot = ETaGSlot::Art1; break;
						case 5: Slot = ETaGSlot::Art2; break;
						default: break;
						}
						const bool bReady = Me->GetCooldownLeft(Slot) <= 0.05f;
						return FSlateColor(bReady ? (Kind >= 4 ? FLinearColor(0.85f, 0.65f, 1.f) : FLinearColor::White) : FLinearColor(0.45f, 0.45f, 0.5f));
					})
				]
			]
		];
	}

	auto InTutorial = [PC]()
	{
		const ATaGGameState* S = GetGS(PC);
		return S && S->bTutorial && S->Phase == ETaGPhase::Playing;
	};

	return SNew(SOverlay)
		.Visibility_Lambda([Playing]() { return Playing() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		// Player markers
		+ SOverlay::Slot()
		[
			SNew(STaGWorldMarkers).PC(PC)
		]
		// Flash
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(White())
			.ColorAndOpacity_Lambda([PC]()
			{
				const ATaGGameState* S = GetGS(PC);
				if (!S || S->GetFlash() <= 0.f) { return FSlateColor(FLinearColor::Transparent); }
				FLinearColor C = S->GetFlashColor();
				C.A *= S->GetFlash() / 0.25f;
				return FSlateColor(C);
			})
		]
		// Tutorial lesson (instead of the score)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 14.f)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(24.f, 10.f))
			.Visibility_Lambda([PC, InTutorial]()
			{
				const ATaGGameState* S = GetGS(PC);
				return InTutorial() && S && !S->IsTutorialFinished() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[
				SNew(SBox).WidthOverride(860.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(Font(24.f)).ColorAndOpacity(Teal)
							.Text_Lambda([PC]()
							{
								const ATaGGameState* S = GetGS(PC);
								if (!S) { return FText::GetEmpty(); }
								return T(FString::Printf(TEXT("レッスン %d / %d   %s"), S->TutorialStep + 1, TaGTutorial::NumLessons(), TaGTutorial::Get(S->TutorialStep).Title));
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(Font(22.f)).ColorAndOpacity(TaG::GoldColor())
							.Text_Lambda([PC]()
							{
								const ATaGGameState* S = GetGS(PC);
								if (!S) { return FText::GetEmpty(); }
								return T(FString::Printf(TEXT("%d / %d %s"), S->TutorialProgress, S->TutorialGoal, TaGTutorial::Get(S->TutorialStep).Unit));
							})
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(STextBlock).Font(Font(17.f)).AutoWrapText(true)
						.Text_Lambda([PC]()
						{
							const ATaGGameState* S = GetGS(PC);
							return S ? T(TaGTutorial::Get(S->TutorialStep).Text) : FText::GetEmpty();
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(STextBlock).Font(Font(13.f, false)).ColorAndOpacity(FLinearColor(0.75f, 0.78f, 0.82f))
						.Text(T(TEXT("ポーズ（Start / Esc）から、このレッスンをとばせる")))
					]
				]
			]
		]
		// Score and clock
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 14.f)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(22.f, 6.f))
			.Visibility_Lambda([InTutorial]() { return InTutorial() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
					[
						SNew(STextBlock).Font(Font(30.f)).ColorAndOpacity(Teal)
						.Text_Lambda([PC]()
						{
							const ATaGGameState* S = GetGS(PC);
							return S ? T(FString::Printf(TEXT("味方  %d"), S->GetScore(S->GetLocalTeam()))) : FText::GetEmpty();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
					[
						SNew(STextBlock).Font(Font(30.f)).Text(T(TEXT("-")))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
					[
						SNew(STextBlock).Font(Font(30.f)).ColorAndOpacity(Orange)
						.Text_Lambda([PC]()
						{
							const ATaGGameState* S = GetGS(PC);
							return S ? T(FString::Printf(TEXT("%d  相手"), S->GetScore(1 - S->GetLocalTeam()))) : FText::GetEmpty();
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(Font(17.f))
					.Text_Lambda([PC]()
					{
						const ATaGGameState* S = GetGS(PC);
						if (!S) { return FText::GetEmpty(); }
						const bool bOff = S->PossessionTeam == S->GetLocalTeam();
						return T(FString::Printf(TEXT("%s   ショットクロック %.1f   %don%d・11点先取"), bOff ? TEXT("攻撃") : TEXT("守備"), FMath::Max(0.f, S->ShotClock), S->TeamSize, S->TeamSize));
					})
					.ColorAndOpacity_Lambda([PC]()
					{
						const ATaGGameState* S = GetGS(PC);
						return FSlateColor(S && S->ShotClock < 4.f ? FLinearColor(1.f, 0.4f, 0.3f) : FLinearColor::White);
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(Font(17.f)).ColorAndOpacity(TaG::GoldColor())
					.Text_Lambda([PC]()
					{
						const ATaGGameState* S = GetGS(PC);
						if (S && S->NeedsClear(S->GetLocalTeam()) && S->PossessionTeam == S->GetLocalTeam())
						{
							return T(TEXT("アークの外へクリアしてから攻める"));
						}
						return FText::GetEmpty();
					})
				]
			]
		]
		// Read-win banner
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 260.f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return S && S->GetBanner().TimeLeft > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font(54.f))
				.Text_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return S ? S->GetBanner().Title : FText::GetEmpty(); })
				.ColorAndOpacity_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return FSlateColor(S && S->GetBanner().bGood ? FLinearColor(0.35f, 1.f, 0.95f) : FLinearColor(1.f, 0.45f, 0.4f)); })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font(24.f))
				.Text_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return S ? S->GetBanner().Rule : FText::GetEmpty(); })
			]
		]
		// Popups (below the lesson panel in the tutorial)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride_Lambda([InTutorial]() { return FOptionalSize(InTutorial() ? 200.f : 150.f); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				PopupBox
			]
		]
		// Crosshair
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(6.f).HeightOverride(6.f)
			[
				SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.8f))
			]
		]
		// Shot meter
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 120.f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([PC]() { const ATaGCharacter* Me = GetMe(PC); return Me && Me->IsChargingShot() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock).Font(Font(22.f))
				.Text_Lambda([PC]()
				{
					const ATaGCharacter* Me = GetMe(PC);
					if (!Me) { return FText::GetEmpty(); }
					const FTaGShotEval E = FTaGRules::EvaluateShot(Me, Me->GetShotCharge(), false);
					return T(FString::Printf(TEXT("%s・%d点  成功率 %d%%%s"), FTaGRules::ShotType(E.Type).Name, E.Points, FMath::RoundToInt(E.Chance * 100.f),
						E.bContested ? TEXT("  妨害されている") : (E.bOpen ? TEXT("  ノーマーク") : TEXT(""))));
				})
				.ColorAndOpacity_Lambda([PC]()
				{
					const ATaGCharacter* Me = GetMe(PC);
					const bool bContested = Me && FTaGRules::EvaluateShot(Me, Me->GetShotCharge(), false).bContested;
					return FSlateColor(bContested ? FLinearColor(1.f, 0.35f, 0.3f) : FLinearColor::White);
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(420.f).HeightOverride(16.f)
				[
					SNew(SCanvas)
					+ SCanvas::Slot().Position(FVector2D(0.f, 0.f)).Size(FVector2D(420.f, 16.f))
					[
						SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f))
					]
					+ SCanvas::Slot().Position(FVector2D(420.f * TaG::ShotGoodLo / TaG::ShotAutoRelease, 0.f)).Size(FVector2D(420.f * (TaG::ShotGoodHi - TaG::ShotGoodLo) / TaG::ShotAutoRelease, 16.f))
					[
						SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.15f, 0.75f, 0.3f, 0.85f))
					]
					+ SCanvas::Slot().Position(FVector2D(420.f * TaG::ShotPerfectLo / TaG::ShotAutoRelease, 0.f)).Size(FVector2D(420.f * (TaG::ShotPerfectHi - TaG::ShotPerfectLo) / TaG::ShotAutoRelease, 16.f))
					[
						SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(1.f, 0.9f, 0.3f, 0.95f))
					]
					+ SCanvas::Slot()
					.Position_Lambda([PC]()
					{
						const ATaGCharacter* Me = GetMe(PC);
						const float C = Me ? FMath::Clamp(Me->GetShotCharge() / TaG::ShotAutoRelease, 0.f, 1.f) : 0.f;
						return FVector2D(420.f * C - 2.f, -6.f);
					})
					.Size(FVector2D(4.f, 28.f))
					[
						SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor::White)
					]
				]
			]
		]
		// Status (bottom left): hero, guard, state
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(22.f)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(14.f, 8.f))
			.Visibility_Lambda([PC]() { return GetMe(PC) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(20.f)).ColorAndOpacity(Teal)
					.Text_Lambda([PC]()
					{
						const ATaGCharacter* Me = GetMe(PC);
						return Me ? T(FString::Printf(TEXT("%s  %s・%s"), Me->GetHeroDef().Name, TaGHeroes::RoleName(Me->GetHeroDef().Role), TaGHeroes::SizeName(Me->GetHeroDef().Size))) : FText::GetEmpty();
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SNew(SBox).WidthOverride(260.f).HeightOverride(10.f)
					[
						SNew(SCanvas)
						+ SCanvas::Slot().Position(FVector2D(0.f, 0.f)).Size(FVector2D(260.f, 10.f))
						[
							SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.12f, 0.9f))
						]
						+ SCanvas::Slot().Position(FVector2D(0.f, 0.f))
						.Size_Lambda([PC]()
						{
							const ATaGCharacter* Me = GetMe(PC);
							const float F = Me ? FMath::Clamp(Me->GetGuard() / FMath::Max(Me->GetMaxGuard(), 1.f), 0.f, 1.f) : 0.f;
							return FVector2D(260.f * F, 10.f);
						})
						[
							SNew(SImage).Image(White()).ColorAndOpacity(Teal)
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(16.f)).ColorAndOpacity(TaG::GoldColor())
					.Text_Lambda([PC]()
					{
						const ATaGCharacter* Me = GetMe(PC);
						if (!Me) { return FText::GetEmpty(); }
						switch (Me->GetAction())
						{
						case ETaGAction::Down: return T(TEXT("ダウン：味方が触れると復帰"));
						case ETaGAction::Gathered: return T(TEXT("ギャザー：パスかシュート"));
						case ETaGAction::Stagger: return T(TEXT("よろけ"));
						default: break;
						}
						if (Me->IsInZone()) { return T(TEXT("ゾーン！ 速くなり、次のシュートが入りやすい")); }
						if (Me->IsScoped()) { return T(TEXT("スコープ中")); }
						if (Me->IsCatchAndShoot()) { return T(TEXT("キャッチ＆シュート")); }
						return T(TEXT("ガード"));
					})
				]
			]
		]
		// Hands and arts (bottom right)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(22.f)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(12.f, 8.f))
			.Visibility_Lambda([PC]() { return GetMe(PC) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				HandBox
			]
		];
}

// ================================================================ Title

TSharedRef<SWidget> TaGUI::MakeTitle(FPCPtr PC, TSharedPtr<SWidget>& OutFocus)
{
	TSharedRef<SEditableTextBox> Address = SNew(SEditableTextBox)
		.Font(Font(18.f, false))
		.Text(T(PC.IsValid() ? PC->LastJoinAddress : TEXT("127.0.0.1")))
		.MinDesiredWidth(220.f);
	// First launch: the tutorial is the highlighted choice; afterwards, straight into a 3on3.
	const bool bLearned = PC.IsValid() && PC->bTutorialDone;

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(70.f, 60.f).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(36.f, 28.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(110.f)).ColorAndOpacity(Teal).Text(T(TEXT("TaG")))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(22.f)).Text(T(TEXT("Basket Team-play TaG")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 24.f)
				[
					SNew(STextBlock).Font(Font(26.f)).ColorAndOpacity(TaG::GoldColor()).Text(T(TEXT("読み勝って、ぶち抜け。")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f)
				[
					bLearned
					? Button(T(TEXT("すぐ遊ぶ（BOTと3on3）")), [PC]() { if (PC.IsValid()) { PC->QuickPlay(); } }, true, &OutFocus, nullptr, 24.f)
					: Button(T(TEXT("チュートリアル（はじめての方へ）")), [PC]() { if (PC.IsValid()) { PC->StartTutorial(); } }, true, &OutFocus, nullptr, 24.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f)
				[
					bLearned
					? Button(T(TEXT("チュートリアル")), [PC]() { if (PC.IsValid()) { PC->StartTutorial(); } }, false, nullptr, nullptr, 20.f)
					: Button(T(TEXT("すぐ遊ぶ（BOTと3on3）")), [PC]() { if (PC.IsValid()) { PC->QuickPlay(); } }, false, nullptr, nullptr, 20.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f)
				[
					Button(T(TEXT("ヒーローを選んでBOTと対戦")), [PC]() { if (PC.IsValid()) { PC->PlayOffline(); } }, false, nullptr, nullptr, 20.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f)
				[
					Button(T(TEXT("友達と遊ぶ：ホストになる")), [PC]() { if (PC.IsValid()) { PC->HostOnline(); } }, false, nullptr, nullptr, 20.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
					[
						Address
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						Button(T(TEXT("参加")), [PC, Address]() { if (PC.IsValid()) { PC->JoinOnline(Address->GetText().ToString().TrimStartAndEnd()); } })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
					[
						Button(T(TEXT("設定")), [PC]() { if (PC.IsValid()) { PC->SetSettingsOpen(true); } })
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						Button(T(TEXT("終了")), [PC]() { if (PC.IsValid()) { PC->QuitGame(); } })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 22.f, 0.f, 0.f)
				[
					SNew(STextBlock).Font(Font(13.f, false)).ColorAndOpacity(FLinearColor(0.8f, 0.82f, 0.86f))
					.Text(T(TEXT("海に沈みかけた積層都市ナギサ。バスケで電気をつくり、勝った地区に明かりが灯る。\n普通のバスケに、アビリティのじゃんけん：突破・シュート・かわし 対 タックル・シュート妨害・間合い。")))
				]
			]
		];
}

// ================================================================ Lobby / hero select

namespace
{
	/** Portrait rendered by tools/blender (T_Portrait_<Key>); nullptr until the art exists. */
	const FSlateBrush* PortraitBrush(const FTaGHeroDef& H)
	{
		static TMap<FString, TSharedPtr<FSlateBrush>> Cache;
		if (const TSharedPtr<FSlateBrush>* Found = Cache.Find(H.Key))
		{
			return Found->Get();
		}
		TSharedPtr<FSlateBrush> Brush;
		const FString Path = FString::Printf(TEXT("/Game/Art/UI/T_Portrait_%s.T_Portrait_%s"), H.Key, H.Key);
		if (UTexture2D* Tex = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)))
		{
			Tex->AddToRoot(); // Slate only keeps a weak pointer
			Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Tex);
			Brush->ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
		}
		Cache.Add(H.Key, Brush);
		return Brush.Get();
	}

	FLinearColor RoleColor(ETaGRole Role)
	{
		switch (Role)
		{
		case ETaGRole::Wall: return FLinearColor(0.95f, 0.62f, 0.25f);
		case ETaGRole::Scorer: return FLinearColor(1.f, 0.45f, 0.45f);
		default: return FLinearColor(0.45f, 0.85f, 1.f);
		}
	}

	TSharedRef<SWidget> Portrait(const FTaGHeroDef& H, float Size)
	{
		if (const FSlateBrush* B = PortraitBrush(H))
		{
			return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[SNew(SImage).Image(B)];
		}
		// No art yet: the hero colours as a badge
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(White()).ColorAndOpacity(H.PrimaryColor)]
				+ SOverlay::Slot().VAlign(VAlign_Bottom)
				[
					SNew(SBox).HeightOverride(Size * 0.3f)[SNew(SImage).Image(White()).ColorAndOpacity(H.SecondaryColor)]
				]
			];
	}

	FText ArtLine(const FTaGArtDef& A, const TCHAR* Key)
	{
		return FText::FromString(FString::Printf(TEXT("%s  %s〔%s〕"), Key, A.Name, TaGHeroes::HandName(A.Hand)));
	}
}

TSharedRef<SWidget> TaGUI::MakeLobby(FPCPtr PC, TSharedPtr<SWidget>& OutFocus)
{
	const bool bHost = PC.IsValid() && PC->IsHost();
	auto Selected = [PC]() -> const FTaGHeroDef&
	{
		const ATaGPlayerState* P = GetMyPS(PC);
		return TaGHeroes::Get(P ? P->Hero : ETaGHero::Rei);
	};

	// 30 hero cards: portrait, name, role and size (grouped by role, as the roster lists them)
	TArray<ETaGHero> Order;
	for (const ETaGRole Role : { ETaGRole::Wall, ETaGRole::Scorer, ETaGRole::Playmaker })
	{
		for (int32 i = 0; i < TaGHeroes::Num; ++i)
		{
			if (TaGHeroes::Get(static_cast<ETaGHero>(i)).Role == Role)
			{
				Order.Add(static_cast<ETaGHero>(i));
			}
		}
	}
	constexpr int32 Cols = 6;
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(3.f));
	for (int32 n = 0; n < Order.Num(); ++n)
	{
		const ETaGHero Id = Order[n];
		const FTaGHeroDef& H = TaGHeroes::Get(Id);
		TSharedRef<SButton> Card = SNew(SButton)
			.ButtonColorAndOpacity_Lambda([PC, Id]()
			{
				const ATaGPlayerState* P = GetMyPS(PC);
				const bool bSel = P && P->Hero == Id;
				return FSlateColor(bSel ? FLinearColor(0.1f, 0.85f, 0.75f, 1.f) : FLinearColor(0.14f, 0.16f, 0.2f, 1.f));
			})
			.ContentPadding(FMargin(5.f, 4.f))
			.OnClicked_Lambda([PC, Id]() { if (PC.IsValid()) { PC->ServerSetHero(Id); } return FReply::Handled(); })
			[
				SNew(SBox).WidthOverride(158.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						Portrait(H, 46.f)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font(16.f)).Text(T(H.Name))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font(10.f, false)).ColorAndOpacity(RoleColor(H.Role))
							.Text(T(FString::Printf(TEXT("%s・%s"), H.Role == ETaGRole::Wall ? TEXT("壁役") : (H.Role == ETaGRole::Scorer ? TEXT("点取り") : TEXT("司令塔")), TaGHeroes::SizeName(H.Size))))
						]
					]
				]
			];
		if (!OutFocus.IsValid())
		{
			OutFocus = Card;
		}
		Grid->AddSlot(n % Cols, n / Cols)[Card];
	}

	auto Roster = [PC](int32 Team)
	{
		const ATaGGameState* S = GetGS(PC);
		if (!S) { return FText::GetEmpty(); }
		FString Out;
		int32 Humans = 0;
		for (const APlayerState* P : S->PlayerArray)
		{
			const ATaGPlayerState* T2 = Cast<ATaGPlayerState>(P);
			if (T2 && !T2->bIsBotPlayer && T2->Team == Team)
			{
				Out += FString::Printf(TEXT("%s ・ %s\n"), *T2->GetPlayerName(), TaGHeroes::Get(T2->Hero).Name);
				Humans++;
			}
		}
		for (int32 i = Humans; i < S->TeamSize; ++i)
		{
			Out += TEXT("BOT（ヒーローはランダム）\n");
		}
		return T(Out);
	};

	auto ModeButton = [PC](int32 Size)
	{
		return Button(T(FString::Printf(TEXT("%don%d"), Size, Size)),
			[PC, Size]() { if (PC.IsValid()) { const ATaGGameState* S = GetGS(PC); PC->ServerSetLobbyOptions(Size, S ? S->BotSkill : 1); } },
			false, nullptr, [PC, Size]() { const ATaGGameState* S = GetGS(PC); return S && S->TeamSize == Size; }, 17.f);
	};
	auto BotButton = [PC](int32 Level, const TCHAR* Label)
	{
		return Button(T(Label),
			[PC, Level]() { if (PC.IsValid()) { const ATaGGameState* S = GetGS(PC); PC->ServerSetLobbyOptions(S ? S->TeamSize : 3, Level); } },
			false, nullptr, [PC, Level]() { const ATaGGameState* S = GetGS(PC); return S && S->BotSkill == Level; }, 15.f);
	};

	// The chosen hero in detail
	TSharedRef<SWidget> Detail = SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.45f)).Padding(FMargin(14.f, 10.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(84.f).HeightOverride(84.f)
					[
						SNew(SImage)
						.Image_Lambda([Selected]() { const FSlateBrush* B = PortraitBrush(Selected()); return B ? B : White(); })
						.ColorAndOpacity_Lambda([Selected]() { return FSlateColor(PortraitBrush(Selected()) ? FLinearColor::White : Selected().PrimaryColor); })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(30.f)).Text_Lambda([Selected]() { return T(Selected().Name); })
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(13.f)).AutoWrapText(true)
						.ColorAndOpacity_Lambda([Selected]() { return FSlateColor(RoleColor(Selected().Role)); })
						.Text_Lambda([Selected]()
						{
							const FTaGHeroDef& H = Selected();
							return T(FString::Printf(TEXT("%s・体格%s・%s"), TaGHeroes::RoleName(H.Role), TaGHeroes::SizeName(H.Size), H.Kind));
						})
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
			[
				SNew(STextBlock).Font(Font(13.f, false)).AutoWrapText(true).Text_Lambda([Selected]() { return T(Selected().Blurb); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(15.f)).ColorAndOpacity(FLinearColor(0.85f, 0.65f, 1.f)).AutoWrapText(true)
				.Text_Lambda([Selected]() { return ArtLine(TaGHeroes::GetArt(Selected().Art1), TEXT("LT/C")); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(12.f, false)).AutoWrapText(true).Text_Lambda([Selected]() { return T(TaGHeroes::GetArt(Selected().Art1).Desc); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock).Font(Font(15.f)).ColorAndOpacity(FLinearColor(0.85f, 0.65f, 1.f)).AutoWrapText(true)
				.Text_Lambda([Selected]() { return ArtLine(TaGHeroes::GetArt(Selected().Art2), TEXT("Y/V")); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(12.f, false)).AutoWrapText(true).Text_Lambda([Selected]() { return T(TaGHeroes::GetArt(Selected().Art2).Desc); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(STextBlock).Font(Font(12.f, false)).ColorAndOpacity(FLinearColor(0.75f, 0.78f, 0.82f))
				.Text_Lambda([Selected]()
				{
					const FTaGHeroDef& H = Selected();
					return T(FString::Printf(TEXT("ガード %d　速さ %d%%　ジャンプ %d%%　リーチ %d%%　重さ %.1f　視点 %.2fm"),
						FMath::RoundToInt(H.Guard), FMath::RoundToInt(H.SpeedMul * 100.f), FMath::RoundToInt(H.JumpMul * 100.f),
						FMath::RoundToInt(H.ReachMul * 100.f), H.Weight, H.EyeHeight / 100.f));
				})
			]
		];

	return SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f)).Padding(FMargin(28.f, 18.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Font(34.f)).ColorAndOpacity(Teal).Text(T(TEXT("ヒーロー選択（30体）")))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Font(15.f)).ColorAndOpacity(TaG::GoldColor())
					.Text_Lambda([PC]()
					{
						const ATaGGameState* S = GetGS(PC);
						if (S && S->bOnline && PC.IsValid() && PC->IsHost())
						{
							return T(FString::Printf(TEXT("友達に伝えるアドレス：%s（タイトルの「参加」に入力）"), *LocalIPs()));
						}
						return S && S->bOnline ? T(TEXT("オンライン・ロビー")) : T(TEXT("オフライン（BOTと対戦）"));
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
			[
				SNew(SHorizontalBox)
				.Visibility(bHost ? EVisibility::Visible : EVisibility::Collapsed)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(STextBlock).Font(Font(17.f)).Text(T(TEXT("モード")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.f) [ ModeButton(1) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.f) [ ModeButton(2) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.f) [ ModeButton(3) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(30.f, 0.f, 10.f, 0.f)
				[
					SNew(STextBlock).Font(Font(17.f)).Text(T(TEXT("BOTの強さ")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.f) [ BotButton(0, TEXT("練習")) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.f) [ BotButton(1, TEXT("ふつう")) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.f) [ BotButton(2, TEXT("強い")) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.f) [ BotButton(3, TEXT("鬼")) ]
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(3.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock).Font(Font(12.f, false)).ColorAndOpacity(FLinearColor(0.75f, 0.78f, 0.82f))
						.Text(T(TEXT("ウォール（壁役）10体 → スコアラー（点取り）10体 → プレイメーカー（司令塔）10体の順")))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						Grid
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(18.f, 0.f, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						Detail
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Font(18.f)).ColorAndOpacity(Teal).Text(T(TEXT("チームA")))
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Font(13.f, false)).Text_Lambda([Roster]() { return Roster(0); })
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								Button(T(TEXT("Aに入る")), [PC]() { if (PC.IsValid()) { PC->ServerSetTeam(0); } }, false, nullptr, [PC]() { const ATaGPlayerState* P = GetMyPS(PC); return P && P->Team == 0; }, 13.f)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Font(18.f)).ColorAndOpacity(Orange).Text(T(TEXT("チームB")))
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Font(Font(13.f, false)).Text_Lambda([Roster]() { return Roster(1); })
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								Button(T(TEXT("Bに入る")), [PC]() { if (PC.IsValid()) { PC->ServerSetTeam(1); } }, false, nullptr, [PC]() { const ATaGPlayerState* P = GetMyPS(PC); return P && P->Team == 1; }, 13.f)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							bHost
							? Button(T(TEXT("試合開始")), [PC]() { if (PC.IsValid()) { PC->ServerStartMatch(); } }, true, nullptr, nullptr, 24.f)
							: StaticCastSharedRef<SWidget>(SNew(STextBlock).Font(Font(16.f)).Text(T(TEXT("ホストの開始を待っています…"))))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(10.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
						[
							Button(T(TEXT("タイトルへ")), [PC]() { if (PC.IsValid()) { PC->LeaveToTitle(); } }, false, nullptr, nullptr, 14.f)
						]
					]
				]
			]
		];
}

// ================================================================ Results

TSharedRef<SWidget> TaGUI::MakeResults(FPCPtr PC, TSharedPtr<SWidget>& OutFocus)
{
	const bool bHost = PC.IsValid() && PC->IsHost();
	auto TeamLine = [PC](bool bMine)
	{
		const ATaGGameState* S = GetGS(PC);
		if (!S) { return FText::GetEmpty(); }
		const int32 Team = bMine ? S->GetLocalTeam() : 1 - S->GetLocalTeam();
		const FTaGTeamStats& St = S->Stats(Team);
		const float PPP = St.Possessions ? float(St.Points) / St.Possessions : 0.f;
		const float Iso = St.IsoPossessions ? float(St.IsoPoints) / St.IsoPossessions : 0.f;
		const float Tm = St.TeamPossessions ? float(St.TeamPoints) / St.TeamPossessions : 0.f;
		return T(FString::Printf(TEXT("%s  %d点\n攻撃1回あたり %.2f（1対1 %.2f / 連携 %.2f）\nシュート %d/%d  読み勝ち %d  スティール %d  ブロック %d  パス %d"),
			bMine ? TEXT("味方") : TEXT("相手"), S->GetScore(Team), PPP, Iso, Tm, St.Makes, St.Shots, St.ReadWins, St.Steals, St.Blocks, St.Passes));
	};
	auto Players = [PC]()
	{
		const ATaGGameState* S = GetGS(PC);
		if (!S) { return FText::GetEmpty(); }
		FString Out;
		for (const APlayerState* P : S->PlayerArray)
		{
			const ATaGPlayerState* T2 = Cast<ATaGPlayerState>(P);
			if (!T2 || T2->Team < 0) { continue; }
			Out += FString::Printf(TEXT("%s  %s・%s   %d点 %dアシスト %dスティール %dブロック  読み %d勝%d敗\n"),
				T2->Team == S->GetLocalTeam() ? TEXT("味方") : TEXT("相手"), *T2->GetPlayerName(), TaGHeroes::Get(T2->Hero).Name,
				T2->Stats.Points, T2->Stats.Assists, T2->Stats.Steals, T2->Stats.Blocks, T2->Stats.ReadWins, T2->Stats.ReadLosses);
		}
		return T(Out);
	};

	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(40.f, 26.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(Font(56.f))
					.Text_Lambda([PC]()
					{
						const ATaGGameState* S = GetGS(PC);
						return S && S->WinnerTeam == S->GetLocalTeam() ? T(TEXT("勝利！ 地区に明かりが灯った")) : T(TEXT("敗北…"));
					})
					.ColorAndOpacity_Lambda([PC]()
					{
						const ATaGGameState* S = GetGS(PC);
						return FSlateColor(S && S->WinnerTeam == S->GetLocalTeam() ? Teal : Orange);
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 40.f, 0.f)
					[
						SNew(STextBlock).Font(Font(17.f)).ColorAndOpacity(Teal).Text_Lambda([TeamLine]() { return TeamLine(true); })
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock).Font(Font(17.f)).ColorAndOpacity(Orange).Text_Lambda([TeamLine]() { return TeamLine(false); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
				[
					SNew(STextBlock).Font(Font(14.f, false)).Text_Lambda([Players]() { return Players(); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(6.f)
					[
						bHost ? Button(T(TEXT("もう1回")), [PC]() { if (PC.IsValid()) { PC->ServerRematch(); } }, true, &OutFocus, nullptr, 24.f)
						: StaticCastSharedRef<SWidget>(SNew(STextBlock).Font(Font(18.f)).Text(T(TEXT("ホストの選択を待っています…"))))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(6.f)
					[
						bHost ? Button(T(TEXT("ヒーロー選択へ")), [PC]() { if (PC.IsValid()) { PC->ServerBackToLobby(); } }, false, nullptr, nullptr, 20.f)
						: StaticCastSharedRef<SWidget>(SNew(SBox))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(6.f)
					[
						Button(T(TEXT("タイトルへ")), [PC]() { if (PC.IsValid()) { PC->LeaveToTitle(); } }, false, &OutFocus, nullptr, 20.f)
					]
				]
			]
		];
}

// ================================================================ Pause

TSharedRef<SWidget> TaGUI::MakePause(FPCPtr PC, TSharedPtr<SWidget>& OutFocus)
{
	const ATaGGameState* GS0 = GetGS(PC);
	const bool bTutorial = GS0 && GS0->bTutorial;
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(40.f, 26.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 12.f)
				[
					SNew(STextBlock).Font(Font(40.f)).Text(T(TEXT("ポーズ")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					Button(T(TEXT("再開")), [PC]() { if (PC.IsValid()) { PC->TogglePauseMenu(); } }, true, &OutFocus, nullptr, 22.f)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					bTutorial
					? Button(T(TEXT("このレッスンをとばす")), [PC]()
						{
							if (PC.IsValid())
							{
								PC->ServerTutorialSkip();
								PC->TogglePauseMenu();
							}
						})
					: StaticCastSharedRef<SWidget>(SNew(SBox))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					Button(T(TEXT("カメラ：一人称 / 三人称（View / T）")), [PC]()
					{
						if (ATaGCharacter* Me = GetMe(PC)) { Me->SetThirdPerson(!Me->IsThirdPerson()); }
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					Button(T(TEXT("設定（感度・上下反転・視野角・音量・振動）")), [PC]() { if (PC.IsValid()) { PC->SetSettingsOpen(true); } })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					Button(T(TEXT("タイトルへ")), [PC]() { if (PC.IsValid()) { PC->LeaveToTitle(); } })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f)
				[
					SNew(STextBlock).Font(Font(14.f, false))
					.Text(T(TEXT(
						"移動 左スティック/WASD   視点 右スティック/マウス   ジャンプ・ブロック A/Space\n"
						"シュート RT/左クリック（押して溜め、跳んだ頂点で離す。スティックの向きで種類が変わる）\n"
						"攻撃：突破 LB/Q   かわし RB/E   パス X/F・右クリック\n"
						"守備：タックル LB/Q   シュート妨害 RB/E（長押し）   間合い B/Shift   スティール X/F\n"
						"ヒーローの技 LT/C・Y/V   カメラ切替 View/T   ポーズ Start/Esc   メニューで戻る B")))
				]
			]
		];
}

// ================================================================ Settings

TSharedRef<SWidget> TaGUI::MakeSettings(FPCPtr PC, TSharedPtr<SWidget>& OutFocus)
{
	auto Apply = [PC](TFunction<void(ATaGPlayerController&)> Change)
	{
		if (PC.IsValid())
		{
			Change(*PC.Get());
			PC->ApplySettings();
		}
	};
	auto Percent = [](float V) { return T(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f))); };

	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(40.f, 26.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 14.f)
				[
					SNew(STextBlock).Font(Font(40.f)).Text(T(TEXT("設定")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SettingRow(TEXT("視点の感度"), Stepper(
						[PC]() { return T(FString::Printf(TEXT("%.1f"), PC.IsValid() ? PC->LookSensitivity : 1.f)); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.LookSensitivity -= 0.1f; }); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.LookSensitivity += 0.1f; }); }, &OutFocus))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SettingRow(TEXT("右スティックの上下"), ValueButton(
						[PC]() { return T(PC.IsValid() && PC->bInvertStickY ? TEXT("リバース（上に倒すと下を向く）") : TEXT("ノーマル（上に倒すと上を向く）")); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.bInvertStickY = !P.bInvertStickY; }); }))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SettingRow(TEXT("マウスの上下"), ValueButton(
						[PC]() { return T(PC.IsValid() && PC->bInvertMouseY ? TEXT("リバース（前に動かすと下を向く）") : TEXT("ノーマル（前に動かすと上を向く）")); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.bInvertMouseY = !P.bInvertMouseY; }); }))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SettingRow(TEXT("視野角"), Stepper(
						[PC]() { return T(FString::Printf(TEXT("%d°"), FMath::RoundToInt(PC.IsValid() ? PC->FieldOfView : 95.f))); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.FieldOfView -= 5.f; }); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.FieldOfView += 5.f; }); }))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SettingRow(TEXT("全体の音量"), Stepper(
						[PC, Percent]() { return Percent(PC.IsValid() ? PC->MasterVolume : 1.f); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.MasterVolume -= 0.1f; }); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.MasterVolume += 0.1f; }); }))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SettingRow(TEXT("BGMの音量"), Stepper(
						[PC, Percent]() { return Percent(PC.IsValid() ? PC->MusicVolume : 0.6f); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.MusicVolume -= 0.1f; }); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.MusicVolume += 0.1f; }); }))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SettingRow(TEXT("振動"), ValueButton(
						[PC]() { return T(PC.IsValid() && PC->bVibration ? TEXT("オン") : TEXT("オフ")); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.bVibration = !P.bVibration; }); }))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 16.f, 0.f, 0.f)
				[
					Button(T(TEXT("戻る（B / Esc）")), [PC]() { if (PC.IsValid()) { PC->SetSettingsOpen(false); } }, true, nullptr, nullptr, 22.f)
				]
			]
		];
}

// ================================================================ Tutorial finished

TSharedRef<SWidget> TaGUI::MakeTutorialDone(FPCPtr PC, TSharedPtr<SWidget>& OutFocus)
{
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(44.f, 30.f))
			[
				SNew(SBox).WidthOverride(820.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(Font(48.f)).ColorAndOpacity(Teal).Text(T(TEXT("チュートリアル完了！")))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f)
					[
						SNew(STextBlock).Font(Font(18.f)).AutoWrapText(true).Text(T(TaGTutorial::Get(TaGTutorial::Done).Text))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
					[
						SNew(STextBlock).Font(Font(15.f, false)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.8f, 0.82f, 0.86f))
						.Text(T(TEXT("読み勝つと「ゾーン」：少しの間速くなり、次のシュートが入りやすい。パスでつなぐほどボールが光り、連携のシュートが決まりやすい。")))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 18.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(6.f)
						[
							Button(T(TEXT("BOTと3on3で遊ぶ")), [PC]() { if (PC.IsValid()) { PC->QuickPlay(); } }, true, &OutFocus, nullptr, 22.f)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(6.f)
						[
							Button(T(TEXT("ヒーローを選ぶ")), [PC]() { if (PC.IsValid()) { PC->PlayOffline(); } }, false, nullptr, nullptr, 20.f)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(6.f)
						[
							Button(T(TEXT("タイトルへ")), [PC]() { if (PC.IsValid()) { PC->LeaveToTitle(); } }, false, nullptr, nullptr, 20.f)
						]
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
