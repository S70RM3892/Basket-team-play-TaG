# Basket Team-play TaG — 技術設計 v0.1（Unreal Engine 5）

## 1. 方針

| 項目 | 決定 | 理由 |
|---|---|---|
| エンジン | **Unreal Engine 5.8**（2026年6月リリースの最新版） | UE5の最後の大型アップデートと位置づけられている。UE6は2027年末にEarly Access予定なので、開発中に追いかけない |
| 言語 | C++でゲームの中核を書き、調整と演出はBlueprintで行う | ネットワークと判定はC++、手触りの調整はBlueprintで速く回す |
| 起点 | UE5の **First Person テンプレート**（C++版） | 一人称の移動と視点がすぐ動く |
| スキル | **Gameplay Ability System（GAS）** | スキルの発動・クールダウン・状態タグ・ステータスの変化を、クライアント側の予測付きで同期できる |
| 入力 | Enhanced Input | キーの割り当てを変更でき、守備と攻撃でボタンの意味を切り替えるのも楽 |
| 移動の同期 | Character Movement Component（CMC） | サーバー権威での移動とクライアント側の予測・補正が最初から入っている |
| バージョン管理 | Git + Git LFS（`.uasset` / `.umap` などのバイナリ） | GitHubのリポジトリをそのまま使える |

## 2. クラス構成（案）

```
Source/BasketTaG/
├── Core/
│   ├── TaGGameMode          … サーバー専用のルール（得点・攻守交代・クラッチタイム・オーバーコアの出現）
│   ├── TaGGameState         … 全員に同期する状態（スコア・試合時間・ショットクロック・モード）
│   ├── TaGPlayerState       … ヒーローID・得点・アシスト・ウルト量の統計
│   └── TaGModeConfig        … UPrimaryDataAsset。3on3と5on5の数値（§6の係数を含む）
├── Character/
│   ├── TaGCharacter         … ACharacterを継承。CMC・ASC（AbilitySystemComponent）・一人称の手を持つ
│   └── TaGHeroData          … UPrimaryDataAsset。ヒーローごとのアビリティ一覧・ステータス・モード別の係数
├── Abilities/
│   ├── TaGAttributeSet      … Guard / MaxGuard / MoveSpeed / UltCharge
│   ├── GA_Shoot, GA_Pass, GA_Block, GA_Steal, GA_Revive   … 全ヒーロー共通の基本動作
│   └── Heroes/GA_Gantetsu_Wall, GA_Rei_Blink, ...          … 各ヒーローのアーツ
├── Ball/
│   └── TaGBall              … コアボール。サーバー権威で、エンジンの汎用物理は使わない（§3）
└── UI/
```

### GameplayTagの例
`State.HasBall` `State.Down` `State.Airborne` `Ability.Cooldown.Skill1` `Chain.1` `Chain.2` `Chain.3` `Match.Clutch`

> ASC（AbilitySystemComponent）は、リスポーンもヒーロー変更もない前提でCharacterに持たせる。試合中のヒーロー変更を入れる場合は、PlayerStateに移す。

## 3. ボールの同期（一番難しいところ）

- ボールの状態は **サーバーが決める**。保持者、位置、速度、チェイン数、最後にパスした人（アシストの判定用）を全員に同期する。
- 飛んでいる間は **自前の放物線の式** で計算する。リング・バックボード・床との当たりは、単純な形で判定する。エンジンの汎用物理を使うと同期がずれ、ボールがガタつくため使わない。
- シュートやパスをした本人の画面では、ボールを **先に予測で動かし**、サーバーの結果が届いたら滑らかに補正する。
- **当たり判定は見た目と切り離す**：見た目の大きさは種類ごとに違っても、ボールを持っている判定・ブロック・スティール・体の接触の判定は、ロールごとの共通のカプセルにする。ボールを持つ位置は、全キャラで胸の前の同じ高さのソケットにそろえる（WORLD.md §3.5）。
- ブロックとスティールは **サーバーが判定** する。そのとき、撃った人の通信の遅れの分だけ、ボールと相手の位置を過去に巻き戻して当たりを見る（ラグ補正。**UE5に標準ではないので自前で書く**）。

## 4. 描画（競技FPSなので、フレームレートを最優先）
- 目標は144fps。スタイライズ表現で、光と影の計算を軽くする。
- 5.8で追加された軽量版のLumen（動的グローバルイルミネーション）を試す。重ければ事前計算のライティングにする。
- モーションブラー・カメラの揺れ・歩行時の上下の揺れは、初期設定でオフか弱めにする。

## 5. P1（1on1プロトタイプ）のタスク
1. First Person テンプレート（C++版）からプロジェクトを作る。GASプラグインを有効にする
2. TaGCharacter：移動・ジャンプ・一人称の手とボールの表示
3. TaGBall：保持・ドリブル・放物線・リングの判定（まずはオフラインで）
4. GA_Shoot（溜めて離す）、GA_Block、GA_Steal
5. ホスト兼プレイヤー（listen server）＋クライアント1人で動かして、ずれを確認する
6. 完了条件：「シュートを決めた手応え」と「ブロックを当てた手応え」がある

## 参考資料
- UE 5.8のリリース（PC Gamer）: https://pcgamer.com/software/unreal-engine-5-8-launches-with-improved-terrain-and-vegetation-tools-a-lumen-lite-option-for-faster-global-illumination-and-for-the-times-we-now-live-in-an-open-standard-plugin-for-llm-systems/
- State of Unreal 2026（Epic）: https://www.unrealengine.com/news/state-of-unreal-2026-top-news-from-the-show
- CMCのネットワーク移動（Epic公式）: https://dev.epicgames.com/documentation/ja-jp/unreal-engine/understanding-networked-movement-in-the-character-movement-component-for-unreal-engine
- GASDocumentation（コミュニティ製で、UE5.3時点の内容）: https://github.com/tranek/gasdocumentation
