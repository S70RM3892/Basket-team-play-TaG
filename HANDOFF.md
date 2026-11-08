# 引き継ぎ書 — Basket Team-play TaG

- 作成日：2026-11-07（2026-11-08 更新：UE5版 v1）
- リポジトリ：`S70RM3892/Basket-team-play-TaG`
- 作業ブランチ：`ccr-1c57bb78-u39iz8`（プルリクエストは未作成）
- 最新の成果：**UE5版 v1**（`game/`）。Godotのプロトタイプv0.4（`prototype/`）は手触りの参考として残している
- **最初に読む順番**：この文書 → [game/README.md](game/README.md) → [docs/RULES_V1.md](docs/RULES_V1.md) → [docs/STRATEGY.md](docs/STRATEGY.md) → [tools/README.md](tools/README.md)

---

## 1. 一言で
**「普通のバスケ ＋ アビリティじゃんけん」の一人称チームスポーツ。** ヒーローシューターのようにロールとアビリティで協力して戦う。
世界観は、海に沈みかけた積層都市「ナギサ」で、バスケが発電になり、勝った地区に電気が灯るというもの。

---

## 2. 確定している方針（最新）
| 項目 | 決定 | 詳細 |
|---|---|---|
| 競技の土台 | **普通のバスケ**（3on3はFIBA 3x3方式、5on5はフルコート）。バックボードありの普通のコート | [RULES_V1.md](docs/RULES_V1.md) §5 |
| アビリティの核 | **1対1の3すくみ**。攻撃（突破・シュート・かわし）対 守備（タックル・シュート妨害・間合い）。どの手も1つに勝ち、1つに負け、1つとあいこ | RULES_V1 §3 |
| 攻守のバランス | 理論値では **攻めが少しだけ強い**。1回の攻撃の期待値の目安は1.05〜1.15点（2点／3点方式）。1対1で強引に攻めるより、連携したほうが得をする数値にする | RULES_V1 §2 |
| 逆転の仕組み | **人工的なものは足さない**（バスケは交互に攻めるので、得点の構造そのものに逆転の力がある） | RULES_V1 §2.3 |
| 視点・機種 | PC・一人称（ヒーローFPS風）。プロトタイプはAndroid＋コントローラー | [CONCEPT.md](docs/CONCEPT.md) |
| 世界観 | 発電都市ナギサ。TaG＝Tandem-amplified Gear（同期で増幅するギア）。人間・自我機（機械）・義体者・発電生物・群体機が選手になる | [WORLD.md](docs/WORLD.md) |
| キャラ | ロール3種（ウォール・スコアラー・プレイメーカー）。体の大きさがそのまま性能になる（当たり判定は見た目どおり）。構想は24体 | [ROSTER.md](docs/ROSTER.md) |
| 市場の戦略 | 「ヒーローシューター」とは名乗らず、「一人称スポーツ」と名乗る。発売時は3on3（＋1on1・2on2・bot）。規模は絞る | [STRATEGY.md](docs/STRATEGY.md) |
| 本番のエンジン | **Unreal Engine 5.8.4**（C++・GAS・Enhanced Input）。v1を実装済み。Godot 4.7.2のプロトタイプは参考 | [game/README.md](game/README.md) |

---

## 2.5 UE5版 v1の現状（2026-11-08）
- **遊べるもの**：1on1／2on2／3on3、アビリティじゃんけん（6つの手と読み勝ちの演出）、シュート7種類、パス、ギャザー制、ダウンと味方による復帰、ヒーロー6体（固有技2つずつ）、チームのBOT、プライベートロビー（ホスト／IPで参加）、屋上コートと「点を取ると地区に明かりが灯る」街、効果音、タイトル・ヒーロー選択・結果の画面
- **2回目で足したもの**：チュートリアル（9レッスン。BOTが色つきの予兆を見せてから動き、それに答えて読み合いを覚える）、「すぐ遊ぶ（3on3）」、BGM2曲（場面で切り替え）、振動とカメラの揺れ、ボールの光の尾、設定画面（感度・上下反転・視野角・音量・振動）
- **ユーザーの指示（2回目）**：**タッチ操作は不要**（スマホでもコントローラーで遊ぶ）。**右スティックの上下が反対だった**ので、既定を「リバース」（上に倒すと下を向く）にし、設定で切り替えられるようにした
- **見た目**：Blenderのスクリプトで作ったヒーロー・ボール・コート・街（`tools/blender`）を、関節ごとの硬いパーツとして手続き的に動かしている（Godot版 `rig.gd` の移植）。素材が無い場合はエンジンの図形で動く
- **確かめたこと**：シュートの物理（400/400）、人間の入力経路（入力を流し込むテスト）、BOT同士の全モード、同じPCでのホスト／クライアント接続、各画面のスクリーンショット、素材込みのAndroid APK（164MB）
- **まだ確かめていないこと**：**人が実際に遊んだ手触り**、実機（Android）、インターネット越しの対戦
- **APKが入らなかった件**：1回目のAPK（Development、164MB）は実機で「パッケージが無効の可能性があるため、アプリをインストールできませんでした」と出た。APKの中身（v2署名、zipalign、resources.arsc の無圧縮と整列、testOnlyなし、exportedの指定、16KBページ対応）はすべて正常で、試作版（`com.s70rm3892.tagproto`）ともパッケージ名は別。ダウンロードの途中切れか端末側の事情と見て、2回目はShippingビルド（小さい）、署名v1・v2・v3、versionCode 2にし、サイズとSHA-256を添えて渡す。それでも入らなければ、USBでつないで `adb install` のエラー文を見る
- **開発環境（このPC）**：UE 5.8.4は `C:\Epic Games\UE_5.8`（Android対応込み）。古い `C:\Program Files\Epic Games\UE_5.8` はAndroid非対応のまま残している。Visual Studio 2026（MSVC 14.50、Windows SDK 22621）、Android SDK（`%LOCALAPPDATA%\Android\Sdk`、NDK r27c、`ANDROID_HOME`/`NDKROOT` 設定済み）、JDK 21、Blender 5.2、GitHub CLI（`gh`、ログイン済み）
- **検証用のモード**：`-TaGShotTest`、`-TaGAutoTest=1|2|3`、`-TaGInputTest`、`-TaGTutorialTest`、`-TaGNetHost`/`-TaGNetClient`、`-TaGSnap=<画面>`（詳細は game/README.md）

---

## 3. 廃案・凍結したもの（復活させないこと）
| 項目 | 状態 | 理由 |
|---|---|---|
| 臨界コア（パスで充電される「爆弾」ボール）、コア・ストライク | **廃案** | ユーザーの判断で「謎の爆弾パス」は不要。普通のバスケに戻した |
| 三層リング、ケージ・アリーナ（壁・天井を使う）、ディープライン | 廃案 | 同上（[BEYOND.md](docs/BEYOND.md) は記録として残してある） |
| 中央のポールを囲むドーナツ型のリング | 不採用 | バックボードを使うレイアップ・バンクショット・アリウープが消える。360°から攻められて攻めが強すぎる。一人称でポールが視界を塞ぐ |
| スマホ向けの斜め上カメラ | 不採用 | 一人称のPCに変更した |
| クラッチタイム、オーバーコア（逆転用の仕組み） | 凍結 | バスケの構造に逆転の力があるため不要 |
| 降下式コート、タッグ交代、5on5、24体のヒーロー | 凍結（拡張の候補） | 人が集まってから。一人で作れる規模に絞った |

---

## 4. ドキュメント一覧
| ファイル | 中身 | 状態 |
|---|---|---|
| [README.md](README.md) | 概要と目次 | 最新 |
| [docs/RULES_V1.md](docs/RULES_V1.md) | **最新の競技ルール**（攻守バランスの理論、じゃんけん、コート） | 最新・最優先 |
| [docs/STRATEGY.md](docs/STRATEGY.md) | 市場の現実、勝ち筋、規模の棚卸し | 最新（核の記述はRULES_V1に合わせて更新済み） |
| [docs/ABILITY_DESIGN.md](docs/ABILITY_DESIGN.md) | アビリティ設計の原則（ズレ、ギャザー制、バスケの動作で対抗できること） | 有効 |
| [docs/ROSTER.md](docs/ROSTER.md) | 体格システム、ヒーロー24体の構想 | 有効 |
| [docs/WORLD.md](docs/WORLD.md) | 世界観（ナギサ、TaG、住人の種類、ヒーロー、アート） | 有効 |
| [docs/TECH.md](docs/TECH.md) | UE5の技術設計（GAS、CMC、ボールの同期） | 有効（本番用） |
| [docs/CONCEPT.md](docs/CONCEPT.md) | 初期からのゲームデザイン全体 | 一部古い。食い違いはRULES_V1が優先 |
| [docs/WORLD_IDEAS.md](docs/WORLD_IDEAS.md) | 世界観の候補12案 | 参考 |
| [docs/BEYOND.md](docs/BEYOND.md) | 現実のバスケを超えるルール案 | **廃案**（記録） |
| [prototype/README.md](prototype/README.md) | プロトタイプの操作・変更履歴・ビルド方法 | 最新 |

---

## 5. プロトタイプの現状（Godot 4.7.2、v0.4）
### 5.1 できること
- 一人称の1on1（対BOT）。3x3方式（アーク内1点・外2点、11点先取、ショットクロック12秒、ボールを奪ったらアーク外へクリア）
- **アビリティじゃんけん**：攻撃はLB突破・RBかわし、守備はLBタックル・RB長押しでシュート妨害・Xスティール・B追走
- **読み勝ちのご褒美**：一瞬止まってからスローになり、画面が光って音が鳴る。負けた側は1秒以上動けず、勝った側は2秒間ゾーン（速くなり、次のシュートが入りやすい）
- **シュート7種類が、基本操作の組み合わせで出る**：
  - RT＝ジャンプショット
  - ↑+RT＝レイアップ（近い）・フローター（中距離）
  - ↓+RT＝ステップバック
  - A→空中で↓+RT＝フェイダウェイ
  - LB突破→RT＝ダンク・ユーロステップ
- シュートは、溜めている間にリングへ自動で照準が合う。ジャンプの頂点で離すと「パーフェクト」。溜めている間、成功率を％で表示する
- **テンポは実測値に合わせた**（ジャンプショット約0.75秒、移動は秒速4.8m、重力9.8）。ポーズ中のRBで「速い」に切り替えられる
- **関節で動く選手モデル**（プログラムで組んだ仮のもの）。走る・ドリブル・守備の構え・各シュート・妨害・タックル・スピン・ダウンの動きがある
- BOT：試合ごとにクセ（好きな手）があり、試合が終わると答え合わせを表示する。弱いBOTは技の前に構えが見える。強さは4段階
- ポーズ中：コート倍率（BACK）、BOTの強さ（X）、一人称／三人称（LB）、テンポ（RB）、計測値（攻撃1回あたりの得点、読み勝ちの回数、シュートの種類ごとの成功数）

### 5.2 これまでの経緯（ユーザーの感想 → 対応）
| 版 | ユーザーの感想 | 対応 |
|---|---|---|
| v0.1 | 「読み勝っても手応えがない／BOTが強い／シュートが全く入らない／全く面白くない」 | 照準のずれへの隠れたペナルティ（0.3倍）が原因だった → v0.2でロックオン、読み勝ちの演出と効果、BOTのクセ、三人称 |
| v0.2 | 「基本操作の組み合わせでフェイダウェイ・レイアップ・ステップバックなどが出るようにすべき。**手触りは悪くなかった**」 | v0.3でシュート7種類 |
| v0.3 | 「出るけど、現実より速すぎて楽しくないと思う人が多いと思う。キャラのモデルもちゃんとやったらいいかも」 | v0.4で実測値のテンポ、関節モデル、テンポの切り替え |
| v0.4 | （まだ感想をもらっていない） | 確認したいこと：「リアル」と「速い」のどちらが楽しいか、相手の体の動きを見て技を読めるか |

### 5.3 ファイル構成（`prototype/`）
| ファイル | 中身 |
|---|---|
| `scripts/main.gd` | 試合の進行、コートの生成、入力の割り当て、じゃんけんの判定（`resolve_tackle`、`on_read_win` など）、シュートの成功率（`shot_chance`）、計測、検証用のモード |
| `scripts/athlete.gd` | 選手（人間とBOT共通）。状態遷移、移動、6つの手、**シュートの種類の判定（`_classify_shot`）と種類ごとの値（`SHOT_TYPES`）**、テンポの定数 |
| `scripts/rig.gd` | 関節モデルの組み立てと、プログラムによる動き（`animate`） |
| `scripts/ai_brain.gd` | BOTの思考（クセ、構え、反応、シュートの種類の選び方） |
| `scripts/ball.gd` | ボール（保持・シュート・ルーズボール・得点の判定） |
| `scripts/hud.gd` | 画面表示とポーズメニュー |

### 5.4 検証用のモード（ヘッドレスで動く）
```bash
godot --headless --path prototype -- --autotest   # BOTどうしで1試合して、計測値を出力する
godot --headless --path prototype -- --shottest   # 計画上「成功」のシュートが、物理で本当に入るかを距離ごと（＋ダンク）に確かめる
xvfb-run -a godot --path prototype --rendering-driver opengl3 --resolution 1280x720 -- --snap=/path/out.png [--snap-play|--snap-demo|--snap-step|--snap-tps]   # 画面のスクリーンショット
```
最新の結果（v0.4）：autotestは11対10で、攻撃1回あたりの得点は約0.5（1点／2点方式）。shottestは全距離とダンクで100%入った。

### 5.5 既知の課題・未検証
- **実機で遊んだのはユーザーだけ。** コントローラーのトリガーの割り当ては端末によって違う可能性がある（RTの代わりにY、LTの代わりにBでも操作できるようにしてある）
- シュートの種類を判定するしきい値（距離、スティックの角度）は、まだユーザーの感想で調整していない
- 攻撃1回あたりの得点が目標より低め（BOTどうしで約0.5）。RULES_V1の目標は2点／3点方式で1.05〜1.15で、1点／2点方式に換算するとおよそ0.7
- 2on2（「連携したほうが得をするか」の検証。RULES_V1 §7-4）は未実装
- 選手モデルはプログラムで組んだ仮のもの。本格的なモデルとモーションは、遊びの核が面白いと確信できてから
- 一人称と三人称のどちらを主にするかは未決定

---

## 6. ビルド環境の作り直し（新しい環境でAPKを作る手順）
前の環境（クラウド）では、github.com への接続が遮断されていた。そのため、Godotは公式サイトのミラーから取得した。

```bash
# 1) Godot 4.7.2（エディタ）と書き出しテンプレート
curl -L -o godot.zip "https://downloads.godotengine.org/?version=4.7.2&flavor=stable&slug=linux.x86_64.zip&platform=linux.64"
curl -L -o templates.tpz "https://downloads.godotengine.org/?version=4.7.2&flavor=stable&slug=export_templates.tpz&platform=templates"
unzip godot.zip
mkdir -p ~/.local/share/godot/export_templates/4.7.2.stable
unzip -j templates.tpz 'templates/android*' 'templates/version.txt' -d ~/.local/share/godot/export_templates/4.7.2.stable
#   ※ tpz は約1.2GB。Androidの分だけ取り出せばよい（Windowsの場合はエディタの「Manage Export Templates」から入れられる）

# 2) Android SDK（cmdline-tools → platform-tools・build-tools 35.0.1・platforms;android-35）
#    https://dl.google.com/android/repository/commandlinetools-linux-<番号>_latest.zip を展開して cmdline-tools/latest に置く
yes | sdkmanager --sdk_root=$ANDROID_SDK --licenses
sdkmanager --sdk_root=$ANDROID_SDK "platform-tools" "build-tools;35.0.1" "platforms;android-35"

# 3) JDK（Godotの推奨は17。前の環境では21で動いた）と署名鍵
keytool -genkeypair -v -keystore release.keystore -alias tagproto -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=TaG Prototype,O=TaG,C=JP"

# 4) Godotのエディタ設定（~/.config/godot/editor_settings-4.7.tres）
#    export/android/android_sdk_path = "<SDKのパス>"
#    export/android/java_sdk_path    = "<JDKのパス>"
#    （一度 godot --headless --path prototype --import を実行すると、設定ファイルが作られる）

# 5) 書き出し
godot --headless --path prototype --import
export GODOT_ANDROID_KEYSTORE_RELEASE_PATH=/path/release.keystore
export GODOT_ANDROID_KEYSTORE_RELEASE_USER=tagproto
export GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD=<鍵のパスワード>
godot --headless --path prototype --export-release "Android" ../build/TaG-prototype.apk
```

- 書き出しの設定は `prototype/export_presets.cfg` にある。arm64のみ、パッケージ名 `com.s70rm3892.tagproto`、versionCode 4。次の版では5に上げる。
- **署名鍵はリポジトリに入れていない**（`.gitignore` で除外している）。新しい環境で鍵を作り直すと署名が変わるので、**端末に入っている古いAPKを一度アンインストールしてから** 新しいものを入れること。
- `build/`、`.godot/`、`*.apk`、`*.keystore` は `.gitignore` で除外している。

---

## 7. 次にやること（優先順、2026-11-08 更新）
1. **UE5版 v1を人が遊んで感想をもらう**：ロックオンとシュートのタイミング、視点の感度、技の読み合いが体の動きとTaGの光で読めるか、ヒーローごとの差が分かるか。STRATEGY §6の基準「友達が『もう1回』と言うか」
2. 感想に合わせて数値を調整する（`Core/TaGTypes.h`、`Core/TaGHeroData.cpp`、`Core/TaGRules.cpp`）。特に **連携と1対1の得点効率の差がまだ小さい**（3on3でおよそ0.61対0.58）ので、RULES_V1 §2.4の狙いに向けて広げる
3. Android実機でAPKを動かす（インストールできるか、コントローラーの割り当て、性能）。タッチ操作は作らない（ユーザーの指示）
4. インターネット越しの対戦（Steam／EOSのマッチング、または招待コード）
5. 見た目の仕上げ：スキンメッシュとアニメーション、ネットとツムギの糸・ヒビキの触手の揺れ、ゴールの「ゴール炉」の演出
6. 配布の準備：署名鍵、ストアのページ（STRATEGY §3.5の価格の方針）。Shippingビルドは作れるようになった

---

## 8. 進め方の約束（このプロジェクトでのやり取り）
- やり取りは日本語。口調は率直でよい（ですます調でなくてよい）
- 設計の判断には、信頼できる出典（研究、公式の資料、実在のゲームの前例）を付ける
- 既存のゲームの丸写しはしない（名前・キャラ・見た目はオリジナル）。仕組みのジャンルを参考にするのはよい
- 成果物はコミットしてブランチにpushする。APKはリポジトリに入れず、直接ユーザーに渡す

---

## 9. 新しいセッションに貼る指示文（例）
```
リポジトリ S70RM3892/Basket-team-play-TaG のブランチ ccr-1c57bb78-u39iz8 で作業を続ける。
まず HANDOFF.md、docs/RULES_V1.md、prototype/README.md を読んで現状を把握すること。
プロトタイプは Godot 4.7.2（prototype/）で、Android APK（コントローラー対応）として書き出す。
ビルド環境は HANDOFF.md §6 の手順で作り直す（署名鍵は新しく作るので、端末の古いAPKは一度アンインストールが必要）。
次の作業は HANDOFF.md §7 の1から。
```
