#include "Core/TaGHeroData.h"

namespace
{
	FLinearColor Hex(uint32 RGB)
	{
		return FLinearColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF));
	}

	/** One row of the roster: body numbers by size class and role, two arts, colours from tools/art/roster.json. */
	FTaGHeroDef Row(ETaGHero Id, const TCHAR* Key, const TCHAR* Name, const TCHAR* Kind, const TCHAR* Blurb, ETaGRole Role, ETaGSize Size,
		float Eye, float Width, float Guard, float Speed, float Accel, float Jump, float Reach, float Weight,
		ETaGArt Art1, ETaGArt Art2, uint32 Primary, uint32 Secondary)
	{
		FTaGHeroDef H;
		H.Id = Id;
		H.Key = Key;
		H.Name = Name;
		H.Kind = Kind;
		H.Blurb = Blurb;
		H.Role = Role;
		H.Size = Size;
		H.EyeHeight = Eye;
		H.BodyWidth = Width;
		H.CapsuleRadius = FMath::Min(Width * 0.5f, 52.f);
		H.CapsuleHalfHeight = FMath::RoundToFloat(Eye * 0.537f);
		H.Guard = Guard;
		H.SpeedMul = Speed;
		H.AccelMul = Accel;
		H.JumpMul = Jump;
		H.ReachMul = Reach;
		H.Weight = Weight;
		H.Art1 = Art1;
		H.Art2 = Art2;
		H.PrimaryColor = Hex(Primary);
		H.SecondaryColor = Hex(Secondary);
		return H;
	}

	TArray<FTaGHeroDef> MakeHeroes()
	{
		using R = ETaGRole;
		using S = ETaGSize;
		using A = ETaGArt;
		using E = ETaGHero;
		TArray<FTaGHeroDef> H;
		H.SetNum(TaGHeroes::Num);
		auto Add = [&H](const FTaGHeroDef& D) { H[static_cast<int32>(D.Id)] = D; };
		// ---- the first six (ROSTER.md section 3; XL jumps least so not every possession ends in a dunk)
		Add(Row(E::Gantetsu, TEXT("Gantetsu"), TEXT("ガンテツ"), TEXT("自我機（解体用の重機）"), TEXT("親方の名前を継いだ巨体。壁と体当たりでゴール下を守る"),
			R::Wall, S::XL, 250.f, 95.f, 170.f, 0.85f, 0.8f, 0.45f, 1.35f, 3.f, A::ShoulderCharge, A::IronWall, 0xD98C1F, 0x2E2F35));
		Add(Row(E::Minamo, TEXT("Minamo"), TEXT("ミナモ"), TEXT("義体者"), TEXT("水路の配達屋。義体の脚の水流で、相手を引き寄せて足を止める"),
			R::Wall, S::L, 210.f, 70.f, 135.f, 0.92f, 0.9f, 0.8f, 1.15f, 2.2f, A::Pull, A::SlowField, 0x2F7FC1, 0xE8EEF2));
		Add(Row(E::Rei, TEXT("Rei"), TEXT("レイ"), TEXT("不明"), TEXT("地図にない地区から来た正体不明の天才。誰とでも同期する"),
			R::Scorer, S::M, 175.f, 50.f, 100.f, 1.f, 1.f, 1.f, 1.f, 1.f, A::Blink, A::Afterimage, 0x1C1D26, 0x3EE6FF));
		Add(Row(E::Hayate, TEXT("Hayate"), TEXT("ハヤテ"), TEXT("人間"), TEXT("企業チームのエース。精密照準のTaGで、遠くから射抜く"),
			R::Scorer, S::M, 190.f, 50.f, 100.f, 0.98f, 1.f, 1.f, 1.08f, 1.05f, A::Scope, A::SpotUp, 0xF4F4F7, 0xD9A33A));
		Add(Row(E::Tsumugi, TEXT("Tsumugi"), TEXT("ツムギ"), TEXT("人間"), TEXT("織物工房の跡取り。導電糸でパスをつなぐ司令塔"),
			R::Playmaker, S::S, 145.f, 40.f, 70.f, 1.15f, 1.2f, 1.1f, 1.15f, 0.6f, A::ThreadPass, A::Tether, 0xB8324F, 0xF2CF4F));
		Add(Row(E::Hibiki, TEXT("Hibiki"), TEXT("ヒビキ"), TEXT("発電生物（発光クラゲの群体）"), TEXT("人の形をとったクラゲの群体。光と音で話すDJ"),
			R::Playmaker, S::M, 175.f, 50.f, 100.f, 1.f, 1.f, 1.f, 1.f, 0.9f, A::SonicWave, A::Resync, 0x8C5CF2, 0x4FF2F2));
		// ---- walls
		Add(Row(E::Kokemaru, TEXT("Kokemaru"), TEXT("コケマル"), TEXT("群体（菌糸と苔）"), TEXT("屋上農園で育った菌糸の巨人。根を張ると誰にも動かせない"),
			R::Wall, S::XL, 255.f, 100.f, 180.f, 0.82f, 0.78f, 0.4f, 1.35f, 3.2f, A::RootDown, A::SporeField, 0x4E8F3A, 0x9CF2C2));
		Add(Row(E::Keibi, TEXT("Keibi"), TEXT("ケイビ"), TEXT("自我機（引退した警備ロボ）"), TEXT("引退した警備ロボ。規則に厳しく情に弱い、守備の号令役"),
			R::Wall, S::L, 205.f, 75.f, 140.f, 0.92f, 0.9f, 0.75f, 1.15f, 2.f, A::Arrest, A::Rally, 0x23345C, 0xE23B3B));
		Add(Row(E::Yadori, TEXT("Yadori"), TEXT("ヤドリ"), TEXT("発電生物（廃船を背負ったヤドカリ）"), TEXT("廃船を背負ったヤドカリ。殻にこもると何も通さない"),
			R::Wall, S::L, 200.f, 80.f, 145.f, 0.9f, 0.88f, 0.7f, 1.1f, 2.2f, A::Shell, A::HullWall, 0xE07A3C, 0x4FB8A0));
		Add(Row(E::Iwakura, TEXT("Iwakura"), TEXT("イワクラ"), TEXT("人間（パワードスーツ）"), TEXT("上層企業の警備主任。大盾を構えて前を固める"),
			R::Wall, S::M, 178.f, 60.f, 125.f, 0.95f, 0.95f, 0.9f, 1.05f, 1.6f, A::TowerShield, A::ShieldBash, 0xDDE3EA, 0x3D6E9E));
		Add(Row(E::Kotetsu, TEXT("Kotetsu"), TEXT("コテツ"), TEXT("自我機（小型の溶接ロボ）"), TEXT("ガンテツの部品から生まれた小さな溶接ロボ。遠くに壁を立てる"),
			R::Wall, S::S, 140.f, 44.f, 95.f, 1.08f, 1.15f, 1.f, 0.95f, 1.1f, A::RemoteBarrier, A::SparkBolt, 0xE58A2A, 0x7FD8FF));
		Add(Row(E::Kawara, TEXT("Kawara"), TEXT("カワラ"), TEXT("自我機（瓦職人のロボ）"), TEXT("旧市街の瓦職人ロボ。瓦の壁と鎧で守る"),
			R::Wall, S::M, 176.f, 58.f, 125.f, 0.95f, 0.95f, 0.9f, 1.05f, 1.5f, A::TileWall, A::TileGuard, 0x4A5160, 0xE2553B));
		Add(Row(E::Aoji, TEXT("Aoji"), TEXT("アオジ"), TEXT("自我機（旧水門）"), TEXT("古い水門の自我機。水門を落とし、放水で押し流す"),
			R::Wall, S::XL, 250.f, 105.f, 185.f, 0.8f, 0.76f, 0.38f, 1.35f, 3.3f, A::GateSlam, A::Floodgate, 0x4F7A5A, 0x5FE0E6));
		Add(Row(E::Ankou, TEXT("Ankou"), TEXT("アンコウ"), TEXT("発電生物（チョウチンアンコウ）"), TEXT("水路の底のチョウチンアンコウ。灯りでボールを釣る"),
			R::Wall, S::L, 200.f, 85.f, 145.f, 0.9f, 0.88f, 0.7f, 1.15f, 2.3f, A::Lure, A::Gulp, 0x3A2D5C, 0xFFF06A));
		// ---- scorers
		Add(Row(E::Mugura, TEXT("Mugura"), TEXT("ムグラ"), TEXT("発電生物（デンキウナギ）"), TEXT("体長4mのデンキウナギ。放電を推進力に、泳ぐように跳ぶ"),
			R::Scorer, S::L, 205.f, 88.f, 125.f, 0.97f, 0.95f, 1.05f, 1.1f, 1.7f, A::EelLeap, A::Discharge, 0x2F4A2A, 0xE8E05A));
		Add(Row(E::Amagi, TEXT("Amagi"), TEXT("アマギ"), TEXT("自我機（企業の試作アンドロイド）"), TEXT("心が宿る前の最後の機械として作られた試作アンドロイド"),
			R::Scorer, S::M, 180.f, 48.f, 100.f, 1.f, 1.f, 1.f, 1.f, 1.f, A::SureShot, A::AutoDodge, 0xF5F3EE, 0x8FD3FF));
		Add(Row(E::Ushio, TEXT("Ushio"), TEXT("ウシオ"), TEXT("人間×発電生物の共生"), TEXT("鰻を背負う下層の漁師。2つの拍で続けて抜く"),
			R::Scorer, S::M, 178.f, 54.f, 105.f, 1.f, 1.02f, 1.f, 1.f, 1.15f, A::DoubleBeat, A::EelWhip, 0x24375C, 0xF2C12E));
		Add(Row(E::Tsurube, TEXT("Tsurube"), TEXT("ツルベ"), TEXT("自我機（元クレーン）"), TEXT("元クレーンの自我機。長い腕から高い弧のシュートを落とす"),
			R::Scorer, S::XL, 260.f, 85.f, 155.f, 0.82f, 0.78f, 0.4f, 1.5f, 2.6f, A::HighArc, A::BoomSwing, 0xF2C230, 0xF25C3D));
		Add(Row(E::Suzu, TEXT("Suzu"), TEXT("スズ"), TEXT("人間"), TEXT("ソコマチの鈴売り。鈴の音で相手を惑わせる"),
			R::Scorer, S::S, 140.f, 38.f, 70.f, 1.18f, 1.25f, 1.12f, 0.9f, 0.55f, A::BellDecoy, A::QuickShot, 0xD9333B, 0xF2C744));
		Add(Row(E::Nikoichi, TEXT("Nikoichi"), TEXT("ニコイチ"), TEXT("人間＋自我機（子どもとロボの2人組）"), TEXT("ロボと、その肩に乗る子どもの2人組。高い位置から打つ"),
			R::Scorer, S::L, 215.f, 72.f, 130.f, 0.94f, 0.92f, 0.85f, 1.15f, 1.9f, A::ShoulderShot, A::KidHop, 0x2FB5A5, 0xFFE04F));
		Add(Row(E::Hibana, TEXT("Hibana"), TEXT("ヒバナ"), TEXT("人間"), TEXT("テツバの溶接見習い。火花をまとって突っ込む"),
			R::Scorer, S::S, 148.f, 40.f, 72.f, 1.17f, 1.22f, 1.12f, 0.9f, 0.6f, A::SparkDash, A::SparkBlind, 0xF27C22, 0xFFD84A));
		Add(Row(E::Tsurara, TEXT("Tsurara"), TEXT("ツララ"), TEXT("義体者"), TEXT("冷却義体のスケーター。氷の上を滑って抜く"),
			R::Scorer, S::M, 176.f, 50.f, 98.f, 1.04f, 1.05f, 1.f, 1.f, 1.f, A::IceSkate, A::IceField, 0xE9F4FF, 0x4A6AF2));
		// ---- playmakers
		Add(Row(E::Tsubame, TEXT("Tsubame"), TEXT("ツバメ便"), TEXT("群体機（配送ドローン）"), TEXT("配送ドローンの群れ。相手をよけて曲がるパスを届ける"),
			R::Playmaker, S::S, 145.f, 40.f, 72.f, 1.16f, 1.2f, 1.1f, 0.95f, 0.55f, A::CurvePass, A::Scatter, 0x1F4E9E, 0xE2463B));
		Add(Row(E::Kazami, TEXT("Kazami"), TEXT("カザミ"), TEXT("群体機（気象観測機）"), TEXT("気象観測機の群れ。風を読み、味方に追い風を吹かせる"),
			R::Playmaker, S::M, 174.f, 50.f, 98.f, 1.02f, 1.02f, 1.f, 1.f, 0.95f, A::WindField, A::LobPass, 0x7CC6F2, 0xF2D16B));
		Add(Row(E::Hari, TEXT("Hari"), TEXT("ハリ"), TEXT("義体者"), TEXT("下層の鍼灸師。導電針で味方を治し、相手を縫い止める"),
			R::Playmaker, S::M, 176.f, 50.f, 100.f, 1.f, 1.f, 1.f, 1.f, 1.f, A::Heal, A::NeedleBind, 0x2E8C73, 0xC9A24A));
		Add(Row(E::Tomoshibi, TEXT("Tomoshibi"), TEXT("トモシビ"), TEXT("自我機（古い街灯）"), TEXT("心が宿った古い街灯。照らした相手はまぶしくて打てない"),
			R::Playmaker, S::L, 220.f, 66.f, 130.f, 0.92f, 0.9f, 0.8f, 1.2f, 1.8f, A::FlashLight, A::Lamplight, 0x2E4A3A, 0xF2B04A));
		Add(Row(E::Koyomi, TEXT("Koyomi"), TEXT("コヨミ"), TEXT("人間"), TEXT("企業のアナリスト。相手のパスの行き先を先に読む"),
			R::Playmaker, S::M, 172.f, 48.f, 95.f, 1.02f, 1.05f, 1.f, 1.f, 0.95f, A::Predict, A::DataShare, 0x263A66, 0x42E0C8));
		Add(Row(E::Yatai, TEXT("Yatai"), TEXT("ヤタイ"), TEXT("自我機（移動屋台）"), TEXT("歩く屋台。味方に差し入れを配る、動く補給所"),
			R::Playmaker, S::XL, 235.f, 110.f, 165.f, 0.84f, 0.8f, 0.4f, 1.3f, 2.8f, A::Supply, A::FeastField, 0xC43C2E, 0xFFD27A));
		Add(Row(E::Oboro, TEXT("Oboro"), TEXT("オボロ"), TEXT("不明"), TEXT("ウラカゼから来た2人目。霧になってすり抜ける"),
			R::Playmaker, S::M, 180.f, 50.f, 100.f, 1.02f, 1.05f, 1.f, 1.f, 0.85f, A::MistField, A::MistStep, 0x8A8FA3, 0xC9B8F2));
		Add(Row(E::Hotaru, TEXT("Hotaru"), TEXT("ホタル"), TEXT("群体（ホタルの群れ）"), TEXT("ホタルの群れ。味方に灯りを託し、群れで相手を囲む"),
			R::Playmaker, S::S, 138.f, 38.f, 65.f, 1.18f, 1.22f, 1.12f, 0.9f, 0.5f, A::Lantern, A::SwarmContest, 0x2E4A2E, 0xD9F25C));
		return H;
	}

	/** Builder for one art. Unset fields keep the struct defaults. */
	struct FArt
	{
		FTaGArtDef D;
		FArt(ETaGArt Id, ETaGHand Hand, const TCHAR* Name, float Cooldown, const TCHAR* Desc)
		{
			D.Id = Id;
			D.Hand = Hand;
			D.Name = Name;
			D.Cooldown = Cooldown;
			D.Desc = Desc;
		}
		FArt& K(ETaGArtKind V) { D.Kind = V; return *this; }
		FArt& F(ETaGArtFx V) { D.Fx = V; return *this; }
		FArt& Range(float V) { D.Range = V; return *this; }
		FArt& Radius(float V) { D.Radius = V; return *this; }
		FArt& Dur(float V) { D.Duration = V; return *this; }
		FArt& Pow(float V) { D.Power = V; return *this; }
		FArt& Startup(float V) { D.Startup = V; return *this; }
		FArt& Count(int32 V) { D.Count = V; return *this; }
		FArt& AI(ETaGArtAI V) { D.AI = V; return *this; }
		FArt& Need(uint8 V) { D.Need = V; return *this; }
		FArt& Col(uint32 RGB) { D.Color = Hex(RGB); return *this; }
	};

	TArray<FTaGArtDef> MakeArts()
	{
		using A = ETaGArt;
		using H = ETaGHand;
		using K = ETaGArtKind;
		using X = ETaGArtFx;
		using I = ETaGArtAI;
		namespace N = TaGArtNeed;
		TArray<FTaGArtDef> Out;
		Out.SetNum(TaGHeroes::NumArts);
		auto Add = [&Out](const FArt& B) { Out[static_cast<int32>(B.D.Id)] = B.D; };
		Out[0] = FTaGArtDef();

		// ---- the first twelve (hand-written in TaGCharacter.cpp)
		Add(FArt(A::ShoulderCharge, H::Tackle, TEXT("ショルダーチャージ"), 7.f, TEXT("広い範囲に体当たり。構えている相手も押し返す。突破中の相手は倒してボールを奪う")).Col(0xFF8A2A));
		Add(FArt(A::IronWall, H::Contest, TEXT("鉄の壁"), 10.f, TEXT("前方に4秒間の壁。壁の陰からのシュートを妨害し、横を通った味方は加速する")).Col(0xFFB347));
		Add(FArt(A::Pull, H::Tackle, TEXT("引き寄せ"), 8.f, TEXT("発生は遅いが、8m先の相手を水流で引き寄せる。突破中なら倒してボールをこぼす")).Col(0x3FA9F5));
		Add(FArt(A::SlowField, H::Spacing, TEXT("減速フィールド"), 10.f, TEXT("半径4mの水流。中の相手は遅くなり、突破とかわしも短くなる")).Col(0x3FD0D4));
		Add(FArt(A::Blink, H::Drive, TEXT("ブリンク"), 7.f, TEXT("4.5m先へ瞬間移動する突破。ボールを持っていればギャザーになる")).Col(0x3EE6FF));
		Add(FArt(A::Afterimage, H::Evade, TEXT("残像ステップ"), 8.f, TEXT("残像を残してかわす。タックルを空振りさせると長く止められる")).Col(0x9FF3FF));
		Add(FArt(A::Scope, H::Shot, TEXT("スコープ"), 9.f, TEXT("3秒間、5m以上のシュートが入りやすくなる。妨害には負ける")).Col(0xF2D16B));
		Add(FArt(A::SpotUp, H::Teamplay, TEXT("スポットアップ"), 8.f, TEXT("止まって待つと、パスを受けてすぐのシュートの溜めが短くなる")).Col(0xD9A33A));
		Add(FArt(A::ThreadPass, H::Teamplay, TEXT("糸パス"), 6.f, TEXT("糸の上を走る速いパス。糸の真上に立たないと奪えない")).Col(0xFFD95A));
		Add(FArt(A::Tether, H::Teamplay, TEXT("テザー"), 10.f, TEXT("近くの味方と糸でつながり、2人とも速くなる。2人の間のパスはすべて糸パス")).Col(0xF2CF4F));
		Add(FArt(A::SonicWave, H::Contest, TEXT("音波"), 9.f, TEXT("周囲4mのシュートを妨害し、突破を遅くする。突破には負ける")).Col(0xB18CFF));
		Add(FArt(A::Resync, H::Teamplay, TEXT("リシンク"), 12.f, TEXT("倒れた味方を遠くから起こす。いなければ、近くの味方の技をすぐ使えるようにする")).Col(0x4FF2F2));

		// ---- walls
		Add(FArt(A::RootDown, H::Contest, TEXT("根張り"), 11.f, TEXT("3秒間その場に根を張る。動けないが押されず、周り3.2mのシュートを妨害する"))
			.K(K::Root).Dur(3.f).Radius(320.f).AI(I::DefendPaint).Need(N::Free | N::NoBall).Col(0x9CF2C2));
		Add(FArt(A::SporeField, H::Spacing, TEXT("胞子の霧"), 10.f, TEXT("半径3.8mの胞子。中の相手はガードが削れ続け、回復しない"))
			.K(K::Field).F(X::FieldSpore).Radius(380.f).Dur(4.f).AI(I::DefendPaint).Need(N::Free).Col(0xB8F27A));
		Add(FArt(A::Arrest, H::Tackle, TEXT("確保"), 6.f, TEXT("タックル。当たった相手を1秒その場に止める。突破中なら倒してボールを奪う"))
			.K(K::Tackle).F(X::TackleArrest).Dur(1.f).AI(I::DefendHandler).Need(N::Free | N::NoBall).Col(0x3B7BE2));
		Add(FArt(A::Rally, H::Teamplay, TEXT("号令"), 12.f, TEXT("周り9mの味方が3.5秒速くなり、ガードが30回復する"))
			.K(K::TeamAura).F(X::Speed).Radius(900.f).Dur(3.5f).Pow(30.f).AI(I::SupportAllies).Need(N::Ally).Col(0xE23B3B));
		Add(FArt(A::Shell, H::Evade, TEXT("殻ごもり"), 8.f, TEXT("1.6秒殻にこもる。動けないが、タックル・スティール・引き寄せをすべて弾く"))
			.K(K::Shell).Dur(1.6f).AI(I::OffenseEvade).Need(N::Free).Col(0xF2E2C9));
		Add(FArt(A::HullWall, H::Contest, TEXT("廃船の盾"), 10.f, TEXT("背負った船体を前に置く。幅の広い壁で、陰からのシュートを妨害する"))
			.K(K::Wall).Range(230.f).Pow(1.35f).Dur(4.f).AI(I::DefendPaint).Need(N::Free).Col(0xE07A3C));
		Add(FArt(A::TowerShield, H::Contest, TEXT("大盾"), 9.f, TEXT("3秒間、大盾を前に構える。前からのパスとシュートを止める。構えている間は遅い"))
			.K(K::ShieldFollow).Range(95.f).Pow(0.55f).Dur(3.f).AI(I::DefendShooter).Need(N::Free | N::NoBall).Col(0xF2B33D));
		Add(FArt(A::ShieldBash, H::Tackle, TEXT("盾打ち"), 6.f, TEXT("広い盾で殴るタックル。届く幅が広く、構えた相手も大きく押し返す"))
			.K(K::Tackle).F(X::TackleBash).Pow(1.5f).AI(I::DefendHandler).Need(N::Free | N::NoBall).Col(0x8FC7FF));
		Add(FArt(A::RemoteBarrier, H::Contest, TEXT("遠隔溶接壁"), 10.f, TEXT("6m先に溶接の障壁を立てる。小さな体で、遠くの通り道をふさぐ"))
			.K(K::Wall).Range(600.f).Pow(0.9f).Dur(3.5f).AI(I::DefendRanged).Need(N::Free).Col(0x7FD8FF));
		Add(FArt(A::SparkBolt, H::Tackle, TEXT("溶接スパーク"), 7.f, TEXT("8m先まで飛ぶ火花。当たると0.6秒よろける。突破中なら倒す"))
			.K(K::Bolt).F(X::Stagger).Range(800.f).Startup(0.25f).Dur(0.6f).AI(I::DefendRanged).Need(N::Free | N::NoBall).Col(0x7FD8FF));
		Add(FArt(A::TileWall, H::Contest, TEXT("瓦屋根"), 10.f, TEXT("瓦の壁を3枚、前に弧を描いて立てる"))
			.K(K::Wall).Range(240.f).Pow(0.5f).Count(3).Dur(4.f).AI(I::DefendPaint).Need(N::Free).Col(0xE2553B));
		Add(FArt(A::TileGuard, H::Evade, TEXT("瓦鎧"), 8.f, TEXT("3秒以内に受けたタックルを瓦の鎧で弾く。相手はよろけ、自分は削れない"))
			.K(K::Counter).F(X::CounterTile).Dur(3.f).AI(I::OffenseEvade).Col(0xC9CCD3));
		Add(FArt(A::GateSlam, H::Tackle, TEXT("水門落とし"), 8.f, TEXT("前方2.8mに水門を落とす。突破中の相手は倒れ、ほかの相手はよろける"))
			.K(K::Cone).F(X::Stagger).Range(280.f).Radius(50.f).Startup(0.35f).Dur(0.6f).Pow(260.f).AI(I::DefendHandler).Need(N::Free | N::NoBall).Col(0x5FE0E6));
		Add(FArt(A::Floodgate, H::Spacing, TEXT("放水"), 10.f, TEXT("前に一直線の放水。当たった相手を押し流し、2秒遅くする"))
			.K(K::Cone).F(X::Push).Range(650.f).Radius(16.f).Startup(0.3f).Dur(2.f).Pow(380.f).AI(I::DefendRanged).Need(N::Free).Col(0x5FC8F2));
		Add(FArt(A::Lure, H::Tackle, TEXT("誘い灯"), 9.f, TEXT("灯りの釣り糸を8m先まで伸ばす。ボールを持つ相手に当たると、ボールが手元へ飛んでくる（かわし・シュート中には効かない）"))
			.K(K::Bolt).F(X::BallSteal).Range(800.f).Startup(0.4f).AI(I::DefendRanged).Need(N::Free | N::NoBall).Col(0xFFF06A));
		Add(FArt(A::Gulp, H::Contest, TEXT("大口"), 8.f, TEXT("1.5秒間、大口を開けて構える。前3mのシュートを強く妨害する"))
			.K(K::ContestStance).Range(300.f).Dur(1.5f).Pow(0.6f).AI(I::DefendShooter).Need(N::Free | N::NoBall).Col(0x6FD9C6));

		// ---- scorers
		Add(FArt(A::EelLeap, H::Drive, TEXT("放電跳躍"), 8.f, TEXT("放電の勢いで6m跳ぶ突破。相手の頭上を越え、着地でギャザー"))
			.K(K::Leap).Range(600.f).AI(I::OffenseDrive).Need(N::Free).Col(0x6FF2FF));
		Add(FArt(A::Discharge, H::Contest, TEXT("放電"), 9.f, TEXT("周り3.3mに放電。相手はよろけてガードが削れ、シュート中なら当たられながら打つことになる"))
			.K(K::Aura).F(X::GuardDamage).Radius(330.f).Startup(0.25f).Dur(0.45f).Pow(20.f).AI(I::DefendShooter).Need(N::Free | N::NoBall).Col(0xE8E05A));
		Add(FArt(A::SureShot, H::Shot, TEXT("演算射撃"), 9.f, TEXT("4秒以内の次のシュートは、タイミングの許容が広く、最高の放ち方に近づく"))
			.K(K::ShotBuff).F(X::ShotSure).Dur(4.f).AI(I::OffenseShoot).Need(N::Ball).Col(0x8FD3FF));
		Add(FArt(A::AutoDodge, H::Evade, TEXT("予測回避"), 9.f, TEXT("3秒以内に来たタックルを自動でかわす（読み勝ち）"))
			.K(K::Counter).F(X::CounterSlip).Dur(3.f).AI(I::OffenseEvade).Col(0xBFE6FF));
		Add(FArt(A::DoubleBeat, H::Drive, TEXT("二拍子"), 8.f, TEXT("突破のあと、もう一度すぐ突破できる（2拍の連続突破）"))
			.K(K::Dash).F(X::DashDouble).Pow(1.05f).Dur(1.f).AI(I::OffenseDrive).Need(N::Free).Col(0x59D97A));
		Add(FArt(A::EelWhip, H::Tackle, TEXT("鰻むち"), 8.f, TEXT("4.5m先まで鰻が飛びつく。ボールを持つ相手に当たるとボールをはたき落とす（かわしには負ける）"))
			.K(K::Bolt).F(X::BallSteal).Range(450.f).Startup(0.2f).AI(I::DefendHandler).Need(N::Free | N::NoBall).Col(0x59D97A));
		Add(FArt(A::HighArc, H::Shot, TEXT("高弧"), 8.f, TEXT("次のシュートは高い弧になり、ブロックされず、妨害も半分しか効かない"))
			.K(K::ShotBuff).F(X::ShotHigh).Dur(4.f).AI(I::OffenseShoot).Need(N::Ball).Col(0xF2C230));
		Add(FArt(A::BoomSwing, H::Spacing, TEXT("ブーム旋回"), 9.f, TEXT("腕を振り回して、周り3.3mの相手を押しのける。シュートの間合いを作る"))
			.K(K::Aura).F(X::Push).Radius(330.f).Startup(0.3f).Dur(0.5f).Pow(320.f).AI(I::OffenseSpace).Need(N::Free).Col(0xF25C3D));
		Add(FArt(A::BellDecoy, H::Evade, TEXT("鈴の囮"), 8.f, TEXT("鈴の残像でかわす。周りの相手は鈴の音に惑わされ、2.5秒間、技の出だしが遅くなる"))
			.K(K::Decoy).Radius(450.f).Dur(2.5f).AI(I::OffenseEvade).Need(N::Free).Col(0xF2C744));
		Add(FArt(A::QuickShot, H::Shot, TEXT("早撃ち"), 7.f, TEXT("次のシュートの溜めが速く、妨害も効きにくい"))
			.K(K::ShotBuff).F(X::ShotQuick).Dur(4.f).AI(I::OffenseShoot).Need(N::Ball).Col(0xFF6B6B));
		Add(FArt(A::ShoulderShot, H::Shot, TEXT("肩車シュート"), 9.f, TEXT("次のシュートは肩の上の子どもが高い位置から打つ。ブロックされにくく、ノーマークならさらに入る"))
			.K(K::ShotBuff).F(X::ShotShoulder).Dur(4.f).AI(I::OffenseShoot).Need(N::Ball).Col(0xFFE04F));
		Add(FArt(A::KidHop, H::Drive, TEXT("子ども発進"), 7.f, TEXT("子どもが飛び出して3.5m先へ。短いブリンクの突破"))
			.K(K::Dash).F(X::DashHop).Range(350.f).AI(I::OffenseDrive).Need(N::Free).Col(0xFF8A3D));
		Add(FArt(A::SparkDash, H::Drive, TEXT("火花突進"), 6.f, TEXT("火花をまとった突破。すれ違った相手はよろける"))
			.K(K::Dash).F(X::DashSpark).Pow(1.1f).Dur(1.f).AI(I::OffenseDrive).Need(N::Free).Col(0xFFD84A));
		Add(FArt(A::SparkBlind, H::Spacing, TEXT("火花散らし"), 9.f, TEXT("周り3.5mに火花を散らす。相手は2.5秒、スティールとブロックの手が届きにくい"))
			.K(K::Aura).F(X::Blind).Radius(350.f).Startup(0.15f).Dur(2.5f).AI(I::OffenseSpace).Need(N::Free).Col(0xFFB13B));
		Add(FArt(A::IceSkate, H::Drive, TEXT("氷滑走"), 7.f, TEXT("長く速い滑走の突破。通った跡は凍り、相手が滑る"))
			.K(K::Dash).F(X::DashSkate).Pow(1.15f).Dur(1.6f).AI(I::OffenseDrive).Need(N::Free).Col(0x9FD8FF));
		Add(FArt(A::IceField, H::Spacing, TEXT("凍て床"), 10.f, TEXT("半径4mを凍らせる。中の相手は滑って遅くなり、突破とかわしも鈍る"))
			.K(K::Field).F(X::FieldIce).Radius(400.f).Dur(4.f).AI(I::DefendPaint).Need(N::Free).Col(0xB8E6FF));

		// ---- playmakers
		Add(FArt(A::CurvePass, H::Teamplay, TEXT("曲がる配達"), 6.f, TEXT("相手をよけて曲がるパス。奪われず、受けた味方はすぐ打てる"))
			.K(K::Pass).F(X::PassCurve).AI(I::OffensePass).Need(N::Ball | N::Ally).Col(0x5A8CFF));
		Add(FArt(A::Scatter, H::Evade, TEXT("散開"), 8.f, TEXT("群れがばらけて1.2秒すり抜ける。タックルもスティールも当たらない"))
			.K(K::Phase).Dur(1.2f).Pow(1.15f).AI(I::OffenseEvade).Need(N::Free).Col(0xC9DAFF));
		Add(FArt(A::WindField, H::Spacing, TEXT("追い風"), 10.f, TEXT("半径4.5mの追い風。中の味方は速くなり、遠くからのシュートが入りやすい"))
			.K(K::Field).F(X::FieldWind).Radius(450.f).Dur(4.f).AI(I::SupportAllies).Need(N::Free).Col(0xBFE9FF));
		Add(FArt(A::LobPass, H::Teamplay, TEXT("風のロブ"), 6.f, TEXT("風に乗せた高いロブパス。誰にも奪われない"))
			.K(K::Pass).F(X::PassLob).AI(I::OffensePass).Need(N::Ball | N::Ally).Col(0xF2D16B));
		Add(FArt(A::Heal, H::Teamplay, TEXT("鍼治療"), 12.f, TEXT("周り7mの味方のガードを全回復し、減速を消し、倒れた味方を起こす"))
			.K(K::TeamAura).F(X::Heal).Radius(700.f).AI(I::SupportAllies).Need(N::Ally).Col(0x5FE0B0));
		Add(FArt(A::NeedleBind, H::Tackle, TEXT("影縫い"), 8.f, TEXT("8m先まで針を飛ばす。当たった相手は1秒その場に縫い止められる。突破中なら倒す"))
			.K(K::Bolt).F(X::Root).Range(800.f).Startup(0.3f).Dur(1.f).AI(I::DefendRanged).Need(N::Free | N::NoBall).Col(0xC9A24A));
		Add(FArt(A::FlashLight, H::Contest, TEXT("照射"), 9.f, TEXT("前方7mを照らす。照らされた相手は3秒間まぶしく、シュートが入りにくい"))
			.K(K::Cone).F(X::ShotDebuff).Range(700.f).Radius(30.f).Startup(0.2f).Dur(3.f).AI(I::DefendRanged).Need(N::Free).Col(0xFFE9A8));
		Add(FArt(A::Lamplight, H::Teamplay, TEXT("灯りの輪"), 12.f, TEXT("周り7mの味方の手（突破・タックルなど）のクールダウンを0にする"))
			.K(K::TeamAura).F(X::Reset).Radius(700.f).AI(I::SupportAllies).Need(N::Ally).Col(0xF2B04A));
		Add(FArt(A::Predict, H::Spacing, TEXT("予報"), 10.f, TEXT("4秒間、味方全員のパスカットとスティールが届きやすくなる"))
			.K(K::TeamAura).F(X::Intercept).Radius(3000.f).Dur(4.f).AI(I::DefendRanged).Col(0x42E0C8));
		Add(FArt(A::DataShare, H::Teamplay, TEXT("データ共有"), 11.f, TEXT("5秒間、パスを受けた味方のボールが1段階強く光り（チェイン+1）、すぐ打てる"))
			.K(K::TeamAura).F(X::Mark).Radius(3000.f).Dur(5.f).Pow(1.f).AI(I::OffBall).Need(N::Ally).Col(0x7FF2E0));
		Add(FArt(A::Supply, H::Teamplay, TEXT("差し入れ"), 9.f, TEXT("近くの味方1人に差し入れ。手のクールダウンを0にし、ガードを40回復する"))
			.K(K::TeamOne).F(X::Reset).Range(900.f).Pow(40.f).AI(I::SupportAllies).Need(N::Ally).Col(0xFFD27A));
		Add(FArt(A::FeastField, H::Spacing, TEXT("屋台の灯"), 12.f, TEXT("半径4.5mの屋台の灯り。中の味方はガードがどんどん回復し、少し速くなる"))
			.K(K::Field).F(X::FieldFeast).Radius(450.f).Dur(5.f).AI(I::SupportAllies).Need(N::Free).Col(0xFFB25A));
		Add(FArt(A::MistField, H::Spacing, TEXT("霧の帳"), 11.f, TEXT("半径4.5mの霧。中の相手のシュート妨害は半分しか効かず、霧を通るパスは奪われない"))
			.K(K::Field).F(X::FieldMist).Radius(450.f).Dur(4.5f).AI(I::OffBall).Need(N::Free).Col(0xC9B8F2));
		Add(FArt(A::MistStep, H::Evade, TEXT("朧渡り"), 8.f, TEXT("1秒間、霧になってすり抜ける。タックルもスティールも当たらない"))
			.K(K::Phase).Dur(1.f).Pow(1.3f).AI(I::OffenseEvade).Need(N::Free).Col(0xE0D6FF));
		Add(FArt(A::Lantern, H::Teamplay, TEXT("蛍火"), 8.f, TEXT("味方1人に蛍を付ける。その味方が次にパスを受けると、ボールが2段階光り（チェイン+2）、すぐ打てる"))
			.K(K::TeamOne).F(X::Mark).Range(1500.f).Dur(6.f).Pow(2.f).AI(I::OffBall).Need(N::Ally).Col(0xD9F25C));
		Add(FArt(A::SwarmContest, H::Contest, TEXT("群れ光"), 8.f, TEXT("7.5m先のボールを持つ相手に蛍の群れを送る。1.6秒、その周りのシュートを妨害する"))
			.K(K::ContestAt).Range(750.f).Radius(230.f).Dur(1.6f).AI(I::DefendRanged).Need(N::NoBall).Col(0xEFFF8A));
		return Out;
	}

	struct FTables
	{
		TArray<FTaGHeroDef> Heroes;
		TArray<FTaGArtDef> Arts;
		FTables()
		{
			Heroes = MakeHeroes();
			Arts = MakeArts();
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
