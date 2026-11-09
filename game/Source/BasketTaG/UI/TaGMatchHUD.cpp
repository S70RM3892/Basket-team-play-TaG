#include "UI/TaGMenus.h"
#include "Ball/TaGBall.h"
#include "Character/TaGCharacter.h"
#include "Character/TaGPlayerController.h"
#include "Core/TaGGameState.h"
#include "Core/TaGHeroData.h"
#include "Core/TaGPlayerState.h"
#include "Core/TaGReplay.h"
#include "Core/TaGRules.h"
#include "Core/TaGTutorial.h"
#include "Core/TaGTypes.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

/**
 * The match HUD. Laid out the way first-person team games keep the screen clear: the scene owns the middle, and the
 * game state sits in the corners and along the edges, as icons and numbers rather than sentences.
 *   top     the score bug (each team's score, the shot clock, possession) with both teams' portraits beside it
 *   right   the event feed (reads, steals, blocks, baskets) in the colour of the team it favours
 *   bottom  left: your hero, guard in 25-point segments, your state / right: the six hands and arts with cooldowns
 *   centre  crosshair, shot meter, read-win banner, alley-oop prompt, the line warning, the replay frame
 */
namespace
{
	using FPCPtr = TWeakObjectPtr<ATaGPlayerController>;

	const FSlateBrush* White() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
	FText T(const FString& S) { return FText::FromString(S); }
	ATaGGameState* GetGS(const FPCPtr& PC) { return PC.IsValid() ? PC->GetWorld()->GetGameState<ATaGGameState>() : nullptr; }
	ATaGCharacter* GetMe(const FPCPtr& PC) { return PC.IsValid() ? Cast<ATaGCharacter>(PC->GetPawn()) : nullptr; }

	const FLinearColor Ink(0.025f, 0.03f, 0.055f, 0.84f);
	const FLinearColor Dim(0.62f, 0.66f, 0.72f);
	const FLinearColor AllyDeep(0.02f, 0.38f, 0.34f, 0.94f);
	const FLinearColor EnemyDeep(0.48f, 0.12f, 0.035f, 0.94f);

	/** The players of one team in a fixed order (by spawn), read every frame by the score bug. */
	TArray<ATaGCharacter*> TeamChars(const FPCPtr& PC, int32 Team)
	{
		TArray<ATaGCharacter*> Out;
		if (!PC.IsValid())
		{
			return Out;
		}
		for (TActorIterator<ATaGCharacter> It(PC->GetWorld()); It; ++It)
		{
			if (It->GetTeam() == Team)
			{
				Out.Add(*It);
			}
		}
		Out.Sort([](const ATaGCharacter& A, const ATaGCharacter& B) { return A.GetUniqueID() < B.GetUniqueID(); });
		return Out;
	}

	FLinearColor GuardColor(float Frac)
	{
		if (Frac > 0.5f) { return TaG::AllyColor(); }
		if (Frac > 0.25f) { return TaG::GoldColor(); }
		return FLinearColor(1.f, 0.3f, 0.25f);
	}

	// ------------------------------------------------------------ Hands and arts

	struct FTile
	{
		FString Icon;
		FString Label;
		FLinearColor Tint = FLinearColor::White;
		ETaGSlot Slot = ETaGSlot::Drive;
		bool bUsable = true;
	};

	/** What a button does right now (it changes with offense, defense and off the ball). */
	FTile TileInfo(const ATaGCharacter* Me, int32 Kind)
	{
		FTile Out;
		const int32 Role = Me->GetRole();
		const FTaGHeroDef& H = Me->GetHeroDef();
		const FLinearColor DriveC(0.45f, 0.85f, 1.f), TackleC(1.f, 0.55f, 0.2f), ContestC(1.f, 0.38f, 0.36f), DodgeC(0.92f, 0.94f, 1.f);
		const FLinearColor StepC(0.62f, 1.f, 0.62f), PassC = TaG::GoldColor();
		switch (Kind)
		{
		case 0:
			if (Role == 2) { Out = { TEXT("Tackle"), TEXT("タックル"), TackleC, ETaGSlot::Tackle, true }; }
			else if (Role == 1) { Out = { TEXT("Drive"), TEXT("突破"), DriveC, ETaGSlot::Drive, true }; }
			else { Out = { TEXT("Cut"), TEXT("カット"), DriveC, ETaGSlot::Drive, true }; }
			break;
		case 1:
			if (Role == 2) { Out = { TEXT("Contest"), TEXT("シュート妨害"), ContestC, ETaGSlot::Contest, true }; }
			else { Out = { TEXT("Dodge"), TEXT("かわし"), DodgeC, ETaGSlot::Dodge, Role == 1 }; }
			break;
		case 2:
			Out = { TEXT("Step"), TEXT("間合い"), StepC, ETaGSlot::Step, Role != 1 };
			break;
		case 3:
			if (Role == 2) { Out = { TEXT("Steal"), TEXT("スティール"), PassC, ETaGSlot::Steal, true }; }
			else { Out = { TEXT("Pass"), TEXT("パス"), PassC, ETaGSlot::Pass, Role == 1 }; }
			break;
		default:
		{
			const ETaGArt Art = Kind == 4 ? H.Art1 : H.Art2;
			const FTaGArtDef& A = TaGHeroes::GetArt(Art);
			Out = { TaGUI::ArtIconName(Art), A.Name, A.Color, Kind == 4 ? ETaGSlot::Art1 : ETaGSlot::Art2, Art != ETaGArt::None };
			break;
		}
		}
		return Out;
	}

	TSharedRef<SWidget> AbilityTile(FPCPtr PC, int32 Kind, const TCHAR* PadKey, const TCHAR* PcKey)
	{
		constexpr float S = 60.f;
		auto Info = [PC, Kind]() { const ATaGCharacter* Me = GetMe(PC); return Me ? TileInfo(Me, Kind) : FTile(); };
		auto Left = [PC, Kind]() { const ATaGCharacter* Me = GetMe(PC); return Me ? Me->GetCooldownLeft(TileInfo(Me, Kind).Slot) : 0.f; };
		auto Frac = [PC, Kind]() { const ATaGCharacter* Me = GetMe(PC); return Me ? Me->GetCooldownFraction(TileInfo(Me, Kind).Slot) : 0.f; };
		const FString Pad(PadKey);
		const FString Pc(PcKey);
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).WidthOverride(S).HeightOverride(S)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SImage).Image(White()).ColorAndOpacity(Ink)
					]
					// The icon, tinted by what the hand is; greyed while it comes back, dark when it does nothing here
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(40.f).HeightOverride(40.f)
						[
							SNew(SImage)
							.Image_Lambda([Info]() { const FSlateBrush* B = TaGUI::IconBrush(Info().Icon); return B ? B : White(); })
							.ColorAndOpacity_Lambda([Info, Left]()
							{
								const FTile Tl = Info();
								if (!TaGUI::IconBrush(Tl.Icon))
								{
									return FSlateColor(FLinearColor::Transparent);
								}
								if (!Tl.bUsable)
								{
									return FSlateColor(FLinearColor(0.28f, 0.3f, 0.34f));
								}
								return FSlateColor(Left() > 0.05f ? FMath::Lerp(Tl.Tint, FLinearColor(0.42f, 0.44f, 0.48f), 0.7f) : Tl.Tint);
							})
						]
					]
					// Without the icon art: the first characters of the name
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Font(TaGUI::Font(15.f))
						.Text_Lambda([Info]() { const FTile Tl = Info(); return TaGUI::IconBrush(Tl.Icon) ? FText::GetEmpty() : T(Tl.Label.Left(2)); })
					]
					// The cooldown shade drains from the top as the hand comes back
					+ SOverlay::Slot().VAlign(VAlign_Top)
					[
						SNew(SBox).HeightOverride_Lambda([Frac]() { return FOptionalSize(S * Frac()); })
						[
							SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
						]
					]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Font(TaGUI::Font(22.f))
						.Text_Lambda([Left]()
						{
							const float L = Left();
							if (L <= 0.05f)
							{
								return FText::GetEmpty();
							}
							return T(L >= 1.f ? FString::Printf(TEXT("%d"), FMath::CeilToInt(L)) : FString::Printf(TEXT("%.1f"), L));
						})
					]
					// Ready: a bar in the hand's colour along the bottom edge
					+ SOverlay::Slot().VAlign(VAlign_Bottom)
					[
						SNew(SBox).HeightOverride(4.f)
						[
							SNew(SImage).Image(White())
							.ColorAndOpacity_Lambda([Info, Left]()
							{
								const FTile Tl = Info();
								return FSlateColor(Tl.bUsable && Left() <= 0.05f ? Tl.Tint : FLinearColor(0.12f, 0.13f, 0.17f, 0.9f));
							})
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 3.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
				[
					SNew(STextBlock).Font(TaGUI::Font(14.f)).Text(T(Pad))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(4.f, 0.f, 0.f, 1.f)
				[
					SNew(STextBlock).Font(TaGUI::Font(10.f, false)).ColorAndOpacity(Dim).Text(T(Pc))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(74.f).HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(TaGUI::Font(11.f, false)).ColorAndOpacity(Dim).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text_Lambda([Info]() { return T(Info().Label); })
				]
			];
	}

	// ------------------------------------------------------------ Your hero

	TSharedRef<SWidget> HeroCard(FPCPtr PC)
	{
		return SNew(SHorizontalBox)
			.Visibility_Lambda([PC]() { return GetMe(PC) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
			[
				SNew(SBorder).BorderImage(White()).Padding(2.f)
				.BorderBackgroundColor_Lambda([PC]()
				{
					const ATaGCharacter* Me = GetMe(PC);
					return FSlateColor(Me && Me->IsInZone() ? TaG::GoldColor() : TaG::AllyColor());
				})
				[
					SNew(SBox).WidthOverride(76.f).HeightOverride(76.f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.05f, 0.06f, 0.09f, 0.95f))
						]
						+ SOverlay::Slot()
						[
							SNew(SImage)
							.Image_Lambda([PC]() { const ATaGCharacter* Me = GetMe(PC); const FSlateBrush* B = Me ? TaGUI::PortraitBrush(Me->GetHeroDef()) : nullptr; return B ? B : White(); })
							.ColorAndOpacity_Lambda([PC]()
							{
								const ATaGCharacter* Me = GetMe(PC);
								if (!Me) { return FSlateColor(FLinearColor::Transparent); }
								return FSlateColor(TaGUI::PortraitBrush(Me->GetHeroDef()) ? FLinearColor::White : Me->GetHeroDef().PrimaryColor);
							})
						]
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
					[
						SNew(STextBlock).Font(TaGUI::Font(26.f))
						.Text_Lambda([PC]() { const ATaGCharacter* Me = GetMe(PC); return Me ? T(Me->GetHeroDef().Name) : FText::GetEmpty(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock).Font(TaGUI::Font(13.f))
						.ColorAndOpacity_Lambda([PC]() { const ATaGCharacter* Me = GetMe(PC); return FSlateColor(Me ? TaGUI::RoleColor(Me->GetHeroDef().Role) : Dim); })
						.Text_Lambda([PC]()
						{
							const ATaGCharacter* Me = GetMe(PC);
							return Me ? T(FString::Printf(TEXT("%s・%s"), TaGHeroes::RoleName(Me->GetHeroDef().Role), TaGHeroes::SizeName(Me->GetHeroDef().Size))) : FText::GetEmpty();
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 3.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(232.f).HeightOverride(13.f)
						[
							SNew(STaGSegmentBar).Segment(25.f).Size(FVector2D(232.f, 13.f))
							.Value_Lambda([PC]() { const ATaGCharacter* Me = GetMe(PC); return Me ? Me->GetGuard() : 0.f; })
							.Max_Lambda([PC]() { const ATaGCharacter* Me = GetMe(PC); return Me ? Me->GetMaxGuard() : 100.f; })
							.Color_Lambda([PC]()
							{
								const ATaGCharacter* Me = GetMe(PC);
								return Me ? GuardColor(Me->GetGuard() / FMath::Max(Me->GetMaxGuard(), 1.f)) : TaG::AllyColor();
							})
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock).Font(TaGUI::Font(16.f))
						.Text_Lambda([PC]() { const ATaGCharacter* Me = GetMe(PC); return Me ? FText::AsNumber(FMath::RoundToInt(Me->GetGuard())) : FText::GetEmpty(); })
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(TaGUI::Font(14.f)).ColorAndOpacity(TaG::GoldColor())
					.Text_Lambda([PC]()
					{
						const ATaGCharacter* Me = GetMe(PC);
						if (!Me) { return FText::GetEmpty(); }
						switch (Me->GetAction())
						{
						case ETaGAction::Down: return T(TEXT("ダウン：味方が触れると復帰"));
						case ETaGAction::Gathered: return T(TEXT("ギャザー：2歩でパスかシュート"));
						case ETaGAction::Stagger: return T(TEXT("よろけ"));
						case ETaGAction::Leap: return T(TEXT("アリウープ：空中で受けて決める"));
						default: break;
						}
						if (Me->IsInZone()) { return T(TEXT("ゾーン：速く、次のシュートが入りやすい")); }
						if (Me->IsScoped()) { return T(TEXT("スコープ中")); }
						if (Me->IsCatchAndShoot()) { return T(TEXT("キャッチ＆シュート")); }
						return T(TEXT("ガード"));
					})
				]
			];
	}

	// ------------------------------------------------------------ The score bug and the teams

	TSharedRef<SWidget> TeamStrip(FPCPtr PC, bool bAlly)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		const FLinearColor TeamC = bAlly ? TaG::AllyColor() : TaG::EnemyColor();
		for (int32 i = 0; i < 3; ++i)
		{
			auto Who = [PC, bAlly, i]() -> ATaGCharacter*
			{
				const ATaGGameState* S = GetGS(PC);
				if (!S) { return nullptr; }
				const TArray<ATaGCharacter*> Team = TeamChars(PC, bAlly ? S->GetLocalTeam() : 1 - S->GetLocalTeam());
				return Team.IsValidIndex(i) ? Team[i] : nullptr;
			};
			Row->AddSlot().AutoWidth().Padding(2.f, 0.f)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([Who]() { return Who() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(White()).Padding(2.f)
					.BorderBackgroundColor_Lambda([Who, TeamC]() { const ATaGCharacter* C = Who(); return FSlateColor(C && C->HasBall() ? TaG::GoldColor() : TeamC); })
					[
						SNew(SBox).WidthOverride(36.f).HeightOverride(36.f)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.05f, 0.06f, 0.09f, 0.95f))
							]
							+ SOverlay::Slot()
							[
								SNew(SImage)
								.Image_Lambda([Who]() { const ATaGCharacter* C = Who(); const FSlateBrush* B = C ? TaGUI::PortraitBrush(C->GetHeroDef()) : nullptr; return B ? B : White(); })
								.ColorAndOpacity_Lambda([Who]()
								{
									const ATaGCharacter* C = Who();
									if (!C) { return FSlateColor(FLinearColor::Transparent); }
									const FLinearColor Base = TaGUI::PortraitBrush(C->GetHeroDef()) ? FLinearColor::White : C->GetHeroDef().PrimaryColor;
									return FSlateColor(C->IsDown() ? Base * 0.3f : Base);
								})
							]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(TaGUI::Font(24.f)).ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.3f))
								.Text_Lambda([Who]() { const ATaGCharacter* C = Who(); return C && C->IsDown() ? T(TEXT("×")) : FText::GetEmpty(); })
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(SBox).WidthOverride(40.f).HeightOverride(4.f)
					[
						SNew(STaGSegmentBar).Segment(1000.f).Size(FVector2D(40.f, 4.f))
						.Value_Lambda([Who]() { const ATaGCharacter* C = Who(); return C ? C->GetGuard() : 0.f; })
						.Max_Lambda([Who]() { const ATaGCharacter* C = Who(); return C ? C->GetMaxGuard() : 100.f; })
						.Color(TeamC)
					]
				]
			];
		}
		return Row;
	}

	TSharedRef<SWidget> ScoreBlock(FPCPtr PC, bool bAlly)
	{
		auto TeamOf = [PC, bAlly]() { const ATaGGameState* S = GetGS(PC); return S ? (bAlly ? S->GetLocalTeam() : 1 - S->GetLocalTeam()) : 0; };
		return SNew(SBox).WidthOverride(90.f).HeightOverride(56.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage).Image(White()).ColorAndOpacity(bAlly ? AllyDeep : EnemyDeep)
				]
				+ SOverlay::Slot().VAlign(VAlign_Top)
				[
					SNew(SBox).HeightOverride(3.f)[SNew(SImage).Image(White()).ColorAndOpacity(bAlly ? TaG::AllyColor() : TaG::EnemyColor())]
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Font(TaGUI::Font(36.f))
					.Text_Lambda([PC, TeamOf]() { const ATaGGameState* S = GetGS(PC); return S ? FText::AsNumber(S->GetScore(TeamOf())) : FText::GetEmpty(); })
				]
				// The team with the ball: a bright line along the bottom
				+ SOverlay::Slot().VAlign(VAlign_Bottom)
				[
					SNew(SBox).HeightOverride(3.f)
					[
						SNew(SImage).Image(White())
						.ColorAndOpacity_Lambda([PC, TeamOf]() { const ATaGGameState* S = GetGS(PC); return FSlateColor(S && S->PossessionTeam == TeamOf() ? TaG::GoldColor() : FLinearColor::Transparent); })
					]
				]
			];
	}

	TSharedRef<SWidget> ScoreBug(FPCPtr PC)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[
					TeamStrip(PC, true)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					ScoreBlock(PC, true)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(116.f).HeightOverride(56.f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SImage).Image(White()).ColorAndOpacity(Ink)
						]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SNew(STextBlock).Font(TaGUI::Font(28.f))
								.Text_Lambda([PC]()
								{
									const ATaGGameState* S = GetGS(PC);
									if (!S) { return FText::GetEmpty(); }
									// The clock stops with the match: the result instead of a frozen shot clock
									if (S->Phase == ETaGPhase::Results) { return T(TEXT("終了")); }
									return T(FString::Printf(TEXT("%.1f"), FMath::Max(0.f, S->ShotClock)));
								})
								.ColorAndOpacity_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return FSlateColor(S && S->Phase != ETaGPhase::Results && S->ShotClock < 4.f ? FLinearColor(1.f, 0.38f, 0.3f) : FLinearColor::White); })
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SNew(STextBlock).Font(TaGUI::Font(11.f, false)).ColorAndOpacity(Dim)
								.Text_Lambda([PC]()
								{
									const ATaGGameState* S = GetGS(PC);
									if (!S) { return FText::GetEmpty(); }
									if (S->Phase == ETaGPhase::Results) { return T(FString::Printf(TEXT("%d点先取"), TaG::TargetScore)); }
									return T(FString::Printf(TEXT("%s・%d点先取"), S->PossessionTeam == S->GetLocalTeam() ? TEXT("攻撃") : TEXT("守備"), TaG::TargetScore));
								})
							]
						]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					ScoreBlock(PC, false)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
				[
					TeamStrip(PC, false)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.3f, 0.22f, 0.02f, 0.9f)).Padding(FMargin(12.f, 3.f))
				.Visibility_Lambda([PC]()
				{
					const ATaGGameState* S = GetGS(PC);
					return S && S->NeedsClear(S->GetLocalTeam()) && S->PossessionTeam == S->GetLocalTeam() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				[
					SNew(STextBlock).Font(TaGUI::Font(14.f)).ColorAndOpacity(TaG::GoldColor()).Text(T(TEXT("アークの外へクリアしてから攻める")))
				]
			];
	}

	// ------------------------------------------------------------ The lineup at the start of a match

	TSharedRef<SWidget> LineupRow(FPCPtr PC, bool bAlly)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		const FLinearColor C = bAlly ? TaG::AllyColor() : TaG::EnemyColor();
		for (int32 i = 0; i < 3; ++i)
		{
			auto Who = [PC, bAlly, i]() -> ATaGCharacter*
			{
				const ATaGGameState* S = GetGS(PC);
				if (!S) { return nullptr; }
				const TArray<ATaGCharacter*> Team = TeamChars(PC, bAlly ? S->GetLocalTeam() : 1 - S->GetLocalTeam());
				return Team.IsValidIndex(i) ? Team[i] : nullptr;
			};
			Row->AddSlot().AutoWidth().Padding(6.f, 0.f)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([Who]() { return Who() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(White()).BorderBackgroundColor(C).Padding(3.f)
					[
						SNew(SBox).WidthOverride(96.f).HeightOverride(96.f)
						[
							SNew(SImage)
							.Image_Lambda([Who]() { const ATaGCharacter* Ch = Who(); const FSlateBrush* B = Ch ? TaGUI::PortraitBrush(Ch->GetHeroDef()) : nullptr; return B ? B : White(); })
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock).Font(TaGUI::Font(17.f))
					.Text_Lambda([Who]() { const ATaGCharacter* Ch = Who(); return Ch ? T(Ch->GetHeroDef().Name) : FText::GetEmpty(); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(TaGUI::Font(11.f, false)).ColorAndOpacity(Dim)
					.Text_Lambda([Who]()
					{
						const ATaGCharacter* Ch = Who();
						return Ch ? T(FString::Printf(TEXT("%s・%d cm"), TaGHeroes::RoleName(Ch->GetHeroDef().Role), FMath::RoundToInt(Ch->GetHeroDef().EyeHeight + 11.f))) : FText::GetEmpty();
					})
				]
			];
		}
		return Row;
	}

	TSharedRef<SWidget> Lineup(FPCPtr PC)
	{
		// Fades out over the last 0.4 s before the first possession goes live
		auto Fade = [PC]()
		{
			const ATaGGameState* S = GetGS(PC);
			return S ? FMath::Clamp((S->IntroSeconds - S->MatchTime) / 0.4f, 0.f, 1.f) : 0.f;
		};
		return SNew(SBorder).BorderImage(White()).Padding(FMargin(30.f, 18.f))
			.BorderBackgroundColor_Lambda([Fade]() { return FSlateColor(FLinearColor(0.01f, 0.015f, 0.03f, 0.8f * Fade())); })
			.ColorAndOpacity_Lambda([Fade]() { return FLinearColor(1.f, 1.f, 1.f, Fade()); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[LineupRow(PC, true)]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(26.f, 0.f)
				[
					SNew(STextBlock).Font(TaGUI::Font(44.f)).ColorAndOpacity(TaG::GoldColor()).Text(T(TEXT("VS")))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[LineupRow(PC, false)]
			];
	}

	// ------------------------------------------------------------ The event feed

	TSharedRef<SWidget> EventFeed(FPCPtr PC)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (int32 i = 0; i < 5; ++i)
		{
			auto Pop = [PC, i]() -> const FTaGPopup*
			{
				const ATaGGameState* S = GetGS(PC);
				return S && S->GetPopups().IsValidIndex(i) ? &S->GetPopups()[i] : nullptr;
			};
			auto Alpha = [Pop]() { const FTaGPopup* P = Pop(); return P ? FMath::Clamp(P->TimeLeft / 0.5f, 0.f, 1.f) : 0.f; };
			auto Col = [PC, Pop]()
			{
				const ATaGGameState* S = GetGS(PC);
				const FTaGPopup* P = Pop();
				return S && P && P->Team >= 0 ? S->TeamColorFor(P->Team) : TaG::GoldColor();
			};
			Box->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 2.f)
			[
				SNew(SBorder).BorderImage(White()).Padding(0.f)
				.Visibility_Lambda([Pop]() { return Pop() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.BorderBackgroundColor_Lambda([Alpha]() { return FSlateColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.78f * Alpha())); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(4.f)
						[
							SNew(SImage).Image(White()).ColorAndOpacity_Lambda([Col, Alpha]() { return FSlateColor(Col().CopyWithNewOpacity(Alpha())); })
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 4.f, 14.f, 4.f)
					[
						SNew(STextBlock).Font(TaGUI::Font(i == 0 ? 19.f : 16.f))
						.Text_Lambda([Pop]() { const FTaGPopup* P = Pop(); return P ? P->Text : FText::GetEmpty(); })
						.ColorAndOpacity_Lambda([Col, Alpha]() { return FSlateColor(FMath::Lerp(FLinearColor::White, Col(), 0.55f).CopyWithNewOpacity(Alpha())); })
					]
				]
			];
		}
		return Box;
	}

	/** Lessons keep their messages in the middle, under the lesson panel. */
	TSharedRef<SWidget> CentrePopups(FPCPtr PC)
	{
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (int32 i = 0; i < 4; ++i)
		{
			Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f)
			[
				SNew(STextBlock).Font(TaGUI::Font(i == 0 ? 28.f : 20.f))
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
		return Box;
	}

	// ------------------------------------------------------------ Lessons

	TSharedRef<SWidget> LessonPanel(FPCPtr PC)
	{
		return SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Ink).Padding(FMargin(24.f, 10.f))
			[
				SNew(SBox).WidthOverride(860.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(TaGUI::Font(24.f)).ColorAndOpacity(TaG::AllyColor())
							.Text_Lambda([PC]()
							{
								const ATaGGameState* S = GetGS(PC);
								if (!S) { return FText::GetEmpty(); }
								return T(FString::Printf(TEXT("レッスン %d / %d   %s"), S->TutorialStep + 1, TaGTutorial::NumLessons(), TaGTutorial::Get(S->TutorialStep).Title));
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(TaGUI::Font(22.f)).ColorAndOpacity(TaG::GoldColor())
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
						SNew(STextBlock).Font(TaGUI::Font(17.f)).AutoWrapText(true)
						.Text_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return S ? T(TaGTutorial::Get(S->TutorialStep).Text) : FText::GetEmpty(); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(STextBlock).Font(TaGUI::Font(13.f, false)).ColorAndOpacity(Dim)
						.Text(T(TEXT("ポーズ（Start / Esc）から、このレッスンをとばせる")))
					]
				]
			];
	}

	// ------------------------------------------------------------ The shot meter

	TSharedRef<SWidget> ShotMeter(FPCPtr PC)
	{
		constexpr float W = 440.f;
		auto At = [](float Charge) { return W * FMath::Clamp(Charge / TaG::ShotAutoRelease, 0.f, 1.f); };
		return SNew(SVerticalBox)
			.Visibility_Lambda([PC]() { const ATaGCharacter* Me = GetMe(PC); return Me && Me->IsChargingShot() && Me->GetShotType() != ETaGShotType::AlleyOop ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock).Font(TaGUI::Font(18.f))
				.Visibility_Lambda([PC]() { const ATaGCharacter* Me = GetMe(PC); return Me && Me->CanFakeShot() ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
				.Text_Lambda([PC]()
				{
					const ATaGCharacter* Me = GetMe(PC);
					const ATaGCharacter* O = Me ? Me->FindNearestOpponent(320.f) : nullptr;
					const bool bUp = O && (O->IsContesting() || O->IsContestStarting());
					return T(bUp ? TEXT("手が上がった！ LB で突破 / X でパス（フェイク）") : TEXT("跳ぶ前なら LB 突破 / X パスでフェイク"));
				})
				.ColorAndOpacity_Lambda([PC]()
				{
					const ATaGCharacter* Me = GetMe(PC);
					const ATaGCharacter* O = Me ? Me->FindNearestOpponent(320.f) : nullptr;
					const bool bUp = O && (O->IsContesting() || O->IsContestStarting());
					return FSlateColor(bUp ? FLinearColor(0.35f, 0.85f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 0.6f));
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
				[
					SNew(STextBlock).Font(TaGUI::Font(22.f))
					.Text_Lambda([PC]()
					{
						const ATaGCharacter* Me = GetMe(PC);
						if (!Me) { return FText::GetEmpty(); }
						const FTaGShotEval E = FTaGRules::EvaluateShot(Me, Me->GetShotCharge(), false);
						return T(FString::Printf(TEXT("%s・%d点"), FTaGRules::ShotType(E.Type).Name, E.Points));
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(14.f, 0.f, 0.f, 1.f)
				[
					SNew(STextBlock).Font(TaGUI::Font(20.f))
					.Text_Lambda([PC]()
					{
						const ATaGCharacter* Me = GetMe(PC);
						if (!Me) { return FText::GetEmpty(); }
						const FTaGShotEval E = FTaGRules::EvaluateShot(Me, Me->GetShotCharge(), false);
						// The bodies decide every contest but the arts': say how much taller the defender reaches
						FString Why;
						if (E.bWalled && !E.bContested) { Why = TEXT("  壁（技）"); }
						else if (E.bReached && E.bArtContest) { Why = TEXT("  技の妨害"); }
						else if (E.bReached && E.Contest < 0.99f)
						{
							const int32 Gap = FMath::RoundToInt(E.ReachGap);
							const FString Height = FMath::Abs(Gap) < 8 ? FString(TEXT("高さは互角"))
								: (Gap > 0 ? FString::Printf(TEXT("相手が%dcm高い"), Gap) : FString::Printf(TEXT("こちらが%dcm高い"), -Gap));
							Why = FString::Printf(TEXT("  %s・%s"), E.bContested ? TEXT("妨害") : TEXT("寄せ"), *Height);
						}
						else if (E.bOpen) { Why = TEXT("  ノーマーク"); }
						return T(FString::Printf(TEXT("%d%%%s"), FMath::RoundToInt(E.Chance * 100.f), *Why));
					})
					.ColorAndOpacity_Lambda([PC]()
					{
						const ATaGCharacter* Me = GetMe(PC);
						const FTaGShotEval E = Me ? FTaGRules::EvaluateShot(Me, Me->GetShotCharge(), false) : FTaGShotEval();
						return FSlateColor(E.bContested || E.bWalled ? FLinearColor(1.f, 0.38f, 0.32f) : (E.bOpen ? TaG::AllyColor() : FLinearColor::White));
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(W).HeightOverride(14.f)
				[
					SNew(SCanvas)
					+ SCanvas::Slot().Position(FVector2D(-3.f, -3.f)).Size(FVector2D(W + 6.f, 20.f))
					[
						SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
					]
					+ SCanvas::Slot().Position(FVector2D(At(TaG::ShotGoodLo), 0.f)).Size(FVector2D(At(TaG::ShotGoodHi) - At(TaG::ShotGoodLo), 14.f))
					[
						SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.12f, 0.7f, 0.42f, 0.9f))
					]
					+ SCanvas::Slot().Position(FVector2D(At(TaG::ShotPerfectLo), 0.f)).Size(FVector2D(At(TaG::ShotPerfectHi) - At(TaG::ShotPerfectLo), 14.f))
					[
						SNew(SImage).Image(White()).ColorAndOpacity(TaG::GoldColor())
					]
					+ SCanvas::Slot()
					.Position_Lambda([PC, At]() { const ATaGCharacter* Me = GetMe(PC); return FVector2D((Me ? At(Me->GetShotCharge()) : 0.f) - 2.f, -7.f); })
					.Size(FVector2D(4.f, 28.f))
					[
						SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor::White)
					]
				]
			];
	}
}

// ================================================================ Segment bar

void STaGSegmentBar::Construct(const FArguments& InArgs)
{
	Value = InArgs._Value;
	Max = InArgs._Max;
	Segment = FMath::Max(InArgs._Segment, 1.f);
	Color = InArgs._Color;
	Size = InArgs._Size;
}

int32 STaGSegmentBar::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const float MaxV = FMath::Max(Max.Get(), 1.f);
	const float V = FMath::Clamp(Value.Get(), 0.f, MaxV);
	const int32 N = FMath::Clamp(FMath::CeilToInt(MaxV / Segment), 1, 40);
	const FVector2f Area = AllottedGeometry.GetLocalSize();
	const float Gap = N > 1 ? 3.f : 0.f;
	const float W = (Area.X - Gap * (N - 1)) / N;
	const FSlateBrush* B = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FLinearColor Fill = Color.Get();
	for (int32 i = 0; i < N; ++i)
	{
		const float X = i * (W + Gap);
		const float Lo = i * Segment;
		const float Hi = FMath::Min((i + 1) * Segment, MaxV);
		const float F = FMath::Clamp((V - Lo) / FMath::Max(Hi - Lo, 0.01f), 0.f, 1.f);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(W, Area.Y), FSlateLayoutTransform(FVector2f(X, 0.f))),
			B, ESlateDrawEffect::None, FLinearColor(0.07f, 0.08f, 0.11f, 0.92f));
		if (F > 0.f)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(W * F, Area.Y), FSlateLayoutTransform(FVector2f(X, 0.f))),
				B, ESlateDrawEffect::None, Fill);
		}
	}
	return LayerId + 2;
}

// ================================================================ The HUD

TSharedRef<SWidget> TaGUI::MakeMatchHUD(TWeakObjectPtr<ATaGPlayerController> PC)
{
	auto Playing = [PC]()
	{
		const ATaGGameState* S = GetGS(PC);
		return S && (S->Phase == ETaGPhase::Playing || S->Phase == ETaGPhase::Results);
	};
	auto InTutorial = [PC]()
	{
		const ATaGGameState* S = GetGS(PC);
		return S && S->bTutorial && S->Phase == ETaGPhase::Playing;
	};
	auto Replaying = [PC]() { return PC.IsValid() && ATaGReplay::IsPlaying(PC->GetWorld()); };
	// The results screen and replays take the bottom of the screen
	auto Results = [PC]() { const ATaGGameState* S = GetGS(PC); return S && S->Phase == ETaGPhase::Results; };

	TSharedRef<SHorizontalBox> Tiles = SNew(SHorizontalBox);
	const TCHAR* Pad[6] = { TEXT("LB"), TEXT("RB"), TEXT("B"), TEXT("X"), TEXT("LT"), TEXT("Y") };
	const TCHAR* Pc[6] = { TEXT("Q"), TEXT("E"), TEXT("Shift"), TEXT("F"), TEXT("C"), TEXT("V") };
	for (int32 i = 0; i < 6; ++i)
	{
		// The two arts sit apart from the four hands
		Tiles->AddSlot().AutoWidth().Padding(i == 4 ? 18.f : 4.f, 0.f, 4.f, 0.f)
		[
			AbilityTile(PC, i, Pad[i], Pc[i])
		];
	}

	return SNew(SOverlay)
		.Visibility_Lambda([Playing]() { return Playing() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		// Names over heads
		+ SOverlay::Slot()
		[
			SNew(STaGWorldMarkers).PC(PC)
			.Visibility_Lambda([Replaying]() { return Replaying() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
		]
		// Flash (read wins, being read)
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
		// Lesson (instead of the score)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 14.f)
		[
			SNew(SBox)
			.Visibility_Lambda([PC, InTutorial]() { const ATaGGameState* S = GetGS(PC); return InTutorial() && S && !S->IsTutorialFinished() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				LessonPanel(PC)
			]
		]
		// The score bug
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 12.f)
		[
			SNew(SBox)
			.Visibility_Lambda([InTutorial, Replaying]() { return InTutorial() || Replaying() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				ScoreBug(PC)
			]
		]
		// Event feed (right) / lesson messages (centre)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.f, 96.f, 20.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([InTutorial, Replaying, Results]() { return InTutorial() || Replaying() || Results() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				EventFeed(PC)
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 200.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility_Lambda([InTutorial]() { return InTutorial() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				CentrePopups(PC)
			]
		]
		// The two lineups while the first possession is set up
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 60.f)
		[
			SNew(SBox)
			.Visibility_Lambda([PC, InTutorial]()
			{
				const ATaGGameState* S = GetGS(PC);
				return S && S->Phase == ETaGPhase::Playing && S->MatchTime < S->IntroSeconds && !InTutorial() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[
				Lineup(PC)
			]
		]
		// Read-win banner across the middle
		+ SOverlay::Slot().VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 250.f)
		[
			SNew(SBorder).BorderImage(White()).Padding(FMargin(0.f, 8.f))
			.Visibility_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return S && S->GetBanner().TimeLeft > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.BorderBackgroundColor_Lambda([PC]()
			{
				const ATaGGameState* S = GetGS(PC);
				const float A = S ? FMath::Clamp(S->GetBanner().TimeLeft / 0.3f, 0.f, 1.f) : 0.f;
				return FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.42f * A));
			})
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(TaGUI::Font(50.f))
					.Text_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return S ? S->GetBanner().Title : FText::GetEmpty(); })
					.ColorAndOpacity_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return FSlateColor(S && S->GetBanner().bGood ? FLinearColor(0.35f, 1.f, 0.95f) : FLinearColor(1.f, 0.45f, 0.4f)); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(TaGUI::Font(22.f))
					.Text_Lambda([PC]() { const ATaGGameState* S = GetGS(PC); return S ? S->GetBanner().Rule : FText::GetEmpty(); })
				]
			]
		]
		// Alley-oop: a lob on its way to you -> jump so the top of the leap meets it
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 230.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([PC]()
			{
				const ATaGCharacter* Me = GetMe(PC);
				const bool bShow = Me && Me->HasAlleyOop() && Me->GetAction() != ETaGAction::Leap && Me->GetAction() != ETaGAction::Shoot;
				return bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(TaGUI::Font(30.f))
				.Text_Lambda([PC]()
				{
					const ATaGCharacter* Me = GetMe(PC);
					const float In = Me ? Me->GetAlleyOopJumpIn() : 9.f;
					return T(In < 0.1f ? TEXT("今！ ジャンプ（A / Space）") : TEXT("アリウープ！ ジャンプで受ける（A / Space）"));
				})
				.ColorAndOpacity_Lambda([PC]()
				{
					const ATaGCharacter* Me = GetMe(PC);
					const float In = Me ? Me->GetAlleyOopJumpIn() : 9.f;
					return FSlateColor(In < 0.1f ? FLinearColor(1.f, 1.f, 1.f) : TaG::GoldColor());
				})
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f)
			[
				SNew(SBox).WidthOverride(320.f).HeightOverride(12.f)
				[
					SNew(SCanvas)
					+ SCanvas::Slot().Position(FVector2D(0.f, 0.f)).Size(FVector2D(320.f, 12.f))
					[
						SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f))
					]
					+ SCanvas::Slot().Position(FVector2D(0.f, 0.f))
					.Size_Lambda([PC]()
					{
						const ATaGCharacter* Me = GetMe(PC);
						const float In = Me ? Me->GetAlleyOopJumpIn() : 9.f;
						return FVector2D(320.f * FMath::Clamp(1.f - In / 0.9f, 0.f, 1.f), 12.f);
					})
					[
						SNew(SImage).Image(White()).ColorAndOpacity(TaG::GoldColor())
					]
				]
			]
		]
		// Crosshair
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(6.f).HeightOverride(6.f)
			.Visibility_Lambda([Replaying]() { return Replaying() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.8f))
			]
		]
		// The line: with the ball near the boundary (in first person the feet are out of sight)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 196.f)
		[
			SNew(SBorder).BorderImage(White()).BorderBackgroundColor(FLinearColor(0.45f, 0.08f, 0.04f, 0.88f)).Padding(FMargin(14.f, 4.f))
			.Visibility_Lambda([PC]()
			{
				const ATaGCharacter* Me = GetMe(PC);
				const bool bNear = Me && Me->HasBall() && Me->GetCharacterMovement()->IsMovingOnGround() && TaG::DistToBoundary(Me->GetFeetLocation()) < 70.f;
				return bNear ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			[
				SNew(STextBlock).Font(TaGUI::Font(17.f)).Text(T(TEXT("ライン際：踏むとアウト・オブ・バウンズ")))
			]
		]
		// Shot meter
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 118.f)
		[
			ShotMeter(PC)
		]
		// Your hero (bottom left)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(26.f, 0.f, 0.f, 24.f)
		[
			SNew(SBox)
			.Visibility_Lambda([Replaying, Results]() { return Replaying() || Results() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				HeroCard(PC)
			]
		]
		// Hands and arts (bottom right)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 22.f, 16.f)
		[
			SNew(SBox)
			.Visibility_Lambda([PC, Replaying, Results]() { return GetMe(PC) && !Replaying() && !Results() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				Tiles
			]
		]
		// Replay: letterbox and a label
		+ SOverlay::Slot().VAlign(VAlign_Top)
		[
			SNew(SBox).HeightOverride(64.f)
			.Visibility_Lambda([Replaying]() { return Replaying() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.92f))
			]
		]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)
		[
			SNew(SBox).HeightOverride(64.f)
			.Visibility_Lambda([Replaying]() { return Replaying() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.92f))
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(36.f, 14.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([Replaying]() { return Replaying() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(6.f).HeightOverride(30.f)[SNew(SImage).Image(White()).ColorAndOpacity(TaG::GoldColor())]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f)
			[
				SNew(STextBlock).Font(TaGUI::Font(26.f)).Text_Lambda([PC]() { return PC.IsValid() ? ATaGReplay::GetTitle(PC->GetWorld()) : FText::GetEmpty(); })
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(36.f, 20.f)
		[
			SNew(STextBlock).Font(TaGUI::Font(15.f, false)).ColorAndOpacity(Dim)
			.Visibility_Lambda([Replaying]() { return Replaying() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.Text(T(TEXT("A / Space：とばす")))
		];
}
