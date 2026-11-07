extends CharacterBody3D
## 選手（人間・BOT共通）。移動・状態・アビリティじゃんけんの処理を持つ。
## 入力は「意図（intent）」として受け取り、人間は入力デバイスから、BOTはai_brainから設定する。

enum S { NORMAL, DASH_START, DASH, DODGE_START, DODGE, DODGE_RECOVER, TACKLE_START, TACKLE, TACKLE_RECOVER, CONTEST_START, CONTEST, STEP, DOWN, STAGGER, STEAL_RECOVER, SHOOT }

const WALK_SPEED := 6.2
const DRIBBLE_SPEED := 5.6
const SHOOT_MOVE_SPEED := 1.6
const CONTEST_MOVE_SPEED := 1.4
const ACCEL := 34.0
const GRAVITY := 13.0
const JUMP_VEL := 4.6

# 突破（攻撃・移動系）
const DASH_START_T := 0.15
const DASH_T := 0.30
const DASH_SPEED := 13.0
const DASH_CD := 4.0
# かわし（攻撃・妨害回避系）
const DODGE_START_T := 0.08
const DODGE_T := 0.36
const DODGE_SPEED := 4.8
const DODGE_RECOVER_T := 0.45
const DODGE_CD := 5.0
# タックル（守備・タックル系）
const TACKLE_START_T := 0.15
const TACKLE_T := 0.20
const TACKLE_SPEED := 9.0
const TACKLE_RECOVER_T := 0.50
const TACKLE_WHIFF_EXTRA := 0.35
const TACKLE_CD := 5.0
const TACKLE_RANGE := 1.15
# シュート妨害（守備・妨害系）
const CONTEST_START_T := 0.15
const CONTEST_MAX_T := 1.2
const CONTEST_CD := 5.0
const CONTEST_RANGE := 2.4
# 追走ステップ（守備・移動系＝間合い）
const STEP_T := 0.2
const STEP_SPEED := 8.5
const STEP_CD := 2.5
# 素の守備
const STEAL_CD := 1.2
const STEAL_RANGE := 1.8
const DOWN_T := 1.2
const STAGGER_T := 0.7
const SHOT_METER_T := 0.7
# シュートの種類：基本操作の組み合わせで決まる（_classify_shot）
#  meter=溜めが1.0になるまでの秒数 / mult=成功率の倍率 / prox=寄せの効きやすさ（小さいほど寄せに強い）
#  block=ブロックされやすさ / angle=弾道の角度（0は距離で自動）/ jump_at・jump_v=自動ジャンプ（頂点が「パーフェクト」付近に来る）
const SHOT_TYPES := {
	"jumper": {"name": "ジャンプショット", "meter": 0.7, "mult": 1.0, "prox": 1.0, "block": 1.0, "angle": 0.0, "jump_at": 0.2, "jump_v": 4.0},
	"stepback": {"name": "ステップバック", "meter": 0.75, "mult": 0.95, "prox": 1.0, "block": 0.8, "angle": 0.0, "jump_at": 0.25, "jump_v": 4.0},
	"fade": {"name": "フェイダウェイ", "meter": 0.45, "mult": 0.86, "prox": 0.45, "block": 0.4, "angle": 56.0, "jump_at": -1.0, "jump_v": 0.0},
	"layup": {"name": "レイアップ", "meter": 0.8, "mult": 1.05, "prox": 1.0, "block": 1.3, "angle": 0.0, "jump_at": 0.25, "jump_v": 4.4},
	"euro": {"name": "ユーロステップ", "meter": 0.8, "mult": 1.0, "prox": 0.5, "block": 0.6, "angle": 0.0, "jump_at": 0.25, "jump_v": 4.4},
	"floater": {"name": "フローター", "meter": 0.6, "mult": 0.95, "prox": 0.6, "block": 0.3, "angle": 66.0, "jump_at": 0.19, "jump_v": 3.2},
	"dunk": {"name": "ダンク", "meter": 0.5, "mult": 1.0, "prox": 0.7, "block": 1.5, "angle": 0.0, "jump_at": 0.0, "jump_v": 5.0},
}
const SHOT_AUTO_RELEASE := 1.3
const ZONE_T := 2.0

var game
var team := 0
var is_human := false
var brain = null
var eye_height := 1.7
var state := S.NORMAL
var state_t := 0.0
var recover_len := TACKLE_RECOVER_T
var cd := {"dash": 0.0, "dodge": 0.0, "tackle": 0.0, "contest": 0.0, "step": 0.0, "steal": 0.0}
var dir_lock := Vector3.ZERO
var tackle_resolved := false
var dash_beat_done := false
var dodge_evaded := false
var shoot_charge := 0.0
var shot_type := "jumper"
var shot_side := 1.0
var shot_jumped := false
var since_dash := 9.0
var dash_cancel_t := 0.0  # 突破からダンク等へつないだ直後は、まだ「突破」としてタックルに負ける
var shoot_contact := false
var yaw := 0.0
var pitch := 0.0
var dribble_t := 0.0
var team_color := Color(0.2, 0.8, 0.9)
var zone_t := 0.0  # 読み勝ちの後の「ゾーン」：速くなり、次のシュートが入りやすい
var stagger_len := STAGGER_T
var pretell := ""  # BOTの「構え」（弱いBOTは技の前に構えが見える）
var pretell_t := 0.0

# 意図（毎フレーム設定される）
var i_move := Vector2.ZERO
var i_jump := false
var i_shoot_held := false
var i_ab1 := false
var i_ab2 := false
var i_ab2_held := false
var i_step := false
var i_steal := false

var camera: Camera3D
var pivot: Node3D
var ring1_mat: StandardMaterial3D
var ring2_mat: StandardMaterial3D
var body_mat: StandardMaterial3D
var hand_l: Node3D
var hand_r: Node3D
var field_mesh: MeshInstance3D
var sfx: AudioStreamPlayer3D


func setup(p_game, p_team: int, p_human: bool, color: Color) -> void:
	game = p_game
	team = p_team
	is_human = p_human
	team_color = color
	collision_layer = 2
	collision_mask = 11
	floor_snap_length = 0.3
	var col := CollisionShape3D.new()
	var cap := CapsuleShape3D.new()
	cap.radius = 0.32
	cap.height = 1.85
	col.shape = cap
	col.position.y = 0.925
	add_child(col)

	pivot = Node3D.new()
	add_child(pivot)
	body_mat = StandardMaterial3D.new()
	body_mat.albedo_color = color.darkened(0.35)
	var body := MeshInstance3D.new()
	var cm := CapsuleMesh.new()
	cm.radius = 0.3
	cm.height = 1.55
	body.mesh = cm
	body.material_override = body_mat
	body.position.y = 0.78
	pivot.add_child(body)
	var head := MeshInstance3D.new()
	var hm := SphereMesh.new()
	hm.radius = 0.16
	hm.height = 0.32
	head.mesh = hm
	head.material_override = body_mat
	head.position.y = 1.7
	pivot.add_child(head)
	var visor := MeshInstance3D.new()
	var vm := BoxMesh.new()
	vm.size = Vector3(0.24, 0.07, 0.05)
	visor.mesh = vm
	var vmat := StandardMaterial3D.new()
	vmat.albedo_color = Color(0.05, 0.05, 0.08)
	vmat.emission_enabled = true
	vmat.emission = color
	vmat.emission_energy_multiplier = 1.5
	visor.material_override = vmat
	visor.position = Vector3(0, 1.72, -0.15)
	pivot.add_child(visor)
	# TaGリング：上＝アビリティ1、下＝アビリティ2。光っていれば使用可能（クールダウンが相手に見える）
	ring1_mat = _make_glow(color)
	ring2_mat = _make_glow(color)
	pivot.add_child(_make_ring(1.05, ring1_mat))
	pivot.add_child(_make_ring(0.85, ring2_mat))
	hand_l = _make_hand(Vector3(-0.38, 0.95, -0.05))
	hand_r = _make_hand(Vector3(0.38, 0.95, -0.05))
	pivot.add_child(hand_l)
	pivot.add_child(hand_r)
	# シュート妨害フィールド
	field_mesh = MeshInstance3D.new()
	var fm := BoxMesh.new()
	fm.size = Vector3(2.0, 2.3, 0.06)
	field_mesh.mesh = fm
	var fmat := StandardMaterial3D.new()
	fmat.albedo_color = Color(1.0, 0.25, 0.2, 0.16)
	fmat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	fmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	fmat.cull_mode = BaseMaterial3D.CULL_DISABLED
	field_mesh.material_override = fmat
	field_mesh.position = Vector3(0, 1.3, -0.9)
	field_mesh.visible = false
	add_child(field_mesh)

	sfx = AudioStreamPlayer3D.new()
	sfx.unit_size = 6.0
	add_child(sfx)

	if is_human:
		pivot.visible = false
		camera = Camera3D.new()
		camera.position.y = eye_height
		camera.near = 0.05
		camera.fov = 78.0
		add_child(camera)
		camera.current = true
		# 一人称の手
		var vh_l := _make_hand(Vector3(-0.3, -0.38, -0.45))
		var vh_r := _make_hand(Vector3(0.3, -0.38, -0.45))
		camera.add_child(vh_l)
		camera.add_child(vh_r)
		hand_l = vh_l
		hand_r = vh_r
		field_mesh.position = Vector3(0, 1.3, -1.3)


func _make_glow(c: Color) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = c
	m.emission_enabled = true
	m.emission = c
	m.emission_energy_multiplier = 2.5
	return m


func _make_ring(y: float, mat: StandardMaterial3D) -> MeshInstance3D:
	var r := MeshInstance3D.new()
	var t := TorusMesh.new()
	t.inner_radius = 0.31
	t.outer_radius = 0.37
	r.mesh = t
	r.material_override = mat
	r.position.y = y
	return r


func _make_hand(p: Vector3) -> MeshInstance3D:
	var h := MeshInstance3D.new()
	var b := BoxMesh.new()
	b.size = Vector3(0.11, 0.11, 0.2)
	h.mesh = b
	var m := StandardMaterial3D.new()
	m.albedo_color = Color(0.85, 0.65, 0.5)
	h.material_override = m
	h.position = p
	h.set_meta("rest", p)
	return h


# ───────── 基本ベクトル ─────────
func forward() -> Vector3:
	return Vector3(-sin(yaw), 0.0, -cos(yaw))


func right_vec() -> Vector3:
	return Vector3(cos(yaw), 0.0, -sin(yaw))


func look_dir() -> Vector3:
	if camera:
		return -camera.global_transform.basis.z
	return forward()


func eye_pos() -> Vector3:
	return global_position + Vector3(0, eye_height, 0)


func flat_pos() -> Vector3:
	return Vector3(global_position.x, 0.0, global_position.z)


func role() -> int:
	## 0=ルーズ, 1=ボール保持（攻撃）, 2=相手がボール保持（守備）
	var b = game.ball
	if b.holder == self:
		return 1
	if b.holder != null and b.holder.team != team:
		return 2
	return 0


func is_busy() -> bool:
	return state != S.NORMAL


func face_point(p: Vector3, delta: float, rate := 10.0) -> void:
	var d := p - global_position
	d.y = 0
	if d.length() < 0.01:
		return
	var target := atan2(-d.x, -d.z)
	yaw = lerp_angle(yaw, target, clamp(rate * delta, 0.0, 1.0))


func set_move_world(dir: Vector3, mag := 1.0) -> void:
	dir.y = 0
	if dir.length() < 0.001:
		i_move = Vector2.ZERO
		return
	dir = dir.normalized() * clamp(mag, 0.0, 1.0)
	i_move = Vector2(dir.dot(right_vec()), dir.dot(forward()))


func _tps() -> bool:
	return is_human and game != null and game.camera_tps


func ball_hold_point() -> Vector3:
	var bob := absf(sin(dribble_t * 7.5))
	if is_human and camera and not _tps():
		var local := Vector3(0.24, -0.3 - (1.0 - bob) * 0.32, -0.62)
		if state == S.SHOOT:
			local = Vector3(0.2, -0.3, -0.62)
		var p := camera.global_transform * local
		p.y = max(p.y, 0.13)
		return p
	var base := global_position + forward() * 0.42 + right_vec() * 0.22
	if state == S.SHOOT:
		return global_position + forward() * 0.3 + Vector3(0, 2.05, 0)
	base.y = global_position.y + 0.13 + bob * 0.9
	return base


func hand_reach_point() -> Vector3:
	## ブロック判定用の手の位置（人間は視線の先、BOTは頭上前方）
	if is_human and camera and not _tps():
		return eye_pos() + look_dir() * 0.55 + Vector3(0, 0.25, 0)
	return global_position + Vector3(0, eye_height + 0.55, 0) + forward() * 0.3


# ───────── メインループ ─────────
func _physics_process(delta: float) -> void:
	if game == null or game.paused or game.game_over:
		return
	for k in cd.keys():
		cd[k] = max(0.0, cd[k] - delta)
	zone_t = max(0.0, zone_t - delta)
	since_dash += delta
	dash_cancel_t = max(0.0, dash_cancel_t - delta)
	if pretell_t > 0.0:
		pretell_t -= delta
		if pretell_t <= 0.0:
			pretell = ""
	if is_human:
		_read_human_input(delta)
	elif brain:
		brain.think(self, delta)
	_update_state(delta)
	_apply_movement(delta)
	_update_visuals(delta)
	dribble_t += delta
	_clear_intents()


func _clear_intents() -> void:
	i_jump = false
	i_ab1 = false
	i_ab2 = false
	i_step = false
	i_steal = false
	if not is_human:
		i_shoot_held = false
		i_ab2_held = false
		i_move = Vector2.ZERO


func _read_human_input(delta: float) -> void:
	i_move = Input.get_vector("move_left", "move_right", "move_back", "move_fwd")
	var look := Input.get_vector("look_left", "look_right", "look_down", "look_up")
	look = look * look.length()
	yaw -= look.x * 3.4 * delta
	pitch += look.y * 2.4 * delta
	yaw -= game.mouse_delta.x * 0.0028
	pitch -= game.mouse_delta.y * 0.0028
	game.mouse_delta = Vector2.ZERO
	if state == S.SHOOT:
		# シュートのロックオン：溜めている間、視点がリングへ吸い付く
		var d: Vector3 = game.rim_center() - eye_pos()
		var ty := atan2(-d.x, -d.z)
		var tp := atan2(d.y, Vector2(d.x, d.z).length())
		yaw = lerp_angle(yaw, ty, clamp(14.0 * delta, 0.0, 1.0))
		pitch = lerp(pitch, tp, clamp(14.0 * delta, 0.0, 1.0))
	pitch = clamp(pitch, -1.25, 1.25)
	i_jump = Input.is_action_just_pressed("jump")
	i_shoot_held = Input.is_action_pressed("shoot")
	i_ab1 = Input.is_action_just_pressed("ability1")
	i_ab2 = Input.is_action_just_pressed("ability2")
	i_ab2_held = Input.is_action_pressed("ability2")
	i_step = Input.is_action_just_pressed("step")
	i_steal = Input.is_action_just_pressed("steal")


func _enter(s: int) -> void:
	state = s
	state_t = 0.0


func _update_state(delta: float) -> void:
	state_t += delta
	match state:
		S.NORMAL:
			if i_jump and is_on_floor():
				velocity.y = JUMP_VEL
			var r := role()
			if r == 1:
				if i_shoot_held:
					_begin_shot()
				elif i_ab1 and cd.dash <= 0.0:
					cd.dash = DASH_CD
					_enter(S.DASH_START)
					_tell("dash")
				elif i_ab2 and cd.dodge <= 0.0:
					cd.dodge = DODGE_CD
					_enter(S.DODGE_START)
					_tell("dodge")
			elif r == 2:
				if i_ab1 and cd.tackle <= 0.0:
					cd.tackle = TACKLE_CD
					_enter(S.TACKLE_START)
					_tell("tackle")
				elif i_ab2 and cd.contest <= 0.0:
					_enter(S.CONTEST_START)
					_tell("contest")
				elif i_steal and cd.steal <= 0.0:
					_try_steal()
				elif i_step and cd.step <= 0.0:
					_start_step()
			else:
				if i_ab1 and cd.dash <= 0.0:
					cd.dash = DASH_CD * 0.5
					_enter(S.DASH_START)
					_tell("dash")
				elif i_step and cd.step <= 0.0:
					_start_step()
		S.SHOOT:
			var st: Dictionary = SHOT_TYPES[shot_type]
			shoot_charge += delta / float(st.meter)
			if not shot_jumped and float(st.jump_at) >= 0.0 and state_t >= float(st.jump_at) and is_on_floor():
				velocity.y = float(st.jump_v)
				shot_jumped = true
			if role() != 1:
				_enter(S.NORMAL)
			elif not i_shoot_held or shoot_charge >= SHOT_AUTO_RELEASE:
				_release_shot()
		S.DASH_START:
			if state_t >= DASH_START_T:
				dir_lock = _wish_dir()
				if dir_lock.length() < 0.1:
					dir_lock = forward()
				dash_beat_done = false
				collision_mask = 9
				_enter(S.DASH)
		S.DASH:
			since_dash = 0.0
			_check_dash_beats()
			if i_shoot_held and role() == 1:
				# 突破 → シュート：ダンクかユーロステップにつながる
				collision_mask = 11
				dash_cancel_t = 0.2
				_begin_shot()
			elif state_t >= DASH_T:
				collision_mask = 11
				_enter(S.NORMAL)
				game.on_dash_end(self)
		S.DODGE_START:
			if state_t >= DODGE_START_T:
				var side := signf(i_move.x)
				if absf(i_move.x) < 0.3:
					side = 1.0 if randf() < 0.5 else -1.0
				dir_lock = right_vec() * side
				dodge_evaded = false
				collision_mask = 9
				_enter(S.DODGE)
		S.DODGE:
			if state_t >= DODGE_T:
				collision_mask = 11
				if dodge_evaded:
					_enter(S.NORMAL)
				else:
					_enter(S.DODGE_RECOVER)
					game.on_dodge_read(self)
		S.DODGE_RECOVER:
			if state_t >= DODGE_RECOVER_T:
				_enter(S.NORMAL)
		S.TACKLE_START:
			if state_t >= TACKLE_START_T:
				dir_lock = forward()
				tackle_resolved = false
				recover_len = TACKLE_RECOVER_T
				_enter(S.TACKLE)
		S.TACKLE:
			_check_tackle_hit()
			if state == S.TACKLE and state_t >= TACKLE_T:
				_enter(S.TACKLE_RECOVER)
		S.TACKLE_RECOVER:
			if state_t >= recover_len:
				_enter(S.NORMAL)
		S.CONTEST_START:
			if state_t >= CONTEST_START_T:
				_enter(S.CONTEST)
		S.CONTEST:
			var release := (not i_ab2_held and state_t > 0.25) or state_t >= CONTEST_MAX_T
			if role() != 2 and state_t > 0.5:
				release = true
			if release:
				cd.contest = CONTEST_CD
				_enter(S.NORMAL)
		S.STEP:
			if state_t >= STEP_T:
				_enter(S.NORMAL)
		S.DOWN:
			if state_t >= DOWN_T:
				_enter(S.NORMAL)
		S.STAGGER:
			if state_t >= stagger_len:
				_enter(S.NORMAL)
		S.STEAL_RECOVER:
			if state_t >= 0.45:
				_enter(S.NORMAL)


func _classify_shot() -> String:
	## 基本操作の組み合わせからシュートの種類を決める
	var to_rim: Vector3 = game.rim_floor() - flat_pos()
	var nd: float = to_rim.length() / game.court_scale
	var w := _wish_dir()
	var toward := w.dot(to_rim.normalized()) if w.length() > 0.1 and to_rim.length() > 0.01 else 0.0
	shot_side = signf(i_move.x) if absf(i_move.x) > 0.25 else (1.0 if randf() < 0.5 else -1.0)
	if state == S.DASH or since_dash < 0.5:
		if nd < 1.9:
			return "dunk"
		if nd < 3.6:
			return "euro"
	if not is_on_floor():
		return "fade" if toward < -0.3 else "jumper"
	if toward > 0.5:
		if nd < 3.2:
			return "layup"
		if nd < 5.5:
			return "floater"
	if toward < -0.5:
		return "stepback"
	return "jumper"


func _begin_shot() -> void:
	shot_type = _classify_shot()
	shoot_charge = 0.0
	shoot_contact = false
	shot_jumped = false
	collision_mask = 9 if shot_type == "euro" else 11
	_enter(S.SHOOT)
	game.on_shot_start(self)


func shot_meter_t() -> float:
	return float(SHOT_TYPES[shot_type].meter)


func _shot_motion(hv: Vector3, delta: float) -> Vector3:
	## シュートの種類ごとの体の動き（レイアップの2歩、ステップバックの跳び退き など）
	var to_rim: Vector3 = game.rim_floor() - flat_pos()
	var dist := to_rim.length()
	var tr: Vector3 = to_rim / dist if dist > 0.01 else forward()
	var t := state_t
	match shot_type:
		"layup":
			if dist < 0.7:
				return Vector3.ZERO
			return tr * (5.0 if t < 0.28 else 2.0)
		"euro":
			var side := right_vec() * shot_side
			if t < 0.14:
				return side * 5.5 + tr * 2.0
			if dist < 0.7:
				return Vector3.ZERO
			return tr * (5.0 if t < 0.3 else 1.5)
		"dunk":
			return tr * 6.5 if dist > 0.55 else Vector3.ZERO
		"stepback":
			return -tr * 6.5 if t < 0.22 else hv.move_toward(Vector3.ZERO, ACCEL * delta)
		"fade":
			return -tr * 2.4
		"floater":
			return tr * 2.5 if dist > 1.0 else Vector3.ZERO
	return hv.move_toward(Vector3.ZERO, ACCEL * delta)


func _wish_dir() -> Vector3:
	var w := right_vec() * i_move.x + forward() * i_move.y
	w.y = 0
	return w.normalized() if w.length() > 0.15 else Vector3.ZERO


func _apply_movement(delta: float) -> void:
	var hv := Vector3(velocity.x, 0, velocity.z)
	match state:
		S.DASH:
			hv = dir_lock * DASH_SPEED
		S.DODGE:
			hv = dir_lock * DODGE_SPEED + forward() * 1.5
		S.TACKLE:
			hv = dir_lock * TACKLE_SPEED
		S.STEP:
			hv = dir_lock * STEP_SPEED
		S.SHOOT:
			hv = _shot_motion(hv, delta)
		S.DOWN, S.TACKLE_RECOVER, S.DODGE_RECOVER, S.DASH_START, S.TACKLE_START, S.STEAL_RECOVER:
			hv = hv.move_toward(Vector3.ZERO, ACCEL * 1.5 * delta)
		_:
			var spd := WALK_SPEED
			if role() == 1:
				spd = DRIBBLE_SPEED
			if state == S.SHOOT:
				spd = SHOOT_MOVE_SPEED
			elif state == S.CONTEST or state == S.CONTEST_START:
				spd = CONTEST_MOVE_SPEED
			elif state == S.STAGGER:
				spd = 1.0
			if zone_t > 0.0 and state != S.STAGGER:
				spd *= 1.25
			var wish := right_vec() * i_move.x + forward() * i_move.y
			wish.y = 0
			if wish.length() > 1.0:
				wish = wish.normalized()
			hv = hv.move_toward(wish * spd, ACCEL * delta)
	velocity.x = hv.x
	velocity.z = hv.z
	if not is_on_floor():
		velocity.y -= GRAVITY * delta
	elif velocity.y < 0:
		velocity.y = 0
	move_and_slide()
	rotation.y = yaw
	if camera and not _tps():
		camera.rotation.x = pitch


func _start_step() -> void:
	cd.step = STEP_CD
	dir_lock = _wish_dir()
	if dir_lock.length() < 0.1:
		dir_lock = -forward()
	_enter(S.STEP)
	_tell("step")


# ───────── じゃんけんの判定 ─────────
func _check_dash_beats() -> void:
	if dash_beat_done:
		return
	for o in game.athletes:
		if o == self or o.team == team:
			continue
		if o.state == S.CONTEST or o.state == S.CONTEST_START:
			var d: Vector3 = o.global_position - global_position
			d.y = 0
			if d.length() < 2.3:
				dash_beat_done = true
				o.stagger()
				game.on_dash_beats_contest(self, o)


func _check_tackle_hit() -> void:
	if tackle_resolved:
		return
	for o in game.athletes:
		if o == self or o.team == team:
			continue
		var to: Vector3 = o.global_position - global_position
		to.y = 0
		if to.length() < TACKLE_RANGE and (to.length() < 0.4 or to.normalized().dot(forward()) > 0.2):
			tackle_resolved = true
			game.resolve_tackle(self, o)
			return


func _try_steal() -> void:
	cd.steal = STEAL_CD
	var h = game.ball.holder
	var success := false
	if h != null and h.team != team:
		var to: Vector3 = h.global_position - global_position
		to.y = 0
		var lf := look_dir()
		lf.y = 0
		if to.length() < STEAL_RANGE and to.normalized().dot(lf.normalized()) > 0.7:
			if h.state == S.DODGE_RECOVER:
				success = true
			elif h.state == S.SHOOT or h.state == S.DODGE or h.state == S.DODGE_START:
				success = false
			else:
				success = randf() < 0.2
	if success:
		game.steal(self, h)
	else:
		_enter(S.STEAL_RECOVER)
		_tell("whiff")


func _release_shot() -> void:
	var c := shoot_charge
	collision_mask = 11
	_enter(S.NORMAL)
	game.release_shot(self, c, shoot_contact)


func knock_down(push: Vector3) -> void:
	collision_mask = 11
	_enter(S.DOWN)
	velocity = push * 3.0 + Vector3(0, 1.5, 0)
	_tell("down")


func stagger(t := STAGGER_T) -> void:
	if state == S.DOWN:
		return
	cd.contest = CONTEST_CD
	stagger_len = t
	_enter(S.STAGGER)


func reset_for_play(pos: Vector3, face: Vector3) -> void:
	global_position = pos
	velocity = Vector3.ZERO
	collision_mask = 11
	_enter(S.NORMAL)
	zone_t = 0.0
	pretell = ""
	var d := face - pos
	yaw = atan2(-d.x, -d.z)
	pitch = -0.08


# ───────── 見た目・音 ─────────
func _tell(kind: String) -> void:
	if game and game.sfx_bank.has(kind):
		sfx.stream = game.sfx_bank[kind]
		sfx.play()


func _update_visuals(_delta: float) -> void:
	var r1_ready: bool
	var r2_ready: bool
	if role() == 2:
		r1_ready = cd.tackle <= 0.0
		r2_ready = cd.contest <= 0.0
	else:
		r1_ready = cd.dash <= 0.0
		r2_ready = cd.dodge <= 0.0
	ring1_mat.emission_energy_multiplier = 1.4 if r1_ready else 0.05
	ring2_mat.emission_energy_multiplier = 1.4 if r2_ready else 0.05
	var tell := team_color
	match state:
		S.TACKLE_START, S.TACKLE:
			tell = Color(1.0, 0.45, 0.1)
		S.DASH_START, S.DASH:
			tell = Color(0.4, 0.8, 1.0)
		S.DODGE_START, S.DODGE:
			tell = Color(1, 1, 1)
		S.CONTEST_START, S.CONTEST:
			tell = Color(1.0, 0.2, 0.2)
	if state == S.NORMAL and pretell == "tackle":
		tell = Color(1.0, 0.45, 0.1)
	elif state == S.NORMAL and pretell == "contest":
		tell = Color(1.0, 0.2, 0.2)
	elif zone_t > 0.0:
		tell = Color(1.0, 0.85, 0.2)
	ring1_mat.emission = tell
	ring2_mat.emission = tell
	ring1_mat.albedo_color = tell if r1_ready else Color(0.15, 0.15, 0.18)
	ring2_mat.albedo_color = tell if r2_ready else Color(0.15, 0.15, 0.18)
	field_mesh.visible = state == S.CONTEST or state == S.CONTEST_START
	var crouch := 1.0
	var tilt := 0.0
	if state == S.TACKLE_START:
		crouch = 0.82
	elif state == S.NORMAL and pretell == "tackle":
		crouch = 0.88
	elif state == S.DOWN:
		tilt = -1.35
	elif state == S.STAGGER:
		tilt = 0.25
	elif state == S.SHOOT and shot_type == "fade":
		tilt = 0.3
	elif state == S.SHOOT and (shot_type == "layup" or shot_type == "euro" or shot_type == "dunk"):
		tilt = -0.15
	pivot.scale.y = crouch
	pivot.rotation.x = tilt
	var up := state == S.CONTEST or state == S.CONTEST_START or (not is_on_floor() and role() != 1) or (state == S.NORMAL and pretell == "contest")
	for h in [hand_l, hand_r]:
		var rest: Vector3 = h.get_meta("rest")
		if is_human:
			h.position = rest + (Vector3(0, 0.45, -0.1) if up else Vector3.ZERO)
			h.visible = role() != 1 or state == S.SHOOT
		else:
			h.position = rest + (Vector3(0, 1.15, -0.1) if up else Vector3.ZERO)
	if camera:
		var tps := _tps()
		pivot.visible = tps
		var target_y := eye_height
		if state == S.DOWN:
			target_y = 0.6
		elif state == S.TACKLE_START:
			target_y = eye_height - 0.25
		if tps:
			# 三人称：右肩の後ろ上から
			camera.position = camera.position.lerp(Vector3(0.6, 2.4 if state != S.DOWN else 1.6, 3.3), 0.25)
			camera.rotation.x = pitch - 0.2
			for h in [hand_l, hand_r]:
				h.visible = false
		else:
			camera.position = camera.position.lerp(Vector3(0, target_y, 0), 0.25)
		camera.rotation.z = lerp(camera.rotation.z, 0.35 if state == S.DOWN and not tps else 0.0, 0.2)
		var fov_target := 78.0 + (6.0 if zone_t > 0.0 else 0.0)
		camera.fov = lerp(camera.fov, fov_target, 0.15)
