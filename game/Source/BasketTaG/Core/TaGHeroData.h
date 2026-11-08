#pragma once

#include "CoreMinimal.h"
#include "TaGHeroData.generated.h"

/** The 30 heroes (ROSTER.md section 4; art spec in tools/art/roster.json). The first six keep their old indices. */
UENUM(BlueprintType)
enum class ETaGHero : uint8
{
	Gantetsu,
	Minamo,
	Rei,
	Hayate,
	Tsumugi,
	Hibiki,
	Kokemaru,
	Keibi,
	Yadori,
	Iwakura,
	Kotetsu,
	Mugura,
	Amagi,
	Ushio,
	Tsurube,
	Suzu,
	Nikoichi,
	Tsubame,
	Kazami,
	Hari,
	Tomoshibi,
	Koyomi,
	Yatai,
	Oboro,
	Hibana,
	Tsurara,
	Kawara,
	Aoji,
	Ankou,
	Hotaru,
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

/** Hero abilities ("arts"). Two per hero; the first twelve are hand-written, the rest run on ETaGArtKind. */
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
	Resync,         // Hibiki    [Teamplay]
	RootDown,       // Kokemaru  [Contest]
	SporeField,     // Kokemaru  [Spacing]
	Arrest,         // Keibi     [Tackle]
	Rally,          // Keibi     [Teamplay]
	Shell,          // Yadori    [Evade]
	HullWall,       // Yadori    [Contest]
	TowerShield,    // Iwakura   [Contest]
	ShieldBash,     // Iwakura   [Tackle]
	RemoteBarrier,  // Kotetsu   [Contest]
	SparkBolt,      // Kotetsu   [Tackle]
	EelLeap,        // Mugura    [Drive]
	Discharge,      // Mugura    [Contest]
	SureShot,       // Amagi     [Shot]
	AutoDodge,      // Amagi     [Evade]
	DoubleBeat,     // Ushio     [Drive]
	EelWhip,        // Ushio     [Tackle]
	HighArc,        // Tsurube   [Shot]
	BoomSwing,      // Tsurube   [Spacing]
	BellDecoy,      // Suzu      [Evade]
	QuickShot,      // Suzu      [Shot]
	ShoulderShot,   // Nikoichi  [Shot]
	KidHop,         // Nikoichi  [Drive]
	CurvePass,      // Tsubame   [Teamplay]
	Scatter,        // Tsubame   [Evade]
	WindField,      // Kazami    [Spacing]
	LobPass,        // Kazami    [Teamplay]
	Heal,           // Hari      [Teamplay]
	NeedleBind,     // Hari      [Tackle]
	FlashLight,     // Tomoshibi [Contest]
	Lamplight,      // Tomoshibi [Teamplay]
	Predict,        // Koyomi    [Spacing]
	DataShare,      // Koyomi    [Teamplay]
	Supply,         // Yatai     [Teamplay]
	FeastField,     // Yatai     [Spacing]
	MistField,      // Oboro     [Spacing]
	MistStep,       // Oboro     [Evade]
	SparkDash,      // Hibana    [Drive]
	SparkBlind,     // Hibana    [Spacing]
	IceSkate,       // Tsurara   [Drive]
	IceField,       // Tsurara   [Spacing]
	TileWall,       // Kawara    [Contest]
	TileGuard,      // Kawara    [Evade]
	GateSlam,       // Aoji      [Tackle]
	Floodgate,      // Aoji      [Spacing]
	Lure,           // Ankou     [Tackle]
	Gulp,           // Ankou     [Contest]
	Lantern,        // Hotaru    [Teamplay]
	SwarmContest,   // Hotaru    [Contest]
	Count UMETA(Hidden)
};

/** What an art does mechanically (the generic arts are data on top of these). */
enum class ETaGArtKind : uint8
{
	Custom,         // the first twelve arts: their own code
	Dash,           // a stronger drive (Power = speed, Duration = length; Fx picks the flavour)
	Leap,           // airborne drive over defenders (Range = distance)
	Shell,          // invulnerable and rooted for Duration
	Phase,          // intangible for Duration: steals and tackles miss (Power = speed)
	Counter,        // the next tackle within Duration is answered (Fx picks how)
	Decoy,          // afterimage; opponents within Radius start their hands slower for Duration
	ShotBuff,       // the next shot within Duration is modified (Fx)
	Wall,           // Count walls Range ahead (Power = width scale) for Duration
	ShieldFollow,   // a shield wall carried Range ahead for Duration (Power = width)
	Root,           // rooted, unmovable, contests shots within Radius for Duration
	Tackle,         // a tackle with an extra effect on hit (Fx)
	Cone,           // after Startup: Fx on opponents within Range and Radius degrees of the facing
	Aura,           // after Startup: Fx on opponents within Radius
	Bolt,           // after Startup: Fx on the first opponent along the aim within Range
	Field,          // a field of Radius on the floor for Duration (Fx = field kind)
	TeamAura,       // Fx on teammates within Radius
	TeamOne,        // Fx on the nearest teammate within Range
	Pass,           // a pass that cannot be intercepted (Fx = curve or lob)
	ContestStance,  // a long contest (Duration) with its own reach (Range) and strength (Power)
	ContestAt       // a contest bubble (Radius) around the opponent ball handler within Range
};

/** The effect an art applies (targets, flavours and field kinds). */
enum class ETaGArtFx : uint8
{
	None,
	Stagger,        // Duration seconds; down if the target was driving (read win)
	Root,           // can't move for Duration; down if driving
	Push,           // pushed Power cm; slowed for Duration
	GuardDamage,    // Power guard damage and a short stagger
	BallSteal,      // the ball pops out toward the user (not through a dodge or a shot)
	Blind,          // steal / block / contest reach halved for Duration
	ShotDebuff,     // shots are harder for Duration (lit up)
	Heal,           // full guard, slows removed, downed teammates revived
	Reset,          // hand cooldowns reset (+ Power guard)
	Speed,          // faster for Duration (+ Power guard)
	Intercept,      // interception and steal reach doubled for Duration
	Mark,           // the next catch within Duration adds Power to the chain and opens catch-and-shoot
	FieldSpore,
	FieldWind,
	FieldFeast,
	FieldMist,
	FieldIce,
	ShotSure,       // wide timing window
	ShotHigh,       // unblockable, contests halved
	ShotQuick,      // fast meter, contests weaker
	ShotShoulder,   // high release: hard to block, better when open
	DashSpark,      // defenders passed by stagger
	DashSkate,      // long, fast, leaves ice
	DashDouble,     // the drive is ready again right after
	DashHop,        // a short blink
	CounterSlip,    // auto-dodge (evade read)
	CounterTile,    // armour: the tackler is staggered, no damage taken
	PassCurve,      // bends around defenders; the receiver can shoot at once
	PassLob,        // high over everyone
	TackleArrest,   // roots the tackled player
	TackleBash      // wide reach, big push
};

/** When bots use a generic art. */
enum class ETaGArtAI : uint8
{
	Never,
	OffenseDrive,    // with the ball, a defender in the way
	OffenseEvade,    // with the ball, a defender close or starting a tackle
	OffenseShoot,    // with the ball, about to shoot
	OffensePass,     // with the ball, a teammate open
	OffenseSpace,    // with the ball, a defender close: make room
	DefendHandler,   // guarding the ball handler up close
	DefendRanged,    // the ball handler 3-8 m in front
	DefendShooter,   // the ball handler is shooting (or about to) in range
	DefendPaint,     // the ball handler near the rim
	SupportAllies,   // teammates close by (team buffs)
	OffBall          // a teammate has the ball
};

namespace TaGArtNeed
{
	constexpr uint8 Ball = 1;     // must hold the ball
	constexpr uint8 NoBall = 2;   // must not hold the ball
	constexpr uint8 Free = 4;     // no other action running
	constexpr uint8 Ally = 8;     // a teammate exists
}

struct FTaGArtDef
{
	ETaGArt Id = ETaGArt::None;
	ETaGHand Hand = ETaGHand::Teamplay;
	const TCHAR* Name = TEXT("");
	const TCHAR* Desc = TEXT("");
	float Cooldown = 8.f;
	ETaGArtKind Kind = ETaGArtKind::Custom;
	ETaGArtFx Fx = ETaGArtFx::None;
	float Range = 0.f;
	float Radius = 0.f;
	float Duration = 0.f;
	float Power = 1.f;
	float Startup = 0.f;
	int32 Count = 1;
	ETaGArtAI AI = ETaGArtAI::Never;
	uint8 Need = 0;
	FLinearColor Color = FLinearColor(0.8f, 0.5f, 1.f);
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

	FLinearColor PrimaryColor = FLinearColor::White;   // fallback body, UI cards and art effects
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
	constexpr int32 NumArts = static_cast<int32>(ETaGArt::Count);
}
