#include "Core/TaGHeroData.h"

namespace
{
	FTaGHeroDef MakeHeroes(ETaGHero Id)
	{
		FTaGHeroDef H;
		H.Id = Id;
		switch (Id)
		{
		case ETaGHero::Gantetsu:
			H.Key = TEXT("Gantetsu");
			H.Name = TEXT("ガンテツ");
			H.Kind = TEXT("自我機（解体用の重機）");
			H.Blurb = TEXT("親方の名前を継いだ巨体。壁と体当たりでゴール下を守る");
			H.Role = ETaGRole::Wall;
			H.Size = ETaGSize::XL;
			H.EyeHeight = 250.f;
			H.CapsuleRadius = 47.f;
			H.CapsuleHalfHeight = 135.f;
			H.Guard = 170.f;
			H.SpeedMul = 0.85f;
			H.AccelMul = 0.8f;
			H.JumpMul = 0.45f;   // XL: almost no jump, so not every possession ends in a dunk
			H.ReachMul = 1.35f;
			H.Weight = 3.f;
			H.BodyWidth = 95.f;
			H.Art1 = ETaGArt::ShoulderCharge;
			H.Art2 = ETaGArt::IronWall;
			H.PrimaryColor = FLinearColor(0.85f, 0.55f, 0.12f);
			H.SecondaryColor = FLinearColor(0.2f, 0.2f, 0.22f);
			break;
		case ETaGHero::Minamo:
			H.Key = TEXT("Minamo");
			H.Name = TEXT("ミナモ");
			H.Kind = TEXT("義体者");
			H.Blurb = TEXT("水路の配達屋。義体の脚の水流で、相手を引き寄せて足を止める");
			H.Role = ETaGRole::Wall;
			H.Size = ETaGSize::L;
			H.EyeHeight = 210.f;
			H.CapsuleRadius = 35.f;
			H.CapsuleHalfHeight = 113.f;
			H.Guard = 135.f;
			H.SpeedMul = 0.92f;
			H.AccelMul = 0.9f;
			H.JumpMul = 0.8f;
			H.ReachMul = 1.15f;
			H.Weight = 2.2f;   // heavy mechanical legs
			H.BodyWidth = 70.f;
			H.Art1 = ETaGArt::Pull;
			H.Art2 = ETaGArt::SlowField;
			H.PrimaryColor = FLinearColor(0.15f, 0.45f, 0.75f);
			H.SecondaryColor = FLinearColor(0.75f, 0.78f, 0.82f);
			break;
		case ETaGHero::Rei:
			H.Key = TEXT("Rei");
			H.Name = TEXT("レイ");
			H.Kind = TEXT("不明");
			H.Blurb = TEXT("地図にない地区から来た正体不明の天才。誰とでも同期する");
			H.Role = ETaGRole::Scorer;
			H.Size = ETaGSize::M;
			H.EyeHeight = 175.f;
			H.CapsuleRadius = 25.f;
			H.CapsuleHalfHeight = 94.f;
			H.Guard = 100.f;
			H.SpeedMul = 1.f;
			H.JumpMul = 1.f;
			H.ReachMul = 1.f;
			H.Weight = 1.f;
			H.BodyWidth = 50.f;
			H.Art1 = ETaGArt::Blink;
			H.Art2 = ETaGArt::Afterimage;
			H.PrimaryColor = FLinearColor(0.12f, 0.12f, 0.16f);
			H.SecondaryColor = FLinearColor(0.2f, 0.9f, 1.f);
			break;
		case ETaGHero::Hayate:
			H.Key = TEXT("Hayate");
			H.Name = TEXT("ハヤテ");
			H.Kind = TEXT("人間");
			H.Blurb = TEXT("企業チームのエース。精密照準のTaGで、遠くから射抜く");
			H.Role = ETaGRole::Scorer;
			H.Size = ETaGSize::M;
			H.EyeHeight = 190.f;   // tall: high release, harder to block
			H.CapsuleRadius = 25.f;
			H.CapsuleHalfHeight = 102.f;
			H.Guard = 100.f;
			H.SpeedMul = 0.98f;
			H.JumpMul = 1.f;
			H.ReachMul = 1.08f;
			H.Weight = 1.05f;
			H.BodyWidth = 50.f;
			H.Art1 = ETaGArt::Scope;
			H.Art2 = ETaGArt::SpotUp;
			H.PrimaryColor = FLinearColor(0.92f, 0.92f, 0.95f);
			H.SecondaryColor = FLinearColor(0.85f, 0.65f, 0.2f);
			break;
		case ETaGHero::Tsumugi:
			H.Key = TEXT("Tsumugi");
			H.Name = TEXT("ツムギ");
			H.Kind = TEXT("人間");
			H.Blurb = TEXT("織物工房の跡取り。導電糸でパスをつなぐ司令塔");
			H.Role = ETaGRole::Playmaker;
			H.Size = ETaGSize::S;
			H.EyeHeight = 145.f;
			H.CapsuleRadius = 20.f;
			H.CapsuleHalfHeight = 79.f;
			H.Guard = 70.f;
			H.SpeedMul = 1.15f;
			H.AccelMul = 1.2f;
			H.JumpMul = 1.1f;
			H.ReachMul = 1.15f;   // the threads give her L-class reach (ROSTER.md section 3)
			H.Weight = 0.6f;
			H.BodyWidth = 40.f;
			H.Art1 = ETaGArt::ThreadPass;
			H.Art2 = ETaGArt::Tether;
			H.PrimaryColor = FLinearColor(0.75f, 0.2f, 0.35f);
			H.SecondaryColor = FLinearColor(0.95f, 0.8f, 0.3f);
			break;
		case ETaGHero::Hibiki:
		default:
			H.Key = TEXT("Hibiki");
			H.Name = TEXT("ヒビキ");
			H.Kind = TEXT("発電生物（発光クラゲの群体）");
			H.Blurb = TEXT("人の形をとったクラゲの群体。光と音で話すDJ");
			H.Role = ETaGRole::Playmaker;
			H.Size = ETaGSize::M;
			H.EyeHeight = 175.f;
			H.CapsuleRadius = 25.f;
			H.CapsuleHalfHeight = 94.f;
			H.Guard = 100.f;
			H.SpeedMul = 1.f;
			H.JumpMul = 1.f;
			H.ReachMul = 1.f;
			H.Weight = 0.9f;
			H.BodyWidth = 50.f;
			H.Art1 = ETaGArt::SonicWave;
			H.Art2 = ETaGArt::Resync;
			H.PrimaryColor = FLinearColor(0.55f, 0.35f, 0.95f);
			H.SecondaryColor = FLinearColor(0.3f, 0.95f, 0.95f);
			break;
		}
		return H;
	}

	FTaGArtDef MakeArt(ETaGArt Id)
	{
		FTaGArtDef A;
		A.Id = Id;
		switch (Id)
		{
		case ETaGArt::ShoulderCharge:
			A = { Id, ETaGHand::Tackle, TEXT("ショルダーチャージ"), TEXT("広い範囲に体当たり。構えている相手も押し返す。突破中の相手は倒してボールを奪う"), 7.f };
			break;
		case ETaGArt::IronWall:
			A = { Id, ETaGHand::Contest, TEXT("鉄の壁"), TEXT("前方に4秒間の壁。壁の陰からのシュートを妨害し、横を通った味方は加速する"), 10.f };
			break;
		case ETaGArt::Pull:
			A = { Id, ETaGHand::Tackle, TEXT("引き寄せ"), TEXT("発生は遅いが、8m先の相手を水流で引き寄せる。突破中なら倒してボールをこぼす"), 8.f };
			break;
		case ETaGArt::SlowField:
			A = { Id, ETaGHand::Spacing, TEXT("減速フィールド"), TEXT("半径4mの水流。中の相手は遅くなり、突破とかわしも短くなる"), 10.f };
			break;
		case ETaGArt::Blink:
			A = { Id, ETaGHand::Drive, TEXT("ブリンク"), TEXT("4.5m先へ瞬間移動する突破。ボールを持っていればギャザーになる"), 7.f };
			break;
		case ETaGArt::Afterimage:
			A = { Id, ETaGHand::Evade, TEXT("残像ステップ"), TEXT("残像を残してかわす。タックルを空振りさせると長く止められる"), 8.f };
			break;
		case ETaGArt::Scope:
			A = { Id, ETaGHand::Shot, TEXT("スコープ"), TEXT("3秒間、5m以上のシュートが入りやすくなる。妨害には負ける"), 9.f };
			break;
		case ETaGArt::SpotUp:
			A = { Id, ETaGHand::Teamplay, TEXT("スポットアップ"), TEXT("止まって待つと、パスを受けてすぐのシュートの溜めが短くなる"), 8.f };
			break;
		case ETaGArt::ThreadPass:
			A = { Id, ETaGHand::Teamplay, TEXT("糸パス"), TEXT("糸の上を走る速いパス。糸の真上に立たないと奪えない"), 6.f };
			break;
		case ETaGArt::Tether:
			A = { Id, ETaGHand::Teamplay, TEXT("テザー"), TEXT("近くの味方と糸でつながり、2人とも速くなる。2人の間のパスはすべて糸パス"), 10.f };
			break;
		case ETaGArt::SonicWave:
			A = { Id, ETaGHand::Contest, TEXT("音波"), TEXT("周囲4mのシュートを妨害し、突破を遅くする。突破には負ける"), 9.f };
			break;
		case ETaGArt::Resync:
			A = { Id, ETaGHand::Teamplay, TEXT("リシンク"), TEXT("倒れた味方を遠くから起こす。いなければ、近くの味方の技をすぐ使えるようにする"), 12.f };
			break;
		default:
			break;
		}
		return A;
	}

	struct FTables
	{
		FTaGHeroDef Heroes[TaGHeroes::Num];
		TArray<FTaGArtDef> Arts;
		FTables()
		{
			for (int32 i = 0; i < TaGHeroes::Num; ++i)
			{
				Heroes[i] = MakeHeroes(static_cast<ETaGHero>(i));
			}
			for (uint8 i = 0; i <= static_cast<uint8>(ETaGArt::Resync); ++i)
			{
				Arts.Add(MakeArt(static_cast<ETaGArt>(i)));
			}
		}
	};

	const FTables& Tables()
	{
		static FTables T;
		return T;
	}
}

namespace TaGHeroes
{
	const FTaGHeroDef& Get(ETaGHero Hero)
	{
		const int32 i = FMath::Clamp(static_cast<int32>(Hero), 0, Num - 1);
		return Tables().Heroes[i];
	}

	const FTaGArtDef& GetArt(ETaGArt Art)
	{
		const int32 i = FMath::Clamp(static_cast<int32>(Art), 0, Tables().Arts.Num() - 1);
		return Tables().Arts[i];
	}

	ETaGHero FromKey(const FString& Key, ETaGHero Fallback)
	{
		for (int32 i = 0; i < Num; ++i)
		{
			if (Key.Equals(Get(static_cast<ETaGHero>(i)).Key, ESearchCase::IgnoreCase))
			{
				return static_cast<ETaGHero>(i);
			}
		}
		return Fallback;
	}

	const TCHAR* RoleName(ETaGRole Role)
	{
		switch (Role)
		{
		case ETaGRole::Wall: return TEXT("ウォール");
		case ETaGRole::Scorer: return TEXT("スコアラー");
		default: return TEXT("プレイメーカー");
		}
	}

	const TCHAR* SizeName(ETaGSize Size)
	{
		switch (Size)
		{
		case ETaGSize::S: return TEXT("S");
		case ETaGSize::M: return TEXT("M");
		case ETaGSize::L: return TEXT("L");
		default: return TEXT("XL");
		}
	}

	const TCHAR* HandName(ETaGHand Hand)
	{
		switch (Hand)
		{
		case ETaGHand::Drive: return TEXT("突破");
		case ETaGHand::Shot: return TEXT("シュート");
		case ETaGHand::Evade: return TEXT("かわし");
		case ETaGHand::Tackle: return TEXT("タックル");
		case ETaGHand::Contest: return TEXT("シュート妨害");
		case ETaGHand::Spacing: return TEXT("間合い");
		default: return TEXT("連携");
		}
	}

	ETaGHero Random(const TArray<ETaGHero>& Avoid)
	{
		TArray<ETaGHero> Pool;
		for (int32 i = 0; i < Num; ++i)
		{
			if (!Avoid.Contains(static_cast<ETaGHero>(i)))
			{
				Pool.Add(static_cast<ETaGHero>(i));
			}
		}
		if (Pool.Num() == 0)
		{
			return static_cast<ETaGHero>(FMath::RandRange(0, Num - 1));
		}
		return Pool[FMath::RandRange(0, Pool.Num() - 1)];
	}
}
