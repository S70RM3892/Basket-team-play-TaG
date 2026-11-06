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
	_place(abil_l, Control.PRESET_BOTTOM_LEFT, 18, -128, 560, -10)
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
	meter_zone = ColorRect.new()
	meter_zone.color = Color(0.2, 0.9, 0.4, 0.6)
	meter_zone.position = Vector2(320 * 0.70 / 1.45, 0)
	meter_zone.size = Vector2(320 * 0.20 / 1.45, 18)
	meter_bg.add_child(meter_zone)
	meter_fill = ColorRect.new()
	meter_fill.color = Color(1, 1, 1, 0.9)
	meter_fill.size = Vector2(0, 18)
	meter_bg.add_child(meter_fill)

	popups = VBoxContainer.new()
	popups.alignment = BoxContainer.ALIGNMENT_END
	popups.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(popups)
	_place(popups, Control.PRESET_CENTER, -450, -190, 450, -60)

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
	pause_l.add_theme_font_size_override("font_size", 19)
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
		abil_l.text = "[LB] %s\n[RB] %s\n[RT/Y] シュート（緑で離す）\n[A] ジャンプ" % [_cd_text("突破", h.cd.dash), _cd_text("かわし", h.cd.dodge)]
	var charging: bool = h.state == S.SHOOT
	meter_bg.visible = charging
	if charging:
		meter_fill.size.x = clamp(h.shoot_charge / 1.45, 0.0, 1.0) * 320.0
	info_l.text = last_shot_info
	pause_panel.visible = game.paused
	if game.paused:
		pause_l.text = _pause_text()
	var no_pad := Input.get_connected_joypads().is_empty()
	pad_l.visible = no_pad and OS.has_feature("mobile")
	pad_l.text = "コントローラーを接続してください（Bluetooth / USB）"


func _pause_text() -> String:
	var g = game
	var head := "TaG プロトタイプ v0.1 ── 1on1 アビリティじゃんけん\n"
	if g.game_over:
		head = "試合終了  YOU %d - %d BOT\n" % [g.score[0], g.score[1]]
	var t := head
	t += "[A] %s   [Y] リスタート   [BACK] コート倍率 %.2fx   [X] BOT強さ %.1f\n\n" % ["新しい試合" if g.game_over else "プレイ", g.court_scale, g.SKILLS[g.skill_idx]]
	t += "■ 操作（コントローラー）\n"
	t += "左スティック: 移動 ／ 右スティック: 視点 ／ A: ジャンプ（ブロック） ／ START: ポーズ\n"
	t += "攻撃: RT（またはY）長押し→緑で離してシュート ／ LB: 突破 ／ RB: かわし\n"
	t += "守備: LB: タックル ／ RB長押し: シュート妨害 ／ X: スティール ／ B（またはLT）: 追走ステップ\n"
	t += "（PC: WASD・マウス・左クリック=シュート・Q・E・F・Shift・Space・Esc）\n\n"
	t += "■ じゃんけん\n"
	t += "突破 ＞ シュート妨害 ／ シュート ＞ 間合い ／ かわし ＞ タックル\n"
	t += "タックル ＞ 突破 ／ シュート妨害 ＞ シュート ／ 間合い ＞ かわし\n"
	t += "相手の腰のリングが光っていればアビリティ使用可（上=LB系、下=RB系）\n"
	t += "ルール: 3x3方式。アーク内1点・外2点。守備側がボールを取ったらアーク外へクリアしてから攻める\n\n"
	t += "■ この試合の計測\n"
	t += "YOU: 攻撃 %d回 / %d点 / 1回あたり %.2f点（シュート %d本・成功 %d）\n" % [g.stats.poss[0], g.stats.pts[0], g.ppp(0), g.stats.shots[0], g.stats.makes[0]]
	t += "BOT: 攻撃 %d回 / %d点 / 1回あたり %.2f点（シュート %d本・成功 %d）\n" % [g.stats.poss[1], g.stats.pts[1], g.ppp(1), g.stats.shots[1], g.stats.makes[1]]
	t += "タックル＞突破 %d ／ 突破＞妨害 %d ／ かわし＞タックル %d ／ 間合い＞かわし %d\n" % [g.stats.tackle_beats_dash, g.stats.dash_beats_contest, g.stats.dodge_beats_tackle, g.stats.dodge_read]
	t += "ノーマーク %d ／ 妨害されたシュート %d ／ 当たられながら %d ／ スティール %d ／ ブロック %d" % [g.stats.open_shot, g.stats.contested_shot, g.stats.contact_shot, g.stats.steal, g.stats.block]
	return t
