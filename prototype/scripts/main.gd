extends Node3D
## 試合の進行（3x3ルールの簡易版）、コートの生成、入力設定、計測。

const AthleteScript = preload("res://scripts/athlete.gd")
const BallScript = preload("res://scripts/ball.gd")
const BrainScript = preload("res://scripts/ai_brain.gd")
const HudScript = preload("res://scripts/hud.gd")
const S = AthleteScript.S
const B = BallScript.B

const RIM_Y := 3.05
const RIM_Z := 1.575
const SCALES := [1.0, 1.15, 1.3]
const SKILLS := [0.2, 0.4, 0.7, 1.0]
const SKILL_NAMES := ["練習", "ふつう", "強い", "鬼"]
const TARGET_SCORE := 11
const SHOT_CLOCK := 12.0

var scale_idx := 1
var skill_idx := 1
var camera_tps := false
var hitstop_until := 0
var slowmo_until := 0
var court_scale := 1.15
var athletes: Array = []
var human
var bot
var ball
var hud
var world_root: Node3D
var sfx_bank := {}
var ui_sfx: AudioStreamPlayer

var score := [0, 0]
var possession := 0
var needs_clear := false
var shot_clock := SHOT_CLOCK
var paused := true
var game_over := false
var dead_ball_t := -1.0
var dead_ball_next := 0
var mouse_delta := Vector2.ZERO
var autotest := false
var autotest_time := 0.0
var shottest := false
var planned_pending: Array = []
var snap_path := ""
var snap_t := 0.0
var snap_play := false
var snap_demo := false
var snap_step := false
var failed_makes: Array = []
var st_list: Array = []
var st_i := -1
var st_t := 0.0
var st_made := 0
var st_res: Array = []
var stats := {}


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	autotest = "--autotest" in OS.get_cmdline_user_args()
	shottest = "--shottest" in OS.get_cmdline_user_args()
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--snap="):
			snap_path = arg.substr(7)
		if arg == "--snap-play":
			snap_play = true
		if arg == "--snap-step":
			snap_play = true
			snap_step = true
		if arg == "--snap-tps":
			camera_tps = true
		if arg == "--snap-demo":
			snap_play = true
			snap_demo = true
	if shottest:
		autotest = true
	_setup_input()
	_build_sfx()
	hud = HudScript.new()
	add_child(hud)
	hud.setup(self)
	new_game()
	if shottest:
		paused = false
		for d in [-1.0, 0.8, 1.5, 3.0, 4.5, 6.0, 7.5, 9.0]:
			for k in 4:
				st_list.append(d)
	elif autotest:
		paused = false
		Engine.time_scale = 2.0
		print("[autotest] start scale=", court_scale)


# ───────── 入力 ─────────
func _setup_input() -> void:
	_act("move_left", [_axis(JOY_AXIS_LEFT_X, -1.0), _key(KEY_A)])
	_act("move_right", [_axis(JOY_AXIS_LEFT_X, 1.0), _key(KEY_D)])
	_act("move_fwd", [_axis(JOY_AXIS_LEFT_Y, -1.0), _key(KEY_W)])
	_act("move_back", [_axis(JOY_AXIS_LEFT_Y, 1.0), _key(KEY_S)])
	_act("look_left", [_axis(JOY_AXIS_RIGHT_X, -1.0)])
	_act("look_right", [_axis(JOY_AXIS_RIGHT_X, 1.0)])
	_act("look_up", [_axis(JOY_AXIS_RIGHT_Y, -1.0)])
	_act("look_down", [_axis(JOY_AXIS_RIGHT_Y, 1.0)])
	_act("jump", [_btn(JOY_BUTTON_A), _key(KEY_SPACE)])
	_act("shoot", [_axis(JOY_AXIS_TRIGGER_RIGHT, 1.0), _btn(JOY_BUTTON_Y), _mouse(MOUSE_BUTTON_LEFT)], 0.35)
	_act("ability1", [_btn(JOY_BUTTON_LEFT_SHOULDER), _key(KEY_Q)])
	_act("ability2", [_btn(JOY_BUTTON_RIGHT_SHOULDER), _key(KEY_E), _mouse(MOUSE_BUTTON_RIGHT)])
	_act("steal", [_btn(JOY_BUTTON_X), _key(KEY_F)])
	_act("step", [_btn(JOY_BUTTON_B), _axis(JOY_AXIS_TRIGGER_LEFT, 1.0), _key(KEY_SHIFT)], 0.35)
	_act("pause", [_btn(JOY_BUTTON_START), _key(KEY_ESCAPE)])
	_act("menu_ok", [_btn(JOY_BUTTON_A), _key(KEY_ENTER), _key(KEY_SPACE)])
	_act("menu_restart", [_btn(JOY_BUTTON_Y), _key(KEY_R)])
	_act("menu_scale", [_btn(JOY_BUTTON_BACK), _key(KEY_C)])
	_act("menu_skill", [_btn(JOY_BUTTON_X), _key(KEY_B)])
	_act("menu_camera", [_btn(JOY_BUTTON_LEFT_SHOULDER), _key(KEY_V)])


func _act(name: String, events: Array, deadzone := 0.2) -> void:
	if InputMap.has_action(name):
		InputMap.erase_action(name)
	InputMap.add_action(name, deadzone)
	for e in events:
		InputMap.action_add_event(name, e)


func _axis(axis: int, v: float) -> InputEventJoypadMotion:
	var e := InputEventJoypadMotion.new()
	e.device = -1
	e.axis = axis
	e.axis_value = v
	return e


func _btn(b: int) -> InputEventJoypadButton:
	var e := InputEventJoypadButton.new()
	e.device = -1
	e.button_index = b
	return e


func _key(k: int) -> InputEventKey:
	var e := InputEventKey.new()
	e.physical_keycode = k
	return e


func _mouse(b: int) -> InputEventMouseButton:
	var e := InputEventMouseButton.new()
	e.button_index = b
	return e


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		mouse_delta += event.relative


# ───────── 効果音（その場で合成） ─────────
func _build_sfx() -> void:
	sfx_bank["tackle"] = _tone(110.0, 0.22, 0.7, 1)
	sfx_bank["dash"] = _tone(900.0, 0.16, 0.35, 2)
	sfx_bank["dodge"] = _tone(1400.0, 0.12, 0.3, 0)
	sfx_bank["contest"] = _tone(330.0, 0.25, 0.4, 1)
	sfx_bank["step"] = _tone(600.0, 0.06, 0.25, 2)
	sfx_bank["whiff"] = _tone(250.0, 0.08, 0.25, 2)
	sfx_bank["down"] = _tone(70.0, 0.3, 0.8, 1)
	sfx_bank["score"] = _tone(880.0, 0.35, 0.4, 0)
	sfx_bank["win"] = _tone(660.0, 0.2, 0.45, 0)
	sfx_bank["lose"] = _tone(180.0, 0.25, 0.45, 1)
	sfx_bank["read"] = _tone(440.0, 0.45, 0.55, 3)
	sfx_bank["perfect"] = _tone(1320.0, 0.18, 0.35, 0)
	ui_sfx = AudioStreamPlayer.new()
	add_child(ui_sfx)


func _tone(freq: float, dur: float, vol: float, kind: int) -> AudioStreamWAV:
	var rate := 22050
	var n := int(rate * dur)
	var data := PackedByteArray()
	data.resize(n * 2)
	var phase := 0.0
	for i in n:
		var t := float(i) / rate
		var env: float = min(1.0, t * 60.0) * pow(1.0 - float(i) / n, 1.5)
		var f := freq * (1.0 - 0.3 * float(i) / n)
		phase += f / rate
		var s := 0.0
		match kind:
			0:
				s = sin(TAU * phase)
			1:
				s = 1.0 if fmod(phase, 1.0) < 0.5 else -1.0
				s *= 0.6
			2:
				s = randf_range(-1.0, 1.0) * 0.8 + sin(TAU * phase) * 0.2
			3:
				var k: float = 1.0 + floor(t / dur * 4.0) * 0.26
				s = sin(TAU * t * freq * k) * 0.6 + sin(TAU * t * freq * k * 1.5) * 0.3
		data.encode_s16(i * 2, int(clamp(s * env * vol, -1.0, 1.0) * 32767.0))
	var w := AudioStreamWAV.new()
	w.format = AudioStreamWAV.FORMAT_16_BITS
	w.mix_rate = rate
	w.stereo = false
	w.data = data
	return w


func play_ui(kind: String) -> void:
	if sfx_bank.has(kind):
		ui_sfx.stream = sfx_bank[kind]
		ui_sfx.play()


# ───────── コート ─────────
func rim_center() -> Vector3:
	return Vector3(0, RIM_Y, RIM_Z)


func rim_floor() -> Vector3:
	return Vector3(0, 0, RIM_Z)


func arc_radius() -> float:
	return 6.75 * court_scale


func half_width() -> float:
	return 7.5 * court_scale


func court_length() -> float:
	return 11.0 * court_scale


func check_point() -> Vector3:
	return Vector3(0, 0, RIM_Z + arc_radius() + 1.0)


func is_out_of_bounds(p: Vector3) -> bool:
	return absf(p.x) > half_width() or p.z < 0.0 or p.z > court_length()


func is_two_point(p: Vector3) -> bool:
	var d := Vector2(p.x, p.z - RIM_Z).length()
	return d > arc_radius() or absf(p.x) > 6.6 * court_scale


func _build_world() -> void:
	if world_root:
		world_root.queue_free()
	world_root = Node3D.new()
	add_child(world_root)
	var env := WorldEnvironment.new()
	var e := Environment.new()
	e.background_mode = Environment.BG_COLOR
	e.background_color = Color(0.06, 0.08, 0.14)
	e.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	e.ambient_light_color = Color(0.55, 0.6, 0.75)
	e.ambient_light_energy = 0.6
	env.environment = e
	world_root.add_child(env)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-55, 35, 0)
	sun.light_energy = 1.1
	sun.shadow_enabled = true
	sun.directional_shadow_max_distance = 35.0
	world_root.add_child(sun)
	var lamp := OmniLight3D.new()
	lamp.position = Vector3(0, 8, court_length() * 0.5)
	lamp.omni_range = 25.0
	lamp.light_energy = 0.8
	lamp.light_color = Color(1.0, 0.9, 0.75)
	world_root.add_child(lamp)

	var W := half_width()
	var L := court_length()
	# 床（コリジョンは広め）
	var floor_body := StaticBody3D.new()
	floor_body.collision_layer = 1
	var fc := CollisionShape3D.new()
	var fs := BoxShape3D.new()
	fs.size = Vector3(80, 1, 80)
	fc.shape = fs
	fc.position = Vector3(0, -0.5, 0)
	floor_body.add_child(fc)
	world_root.add_child(floor_body)
	_box(Vector3(80, 0.02, 80), Vector3(0, -0.01, 0), Color(0.12, 0.13, 0.17))
	_box(Vector3(W * 2, 0.02, L), Vector3(0, 0.0, L * 0.5), Color(0.62, 0.42, 0.26))
	# ペイント
	_box(Vector3(4.9 * court_scale, 0.022, 5.8), Vector3(0, 0.001, 2.9), Color(0.16, 0.42, 0.55))
	# ライン
	var lc := Color(0.95, 0.95, 0.95)
	_box(Vector3(W * 2, 0.03, 0.05), Vector3(0, 0.005, 0.025), lc)
	_box(Vector3(W * 2, 0.03, 0.05), Vector3(0, 0.005, L - 0.025), lc)
	_box(Vector3(0.05, 0.03, L), Vector3(-W + 0.025, 0.005, L * 0.5), lc)
	_box(Vector3(0.05, 0.03, L), Vector3(W - 0.025, 0.005, L * 0.5), lc)
	# 2点ライン（アーク＋コーナーの直線）
	var R := arc_radius()
	var cx := 6.6 * court_scale
	var z_int := RIM_Z + sqrt(max(R * R - cx * cx, 0.0))
	_box(Vector3(0.05, 0.03, z_int), Vector3(-cx, 0.005, z_int * 0.5), lc)
	_box(Vector3(0.05, 0.03, z_int), Vector3(cx, 0.005, z_int * 0.5), lc)
	var a0 := asin(clamp(cx / R, -1.0, 1.0))
	var seg := 40
	for i in seg:
		var t0 := -a0 + 2.0 * a0 * float(i) / seg
		var t1 := -a0 + 2.0 * a0 * float(i + 1) / seg
		var p0 := Vector3(sin(t0) * R, 0.005, RIM_Z + cos(t0) * R)
		var p1 := Vector3(sin(t1) * R, 0.005, RIM_Z + cos(t1) * R)
		var mid := (p0 + p1) * 0.5
		var mi := _box(Vector3(0.05, 0.03, p0.distance_to(p1) + 0.01), mid, lc)
		mi.look_at_from_position(mid, p1, Vector3.UP)
	# チェックの位置
	_box(Vector3(0.6, 0.031, 0.05), check_point() + Vector3(0, 0.005, 0), Color(0.3, 0.95, 0.9))

	# バックボード・リング・支柱
	var board := StaticBody3D.new()
	board.collision_layer = 1
	var bc := CollisionShape3D.new()
	var bs := BoxShape3D.new()
	bs.size = Vector3(1.8, 1.05, 0.05)
	bc.shape = bs
	board.add_child(bc)
	board.position = Vector3(0, 3.425, 1.175)
	world_root.add_child(board)
	var bm := _box(Vector3(1.8, 1.05, 0.05), Vector3(0, 3.425, 1.175), Color(0.85, 0.92, 1.0, 0.35), true)
	bm.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_box(Vector3(0.59, 0.45, 0.052), Vector3(0, 3.275, 1.176), Color(1, 1, 1, 0.0), true)
	_box(Vector3(0.6, 0.04, 0.055), Vector3(0, 3.07, 1.176), Color(1, 1, 1))
	_box(Vector3(0.04, 0.45, 0.055), Vector3(-0.29, 3.27, 1.176), Color(1, 1, 1))
	_box(Vector3(0.04, 0.45, 0.055), Vector3(0.29, 3.27, 1.176), Color(1, 1, 1))
	_box(Vector3(0.6, 0.04, 0.055), Vector3(0, 3.48, 1.176), Color(1, 1, 1))
	_box(Vector3(0.18, 0.18, 1.6), Vector3(0, 3.6, 0.35), Color(0.2, 0.2, 0.25))
	_box(Vector3(0.25, 3.7, 0.25), Vector3(0, 1.85, -0.45), Color(0.2, 0.2, 0.25))
	var rim_body := StaticBody3D.new()
	rim_body.collision_layer = 1
	rim_body.name = "Rim"
	var n := 24
	for i in n:
		var ang := TAU * float(i) / n
		var c := CollisionShape3D.new()
		var sp := SphereShape3D.new()
		sp.radius = 0.02
		c.shape = sp
		c.position = Vector3(cos(ang) * 0.235, 0, sin(ang) * 0.235)
		rim_body.add_child(c)
	rim_body.position = rim_center()
	world_root.add_child(rim_body)
	var rim_mi := MeshInstance3D.new()
	var tm := TorusMesh.new()
	tm.inner_radius = 0.215
	tm.outer_radius = 0.255
	rim_mi.mesh = tm
	rim_mi.material_override = _mat(Color(1.0, 0.35, 0.05))
	rim_mi.position = rim_center()
	world_root.add_child(rim_mi)
	var net := MeshInstance3D.new()
	var cyl := CylinderMesh.new()
	cyl.top_radius = 0.225
	cyl.bottom_radius = 0.15
	cyl.height = 0.42
	cyl.cap_top = false
	cyl.cap_bottom = false
	net.mesh = cyl
	var nm := _mat(Color(1, 1, 1, 0.35), true)
	nm.cull_mode = BaseMaterial3D.CULL_DISABLED
	net.material_override = nm
	net.position = rim_center() + Vector3(0, -0.22, 0)
	world_root.add_child(net)

	# 選手用の見えない柵（ボールは通す）
	var wall_body := StaticBody3D.new()
	wall_body.collision_layer = 8
	for spec in [[Vector3(0.2, 4, L + 6), Vector3(-W - 1.5, 2, L * 0.5)], [Vector3(0.2, 4, L + 6), Vector3(W + 1.5, 2, L * 0.5)], [Vector3(W * 2 + 3, 4, 0.2), Vector3(0, 2, -1.0)], [Vector3(W * 2 + 3, 4, 0.2), Vector3(0, 2, L + 1.5)]]:
		var c := CollisionShape3D.new()
		var s := BoxShape3D.new()
		s.size = spec[0]
		c.shape = s
		c.position = spec[1]
		wall_body.add_child(c)
	world_root.add_child(wall_body)
	# 金網（雰囲気）
	var fence := Color(0.25, 0.3, 0.38, 0.5)
	_box(Vector3(0.05, 3.5, L + 4), Vector3(-W - 2.0, 1.75, L * 0.5), fence, true)
	_box(Vector3(0.05, 3.5, L + 4), Vector3(W + 2.0, 1.75, L * 0.5), fence, true)
	_box(Vector3(W * 2 + 4, 3.5, 0.05), Vector3(0, 1.75, -1.6), fence, true)
	# ネオン（ナギサの雰囲気）
	_neon(Vector3(W * 2 + 4, 0.08, 0.08), Vector3(0, 3.5, -1.6), Color(0.2, 0.95, 0.9))
	_neon(Vector3(0.08, 0.08, L + 4), Vector3(-W - 2.0, 3.5, L * 0.5), Color(1.0, 0.35, 0.6))
	_neon(Vector3(0.08, 0.08, L + 4), Vector3(W + 2.0, 3.5, L * 0.5), Color(1.0, 0.35, 0.6))


func _mat(c: Color, transparent := false) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = c
	if transparent:
		m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	return m


func _box(size: Vector3, pos: Vector3, c: Color, transparent := false) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = size
	mi.mesh = bm
	mi.material_override = _mat(c, transparent)
	mi.position = pos
	world_root.add_child(mi)
	return mi


func _neon(size: Vector3, pos: Vector3, c: Color) -> void:
	var mi := _box(size, pos, c)
	var m: StandardMaterial3D = mi.material_override
	m.emission_enabled = true
	m.emission = c
	m.emission_energy_multiplier = 3.0
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED


# ───────── 試合の開始 ─────────
func new_game() -> void:
	court_scale = SCALES[scale_idx]
	for a in athletes:
		a.queue_free()
	athletes.clear()
	if ball:
		ball.queue_free()
	_build_world()
	human = AthleteScript.new()
	add_child(human)
	human.setup(self, 0, not autotest, Color(0.2, 0.85, 0.95))
	bot = AthleteScript.new()
	add_child(bot)
	bot.setup(self, 1, false, Color(1.0, 0.35, 0.3))
	bot.brain = BrainScript.new(SKILLS[skill_idx])
	if autotest:
		human.brain = BrainScript.new(0.7)
		var cam := Camera3D.new()
		add_child(cam)
		cam.position = Vector3(0, 9, court_length() + 4)
		cam.look_at(Vector3(0, 0, court_length() * 0.4))
	athletes = [human, bot]
	ball = BallScript.new()
	add_child(ball)
	ball.setup(self)
	score = [0, 0]
	game_over = false
	stats = {
		"poss": [0, 0], "pts": [0, 0], "shots": [0, 0], "makes": [0, 0],
		"tackle_beats_dash": 0, "tackle_hit": 0, "dash_beats_contest": 0, "dash_tie": 0, "dash_clear": 0,
		"dodge_beats_tackle": 0, "dodge_read": 0, "open_shot": 0, "contested_shot": 0, "contact_shot": 0,
		"steal": 0, "block": 0, "turnover": 0, "exp_makes": 0.0, "planned_makes": 0, "reads": [0, 0], "types": {},
	}
	give_ball(0)


func give_ball(team: int) -> void:
	var holder = human if team == 0 else bot
	var other = bot if team == 0 else human
	var cp := check_point()
	holder.reset_for_play(cp, rim_floor())
	var gpos := cp + (rim_floor() - cp).normalized() * 2.0
	other.reset_for_play(gpos, cp)
	ball.attach(holder)
	possession = team
	needs_clear = false
	shot_clock = SHOT_CLOCK
	dead_ball_t = -1.0
	stats.poss[team] += 1


func opponent_of(a):
	return bot if a == human else human


func team_name(t: int) -> String:
	return "YOU" if t == 0 else "BOT"


# ───────── 進行 ─────────
func _process(_delta: float) -> void:
	if not autotest:
		var now := Time.get_ticks_msec()
		if now < hitstop_until:
			Engine.time_scale = 0.05
		elif now < slowmo_until:
			Engine.time_scale = 0.4
		else:
			Engine.time_scale = 1.0
	if snap_path != "":
		snap_t += _delta
		if snap_play and paused and snap_t > 0.3:
			set_paused(false)
		if snap_demo and snap_t > 1.9 and not Input.is_action_pressed("shoot"):
			Input.action_press("shoot")
		if snap_demo and snap_t > 2.25 and stats.reads[0] == 0:
			on_read_win(human, bot, "かわし ＞ タックル")
		if snap_step and snap_t > 1.95 and not Input.is_action_pressed("shoot"):
			Input.action_press("move_back")
			Input.action_press("shoot")
		if snap_t > 2.6:
			get_viewport().get_texture().get_image().save_png(snap_path)
			print("[snap] saved ", snap_path)
			get_tree().quit()
			return
	if Input.is_action_just_pressed("pause") and not autotest:
		set_paused(not paused)
	if paused and not autotest:
		if Input.is_action_just_pressed("menu_ok"):
			if game_over:
				new_game()
			set_paused(false)
		elif Input.is_action_just_pressed("menu_restart"):
			new_game()
			set_paused(false)
		elif Input.is_action_just_pressed("menu_scale"):
			scale_idx = (scale_idx + 1) % SCALES.size()
			new_game()
		elif Input.is_action_just_pressed("menu_skill"):
			skill_idx = (skill_idx + 1) % SKILLS.size()
			new_game()
		elif Input.is_action_just_pressed("menu_camera"):
			camera_tps = not camera_tps
	hud.refresh()


func set_paused(p: bool) -> void:
	paused = p
	if not OS.has_feature("mobile"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE if p else Input.MOUSE_MODE_CAPTURED


func _physics_process(delta: float) -> void:
	if shottest:
		_shottest_step(delta)
		return
	if autotest:
		autotest_time += delta
		if autotest_time > 240.0 or game_over:
			_print_report()
			get_tree().quit()
			return
	if paused or game_over:
		return
	if dead_ball_t >= 0.0:
		dead_ball_t -= delta
		if dead_ball_t < 0.0:
			give_ball(dead_ball_next)
		return
	# ショットクロック
	if ball.state == B.HELD:
		shot_clock -= delta
		if shot_clock <= 0.0:
			hud.popup("ショットクロック違反", Color(1, 0.8, 0.3))
			stats.turnover += 1
			_dead_ball(1 - possession, 0.8)
			return
		if needs_clear and ball.holder.team == possession and is_two_point(ball.holder.flat_pos()):
			needs_clear = false
			hud.popup("クリア！", Color(0.6, 1.0, 0.9))
	# ブロック判定
	if ball.state == B.FLIGHT and ball.shot_time < 0.35 and not ball.blocked:
		for a in athletes:
			if a.team == ball.shot_team or a.is_on_floor():
				continue
			if ball.linear_velocity.y > 0.0 and a.hand_reach_point().distance_to(ball.global_position) < 0.32 * ball.block_mult:
				if not a.is_human and (ball.block_rolled or randf() > (0.25 + 0.3 * a.brain.skill) * ball.block_mult):
					ball.block_rolled = true
					continue
				ball.blocked = true
				var away: Vector3 = (ball.global_position - rim_center())
				away.y = 0
				ball.state = B.LOOSE
				ball.loose_t = 0.0
				ball.last_touch_team = a.team
				ball.launch(away.normalized() * 3.5 + Vector3(randf_range(-1.5, 1.5), 1.0, 0))
				stats.block += 1
				hud.popup("ブロック！ " + team_name(a.team), _col(a.team))
				play_ui("win" if a.team == 0 else "lose")
				break
	# ルーズボールの確保
	if ball.state == B.LOOSE or (ball.state == B.FLIGHT and ball.shot_time > 0.4):
		for a in athletes:
			if a.state == S.DOWN:
				continue
			if a == ball.shooter and ball.state == B.FLIGHT:
				continue
			var fp: Vector3 = a.flat_pos()
			var bp: Vector3 = ball.global_position
			var hd := Vector2(bp.x - fp.x, bp.z - fp.z).length()
			var reach := 2.7 if not a.is_on_floor() else 2.2
			if hd < 0.75 and bp.y < reach:
				_pickup(a)
				break


func _pickup(a) -> void:
	var prev := possession
	ball.attach(a)
	if a.team != prev:
		possession = a.team
		needs_clear = true
		shot_clock = SHOT_CLOCK
		stats.poss[a.team] += 1
		hud.popup("%s がボール確保 → アーク外へクリア" % team_name(a.team), _col(a.team))
	else:
		shot_clock = SHOT_CLOCK
	ball.shooter = null


func _dead_ball(next_team: int, t := 1.2) -> void:
	ball.state = B.DEAD
	ball.freeze = true
	ball.holder = null
	dead_ball_next = next_team
	dead_ball_t = t


func _col(t: int) -> Color:
	return Color(0.3, 0.95, 1.0) if t == 0 else Color(1.0, 0.45, 0.4)


# ───────── じゃんけんの結果 ─────────
func on_read_win(w, l, rule: String) -> void:
	## 読み勝ちのご褒美：ヒットストップ → スロー、画面のフラッシュ、勝った側は「ゾーン」
	w.zone_t = AthleteScript.ZONE_T
	stats.reads[w.team] += 1
	if autotest:
		return
	var now := Time.get_ticks_msec()
	hitstop_until = now + 90
	slowmo_until = now + 90 + 380
	if w.team == 0:
		hud.big("読み勝ち！", rule, Color(0.35, 1.0, 0.95))
		hud.flash(Color(0.4, 1.0, 0.95, 0.35))
		play_ui("read")
	else:
		hud.big("読まれた…", rule, Color(1.0, 0.45, 0.4))
		hud.flash(Color(1.0, 0.2, 0.2, 0.3))
		play_ui("lose")


func resolve_tackle(t, o) -> void:
	if o.state == S.DODGE:
		# かわし ＞ タックル：タックルした側は長く動けない
		o.dodge_evaded = true
		t.recover_len = 1.0
		stats.dodge_beats_tackle += 1
		on_read_win(o, t, "かわし ＞ タックル")
		return
	if o.state == S.DOWN:
		return
	t.recover_len = 0.3
	t._enter(S.TACKLE_RECOVER)
	if o.state == S.SHOOT and o.dash_cancel_t <= 0.0:
		o.shoot_contact = true
		o._release_shot()
		stats.contact_shot += 1
		hud.popup("当たられながら打った（あいこ）", Color(1, 1, 0.6))
		return
	var had_ball: bool = ball.holder == o
	var was_dash: bool = o.state == S.DASH or o.state == S.DASH_START or (o.state == S.SHOOT and o.dash_cancel_t > 0.0)
	if had_ball and not was_dash and o.state != S.SHOOT:
		# 突破していない相手へのタックルは「当たり負け」させるだけ（ボールは奪えない）
		o.velocity += t.forward() * 2.5
		o.stagger(0.35)
		stats.tackle_hit += 1
		hud.popup("当たっただけ（タックルは突破に刺す技）", Color(1, 1, 0.6))
		return
	o.knock_down(t.forward())
	stats.tackle_hit += 1
	if had_ball:
		# タックル ＞ 突破：ボールはそのままタックルした側のものになる
		stats.turnover += 1
		ball.last_touch_team = t.team
		_pickup(t)
	if was_dash:
		stats.tackle_beats_dash += 1
		on_read_win(t, o, "タックル ＞ 突破")
	else:
		hud.popup("タックル成功！", _col(t.team))


func on_dash_beats_contest(d, o) -> void:
	# 突破 ＞ シュート妨害：妨害した側は1.2秒動けない
	stats.dash_beats_contest += 1
	o.stagger(1.2)
	on_read_win(d, o, "突破 ＞ シュート妨害")


func on_dash_end(d) -> void:
	if d.dash_beat_done or ball.holder != d:
		return
	var o = opponent_of(d)
	var d_rim: float = (rim_floor() - d.flat_pos()).length()
	var o_rim: float = (rim_floor() - o.flat_pos()).length()
	var dist: float = (o.flat_pos() - d.flat_pos()).length()
	if dist < 2.0 and o_rim < d_rim:
		stats.dash_tie += 1
		hud.popup("止められた（あいこ）", Color(1, 1, 0.6))
	elif o_rim > d_rim + 0.4:
		stats.dash_clear += 1
		hud.popup("抜いた！", _col(d.team))


func on_dodge_read(d) -> void:
	# 間合い ＞ かわし：我慢していた守備が、隙だらけのボールを自動で奪う
	var o = opponent_of(d)
	var dist: float = (o.flat_pos() - d.flat_pos()).length()
	if ball.holder != d:
		return
	if dist < 2.2 and o.state == S.NORMAL:
		stats.dodge_read += 1
		on_read_win(o, d, "間合い ＞ かわし")
		steal(o, d)
	elif dist < 3.0:
		hud.popup("かわしが空振り（相手は遠かった）", Color(1, 1, 0.6))


func steal(a, h) -> void:
	ball.last_touch_team = a.team
	stats.steal += 1
	stats.turnover += 1
	hud.popup("スティール！ " + team_name(a.team), _col(a.team))
	play_ui("win" if a.team == 0 else "lose")
	_pickup(a)


# ───────── シュート ─────────
func on_shot_start(_a) -> void:
	pass


const GOOD_LO := 0.52
const GOOD_HI := 0.94
const PERFECT_LO := 0.68
const PERFECT_HI := 0.80


func timing_quality(c: float) -> float:
	if c >= PERFECT_LO and c <= PERFECT_HI:
		return 1.12
	if c >= GOOD_LO and c <= GOOD_HI:
		return 1.0
	if c < GOOD_LO:
		return lerp(0.7, 1.0, clamp(c / GOOD_LO, 0.0, 1.0))
	return clamp(lerp(1.0, 0.7, (c - GOOD_HI) / 0.36), 0.65, 1.0)


func shot_chance(a, charge: float, contact: bool) -> Dictionary:
	## シュートの成功率。照準はロックオンするので、腕前はタイミングと「どこで・誰に寄られて打つか」
	var fp: Vector3 = a.flat_pos()
	var dist := Vector2(fp.x - rim_floor().x, fp.z - rim_floor().z).length()
	var nd := dist / court_scale
	var base: float
	if nd < 1.3:
		base = 0.92
	elif nd < 6.75:
		base = lerp(0.78, 0.6, (nd - 1.3) / 5.45)
	elif nd < 9.5:
		base = lerp(0.55, 0.35, (nd - 6.75) / 2.75)
	else:
		base = 0.1
	var st: Dictionary = AthleteScript.SHOT_TYPES[a.shot_type]
	if a.shot_type == "dunk":
		base = 0.96
	else:
		base *= float(st.mult)
	var q := timing_quality(charge)
	var cont := 1.0
	var kind := ""
	var o = opponent_of(a)
	var od: float = (o.flat_pos() - fp).length()
	var facing: float = o.forward().dot((fp - o.flat_pos()).normalized())
	var o_active: bool = o.state != S.STAGGER and o.state != S.DOWN and o.state != S.TACKLE_RECOVER
	if o.state == S.CONTEST and od < AthleteScript.CONTEST_RANGE and facing > 0.2:
		cont = 0.3
		kind = "contested"
	elif o.state == S.CONTEST_START and od < AthleteScript.CONTEST_RANGE:
		cont = 0.6
	elif not o_active or a.zone_t > 0.0:
		cont = 1.0
		kind = "free"
	elif od < 1.1:
		cont = 0.75
	elif od < 1.8:
		cont = 0.88
	elif od < 2.3:
		cont = 0.96
	elif od < 5.0:
		kind = "open"
	if kind == "" or kind == "open":
		# 寄せの効きやすさはシュートの種類で変わる（フェイダウェイやユーロステップは寄せに強い）
		cont = 1.0 - (1.0 - cont) * float(st.prox)
	var zone := 1.15 if a.zone_t > 0.0 else 1.0
	var contact_m := 0.55 if contact else 1.0
	var p: float = clamp(base * q * cont * contact_m * zone, 0.02, 0.97)
	return {"p": p, "q": q, "cont": cont * contact_m, "kind": kind, "nd": nd, "zone": zone, "name": st.name}


func release_shot(a, charge: float, contact: bool) -> void:
	var fp: Vector3 = a.flat_pos()
	var sc := shot_chance(a, charge, contact)
	var p: float = sc.p
	var q: float = sc.q
	var nd: float = sc.nd
	var o = opponent_of(a)
	var label := ""
	if sc.kind == "contested":
		stats.contested_shot += 1
		on_read_win(o, a, "シュート妨害 ＞ シュート")
	elif sc.kind == "open":
		stats.open_shot += 1
		label = "ノーマーク！  シュート ＞ 間合い"
	var st: Dictionary = AthleteScript.SHOT_TYPES[a.shot_type]
	if a.shot_type != "jumper":
		hud.popup(str(st.name) + "！", _col(a.team))
	if not stats.types.has(a.shot_type):
		stats.types[a.shot_type] = [0, 0]
	stats.types[a.shot_type][0] += 1
	if q > 1.05:
		hud.popup("パーフェクト・リリース！", Color(1, 0.95, 0.4))
		play_ui("perfect")
	var make := randf() < p
	var points := 2 if is_two_point(fp) else 1
	var valid: bool = not (needs_clear and a.team == possession)
	stats.shots[a.team] += 1
	var target := rim_center() + Vector3(0, 0.02, 0)
	if not make:
		var ang := randf() * TAU
		target += Vector3(cos(ang), 0, sin(ang)) * randf_range(0.2, 0.32)
	var from: Vector3 = a.global_position + Vector3(0, 2.35, 0) + a.forward() * 0.35
	if from.z < 1.45:
		from.z = 1.45  # ボードの裏からは打てないので、リバースレイアップ扱いで前に出す
	var off := Vector3(from.x, 0, from.z - RIM_Z)
	if off.length() < 0.6:
		off = Vector3(off.x, 0, max(off.z, 0.3)).normalized() * 0.6
		from = Vector3(off.x, from.y, RIM_Z + off.z)  # リング真下からは少し離して打つ
	stats.exp_makes += p
	if make:
		stats.planned_makes += 1
	if autotest and planned_pending.size() > 0:
		failed_makes.append(planned_pending)
	planned_pending = [snappedf(fp.x, 0.1), snappedf(fp.z, 0.1), snappedf(nd, 0.1)] if make else []
	var angle := 52.0
	if nd < 1.2:
		angle = 76.0
	elif nd < 2.0:
		angle = 64.0
	if float(st.angle) > 0.0 and nd >= 1.2:
		angle = float(st.angle)
	var v := _solve(from, target, angle)
	if a.shot_type == "dunk" and nd < 2.2:
		# ダンク：リングの真上から叩き込む
		var away := Vector3(fp.x, 0, fp.z - RIM_Z)
		away = away.normalized() if away.length() > 0.05 else Vector3(0, 0, 1)
		from = rim_center() + away * 0.3 + Vector3(0, 0.32, 0)
		var aim := rim_center() + Vector3(0, -0.1, 0)
		if not make:
			aim = rim_center() + away * 0.24 + Vector3(0, 0.02, 0)
		v = (aim - from) / 0.14
	ball.shoot(from, v, a, points, valid)
	ball.shot_kind = a.shot_type
	ball.block_mult = float(st.block)
	if a.shot_type == "dunk" and not o.is_on_floor() and (o.flat_pos() - rim_floor()).length() < 1.5:
		# ゴール下で跳んでいた相手には、ダンクを叩かれることがある
		if o.is_human or randf() < 0.3 + 0.4 * o.brain.skill:
			ball.blocked = true
			ball.state = B.LOOSE
			ball.loose_t = 0.0
			ball.last_touch_team = o.team
			var out := Vector3(fp.x, 0, fp.z - RIM_Z).normalized() * 4.0
			ball.launch(out + Vector3(0, 2.0, 0))
			stats.block += 1
			hud.popup("ダンクをブロック！ " + team_name(o.team), _col(o.team))
			play_ui("win" if o.team == 0 else "lose")
	shot_clock = SHOT_CLOCK
	if label != "":
		hud.popup(label, _col(a.team))
	if a.zone_t > 0.0:
		a.zone_t = 0.0
	hud.last_shot_info = "%s のシュート: 成功率 %d%%（タイミング %.2f / 妨害 %.2f / ゾーン %.2f）" % [team_name(a.team), int(p * 100), q, sc.cont, sc.zone]


func _solve(from: Vector3, target: Vector3, angle_deg: float) -> Vector3:
	var g := 9.8
	var diff := target - from
	var h := diff.y
	var flat := Vector3(diff.x, 0, diff.z)
	var d := flat.length()
	var th := deg_to_rad(angle_deg)
	var denom := 2.0 * cos(th) * cos(th) * (d * tan(th) - h)
	if denom <= 0.05:
		th = deg_to_rad(72.0)
		denom = 2.0 * cos(th) * cos(th) * (d * tan(th) - h)
	var v := sqrt(max(g * d * d / max(denom, 0.05), 1.0))
	if d < 0.05:
		return Vector3(0, sqrt(2.0 * g * max(h, 0.2)) + 0.5, 0)
	return flat.normalized() * v * cos(th) + Vector3.UP * v * sin(th)


func on_basket() -> void:
	if shottest:
		_shottest_basket()
		return
	var t: int = ball.shot_team if ball.shot_team >= 0 else ball.last_touch_team
	var pts: int = ball.shot_points if ball.shot_team >= 0 else 1
	var valid: bool = ball.shot_valid if ball.shot_team >= 0 else true
	if not valid:
		hud.popup("クリアしていない！ ノーカウント", Color(1, 0.6, 0.3))
		stats.turnover += 1
		_dead_ball(1 - t, 1.0)
		return
	planned_pending = []
	score[t] += pts
	if ball.shot_team >= 0 and stats.types.has(ball.shot_kind):
		stats.types[ball.shot_kind][1] += 1
	stats.pts[t] += pts
	stats.makes[t] += 1
	play_ui("score")
	hud.popup("%s +%d" % [team_name(t), pts], _col(t))
	if score[t] >= TARGET_SCORE:
		game_over = true
		ball.state = B.DEAD
		hud.popup("%s の勝ち！" % team_name(t), _col(t))
		if not autotest:
			set_paused(true)
		return
	_dead_ball(1 - t, 1.2)


func on_out_of_bounds() -> void:
	if ball.state == B.DEAD:
		return
	var next: int = 1 - ball.last_touch_team
	if next != possession:
		stats.turnover += 1
	hud.popup("アウト・オブ・バウンズ", Color(1, 0.8, 0.3))
	_dead_ball(next, 0.8)


func on_stuck_ball() -> void:
	_dead_ball(1 - possession, 0.5)


func ppp(t: int) -> float:
	return float(stats.pts[t]) / max(1, stats.poss[t])


func _print_report() -> void:
	print("[autotest] time=%.0fs score=%d-%d" % [autotest_time, score[0], score[1]])
	print("[autotest] stats=", stats)
	print("[autotest] PPP A=%.2f B=%.2f" % [ppp(0), ppp(1)])
	print("[autotest] failed planned makes (x,z,nd): ", failed_makes)


func _shottest_step(delta: float) -> void:
	## 計画上「成功」のシュートが、物理で本当に入るかを距離ごとに確かめる
	st_t -= delta
	if st_i < 0 or ball.state == B.DEAD or st_t <= 0.0:
		if st_i >= 0:
			st_res.append([st_list[st_i], st_made])
		st_i += 1
		if st_i >= st_list.size():
			var agg := {}
			for r in st_res:
				if not agg.has(r[0]):
					agg[r[0]] = [0, 0]
				agg[r[0]][0] += r[1]
				agg[r[0]][1] += 1
			print("[shottest] distance -> made/total: ", agg)
			get_tree().quit()
			return
		st_made = 0
		st_t = 3.0
		var d: float = st_list[st_i]
		var side: float = [-1.0, -0.5, 0.5, 1.0][st_i % 4]
		var dirv := Vector3(sin(side * 0.6), 0, cos(side * 0.6))
		var from := rim_floor() + dirv * d + Vector3(0, 2.1, 0)
		dead_ball_t = -1.0
		if d < 0.0:
			# ダンクの弾道
			var away := dirv
			var f2 := rim_center() + away * 0.3 + Vector3(0, 0.32, 0)
			ball.shoot(f2, (rim_center() + Vector3(0, -0.1, 0) - f2) / 0.14, human, 1, true)
		else:
			ball.shoot(from, _solve(from, rim_center() + Vector3(0, 0.02, 0), 76.0 if d < 1.2 else (64.0 if d < 2.0 else 52.0)), human, 1, true)
		ball.shot_team = 0
	if ball.state != B.DEAD:
		var p: Vector3 = ball.global_position
		if ball.prev_pos.y >= RIM_Y and p.y < RIM_Y:
			pass


func _shottest_basket() -> void:
	st_made = 1
	ball.state = B.DEAD
	ball.freeze = true
