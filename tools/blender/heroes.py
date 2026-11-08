"""
heroes.py - the six P2 heroes as 11 rigid parts each (procedural animation rotates the parts around their pivots).

Rest pose: standing straight, arms hanging straight down, legs straight down, feet on z = 0.
Every part is modelled in its own local space: origin = joint with the parent, limbs extend toward -Z.
Blender axes: +X forward, +Y = LEFT, +Z up (UE gets (x, -y, z)).  Side factor s = +1 (left) / -1 (right).

Sockets (local, cm):
  Pelvis: spine, hip_l, hip_r   Torso: neck, shoulder_l, shoulder_r   Head: eyes
  UpperArm: elbow   LowerArm: hand (palm centre)   Thigh: knee   Shin: sole (sole centre, on the floor)
"""
import math
from mathutils import Vector, Matrix
from tagmesh import (Part, vec, frame, T, R, S, sphere, capsule, cylinder, torus, band, box, loft, loft_x, loft_pts, tube,
                     prism, se_points, lathe, bezier, add_capsule, add_cyl, add_sphere, add_box, add_band,
                     add_torus, add_tube, add_loft, add_prism)

PARTS = ["Pelvis", "Torso", "Head", "UpperArm_L", "UpperArm_R", "LowerArm_L", "LowerArm_R",
         "Thigh_L", "Thigh_R", "Shin_L", "Shin_R"]
HERO_IDS = ["Gantetsu", "Minamo", "Rei", "Hayate", "Tsumugi", "Hibiki"]
TARGETS = {  # floor-to-eyes (cm), overall width (cm), size class
    "Gantetsu": (250.0, 95.0, "XL"), "Minamo": (210.0, 70.0, "L"), "Rei": (175.0, 50.0, "M"),
    "Hayate": (190.0, 50.0, "M"), "Tsumugi": (145.0, 40.0, "S"), "Hibiki": (175.0, 50.0, "M"),
}
TRI_BUDGET = 8000


class Rig:
    """Holds the parts of one hero and the rest-pose pivots derived from the socket chain."""

    def __init__(self, hero_id, sk):
        self.id = hero_id
        self.sk = {k: Vector(v) for k, v in sk.items()}
        self.parts = {n: Part("SM_%s_%s" % (hero_id, n)) for n in PARTS}
        sk = self.sk
        m = lambda v: Vector((v.x, -v.y, v.z))
        p = self.parts
        p["Pelvis"].socket("spine", sk["spine"] - sk["pelvis"])
        p["Pelvis"].socket("hip_l", sk["hip"] - sk["pelvis"])
        p["Pelvis"].socket("hip_r", m(sk["hip"]) - sk["pelvis"])
        p["Torso"].socket("neck", sk["neck"] - sk["spine"])
        p["Torso"].socket("shoulder_l", sk["shoulder"] - sk["spine"])
        p["Torso"].socket("shoulder_r", m(sk["shoulder"]) - sk["spine"])
        for sd in ("L", "R"):
            p["UpperArm_" + sd].socket("elbow", sk["elbow"] - sk["shoulder"])
            p["Thigh_" + sd].socket("knee", sk["knee"] - sk["hip"])
        # Head.eyes, LowerArm.hand and Shin.sole are set by the builders (they depend on the geometry).

    # convenient lengths
    def L(self, a, b):
        return (self.sk[a] - self.sk[b]).length

    def rest_origins(self):
        """Pivot of every part in rest pose, chained through the sockets exactly like the game does."""
        p = self.parts
        o = {"Pelvis": self.sk["pelvis"].copy()}
        o["Torso"] = o["Pelvis"] + p["Pelvis"].sockets["spine"]
        o["Head"] = o["Torso"] + p["Torso"].sockets["neck"]
        for sd, s in (("L", "l"), ("R", "r")):
            o["UpperArm_" + sd] = o["Torso"] + p["Torso"].sockets["shoulder_" + s]
            o["LowerArm_" + sd] = o["UpperArm_" + sd] + p["UpperArm_" + sd].sockets["elbow"]
            o["Thigh_" + sd] = o["Pelvis"] + p["Pelvis"].sockets["hip_" + s]
            o["Shin_" + sd] = o["Thigh_" + sd] + p["Thigh_" + sd].sockets["knee"]
        return o

    def measures(self):
        o = self.rest_origins()
        p = self.parts
        eyes = o["Head"] + p["Head"].sockets["eyes"]
        sole_l = o["Shin_L"] + p["Shin_L"].sockets["sole"]
        sole_r = o["Shin_R"] + p["Shin_R"].sockets["sole"]
        floor = (sole_l.z + sole_r.z) * 0.5
        lo = Vector((1e9, 1e9, 1e9))
        hi = Vector((-1e9, -1e9, -1e9))
        for n, part in p.items():
            a, b = part.bounds()
            for i in range(3):
                lo[i] = min(lo[i], a[i] + o[n][i])
                hi[i] = max(hi[i], b[i] + o[n][i])
        return dict(eye_height=eyes.z - floor, width=hi.y - lo.y, height=hi.z - floor,
                    depth=hi.x - lo.x, bounds_lo=lo, bounds_hi=hi,
                    tris=sum(pt.tri_count() for pt in p.values()))


# ================================================================================================================
# Shared limb helpers (local space of the part)
# ================================================================================================================
def hand(P, wz, s, k=1.0, slot="Skin", fingers=4, finger_slot=None, curl=1.0, chunky=1.0):
    """Relaxed hanging hand, palm facing the body midline (-s*Y).  Wrist at (0, 0, wz).  Sets socket 'hand'."""
    fs = finger_slot or slot
    pw, pt, pl = 4.1 * k * chunky, 1.65 * k * chunky, 9.0 * k
    add_loft(P, [
        (wz - pl, 0.3 * k, 0.0, pw * 0.97, pt * 0.85, 2.6),
        (wz - pl * 0.62, 0.3 * k, 0.0, pw, pt, 2.6),
        (wz - pl * 0.25, 0.2 * k, 0.0, pw * 0.95, pt * 1.02, 2.5),
        (wz + 0.6 * k, 0.0, 0.0, pw * 0.75, pt * 0.95, 2.2)], slot, k=12,
        pole0=(0.3 * k, 0.0, wz - pl - 0.7 * k))
    zk = wz - pl + 0.6 * k
    if fingers == 4:
        fx = [2.9, 1.0, -0.9, -2.75]
        fl = [7.4, 8.1, 7.5, 6.0]
        fr = [0.85, 0.88, 0.84, 0.74]
    else:
        fx = [2.4, 0.0, -2.4]
        fl = [7.6, 8.2, 7.0]
        fr = [1.0, 1.05, 0.95]
    for x, l, r in zip(fx, fl, fr):
        x, l, r = x * k * chunky, l * k, r * k * chunky
        pts = [(x, 0.0, zk + 0.8 * k), (x, -s * 0.25 * k * curl, zk - 0.45 * l),
               (x, -s * 1.3 * k * curl, zk - 0.8 * l), (x, -s * 2.5 * k * curl, zk - 0.98 * l)]
        add_tube(P, pts, [r, r * 0.97, r * 0.92, r * 0.85], fs, segs=6, cap0=True, pole1=r * 0.8)
    th = [(3.0 * k * chunky, -s * 0.4 * k, wz - 2.2 * k), (4.5 * k * chunky, -s * 1.5 * k, wz - 5.0 * k),
          (4.6 * k * chunky, -s * 2.6 * k, wz - 7.6 * k)]
    add_tube(P, th, [1.1 * k * chunky, 1.0 * k * chunky, 0.92 * k * chunky], fs, segs=6, cap0=True,
             pole1=0.8 * k)
    P.socket("hand", (0.3 * k, -s * (pt + 0.05), wz - pl * 0.55))


def shoe(P, floor, s, k=1.0, upper="Accent", sole="Dark", trim=None, toe=None, length=1.0, height=1.0):
    """Sneaker-like shoe; ankle above (0,0), toe toward +X, sole bottom on z = floor.  Sets socket 'sole'."""
    kl = k * length
    sec = [(-7.0, 3.6, 4.0), (-4.6, 4.5, 5.4), (0.0, 4.8, 5.3), (5.0, 5.0, 4.3), (11.0, 4.9, 3.3), (16.0, 4.2, 2.6)]
    base = floor + 1.3 * k
    secs = [(x * kl, 0.0, base + bz * k * height, ay * k, bz * k * height, 2.6) for (x, ay, bz) in sec]
    P.add(loft_x(secs, k=14, pole1=(19.0 * kl, 0.0, base + 2.2 * k * height),
                 pole0=(-7.8 * kl, 0.0, base + 3.8 * k * height)), upper, True)
    cx = 5.6 * kl
    pts = se_points(13.4 * kl, 5.15 * k, 2.6, 16)
    add_prism(P, [(cx + x, y) for x, y in pts], 1.6 * k, sole, T(0, 0, floor), bevel=0.35 * k)
    if trim:
        # side swoosh / toe cap
        P.add(loft_x([(2.0 * kl, 0.0, base + 1.6 * k, 5.25 * k, 1.1 * k, 2.4),
                      (12.0 * kl, 0.0, base + 1.3 * k, 5.15 * k, 1.0 * k, 2.4)], k=14), trim, True)
    if toe:
        add_sphere(P, (15.0 * kl, 0, base + 1.6 * k), (4.0 * kl, 4.3 * k, 2.4 * k), toe, segs=10, rings=6)
    P.socket("sole", (cx, 0.0, floor))


def tag_band(P, z, r, w=3.0, t=1.3, axis=(0, 0, 1), center=None, segs=16):
    c = center if center is not None else (0, 0, z)
    add_band(P, c, axis, r - 0.35, r + t, w, "TaG", segs=segs)


def shoulder_pad(P, s, r, slot="Team", dz=1.6, out=1.4, sx=1.18, sy=1.12, sz=0.72, tilt=22.0, segs=12):
    add_sphere(P, (0, s * out, dz), (r * sx, r * sy, r * sz), slot, segs=segs, rings=7, rot=R(-s * tilt, 'X'))


def head_ellipsoid(P, c, r, slot="Skin", segs=16, rings=10):
    add_sphere(P, c, r, slot, segs=segs, rings=rings)


def eyes_pair(P, x, y, z, r=(1.0, 1.3, 1.5), slot="Dark"):
    for s in (1, -1):
        add_sphere(P, (x, s * y, z), r, slot, segs=8, rings=6)


# ================================================================================================================
# Rei - M scorer, mysterious hoodie genius from Urakaze
# ================================================================================================================
def build_rei():
    sk = dict(pelvis=(0, 0, 99), spine=(0, 0, 108), neck=(-1, 0, 152), shoulder=(-1, 17.8, 146),
              elbow=(-1, 17.8, 116), hip=(0, 9.5, 93), knee=(0, 9.5, 50))
    rig = Rig("Rei", sk)
    p = rig.parts
    # ---- Pelvis (jeans/joggers)
    P = p["Pelvis"]
    add_loft(P, [(-15, 0, 0, 7.8, 9.0, 2.2), (-9, 0, 0, 9.6, 14.4, 2.4), (-3, 0, 0, 10.1, 15.2, 2.5),
                 (3, 0, 0, 10.2, 14.9, 2.5), (8, 0, 0, 9.8, 14.0, 2.4), (12, 0, 0, 8.8, 12.0, 2.2)], "Dark", k=16)
    add_loft(P, [(5.0, 0, 0, 10.5, 14.8, 2.5), (8.5, 0, 0, 10.3, 14.4, 2.5)], "Accent", k=16, cap0=False,
             cap1=False)  # waistband
    # ---- Torso (hoodie)
    P = p["Torso"]
    secs = [(-6, 0.0, 0, 10.4, 14.6, 2.4), (0, 0.2, 0, 11.0, 15.6, 2.5), (8, 0.5, 0, 11.3, 16.0, 2.5),
            (18, 1.0, 0, 12.0, 16.6, 2.6), (27, 1.0, 0, 12.3, 17.4, 2.7), (34, 0.0, 0, 11.6, 17.9, 2.8),
            (39, -1.0, 0, 9.6, 16.6, 2.6), (42.5, -1.2, 0, 7.2, 11.2, 2.2), (45.5, -1.2, 0, 6.0, 7.0, 2.0)]
    add_loft(P, secs, "Body", k=20)
    add_loft(P, [(-6.5, 0.0, 0, 10.7, 14.9, 2.4), (-2.5, 0.1, 0, 11.2, 15.8, 2.5)], "Dark", k=20, cap0=False,
             cap1=False)  # ribbed hem
    add_loft(P, [(21.5, 1.0, 0, 12.45, 17.15, 2.65), (24.5, 1.0, 0, 12.6, 17.45, 2.7)], "Team", k=20,
             cap0=False, cap1=False)  # chest stripe
    # kangaroo pocket
    add_prism(P, [(-9.5, 0), (9.5, 0), (7.0, 11.0), (-7.0, 11.0)], 1.4, "Body",
              Matrix.Translation((10.9, 0, 2.0)) @ R(90, 'Y') @ R(90, 'Z'), bevel=0.4)
    add_box(P, (12.1, 0, 7.5), (0.6, 13.0, 1.0), "Dark")
    # drawstrings
    for s in (1, -1):
        add_tube(P, [(6.0, s * 2.6, 42.5), (8.6, s * 2.9, 36.0), (11.2, s * 2.9, 29.0)], 0.45, "Accent", segs=5)
        add_sphere(P, (11.3, s * 2.9, 28.4), 0.75, "Accent", segs=6, rings=4)
    # back number plate + glitch blocks
    add_prism(P, [(x, y) for x, y in se_points(6.5, 6.0, 4.0, 12)], 0.9, "Team",
              Matrix.Translation((-11.6, 0, 25.0)) @ R(-90, 'Y'), bevel=0.25)
    for (x, y, z, sy, sz) in [(-11.0, 8.5, 33.0, 4.5, 1.0), (-11.6, 5.5, 31.2, 2.0, 0.8), (-11.3, 10.0, 29.5, 3.0, 0.7)]:
        add_box(P, (x, y, z), (1.2, sy, sz), "Glow")
    # collar / hood opening ring
    add_torus(P, (-0.6, 0, 43.5), (0, 0, 1), 6.2, 1.5, "Body", segs=14, csegs=6, scale=(1.15, 1.0, 1.0))
    # ---- Head (hood up, silver bangs)
    P = p["Head"]
    add_capsule(P, (0, 0, -3), (1.0, 0, 9), 5.0, 5.0, "Skin", segs=12)
    head_ellipsoid(P, (1.8, 0, 19.5), (10.4, 8.8, 11.8))
    eyes_pair(P, 10.4, 3.3, 22.9, r=(1.0, 1.35, 1.6))
    add_sphere(P, (-3.4, 0, 21.6), (10.6, 9.7, 12.4), "Body", segs=16, rings=9)  # hood
    add_tube(P, [(-11.5, 0, 27.0), (-15.5, 0, 22.0), (-17.0, 0, 15.0)], [4.6, 2.8, 1.1], "Body", segs=8,
             pole1=1.0)  # hood point drooping down the back
    add_torus(P, (5.0, 0, 21.8), (1, 0, 0.25), 1.0, 1.0, "Body", segs=16, csegs=6, scale=(8.1, 10.4, 1.0))
    for (y, z, ln) in [(-4.6, 29.4, 3.6), (-1.5, 30.2, 4.4), (1.8, 30.0, 4.2), (4.8, 29.0, 3.4)]:
        add_tube(P, [(6.6, y, z + 0.6), (9.6, y * 1.05, z - 1.2), (10.8, y * 1.1, z - ln)], [1.6, 1.2, 0.7],
                 "Hair", segs=5, pole1=0.7)
    add_box(P, (1.0, 9.9, 24.0), (3.5, 0.9, 0.5), "Glow")
    add_box(P, (-0.5, 10.0, 22.6), (1.8, 0.9, 0.4), "Glow")
    p["Head"].socket("eyes", (10.0, 0.0, 23.0))
    # ---- Arms
    for sd, s in (("L", 1), ("R", -1)):
        P = p["UpperArm_" + sd]
        add_capsule(P, (0, 0, -30), (0, 0, 0), 4.6, 5.6, "Body", segs=12)
        shoulder_pad(P, s, 5.8, out=0.6, sy=1.0)
        if s == 1:
            add_box(P, (1.0, s * 5.2, -14.0), (2.4, 0.8, 3.6), "Glow")
            add_box(P, (-1.2, s * 4.9, -17.5), (1.6, 0.8, 1.2), "Glow")
        P = p["LowerArm_" + sd]
        add_capsule(P, (0, 0, -24.5), (0, 0, 0), 3.5, 4.5, "Body", segs=12)
        add_band(P, (0, 0, -21.0), (0, 0, 1), 3.4, 4.5, 3.0, "Body", segs=12)
        tag_band(P, -24.2, 3.2, w=2.6, t=1.2)
        hand(P, -26.0, s, 1.08, "Skin")
    # ---- Legs
    for sd, s in (("L", 1), ("R", -1)):
        P = p["Thigh_" + sd]
        add_capsule(P, (0, 0, -43), (0, 0, 0), 5.8, 7.7, "Dark", segs=12)
        add_capsule(P, (0, s * 5.2, -34), (0, s * 7.0, -3), 1.1, 1.2, "Team", segs=6, cap_rings=2)
        P = p["Shin_" + sd]
        add_capsule(P, (0, 0, -38), (0, 0, 0), 4.4, 5.8, "Dark", segs=12)
        add_cyl(P, (0, 0, -41.5), (0, 0, -36), 5.0, 5.3, "Dark", segs=12)
        tag_band(P, -34.0, 4.6, w=2.6, t=1.3)
        shoe(P, -50.0, s, 1.02, upper="Accent", sole="Dark", trim="Body")
    return rig


# ================================================================================================================
# Hayate - tall M scorer, corporate ace (white / gold, scope eyepiece)
# ================================================================================================================
def build_hayate():
    sk = dict(pelvis=(0, 0, 108), spine=(0, 0, 118), neck=(-1, 0, 166), shoulder=(-1, 18.5, 159),
              elbow=(-1, 18.5, 127), hip=(0, 9.2, 101.5), knee=(0, 9.2, 55))
    rig = Rig("Hayate", sk)
    p = rig.parts
    P = p["Pelvis"]
    add_loft(P, [(-15, 0, 0, 7.8, 9.0, 2.2), (-9, 0, 0, 9.4, 13.8, 2.4), (-3, 0, 0, 9.8, 14.4, 2.5),
                 (3, 0, 0, 9.8, 14.2, 2.5), (8, 0, 0, 9.4, 13.4, 2.4), (12, 0, 0, 8.4, 11.6, 2.2)], "Body", k=16)
    add_loft(P, [(5.5, 0, 0, 10.05, 14.05, 2.5), (9.0, 0, 0, 9.75, 13.55, 2.45)], "Dark", k=16, cap0=False,
             cap1=False)  # belt
    add_box(P, (10.2, 0, 7.2), (1.0, 4.4, 3.0), "Metal", bevel=0.4)
    # ---- Torso: fitted jacket, high collar, gold piping, chest sash
    P = p["Torso"]
    add_loft(P, [(-7, 0, 0, 9.7, 13.4, 2.3), (2, 0.3, 0, 10.0, 13.9, 2.4), (14, 0.8, 0, 10.8, 15.2, 2.5),
                 (26, 1.2, 0, 11.8, 17.2, 2.6), (35, 0.6, 0, 11.4, 18.6, 2.8), (41, -0.6, 0, 9.4, 17.6, 2.6),
                 (45, -1.0, 0, 6.8, 10.5, 2.2), (48, -1.0, 0, 5.6, 6.4, 2.0)], "Body", k=20)
    add_cyl(P, (-1.0, 0, 43.5), (-1.2, 0, 50.0), 6.6, 6.2, "Body", segs=14)
    add_band(P, (-1.2, 0, 50.0), (0, 0, 1), 5.9, 6.7, 0.8, "Accent", segs=14)
    front = [(10.1, 0, -6), (10.4, 0, 2), (11.2, 0, 14), (12.3, 0, 24), (12.6, 0, 30), (11.6, 0, 37), (9.0, 0, 42.6)]
    add_tube(P, front, 0.55, "Accent", segs=5)
    for s in (1, -1):
        add_tube(P, [(9.0, s * 3.0, 42.6), (9.2, s * 9.0, 40.6), (6.0, s * 15.0, 40.0)], 0.5, "Accent", segs=5)
    add_loft(P, [(29.5, 1.0, 0, 12.0, 18.05, 2.75), (33.0, 0.8, 0, 11.75, 18.65, 2.8)], "Team", k=20,
             cap0=False, cap1=False)
    add_prism(P, [(x, y) for x, y in se_points(6.0, 7.0, 4.0, 12)], 0.8, "Team",
              Matrix.Translation((-11.2, 0, 20.0)) @ R(-90, 'Y'), bevel=0.25)
    add_prism(P, [(0, 1.8), (-1.4, 0), (0, -1.8), (1.4, 0)], 0.6, "Glow",
              Matrix.Translation((12.0, 6.5, 27.0)) @ R(90, 'Y') @ R(90, 'Z'))
    for s in (1, -1):  # coat tails
        add_box(P, (-8.6, s * 6.4, -10.0), (1.6, 11.0, 18.0), "Body", bevel=0.5, rot=R(-8, 'Y'))
        add_box(P, (-9.6, s * 6.4, -18.6), (1.7, 11.2, 1.2), "Accent", rot=R(-8, 'Y'))
    # ---- Head: slick hair, scope over the right eye
    P = p["Head"]
    add_capsule(P, (0, 0, -3), (1.0, 0, 9), 4.9, 4.9, "Skin", segs=12)
    head_ellipsoid(P, (1.8, 0, 20.5), (10.3, 8.8, 12.0))
    eyes_pair(P, 10.4, 3.3, 24.0)
    add_sphere(P, (-1.0, 0, 23.6), (11.0, 9.6, 10.9), "Hair", segs=16, rings=9)
    add_sphere(P, (6.5, 1.0, 30.5), (6.5, 8.0, 3.6), "Hair", segs=12, rings=6, rot=R(-18, 'Y'))
    add_sphere(P, (-7.0, 0, 26.0), (6.0, 8.6, 6.0), "Hair", segs=10, rings=6)
    y = -3.3
    add_cyl(P, (10.0, y, 24.0), (14.6, y, 24.0), 2.35, 2.2, "Metal", segs=12)
    add_cyl(P, (14.4, y, 24.0), (14.9, y, 24.0), 1.85, 1.85, "Glow", segs=12)
    add_box(P, (3.5, -9.4, 24.5), (13.0, 1.0, 1.6), "Metal", bevel=0.3)
    add_box(P, (-2.0, -9.6, 27.5), (3.0, 0.8, 5.0), "Metal", bevel=0.3, rot=R(-25, 'Y'))
    p["Head"].socket("eyes", (10.0, 0.0, 24.0))
    for sd, s in (("L", 1), ("R", -1)):
        P = p["UpperArm_" + sd]
        add_capsule(P, (0, 0, -32), (0, 0, 0), 4.3, 5.3, "Body", segs=12)
        shoulder_pad(P, s, 5.5, out=0.4, sy=1.0, sz=0.6)
        add_capsule(P, (0, s * 4.5, -30), (0, s * 5.2, -6), 0.55, 0.55, "Accent", segs=5, cap_rings=2)
        P = p["LowerArm_" + sd]
        add_capsule(P, (0, 0, -25.5), (0, 0, 0), 3.3, 4.2, "Body", segs=12)
        add_band(P, (0, 0, -22.0), (0, 0, 1), 3.3, 4.2, 1.6, "Accent", segs=12)
        tag_band(P, -25.4, 3.1, w=2.6, t=1.2)
        hand(P, -28.0, s, 1.1, "Dark")
    for sd, s in (("L", 1), ("R", -1)):
        P = p["Thigh_" + sd]
        add_capsule(P, (0, 0, -46.5), (0, 0, 0), 5.6, 7.4, "Body", segs=12)
        add_capsule(P, (0, s * 4.9, -40), (0, s * 6.8, -3), 0.6, 0.6, "Accent", segs=5, cap_rings=2)
        P = p["Shin_" + sd]
        add_capsule(P, (0, 0, -42), (0, 0, 0), 4.2, 5.6, "Body", segs=12)
        add_sphere(P, (2.4, 0, -2.0), (4.2, 5.0, 5.2), "Dark", segs=10, rings=6)
        tag_band(P, -38.5, 4.3, w=2.6, t=1.3)
        shoe(P, -55.0, s, 1.05, upper="Body", sole="Dark", trim="Accent")
    return rig


# ================================================================================================================
# Tsumugi - S playmaker, weaving-workshop heir (poncho, braid, thread spools, trailing threads)
# ================================================================================================================
def build_tsumugi():
    sk = dict(pelvis=(0, 0, 80), spine=(0, 0, 87.5), neck=(-0.5, 0, 124.5), shoulder=(-0.5, 14.5, 120),
              elbow=(-0.5, 14.5, 97.5), hip=(0, 7.5, 75), knee=(0, 7.5, 41))
    rig = Rig("Tsumugi", sk)
    p = rig.parts
    P = p["Pelvis"]
    add_loft(P, [(-12, 0, 0, 6.4, 7.6, 2.2), (-7, 0, 0, 7.8, 11.6, 2.4), (-2, 0, 0, 8.2, 12.2, 2.5),
                 (3, 0, 0, 8.2, 12.0, 2.5), (7, 0, 0, 7.8, 11.2, 2.4), (10, 0, 0, 7.0, 9.8, 2.2)], "Dark", k=16)
    add_loft(P, [(3.0, 0, 0, 8.45, 12.25, 2.5), (6.5, 0, 0, 8.2, 11.7, 2.45)], "Team", k=16, cap0=False,
             cap1=False)  # obi-like sash
    add_box(P, (-8.4, 0, 4.8), (2.4, 6.0, 5.0), "Team", bevel=0.8, bsegs=1)
    # ---- Torso: shirt + poncho with woven stripes
    P = p["Torso"]
    add_loft(P, [(-4, 0, 0, 7.6, 10.2, 2.2), (4, 0.2, 0, 7.8, 10.6, 2.3), (14, 0.6, 0, 8.4, 11.8, 2.4),
                 (24, 0.4, 0, 8.6, 13.4, 2.5), (30, -0.3, 0, 7.8, 13.8, 2.5), (34, -0.5, 0, 5.8, 9.0, 2.2),
                 (37.5, -0.5, 0, 4.6, 5.0, 2.0)], "Dark", k=16)
    # Poncho: rings whose height varies with the angle -> drapes from the shoulders, hem pointed front & back.
    K = 24

    def pring(z_side, z_tip, ax, by, n=2.3, cx=0.0):
        pts = []
        for (x, y) in se_points(ax, by, n, K):
            w = abs(x) / ax  # 1 at front/back, 0 at the sides
            pts.append((cx + x, y, z_side + (z_tip - z_side) * (w ** 1.6)))
        return pts
    rings = [pring(36.4, 36.4, 5.9, 6.5, 2.0, -0.6), pring(34.0, 34.4, 9.0, 12.2, 2.15, -0.5),
             pring(31.0, 31.6, 10.8, 16.4, 2.25, -0.3), pring(27.0, 26.5, 12.0, 18.4, 2.3, -0.1),
             pring(22.0, 20.5, 12.8, 19.2, 2.35, 0.0), pring(17.0, 14.0, 13.4, 19.6, 2.4, 0.1),
             pring(13.0, 7.0, 13.9, 19.75, 2.4, 0.2), pring(11.0, 2.0, 14.3, 19.8, 2.4, 0.3),
             pring(10.0, -1.0, 14.6, 19.85, 2.4, 0.3)]
    bands = ["Body", "Accent", "Body", "Body", "Accent", "Body", "Body", "Team"]
    for i, sl in enumerate(bands):
        P.add(loft_pts([rings[i + 1], rings[i]], cap0=False, cap1=(i == 0)), sl, True)
    inner = [(x * 0.82, y * 0.8, z + 0.5) for (x, y, z) in rings[-1]]
    P.add(loft_pts([inner, rings[-1]], cap0=True, cap1=False), "Dark", True)  # underside, faces down
    for (x, y, z) in rings[-1][::2]:
        if abs(x) > 6.0:
            add_tube(P, [(x, y, z + 0.6), (x * 1.03, y * 1.0, z - 3.6)], [0.75, 0.45], "Accent", segs=4,
                     pole1=0.5)
    for i in range(10):  # diamond weave motif on the lower band
        a = 2 * math.pi * (i + 0.5) / 10
        c = Vector((13.15 * math.cos(a), 19.45 * math.sin(a), 15.0 - 5.5 * abs(math.cos(a)) ** 1.6))
        n = Vector((math.cos(a) / 13.15, math.sin(a) / 19.45, 0.25)).normalized()
        add_prism(P, [(0, 1.5), (-1.2, 0), (0, -1.5), (1.2, 0)], 0.5, "Accent", frame(c - n * 0.15, n, (0, 0, 1)))
    # ---- Head: big eyes, bangs, long braid down the back
    P = p["Head"]
    add_capsule(P, (0, 0, -3), (0.8, 0, 8), 4.0, 4.0, "Skin", segs=12)
    head_ellipsoid(P, (1.6, 0, 17.6), (9.3, 8.1, 10.6))
    eyes_pair(P, 9.6, 3.0, 20.4, r=(0.9, 1.25, 1.55))
    add_sphere(P, (-1.4, 0, 19.6), (10.1, 8.9, 10.5), "Hair", segs=16, rings=9)
    for (y, z, a) in [(-4.6, 26.0, 20), (-1.2, 26.8, 0), (2.4, 26.6, -10), (5.4, 25.4, -25)]:
        add_tube(P, [(7.5, y, z + 0.6), (9.7, y * 1.05, z - 1.8), (10.4, y * 1.08, z - 4.6)], [1.7, 1.3, 0.8],
                 "Hair", segs=5, pole1=0.8)
    braid = bezier((-8.5, 0, 14.0), (-13.5, 0, 6.0), (-14.0, 0, -14.0), (-15.5, 0, -34.0), 9)
    for i, c in enumerate(braid[1:]):
        rr = 2.6 - 0.07 * i
        add_sphere(P, (c.x, c.y + (0.55 if i % 2 else -0.55), c.z), (rr * 0.9, rr, rr * 1.25), "Hair",
                   segs=8, rings=5)
    end = braid[-1]
    add_band(P, (end.x, 0, end.z - 2.0), (0.05, 0, 1), 1.0, 2.3, 1.8, "Accent", segs=10)
    add_tube(P, [(end.x, 0, end.z - 2.5), (end.x - 0.6, 0, end.z - 7.5)], [1.9, 1.1], "Accent", segs=6,
             pole1=0.8)
    p["Head"].socket("eyes", (9.2, 0.0, 20.5))
    # ---- Arms: sleeves, spools at the wrists with long trailing threads
    for sd, s in (("L", 1), ("R", -1)):
        P = p["UpperArm_" + sd]
        add_capsule(P, (0, 0, -22.5), (0, 0, 0), 3.4, 4.2, "Dark", segs=10)
        P = p["LowerArm_" + sd]
        add_capsule(P, (0, 0, -19.0), (0, 0, 0), 2.7, 3.4, "Dark", segs=10)
        for z in (-12.0, -16.6):
            add_band(P, (0, 0, z), (0, 0, 1), 2.8, 5.0, 0.8, "Metal", segs=12)
        add_band(P, (0, 0, -14.3), (0, 0, 1), 2.8, 4.3, 3.8, "Glow", segs=12)
        tag_band(P, -19.0, 2.6, w=2.2, t=1.0, segs=12)
        hand(P, -20.5, s, 0.82, "Skin")
        for j, (dy, dz, dx) in enumerate([(0.0, 0.0, 0.0), (0.9, -6.0, -3.0), (-1.4, -10.0, 2.0)]):
            pts = bezier((-3.6, s * (2.0 + dy * 0.3), -14.3), (-14.0 + dx, s * (2.6 + dy), -22.0 + dz),
                         (-26.0 + dx, s * (3.4 + dy), -38.0 + dz), (-36.0 + dx, s * (4.2 + dy), -58.0 + dz), 12)
            add_tube(P, pts, [0.45 - 0.012 * i for i in range(13)], "Glow", segs=4, cap0=False, cap1=True)
    for sd, s in (("L", 1), ("R", -1)):
        P = p["Thigh_" + sd]
        add_capsule(P, (0, 0, -34), (0, 0, 0), 5.0, 6.3, "Dark", segs=12)
        add_capsule(P, (0, s * 4.6, -28), (0, s * 5.9, -3), 0.9, 0.9, "Team", segs=6, cap_rings=2)
        P = p["Shin_" + sd]
        add_capsule(P, (0, 0, -30), (0, 0, 0), 3.4, 5.0, "Dark", segs=12)
        add_cyl(P, (0, 0, -31.5), (0, 0, -27.5), 4.0, 4.4, "Dark", segs=12)
        tag_band(P, -26.0, 3.6, w=2.2, t=1.1, segs=12)
        shoe(P, -41.0, s, 0.85, upper="Accent", sole="Dark", trim="Body")
    return rig


# ================================================================================================================
# Minamo - L wall, cyborg courier (mech legs with water jets, mech right arm)
# ================================================================================================================
def build_minamo():
    sk = dict(pelvis=(0, 0, 114), spine=(0, 0, 124), neck=(-1, 0, 182), shoulder=(-1, 25, 175),
              elbow=(-1, 25, 141), hip=(0, 16.5, 108), knee=(0, 16.5, 62))
    rig = Rig("Minamo", sk)
    p = rig.parts
    P = p["Pelvis"]
    add_loft(P, [(-14, 0, 0, 9.6, 11.0, 2.3), (-7, 0, 0, 11.4, 17.5, 2.5), (1, 0, 0, 11.8, 18.6, 2.6),
                 (7, 0, 0, 11.6, 17.0, 2.5), (12, 0, 0, 10.8, 15.0, 2.3)], "Body", k=16)
    add_loft(P, [(2.0, 0, 0, 12.15, 18.65, 2.6), (6.0, 0, 0, 12.0, 17.6, 2.55)], "Dark", k=16, cap0=False,
             cap1=False)  # utility belt
    add_box(P, (12.2, 0, 4.0), (1.2, 5.0, 3.6), "Accent", bevel=0.5)
    for s in (1, -1):
        add_box(P, (2.0, s * 18.4, 3.0), (6.0, 2.4, 5.5), "Dark", bevel=0.8)  # pouches
    # ---- Torso: wetsuit + courier vest + messenger strap
    P = p["Torso"]
    add_loft(P, [(-5, 0, 0, 11.4, 15.4, 2.3), (2, 0.3, 0, 11.4, 16.2, 2.3), (12, 0.8, 0, 11.9, 17.2, 2.4),
                 (24, 1.8, 0, 13.4, 19.2, 2.5), (36, 1.2, 0, 13.2, 21.4, 2.6), (45, 0, 0, 12.0, 23.4, 2.7),
                 (51, -1, 0, 10.4, 22.0, 2.6), (55, -1.2, 0, 8.0, 13.0, 2.2), (59, -1.2, 0, 6.6, 7.6, 2.0)], "Body",
             k=20)
    vest = [(9.0, 0.7, 0, 12.35, 17.55, 2.45), (24.0, 1.8, 0, 13.95, 19.75, 2.55), (36.0, 1.2, 0, 13.75, 21.95, 2.65),
            (44.0, 0.1, 0, 12.6, 23.7, 2.7)]
    add_loft(P, vest, "Accent", k=20, cap0=False, cap1=False)
    add_loft(P, [(15.0, 1.2, 0, 13.2, 18.9, 2.5), (18.0, 1.45, 0, 13.55, 19.25, 2.52)], "Team", k=20,
             cap0=False, cap1=False)
    add_prism(P, [(x, y) for x, y in se_points(7.0, 7.0, 4.0, 12)], 0.8, "Team",
              Matrix.Translation((-13.4, 0, 30.0)) @ R(-90, 'Y'), bevel=0.25)
    add_loft(P, [(44.0, 0.1, 0, 12.6, 23.7, 2.7), (48.5, -0.6, 0, 11.6, 22.9, 2.65), (51.5, -1.0, 0, 10.0, 18.0, 2.5),
                 (54.5, -1.2, 0, 8.2, 11.0, 2.2)], "Accent", k=20, cap0=False, cap1=False)  # jacket shoulders
    add_cyl(P, (-1.2, 0, 53.0), (-1.2, 0, 58.0), 8.4, 8.0, "Accent", segs=14)  # collar
    add_box(P, (-6.0, -17.8, 6.0), (12.0, 5.5, 10.0), "Dark", bevel=1.2)   # messenger bag
    add_box(P, (-6.0, -20.7, 8.5), (12.4, 0.8, 6.0), "Accent", bevel=0.3)  # bag flap
    # ---- Head
    P = p["Head"]
    add_capsule(P, (0, 0, -3), (1.0, 0, 11), 5.9, 5.9, "Skin", segs=12)
    head_ellipsoid(P, (2.2, 0, 23.5), (12.2, 10.3, 13.6), segs=14, rings=9)
    eyes_pair(P, 12.2, 4.0, 27.6, r=(1.15, 1.6, 1.9))
    add_sphere(P, (-1.2, 0, 26.0), (13.2, 11.7, 13.6), "Hair", segs=14, rings=9)
    add_sphere(P, (8.8, 2.0, 33.2), (6.8, 10.5, 4.2), "Hair", segs=12, rings=6, rot=R(-12, 'Y'))
    for s in (1, -1):
        add_sphere(P, (5.0, s * 10.6, 17.5), (5.5, 2.6, 7.5), "Hair", segs=8, rings=5)
    for s in (1, -1):  # diving goggles on the forehead
        add_cyl(P, (9.0, s * 4.6, 34.2), (13.2, s * 4.6, 36.0), 3.0, 3.0, "Metal", segs=10)
        add_cyl(P, (13.0, s * 4.6, 35.9), (13.5, s * 4.6, 36.1), 2.4, 2.4, "Dark", segs=10)
    add_torus(P, (-0.5, 0, 34.0), (0.3, 0, 1), 12.0, 0.9, "Dark", segs=14, csegs=4, scale=(1.08, 1.0, 1.0))
    p["Head"].socket("eyes", (12.0, 0.0, 28.0))
    # ---- Left arm: human (wetsuit)
    P = p["UpperArm_L"]
    s = 1
    add_capsule(P, (0, 0, -34), (0, 0, 0), 5.0, 6.4, "Body", segs=12)
    shoulder_pad(P, s, 6.6)
    P = p["LowerArm_L"]
    add_capsule(P, (0, 0, -28.0), (0, 0, 0), 3.9, 5.0, "Body", segs=12)
    add_band(P, (0, 0, -25.0), (0, 0, 1), 3.8, 4.8, 3.4, "Dark", segs=12)
    tag_band(P, -28.0, 3.6, w=2.8, t=1.3)
    hand(P, -29.5, s, 1.12, "Skin")
    # ---- Right arm: mechanical
    s = -1
    P = p["UpperArm_R"]
    add_sphere(P, (0, 0, 0), 8.6, "Metal", segs=14, rings=8)
    add_sphere(P, (0, s * 2.2, 2.6), (11.0, 9.8, 7.8), "Team", segs=12, rings=7, rot=R(-s * 22, 'X'))
    add_loft(P, [(-30, 0, 0, 6.2, 6.2, 3.0), (-18, 0, 0, 7.4, 7.0, 3.2), (-6, 0, 0, 7.6, 7.2, 3.2)], "Metal", k=16,
             smooth=True)
    add_cyl(P, (5.0, 0, -9), (5.0, 0, -29), 1.6, 1.6, "Dark", segs=8)
    add_sphere(P, (0, 0, -34.0), 6.6, "Dark", segs=12, rings=7)
    P = p["LowerArm_R"]
    add_sphere(P, (0, 0, 0), 6.4, "Dark", segs=12, rings=7)
    add_loft(P, [(-27, 0.5, 0, 6.2, 6.2, 3.0), (-16, 1.0, 0, 8.2, 7.8, 3.3), (-5, 0.5, 0, 7.4, 7.0, 3.1)],
             "Metal", k=16)
    add_box(P, (0, s * 7.0, -15.0), (6.0, 2.0, 12.0), "Dark", bevel=0.6)  # forearm vent
    tag_band(P, -27.6, 5.6, w=2.8, t=1.3)
    hand(P, -29.5, s, 1.22, "Metal", fingers=4, finger_slot="Dark", chunky=1.12)
    # ---- Legs: heavy mechanical, water-jet nozzles
    for sd, s in (("L", 1), ("R", -1)):
        P = p["Thigh_" + sd]
        add_sphere(P, (0, 0, 0), 11.4, "Metal", segs=12, rings=6)
        add_loft(P, [(-41, 1.0, s * 0.5, 10.8, 10.6, 3.0), (-22, 1.5, s * 1.0, 12.6, 12.4, 3.2),
                     (-6, 1.0, s * 1.0, 12.0, 11.8, 3.1)], "Metal", k=12)
        add_cyl(P, (11.6, 0, -10), (11.6, 0, -38), 2.0, 2.0, "Dark", segs=8)
        add_prism(P, [(x, y) for x, y in se_points(4.2, 12.0, 4.0, 12)], 1.2, "Team",
                  Matrix.Translation((1.0, s * 13.1, -23.0)) @ R(-90 * s, 'X'), bevel=0.3)
        add_sphere(P, (0, 0, -46.0), 10.0, "Dark", segs=12, rings=6)
        P = p["Shin_" + sd]
        add_sphere(P, (3.0, 0, 0), (9.0, 10.0, 9.0), "Metal", segs=12, rings=6)
        add_loft(P, [(-46, 0.0, 0, 9.6, 9.6, 3.0), (-36, -1.0, 0, 12.0, 11.8, 3.1), (-20, -2.4, 0, 13.4, 12.8, 3.3),
                     (-6, -1.0, 0, 11.6, 11.4, 3.2)], "Metal", k=12)
        add_prism(P, [(x, y) for x, y in se_points(4.0, 9.0, 3.0, 10)], 1.0, "Dark",
                  Matrix.Translation((11.2, 0, -20.0)) @ R(90, 'Y'), bevel=0.3)
        for dy in (5.0, -5.0):
            a = Vector((-12.8, dy, -24.0))
            b = a + Vector((-3.2, 0, -6.0))
            add_cyl(P, a, b, 3.0, 3.6, "Dark", segs=8)
            add_cyl(P, b, b + (b - a).normalized() * 0.4, 2.7, 2.7, "Glow", segs=8)
        tag_band(P, -44.5, 9.8, w=3.2, t=1.4)
        fl = -62.0
        P.add(loft_x([(-14.0, 0.0, fl + 7.0, 9.5, 7.0, 2.8), (-8.0, 0.0, fl + 8.6, 11.8, 8.6, 3.0),
                      (4.0, 0.0, fl + 8.6, 12.4, 8.6, 3.0), (16.0, 0.0, fl + 6.6, 11.8, 6.6, 3.0),
                      (24.0, 0.0, fl + 5.0, 9.6, 5.0, 2.8)], k=14, pole1=(27.0, 0.0, fl + 4.6)), "Metal", True)
        add_prism(P, [(6.0 + x, y) for x, y in se_points(21.0, 12.2, 3.0, 14)], 2.2, "Dark", T(0, 0, fl),
                  bevel=0.5)
        add_cyl(P, (-14.0, 0, fl + 7.5), (-15.4, 0, fl + 7.5), 3.4, 3.4, "Dark", segs=8)
        add_cyl(P, (-15.3, 0, fl + 7.5), (-15.7, 0, fl + 7.5), 2.6, 2.6, "Glow", segs=8)
        P.socket("sole", (6.0, 0.0, fl))
    return rig


# ================================================================================================================
# Gantetsu - XL wall, self-aware demolition machine (welding visor, exhaust stacks, claw hands)
# ================================================================================================================
def build_gantetsu():
    sk = dict(pelvis=(0, 0, 116), spine=(0, 0, 130), neck=(8, 0, 228), shoulder=(-2, 33, 212),
              elbow=(-2, 33, 156), hip=(0, 21, 106), knee=(0, 21, 60))
    rig = Rig("Gantetsu", sk)
    p = rig.parts
    # ---- Pelvis: heavy hip block + skirt plates
    P = p["Pelvis"]
    add_loft(P, [(-20, 0, 0, 14, 15, 3.0), (-10, 0, 0, 17, 25, 3.2), (2, 0, 0, 18, 26.5, 3.3),
                 (12, 0, 0, 17, 23, 3.0), (18, 0, 0, 14, 18, 2.6)], "Dark", k=16)
    add_box(P, (17.5, 0, -8.0), (4.0, 24.0, 20.0), "Body", bevel=1.5, rot=R(-12, 'Y'))
    add_box(P, (-17.0, 0, -6.0), (4.0, 28.0, 18.0), "Body", bevel=1.5, rot=R(12, 'Y'))
    add_box(P, (19.8, 0, -8.0), (0.8, 12.0, 6.0), "Accent", rot=R(-12, 'Y'))
    # ---- Torso: barrel chassis, engine block, exhaust stacks
    P = p["Torso"]
    add_loft(P, [(-8, 0, 0, 17, 21, 4.0), (0, 1, 0, 19, 23.5, 4.2), (18, 3, 0, 21.5, 27, 4.6), (42, 5, 0, 25, 31, 5.0),
                 (64, 5, 0, 26, 32.5, 5.0), (78, 3, 0, 23.5, 31.5, 4.6), (88, 2, 0, 18, 26, 4.0),
                 (95, 2, 0, 11, 15, 3.0)], "Body", k=24)
    add_loft(P, [(70, -12, 0, 14, 27, 4.5), (92, -13, 0, 13, 26, 4.5), (104, -14, 0, 10, 22, 4.0),
                 (108, -14, 0, 7, 17, 3.2)], "Body", k=16)  # back yoke rising behind the head
    add_box(P, (-22.0, 0, 103.0), (6.0, 30.0, 4.0), "Accent", bevel=1.0)
    add_loft(P, [(-4, 0.5, 0, 18.3, 23.8, 3.1), (5, 1.6, 0, 20.0, 26.3, 3.25)], "Dark", k=20, cap0=False, cap1=False)
    add_prism(P, [(x, y) for x, y in se_points(10.0, 12.0, 4.0, 12)], 1.6, "Team",
              Matrix.Translation((29.2, 0, 56.0)) @ R(90, 'Y') @ R(90, 'Z'), bevel=0.5)
    for i in range(3):  # hazard chevrons on the belly
        add_box(P, (24.6 + i * 0.45, -16.0 + i * 0.0, 26.0 + i * 7.0), (1.6, 6.0, 3.0), "Accent", rot=R(30, 'X'))
        add_box(P, (24.6 + i * 0.45, 16.0, 26.0 + i * 7.0), (1.6, 6.0, 3.0), "Accent", rot=R(-30, 'X'))
    add_box(P, (-24.0, 0, 58.0), (14.0, 34.0, 40.0), "Dark", bevel=3.0, bsegs=2)  # engine pack
    for y in (-11.0, 0.0, 11.0):
        add_box(P, (-31.4, y, 58.0), (1.2, 6.0, 30.0), "Metal", bevel=0.4)
    for s in (1, -1):
        base = Vector((-24.0, s * 12.5, 70.0))
        top = Vector((-27.0, s * 14.0, 165.0))
        add_cyl(P, base, top, 5.6, 5.0, "Metal", segs=12)
        add_cyl(P, top - (top - base).normalized() * 4.0, top + (top - base).normalized() * 3.0, 7.0, 7.4, "Dark",
                segs=12)
        add_cyl(P, top + (top - base).normalized() * 1.5, top + (top - base).normalized() * 2.6, 5.4, 5.4, "Glow",
                segs=12)
        add_band(P, base + (top - base) * 0.45, top - base, 5.2, 6.6, 3.0, "Dark", segs=12)
    add_torus(P, (6, 0, 95.0), (0, 0, 1), 10.0, 2.4, "Dark", segs=16, csegs=6)
    for s in (1, -1):  # hydraulic lines to the shoulders
        add_tube(P, [(-14.0, s * 10.0, 80.0), (-10.0, s * 22.0, 86.0), (-4.0, s * 30.0, 84.0)], 1.6, "Dark", segs=6)
    # ---- Head: welding-visor helmet
    P = p["Head"]
    add_cyl(P, (-2, 0, -6), (0, 0, 8), 9.5, 9.0, "Dark", segs=14)
    add_box(P, (4.0, 0, 18.0), (28.0, 30.0, 27.0), "Body", bevel=5.0, bsegs=2)
    add_box(P, (17.4, 0, 19.0), (5.0, 29.0, 20.0), "Dark", bevel=1.6, bsegs=1, rot=R(-10, 'Y'))
    add_box(P, (20.2, 0, 22.0), (1.6, 22.0, 2.6), "Glow", rot=R(-10, 'Y'))
    add_box(P, (4.0, 0, 32.4), (16.0, 8.0, 2.4), "Accent", bevel=0.8)
    add_cyl(P, (-6.0, 10.0, 31.0), (-8.0, 11.0, 44.0), 0.8, 0.5, "Metal", segs=6)
    for s in (1, -1):
        add_cyl(P, (4.0, s * 15.0, 20.0), (4.0, s * 17.4, 20.0), 4.8, 4.4, "Metal", segs=12)
    p["Head"].socket("eyes", (18.0, 0.0, 22.0))
    # ---- Arms: pauldrons, hydraulic upper arm, massive forearm, demolition claw
    for sd, s in (("L", 1), ("R", -1)):
        P = p["UpperArm_" + sd]
        add_sphere(P, (0, 0, 0), 13.0, "Metal", segs=12, rings=6)
        add_sphere(P, (0, s * 2.0, 5.0), (18.0, 11.5, 12.0), "Body", segs=14, rings=7, rot=R(-s * 12, 'X'))
        add_sphere(P, (0, s * 9.6, 6.5), (11.0, 3.2, 7.0), "Team", segs=10, rings=5, rot=R(-s * 12, 'X'))
        add_cyl(P, (0, 0, -8), (0, 0, -46), 9.2, 8.6, "Dark", segs=12)
        add_cyl(P, (8.6, 0, -12), (8.6, 0, -44), 2.4, 2.4, "Metal", segs=6)
        add_cyl(P, (-8.6, 0, -12), (-8.6, 0, -44), 2.4, 2.4, "Metal", segs=6)
        add_sphere(P, (0, 0, -56.0), 11.0, "Dark", segs=12, rings=6)
        P = p["LowerArm_" + sd]
        add_sphere(P, (0, 0, 0), 10.6, "Metal", segs=12, rings=6)
        tag_band(P, -7.0, 11.6, w=4.4, t=1.8, segs=16)
        add_loft(P, [(-44, 2, 0, 12.0, 11.6, 3.4), (-34, 3, 0, 15.0, 13.6, 3.8), (-18, 3, 0, 14.0, 13.2, 3.6),
                     (-9, 1, 0, 11.4, 11.0, 3.0)], "Body", k=16)
        add_box(P, (3.0, s * 13.0, -28.0), (16.0, 2.4, 12.0), "Accent", bevel=0.6)
        add_sphere(P, (2.0, 0, -46.0), 8.0, "Metal", segs=12, rings=6)
        # claw: palm block + 3 heavy fingers + thumb, palm facing -s*Y
        add_box(P, (2.0, 0, -55.0), (20.0, 15.0, 16.0), "Dark", bevel=2.5, bsegs=1)
        for fx in (-6.0, 1.5, 9.0):
            pts = [(fx, 0, -61.0), (fx, -s * 1.0, -70.0), (fx, -s * 5.0, -78.0), (fx, -s * 9.0, -82.0)]
            add_tube(P, pts, [3.0, 2.9, 2.6, 2.2], "Metal", segs=6, pole1=1.8)
            add_sphere(P, (fx, -s * 1.0, -70.0), 3.4, "Dark", segs=8, rings=4)
        add_tube(P, [(12.5, -s * 4.0, -50.0), (16.0, -s * 7.0, -60.0), (14.0, -s * 10.0, -68.0)], [3.0, 2.8, 2.4],
                 "Metal", segs=6, pole1=1.8)
        P.socket("hand", (2.0, -s * 7.6, -57.0))
    # ---- Legs: short, thick, huge tread feet
    for sd, s in (("L", 1), ("R", -1)):
        P = p["Thigh_" + sd]
        add_sphere(P, (0, 0, 0), 13.5, "Metal", segs=12, rings=6)
        add_cyl(P, (0, 0, -6), (0, 0, -40), 12.4, 11.6, "Dark", segs=12)
        add_box(P, (6.0, s * 4.0, -22.0), (16.0, 22.0, 26.0), "Body", bevel=3.0, bsegs=2)
        add_box(P, (6.0, s * 15.2, -22.0), (10.0, 1.4, 14.0), "Team", bevel=0.4)
        add_sphere(P, (0, 0, -46.0), 11.8, "Dark", segs=12, rings=6)
        P = p["Shin_" + sd]
        add_sphere(P, (0, 0, 0), 11.2, "Metal", segs=12, rings=6)
        tag_band(P, -2.5, 12.4, w=4.4, t=1.8, segs=16)
        add_loft(P, [(-42, 0, 0, 11.0, 11.0, 3.4), (-26, 1.5, 0, 14.0, 13.0, 3.8), (-9, 1.5, 0, 13.2, 12.6, 3.6)],
                 "Body", k=16)
        add_cyl(P, (-12.0, 0, -10), (-12.0, 0, -38), 2.6, 2.6, "Metal", segs=6)
        add_sphere(P, (0, 0, -43.0), 9.0, "Metal", segs=10, rings=5)
        fl = -60.0
        add_box(P, (6.0, 0, fl + 9.0), (44.0, 31.0, 14.0), "Dark", bevel=3.5, bsegs=2)
        add_box(P, (16.0, 0, fl + 15.0), (22.0, 27.0, 5.0), "Body", bevel=2.0, bsegs=1, rot=R(10, 'Y'))
        for x in (-10.0, -2.0, 6.0, 14.0, 22.0):
            add_box(P, (x, 0, fl + 1.0), (4.0, 31.6, 2.0), "Metal")
        P.socket("sole", (6.0, 0.0, fl))
    return rig


# ================================================================================================================
# Hibiki - M playmaker, bioluminescent jellyfish colony in human shape
# ================================================================================================================
def build_hibiki():
    sk = dict(pelvis=(0, 0, 99), spine=(0, 0, 108), neck=(-1, 0, 150), shoulder=(-1, 18.5, 145),
              elbow=(-1, 18.5, 116), hip=(0, 9, 93), knee=(0, 9, 50))
    rig = Rig("Hibiki", sk)
    p = rig.parts
    # ---- Pelvis: jelly hips + oral-arm skirt with TaG tips
    P = p["Pelvis"]
    add_loft(P, [(-14, 0, 0, 7.4, 8.6, 2.2), (-8, 0, 0, 9.0, 13.4, 2.3), (-2, 0, 0, 9.4, 14.0, 2.4),
                 (4, 0, 0, 9.2, 13.6, 2.4), (9, 0, 0, 8.6, 12.6, 2.3), (12, 0, 0, 7.8, 11.2, 2.2)], "Jelly", k=16)
    add_loft(P, [(3.5, 0, 0, 9.55, 13.95, 2.4), (6.5, 0, 0, 9.3, 13.4, 2.35)], "Team", k=16, cap0=False,
             cap1=False)
    n = 11
    for i in range(n):
        a = 2 * math.pi * (i + 0.5) / n
        c, sn = math.cos(a), math.sin(a)
        base = Vector((10.0 * c, 14.2 * sn, 3.0))
        out = Vector((c, sn * 0.9, 0.0))
        ln = 38.0 + 6.0 * math.sin(i * 1.7)
        pts = []
        for j in range(7):
            t = j / 6.0
            wig = math.sin(t * 5.0 + i) * 1.6
            pts.append(base + out * (3.0 * t + 2.5 * t * t) + Vector((wig * -sn, wig * c, -ln * t)))
        add_tube(P, pts, [1.5 - 0.15 * j for j in range(7)], "Jelly", segs=5, cap0=True, cap1=True)
        add_sphere(P, pts[-1] + Vector((0, 0, -0.6)), 1.1, "TaG", segs=6, rings=3)
    # ---- Torso: translucent body, glowing core, team harness
    P = p["Torso"]
    add_loft(P, [(-5, 0, 0, 8.5, 12.0, 2.2), (4, 0.2, 0, 8.8, 12.4, 2.2), (14, 0.6, 0, 9.4, 13.6, 2.3),
                 (24, 0.8, 0, 10.2, 15.2, 2.4), (33, 0.2, 0, 9.8, 16.8, 2.5), (38, -0.8, 0, 8.2, 16.2, 2.4),
                 (41, -1.0, 0, 6.0, 9.5, 2.1), (43.5, -1.0, 0, 5.0, 5.6, 2.0)], "Jelly", k=18)
    add_sphere(P, (1.5, 0, 25.0), (3.6, 4.8, 5.0), "Glow", segs=10, rings=6)
    for i in range(6):
        add_sphere(P, (-3.5, 0, -2.0 + i * 6.8), (1.6, 2.2, 1.7), "Glow", segs=8, rings=5)
    for s in (1, -1):
        add_tube(P, [(4.6, s * 13.6, 39.4), (9.6, s * 8.6, 32.0), (11.0, s * 2.0, 24.0)], 1.0, "Team", segs=6)
        add_tube(P, [(-4.0, s * 13.6, 39.4), (-9.0, s * 8.6, 32.0), (-10.0, s * 2.0, 24.0)], 1.0, "Team", segs=6)
    add_cyl(P, (10.6, 0, 24.0), (11.9, 0, 24.0), 3.4, 3.4, "Team", segs=14)
    add_cyl(P, (-9.6, 0, 24.0), (-11.0, 0, 24.0), 3.4, 3.4, "Team", segs=14)
    # ---- Head: jelly bell with TaG rim, clover gonads, glowing eyes, tentacle hair
    P = p["Head"]
    add_capsule(P, (0, 0, -3), (1.0, 0, 12), 4.6, 4.6, "Jelly", segs=12)
    add_sphere(P, (2.4, 0, 21.0), (8.4, 7.2, 9.4), "Jelly", segs=14, rings=8)
    eyes_pair(P, 10.0, 3.2, 25.0, r=(0.9, 1.2, 1.6), slot="Glow")
    bc = Vector((1.0, 0, 0))
    prof = [(0.0, 44.5), (5.6, 43.6), (10.6, 40.0), (14.2, 34.8), (16.2, 28.6), (16.8, 22.6), (16.2, 19.4),
            (14.6, 19.0), (13.6, 21.6), (11.4, 29.0), (6.4, 35.4), (0.0, 37.2)]
    P.add(lathe(prof, 16), "Jelly", True, T(bc.x, 0, 0))
    add_band(P, (bc.x, 0, 19.6), (0, 0, 1), 14.6, 17.4, 2.2, "TaG", segs=16)
    for i in range(4):
        a = math.pi / 4 + i * math.pi / 2
        c = Vector((bc.x + 5.6 * math.cos(a), 5.6 * math.sin(a), 36.0))
        add_torus(P, c, (math.cos(a) * 0.3, math.sin(a) * 0.3, 1.0), 2.6, 0.7, "Glow", segs=10, csegs=5)
    hair = [(100, 18), (125, 30), (150, 38), (172, 42), (188, 42), (210, 38), (235, 30), (260, 18), (80, 8), (280, 8)]
    for k, (deg, ln) in enumerate(hair):
        a = math.radians(deg)
        root = Vector((bc.x + 15.4 * math.cos(a), 15.4 * math.sin(a), 20.0))
        out = Vector((math.cos(a), math.sin(a), 0.0))
        pts = []
        for j in range(6):
            t = j / 5.0
            wig = math.sin(t * 4.0 + k) * 1.2
            back = Vector((-1.0, 0, 0)) * (6.0 * t * t)
            pts.append(root + out * (2.5 * t) + back + Vector((0, wig, -ln * t)))
        add_tube(P, pts, [1.75 - 0.18 * j for j in range(6)], "Jelly", segs=5, cap0=True, cap1=True)
        add_sphere(P, pts[-1] + Vector((0, 0, -0.6)), 1.05, "TaG", segs=6, rings=3)
    p["Head"].socket("eyes", (11.0, 0.0, 25.0))
    # ---- Arms: jelly with glowing nerve lines, team armbands, tendril hands
    for sd, s in (("L", 1), ("R", -1)):
        P = p["UpperArm_" + sd]
        add_capsule(P, (0, 0, -29), (0, 0, 0), 4.0, 5.0, "Jelly", segs=12)
        add_capsule(P, (0, 0, -27), (0, 0, -2), 0.9, 0.9, "Glow", segs=6, cap_rings=2)
        add_band(P, (0, 0, -9.0), (0, 0, 1), 4.6, 5.6, 3.0, "Team", segs=12)
        shoulder_pad(P, s, 5.0, slot="Jelly", sz=0.6)
        P = p["LowerArm_" + sd]
        add_capsule(P, (0, 0, -24.0), (0, 0, 0), 3.2, 4.0, "Jelly", segs=12)
        add_capsule(P, (0, 0, -23.0), (0, 0, -2), 0.8, 0.8, "Glow", segs=6, cap_rings=2)
        hand(P, -25.0, s, 0.98, "Jelly", fingers=3, curl=1.3)
    for sd, s in (("L", 1), ("R", -1)):
        P = p["Thigh_" + sd]
        add_capsule(P, (0, 0, -43), (0, 0, 0), 5.2, 7.0, "Jelly", segs=12)
        add_capsule(P, (0, 0, -40), (0, 0, -3), 1.0, 1.0, "Glow", segs=6, cap_rings=2)
        P = p["Shin_" + sd]
        add_capsule(P, (0, 0, -40), (0, 0, 0), 3.6, 5.2, "Jelly", segs=12)
        add_capsule(P, (0, 0, -37), (0, 0, -3), 0.9, 0.9, "Glow", segs=6, cap_rings=2)
        shoe(P, -50.0, s, 0.98, upper="Jelly", sole="Body")
    return rig


BUILDERS = {"Gantetsu": build_gantetsu, "Minamo": build_minamo, "Rei": build_rei, "Hayate": build_hayate,
            "Tsumugi": build_tsumugi, "Hibiki": build_hibiki}


def build_all():
    out = {}
    for hid in HERO_IDS:
        rig = BUILDERS[hid]()
        for part in rig.parts.values():
            part.finalize()
        out[hid] = rig
    return out
