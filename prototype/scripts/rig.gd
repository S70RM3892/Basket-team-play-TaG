extends Node3D
## 関節つきの選手モデル（プリミティブで組み、動きはプログラムで付ける）。
## 走る・ドリブル・シュート（種類ごと）・妨害・タックル・かわし（スピン）などのポーズを持つ。
## 腕・脚は「下向きにぶら下がる」向きで作ってあり、rotation.x を正にすると前へ振り上がる。

var hips: Node3D
var spine: Node3D
var neck: Node3D
var sh_l: Node3D
var sh_r: Node3D
var el_l: Node3D
var el_r: Node3D
var hip_l: Node3D
var hip_r: Node3D
var kn_l: Node3D
var kn_r: Node3D
var phase := 0.0
var suit: StandardMaterial3D
var trim: StandardMaterial3D
var dark: StandardMaterial3D


func build(color: Color) -> void:
	suit = _mat(color.darkened(0.15))
	trim = _mat(Color(0.92, 0.93, 0.95))
	dark = _mat(Color(0.12, 0.13, 0.16))
	hips = _joint(self, Vector3(0, 0.95, 0))
	_part(hips, _box(Vector3(0.34, 0.16, 0.2)), Vector3(0, 0.0, 0), dark)
	spine = _joint(hips, Vector3(0, 0.05, 0))
	_part(spine, _capsule(0.17, 0.6), Vector3(0, 0.3, 0), suit, Vector3(1.3, 1.0, 0.8))
	_part(spine, _box(Vector3(0.3, 0.12, 0.05)), Vector3(0, 0.42, -0.13), trim)  # 胸のTaGプレート
	neck = _joint(spine, Vector3(0, 0.66, 0))
	_part(neck, _sphere(0.13), Vector3(0, 0.12, 0), dark)
	var visor := _part(neck, _box(Vector3(0.2, 0.06, 0.05)), Vector3(0, 0.14, -0.11), null)
	var vm := StandardMaterial3D.new()
	vm.albedo_color = Color(0.05, 0.05, 0.08)
	vm.emission_enabled = true
	vm.emission = color
	vm.emission_energy_multiplier = 1.6
	visor.material_override = vm
	# 腕
	sh_l = _joint(spine, Vector3(-0.25, 0.55, 0))
	sh_r = _joint(spine, Vector3(0.25, 0.55, 0))
	for sh in [sh_l, sh_r]:
		_part(sh, _capsule(0.055, 0.32), Vector3(0, -0.15, 0), suit)
	el_l = _joint(sh_l, Vector3(0, -0.3, 0))
	el_r = _joint(sh_r, Vector3(0, -0.3, 0))
	for el in [el_l, el_r]:
		_part(el, _capsule(0.05, 0.3), Vector3(0, -0.14, 0), trim)
		_part(el, _sphere(0.062), Vector3(0, -0.3, 0), dark)
	# 脚
	hip_l = _joint(hips, Vector3(-0.11, -0.02, 0))
	hip_r = _joint(hips, Vector3(0.11, -0.02, 0))
	for hp in [hip_l, hip_r]:
		_part(hp, _capsule(0.08, 0.46), Vector3(0, -0.22, 0), dark)
	kn_l = _joint(hip_l, Vector3(0, -0.45, 0))
	kn_r = _joint(hip_r, Vector3(0, -0.45, 0))
	for kn in [kn_l, kn_r]:
		_part(kn, _capsule(0.065, 0.44), Vector3(0, -0.21, 0), suit)
		_part(kn, _box(Vector3(0.11, 0.07, 0.26)), Vector3(0, -0.44, -0.05), trim)


func spine_node() -> Node3D:
	return spine


func _mat(c: Color) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = c
	m.roughness = 0.6
	return m


func _joint(parent: Node3D, p: Vector3) -> Node3D:
	var j := Node3D.new()
	j.position = p
	parent.add_child(j)
	return j


func _part(parent: Node3D, mesh: Mesh, p: Vector3, mat: Material, sc := Vector3.ONE) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	mi.position = p
	mi.scale = sc
	if mat:
		mi.material_override = mat
	parent.add_child(mi)
	return mi


func _capsule(r: float, h: float) -> CapsuleMesh:
	var m := CapsuleMesh.new()
	m.radius = r
	m.height = h
	m.radial_segments = 12
	m.rings = 4
	return m


func _sphere(r: float) -> SphereMesh:
	var m := SphereMesh.new()
	m.radius = r
	m.height = r * 2.0
	m.radial_segments = 12
	m.rings = 6
	return m


func _box(s: Vector3) -> BoxMesh:
	var m := BoxMesh.new()
	m.size = s
	return m


## a: 選手。S: 状態の列挙。毎フレーム呼ぶ。
func animate(a, S, delta: float) -> void:
	var hv := Vector3(a.velocity.x, 0, a.velocity.z)
	var spd := hv.length()
	var fwd_spd: float = hv.dot(a.forward())
	var amp: float = clamp(spd / 5.0, 0.0, 1.0)
	if spd > 0.3:
		phase += delta * (5.0 + spd * 1.5) * (1.0 if fwd_spd >= -0.5 else -1.0)
	var on_floor: bool = a.is_on_floor()
	var role: int = a.role()
	var st: int = a.state

	# 既定：走り／立ち
	var sw := sin(phase)
	var t := {
		"hips_y": 0.95 - 0.05 * absf(sw) * amp, "lean": 0.12 * amp, "lean_z": 0.0, "spin": 0.0,
		"hip_l": sw * 0.75 * amp, "hip_r": -sw * 0.75 * amp,
		"kn_l": -(0.15 + 0.9 * amp * maxf(0.0, -sw)), "kn_r": -(0.15 + 0.9 * amp * maxf(0.0, sw)),
		"sh_l": -sw * 0.6 * amp + 0.1, "sh_r": sw * 0.6 * amp + 0.1, "shz_l": -0.12, "shz_r": 0.12,
		"el_l": 0.5, "el_r": 0.5,
	}
	if role == 2 and st == S.NORMAL:
		# 守備の構え：腰を落として腕を広げる
		t.hips_y -= 0.1
		t.kn_l -= 0.55
		t.kn_r -= 0.55
		t.hip_l += 0.35
		t.hip_r += 0.35
		t.lean = 0.25
		t.sh_l = 0.5
		t.sh_r = 0.5
		t.shz_l = -0.9
		t.shz_r = 0.9
		t.el_l = 0.4
		t.el_r = 0.4
	if role == 1 and st == S.NORMAL:
		# ドリブル：右手がボールに合わせて上下、左手は相手から守る
		var bob := absf(sin(a.dribble_t * 7.5))
		t.sh_r = 0.45 + 0.25 * bob
		t.el_r = 0.3 + 0.5 * bob
		t.shz_r = 0.25
		t.sh_l = 0.9
		t.el_l = 0.9
		t.shz_l = -0.4
		t.lean += 0.12
	if not on_floor and st == S.NORMAL:
		t.kn_l = -1.0
		t.kn_r = -0.7
		t.hip_l = 0.6
		t.hip_r = 0.3
		if role != 1:
			t.sh_l = 2.95
			t.sh_r = 2.95
			t.el_l = 0.1
			t.el_r = 0.1
	match st:
		S.SHOOT:
			var k: float = clamp(a.shoot_charge / 0.7, 0.0, 1.0)
			var ty: String = a.shot_type
			if ty == "layup" or ty == "euro" or ty == "dunk":
				# 片手を高く伸ばし、反対の膝を引き上げる
				t.sh_r = lerp(1.2, 3.0, k)
				t.el_r = lerp(1.2, 0.05, k)
				t.sh_l = 1.0
				t.el_l = 0.8
				if not on_floor:
					t.hip_l = 1.3
					t.kn_l = -1.6
					t.hip_r = 0.1
					t.kn_r = -0.2
				t.lean = 0.15 if ty != "dunk" else 0.3
			else:
				# ジャンプショット系：両手でボールを額の上へ持ち上げて放つ
				t.sh_l = lerp(0.7, 2.7, k)
				t.sh_r = lerp(0.8, 2.85, k)
				t.el_l = lerp(1.7, 0.6, k)
				t.el_r = lerp(1.9, 0.15, k)
				t.shz_l = -0.15
				t.shz_r = 0.1
				if on_floor and k < 0.5:
					t.kn_l = -0.7
					t.kn_r = -0.7
					t.hip_l = 0.45
					t.hip_r = 0.45
					t.hips_y = 0.85
				elif not on_floor:
					t.kn_l = -0.3
					t.kn_r = -0.3
					t.hip_l = 0.1
					t.hip_r = 0.1
				t.lean = -0.38 if ty == "fade" else (-0.15 if ty == "stepback" else 0.0)
		S.CONTEST_START, S.CONTEST:
			t.sh_l = 3.0
			t.sh_r = 3.0
			t.el_l = 0.05
			t.el_r = 0.05
			t.shz_l = -0.15
			t.shz_r = 0.15
			t.lean = 0.0
		S.TACKLE_START:
			t.hips_y = 0.78
			t.kn_l = -1.1
			t.kn_r = -1.1
			t.hip_l = 0.7
			t.hip_r = 0.7
			t.lean = 0.55
			t.sh_l = 0.9
			t.sh_r = 0.9
			t.el_l = 1.2
			t.el_r = 1.2
		S.TACKLE:
			t.lean = 0.85
			t.sh_l = 1.5
			t.sh_r = 1.5
			t.el_l = 0.3
			t.el_r = 0.3
			t.hip_r = 0.9
			t.kn_r = -0.8
			t.hip_l = -0.4
		S.TACKLE_RECOVER:
			t.lean = 0.6
			t.sh_l = 0.4
			t.sh_r = 0.4
		S.DASH_START, S.DASH:
			t.lean = 0.45
		S.DODGE_START, S.DODGE:
			# スピンムーブ
			var side: float = signf(a.dir_lock.dot(a.right_vec()))
			t.spin = TAU * clamp(a.state_t / 0.36, 0.0, 1.0) * (side if side != 0.0 else 1.0)
			t.lean = 0.2
		S.DODGE_RECOVER:
			t.lean = 0.25
			t.sh_l = 0.2
			t.sh_r = 0.2
		S.STAGGER:
			t.lean = -0.3
			t.sh_l = 1.4
			t.sh_r = 1.4
			t.shz_l = -1.0
			t.shz_r = 1.0
		S.STEAL_RECOVER:
			t.sh_r = 1.6
			t.el_r = 0.1
			t.lean = 0.4
		S.DOWN:
			t.shz_l = -1.2
			t.shz_r = 1.2
			t.sh_l = 0.4
			t.sh_r = 0.4
			t.kn_l = -0.3
			t.kn_r = -0.6
		S.STEP:
			t.lean = 0.2
	if a.pretell == "tackle" and st == S.NORMAL:
		t.hips_y = 0.82
		t.kn_l = -1.0
		t.kn_r = -1.0
		t.hip_l = 0.6
		t.hip_r = 0.6
		t.lean = 0.5
	elif a.pretell == "contest" and st == S.NORMAL:
		t.sh_l = 2.2
		t.sh_r = 2.2

	var r: float = clamp(18.0 * delta, 0.0, 1.0)
	hips.position.y = lerp(hips.position.y, float(t.hips_y), r)
	spine.rotation.x = lerp_angle(spine.rotation.x, -float(t.lean), r)
	rotation.y = float(t.spin) if st == S.DODGE or st == S.DODGE_START else lerp_angle(rotation.y, 0.0, r)
	hip_l.rotation.x = lerp_angle(hip_l.rotation.x, float(t.hip_l), r)
	hip_r.rotation.x = lerp_angle(hip_r.rotation.x, float(t.hip_r), r)
	kn_l.rotation.x = lerp_angle(kn_l.rotation.x, float(t.kn_l), r)
	kn_r.rotation.x = lerp_angle(kn_r.rotation.x, float(t.kn_r), r)
	sh_l.rotation.x = lerp_angle(sh_l.rotation.x, float(t.sh_l), r)
	sh_r.rotation.x = lerp_angle(sh_r.rotation.x, float(t.sh_r), r)
	sh_l.rotation.z = lerp_angle(sh_l.rotation.z, float(t.shz_l), r)
	sh_r.rotation.z = lerp_angle(sh_r.rotation.z, float(t.shz_r), r)
	el_l.rotation.x = lerp_angle(el_l.rotation.x, float(t.el_l), r)
	el_r.rotation.x = lerp_angle(el_r.rotation.x, float(t.el_r), r)
