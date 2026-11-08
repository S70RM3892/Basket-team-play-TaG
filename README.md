# Basket Team-play TaG

**普通のバスケ ＋ アビリティじゃんけん**。PC・一人称視点・Unreal Engine 5.8.4。**v1（1on1／2on2／3on3、ヒーロー6体、BOT、友達とのホスト／参加）が遊べる。**

> 海に沈みかけた積層都市「ナギサ」では、バスケで電気をつくる。勝った地区に明かりが灯る。

- 1対1の攻防は3すくみ：**突破・シュート・かわし** 対 **タックル・シュート妨害・間合い**
- 理論値では攻めを少しだけ強くする。1対1より連携したほうが得をする数値設計
- 普通のコート（バックボードあり）。3on3と5on5
- シュート・パス・ブロック・スティールは照準で狙う
- キルなし：ダウンさせても、味方が触れれば即座に復帰

| ドキュメント | 中身 |
|---|---|
| [HANDOFF.md](HANDOFF.md) | **引き継ぎ書**（現状・確定事項・廃案・ビルド手順・次の作業）。別の環境で続けるときは最初に読む |
| [game/](game/README.md) | **本番：UE5版 v1**（C++・GAS）。操作、ヒーロー、ビルド、検証用のモード |
| [tools/](tools/README.md) | 素材の作り方：Blenderのスクリプトでモデル、Pythonで効果音を作り、UEに取り込む |
| [prototype/](prototype/README.md) | **Godotのプロトタイプ v0.4**：1on1アビリティじゃんけん（UE版の手触りの元になった版） |
| [docs/RULES_V1.md](docs/RULES_V1.md) | **最新の競技ルール**：攻守バランスの理論、アビリティじゃんけん、コートの決定 |
| [docs/STRATEGY.md](docs/STRATEGY.md) | 勝ち筋の戦略：市場の現実、核を1つに絞る、規模の棚卸し |
| [docs/ABILITY_DESIGN.md](docs/ABILITY_DESIGN.md) | アビリティ設計の原則（ズレ／ギャザー制／バスケの動作で対抗できる） |
| [docs/ROSTER.md](docs/ROSTER.md) | 体格システムとヒーロー一覧（構想24体） |
| [docs/CONCEPT.md](docs/CONCEPT.md) | ゲームデザインの全体（古い記述はRULES_V1.mdが優先） |
| [docs/WORLD.md](docs/WORLD.md) | 世界観（都市・TaG・住人の種類・ヒーロー・アート） |
| [docs/TECH.md](docs/TECH.md) | UE5の技術設計 |
| [docs/WORLD_IDEAS.md](docs/WORLD_IDEAS.md) | 世界観の候補集（12案の比較） |
| [docs/BEYOND.md](docs/BEYOND.md) | （廃案）現実のバスケを超えるルール案 |
