extends RefCounted
## BOTの思考。じゃんけんの手を「混合戦略＋相手の状態への反応」で選ぶ。

const A = preload("res://scripts/athlete.gd")
const BL = preload("res://scripts/ball.gd")

var skill := 0.7
var think_t := 0.0
var strafe := 1.0
var shooting := false
var release_at := 0.8
var contest_hold := 0.0
var reacted_state := -1
var reacted_owner_t := -1.0
var queue: Array = []  # [{"t": float, "act": String}]


func _init(p_skill := 0.7) -> void:
	skill = p_skill


func _schedule(act: String, delay: float) -> void:
	for q in queue:
		if q.act == act:
			return
	queue.append({"t": delay, "act": act})


func _run_queue(a, delta: float) -> void:
	var keep: Array = []
	for q in queue:
		q.t -= delta
		if q.t <= 0.0:
			_do(a, q.act)
		else:
			keep.append(q)
	queue = keep


func _do(a, act: String) -> void:
	if a.state != A.S.NORMAL:
		return
	match act:
		"tackle":
			if a.role() == 2 and a.cd.tackle <= 0.0:
				a.i_ab1 = true
		"contest":
			if a.role() == 2 and a.cd.contest <= 0.0:
				a.i_ab2 = true
				a.i_ab2_held = true
				contest_hold = randf_range(0.5, 0.9)
		"jump":
			a.i_jump = true
		"dodge":
			if a.role() == 1 and a.cd.dodge <= 0.0:
				a.i_ab2 = true
		"steal":
			if a.role() == 2 and a.cd.steal <= 0.0:
				a.i_steal = true


func _react_delay() -> float:
	return randf_range(0.22, 0.34) - skill * 0.06


func think(a, delta: float) -> void:
	var g = a.game
	var opp = g.opponent_of(a)
	var rim: Vector3 = g.rim_floor()
	_run_queue(a, delta)
	if contest_hold > 0.0:
		contest_hold -= delta
		a.i_ab2_held = true
	var r: int = a.role()
	if r == 1:
		_offense(a, opp, rim, delta)
	elif r == 2:
		_defense(a, opp, rim, delta)
	else:
		_loose(a, opp, rim, delta)


func _offense(a, opp, rim: Vector3, delta: float) -> void:
	var g = a.game
	var to_rim: Vector3 = rim - a.flat_pos()
	var dist_rim := to_rim.length()
	var nd: float = dist_rim / g.court_scale
	var to_opp: Vector3 = opp.flat_pos() - a.flat_pos()
	var d_opp := to_opp.length()
	var between: bool = d_opp < 3.0 and to_opp.normalized().dot(to_rim.normalized()) > 0.45
	a.face_point(rim, delta, 8.0)

	if a.state == A.S.SHOOT:
		a.i_shoot_held = a.shoot_charge < release_at
		return
	shooting = false

	if g.needs_clear and g.possession == a.team:
		var out: Vector3 = rim - to_rim.normalized() * (g.arc_radius() + 1.2)
		a.set_move_world(out - a.flat_pos())
		return

	var wish := to_rim.normalized()
	if between:
		wish = (wish + a.right_vec() * strafe * 0.9).normalized()
	a.set_move_world(wish)
	if a.state != A.S.NORMAL:
		return
	# 反応：タックルの出始めを見てかわす（反応時間より発生が速いので、成功は一部だけ）
	if opp.state == A.S.TACKLE_START and reacted_state != A.S.TACKLE_START:
		reacted_state = A.S.TACKLE_START
		if randf() < skill * 0.45:
			_schedule("dodge", _react_delay() * 0.5)
	elif opp.state != A.S.TACKLE_START:
		reacted_state = -1

	if g.shot_clock < 2.0 or dist_rim < 1.5:
		_start_shot(a)
		return
	think_t -= delta
	if think_t > 0.0:
		return
	think_t = randf_range(0.3, 0.6) / (0.6 + skill * 0.6)
	if randf() < 0.25:
		strafe = -strafe
	var open := d_opp > 2.3 or not between
	if open and nd < 8.0 and randf() < 0.75:
		_start_shot(a)
		return
	if not between:
		return
	var w := {"dash": 1.0, "dodge": 0.35, "shoot": 0.7 if nd < 7.5 else 0.15, "wait": 0.6}
	if opp.state == A.S.CONTEST or opp.state == A.S.CONTEST_START:
		w.dash = 3.0
		w.shoot = 0.05
	if opp.cd.tackle > 0.6:
		w.dash += 2.0
		w.dodge = 0.1
	if a.cd.dash > 0.0:
		w.dash = 0.0
	if a.cd.dodge > 0.0:
		w.dodge = 0.0
	match _pick(w):
		"dash":
			a.i_ab1 = true
		"dodge":
			a.i_ab2 = true
		"shoot":
			_start_shot(a)


func _start_shot(a) -> void:
	shooting = true
	release_at = clamp(0.8 + randfn(0.0, 0.16 * (1.2 - skill)), 0.45, 1.2)
	a.i_shoot_held = true


func _defense(a, h, rim: Vector3, delta: float) -> void:
	var g = a.game
	var hp: Vector3 = h.flat_pos()
	var to_rim: Vector3 = rim - hp
	var guard_d := 1.5 if to_rim.length() < 8.5 * g.court_scale else 2.2
	var gp: Vector3 = hp + to_rim.normalized() * min(guard_d, to_rim.length() * 0.6)
	var to_gp: Vector3 = gp - a.flat_pos()
	a.set_move_world(to_gp, to_gp.length() / 0.6)
	a.face_point(h.global_position, delta, 12.0)
	var d: float = (hp - a.flat_pos()).length()

	# 反応
	if h.state != reacted_state:
		reacted_state = h.state
		match h.state:
			A.S.SHOOT:
				if d < 2.6 and a.cd.contest <= 0.0 and randf() < 0.45 + skill * 0.3:
					_schedule("contest", _react_delay())
				elif d < 1.9 and randf() < 0.15 + skill * 0.3:
					_schedule("jump", _react_delay() + 0.1)
			A.S.DASH_START:
				if d < 3.0 and randf() < skill * 0.5:
					_schedule("tackle", _react_delay() * 0.6)
			A.S.DODGE_RECOVER:
				if d < 2.0:
					_schedule("steal", 0.08)
	if h.state == A.S.DASH and a.cd.step <= 0.0 and a.state == A.S.NORMAL:
		var h_rim: float = (rim - hp).length()
		var me_rim: float = (rim - a.flat_pos()).length()
		if h_rim < me_rim + 0.3 and randf() < skill:
			a.i_step = true
			a.set_move_world(to_gp)

	if a.state != A.S.NORMAL or d > 2.4:
		return
	think_t -= delta
	if think_t > 0.0:
		return
	think_t = randf_range(0.4, 0.8) / (0.6 + skill * 0.6)
	var w := {"tackle": 0.6, "contest": 0.6, "wait": 1.8, "steal": 0.25}
	if a.cd.tackle > 0.0:
		w.tackle = 0.0
	if a.cd.contest > 0.0:
		w.contest = 0.0
	if a.cd.steal > 0.0:
		w.steal = 0.0
	if (rim - hp).length() / g.court_scale > 7.5:
		w.contest *= 0.3
	match _pick(w):
		"tackle":
			a.i_ab1 = true
		"contest":
			a.i_ab2 = true
			a.i_ab2_held = true
			contest_hold = randf_range(0.5, 0.9)
		"steal":
			a.i_steal = true


func _loose(a, opp, rim: Vector3, delta: float) -> void:
	var g = a.game
	var b = g.ball
	var bp: Vector3 = b.global_position
	var target := Vector3(bp.x, 0, bp.z)
	if b.state == BL.B.FLIGHT:
		# シュート中：リング付近に入る
		target = rim + (a.flat_pos() - rim).normalized() * 1.4
		if b.shot_team != a.team and b.shot_time < 0.12:
			var sd: float = (b.shooter.flat_pos() - a.flat_pos()).length() if b.shooter else 99.0
			if sd < 1.6 and randf() < 0.1 + skill * 0.2:
				_schedule("jump", 0.05)
	var to_t: Vector3 = target - a.flat_pos()
	a.set_move_world(to_t, to_t.length() / 0.5)
	a.face_point(bp, delta, 10.0)
	if a.state == A.S.NORMAL and b.state == BL.B.LOOSE:
		if to_t.length() > 4.0 and a.cd.dash <= 0.0 and randf() < 0.05:
			a.i_ab1 = true
		if bp.y > 2.0 and to_t.length() < 1.2 and a.is_on_floor():
			a.i_jump = true


func _pick(w: Dictionary) -> String:
	var total := 0.0
	for k in w.keys():
		total += float(w[k])
	if total <= 0.0:
		return "wait"
	var r := randf() * total
	for k in w.keys():
		r -= float(w[k])
		if r <= 0.0:
			return k
	return "wait"
