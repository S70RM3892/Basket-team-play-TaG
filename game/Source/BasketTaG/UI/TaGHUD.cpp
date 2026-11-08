#include "UI/TaGHUD.h"
#include "Character/TaGCharacter.h"
#include "Core/TaGGameState.h"
#include "Core/TaGRules.h"
#include "Core/TaGTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float MeterW = 420.f;
	constexpr float MeterH = 16.f;

	float MeterX(float T)
	{
		return FMath::Clamp(T / TaG::ShotAutoRelease, 0.f, 1.f) * MeterW;
	}

	FSlateFontInfo Font(float Size)
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		F.OutlineSettings.OutlineSize = 2;
		F.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		return F;
	}
}

void ATaGHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalController() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	TWeakObjectPtr<APlayerController> WeakPC = PC;
	TWeakObjectPtr<UWorld> WeakWorld = GetWorld();
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");

	auto GetGS = [WeakWorld]() -> ATaGGameState*
	{
		return WeakWorld.IsValid() ? WeakWorld->GetGameState<ATaGGameState>() : nullptr;
	};
	auto GetMe = [WeakPC]() -> ATaGCharacter*
	{
		return WeakPC.IsValid() ? Cast<ATaGCharacter>(WeakPC->GetPawn()) : nullptr;
	};
	auto IsCharging = [GetMe]()
	{
		const ATaGCharacter* Me = GetMe();
		return Me && Me->IsChargingShot();
	};

	TSharedRef<SVerticalBox> PopupBox = SNew(SVerticalBox);
	for (int32 i = 0; i < 4; ++i)
	{
		PopupBox->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Font(Font(i == 0 ? 30.f : 22.f))
			.Text_Lambda([GetGS, i]()
			{
				const ATaGGameState* S = GetGS();
				return S && S->GetPopups().IsValidIndex(i) ? S->GetPopups()[i].Text : FText::GetEmpty();
			})
			.ColorAndOpacity_Lambda([GetGS, i]()
			{
				const ATaGGameState* S = GetGS();
				if (S && S->GetPopups().IsValidIndex(i))
				{
					FLinearColor C = S->GetPopups()[i].Color;
					C.A = FMath::Clamp(S->GetPopups()[i].TimeLeft / 0.4f, 0.f, 1.f);
					return FSlateColor(C);
				}
				return FSlateColor(FLinearColor::Transparent);
			})
		];
	}

	Root = SNew(SOverlay)
		// Score and clock
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 18.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font(30.f))
				.Text_Lambda([GetGS]()
				{
					const ATaGGameState* S = GetGS();
					return S ? FText::FromString(FString::Printf(TEXT("あなた  %d  -  %d  BOT"), S->Score0, S->Score1)) : FText::GetEmpty();
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font(20.f))
				.Text_Lambda([GetGS]()
				{
					const ATaGGameState* S = GetGS();
					if (!S)
					{
						return FText::GetEmpty();
					}
					const TCHAR* Side = S->PossessionTeam == 0 ? TEXT("攻撃") : TEXT("守備");
					return FText::FromString(FString::Printf(TEXT("%s   ショットクロック %.1f   (11点先取)"), Side, FMath::Max(0.f, S->ShotClock)));
				})
				.ColorAndOpacity_Lambda([GetGS]()
				{
					const ATaGGameState* S = GetGS();
					return FSlateColor(S && S->ShotClock < 4.f ? FLinearColor(1.f, 0.4f, 0.3f) : FLinearColor::White);
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f)
			[
				SNew(STextBlock).Font(Font(20.f))
				.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.3f))
				.Text_Lambda([GetGS, GetMe]()
				{
					const ATaGGameState* S = GetGS();
					const ATaGCharacter* Me = GetMe();
					if (S && Me && S->NeedsClear(Me->GetTeam()))
					{
						return FText::FromString(TEXT("アークの外へクリアしてから攻める"));
					}
					return FText::GetEmpty();
				})
			]
		]
		// Popups
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 170.f)
		[
			PopupBox
		]
		// Crosshair
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(6.f).HeightOverride(6.f)
			[
				SNew(SImage).Image(White).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.8f))
			]
		]
		// Shot meter
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 110.f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([IsCharging]() { return IsCharging() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock).Font(Font(24.f))
				.Text_Lambda([GetMe]()
				{
					const ATaGCharacter* Me = GetMe();
					if (!Me)
					{
						return FText::GetEmpty();
					}
					const FTaGShotEval E = FTaGRules::EvaluateShot(Me, Me->GetChargeTime());
					return FText::FromString(FString::Printf(TEXT("%d点シュート  成功率 %d%%%s"), E.Points, FMath::RoundToInt(E.Chance * 100.f), E.bContested ? TEXT("  (妨害されている)") : TEXT("")));
				})
				.ColorAndOpacity_Lambda([GetMe]()
				{
					const ATaGCharacter* Me = GetMe();
					const bool bContested = Me && FTaGRules::EvaluateShot(Me, Me->GetChargeTime()).bContested;
					return FSlateColor(bContested ? FLinearColor(1.f, 0.35f, 0.3f) : FLinearColor::White);
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(MeterW).HeightOverride(MeterH)
				[
					SNew(SCanvas)
					+ SCanvas::Slot().Position(FVector2D(0.f, 0.f)).Size(FVector2D(MeterW, MeterH))
					[
						SNew(SImage).Image(White).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f))
					]
					+ SCanvas::Slot().Position(FVector2D(MeterX(TaG::ShotGoodLo), 0.f)).Size(FVector2D(MeterX(TaG::ShotGoodHi) - MeterX(TaG::ShotGoodLo), MeterH))
					[
						SNew(SImage).Image(White).ColorAndOpacity(FLinearColor(0.15f, 0.75f, 0.3f, 0.85f))
					]
					+ SCanvas::Slot().Position(FVector2D(MeterX(TaG::ShotPerfectLo), 0.f)).Size(FVector2D(MeterX(TaG::ShotPerfectHi) - MeterX(TaG::ShotPerfectLo), MeterH))
					[
						SNew(SImage).Image(White).ColorAndOpacity(FLinearColor(1.f, 0.9f, 0.3f, 0.95f))
					]
					+ SCanvas::Slot()
					.Position_Lambda([GetMe]()
					{
						const ATaGCharacter* Me = GetMe();
						return FVector2D(MeterX(Me ? Me->GetChargeTime() : 0.f) - 2.f, -6.f);
					})
					.Size(FVector2D(4.f, MeterH + 12.f))
					[
						SNew(SImage).Image(White).ColorAndOpacity(FLinearColor::White)
					]
				]
			]
		]
		// Controls help
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(18.f)
		[
			SNew(STextBlock).Font(Font(13.f))
			.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.75f))
			.Text(FText::FromString(TEXT(
				"移動 WASD/左スティック   視点 マウス/右スティック   ジャンプ Space/A\n"
				"シュート 左クリック/RT（押して溜め、跳んだ頂点で離す）\n"
				"ブロック 右クリック/RB（視線の先に手を出す）   スティール F/X   リスタート R/START")))
		]
		// Result
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 200.f)
		[
			SNew(STextBlock).Font(Font(44.f))
			.Text_Lambda([GetGS]()
			{
				const ATaGGameState* S = GetGS();
				if (S && S->bMatchOver)
				{
					return FText::FromString(S->WinnerTeam == 0 ? TEXT("勝利！") : TEXT("敗北…"));
				}
				return FText::GetEmpty();
			})
		];

	GEngine->GameViewport->AddViewportWidgetContent(Root.ToSharedRef(), 10);
}

void ATaGHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Root.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Root.ToSharedRef());
	}
	Root.Reset();
	Super::EndPlay(EndPlayReason);
}
