extends RigidBody3D
## ボール。保持中は持ち手に追従し、シュート・ルーズの間は物理で動く。

enum B { HELD, FLIGHT, LOOSE, DEAD }

var game
var state := B.DEAD
var holder = null
var last_touch_team := 0
var loose_t := 0.0
var prev_pos := Vector3.ZERO
var pending_v := Vector3.ZERO
var pending_w := Vector3.ZERO
var has_pending := false

# シュート情報
var shot_team := -1
var shot_points := 0
var shot_valid := true
var shot_time := 0.0
var shooter = null
var blocked := false
var block_rolled := false
var shot_kind := ""
var block_mult := 1.0


func setup(p_game) -> void:
	game = p_game
	collision_layer = 4
	collision_mask = 1
	mass = 0.6
	continuous_cd = true
	linear_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	linear_damp = 0.0
	angular_damp_mode = RigidBody3D.DAMP_MODE_REPLACE
	angular_damp = 0.3
	can_sleep = false
	var pm := PhysicsMaterial.new()
	pm.bounce = 0.72
	pm.friction = 0.6
	physics_material_override = pm
	var col := CollisionShape3D.new()
	var sp := SphereShape3D.new()
	sp.radius = 0.12
	col.shape = sp
	add_child(col)
	var mi := MeshInstance3D.new()
	var sm := SphereMesh.new()
	sm.radius = 0.12
	sm.height = 0.24
	mi.mesh = sm
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.95, 0.45, 0.12)
	mat.emission_enabled = true
	mat.emission = Color(0.9, 0.35, 0.05)
	mat.emission_energy_multiplier = 0.35
	mi.material_override = mat
	add_child(mi)
	freeze_mode = RigidBody3D.FREEZE_MODE_KINEMATIC


func attach(a) -> void:
	holder = a
	state = B.HELD
	last_touch_team = a.team
	freeze = true
	global_position = a.ball_hold_point()


func _integrate_forces(s: PhysicsDirectBodyState3D) -> void:
	if has_pending:
		s.linear_velocity = pending_v
		s.angular_velocity = pending_w
		has_pending = false


func launch(v: Vector3, spin := Vector3.ZERO) -> void:
	holder = null
	freeze = false
	pending_v = v
	pending_w = spin
	has_pending = true
	linear_velocity = v


func drop(push: Vector3) -> void:
	## タックルやスティールでこぼれたボール
	var from = holder
	holder = null
	state = B.LOOSE
	loose_t = 0.0
	shot_team = -1
	if from:
		global_position = from.global_position + Vector3(0, 1.0, 0) + from.forward() * 0.35
	launch(push + Vector3(0, 2.5, 0))


func shoot(from_pos: Vector3, v: Vector3, p_shooter, points: int, valid: bool) -> void:
	holder = null
	state = B.FLIGHT
	freeze = true
	global_position = from_pos
	shooter = p_shooter
	shot_team = p_shooter.team
	last_touch_team = p_shooter.team
	shot_points = points
	shot_valid = valid
	shot_time = 0.0
	blocked = false
	block_rolled = false
	prev_pos = from_pos
	launch(v, p_shooter.right_vec() * -12.0)


func _physics_process(delta: float) -> void:
	if game == null:
		return
	if state == B.HELD and holder:
		global_position = holder.ball_hold_point()
		prev_pos = global_position
		return
	if game.paused:
		return
	if state == B.FLIGHT:
		shot_time += delta
		if shot_time > 0.35 and linear_velocity.y < 0 and global_position.y < 2.6:
			state = B.LOOSE
			loose_t = 0.0
	elif state == B.LOOSE:
		loose_t += delta
	if state == B.FLIGHT or state == B.LOOSE:
		var rim: Vector3 = game.rim_center()
		var p := global_position
		if prev_pos.y >= rim.y and p.y < rim.y and linear_velocity.y < 0:
			var flat := Vector2(p.x - rim.x, p.z - rim.z)
			if flat.length() < 0.2:
				game.on_basket()
		if game.is_out_of_bounds(p) and p.y < 0.4:
			game.on_out_of_bounds()
		if loose_t > 8.0:
			game.on_stuck_ball()
	prev_pos = global_position
