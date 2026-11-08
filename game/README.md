# TaG — UE5版 v1（普通のバスケ＋アビリティじゃんけん、3on3まで）

「読み勝って、ぶち抜け。」——アビリティのじゃんけんで戦う、一人称の3on3バスケ。
Unreal Engine 5.8.4、C++、Gameplay Ability System（GAS）、Enhanced Input。発売時の核（[docs/STRATEGY.md](../docs/STRATEGY.md) §4）を、ひととおり遊べる形にした版。

## 入っているもの

| 区分 | 中身 | 設計の根拠 |
|---|---|---|
| 試合 | 1on1／2on2／3on3（FIBA 3x3のハーフコート、アーク内1点・外2点、11点先取、ショットクロック12秒、攻守が替わったらアークの外へクリア） | RULES_V1 §5、§6 |
| じゃんけん | 攻撃：突破・シュート・かわし／守備：タックル・シュート妨害・間合い。発生0.15秒前後、外したときの隙0.4〜0.6秒（Godot版v0.4の値） | RULES_V1 §3 |
| 読み勝ち | ヒットストップ→スロー、画面のフラッシュ、「読み勝ち！」の表示、勝った側は2秒間「ゾーン」（速くなり、次のシュートが入りやすい） | Godot版v0.2 |
| シュート | 7種類（ジャンプショット、ステップバック、フェイダウェイ、レイアップ、フローター、ユーロステップ、ダンク）。スティックの向き・ジャンプ・突破の組み合わせで決まる | Godot版v0.3 |
| 連携 | パス（インターセプトあり）、受けてすぐのシュート、**パスをつなぐほどコアボールが光り、シュートが少し入りやすくなる** | RULES_V1 §2.4、WORLD §3 |
| ギャザー制 | ボールを持ったまま突破すると、ドリブルが終わる。2歩（0.6秒）以内にパスかシュート | ABILITY_DESIGN §4 |
| ダウン | タックルで倒れると1.2秒動けない。味方が触れると即復帰（リシンク）。ガードが尽きてもダウン | WORLD §3 |
| ヒーロー | 6体（ウォール・スコアラー・プレイメーカー各2体）。体格S〜XLで、視点の高さ・当たり判定・速さ・ジャンプ・リーチ・重さ・ガードが変わる。固有技が2つずつ | ROSTER、RULES_V1 §4 |
| BOT | 試合ごとの「クセ」、弱いBOTは技の前に構えが見える。チームでは、空いたスポットへの移動、カット、空いた味方へのパス、マンツーマンとヘルプ、リバウンド、固有技 | Godot版ai_brain.gd |
| 友達と遊ぶ | プライベートロビー：ホストになる／IPアドレスで参加。足りない枠はBOTが埋める。途中で抜けた人はBOTが引き継ぐ | STRATEGY §3.3 |
| 世界 | 積層都市ナギサ・テツバ地区の屋上コート、夕方。**点を取ったチームの地区に明かりが灯る**（11点で全部） | WORLD §1、§6 |
| 画面 | タイトル、ヒーロー選択（ロビー）、試合中の表示（スコア、ショットクロック、シュートメーターと成功率、手と技のクールダウン、頭上の名前とパス先）、ポーズ、結果（攻撃1回あたりの得点を「1対1」と「連携」に分けて表示） | |

### ヒーロー

| ヒーロー | ロール・体格 | 技1（LT / C） | 技2（Y / V） |
|---|---|---|---|
| ガンテツ | ウォール・XL | ショルダーチャージ〔タックル〕構えている相手も押し返す | 鉄の壁〔シュート妨害〕4秒間の壁。陰からのシュートを妨害し、横を通った味方は加速 |
| ミナモ | ウォール・L | 引き寄せ〔タックル・遠距離〕発生は遅いが8m先まで届く | 減速フィールド〔間合い〕半径4m、中の相手は遅くなる |
| レイ | スコアラー・M | ブリンク〔突破〕4.5m先へ瞬間移動 | 残像ステップ〔かわし〕タックルを空振りさせると長く止める |
| ハヤテ | スコアラー・M（長身） | スコープ〔シュート〕3秒間、5m以上が入りやすい | スポットアップ〔連携〕止まって受けると溜めが短い |
| ツムギ | プレイメーカー・S | 糸パス〔連携〕糸の真上に立たないと奪えない速いパス | テザー〔連携〕近くの味方とつながり、2人とも速くなる |
| ヒビキ | プレイメーカー・M | 音波〔シュート妨害・範囲〕周囲4mのシュートを妨害（突破には負ける） | リシンク〔連携〕倒れた味方を遠くから起こす |

## 操作

| 操作 | PC | コントローラー |
|---|---|---|
| 移動 / 視点 | WASD / マウス | 左スティック / 右スティック |
| ジャンプ・ブロック | Space | A |
| シュート（押して溜め、跳んだ頂点で離す） | 左クリック | RT |
| 攻撃：突破 / かわし | Q / E | LB / RB |
| 守備：タックル / シュート妨害（長押し） | Q / E | LB / RB |
| 間合い（ステップ） | Shift | B |
| パス（攻撃） / スティール（守備） | F・右クリック | X |
| ヒーローの技1 / 技2 | C / V | LT / Y |
| カメラ（一人称 / 三人称） | T | View |
| ポーズ | Esc | Start |

## 見た目と音
- 選手は、関節ごとの硬いパーツを手続き的に動かしている（Godot版の `rig.gd` を移植）。**Blenderで作ったパーツ（`/Game/Art/Heroes/<ヒーロー>/SM_<ヒーロー>_<部位>`）があればそれを使い、なければエンジンの基本図形で組む。**
- コートと街（`/Game/Art/Court`、`/Game/Art/City`）、効果音（`/Game/Audio/SFX`）も同じで、無い場合はエンジンの図形と無音で動く。
- 素材の作り方とUEへの取り込み方は [tools/README.md](../tools/README.md) にある。

## ビルドと起動
エンジンは `C:\Epic Games\UE_5.8` を前提にしている。**遊ぶだけなら `game/Play.bat` をダブルクリック**（エディタ用のビルドが済んでいる必要がある）。
Windows用のパッケージ版（`build/win/Windows/BasketTaG.exe`）も作れるが、署名がないため、Smart App Controlなどのアプリ制御が有効なPCでは起動を止められる。
```bash
"C:/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" BasketTaGEditor Win64 Development -Project="C:/TaG/game/BasketTaG.uproject" -WaitMutex
"C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/TaG/game/BasketTaG.uproject" -game -windowed -ResX=1600 -ResY=900
```

### Android APK
```bash
"C:/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun -project="C:/TaG/game/BasketTaG.uproject" -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -pak -package -archive -archivedirectory="C:/TaG/build/ue" -nop4 -utf8output -unattended
```
- Android SDK（`%LOCALAPPDATA%\Android\Sdk`、NDK r27c）とJDK 21が必要。環境変数は `ANDROID_HOME`、`NDKROOT`、`JAVA_HOME`
- 今はDevelopmentビルドで、デバッグ署名。配布用の署名鍵はまだない

## 友達と遊ぶ（同じLAN、またはポート開放済みの回線）
1. ホスト：タイトルで「友達と遊ぶ：ホストになる」。ロビーの右上に、友達に伝えるアドレスが出る
2. 友達：タイトルのアドレス欄に入力して「参加」
3. ホストがモードとBOTの強さを決めて「試合開始」。各自がヒーローを選ぶ

- 使うポートはUDP 7777。インターネット越しに遊ぶには、ホスト側のルーターでポートの転送が必要（Steamなどのマッチングはまだない）

## 検証用のモード
```bash
UE="C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
P="C:/TaG/game/BasketTaG.uproject"
"$UE" "$P" -game -nullrhi -unattended -TaGShotTest -stdout                        # 計画上「成功」のシュートが物理で入るか（体格ごとの打点×距離×方向）
"$UE" "$P" -game -nullrhi -unattended -TaGAutoTest=3 -benchmark -fps=60 -stdout   # BOT同士の3on3（=1、=2で1on1・2on2）。STATS と PLAYER を出力
"$UE" "$P" -game -nullrhi -unattended -TaGInputTest -TaGHero=Hayate -stdout       # 人間の入力経路（Enhanced Input→GAS→選手）に入力を流し込んで確かめる
"$UE" "$P" "/Engine/Maps/Entry?listen?Lobby=1?TeamSize=2" -game -nullrhi -TaGNetHost   # ネット対戦のホスト（クライアントが来たら開始）
"$UE" "$P" 127.0.0.1 -game -nullrhi -TaGNetClient                                  # 上のホストに参加して、見えている状態を記録
"C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "$P" -game -windowed -TaGSnap=Play -TaGHero=Gantetsu   # Title / Lobby / Play / TPS / Defense / Results のスクリーンショット
```

### 最新の結果（2026-11-08）
- **shottest**：入るはずのシュートが、打点4種（体格S〜XL）×0.8〜9m×5方向×ジャンプの有無のすべてで入った（400/400）
- **inputtest**：人間の入力から、突破、シュート（溜め→離す）、技、ポーズ、カメラ切替が動いた
- **ネット対戦**：同じPCでホストとクライアントを起動し、クライアントが接続して試合の状態（フェーズ、選手4人、自分のヒーローとチーム、ショットクロック）を受け取れた
- **autotest**（BOT同士、強さ「強い」、素材を入れた最終版で1on1×2、2on2×2、3on3×3）：攻撃1回あたりの得点（1点／2点方式）は、1on1で0.46〜0.79、2on2で0.29〜0.79、3on3で0.30〜0.85。3on3の3試合を合計すると、**連携した攻撃（パスあり）が約0.61、1対1が約0.58で、連携がわずかに上**。RULES_V1 §2.4の「連携のほうが得」は、向きは合っているが差はまだ小さい。1試合ごとのばらつきも大きい
- **画面**：タイトル、ヒーロー選択、試合（一人称・三人称・守備）、結果を、Blender製の素材を入れた状態でスクリーンショットで確認した
- **Android**：素材込みのAPK（164MB、arm64）を作れた。実機ではまだ動かしていない

## ファイル構成
| ファイル | 中身 |
|---|---|
| `Core/TaGTypes.h` | コートの寸法、テンポ、じゃんけんのタイミング、ルール、色 |
| `Core/TaGHeroData.*` | ヒーロー6体と技12種のデータ（体格・速さ・リーチ・重さ・技） |
| `Core/TaGRules.*` | シュートの種類と成功率（距離・タイミング・守備・ゾーン・チェイン・固有技） |
| `Core/TaGGameMode.*` | ロビー、チーム分け、試合の進行、じゃんけんの判定、計測、検証用のモード |
| `Core/TaGGameState.*`、`TaGPlayerState.*` | 全員に同期する状態（フェーズ、スコア、チームの計測値）、個人の選択と成績 |
| `Core/TaGStage.*` | 屋上コートと街。得点に応じて地区の窓が灯る |
| `Core/TaGAudio.*`、`TaGArtLib.*` | 効果音と、Blender製の素材の読み込み |
| `Ball/TaGBallSim.*`、`TaGBall.*` | ボールの物理（自前の放物線）、パス、インターセプト、ネット同期 |
| `Character/TaGCharacter.*` | 選手：行動の状態機械（じゃんけんの手、シュート、パス、固有技）、ダウンと復帰、見た目 |
| `Character/TaGRig.*` | 関節モデルと手続き的なアニメーション |
| `Character/TaGArtActors.*` | 鉄の壁、フィールド、光の線、残像 |
| `Character/TaGMovementComponent.*` | 行動ごとの速さと加速（クライアント予測と整合） |
| `Character/TaGBotController.*` | BOT |
| `Character/TaGPlayerController.*` | 入力、メニュー操作、ロビーのRPC、検証用の入力 |
| `Abilities/*` | 入力チャンネルごとのGameplay Ability（予測あり）、GameplayTag |
| `UI/*` | Slateの画面（タイトル、ロビー、HUD、ポーズ、結果） |

## まだのこと
- 人が実際に遊んで手触りを確かめる（ロックオン、視点の感度、技のタイミング）
- インターネット越しのマッチング（Steamなど）、配布用の署名とShippingビルド
- 5on5、追加ヒーロー、モーションキャプチャ等の本格的なアニメーション（[ROSTER.md](../docs/ROSTER.md) の拡張の棚）
