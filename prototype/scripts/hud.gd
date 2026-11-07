extends CanvasLayer
## 画面表示：スコア・ショットクロック・シュートメーター・アビリティ・じゃんけんの結果・ポーズメニュー

const AthleteScript = preload("res://scripts/athlete.gd")
const S = AthleteScript.S

var game
var root: Control
var score_l: Label
var clock_l: Label
var status_l: Label
var abil_l: Label
var info_l: Label
var pause_panel: PanelContainer
var pause_l: Label
var pad_l: Label
var meter_bg: ColorRect
var meter_fill: ColorRect
var meter_zone: ColorRect
var cross: ColorRect
var popups: VBoxContainer
var last_shot_info := ""
var meter_perfect: ColorRect
var meter_pct: Label
var flash_rect: ColorRect
var big_l: Label
var big_sub: Label


func setup(p_game) -> void:
	game = p_game
	layer = 10
	process_mode = Node.PROCESS_MODE_ALWAYS
	root = Control.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var th := Theme.new()
	var f := SystemFont.new()
	f.font_names = PackedStringArray(["Noto Sans CJK JP", "Noto Sans JP", "Source Han Sans JP", "Hiragino Sans", "Yu Gothic", "Meiryo", "Droid Sans Fallback", "sans-serif"])
	th.default_font = f
	th.default_font_size = 22
	root.theme = th
	add_child(root)

	score_l = _label(34, HORIZONTAL_ALIGNMENT_CENTER)
	_place(score_l, Control.PRESET_CENTER_TOP, -400, 8, 400, 52)
	clock_l = _label(26, HORIZONTAL_ALIGNMENT_CENTER)
	_place(clock_l, Control.PRESET_CENTER_TOP, -300, 52, 300, 86)
	status_l = _label(22, HORIZONTAL_ALIGNMENT_CENTER)
	_place(status_l, Control.PRESET_CENTER_TOP, -500, 88, 500, 118)
	abil_l = _label(20, HORIZONTAL_ALIGNMENT_LEFT)
	_place(abil_l, Control.PRESET_BOTTOM_LEFT, 18, -128, 640, -10)
	info_l = _label(16, HORIZONTAL_ALIGNMENT_RIGHT)
	_place(info_l, Control.PRESET_BOTTOM_RIGHT, -760, -36, -16, -8)

	cross = ColorRect.new()
	cross.color = Color(1, 1, 1, 0.85)
	root.add_child(cross)
	_place(cross, Control.PRESET_CENTER, -3, -3, 3, 3)

	meter_bg = ColorRect.new()
	meter_bg.color = Color(0, 0, 0, 0.55)
	root.add_child(meter_bg)
	_place(meter_bg, Control.PRESET_CENTER_BOTTOM, -160, -150, 160, -132)
	var full: float = AthleteScript.SHOT_AUTO_RELEASE
	meter_zone = ColorRect.new()
	meter_zone.color = Color(0.2, 0.9, 0.4, 0.55)
	meter_zone.position = Vector2(320 * game.GOOD_LO / full, 0)
	meter_zone.size = Vector2(320 * (game.GOOD_HI - game.GOOD_LO) / full, 18)
	meter_bg.add_child(meter_zone)
	meter_perfect = ColorRect.new()
	meter_perfect.color = Color(1.0, 0.95, 0.4, 0.8)
	meter_perfect.position = Vector2(320 * game.PERFECT_LO / full, 0)
	meter_perfect.size = Vector2(320 * (game.PERFECT_HI - game.PERFECT_LO) / full, 18)
	meter_bg.add_child(meter_perfect)
	meter_pct = Label.new()
	meter_pct.add_theme_font_size_override("font_size", 24)
	meter_pct.add_theme_color_override("font_outline_color", Color(0, 0, 0))
	meter_pct.add_theme_constant_override("outline_size", 6)
	meter_pct.position = Vector2(0, -34)
	meter_pct.size = Vector2(320, 30)
	meter_pct.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	meter_bg.add_child(meter_pct)
	meter_fill = ColorRect.new()
	meter_fill.color = Color(1, 1, 1, 0.9)
	meter_fill.size = Vector2(0, 18)
	meter_bg.add_child(meter_fill)

	flash_rect = ColorRect.new()
	flash_rect.color = Color(1, 1, 1, 0)
	flash_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(flash_rect)
	_place(flash_rect, Control.PRESET_FULL_RECT, 0, 0, 0, 0)
	big_l = _label(64, HORIZONTAL_ALIGNMENT_CENTER)
	_place(big_l, Control.PRESET_CENTER, -500, -235, 500, -155)
	big_l.add_theme_constant_override("outline_size", 12)
	big_l.modulate.a = 0.0
	big_sub = _label(30, HORIZONTAL_ALIGNMENT_CENTER)
	_place(big_sub, Control.PRESET_CENTER, -500, -155, 500, -115)
	big_sub.modulate.a = 0.0

	popups = VBoxContainer.new()
	popups.alignment = BoxContainer.ALIGNMENT_END
	popups.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(popups)
	_place(popups, Control.PRESET_CENTER, -450, -112, 450, -14)

	pause_panel = PanelContainer.new()
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(0.03, 0.05, 0.09, 0.9)
	sb.border_color = Color(0.2, 0.9, 0.9)
	sb.set_border_width_all(2)
	sb.set_corner_radius_all(10)
	sb.content_margin_left = 24
	sb.content_margin_right = 24
	sb.content_margin_top = 16
	sb.content_margin_bottom = 16
	pause_panel.add_theme_stylebox_override("panel", sb)
	pause_l = Label.new()
	pause_l.add_theme_font_size_override("font_size", 17)
	pause_l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	pause_panel.add_child(pause_l)
	root.add_child(pause_panel)
	_place(pause_panel, Control.PRESET_CENTER, -560, -330, 560, 330)

	pad_l = _label(22, HORIZONTAL_ALIGNMENT_CENTER)
	_place(pad_l, Control.PRESET_CENTER_BOTTOM, -450, -44, 450, -12)
	pad_l.add_theme_color_override("font_color", Color(1, 0.7, 0.3))


func _place(c: Control, preset: int, l: float, t: float, r: float, b: float) -> void:
	c.set_anchors_preset(preset)
	c.offset_left = l
	c.offset_top = t
	c.offset_right = r
	c.offset_bottom = b


func _label(size: int, align: int) -> Label:
	var l := Label.new()
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_outline_color", Color(0, 0, 0))
	l.add_theme_constant_override("outline_size", 6)
	l.horizontal_alignment = align
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(l)
	return l


func popup(text: String, c := Color.WHITE) -> void:
	if game.autotest:
		return
	var l := Label.new()
	l.text = text
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	l.add_theme_font_size_override("font_size", 30)
	l.add_theme_color_override("font_color", c)
	l.add_theme_color_override("font_outline_color", Color(0, 0, 0))
	l.add_theme_constant_override("outline_size", 8)
	popups.add_child(l)
	while popups.get_child_count() > 3:
		popups.get_child(0).queue_free()
		popups.remove_child(popups.get_child(0))
	var tw := l.create_tween()
	tw.tween_interval(1.1)
	tw.tween_property(l, "modulate:a", 0.0, 0.5)
	tw.tween_callback(l.queue_free)


func flash(c: Color) -> void:
	if game.autotest:
		return
	flash_rect.color = c
	var tw := flash_rect.create_tween()
	tw.set_ignore_time_scale(true)
	tw.tween_property(flash_rect, "color:a", 0.0, 0.45)


func big(text: String, sub: String, c: Color) -> void:
	if game.autotest:
		return
	big_l.text = text
	big_sub.text = sub
	big_l.add_theme_color_override("font_color", c)
	big_sub.add_theme_color_override("font_color", c.lightened(0.3))
	for l in [big_l, big_sub]:
		l.modulate.a = 1.0
		l.scale = Vector2(1.0, 1.0)
		var tw: Tween = l.create_tween()
		tw.set_ignore_time_scale(true)
		tw.tween_interval(0.9)
		tw.tween_property(l, "modulate:a", 0.0, 0.4)


func _cd_text(name: String, v: float) -> String:
	return "%s 使用可" % name if v <= 0.0 else "%s %.1f秒" % [name, v]


func refresh() -> void:
	if game.human == null:
		return
	var h = game.human
	score_l.text = "YOU  %d  -  %d  BOT      （%d点先取）" % [game.score[0], game.score[1], game.TARGET_SCORE]
	clock_l.text = "ショットクロック %.1f" % max(game.shot_clock, 0.0)
	var r: int = h.role()
	var st := ""
	if r == 1:
		st = "攻撃中"
	elif r == 2:
		st = "守備中"
	else:
		st = "ルーズボール"
	if game.needs_clear:
		st += "   ▶ %s はアーク外へクリアが必要" % game.team_name(game.possession)
	status_l.text = st
	if r == 2:
		abil_l.text = "[LB] %s\n[RB長押し] %s\n[X] スティール   [B] %s\n[A] ジャンプでブロック" % [_cd_text("タックル", h.cd.tackle), _cd_text("シュート妨害", h.cd.contest), _cd_text("追走", h.cd.step)]
	else:
		abil_l.text = "[LB] %s   [RB] %s\nRT: ジャンプショット ／ ↑+RT: レイアップ・フローター\n↓+RT: ステップバック ／ A→空中で↓+RT: フェイダウェイ\nLB突破→RT: ダンク・ユーロステップ" % [_cd_text("突破", h.cd.dash), _cd_text("かわし", h.cd.dodge)]
	var charging: bool = h.state == S.SHOOT
	meter_bg.visible = charging
	if charging:
		meter_fill.size.x = clamp(h.shoot_charge / AthleteScript.SHOT_AUTO_RELEASE, 0.0, 1.0) * 320.0
		var sc: Dictionary = game.shot_chance(h, h.shoot_charge, false)
		var pct := int(sc.p * 100.0)
		var note := ""
		if sc.kind == "contested":
			note = "  妨害されている！"
		elif sc.kind == "free" or sc.kind == "open":
			note = "  フリー"
		meter_pct.text = "%s  成功率 %d%%%s" % [sc.name, pct, note]
		meter_pct.add_theme_color_override("font_color", Color(0.4, 1.0, 0.5) if pct >= 50 else (Color(1, 0.9, 0.4) if pct >= 30 else Color(1, 0.4, 0.4)))
	info_l.text = last_shot_info
	pause_panel.visible = game.paused
	abil_l.visible = not game.paused
	info_l.visible = not game.paused
	if game.paused:
		pause_l.text = _pause_text()
	var no_pad := Input.get_connected_joypads().is_empty()
	pad_l.visible = no_pad and OS.has_feature("mobile")
	pad_l.text = "コントローラーを接続してください（Bluetooth / USB）"


func _pause_text() -> String:
	var g = game
	var t := "TaG プロトタイプ v0.3 ── 1on1 アビリティじゃんけん\n"
	if g.game_over:
		t = "試合終了  YOU %d - %d BOT     BOTのクセ: %s\n" % [g.score[0], g.score[1], g.bot.brain.habit_text()]
	t += "[A] %s  [Y] リスタート  [BACK] コート %.2fx  [X] BOT: %s  [LB] カメラ: %s\n\n" % ["新しい試合" if g.game_over else "プレイ", g.court_scale, g.SKILL_NAMES[g.skill_idx], "三人称" if g.camera_tps else "一人称"]
	t += "■ シュート（RT長押し→リングに自動照準→緑で離す。黄色はパーフェクト）  ↑＝リングへ向かう ／ ↓＝リングから離れる\n"
	t += "RT＝ジャンプショット ／ ↑+RT＝レイアップ（近い）・フローター（中距離） ／ ↓+RT＝ステップバック\n"
	t += "A→空中で↓+RT＝フェイダウェイ（寄せとブロックに強い） ／ LB突破→RT＝ダンク（近い）・ユーロステップ\n\n"
	t += "■ じゃんけん  攻撃 LB突破・RBかわし ／ 守備 LBタックル・RB長押しシュート妨害・Xスティール・B追走\n"
	t += "突破＞妨害 ／ シュート＞間合い ／ かわし＞タックル ／ タックル＞突破 ／ 妨害＞シュート ／ 間合い＞かわし\n"
	t += "読み勝つと相手は1秒以上動けず、自分は2秒間ゾーン。BOTには試合ごとのクセがあり、弱いBOTは技の前に構えが見える\n"
	t += "3x3方式：アーク内1点・外2点、11点先取。ボールを奪ったらアーク外へクリアしてから攻める\n\n"
	t += "■ この試合の計測\n"
	t += "YOU: 攻撃 %d回 / %d点 / 1回あたり %.2f点（%d本中%d本成功）   BOT: 攻撃 %d回 / %d点 / 1回あたり %.2f点（%d本中%d本成功）\n" % [g.stats.poss[0], g.stats.pts[0], g.ppp(0), g.stats.shots[0], g.stats.makes[0], g.stats.poss[1], g.stats.pts[1], g.ppp(1), g.stats.shots[1], g.stats.makes[1]]
	t += "読み勝ち YOU %d / BOT %d ／ スティール %d ／ ブロック %d\n" % [g.stats.reads[0], g.stats.reads[1], g.stats.steal, g.stats.block]
	var ty := ""
	for k in g.stats.types.keys():
		ty += "%s %d/%d  " % [AthleteScript.SHOT_TYPES[k].name, g.stats.types[k][1], g.stats.types[k][0]]
	if ty != "":
		t += "シュートの種類（成功/本数・両チーム合計）: " + ty
	return t
