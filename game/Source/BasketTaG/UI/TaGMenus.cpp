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
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
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
	// Centre the labels on the body by their measured width (Japanese glyphs are about twice as wide as a guess by
	// character count)
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

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
		const float NameW = (float)Measure->Measure(Label, NameFont).X;
		FSlateDrawElement::MakeText(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Local - FVector2f(NameW * 0.5f, 18.f))),
			Label, NameFont, ESlateDrawEffect::None, Color.CopyWithNewOpacity(Alpha));
		// Guard under the name: a body low on guard is one tackle from going down
		{
			const FSlateBrush* Bar = FCoreStyle::Get().GetBrush("WhiteBrush");
			const float G = FMath::Clamp(C->GetGuard() / FMath::Max(C->GetMaxGuard(), 1.f), 0.f, 1.f);
			const FVector2f At = Local + FVector2f(-22.f, 5.f);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(44.f, 4.f), FSlateLayoutTransform(At)),
				Bar, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(44.f * G, 4.f), FSlateLayoutTransform(At)),
				Bar, ESlateDrawEffect::None, Color.CopyWithNewOpacity(Alpha));
		}
		FString Sub;
		if (C->IsDown()) { Sub = TEXT("ダウン"); }
		else if (C == PassTarget) { Sub = TEXT("▼ パス先"); }
		else if (C->IsInZone()) { Sub = TEXT("ゾーン"); }
		if (!Sub.IsEmpty())
		{
			const float SubW = (float)Measure->Measure(Sub, SmallFont).X;
			FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
				AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Local - FVector2f(SubW * 0.5f, 36.f))),
				Sub, SmallFont, ESlateDrawEffect::None, C == PassTarget ? TaG::GoldColor() : FLinearColor::White);
		}
	}
	return LayerId + 2;
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
					.Text(T(TEXT("海に沈みかけた積層都市ナギサ。バスケで電気をつくり、勝った地区に明かりが灯る。\n技のほかは、背・リーチ・跳躍・速さ・重さの差がそのまま出る普通のバスケ。技だけが、その差を越える。\n読み合いは じゃんけん：突破・シュート・かわし 対 タックル・シュート妨害・間合い。")))
				]
			]
		];
}

// ================================================================ Shared art (portraits, icons, roles)

const FSlateBrush* TaGUI::PortraitBrush(const FTaGHeroDef& H)
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

const FSlateBrush* TaGUI::IconBrush(const FString& Name)
{
	static TMap<FString, TSharedPtr<FSlateBrush>> Cache;
	if (Name.IsEmpty())
	{
		return nullptr;
	}
	if (const TSharedPtr<FSlateBrush>* Found = Cache.Find(Name))
	{
		return Found->Get();
	}
	TSharedPtr<FSlateBrush> Brush;
	const FString Path = FString::Printf(TEXT("/Game/Art/UI/Icons/T_Icon_%s.T_Icon_%s"), *Name, *Name);
	if (UTexture2D* Tex = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)))
	{
		Tex->AddToRoot();
		Brush = MakeShared<FSlateBrush>();
		Brush->SetResourceObject(Tex);
		Brush->ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
	}
	Cache.Add(Name, Brush);
	return Brush.Get();
}

FString TaGUI::ArtIconName(ETaGArt Art)
{
	const FTaGArtDef& A = TaGHeroes::GetArt(Art);
	if (Art == ETaGArt::None)
	{
		return FString();
	}
	if (A.Kind == ETaGArtKind::Custom)
	{
		return TEXT("Art_") + StaticEnum<ETaGArt>()->GetNameStringByValue(static_cast<int64>(Art));
	}
	switch (A.Kind)
	{
	case ETaGArtKind::Dash: return TEXT("ArtKind_Dash");
	case ETaGArtKind::Leap: return TEXT("ArtKind_Leap");
	case ETaGArtKind::Shell: return TEXT("ArtKind_Shell");
	case ETaGArtKind::Phase: return TEXT("ArtKind_Phase");
	case ETaGArtKind::Counter: return TEXT("ArtKind_Counter");
	case ETaGArtKind::Decoy: return TEXT("ArtKind_Decoy");
	case ETaGArtKind::ShotBuff: return TEXT("ArtKind_ShotBuff");
	case ETaGArtKind::Wall: return TEXT("ArtKind_Wall");
	case ETaGArtKind::ShieldFollow: return TEXT("ArtKind_ShieldFollow");
	case ETaGArtKind::Root: return TEXT("ArtKind_Root");
	case ETaGArtKind::Tackle: return TEXT("ArtKind_Tackle");
	case ETaGArtKind::Cone: return TEXT("ArtKind_Cone");
	case ETaGArtKind::Aura: return TEXT("ArtKind_Aura");
	case ETaGArtKind::Bolt: return TEXT("ArtKind_Bolt");
	case ETaGArtKind::Field: return TEXT("ArtKind_Field");
	case ETaGArtKind::TeamAura: return TEXT("ArtKind_TeamAura");
	case ETaGArtKind::TeamOne: return TEXT("ArtKind_TeamOne");
	case ETaGArtKind::Pass: return TEXT("ArtKind_Pass");
	case ETaGArtKind::ContestStance: return TEXT("ArtKind_ContestStance");
	case ETaGArtKind::ContestAt: return TEXT("ArtKind_ContestAt");
	default: return FString();
	}
}

FLinearColor TaGUI::RoleColor(ETaGRole Role)
{
	switch (Role)
	{
	case ETaGRole::Wall: return FLinearColor(0.95f, 0.62f, 0.25f);
	case ETaGRole::Scorer: return FLinearColor(1.f, 0.45f, 0.45f);
	default: return FLinearColor(0.45f, 0.85f, 1.f);
	}
}

const TCHAR* TaGUI::RoleIconName(ETaGRole Role)
{
	switch (Role)
	{
	case ETaGRole::Wall: return TEXT("Role_Wall");
	case ETaGRole::Scorer: return TEXT("Role_Scorer");
	default: return TEXT("Role_Playmaker");
	}
}

// ================================================================ Lobby / hero select

namespace
{
	TSharedRef<SWidget> Portrait(const FTaGHeroDef& H, float Size)
	{
		if (const FSlateBrush* B = TaGUI::PortraitBrush(H))
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

	/** An icon tinted by Color on a dark tile; the first letters of Fallback when the icon art is missing. */
	TSharedRef<SWidget> IconTile(TFunction<FString()> Name, TFunction<FLinearColor()> Color, float Size, float Inner)
	{
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.04f, 0.05f, 0.08f, 0.9f))]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(Inner).HeightOverride(Inner)
					[
						SNew(SImage)
						.Image_Lambda([Name]() { const FSlateBrush* B = TaGUI::IconBrush(Name()); return B ? B : White(); })
						.ColorAndOpacity_Lambda([Name, Color]() { return FSlateColor(TaGUI::IconBrush(Name()) ? Color() : Color() * 0.35f); })
					]
				]
			];
	}

	/**
	 * The body in real units. Everything outside the arts is plain basketball, where these numbers decide who wins
	 * the rebound, who gets past whom and whose hand reaches the shot; the arts are what can close the gap.
	 */
	struct FBodyStat
	{
		const TCHAR* Name;
		float Value;
		float Lo;
		float Hi;
		FString Text;
	};

	TArray<FBodyStat> BodyStats(const FTaGHeroDef& H)
	{
		const float Height = H.EyeHeight + 11.f;                                                // the crown above the eyes
		const float Speed = TaG::WalkSpeed * H.SpeedMul / 100.f;                                // top running speed, m/s
		const float Jump = FMath::Square(TaG::JumpVelocity * H.JumpMul) / (2.f * TaG::Gravity); // standing jump, cm
		const float Reach = FTaGRules::StandingReach(H) * FMath::Sqrt(H.ReachMul);              // hand overhead, cm
		const float Mass = H.Weight * 75.f;
		return {
			{ TEXT("身長"), Height, 140.f, 270.f, FString::Printf(TEXT("%d cm"), FMath::RoundToInt(Height)) },
			{ TEXT("速さ"), Speed, 3.6f, 5.6f, FString::Printf(TEXT("%.1f m/s"), Speed) },
			{ TEXT("跳躍"), Jump, 8.f, 95.f, FString::Printf(TEXT("%d cm"), FMath::RoundToInt(Jump)) },
			{ TEXT("リーチ"), Reach, 180.f, 420.f, FString::Printf(TEXT("%d cm"), FMath::RoundToInt(Reach)) },
			{ TEXT("体重"), Mass, 40.f, 260.f, FString::Printf(TEXT("%d kg"), FMath::RoundToInt(Mass)) },
		};
	}

	const TCHAR* RoleLabel(ETaGRole Role)
	{
		switch (Role)
		{
		case ETaGRole::Wall: return TEXT("ウォール（壁役）");
		case ETaGRole::Scorer: return TEXT("スコアラー（点取り）");
		default: return TEXT("プレイメーカー（司令塔）");
		}
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

	// ---- Bottom: the thirty heroes as portraits, by role (ten each, two rows of five)
	TSharedRef<SHorizontalBox> Strip = SNew(SHorizontalBox);
	for (const ETaGRole Role : { ETaGRole::Wall, ETaGRole::Scorer, ETaGRole::Playmaker })
	{
		TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(2.f));
		int32 n = 0;
		for (int32 i = 0; i < TaGHeroes::Num; ++i)
		{
			const ETaGHero Id = static_cast<ETaGHero>(i);
			const FTaGHeroDef& H = TaGHeroes::Get(Id);
			if (H.Role != Role)
			{
				continue;
			}
			TSharedRef<SButton> Card = SNew(SButton)
				.ButtonColorAndOpacity_Lambda([PC, Id]()
				{
					const ATaGPlayerState* P = GetMyPS(PC);
					return FSlateColor(P && P->Hero == Id ? FLinearColor(0.1f, 0.9f, 0.8f, 1.f) : FLinearColor(0.12f, 0.13f, 0.17f, 1.f));
				})
				.ContentPadding(FMargin(2.f))
				.ToolTipText(T(H.Name))
				.OnClicked_Lambda([PC, Id]() { if (PC.IsValid()) { PC->ServerSetHero(Id); } return FReply::Handled(); })
				[
					Portrait(H, 46.f)
				];
			if (!OutFocus.IsValid())
			{
				OutFocus = Card;
			}
			Grid->AddSlot(n % 5, n / 5)[Card];
			n++;
		}
		const FLinearColor RC = RoleColor(Role);
		const FString RoleIcon = RoleIconName(Role);
		Strip->AddSlot().AutoWidth().Padding(0.f, 0.f, 14.f, 0.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(2.f, 0.f, 0.f, 3.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					IconTile([RoleIcon]() { return RoleIcon; }, [RC]() { return RC; }, 22.f, 18.f)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock).Font(Font(13.f)).ColorAndOpacity(RC).Text(T(RoleLabel(Role)))
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Grid
			]
		];
	}

	// ---- Left: the chosen hero
	TSharedRef<SVerticalBox> Stats = SNew(SVerticalBox);
	for (int32 i = 0; i < 5; ++i)
	{
		Stats->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(58.f)
				[
					SNew(STextBlock).Font(Font(14.f)).Text_Lambda([Selected, i]() { return T(BodyStats(Selected())[i].Name); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(200.f).HeightOverride(8.f)
				[
					SNew(STaGSegmentBar).Segment(1000.f).Size(FVector2D(200.f, 8.f)).Max(1.f)
					.Value_Lambda([Selected, i]()
					{
						const FBodyStat St = BodyStats(Selected())[i];
						return FMath::Clamp((St.Value - St.Lo) / (St.Hi - St.Lo), 0.04f, 1.f);
					})
					.Color_Lambda([Selected]() { return RoleColor(Selected().Role); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Font(Font(15.f)).Text_Lambda([Selected, i]() { return T(BodyStats(Selected())[i].Text); })
			]
		];
	}

	auto ArtCard = [Selected](int32 Which, const TCHAR* Key)
	{
		auto Art = [Selected, Which]() { return Which == 1 ? Selected().Art1 : Selected().Art2; };
		const FString KeyText(Key);
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
			[
				IconTile([Art]() { return ArtIconName(Art()); }, [Art]() { return TaGHeroes::GetArt(Art()).Color; }, 48.f, 36.f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						SNew(STextBlock).Font(Font(18.f)).Text_Lambda([Art]() { return T(TaGHeroes::GetArt(Art()).Name); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(8.f, 0.f, 0.f, 2.f)
					[
						SNew(STextBlock).Font(Font(12.f)).ColorAndOpacity(TaG::GoldColor())
						.Text_Lambda([Art]() { return T(FString::Printf(TEXT("〔%s〕"), TaGHeroes::HandName(TaGHeroes::GetArt(Art()).Hand))); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(8.f, 0.f, 0.f, 2.f)
					[
						SNew(STextBlock).Font(Font(11.f, false)).ColorAndOpacity(FLinearColor(0.7f, 0.73f, 0.78f)).Text(T(KeyText))
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(13.f, false)).AutoWrapText(true).Text_Lambda([Art]() { return T(TaGHeroes::GetArt(Art()).Desc); })
				]
			];
	};

	TSharedRef<SWidget> Detail = SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.015f, 0.02f, 0.04f, 0.78f)).Padding(FMargin(20.f, 16.f))
		[
			SNew(SBox).WidthOverride(430.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Font(46.f)).Text_Lambda([Selected]() { return T(Selected().Name); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						IconTile([Selected]() { return FString(RoleIconName(Selected().Role)); }, [Selected]() { return RoleColor(Selected().Role); }, 24.f, 20.f)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock).Font(Font(14.f)).AutoWrapText(true)
						.ColorAndOpacity_Lambda([Selected]() { return FSlateColor(RoleColor(Selected().Role)); })
						.Text_Lambda([Selected]()
						{
							const FTaGHeroDef& H = Selected();
							return T(FString::Printf(TEXT("%s・体格%s・%s"), TaGHeroes::RoleName(H.Role), TaGHeroes::SizeName(H.Size), H.Kind));
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
				[
					SNew(STextBlock).Font(Font(14.f, false)).AutoWrapText(true).Text_Lambda([Selected]() { return T(Selected().Blurb); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock).Font(Font(12.f)).ColorAndOpacity(FLinearColor(0.7f, 0.73f, 0.78f))
					.Text(T(TEXT("からだ（技のほかは、この差がそのまま出る）")))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					Stats
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 10.f)
				[
					SNew(STextBlock).Font(Font(12.f, false)).ColorAndOpacity(FLinearColor(0.7f, 0.73f, 0.78f))
					.Text_Lambda([Selected]() { return T(FString::Printf(TEXT("ガード %d（TaGの耐久。タックルで削られる）"), FMath::RoundToInt(Selected().Guard))); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock).Font(Font(12.f)).ColorAndOpacity(FLinearColor(0.7f, 0.73f, 0.78f))
					.Text(T(TEXT("技（体の差を越えられる、ただ二つの力）")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
				[
					ArtCard(1, TEXT("LT / C"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					ArtCard(2, TEXT("Y / V"))
				]
			]
		];

	// ---- Right: the teams and the start
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
			Out += TEXT("BOT（ランダム）\n");
		}
		return T(Out.TrimEnd());
	};
	auto TeamBox = [PC, Roster](int32 Team)
	{
		const FLinearColor C = Team == 0 ? Teal : Orange;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(3.f)[SNew(SImage).Image(White()).ColorAndOpacity(C)]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 2.f)
			[
				SNew(STextBlock).Font(Font(17.f)).ColorAndOpacity(C).Text(T(Team == 0 ? TEXT("チームA") : TEXT("チームB")))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(13.f, false)).Text_Lambda([Roster, Team]() { return Roster(Team); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				Button(T(Team == 0 ? TEXT("Aに入る") : TEXT("Bに入る")), [PC, Team]() { if (PC.IsValid()) { PC->ServerSetTeam(Team); } }, false, nullptr,
					[PC, Team]() { const ATaGPlayerState* P = GetMyPS(PC); return P && P->Team == Team; }, 13.f)
			];
	};

	auto ModeButton = [PC](int32 Size)
	{
		return Button(T(FString::Printf(TEXT("%don%d"), Size, Size)),
			[PC, Size]() { if (PC.IsValid()) { const ATaGGameState* S = GetGS(PC); PC->ServerSetLobbyOptions(Size, S ? S->BotSkill : 1); } },
			false, nullptr, [PC, Size]() { const ATaGGameState* S = GetGS(PC); return S && S->TeamSize == Size; }, 16.f);
	};
	auto BotButton = [PC](int32 Level, const TCHAR* Label)
	{
		return Button(T(Label),
			[PC, Level]() { if (PC.IsValid()) { const ATaGGameState* S = GetGS(PC); PC->ServerSetLobbyOptions(S ? S->TeamSize : 3, Level); } },
			false, nullptr, [PC, Level]() { const ATaGGameState* S = GetGS(PC); return S && S->BotSkill == Level; }, 14.f);
	};

	return SNew(SOverlay)
		// Top bar
		+ SOverlay::Slot().VAlign(VAlign_Top)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.01f, 0.015f, 0.03f, 0.82f)).Padding(FMargin(26.f, 10.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Font(28.f)).ColorAndOpacity(Teal).Text(T(TEXT("ヒーロー選択")))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(26.f, 0.f, 6.f, 0.f)
				[
					SNew(SHorizontalBox)
					.Visibility(bHost ? EVisibility::Visible : EVisibility::Collapsed)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[ModeButton(1)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[ModeButton(2)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[ModeButton(3)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(18.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock).Font(Font(14.f)).Text(T(TEXT("BOT")))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[BotButton(0, TEXT("練習"))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[BotButton(1, TEXT("ふつう"))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[BotButton(2, TEXT("強い"))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[BotButton(3, TEXT("鬼"))]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(Font(14.f)).ColorAndOpacity(TaG::GoldColor())
					.Text_Lambda([PC]()
					{
						const ATaGGameState* S = GetGS(PC);
						if (S && S->bOnline && PC.IsValid() && PC->IsHost())
						{
							return T(FString::Printf(TEXT("友達に伝えるアドレス：%s"), *LocalIPs()));
						}
						return S && S->bOnline ? T(TEXT("オンライン・ロビー")) : T(TEXT("オフライン（BOTと対戦）"));
					})
				]
			]
		]
		// The chosen hero (the body itself stands on the court to the right)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(26.f, 74.f, 0.f, 0.f)
		[
			Detail
		]
		// The roster strip
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(26.f, 0.f, 0.f, 18.f)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.01f, 0.015f, 0.03f, 0.72f)).Padding(FMargin(12.f, 8.f))
			[
				Strip
			]
		]
		// Teams and start
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 26.f, 18.f)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.01f, 0.015f, 0.03f, 0.8f)).Padding(FMargin(16.f, 12.f))
			[
				SNew(SBox).WidthOverride(330.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f, 0.f)[TeamBox(0)]
						+ SHorizontalBox::Slot().FillWidth(1.f)[TeamBox(1)]
					]
					// The start button takes the whole width (it was clipped beside the other one)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
					[
						bHost
						? Button(T(TEXT("試合開始")), [PC]() { if (PC.IsValid()) { PC->ServerStartMatch(); } }, true, nullptr, nullptr, 24.f)
						: StaticCastSharedRef<SWidget>(SNew(STextBlock).Font(Font(16.f)).Text(T(TEXT("ホストの開始を待っています…"))))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 8.f, 0.f, 0.f)
					[
						Button(T(TEXT("タイトルへ")), [PC]() { if (PC.IsValid()) { PC->LeaveToTitle(); } }, false, nullptr, nullptr, 14.f)
					]
				]
			]
		];
}

// ================================================================ Results

namespace
{
	TArray<const ATaGPlayerState*> TeamPlayers(const ATaGGameState* S, int32 Team)
	{
		TArray<const ATaGPlayerState*> Out;
		for (const APlayerState* P : S->PlayerArray)
		{
			const ATaGPlayerState* T2 = Cast<ATaGPlayerState>(P);
			if (T2 && T2->Team == Team)
			{
				Out.Add(T2);
			}
		}
		return Out;
	}

	/** Who carried the match: points first, then the plays that made points for others or stopped them. */
	float MvpScore(const ATaGPlayerState* P)
	{
		return P->Stats.Points + 0.7f * P->Stats.Assists + 0.5f * (P->Stats.Steals + P->Stats.Blocks) + 0.3f * P->Stats.ReadWins;
	}

	const ATaGPlayerState* Mvp(const ATaGGameState* S)
	{
		const ATaGPlayerState* Best = nullptr;
		for (const APlayerState* P : S->PlayerArray)
		{
			const ATaGPlayerState* T2 = Cast<ATaGPlayerState>(P);
			if (T2 && T2->Team >= 0 && (!Best || MvpScore(T2) > MvpScore(Best)))
			{
				Best = T2;
			}
		}
		return Best;
	}

	FString PlayerLabel(const ATaGPlayerState* P)
	{
		return P->bIsBotPlayer ? FString(TEXT("BOT")) : P->GetPlayerName();
	}
}

TSharedRef<SWidget> TaGUI::MakeResults(FPCPtr PC, TSharedPtr<SWidget>& OutFocus)
{
	const bool bHost = PC.IsValid() && PC->IsHost();
	auto TeamOf = [PC](bool bMine) { const ATaGGameState* S = GetGS(PC); return S ? (bMine ? S->GetLocalTeam() : 1 - S->GetLocalTeam()) : 0; };

	auto Cell = [](TFunction<FText()> Text, float Width, float Size, bool bBold)
	{
		return SNew(SBox).WidthOverride(Width).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font(Size, bBold)).Text_Lambda([Text]() { return Text(); })
			];
	};

	auto TeamPanel = [PC, TeamOf, Cell](bool bMine)
	{
		const FLinearColor C = bMine ? Teal : Orange;
		const FLinearColor Deep = bMine ? FLinearColor(0.02f, 0.36f, 0.32f, 0.95f) : FLinearColor(0.46f, 0.12f, 0.035f, 0.95f);
		TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		for (int32 i = 0; i < 3; ++i)
		{
			auto Who = [PC, TeamOf, bMine, i]() -> const ATaGPlayerState*
			{
				const ATaGGameState* S = GetGS(PC);
				if (!S) { return nullptr; }
				const TArray<const ATaGPlayerState*> Team = TeamPlayers(S, TeamOf(bMine));
				return Team.IsValidIndex(i) ? Team[i] : nullptr;
			};
			auto Num = [Who](int32 Field)
			{
				return [Who, Field]()
				{
					const ATaGPlayerState* P = Who();
					if (!P) { return FText::GetEmpty(); }
					const int32 V[5] = { P->Stats.Points, P->Stats.Assists, P->Stats.Steals, P->Stats.Blocks, P->Stats.ReadWins };
					return FText::AsNumber(V[Field]);
				};
			};
			Rows->AddSlot().AutoHeight().Padding(0.f, 3.f)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([Who]() { return Who() ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(42.f).HeightOverride(42.f)
					[
						SNew(SImage)
						.Image_Lambda([Who]() { const ATaGPlayerState* P = Who(); const FSlateBrush* B = P ? PortraitBrush(TaGHeroes::Get(P->Hero)) : nullptr; return B ? B : White(); })
						.ColorAndOpacity_Lambda([Who]() { const ATaGPlayerState* P = Who(); return FSlateColor(P && PortraitBrush(TaGHeroes::Get(P->Hero)) ? FLinearColor::White : FLinearColor(0.2f, 0.22f, 0.26f)); })
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(16.f)).Text_Lambda([Who]() { const ATaGPlayerState* P = Who(); return P ? T(TaGHeroes::Get(P->Hero).Name) : FText::GetEmpty(); })
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(11.f, false)).ColorAndOpacity(FLinearColor(0.7f, 0.73f, 0.78f))
						.Text_Lambda([Who]() { const ATaGPlayerState* P = Who(); return P ? T(PlayerLabel(P)) : FText::GetEmpty(); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell(Num(0), 44.f, 18.f, true)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell(Num(1), 44.f, 16.f, false)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell(Num(2), 44.f, 16.f, false)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell(Num(3), 44.f, 16.f, false)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell(Num(4), 44.f, 16.f, false)]
			];
		}
		return SNew(SBox).WidthOverride(470.f)
			[
				SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.015f, 0.02f, 0.04f, 0.86f)).Padding(0.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Deep).Padding(FMargin(14.f, 6.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(Font(20.f))
								.Text_Lambda([PC, TeamOf, bMine]() { const ATaGGameState* S = GetGS(PC); return S ? T(FString::Printf(TEXT("%s  %d点"), bMine ? TEXT("味方") : TEXT("相手"), S->GetScore(TeamOf(bMine)))) : FText::GetEmpty(); })
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell([]() { return T(TEXT("点")); }, 44.f, 12.f, true)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell([]() { return T(TEXT("アシ")); }, 44.f, 12.f, true)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell([]() { return T(TEXT("スティ")); }, 44.f, 12.f, true)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell([]() { return T(TEXT("ブロ")); }, 44.f, 12.f, true)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell([]() { return T(TEXT("読み")); }, 44.f, 12.f, true)]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(14.f, 6.f))
					[
						Rows
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(14.f, 0.f, 14.f, 10.f))
					[
						SNew(STextBlock).Font(Font(12.f, false)).ColorAndOpacity(FLinearColor(0.7f, 0.73f, 0.78f)).AutoWrapText(true)
						.Text_Lambda([PC, TeamOf, bMine]()
						{
							const ATaGGameState* S = GetGS(PC);
							if (!S) { return FText::GetEmpty(); }
							const FTaGTeamStats& St = S->Stats(TeamOf(bMine));
							const float PPP = St.Possessions ? float(St.Points) / St.Possessions : 0.f;
							const float Iso = St.IsoPossessions ? float(St.IsoPoints) / St.IsoPossessions : 0.f;
							const float Tm = St.TeamPossessions ? float(St.TeamPoints) / St.TeamPossessions : 0.f;
							return T(FString::Printf(TEXT("シュート %d/%d　攻撃1回あたり %.2f（1対1 %.2f / 連携 %.2f）"), St.Makes, St.Shots, PPP, Iso, Tm));
						})
					]
				]
			];
	};

	auto MvpPS = [PC]() -> const ATaGPlayerState* { const ATaGGameState* S = GetGS(PC); return S ? Mvp(S) : nullptr; };
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f))
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font(62.f))
				.Text_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return S && S->WinnerTeam == S->GetLocalTeam() ? T(TEXT("勝利")) : T(TEXT("敗北")); })
				.ColorAndOpacity_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return FSlateColor(S && S->WinnerTeam == S->GetLocalTeam() ? Teal : Orange); })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font(16.f, false)).ColorAndOpacity(FLinearColor(0.82f, 0.85f, 0.9f))
				.Text_Lambda([PC]()
				{
					const ATaGGameState* S = GetGS(PC);
					if (!S) { return FText::GetEmpty(); }
					const bool bWin = S->WinnerTeam == S->GetLocalTeam();
					return T(FString::Printf(TEXT("%d - %d　%s"), S->GetScore(S->GetLocalTeam()), S->GetScore(1 - S->GetLocalTeam()),
						bWin ? TEXT("味方の地区に明かりが灯った") : TEXT("相手の地区に明かりが灯った")));
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 18.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[TeamPanel(true)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(22.f, 0.f, 0.f, 0.f)[TeamPanel(false)]
			]
			// The player of the match
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 16.f, 0.f, 0.f)
			[
				SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.2f, 0.15f, 0.02f, 0.9f)).Padding(FMargin(14.f, 8.f))
				.Visibility_Lambda([MvpPS]() { return MvpPS() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(64.f).HeightOverride(64.f)
						[
							SNew(SImage)
							.Image_Lambda([MvpPS]() { const ATaGPlayerState* P = MvpPS(); const FSlateBrush* B = P ? PortraitBrush(TaGHeroes::Get(P->Hero)) : nullptr; return B ? B : White(); })
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font(14.f)).ColorAndOpacity(TaG::GoldColor()).Text(T(TEXT("この試合の主役")))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font(24.f))
							.Text_Lambda([MvpPS]() { const ATaGPlayerState* P = MvpPS(); return P ? T(FString::Printf(TEXT("%s（%s）"), TaGHeroes::Get(P->Hero).Name, *PlayerLabel(P))) : FText::GetEmpty(); })
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(Font(14.f, false))
							.Text_Lambda([MvpPS]()
							{
								const ATaGPlayerState* P = MvpPS();
								return P ? T(FString::Printf(TEXT("%d点　%dアシスト　%dスティール　%dブロック　読み勝ち%d"), P->Stats.Points, P->Stats.Assists, P->Stats.Steals, P->Stats.Blocks, P->Stats.ReadWins)) : FText::GetEmpty();
							})
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 18.f, 0.f, 0.f)
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
						"レイアップ：リングへ走りながらシュート（2歩で踏み切る）。突破からリング近くでダンク\n"
						"アリウープ：リングへ走る味方にパス。受ける側は「今！」でジャンプ A/Space\n"
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
					SettingRow(TEXT("視点の揺れ"), Stepper(
						[PC, Percent]() { return Percent(PC.IsValid() ? PC->ViewBob : 1.f); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.ViewBob -= 0.25f; }); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.ViewBob += 0.25f; }); }))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SettingRow(TEXT("リプレイ"), ValueButton(
						[PC]() { return T(PC.IsValid() && PC->bReplays ? TEXT("ダンクとアリウープを再生する") : TEXT("再生しない")); },
						[Apply]() { Apply([](ATaGPlayerController& P) { P.bReplays = !P.bReplays; }); }))
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
