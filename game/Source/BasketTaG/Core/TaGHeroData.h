#pragma once

#include "CoreMinimal.h"
#include "TaGHeroData.generated.h"

UENUM(BlueprintType)
enum class ETaGHero : uint8
{
	Gantetsu,
	Minamo,
	Rei,
	Hayate,
	Tsumugi,
	Hibiki,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ETaGRole : uint8
{
	Wall,
	Scorer,
	Playmaker
};

UENUM(BlueprintType)
enum class ETaGSize : uint8
{
	S,
	M,
	L,
	XL
};

/** The rock-paper-scissors tag every ability carries (RULES_V1 section 3.5). */
UENUM(BlueprintType)
enum class ETaGHand : uint8
{
	Drive,
	Shot,
	Evade,
	Tackle,
	Contest,
	Spacing,
	Teamplay
};

/** Hero abilities ("arts"). */
UENUM(BlueprintType)
enum class ETaGArt : uint8
{
	None,
	ShoulderCharge, // Gantetsu  [Tackle]
	IronWall,       // Gantetsu  [Contest]
	Pull,           // Minamo    [Tackle, ranged, slow startup]
	SlowField,      // Minamo    [Spacing]
	Blink,          // Rei       [Drive]
	Afterimage,     // Rei       [Evade]
	Scope,          // Hayate    [Shot]
	SpotUp,         // Hayate    [Teamplay]
	ThreadPass,     // Tsumugi   [Teamplay]
	Tether,         // Tsumugi   [Teamplay]
	SonicWave,      // Hibiki    [Contest, area]
	Resync          // Hibiki    [Teamplay]
};

struct FTaGArtDef
{
	ETaGArt Id = ETaGArt::None;
	ETaGHand Hand = ETaGHand::Teamplay;
	const TCHAR* Name = TEXT("");
	const TCHAR* Desc = TEXT("");
	float Cooldown = 8.f;
};

/**
 * Everything that makes a hero a hero, as data (ROSTER.md section 5-4: define it in data, tune without rewriting code).
 * Body numbers follow the ROSTER.md size classes.
 */
struct FTaGHeroDef
{
	ETaGHero Id = ETaGHero::Rei;
	const TCHAR* Key = TEXT("Rei");      // asset folder / URL option
	const TCHAR* Name = TEXT("");        // display name (Japanese)
	const TCHAR* Kind = TEXT("");        // species
	const TCHAR* Blurb = TEXT("");       // one line
	ETaGRole Role = ETaGRole::Scorer;
	ETaGSize Size = ETaGSize::M;

	float EyeHeight = 175.f;
	float CapsuleRadius = 25.f;
	float CapsuleHalfHeight = 94.f;
	float Guard = 100.f;
	float SpeedMul = 1.f;
	float AccelMul = 1.f;
	float JumpMul = 1.f;
	float ReachMul = 1.f;       // block / steal / rebound reach
	float Weight = 1.f;         // pushes and staggers scale with weight ratios
	float BodyWidth = 50.f;

	ETaGArt Art1 = ETaGArt::None;
	ETaGArt Art2 = ETaGArt::None;

	FLinearColor PrimaryColor = FLinearColor::White;   // used by the fallback body when art is missing
	FLinearColor SecondaryColor = FLinearColor::Gray;
};

namespace TaGHeroes
{
	BASKETTAG_API const FTaGHeroDef& Get(ETaGHero Hero);
	BASKETTAG_API const FTaGArtDef& GetArt(ETaGArt Art);
	BASKETTAG_API ETaGHero FromKey(const FString& Key, ETaGHero Fallback = ETaGHero::Rei);
	BASKETTAG_API const TCHAR* RoleName(ETaGRole Role);
	BASKETTAG_API const TCHAR* SizeName(ETaGSize Size);
	BASKETTAG_API const TCHAR* HandName(ETaGHand Hand);
	BASKETTAG_API ETaGHero Random(const TArray<ETaGHero>& Avoid);
	constexpr int32 Num = static_cast<int32>(ETaGHero::Count);
}
