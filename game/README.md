# TaG — UE5版 P1（素のバスケ・1on1）

[docs/TECH.md](../docs/TECH.md) §5「P1」の実装。Unreal Engine 5.8.4、C++、Gameplay Ability System（GAS）、Enhanced Input。
Godot版のプロトタイプ（[prototype/](../prototype/)）とは別物として、本番の土台から作り直している。

## 今できること
- 一人称の1on1（対BOT）。FIBA 3x3のハーフコート、アークの内側で1点・外側で2点、11点先取、ショットクロック12秒
- 守備側がボールを取ったら（リバウンド・スティール）、アークの外へ出す（クリアする）までシュートは無効
- **シュート（GA_Shoot）**：押すと溜め始め、0.36秒で自動的に跳ぶ。跳んだ頂点（0.68〜0.80秒）で離すと「パーフェクト」。溜めている間は視点がリングに吸い付き、画面下に成功率が出る
- **ブロック（GA_Block）**：視線の先に0.25秒だけ手を出す。上がっていくボールに手が当たれば叩き落とす
- **スティール（GA_Steal）**：正面のボール保持者に手を伸ばす。外すと0.45秒動きが鈍る
- **ボール**：エンジンの物理は使わず、自前の放物線で計算する。リングはトーラス、ボードは箱、床は平面として当たりを判定する（TECH.md §3）
- **BOT**：所定の位置まで運んでシュートし、守備では保持者とリングの間に立って、跳んで妨害する。ブロック、スティール、リバウンドもする

テンポはGodot版v0.4の「リアル」の値を使っている。移動は秒速4.8m、重力は9.8m/s²、ジャンプは約0.7m。

**アビリティじゃんけんはまだ入っていない。** P1は素のバスケの手触りを確かめる段階で、じゃんけんはその次（RULES_V1 §7-2）。

## アセットを持たない構成
コート、ライト、選手の見た目、入力の割り当て、HUDは、すべてC++で実行時に作っている。マップはエンジンの空のマップ（`/Engine/Maps/Entry`）を使う。
そのため、リポジトリにバイナリ（`.uasset` や `.umap`）がなく、差分がすべて読める。モデルやアニメーションを入れる段階になったら、Git LFSで管理する（`.gitattributes` は設定済み）。

## 操作
| 操作 | PC | コントローラー |
|---|---|---|
| 移動 / 視点 | WASD / マウス | 左スティック / 右スティック |
| ジャンプ | Space | A |
| シュート（押して溜め、頂点で離す） | 左クリック | RT（またはY） |
| ブロック | 右クリック | RB |
| スティール | F | X |
| リスタート | R | START |

## ビルドと起動
エンジンは `C:\Epic Games\UE_5.8` を前提にしている。
```bash
# エディタ用のビルド（テストと起動に使う）
"C:/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" BasketTaGEditor Win64 Development -Project="C:/TaG/game/BasketTaG.uproject" -WaitMutex
# ゲームとして起動
"C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/TaG/game/BasketTaG.uproject" -game -windowed -ResX=1600 -ResY=900
```

### Android APK
```bash
"C:/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun -project="C:/TaG/game/BasketTaG.uproject" -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -pak -package -archive -archivedirectory="C:/TaG/build/ue" -nop4 -utf8output -unattended
```
- Android SDK（`%LOCALAPPDATA%\Android\Sdk`、NDK r27c）とJDK 21が必要。環境変数は `ANDROID_HOME`、`NDKROOT`、`JAVA_HOME`
- UEのSDK検証（Turnkey）は「Android Studio is not installed correctly」と出るが、APKは作れる
- 今はDevelopmentビルドで、デバッグ署名を使っている。配布用の署名鍵は、まだ作っていない

## 検証用のモード（ヘッドレスで動く）
```bash
UE="C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
"$UE" "C:/TaG/game/BasketTaG.uproject" -game -nullrhi -unattended -TaGShotTest -stdout              # 計画上「成功」のシュートが、物理で本当に入るかを確かめる
"$UE" "C:/TaG/game/BasketTaG.uproject" -game -nullrhi -unattended -TaGAutoTest -benchmark -fps=60 -stdout   # BOT同士で1試合して、計測値（STATS）を出す
"C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "C:/TaG/game/BasketTaG.uproject" -game -windowed -TaGSnap   # 4秒後にスクリーンショットを撮って終了する（game/Saved/Screenshots）
```

### 最新の結果（2026-11-08）
- **shottest**：計画上「成功」のシュートが、0.8〜9mの全距離・5方向・ジャンプの有無のすべてで入った（100/100）。外す計画のシュートが偶然入ったのは4%（46/1200）
- **autotest**（BOTどうし、強さ0.6、3試合）

各列は「チーム0 / チーム1」の順。成功率は「実際（予測）」。

| 試合 | スコア | 攻撃1回あたりの得点 | 成功率 | ブロック | スティール |
|---|---|---|---|---|---|
| 1 | 6 - 11 | 0.67 / 1.22 | 0.28（0.44） / 0.70（0.49） | 0 / 2 | 2 / 0 |
| 2 | 11 - 7 | 0.58 / 0.35 | 0.38（0.38） / 0.22（0.36） | 3 / 4 | 4 / 3 |
| 3 | 11 - 6 | 0.79 / 0.35 | 0.42（0.40） / 0.25（0.41） | 4 / 1 | 2 / 0 |

攻撃1回あたりの得点は、1点／2点方式でおよそ0.35〜1.2。試合ごとのばらつきが大きく、目標の約0.7（RULES_V1の2点／3点方式で1.05〜1.15に相当）にはまだ届いていない。調整は、人が遊んだ感想と合わせて行う。

## ファイル構成
| ファイル | 中身 |
|---|---|
| `Source/BasketTaG/Core/TaGTypes.h` | コートの寸法、テンポ、ルールの定数（単位はcmと秒） |
| `Source/BasketTaG/Core/TaGRules.*` | シュートの成功率（距離、タイミング、守備の寄せ）。サーバー、HUD、BOTで共通 |
| `Source/BasketTaG/Core/TaGGameMode.*` | 試合の進行（得点、攻守交代、クリア、ショットクロック、アウト・オブ・バウンズ）、計測、検証用のモード |
| `Source/BasketTaG/Core/TaGGameState.*` | 全員に同期する状態（スコア、ショットクロック、攻撃側、クリアが必要か）と、画面に出す短いメッセージ |
| `Source/BasketTaG/Core/TaGCourt.*` | コート、リング、ボード、ライトの生成 |
| `Source/BasketTaG/Ball/TaGBallSim.*` | ボールの放物線と当たり判定（純粋な関数。テストと同じコードを使う） |
| `Source/BasketTaG/Ball/TaGBall.*` | ボールのアクター（保持、シュート、ルーズボール、ブロックとリバウンドの判定）。サーバー権威 |
| `Source/BasketTaG/Character/TaGCharacter.*` | 選手（人間とBOTで共通）。移動はCMC、能力はASC |
| `Source/BasketTaG/Character/TaGPlayerController.*` | 入力の割り当て（Enhanced InputをC++で作る） |
| `Source/BasketTaG/Character/TaGBotController.*` | BOTの思考 |
| `Source/BasketTaG/Abilities/*` | GA_Shoot、GA_Block、GA_Steal、GameplayTag |
| `Source/BasketTaG/UI/TaGHUD.*` | 画面表示（SlateをC++で組む） |

## P1の残り（TECH.md §5）
- [x] 1. プロジェクトの作成、GASの有効化
- [x] 2. TaGCharacter（移動、ジャンプ、ボールの保持）
- [x] 3. TaGBall（保持、ドリブル、放物線、リングの判定。オフライン）
- [x] 4. GA_Shoot、GA_Block、GA_Steal
- [ ] 5. listen server（ホスト兼プレイヤー）＋クライアント1人で動かして、同期のずれを確かめる
- [ ] 6. 完了条件：「シュートを決めた手応え」と「ブロックを当てた手応え」がある（人が遊んで確かめる）
- [ ] 一人称の手と、ボールを持っている見た目
- [ ] コートの倍率（1.0倍・1.15倍・1.3倍）の切り替え（RULES_V1 §5.2）
