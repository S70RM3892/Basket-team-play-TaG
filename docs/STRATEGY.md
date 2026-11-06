# Basket Team-play TaG — 勝ち筋の戦略 v0.1

> 「覇権が見えない」は正しい直感だ。今の設計は **足し算で作ったもの** なので、覇権の条件をほとんど満たしていない。
> この文書では、なぜ見えないのかを分解し、見える状態に作り直す。

---

## 1. 現実のデータ：市場で何が起きているか

### 死んだもの（ヒーローシューター・競技シューターはレッドオーシャン）
| タイトル | 何が起きたか |
|---|---|
| **Concord**（Sony、2024年） | Steamでの同時接続のピークは697人。発売から約2週間でサーバーを停止し、全額返金。「ヒーローシューターのブームが終わった後に出た」「40ドルの価格」が敗因として指摘された |
| **Spectre Divide**（2025年） | 6,000万ドル以上の投資を受けながら、最初のシーズンで運営費を賄えず、発売から約6ヶ月で開発スタジオごと閉鎖 |
| **XDefiant**（Ubisoft） | 2024年12月に終了を発表し、2025年6月にサーバーを停止 |
| **Omega Strikers**（ヒーロー制スポーツ） | プレイヤー数を伸ばせず、発売から約8ヶ月でコンテンツ開発を終了 |

### 当たったもの
| タイトル | 何が当たったか | 学べること |
|---|---|---|
| **Rematch**（Sloclap、2025年） | 5v5のサッカー。初日に100万人、1ヶ月で500万人が遊んだ。**ヒーローもアビリティもない**、純粋な操作の腕前勝負のスポーツPvP。29.99ドルの買い切りで、Game Passにも同時に入った | **スポーツPvPは、素の操作が面白ければ当たる。** アビリティは必須ではない |
| **Lethal Company**（一人開発、2023年） | 推定で約1,000万本。友達と遊ぶ協力ゲームで、配信で爆発的に広まった | **一人開発でも当たる。ただし友達と遊ぶ協力型で、安い** |
| **Marvel Rivals**（2024年） | 発売時33体のヒーロー | 巨大なIPと大規模な開発。一人開発が真似できる条件ではない |

### 前提
- Steamでは2025年に **1万9,000本以上** のゲームが出て、その約半分はレビューが10件にも届いていない。

---

## 2. 今の設計で覇権が見えない5つの理由

| # | 理由 | 具体的には |
|---|---|---|
| 1 | **一言で説明できない** | 臨界コア、三層リング、ストライク、降下式コート、タッグ交代、24体、3ロール、体格システム…。当たるゲームは5秒で分かる（「車でサッカー」「一人称サッカー」） |
| 2 | **「ヒーローシューター」と名乗ると死ぬ** | その棚にはOverwatchとMarvel Rivalsがいて、比較された瞬間に負ける。疲れた客はConcordを見て逃げた |
| 3 | **人が集まらないと遊べない構造** | 5on5は1試合に10人必要で、ロールとタッグの選択まであると、マッチングに大量の同時接続が要る。Omega Strikersはまさにこれで死んだ |
| 4 | **一人開発の規模と矛盾している** | 24体、3層の降下ステージ、オンラインの専用サーバー。完成する前に力尽きる |
| 5 | **アビリティが土台の面白さを隠している** | アビリティなしで面白いかどうかを、まだ誰も確かめていない。Rematchは素の操作だけで500万人を集めた |

---

## 3. 作り直し：覇権が見える形へ

### 3.1 核を1つに絞る
**「パスで溜めて、爆発させろ。」——一人称の3on3。**

- 核にするのは **臨界コア**（パスするほど点が膨らみ、時間が減る）と、**コア・ストライクとキャッチ** の2つだけ。
- 5秒のクリップで伝わるのは、「光が強くなっていくボール → 最後の1本 → 爆発的な得点」か、「ストライクを至近距離でキャッチ」の場面だ。
- ジャンルの名前は「ヒーローシューター」ではなく **「一人称スポーツ」**。競合はHoopsFPSのような小規模なタイトルしかいないので、**ジャンルの中でなら覇権が取れる**。

### 3.2 アビリティなしで面白いことを先に証明する
- **最初のプロトタイプは、ヒーローなし・アビリティなし** で作る。臨界コア＋壁で跳ね返るケージ＋壁走り＋ストライクとキャッチだけ。
- アビリティを全部オフにしても面白ければ、土台は強い。面白くなければ、アビリティを足しても救えない。

### 3.3 人が少なくても遊べる構造にする
- **発売時は3on3だけ**（1試合6人）。5on5は人が集まってから追加する。
- **1on1と2on2も用意する**（友達1人だけでも遊べる）。プライベートロビーと、足りない人数を埋めるbotも入れる。
- ロールは「推奨」にとどめ、ロールの固定キューは、人が十分にいるようになってから入れる。

### 3.4 配信で広がるように作る
- 試合のハイライト（爆発的な得点、キャッチ）を自動で切り出して、クリップとして保存できるようにする。
- 観戦者からも、ボールの充電の状態がひと目で分かる見た目にする。

### 3.5 値段で負けない
- 低価格の買い切り（1,500円前後）、または「1人が買えば友達も無料で一緒に遊べる」方式にする。Concordは40ドルの値付けが敗因の一つとされた。
- 基本無料（F2P）は運営の負荷が重く、一人開発には向かない（Spectre Divideは基本無料で、運営費を賄えなかった）。

---

## 4. 規模の棚卸し

| 区分 | 入れるもの |
|---|---|
| **核（発売時に必須）** | 臨界コア、ケージ・アリーナ1つ、壁走り、ストライクとキャッチ、3on3／2on2／1on1、bot、プライベートロビー |
| **発売時にあれば強い** | ヒーロー6体（3ロール×2体）、体格システム、ギャザー制 |
| **拡張の棚（凍結。消さない）** | 5on5、三層リング、降下式コート、タッグ交代、24体のヒーロー、ロール固定キュー、世界観の物語 |

> 世界観（ナギサ、TaG、コアボール）は捨てない。**臨界コアの見た目と演出の理由として、最初から効いている。** 物語を深く語るのは後でいい。

---

## 5. 「覇権」の段階を現実的に決める

| 段階 | 目安 | 一人開発で届くか |
|---|---|---|
| 第1段階：ジャンルの覇権 | 「一人称スポーツ」でSteamの1位。同時接続が数千人 | **届く**（競合が小さい） |
| 第2段階：インディーの大ヒット | 同時接続が数万人、数百万本 | 運と配信次第。Lethal Companyは一人開発で届いた |
| 第3段階：Rematch級 | 数百万人 | チームとパブリッシャーが要る。第2段階まで届いてから考える |
| 第4段階：Overwatch級 | — | 一人では無理。目標にしない |

---

## 6. 次の一手（検証の基準）
1. ヒーローなしの3on3プロトタイプを作る（臨界コア、ケージ、壁走り、ストライクとキャッチ）
2. **友達5人に遊んでもらい、「もう1回」と言うかどうか** だけを見る
3. 言わなければ、核を作り直す。言えば、ヒーロー6体を足す

---

## 参考資料
- Concordのサービス終了（PC Gamer）: https://pcgamer.com/games/fps/streamers-captured-concords-final-moments-as-the-servers-went-offline-what-a-huge-fing-honor
- Concordのサービス終了（Thurrott）: https://www.thurrott.com/games/308114/playstation-concord-shut-down
- Spectre Divideの閉鎖（PC Gamer）: https://pcgamer.com/games/fps/spectre-divide-and-its-studio-are-shutting-down-after-just-six-months-the-industry-is-in-a-tough-spot-right-now
- Spectre Divideの閉鎖（GamesRadar）: https://gamesradar.com/games/fps/despite-more-than-usd60-million-in-investments-shroud-backed-fps-spectre-divide-and-its-dev-are-shutting-down-after-the-first-season-failed-to-cover-day-to-day-costs
- XDefiant（Wikipedia）: https://en.wikipedia.org/wiki/XDefiant
- Rematch 初日に100万人（PC Gamer）: https://www.pcgamer.com/games/sports/rematch-surpasses-1-million-players-in-its-first-day-of-full-release
- Rematch 1ヶ月で500万人（Gamereactor）: https://www.gamereactor.eu/rematch-reaches-5-million-unique-players-1576873/
- Lethal Company 推定1,000万本（Game Developer）: https://gamedeveloper.com/business/lethal-company-sold-an-estimated-10-million-copies
- Steamの2025年のリリース数（GamingOnLinux）: https://www.gamingonlinux.com/2025/12/over-19000-games-have-released-on-steam-in-2025-with-nearly-half-seeing-less-than-10-reviews
- Omega Strikersの開発終了（TechRaptor）: https://techraptor.net/gaming/news/omega-strikers-will-cease-content-development-8-months-after-release-as-studio-moves-on
