# TaG — UE5版 v1.2（普通のバスケ＋アビリティじゃんけん、3on3まで）

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
| ヒーロー | **30体**（ウォール・スコアラー・プレイメーカー各10体）。体格S〜XLで、視点の高さ・当たり判定・速さ・ジャンプ・リーチ・重さ・ガードが変わる。固有技が2つずつで**計60種**。どの技も6つの手のどれかとして三すくみに入る。BOTも全員の技を使う | ROSTER §4・§5、RULES_V1 §4 |
| 動き | **1歩ごとに足が床に着いて止まる歩き**（足を床に固定するIK）。進む速さから歩幅と歩数を決めるので、足が滑らない。前・後ろ・横・斜めで歩き方が変わる（後ろは歩幅が狭く、横はサイドステップ）。走ると接地が短く、足が高く上がる。着地で沈み、止まると足をそろえる | |
| 見た目 | ファンタジーのトゥーン調。30体すべてを距離関数の造形（Blenderのスクリプト）で作り、輪郭線を付けた。夕焼けの空のドーム、雲、浮島、月、鳥居、提灯、のぼり。光は素材の側で計算する（シーンにライトを置かない）ので、PCとAndroidで同じ色になる | tools/README.md |
| エフェクト | 自前の軽いパーティクル（インスタンス描画）。技ごとの色と形（突進の残光、場の粒、光線、盾）、ヒーローごとのまとう粒（火花、蛍、冷気、胞子など）、足音の土ぼこり、読み勝ち・ブロック・スティール・得点の花火、ゾーンの金の粒。足元の影（キャラとボール） | |
| BOT | 試合ごとの「クセ」、弱いBOTは技の前に構えが見える。チームでは、空いたスポットへの移動、カット、空いた味方へのパス、マンツーマンとヘルプ、リバウンド、固有技 | Godot版ai_brain.gd |
| 友達と遊ぶ | プライベートロビー：ホストになる／IPアドレスで参加。足りない枠はBOTが埋める。途中で抜けた人はBOTが引き継ぐ | STRATEGY §3.3 |
| 世界 | 積層都市ナギサ・テツバ地区の屋上コート、夕方。**点を取ったチームの地区に明かりが灯る**（11点で全部） | WORLD §1、§6 |
| 画面 | タイトル、ヒーロー選択（ロビー）、試合中の表示（スコア、ショットクロック、シュートメーターと成功率、手と技のクールダウン、頭上の名前とパス先）、ポーズ、設定、結果（攻撃1回あたりの得点を「1対1」と「連携」に分けて表示） | |
| チュートリアル | 1on1で9レッスン：移動、シュート、パーフェクト・リリース、突破、読み合い4つ（突破＞シュート妨害、かわし＞タックル、タックル＞突破、シュート妨害＞シュート）、技。BOTが色つきの予兆（赤＝妨害、オレンジ＝タックル、水色＝突破、金＝シュート）を見せてから動くので、それに答えて覚える。できなかったときはヒントを出してやり直す。初回はタイトルでいちばん上に出る | RULES_V1 §3、Godot版v0.2（弱いBOTの予兆） |
| 演出 | BGM2曲（メニュー用の夕方の湾、試合用のテツバの工業ビート。numpyで合成）、読み勝ち・読まれた・ダウン・自チームの得点で振動とカメラの揺れ、シュートとパスの光の尾（パスをつなぐほど金色に） | WORLD §7 |
| 設定 | 視点の感度、右スティックの上下（既定はリバース）、マウスの上下、視野角、全体とBGMの音量、振動。端末ごとに保存 | |

### ヒーロー
30体の一覧（種類・体格・技2つ）と、60種の技の効果は [docs/ROSTER.md](../docs/ROSTER.md) の §4・§5 にある（ゲームのデータ `Core/TaGHeroData.cpp` から作った表）。
技1は LT / C、技2は Y / V。ロビーではロールごとに並んだ顔の絵から選び、選ぶと技の説明が出る。

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
| メニューで戻る | Esc | B |

- **右スティックの上下は、既定で「リバース」（上に倒すと下を向く）**。最初の試遊で「上下が反対」と指摘があったため。設定で「ノーマル」に戻せる。マウスの上下は別に切り替えられる
- タッチ操作（画面上のスティックとボタン）は入れていない。スマホでもコントローラーで遊ぶ前提（Androidの標準の仮想スティックも出さない）

## 見た目と音
- 選手は13個の硬いパーツ（`/Game/Art/Heroes/<ヒーロー>/SM_<ヒーロー>_<部位>`、骨盤・胴・頭・上腕・前腕・太もも・すね・足）を関節で回して動かす。歩きは足を床に固定するIK（`Character/TaGRig.cpp`）。パーツが無い場合はエンジンの基本図形で組む。
- 素材はすべて光を受けないトゥーン素材（`/Game/Art/Materials`）。光の向きと色は素材パラメータのコレクション `MPC_TaG` が持つ。シーンにはライトも大気も霧も置かず、露出は手動の0EV、トーンカーブは切っている（素材の1.0が画面でもそのまま1.0）。そのためLumenとメッシュの距離場は切った（`Config/DefaultEngine.ini`）
- トゥーン素材は影を受けないので、キャラとボールの足元に暗い円を置いている（跳ぶと小さく薄くなる。ボールの高さが分かる）
- 敵味方は、輪郭線とTaGの光の色で分かる（味方は青緑、相手は赤橙。WORLD.md §7）
- コートと街（`/Game/Art/Court`、`/Game/Art/City`）、効果音（`/Game/Audio/SFX`）、BGM（`/Game/Audio/Music`）も同じで、無い場合はエンジンの図形と無音で動く。
- BGMは `tools/audio/synth_music.py`（numpyで合成、継ぎ目なくループ）で作り、`tools/import/import_music.py` で取り込む。メニューとチュートリアルは MusicMenu、試合は MusicMatch に切り替わる（1.2秒のクロスフェード）。
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
"C:/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun -project="C:/TaG/game/BasketTaG.uproject" -platform=Android -cookflavor=ASTC -clientconfig=Shipping -build -cook -stage -pak -package -archive -archivedirectory="C:/TaG/build/ue_ship" -nop4 -utf8output -unattended -nocompileeditor
# 署名をv1・v2・v3の3方式にし直す（古い端末や、端末ごとの検査の差に備える）
SDK="$LOCALAPPDATA/Android/Sdk/build-tools/36.0.0"
"$SDK/zipalign" -f -p 4 BasketTaG-arm64.apk aligned.apk
"$SDK/apksigner.bat" sign --ks "$USERPROFILE/.android/debug.keystore" --ks-pass pass:android --ks-key-alias androiddebugkey --key-pass pass:android --v1-signing-enabled true --v2-signing-enabled true --v3-signing-enabled true --out TaG.apk aligned.apk
"$SDK/apksigner.bat" verify --verbose TaG.apk
```
- Android SDK（`%LOCALAPPDATA%\Android\Sdk`、NDK r27c）とJDK 21が必要。環境変数は `ANDROID_HOME`、`NDKROOT`、`JAVA_HOME`
- Shippingビルド（検証レイヤーなどの開発用ライブラリが入らず、小さい）。署名はデバッグ鍵。配布用の署名鍵はまだない
- versionCode は `Config/DefaultEngine.ini` の `StoreVersion`（いまは2）。上書きインストールするときは前より大きくする

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
"C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "$P" -game -windowed -TaGTutorialTest       # チュートリアルを本物の入力経路で最後まで遊ぶ（BOTの予兆に答える）
"C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "$P" -game -windowed -TaGSnap=Play -TaGHero=Gantetsu   # Title / Lobby / Play / TPS / Defense / Results / Settings / Tutorial（-TaGLesson=N）/ TutorialDone / Trail / Gait のスクリーンショット
"$UE" "$P" -game -nullrhi -unattended -TaGAutoTest=3 -TaGTeamHeroes=Rei,Hayate,Tsumugi,Hibiki,Tsubame,Kazami -benchmark -fps=60 -stdout   # 6人のヒーローを指定した3on3。技を使うたびに ARTUSE を出力
"C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "$P" -game -windowed -TaGSnap=Gait   # 歩きの検査：前・後ろ・横・斜めに歩かせ、接地中の足のずれ（cm）と歩数を測り、横からの連続写真を撮る
```

### 最新の結果（2026-11-08、3回目・v1.2）
- **30体と60種の技**：6人ずつ5組で全30体を出すBOT同士の3on3を2回と、まだ出ていない技を持つ7体で2試合。クラッシュはなく、どの試合も最後まで進んだ。**60種の技はすべて1回以上発動した**（59種はBOTの試合で、ツララの氷滑走は inputtest で確認）。1試合での使用回数は、チームあたり9〜48回
- **歩き**（`-TaGSnap=Gait`）：レイ・ハヤテ・ミナモ・ガンテツを前・後ろ・横・斜めに歩かせ、接地している足のずれを測った。最大でレイ4.1cm、ガンテツ4.5cm、ミナモ6.5cm、ハヤテ13.2cm（背が高く歩幅の大きいハヤテが、後ろ歩きの切り返しで滑る）。歩数は39〜44歩
- **見た目**：Blender側の素材3つがPC（SM6）でコンパイルに失敗して画面が黒くなっていたのを直し、タイトル、ロビー（30体の顔の絵）、試合（一人称・三人称・守備）、結果、歩きをスクリーンショットで確認した
- **回帰**：shottest 400/400、inputtest（突破・シュート・技・ポーズ・カメラ）正常、tutorialtest（9レッスンを37.6秒で通過）

### 2回目の結果（2026-11-08）
- **tutorialtest**：チュートリアルの9レッスンを、入力を流し込む自動プレイで最後まで通過した（41.5秒。読み合いの4レッスンはどれも1回目で成功）
- **回帰**：shottest 400/400、inputtest（突破・シュート・技・ポーズ・カメラ）正常、autotest 1on1（7-11、220秒）と3on3（11-3、166秒）が最後まで進行
- **画面**：タイトル（チュートリアルを先頭に）、設定、チュートリアルのレッスン表示と完了画面、シュートの光の尾をスクリーンショットで確認した

### 1回目の結果（2026-11-08）
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
| `Core/TaGHeroData.*` | ヒーロー30体と技60種のデータ（体格・速さ・リーチ・重さ・技の種類と数値・BOTの使いどころ） |
| `Core/TaGFx.*` | パーティクル（インスタンス描画、1800粒まで）と、技・読み勝ち・得点などの演出のレシピ |
| `Core/TaGRules.*` | シュートの種類と成功率（距離・タイミング・守備・ゾーン・チェイン・固有技） |
| `Core/TaGGameMode.*` | ロビー、チーム分け、試合の進行、じゃんけんの判定、計測、検証用のモード |
| `Core/TaGGameState.*`、`TaGPlayerState.*` | 全員に同期する状態（フェーズ、スコア、チームの計測値）、個人の選択と成績 |
| `Core/TaGStage.*` | 屋上コートと街。得点に応じて地区の窓が灯る |
| `Core/TaGAudio.*`、`TaGArtLib.*` | 効果音とBGM（場面で切り替え、音量）、Blender製の素材の読み込み |
| `Core/TaGTutorial.*` | チュートリアルのレッスン（題、説明、目標、どちらが攻めるか）。進行は `TaGGameMode` |
| `Ball/TaGBallSim.*`、`TaGBall.*` | ボールの物理（自前の放物線）、パス、インターセプト、ネット同期 |
| `Character/TaGCharacter.*` | 選手：行動の状態機械（じゃんけんの手、シュート、パス、固有技）、ダウンと復帰、見た目 |
| `Character/TaGCharacterArts.cpp` | データで動く48種の技（突進・跳躍・殻・すり抜け・反撃・囮・壁・根・光線・場・味方への付与・パスなど）と、その状態（無敵、根、反撃、目くらまし、印など） |
| `Character/TaGRig.*` | 関節モデルと手続き的なアニメーション。足を床に固定するIK（2関節のIK、歩幅と接地時間、つま先の返し、着地の沈み） |
| `Character/TaGArtActors.*` | 壁（鉄・瓦・溶接・水門など）、フィールド（減速・胞子・風・湯気・霧・氷・群れ・根）、光の線、残像 |
| `Character/TaGMovementComponent.*` | 行動ごとの速さと加速（クライアント予測と整合） |
| `Character/TaGBotController.*` | BOT（チュートリアルでは、予兆を見せてから1つの手を出す台本で動く） |
| `Character/TaGPlayerController.*` | 入力、設定（保存）、振動、メニュー操作、ロビーのRPC、検証用の入力 |
| `Abilities/*` | 入力チャンネルごとのGameplay Ability（予測あり）、GameplayTag |
| `UI/*` | Slateの画面（タイトル、ロビー、HUD、チュートリアルの表示、ポーズ、設定、結果） |

## まだのこと
- 人が実際に遊んで手触りを確かめる（ロックオン、視点の感度、技のタイミング、チュートリアルの予兆の長さ）
- Android実機での動作確認（インストールできるか、コントローラーの割り当て、性能）
- インターネット越しのマッチング（Steamなど）、配布用の署名鍵
- 5on5、モーションキャプチャ等の本格的なアニメーション（[ROSTER.md](../docs/ROSTER.md) の拡張の棚）
- 30体の技の強さの調整（いまはBOT同士の試合で全部の技が使われ、試合が最後まで進むことまで確かめた。勝率の偏りは人が遊んでから見る）
