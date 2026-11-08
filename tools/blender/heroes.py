"""
heroes.py - the 30 heroes (tools/art/roster.json) as 13 rigid parts each, modelled with signed distance fields.

Style: stylized fantasy "clay / designer toy" figures (user request 2026-11-08: far from realistic). Every colour
region is its own smooth SDF shell (sdf.Group), meshed with surface nets and decimated (sdfbake.py); an inverted hull
in slot "Outline" draws the ink line (C++ tints it with the team colour).

Rest pose: standing straight, arms hanging straight down, legs straight down, soles on z = 0.
Every part is modelled in its own local space: origin = joint with the parent, limbs extend toward -Z.
Blender axes: +X forward, +Y = LEFT, +Z up (UE gets (x, -y, z)).  Side factor s = +1 (left) / -1 (right).
Recipes below are written in ABSOLUTE rest-pose coordinates (cm, feet on z = 0); PartGroup converts to local.

Sockets (local, cm):
  Pelvis: spine, hip_l, hip_r   Torso: neck, shoulder_l, shoulder_r   Head: eyes
  UpperArm: elbow   LowerArm: hand (palm centre)   Thigh: knee   Shin: ankle   Foot: sole, toe (both on the floor)
"""
import json
import math
import os

import numpy as np
from mathutils import Vector

import sdf
import sdfbake
from sdf import (Group, Sphere, Ellipsoid, RoundCone, RoundBox, Torus, Cylinder, Cone, HalfSpace, Displace,
                 rot_matrix, axis_matrix)
from tagmesh import Part

HERE = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(HERE, "..", "art", "roster.json"), encoding="utf-8") as _f:
    ROSTER = json.load(_f)["heroes"]
SPEC = {h["key"]: h for h in ROSTER}
HERO_IDS = [h["key"] for h in ROSTER]
TARGETS = {h["key"]: (float(h["eye_height"]), float(h["width"]), h["size"]) for h in ROSTER}

PARTS = ["Pelvis", "Torso", "Head", "UpperArm_L", "UpperArm_R", "LowerArm_L", "LowerArm_R",
         "Thigh_L", "Thigh_R", "Shin_L", "Shin_R", "Foot_L", "Foot_R"]
TRI_BUDGET = 9000
# body triangles per part (the outline hull doubles them): 4340 x 2 = 8680 <= 9000
BODY_BUDGET = {"Head": 980, "Torso": 880, "Pelvis": 270, "UpperArm": 170, "LowerArm": 300, "Thigh": 150,
               "Shin": 145, "Foot": 150}
OUTLINE_CM = {"S": 0.42, "M": 0.5, "L": 0.6, "XL": 0.75}


def btype(part):
    return part.split("_")[0]


# ================================================================================================================
# Rig: parts, sockets, rest pivots
# ================================================================================================================
class Rig:
    """Holds the 13 parts of one hero and the rest-pose pivots derived from the socket chain."""

    def __init__(self, hero_id, sk):
        self.id = hero_id
        self.sk = {k: Vector(v) for k, v in sk.items()}
        self.parts = {n: Part("SM_%s_%s" % (hero_id, n)) for n in PARTS}
        sk = self.sk
        m = lambda v: Vector((v.x, -v.y, v.z))   # noqa: E731
        p = self.parts
        p["Pelvis"].socket("spine", sk["spine"] - sk["pelvis"])
        p["Pelvis"].socket("hip_l", sk["hip"] - sk["pelvis"])
        p["Pelvis"].socket("hip_r", m(sk["hip"]) - sk["pelvis"])
        p["Torso"].socket("neck", sk["neck"] - sk["spine"])
        p["Torso"].socket("shoulder_l", sk["shoulder"] - sk["spine"])
        p["Torso"].socket("shoulder_r", m(sk["shoulder"]) - sk["spine"])
        for sd, f in (("L", lambda v: v), ("R", m)):
            p["UpperArm_" + sd].socket("elbow", f(sk["elbow"]) - f(sk["shoulder"]))
            p["Thigh_" + sd].socket("knee", f(sk["knee"]) - f(sk["hip"]))
            p["Shin_" + sd].socket("ankle", f(sk["ankle"]) - f(sk["knee"]))
        # Head.eyes, LowerArm.hand and Foot.sole / Foot.toe are set by the recipes.

    def rest_origins(self):
        p = self.parts
        o = {"Pelvis": self.sk["pelvis"].copy()}
        o["Torso"] = o["Pelvis"] + p["Pelvis"].sockets["spine"]
        o["Head"] = o["Torso"] + p["Torso"].sockets["neck"]
        for sd, s in (("L", "l"), ("R", "r")):
            o["UpperArm_" + sd] = o["Torso"] + p["Torso"].sockets["shoulder_" + s]
            o["LowerArm_" + sd] = o["UpperArm_" + sd] + p["UpperArm_" + sd].sockets["elbow"]
            o["Thigh_" + sd] = o["Pelvis"] + p["Pelvis"].sockets["hip_" + s]
            o["Shin_" + sd] = o["Thigh_" + sd] + p["Thigh_" + sd].sockets["knee"]
            o["Foot_" + sd] = o["Shin_" + sd] + p["Shin_" + sd].sockets["ankle"]
        return o

    def measures(self):
        o = self.rest_origins()
        p = self.parts
        eyes = o["Head"] + p["Head"].sockets["eyes"]
        sole_l = o["Foot_L"] + p["Foot_L"].sockets["sole"]
        sole_r = o["Foot_R"] + p["Foot_R"].sockets["sole"]
        floor = (sole_l.z + sole_r.z) * 0.5
        lo = Vector((1e9, 1e9, 1e9))
        hi = Vector((-1e9, -1e9, -1e9))
        for n, part in p.items():
            if not part.bm.verts:
                continue
            a, b = part.bounds()
            for i in range(3):
                lo[i] = min(lo[i], a[i] + o[n][i])
                hi[i] = max(hi[i], b[i] + o[n][i])
        return dict(eye_height=eyes.z - floor, width=hi.y - lo.y, height=hi.z - floor,
                    depth=hi.x - lo.x, bounds_lo=lo, bounds_hi=hi,
                    tris=sum(pt.tri_count() for pt in p.values()))


# ================================================================================================================
# Designer: groups per part, written in absolute rest-pose coordinates
# ================================================================================================================
def _np(v):
    return np.asarray(v, dtype=np.float64).reshape(3)


class PG:
    """A Group bound to one part: every coordinate given to it is ABSOLUTE (rest pose) and converted to local."""

    def __init__(self, group, origin):
        self.G = group
        self.o = _np(origin)

    def _c(self, c):
        return _np(c) - self.o

    def _op(self, prim, op, k):
        if op == "u":
            self.G.u(prim, k)
        elif op == "s":
            self.G.s(prim, k)
        else:
            self.G.i(prim, k)
        return self

    def sphere(self, c, r, op="u", k=None):
        return self._op(Sphere(self._c(c), r), op, k)

    def ell(self, c, radii, rot=None, op="u", k=None):
        R = rot_matrix(*rot) if isinstance(rot, (tuple, list)) else rot
        return self._op(Ellipsoid(self._c(c), radii, R), op, k)

    def cap(self, a, b, ra, rb=None, op="u", k=None):
        return self._op(RoundCone(self._c(a), self._c(b), ra, rb), op, k)

    def box(self, c, half, r=0.0, rot=None, op="u", k=None):
        R = rot_matrix(*rot) if isinstance(rot, (tuple, list)) else rot
        return self._op(RoundBox(self._c(c), half, r, R), op, k)

    def torus(self, c, R_, r, axis=(0, 0, 1), op="u", k=None, sx=1.0, sy=1.0, hint=(1, 0, 0)):
        return self._op(Torus(self._c(c), R_, r, axis_matrix(axis, hint), sx, sy), op, k)

    def cyl(self, c, r, h, e=0.0, axis=(0, 0, 1), op="u", k=None, hint=(1, 0, 0)):
        return self._op(Cylinder(self._c(c), r, h, e, axis_matrix(axis, hint)), op, k)

    def cone(self, c, r0, r1, h, axis=(0, 0, 1), op="u", k=None):
        return self._op(Cone(self._c(c), r0, r1, h, axis_matrix(axis)), op, k)

    def half(self, c, n, op="i", k=None):
        """Keep the side opposite to n (op 'i'), or remove it (op 's' with -n)."""
        return self._op(HalfSpace(self._c(c), n), op, k)

    def bumpy(self, prim_abs, amp, freq, seed=0.0, op="u", k=None):
        return self._op(Displace(prim_abs, amp, freq, seed), op, k)

    def surf_x(self, y, z, x0=-80.0, x1=80.0, n=600):
        """Absolute x of the front-most surface crossing along +X at (y, z) (the shell as built so far)."""
        xs = np.linspace(x0, x1, n)
        pts = np.stack([xs, np.full(n, float(y)), np.full(n, float(z))], axis=1) - self.o
        d = self.G.dist(pts)
        idx = np.where((d[:-1] < 0) & (d[1:] >= 0))[0]
        if len(idx) == 0:
            return None
        i = idx[-1]
        t = d[i] / (d[i] - d[i + 1])
        return float(xs[i] + t * (xs[i + 1] - xs[i]))


class D:
    """Designer for one hero. k = height scale (E / 175), w = width factor."""

    def __init__(self, key, sk):
        self.key = key
        self.spec = SPEC[key]
        self.E, self.W, self.size = TARGETS[key]
        self.k = self.E / 175.0
        self.rig = Rig(key, sk)
        self.sk = self.rig.sk
        self.O = {n: _np(v) for n, v in self.rig.rest_origins().items()}
        self.groups = {n: [] for n in PARTS}

    def g(self, part, slot, k=None, mirror=False, voxel=None, budget=None, name=""):
        G = Group(slot, k=(1.6 * self.k if k is None else k), mirror_y=mirror, voxel=voxel, budget=budget, name=name)
        self.groups[part].append(G)
        if mirror:
            # mirroring happens in local space: only valid for parts centred on y = 0
            assert abs(self.O[part][1]) < 1e-6, part
        return PG(G, self.O[part])

    def socket(self, part, name, abs_pos):
        self.rig.parts[part].socket(name, Vector(tuple(_np(abs_pos) - self.O[part])))

    def J(self, name, s=1):
        v = self.sk[name]
        return np.array([v.x, v.y * s, v.z])


# ================================================================================================================
# Skeletons
# ================================================================================================================
def skel(E, *, leg=0.94, neck_drop=16.0, shoulder_y=17.5, hip_y=9.5, upper=29.0, ankle=9.0, torso_back=-1.0,
         shoulder_drop=5.0, spine_up=8.0, pelvis_up=6.0):
    """Absolute joint positions. Lengths are given for E = 175 and scaled by E/175 (y values are NOT scaled:
    pass them already sized for the hero)."""
    k = E / 175.0
    ank = ankle * k
    knee = ank + 40.0 * k * leg
    hip = knee + 43.0 * k * leg
    pelvis = hip + pelvis_up * k
    spine = pelvis + spine_up * k
    neck = E - neck_drop * k
    sh = neck - shoulder_drop * k
    return dict(pelvis=(0, 0, pelvis), spine=(0, 0, spine), neck=(torso_back * k, 0, neck),
                shoulder=(torso_back * k, shoulder_y, sh), elbow=(torso_back * k, shoulder_y, sh - upper * k),
                hip=(0, hip_y, hip), knee=(0, hip_y, knee), ankle=(0, hip_y, ank))


# ================================================================================================================
# Feature library (absolute coordinates)
# ================================================================================================================
def ell_front_x(cx, cz, rx, ry, rz, y, z):
    t = 1.0 - (y / ry) ** 2 - ((z - cz) / rz) ** 2
    return cx + rx * math.sqrt(max(t, 0.0))


def tag_band(Dz, part, c, R_, r, axis=(0, 0, 1)):
    Dz.g(part, "TaG", voxel=0.3).torus(c, R_, r, axis=axis)


def bean_eyes(Dz, *, cx, cz, rx, ry, rz, y, z, size=1.0, slot="Dark", hi="White", brow=None, tilt=0.0,
              lid=None, look=0.0, shell=None):
    """Two bean eyes on the face at height z, +-y apart; white highlights. `shell` (a PG) gives the real surface,
    otherwise the ellipsoid (cx, cz, rx, ry, rz) is used. brow = eyebrow slot. lid = half-lid slot (cool look)."""
    k = Dz.k * size

    def front(yy, zz):
        if shell is not None:
            v = shell.surf_x(yy, zz)
            if v is not None:
                return v
        return ell_front_x(cx, cz, rx, ry, rz, yy, zz)

    for s in (1, -1):
        yy = s * y
        fx = front(yy, z)
        e = Dz.g("Head", slot, voxel=0.22)
        e.ell((fx - 0.25 * k, yy, z), (0.95 * k, 1.3 * k, 1.95 * k), rot=(s * tilt, 0, 0))
        h = Dz.g("Head", hi, voxel=0.16)
        h.sphere((fx + 0.45 * k, yy + s * 0.45 * k + look, z + 0.8 * k), 0.5 * k)
        if lid:
            L = Dz.g("Head", lid, voxel=0.22)
            L.ell((fx - 0.15 * k, yy, z + 0.55 * k), (1.15 * k, 1.55 * k, 1.25 * k), rot=(s * tilt, 0, 0))
            L.half((0, 0, z + 0.55 * k), (0, 0, -1))
        if brow:
            b = Dz.g("Head", brow, voxel=0.25)
            fb = front(yy, z + 3.3 * k)
            b.cap((fb - 0.2 * k, yy - s * 1.2 * k, z + 3.1 * k), (fb - 0.6 * k, yy + s * 1.6 * k, z + 3.6 * k),
                  0.5 * k)


def smile(Dz, *, x, y=1.7, z, size=1.0, slot="Dark", open_=False):
    k = Dz.k * size
    m = Dz.g("Head", slot, voxel=0.2)
    m.cap((x, -y * k, z + 0.35 * k), (x + 0.15 * k, 0, z - 0.25 * k), 0.36 * k)
    m.cap((x + 0.15 * k, 0, z - 0.25 * k), (x, y * k, z + 0.35 * k), 0.36 * k)
    if open_:
        m.ell((x - 0.2 * k, 0, z - 0.4 * k), (0.8 * k, 1.1 * k, 0.8 * k))


def human_head(Dz, *, skin="Skin", rx=13.8, ry=13.0, rz=15.6, cx=1.0, up=2.2, cheeks=1.0, chin=1.0,
               ears=True, nose=True, neck_r=4.6, eye_y=5.3, eye_size=1.6, brow="Hair", mouth=True,
               eye_slot="Dark", lid=None, tilt=0.0, mouth_open=False):
    """Big stylized head. Returns the face ellipsoid (cx, cz, rx, ry, rz) in absolute coordinates."""
    k = Dz.k
    E = Dz.E
    cz = E + up * k
    RX, RY, RZ = rx * k, ry * k, rz * k
    CX = cx * k
    h = Dz.g("Head", skin, k=3.0 * k)
    h.ell((CX, 0, cz), (RX, RY, RZ))
    if cheeks:
        for s in (1, -1):
            h.ell((CX + RX * 0.48, s * RY * 0.45, cz - RZ * 0.42), (RX * 0.5 * cheeks, RY * 0.48 * cheeks,
                                                                  RZ * 0.4 * cheeks), k=3.5 * k)
    if chin:
        h.ell((CX + RX * 0.42, 0, cz - RZ * 0.7), (RX * 0.45 * chin, RY * 0.5 * chin, RZ * 0.32 * chin), k=3.0 * k)
    if nose:
        fx = ell_front_x(CX, cz, RX, RY, RZ, 0, cz - 2.4 * k)
        h.sphere((fx + 0.1 * k, 0, cz - 2.6 * k), 0.95 * k, k=1.0 * k)
    # neck
    nz = Dz.J("neck")[2]
    h.cap((Dz.J("neck")[0] + 0.5 * k, 0, nz - 2.0 * k), (CX * 0.3, 0, cz - RZ * 0.6), neck_r * k, neck_r * 0.95 * k,
          k=2.0 * k)
    if ears:
        for s in (1, -1):
            h.ell((CX - RX * 0.05, s * RY * 0.96, cz - 1.0 * k), (2.0 * k, 1.1 * k, 2.9 * k), k=1.0 * k)
    bean_eyes(Dz, cx=CX, cz=cz, rx=RX, ry=RY, rz=RZ, y=eye_y * k, z=E, size=eye_size, slot=eye_slot, brow=brow,
              lid=lid, tilt=tilt, shell=h)
    if mouth:
        mz = E - 7.0 * k
        mx = (h.surf_x(0, mz) or ell_front_x(CX, cz, RX, RY, RZ, 0, mz)) - 0.12 * k
        smile(Dz, x=mx, z=mz, open_=mouth_open)
    fx = h.surf_x(0, E) or ell_front_x(CX, cz, RX, RY, RZ, 0, E)
    Dz.socket("Head", "eyes", (fx - 1.0 * k, 0, E))
    return (CX, cz, RX, RY, RZ)


def visor_head(Dz, *, slot="Body", glow="Glow", c=None, half=(9, 9, 9), r=4.0, visor_w=7.0, visor_h=1.4,
               visor_slot=None, neck=True, neck_slot="Dark"):
    """Machine head: rounded box with a glowing visor slit at eye height. Sets the eyes socket."""
    k = Dz.k
    E = Dz.E
    c = c if c is not None else (1.0 * k, 0, E + 1.0 * k)
    hx, hy, hz = (x * k for x in half)
    h = Dz.g("Head", slot)
    h.box(c, (hx, hy, hz), r * k)
    fx = c[0] + hx
    v = Dz.g("Head", visor_slot or glow, voxel=0.3)
    v.box((fx - 0.2 * k, 0, E), (0.9 * k, visor_w * k, visor_h * k), 0.7 * k)
    if neck:
        n = Dz.g("Head", neck_slot)
        n.cap((Dz.J("neck")[0], 0, Dz.J("neck")[2] - 2 * k), (c[0] * 0.5, 0, c[2] - hz), 3.6 * k)
    Dz.socket("Head", "eyes", (fx - 0.8 * k, 0, E))
    return fx


def torso_human(Dz, *, slot="Body", chest=1.0, belly=1.0, depth=1.0, top=1.0, waist=1.0, k_blend=6.0,
                fwd=0.0, budget=None):
    """Rounded shirt / jacket shell from the spine to the shoulders."""
    k = Dz.k
    sp = Dz.J("spine")[2]
    nk = Dz.J("neck")[2]
    sh = Dz.J("shoulder")
    L = nk - sp
    wy = abs(sh[1])
    t = Dz.g("Torso", slot, k=k_blend * k, budget=budget)
    t.ell((0.6 * k + fwd, 0, sp + L * 0.62), (10.6 * k * depth, (wy - 2.5 * k) * chest, L * 0.36 * top))
    t.ell((0.2 * k + fwd, 0, sp + L * 0.24), (9.6 * k * depth * belly, (wy - 4.2 * k) * waist * belly, L * 0.3))
    for s in (1, -1):
        t.sphere((sh[0], s * (wy - 3.2 * k), sh[2] - 1.0 * k), 6.0 * k, k=5.0 * k)
    return t


def pelvis_shorts(Dz, *, slot="Dark", belt="Accent", w=1.0, depth=1.0, drop=0.0, belt_r=1.4):
    k = Dz.k
    pz = Dz.J("pelvis")[2]
    hy = abs(Dz.J("hip")[1])
    p = Dz.g("Pelvis", slot, k=4 * k)
    p.ell((0, 0, pz - 1.0 * k - drop), (10.2 * k * depth, (hy + 5.0 * k) * w, 9.5 * k))
    for s in (1, -1):
        p.sphere((0, s * hy, pz - 6.0 * k), 6.8 * k, k=4 * k)
    if belt:
        b = Dz.g("Pelvis", belt, voxel=0.35)
        b.torus((0, 0, pz + 5.0 * k), 9.6 * k, belt_r * k, sx=1.04 * depth, sy=1.0 * (hy + 4.2 * k) / (9.6 * k) * w)
    return p


def arm_standard(Dz, s, *, sleeve="Body", forearm="Skin", hand="Skin", sleeve_len=1.0, ball=6.8, ua_r=(6.0, 5.4),
                 fa_r=(5.2, 4.5), cuff=None, glove=None, hand_size=1.55, fingers=3, band=True, puff=1.0):
    """Upper arm (shoulder ball + sleeve) and forearm with the wrist TaG band and a chunky mitten hand."""
    k = Dz.k
    sd = "L" if s > 0 else "R"
    sh = Dz.J("shoulder", s)
    el = Dz.J("elbow", s)
    U = Dz.g("UpperArm_" + sd, sleeve)
    U.sphere(sh + [0, 0, -1.0 * k], ball * k * puff)
    U.cap(sh + [0, 0, -2.5 * k], el + [0, 0, 1.0 * k], ua_r[0] * k * puff, ua_r[1] * k)
    fa = Dz.g("LowerArm_" + sd, forearm)
    fl = 21.0 * k
    if sleeve_len > 1.0:
        # long sleeve continues over the elbow
        fs = Dz.g("LowerArm_" + sd, sleeve)
        fs.cap(el + [0, 0, 0.5 * k], el + [0, 0, -fl * (sleeve_len - 1.0) * 0.9], ua_r[1] * k * 1.05,
               fa_r[0] * k * 1.12)
    fa.cap(el + [0, 0, 0.0], el + [0, 0, -fl], fa_r[0] * k, fa_r[1] * k)
    fa.ell(el + [0.5 * k, 0, -fl * 0.33], (fa_r[0] * 1.12 * k, fa_r[0] * 1.06 * k, fl * 0.26), k=2 * k)
    wrist = el + [0, 0, -fl]
    if cuff:
        cf = Dz.g("LowerArm_" + sd, cuff, voxel=0.35)
        cf.torus(wrist + [0, 0, 4.5 * k], fa_r[1] * k + 0.5 * k, 1.2 * k)
    if band:
        tag_band(Dz, "LowerArm_" + sd, wrist + [0, 0, 1.0 * k], fa_r[1] * k + 0.3 * k, 1.25 * k)
    mitten(Dz, s, wrist + [0, 0, -1.2 * k], slot=glove or hand, size=hand_size, fingers=fingers)


def mitten(Dz, s, wrist, *, slot="Skin", size=1.55, fingers=3, thumb=True, claws=None):
    """Chunky cartoon hand hanging from `wrist`, palm toward the body midline. Sets the hand socket."""
    k = Dz.k * size
    sd = "L" if s > 0 else "R"
    w = _np(wrist)
    h = Dz.g("LowerArm_" + sd, slot, k=0.9 * k, voxel=0.28)
    palm_c = w + [0.4 * k, -s * 0.2 * k, -4.6 * k]
    h.box(palm_c, (3.7 * k, 1.85 * k, 4.0 * k), 1.6 * k)
    xs = [2.5, 0.0, -2.5] if fingers == 3 else [2.9, 1.0, -0.9, -2.8]
    for x in xs:
        a = w + [x * k * 0.95, -s * 0.15 * k, -8.0 * k]
        b = w + [x * k, -s * 1.7 * k, -11.6 * k]
        h.cap(a, b, 1.35 * k if fingers == 3 else 1.12 * k, 1.2 * k if fingers == 3 else 1.0 * k)
    if thumb:
        h.cap(w + [2.9 * k, -s * 1.0 * k, -2.8 * k], w + [4.9 * k, -s * 2.3 * k, -6.2 * k], 1.3 * k, 1.15 * k)
    if claws:
        c = Dz.g("LowerArm_" + sd, claws, voxel=0.25)
        for x in xs:
            c.cone(w + [x * k, -s * 2.1 * k, -12.8 * k], 0.9 * k, 0.1 * k, 1.0 * k, axis=(0, -s * 0.5, -1))
    Dz.socket("LowerArm_" + sd, "hand", w + [0.4 * k, -s * 2.1 * k, -5.0 * k])


def leg_standard(Dz, s, *, pants="Dark", shin="Dark", th_r=(8.8, 7.2), sh_r=(6.6, 5.2), knee_pad=None,
                 band=True, roll=None, calf=1.0):
    k = Dz.k
    sd = "L" if s > 0 else "R"
    hp = Dz.J("hip", s)
    kn = Dz.J("knee", s)
    an = Dz.J("ankle", s)
    T = Dz.g("Thigh_" + sd, pants)
    T.cap(hp + [0, 0, 1.0 * k], kn + [0, 0, 1.5 * k], th_r[0] * k, th_r[1] * k)
    T.ell(hp + [0.5 * k, 0, -(hp[2] - kn[2]) * 0.38], (th_r[0] * 1.08 * k, th_r[0] * 1.05 * k, (hp[2] - kn[2]) * 0.3))
    S = Dz.g("Shin_" + sd, shin)
    S.cap(kn + [0, 0, 0.5 * k], an + [0, 0, 2.5 * k], sh_r[0] * k, sh_r[1] * k)
    if calf:
        L = kn[2] - an[2]
        S.ell(kn + [-1.2 * k, 0, -L * 0.3], (sh_r[0] * 1.12 * k * calf, sh_r[0] * 1.05 * k * calf, L * 0.26),
              k=2.5 * k)
    if knee_pad:
        kp = Dz.g("Shin_" + sd, knee_pad)
        kp.ell(kn + [sh_r[0] * 0.75 * k, 0, -2.0 * k], (2.6 * k, sh_r[0] * 0.9 * k, 3.4 * k))
    if roll:
        r = Dz.g("Shin_" + sd, roll)
        r.torus(an + [0, 0, 10.0 * k], sh_r[1] * k * 1.08, 1.5 * k)
    if band:
        tag_band(Dz, "Shin_" + sd, an + [0, 0, 4.2 * k], sh_r[1] * k + 0.3 * k, 1.3 * k)


def shoe(Dz, s, *, upper="Extra", sole="Dark", toe=None, length=1.28, width=1.25, height=1.3, collar=None,
         boot=0.0, tongue=None, round_toe=1.0):
    """Big rounded sneaker / boot on the Foot part. Sets the sole and toe sockets (on the floor)."""
    k = Dz.k
    sd = "L" if s > 0 else "R"
    an = Dz.J("ankle", s)
    y = an[1]
    L = length
    H = height
    f = Dz.g("Foot_" + sd, upper, k=2.6 * k)
    f.sphere((an[0], y, an[2] - 0.5 * k + boot * k), 4.6 * k * width)
    if boot > 0:
        f.cap((an[0] - 0.5 * k, y, an[2] - 1 * k), (an[0], y, an[2] + boot * k), 5.0 * k * width, 4.8 * k * width)
    f.ell((an[0] + 4.8 * k * L, y, 4.3 * k * H), (10.8 * k * L, 5.7 * k * width, 4.0 * k * H))
    f.ell((an[0] + 11.5 * k * L, y, 3.7 * k * H), (5.6 * k * L * round_toe, 5.8 * k * width, 3.5 * k * H))
    so = Dz.g("Foot_" + sd, sole, voxel=0.4)
    so.box((an[0] + 5.2 * k * L, y, 0.95 * k), (12.4 * k * L, 6.0 * k * width, 0.95 * k), 0.8 * k)
    if toe:
        t = Dz.g("Foot_" + sd, toe)
        t.ell((an[0] + 13.5 * k * L, y, 3.4 * k * H), (3.6 * k * L, 5.95 * k * width, 3.0 * k * H))
    if collar:
        c = Dz.g("Foot_" + sd, collar, voxel=0.35)
        c.torus((an[0] - 0.3 * k, y, an[2] + 1.5 * k + boot * k), 4.4 * k * width, 1.1 * k)
    if tongue:
        t = Dz.g("Foot_" + sd, tongue)
        t.ell((an[0] + 3.0 * k, y, 7.2 * k * H), (3.5 * k, 3.4 * k * width, 2.4 * k))
    Dz.socket("Foot_" + sd, "sole", (an[0] + 5.2 * k * L, y, 0.0))
    Dz.socket("Foot_" + sd, "toe", (an[0] + 17.4 * k * L, y, 0.0))


def foot_mech(Dz, s, *, slot="Metal", sole="Dark", glow=None, length=1.0, width=1.0, claws=0):
    """Chunky machine foot (rounded block + toe pads)."""
    k = Dz.k
    sd = "L" if s > 0 else "R"
    an = Dz.J("ankle", s)
    y = an[1]
    f = Dz.g("Foot_" + sd, slot)
    f.sphere((an[0], y, an[2]), 4.8 * k * width)
    f.box((an[0] + 4.5 * k * length, y, 3.6 * k), (10.5 * k * length, 6.2 * k * width, 3.4 * k), 2.4 * k)
    so = Dz.g("Foot_" + sd, sole, voxel=0.45)
    so.box((an[0] + 4.5 * k * length, y, 0.9 * k), (11.2 * k * length, 6.6 * k * width, 0.9 * k), 0.7 * k)
    for i in range(claws):
        yy = y + (i - (claws - 1) / 2.0) * 4.2 * k * width
        so.ell((an[0] + 15.0 * k * length, yy, 1.8 * k), (2.8 * k, 1.8 * k * width, 1.8 * k))
    if glow:
        gl = Dz.g("Foot_" + sd, glow, voxel=0.3)
        gl.box((an[0] + 15.2 * k * length, y, 3.6 * k), (0.4 * k, 3.6 * k * width, 0.6 * k), 0.3 * k)
    Dz.socket("Foot_" + sd, "sole", (an[0] + 4.5 * k * length, y, 0.0))
    Dz.socket("Foot_" + sd, "toe", (an[0] + 15.7 * k * length, y, 0.0))


def hair_cap(Dz, face, slot="Hair", *, grow=1.08, back=1.0, front_cut=0.25, k_blend=2.0, top=1.0, sides=1.0):
    """Hair shell over the skull, open at the face. Returns the PartGroup to add tufts to."""
    CX, cz, RX, RY, RZ = face
    k = Dz.k
    h = Dz.g("Head", slot, k=k_blend * k)
    h.ell((CX - 0.6 * k * back, 0, cz + 1.2 * k * top), (RX * grow, RY * grow * sides, RZ * grow * top))
    # open the face: cut a forward-down wedge
    h.half((CX + RX * front_cut, 0, cz + 0.5 * k), (-1.0, 0, 0.85), op="s", k=1.2 * k)
    return h



# ---------------------------------------------------------------------------------------------- more pieces
def tufts(h, face, k, items, r=2.4, tip=0.6, kb=1.0):
    """Hair tufts: items = [(y, z_from_center, length, x_out(0..1), angle)] hanging down-forward over the face."""
    CX, cz, RX, RY, RZ = face
    for y, z, ln, xo, ang in items:
        a = _np((CX + RX * xo, y * k, cz + z * k))
        b = a + _np((math.sin(math.radians(ang)) * ln * k * 0.4, y * 0.08 * k, -ln * k))
        h.cap(a, b, r * k, tip * k, k=kb * k)


def spikes(h, face, k, items, r=3.0, kb=1.2):
    """Spiky hair: items = [(direction (x, y, z), length)] from the skull centre."""
    CX, cz, RX, RY, RZ = face
    c = _np((CX - 1.0 * k, 0, cz + 1.5 * k))
    for d, ln in items:
        d = _np(d)
        d = d / np.linalg.norm(d)
        a = c + d * np.array([RX, RY, RZ]) * 0.75
        h.cap(a, a + d * ln * k, r * k, 0.4 * k, k=kb * k)


def ponytail(h, face, k, length=22.0, r=4.0, high=1.0, swing=0.0):
    CX, cz, RX, RY, RZ = face
    a = _np((CX - RX * 0.95, swing * k, cz + RZ * 0.35 * high))
    h.sphere(a, r * 1.05 * k, k=1.0 * k)
    b = a + _np((-6 * k, swing * 0.5 * k, -length * 0.45 * k))
    c = b + _np((-2 * k, swing * k, -length * 0.55 * k))
    h.cap(a, b, r * k, r * 0.85 * k, k=1.5 * k)
    h.cap(b, c, r * 0.85 * k, 0.8 * k, k=1.5 * k)


def kasa(Dz, face, slot="Extra", *, r=24.0, h=9.0, tip=1.0, band=None, z=None):
    """Conical straw hat (kasa) sitting on the head."""
    CX, cz, RX, RY, RZ = face
    k = Dz.k
    zc = (z if z is not None else cz + RZ * 0.62)
    g = Dz.g("Head", slot, k=1.2 * k)
    g.cone((CX - 0.5 * k, 0, zc + h * 0.5 * k), r * k, tip * k, h * 0.5 * k)
    g.half((0, 0, zc - 0.3 * k), (0, 0, -1), op="i")
    if band:
        b = Dz.g("Head", band, voxel=0.35)
        b.torus((CX - 0.5 * k, 0, zc + 2.2 * k), r * k * 0.62, 0.9 * k)
    return g


def goggles(Dz, face, *, strap="Dark", frame="Metal", lens="Glow", z=None, up=True):
    CX, cz, RX, RY, RZ = face
    k = Dz.k
    zz = z if z is not None else cz + RZ * (0.55 if up else 0.05)
    st = Dz.g("Head", strap, voxel=0.35)
    st.torus((CX - 0.6 * k, 0, zz), RX * 1.05, 1.1 * k, sx=1.0, sy=RY / RX)
    for s in (1, -1):
        fx = ell_front_x(CX, cz, RX, RY, RZ, s * 4.6 * k, zz)
        f = Dz.g("Head", frame, voxel=0.3)
        f.cyl((fx + 0.6 * k, s * 4.6 * k, zz), 3.1 * k, 1.5 * k, 0.6 * k, axis=(1, 0, 0.35 if up else 0))
        L = Dz.g("Head", lens, voxel=0.25)
        L.cyl((fx + 1.6 * k, s * 4.6 * k, zz + (0.5 * k if up else 0)), 2.4 * k, 0.6 * k, 0.3 * k,
              axis=(1, 0, 0.35 if up else 0))


def side_mask(Dz, face, *, slot="White", mark="Glow", s=1, z=None):
    """Fox mask worn on the side of the head."""
    CX, cz, RX, RY, RZ = face
    k = Dz.k
    zz = z if z is not None else cz + 3 * k
    c = _np((CX - 1 * k, s * (RY + 1.2 * k), zz))
    m = Dz.g("Head", slot, k=1.0 * k)
    m.ell(c, (5.2 * k, 2.0 * k, 6.4 * k))
    m.cone(c + [5.4 * k, s * 0.6 * k, -2.0 * k], 2.2 * k, 0.6 * k, 2.4 * k, axis=(1, 0, -0.4))
    for dx in (-2.8, 2.8):
        m.cone(c + [dx * k, s * 0.2 * k, 6.2 * k], 1.8 * k, 0.4 * k, 2.2 * k)
    mk = Dz.g("Head", mark, voxel=0.2)
    for dx in (-1.8, 1.8):
        mk.ell(c + [dx * k + 1 * k, s * 1.9 * k, 1.5 * k], (1.4 * k, 0.4 * k, 0.6 * k), rot=(0, 0, 20 * dx))


def coat_skirt(Dz, slot="Body", *, length=22.0, flare=1.0, split=True, hem=None, back_only=False):
    """Coat tails on the pelvis, flaring over the thighs (rigid, so keep it short of the knees)."""
    k = Dz.k
    pz = Dz.J("pelvis")[2]
    hy = abs(Dz.J("hip")[1])
    r0 = hy + 6.0 * k
    g = Dz.g("Pelvis", slot, k=3 * k)
    g.cone((0, 0, pz - length * 0.5 * k), r0 * 1.12 * flare, r0 * 0.92, length * 0.5 * k)
    g.ell((0, 0, pz + 1 * k), (10.4 * k, r0 * 0.95, 7 * k))
    g.half((0, 0, pz - length * k), (0, 0, -1), op="i")
    if split:
        g.box((r0 * 1.15, 0, pz - length * 0.62 * k), (5 * k, 2.6 * k, length * 0.45 * k), 1 * k, op="s", k=1.0 * k)
    if back_only:
        g.half((2 * k, 0, 0), (1, 0, 0), op="i", k=1.5 * k)
    if hem:
        hm = Dz.g("Pelvis", hem, voxel=0.3)
        hm.torus((0, 0, pz - length * k + 0.6 * k), r0 * 1.12 * flare - 0.2 * k, 0.55 * k)
        if split:
            hm.box((r0 * 1.15, 0, pz - length * 0.62 * k), (5.6 * k, 3.2 * k, length * 0.5 * k), 1 * k, op="s",
                   k=0.5 * k)
        if back_only:
            hm.half((2 * k, 0, 0), (1, 0, 0), op="i")
    return g


def cape(Dz, slot="Body", *, length=60.0, width=1.0, flare=1.3, hem=None, tatter=0):
    """Cape hanging from the shoulders down the back (on the Torso part)."""
    k = Dz.k
    nk = Dz.J("neck")[2]
    sh = Dz.J("shoulder")
    wy = abs(sh[1]) * width
    g = Dz.g("Torso", slot, k=2.5 * k)
    top = _np((-9.5 * k, 0, nk - 2 * k))
    bot = _np((-15.0 * k, 0, nk - length * k))
    g.box((top + bot) * 0.5, (1.6 * k, wy * (1 + flare) * 0.5, length * 0.5 * k), 1.4 * k, rot=(0, -8, 0))
    g.ell(top + [2 * k, 0, 0], (6 * k, wy * 0.95, 5 * k))
    for i in range(tatter):
        y = (i - (tatter - 1) / 2.0) * wy * (1 + flare) / tatter
        g.cone(bot + [0, y, -2 * k], 3.0 * k, 0.5 * k, 4 * k, op="s", k=0.6 * k)
    if hem:
        hm = Dz.g("Torso", hem, voxel=0.3)
        hm.box(bot + [-0.6 * k, 0, 1.4 * k], (1.3 * k, wy * (1 + flare) * 0.5, 0.9 * k), 0.4 * k, rot=(0, -8, 0))
    return g


def pauldron(Dz, s, slot="Body", *, size=1.0, trim=None, up=1.0):
    k = Dz.k
    sd = "L" if s > 0 else "R"
    sh = Dz.J("shoulder", s)
    g = Dz.g("UpperArm_" + sd, slot, k=2 * k)
    g.ell(sh + [0, s * 1.6 * k * size, 1.5 * k * up], (8.0 * k * size, 7.6 * k * size, 6.0 * k * size))
    g.half(sh + [0, 0, -2.5 * k], (0, 0, -1), op="i", k=1.5 * k)
    if trim:
        t = Dz.g("UpperArm_" + sd, trim, voxel=0.3)
        t.torus(sh + [0, s * 1.6 * k * size, -2.0 * k], 7.4 * k * size, 0.7 * k, sx=1.04, sy=1.0)
    return g


def backpack(Dz, slot="Accent", *, half=(5, 10, 12), z=None, x=None, r=3.0, straps="Dark"):
    k = Dz.k
    sp, nk = Dz.J("spine")[2], Dz.J("neck")[2]
    zz = z if z is not None else (sp + nk) * 0.55
    xx = x if x is not None else -12.0 * k - half[0] * k
    g = Dz.g("Torso", slot, k=1.5 * k)
    g.box((xx, 0, zz), tuple(v * k for v in half), r * k)
    if straps:
        st = Dz.g("Torso", straps, voxel=0.35)
        for s in (1, -1):
            st.cap((xx + half[0] * k, s * half[1] * 0.6 * k, zz + half[2] * 0.8 * k), (9.5 * k, s * 7 * k, nk - 3 * k),
                   1.0 * k)
    return g


# ================================================================================================================
# Shared helpers for paired parts
# ================================================================================================================
def both(fn):
    for s in (1, -1):
        fn(s)


def side(s):
    return "L" if s > 0 else "R"


# ================================================================================================================
# Hero recipes (hero_designs.py) and the build
# ================================================================================================================
import hero_designs  # noqa: E402  (needs the library above)


def build_hero(key, log=None):
    sk, recipe = hero_designs.RECIPES[key]
    E, W, size = TARGETS[key]
    Dz = D(key, sk(E, W))
    recipe(Dz)
    rig = Dz.rig
    for pn in PARTS:     # nothing below the floor in rest pose (smooth blends and round ends can bulge under it)
        for G in Dz.groups[pn]:
            PG(G, Dz.O[pn]).half((0.0, 0.0, 0.0), (0.0, 0.0, -1.0), op="i")
    for pn in PARTS:
        sdfbake.bake_part(rig.parts[pn], [pg for pg in Dz.groups[pn]], BODY_BUDGET[btype(pn)], log=log)
    # hard cap: body triangles (before the 0.75x outline hull) so the hero stays under TRI_BUDGET
    body_max = int(TRI_BUDGET / 1.84)
    tot = sum(rig.parts[pn].tri_count() for pn in PARTS)
    if tot > body_max:
        f = body_max / float(tot)
        for pn in PARTS:
            P = rig.parts[pn]
            P.bm = sdfbake.decimate(P.bm, int(P.tri_count() * f))
    # hit the roster width exactly (C++ sizes the capsule from it): uniform Y scale of the whole rest pose
    m = rig.measures()
    f = (W - 2.0 * OUTLINE_CM[size]) / max(m["width"], 1.0)
    if abs(f - 1.0) > 0.005:
        sdfbake.scale_y(rig.parts.values(), f)
    rig.width_fix = f
    for pn in PARTS:
        sdfbake.add_outline(rig.parts[pn], OUTLINE_CM[size], ratio=0.75)
    return rig


def build_all(only=None, log=print):
    out = {}
    for hid in HERO_IDS:
        if only and hid not in only:
            continue
        rig = build_hero(hid, log=log)
        for part in rig.parts.values():
            part.finalize()
        out[hid] = rig
        m = rig.measures()
        log("HERO %-9s eye %.1f width %.1f (fix %.3f) tris %d" % (hid, m["eye_height"], m["width"], rig.width_fix,
                                                                 m["tris"]))
    return out
