"""
court.py - "Nagisa - rooftop court in Tetsuba (industrial district)", authored directly in UE court coordinates:
origin = middle of the baseline on the floor, court X 0..1100, Y -750..750, Z up, cm.  (Parts are flagged
ue_space and mirrored to Blender space at finalize, so the FBX round trip lands exactly on these numbers.)

  SM_Hoop                 - backboard / rim / net / bracket / padded pole, numbers from TaGTypes.h
  SM_Rooftop              - the slab: top surface exactly z = 0 (Court / Paint / Apron), 100 cm thick
  SM_RooftopProps         - parapet + railing, 4 light towers, stair hut, water tank, pipes, gantry, vents,
                            host building below the slab
  SM_RooftopProps_Furnace - furnace chimney stacks with glowing mouths + blast furnace body (outside the slab)
"""
import math
import random
import re
import bmesh
from mathutils import Vector, Matrix
from tagmesh import (Part, frame, T, R, S, box, cylinder, torus, band, lathe, tube, prism, se_points, add_box,
                     add_cyl, add_sphere, add_band, add_torus, add_tube, add_prism, quad, sphere)

# ---------------------------------------------------------------- numbers mirrored from TaGTypes.h (cm)
TAG = dict(CourtDepth=1100.0, CourtHalfWidth=750.0, RimX=157.5, RimZ=305.0, RimRadius=22.5, RimTube=1.0,
           BoardFrontX=120.0, BoardThickness=3.0, BoardHalfWidth=90.0, BoardBottomZ=290.0, BoardTopZ=395.0,
           LaneHalfWidth=245.0, FreeThrowX=580.0, BallRadius=12.0)


def read_tagtypes(path):
    """Parse 'constexpr float Name = 123.f;' lines so the builder fails loudly if the header changes."""
    vals = {}
    try:
        txt = open(path, encoding="utf-8").read()
    except OSError:
        return vals
    for m in re.finditer(r"constexpr\s+float\s+(\w+)\s*=\s*([-0-9.]+)f?", txt):
        vals[m.group(1)] = float(m.group(2))
    return vals


def sync_tagtypes(path):
    v = read_tagtypes(path)
    for k in list(TAG.keys()):
        if k in v:
            TAG[k] = v[k]
    return v


# ---------------------------------------------------------------- helpers (UE space)
def open_box(P, lo, hi, slot, skip=(), smooth=False):
    """Axis-aligned box from lo to hi without the faces listed in skip ('+x','-x','+y','-y','+z','-z')."""
    bm = bmesh.new()
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    v = {}
    for ix, x in enumerate((x0, x1)):
        for iy, y in enumerate((y0, y1)):
            for iz, z in enumerate((z0, z1)):
                v[(ix, iy, iz)] = bm.verts.new((x, y, z))
    faces = {
        '-x': [(0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0)],
        '+x': [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)],
        '-y': [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)],
        '+y': [(0, 1, 0), (0, 1, 1), (1, 1, 1), (1, 1, 0)],
        '-z': [(0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0)],
        '+z': [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)],
    }
    for k, idx in faces.items():
        if k not in skip:
            bm.faces.new([v[i] for i in idx])
    P.add(bm, slot, smooth)


def face_grid(P, origin, u_axis, v_axis, us, vs, slot_fn):
    """Planar quads origin + u*u_axis + v*v_axis over the cut lists us/vs; normal = u_axis x v_axis."""
    o, ua, va = Vector(origin), Vector(u_axis), Vector(v_axis)
    for i in range(len(us) - 1):
        for j in range(len(vs) - 1):
            u0, u1, v0, v1 = us[i], us[i + 1], vs[j], vs[j + 1]
            sl = slot_fn((u0 + u1) / 2, (v0 + v1) / 2)
            if sl is None:
                continue
            P.add(quad(o + ua * u0 + va * v0, o + ua * u1 + va * v0, o + ua * u1 + va * v1, o + ua * u0 + va * v1),
                  sl, False)


def beam(P, a, b, w, h, slot, up=(0, 0, 1)):
    """Rectangular beam from a to b (w across, h along `up`)."""
    a, b = Vector(a), Vector(b)
    d = b - a
    m = frame((a + b) / 2, d, up)  # local z along the beam, local x toward `up`
    P.add(box(h, w, d.length), slot, False, m)


def ibeam(P, a, b, w, h, slot, up=(0, 0, 1)):
    """Simple I-beam: two flanges + web."""
    a, b = Vector(a), Vector(b)
    d = b - a
    m = frame((a + b) / 2, d, up)
    tf = max(1.0, h * 0.12)
    P.add(box(tf, w, d.length), slot, False, m @ T(h / 2 - tf / 2, 0, 0))
    P.add(box(tf, w, d.length), slot, False, m @ T(-h / 2 + tf / 2, 0, 0))
    P.add(box(h - 2 * tf, max(0.8, w * 0.15), d.length), slot, False, m)


# ================================================================================================================
# SM_Hoop
# ================================================================================================================
def build_hoop():
    P = Part("SM_Hoop")
    P.ue_space = True
    t = TAG
    bx1 = t["BoardFrontX"]
    bx0 = bx1 - t["BoardThickness"]
    hw = t["BoardHalfWidth"]
    z0, z1 = t["BoardBottomZ"], t["BoardTopZ"]
    # --- backboard: front face partitioned into border / shooter's square / board (coplanar, exact)
    open_box(P, (bx0, -hw, z0), (bx1, hw, z1), "Board", skip=('+x',))
    sq_y, sq_z0, sq_z1, lw, bw = 29.5, t["RimZ"] - 5.0, t["RimZ"] + 40.0, 5.0, 5.0
    us = sorted({-hw, -hw + bw, -sq_y, -sq_y + lw, sq_y - lw, sq_y, hw - bw, hw})
    vs = sorted({z0, z0 + bw, sq_z0, sq_z0 + lw, sq_z1 - lw, sq_z1, z1 - bw, z1})

    def slot_fn(y, z):
        if abs(y) > hw - bw or z < z0 + bw or z > z1 - bw:
            return "Dark"
        in_outer = abs(y) < sq_y and sq_z0 < z < sq_z1
        in_inner = abs(y) < sq_y - lw and sq_z0 + lw < z < sq_z1 - lw
        if in_outer and not in_inner:
            return "Glow"
        return "Board"
    face_grid(P, (bx1, 0, 0), (0, 1, 0), (0, 0, 1), us, vs, slot_fn)
    # --- steel frame behind the board
    fx0, fx1 = bx0 - 6.0, bx0
    for (ylo, yhi, zlo, zhi) in [(-hw + 2, hw - 2, z0 + 2, z0 + 8), (-hw + 2, hw - 2, z1 - 8, z1 - 2),
                                 (-hw + 2, -hw + 8, z0 + 2, z1 - 2), (hw - 8, hw - 2, z0 + 2, z1 - 2),
                                 (-4, 4, z0 + 2, z1 - 2), (-hw + 2, hw - 2, 338, 344)]:
        open_box(P, (fx0, ylo, zlo), (fx1, yhi, zhi), "Metal")
    # --- rim (goal furnace): torus whose tube centre line has radius RimRadius (as in TaGBallSim)
    rx, rz, rr, rt = t["RimX"], t["RimZ"], t["RimRadius"], t["RimTube"]
    P.add(torus(rr, rt, 40, 8), "TaG", True, T(rx, 0, rz))
    # --- bracket: plate on the board face + two arms under the back of the rim
    open_box(P, (bx1, -11, rz - 16), (bx1 + 1.5, 11, rz + 2), "Metal", skip=('-x',))
    for s in (1, -1):
        beam(P, (bx1 + 1.0, s * 7.0, rz - 12.0), (rx - rr - 0.5, s * 4.0, rz - 1.4), 2.4, 2.4, "Metal",
             up=(0, 0, 1))
        beam(P, (bx1 + 1.0, s * 7.0, rz - 1.6), (rx - rr - 0.5, s * 4.0, rz - 1.4), 2.4, 1.6, "Metal")
    # --- net: diamond lattice hanging from the rim, 40 cm
    n_around, rows = 12, 4
    rad = [rr - 0.6, 19.5, 16.8, 15.0, 14.2]
    zs = [rz - 0.8, rz - 11.0, rz - 21.0, rz - 31.0, rz - 40.0]
    pts = []
    for i in range(rows + 1):
        off = 0.5 * (i % 2)
        pts.append([Vector((rx + rad[i] * math.cos(2 * math.pi * (j + off) / n_around),
                            rad[i] * math.sin(2 * math.pi * (j + off) / n_around), zs[i])) for j in range(n_around)])
    for i in range(rows):
        for j in range(n_around):
            a = pts[i][j]
            if i % 2 == 0:
                b1, b2 = pts[i + 1][j], pts[i + 1][j - 1]
            else:
                b1, b2 = pts[i + 1][j], pts[i + 1][(j + 1) % n_around]
            for b in (b1, b2):
                P.add(tube([a, b], 0.35, 4, False, False), "Net", True)
    for j in range(n_around):
        P.add(tube([pts[rows][j], pts[rows][(j + 1) % n_around]], 0.35, 4, False, False), "Net", True)
    # --- padded pole behind the baseline + arm to the board
    px = -100.0
    open_box(P, (px - 11, -11, 0), (px + 11, 11, 352), "Metal")
    open_box(P, (px - 19, -21, 0), (px + 19, 21, 190), "Accent")          # padding
    open_box(P, (px - 19.5, -21.5, 186), (px + 19.5, 21.5, 192), "Dark")   # pad cap seam
    open_box(P, (px - 34, -34, 0), (px + 34, 34, 2.5), "Dark", skip=('-z',))  # base plate
    open_box(P, (px - 11, -9, 334), (fx0, 9, 348), "Metal")                  # main arm
    beam(P, (px + 8, 0, 230), (40.0, 0, 336), 8, 8, "Metal")                  # diagonal strut
    beam(P, (fx0 - 1, -60, 341), (fx0 - 1, 60, 341), 6, 6, "Metal", up=(1, 0, 0))  # cross bar behind board
    for s in (1, -1):
        beam(P, (40.0, 0, 340), (fx0 - 1, s * 58, 341), 4, 4, "Metal")
    # sockets for FX
    P.socket("rim", (rx, 0, rz))
    P.socket("net_bottom", (rx, 0, rz - 40.0))
    # simple collision for the pole/pad only (C++ handles ball physics and walls)
    P.collision.append(((px, 0, 95.0), (40.0, 44.0, 190.0)))
    P.collision.append(((px, 0, 176.0), (22.0, 22.0, 352.0)))
    return P


# ================================================================================================================
# SM_Rooftop
# ================================================================================================================
SLAB = dict(x0=-600.0, x1=1700.0, y0=-1300.0, y1=1300.0, thick=100.0)


def build_rooftop():
    P = Part("SM_Rooftop")
    P.ue_space = True
    t = TAG
    cd, chw, lhw, ftx = t["CourtDepth"], t["CourtHalfWidth"], t["LaneHalfWidth"], t["FreeThrowX"]
    xs = [SLAB["x0"], -300.0, 0.0, ftx, cd, 1400.0, SLAB["x1"]]
    ys = [SLAB["y0"], -1000.0, -chw, -lhw, lhw, chw, 1000.0, SLAB["y1"]]

    def slot_fn(x, y):
        if 0 <= x <= ftx and -lhw <= y <= lhw:
            return "Paint"
        if 0 <= x <= cd and -chw <= y <= chw:
            return "Court"
        return "Apron"
    face_grid(P, (0, 0, 0), (1, 0, 0), (0, 1, 0), xs, ys, slot_fn)
    open_box(P, (SLAB["x0"], SLAB["y0"], -SLAB["thick"]), (SLAB["x1"], SLAB["y1"], 0.0), "Dark", skip=('+z',))
    cx, cy = (SLAB["x0"] + SLAB["x1"]) / 2, (SLAB["y0"] + SLAB["y1"]) / 2
    P.collision.append(((cx, cy, -SLAB["thick"] / 2),
                        (SLAB["x1"] - SLAB["x0"], SLAB["y1"] - SLAB["y0"], SLAB["thick"])))
    return P


# ================================================================================================================
# SM_RooftopProps
# ================================================================================================================
KEEP_OUT = dict(x0=-150.0, x1=1250.0, y0=-900.0, y1=900.0)


def light_tower(P, base, aim):
    """Lattice-ish light mast, lamp head (Glow lenses) aimed at `aim` (court centre)."""
    bx, by = base
    H = 1350.0
    for (dx, dy) in ((-22, -22), (22, -22), (22, 22), (-22, 22)):
        beam(P, (bx + dx * 1.8, by + dy * 1.8, 0), (bx + dx * 0.7, by + dy * 0.7, H), 7, 7, "Metal")
    for z in range(150, int(H), 220):
        f = 1.8 - 1.1 * z / H
        c = [(bx + dx * f, by + dy * f, z) for dx, dy in ((-22, -22), (22, -22), (22, 22), (-22, 22))]
        for i in range(4):
            beam(P, c[i], c[(i + 1) % 4], 4, 4, "Metal")
        beam(P, c[0], (bx + 22 * (f - 0.25), by + 22 * (f - 0.25), z + 200), 3, 3, "Dark")
    open_box(P, (bx - 50, by - 50, 0), (bx + 50, by + 50, 25), "Dark", skip=('-z',))
    d = Vector((aim[0] - bx, aim[1] - by, 0)).normalized()
    side = Vector((-d.y, d.x, 0))
    head = Vector((bx, by, H + 60))
    beam(P, head - side * 150, head + side * 150, 14, 14, "Metal", up=(0, 0, 1))
    beam(P, (bx, by, H - 10), (bx, by, H + 60), 20, 20, "Metal")
    for row in (0, 1):
        for k in range(-2, 3):
            c = head + side * (k * 62) + Vector((0, 0, 45 + row * 62)) + d * 10
            m = frame(c, d + Vector((0, 0, -0.45)), (0, 0, 1))
            P.add(box(48, 54, 26), "Dark", False, m)
            P.add(box(40, 46, 2.0), "Glow", False, m @ T(0, 0, 13.5))
    open_box(P, (bx - 60, by - 60, H - 20), (bx + 60, by + 60, H - 12), "Metal")  # service platform


def build_props():
    P = Part("SM_RooftopProps")
    P.ue_space = True
    x0, x1, y0, y1 = SLAB["x0"], SLAB["x1"], SLAB["y0"], SLAB["y1"]
    th, ph = 30.0, 45.0
    # --- parapet (concrete curb at the slab edge)
    open_box(P, (x0, y0, 0), (x1, y0 + th, ph), "Body", skip=('-z',))
    open_box(P, (x0, y1 - th, 0), (x1, y1, ph), "Body", skip=('-z',))
    open_box(P, (x0, y0 + th, 0), (x0 + th, y1 - th, ph), "Body", skip=('-z',))
    open_box(P, (x1 - th, y0 + th, 0), (x1, y1 - th, ph), "Body", skip=('-z',))
    # --- railing on the parapet
    rail_h = 115.0
    def rail_line(a, b):
        a, b = Vector(a), Vector(b)
        L = (b - a).length
        n = max(1, int(L / 200.0))
        for i in range(n + 1):
            p = a + (b - a) * (i / n)
            open_box(P, (p.x - 3, p.y - 3, ph), (p.x + 3, p.y + 3, ph + rail_h - 6), "Metal", skip=('-z',))
        beam(P, a + Vector((0, 0, ph + rail_h - 3)), b + Vector((0, 0, ph + rail_h - 3)), 7, 6, "Accent")
        beam(P, a + Vector((0, 0, ph + rail_h * 0.5)), b + Vector((0, 0, ph + rail_h * 0.5)), 4, 4, "Metal")
    m = th / 2
    rail_line((x0 + m, y0 + m, 0), (x1 - m, y0 + m, 0))
    rail_line((x1 - m, y0 + m, 0), (x1 - m, y1 - m, 0))
    rail_line((x1 - m, y1 - m, 0), (x0 + m, y1 - m, 0))
    rail_line((x0 + m, y1 - m, 0), (x0 + m, y0 + m, 0))
    # --- light towers (4), lamps aimed at the court centre
    for (bx, by) in ((-430.0, -1120.0), (-430.0, 1120.0), (1530.0, -1120.0), (1530.0, 1120.0)):
        light_tower(P, (bx, by), (550.0, 0.0))
    # --- stair hut (+Y side, behind half court)
    hx0, hx1, hy0, hy1, hh = 1330.0, 1640.0, 960.0, 1240.0, 290.0
    open_box(P, (hx0, hy0, 0), (hx1, hy1, hh), "Body", skip=('-z',))
    open_box(P, (hx0 - 15, hy0 - 15, hh), (hx1 + 15, hy1 + 15, hh + 18), "Dark")
    open_box(P, (hx0 + 90, hy0 - 2, 0), (hx0 + 190, hy0, 220), "Dark", skip=('-z',))        # door (faces -Y)
    open_box(P, (hx0 + 120, hy0 - 30, 228), (hx0 + 160, hy0 - 4, 244), "Dark")              # lamp housing
    open_box(P, (hx0 + 124, hy0 - 28, 224), (hx0 + 156, hy0 - 6, 228), "Glow")
    open_box(P, (hx1, hy0 + 60, 120), (hx1 + 2, hy0 + 200, 220), "Window", skip=('-x',))    # small window
    for k in range(3):  # vents on the hut roof
        add_cyl(P, (hx0 + 60 + k * 90, hy0 + 140, hh + 18), (hx0 + 60 + k * 90, hy0 + 140, hh + 60), 18, 18,
                "Metal", segs=10, smooth=False)
    # --- water tank on stilts (-Y side behind the hoop)
    tx, ty = -380.0, -1080.0
    for (dx, dy) in ((-110, -110), (110, -110), (110, 110), (-110, 110)):
        beam(P, (tx + dx, ty + dy, 0), (tx + dx * 0.8, ty + dy * 0.8, 320), 14, 14, "Metal")
    beam(P, (tx - 110, ty - 110, 40), (tx + 110 * 0.8, ty + 110 * 0.8, 300), 5, 5, "Dark")
    beam(P, (tx + 110, ty - 110, 40), (tx - 110 * 0.8, ty + 110 * 0.8, 300), 5, 5, "Dark")
    P.add(cylinder(260, 150, 150, 20, 4.0), "Body", True, T(tx, ty, 320))
    P.add(lathe([(150.5, 400), (120, 440), (40, 462), (0, 466)], 20), "Dark", True, T(tx, ty, 180))
    for z in (360, 430, 500):
        add_band(P, (tx, ty, z), (0, 0, 1), 149.0, 153.0, 6.0, "Dark", segs=20)
    beam(P, (tx + 150, ty, 330), (tx + 150, ty, 600), 4, 4, "Metal")  # ladder rail
    # --- pipe rack along the back edge (behind the hoop, x ~ -540) + valves
    for (zp, rp, slot) in ((70.0, 16.0, "Metal"), (110.0, 11.0, "Dark"), (140.0, 8.0, "Accent")):
        add_cyl(P, (-540, -1240, zp), (-540, 1240, zp), rp, rp, slot, segs=10, smooth=True)
    for yy in range(-1200, 1201, 300):
        open_box(P, (-560, yy - 6, 0), (-520, yy + 6, 150), "Dark", skip=('-z',))
    for yy in (-700.0, 300.0):
        add_torus(P, (-512.0, yy, 70.0), (1, 0, 0), 22.0, 3.0, "Accent", segs=14, csegs=5)
        add_cyl(P, (-540, yy, 70), (-512, yy, 70), 4, 4, "Metal", segs=6)
    # pipes rising from below the slab edge up into the furnace complex
    for (yy, rp) in ((-1180.0, 28.0), (-1080.0, 18.0)):
        add_cyl(P, (-640, yy, -900), (-640, yy, 420), rp, rp, "Metal", segs=12)
        add_cyl(P, (-640, yy, 420), (-1150, yy - 520, 640), rp, rp, "Metal", segs=12)
    for (xx, rp) in ((1500.0, 24.0),):
        add_cyl(P, (xx, 1340, -900), (xx, 1340, 380), rp, rp, "Metal", segs=12)
        add_cyl(P, (xx, 1340, 380), (xx + 400, 1900, 560), rp, rp, "Metal", segs=12)
    # --- steel gantry behind the hoop (outside the play box)
    gx = 1660.0
    for s in (1, -1):
        ibeam(P, (gx, s * 1240, 0), (gx, s * 1240, 1150), 40, 40, "Metal", up=(1, 0, 0))
    ibeam(P, (gx, -1260, 1150), (gx, 1260, 1150), 45, 45, "Metal", up=(0, 0, 1))
    open_box(P, (gx - 30, -60, 1080), (gx + 30, 60, 1128), "Accent")              # trolley
    add_cyl(P, (gx, 0, 1080), (gx, 0, 860), 2.5, 2.5, "Dark", segs=6)             # cable
    add_torus(P, (gx, 0, 845), (1, 0, 0), 14.0, 3.5, "Dark", segs=12, csegs=5)    # hook
    for s in (1, -1):
        open_box(P, (gx - 8, s * 1240 - 8, 1175), (gx + 8, s * 1240 + 8, 1190), "Glow")  # warning lights
    # --- rooftop clutter: AC units / vents on the side bands
    rng = random.Random(7)
    for (cx, cy) in ((200, -1120), (520, -1150), (820, -1110), (260, 1130), (640, 1150), (980, 1100),
                     (1450, -400), (1480, 250), (-400, 400), (-420, -350)):
        w, d, h = rng.uniform(90, 160), rng.uniform(80, 120), rng.uniform(70, 120)
        open_box(P, (cx - w / 2, cy - d / 2, 0), (cx + w / 2, cy + d / 2, h), "Body", skip=('-z',))
        add_cyl(P, (cx, cy, h), (cx, cy, h + 6), min(w, d) * 0.32, min(w, d) * 0.32, "Dark", segs=12,
                smooth=False)
    # --- host building under the slab (walls down to the sea), with window bands
    hx0b, hx1b, hy0b, hy1b = x0 + 40, x1 - 40, y0 + 40, y1 - 40
    zb0, zb1 = -4000.0, -100.0
    open_box(P, (hx0b, hy0b, zb0), (hx1b, hy1b, zb1), "Body", skip=('+z', '-z'))
    rng = random.Random(11)
    z = zb1 - 260.0
    while z > zb0 + 200:
        for (o, ua, L) in (((hx0b - 6, hy0b, 0), (0, 1, 0), hy1b - hy0b), ((hx1b + 6, hy1b, 0), (0, -1, 0), hy1b - hy0b),
                           ((hx1b, hy0b - 6, 0), (-1, 0, 0), hx1b - hx0b), ((hx0b, hy1b + 6, 0), (1, 0, 0), hx1b - hx0b)):
            o = Vector(o)
            ua = Vector(ua)
            nrm = ua.cross(Vector((0, 0, 1)))
            a = o + ua * 60 + Vector((0, 0, z))
            b = o + ua * (L - 60) + Vector((0, 0, z))
            P.add(quad(a, b, b + Vector((0, 0, 150)), a + Vector((0, 0, 150))), "Window", False,
                  color=(rng.uniform(0.2, 1.0),) * 3 + (1.0,))
        z -= 400.0
    fantasy_dressing(P)
    return P


# ================================================================================================================
# Fantasy dressing (2026-11-08): a shrine gate framing the hoop, paper-lantern strings between the light towers,
# nobori banners along the sidelines. Everything stays outside the play box (KEEP_OUT).
# ================================================================================================================
def lantern(P, c, r=20.0, h=30.0):
    x, y, z = c
    P.add(sphere(r, 10, 6), "White", True, T(x, y, z) @ S(1, 1, h / r))
    P.add(cylinder(5, r * 0.55, r * 0.55, 8), "Dark", True, T(x, y, z + h - 2))
    P.add(cylinder(5, r * 0.5, r * 0.5, 8), "Dark", True, T(x, y, z - h - 3))


def lantern_string(P, a, b, sag, spacing=150.0):
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    n = max(2, int(L / spacing))
    pts = []
    for i in range(n + 1):
        t = i / n
        p = a.lerp(b, t)
        p.z -= sag * 4 * t * (1 - t)
        pts.append(p)
    for p0, p1 in zip(pts[:-1], pts[1:]):
        beam(P, p0, p1, 1.6, 1.6, "Dark")
    for p in pts[1:-1]:
        add_cyl(P, p, p - Vector((0, 0, 18)), 0.8, 0.8, "Dark", segs=4)
        lantern(P, (p.x, p.y, p.z - 50))


def torii(P, x, half_w, height):
    """Shrine gate behind the hoop (vermilion posts and beams, dark top beam with upturned ends)."""
    for s in (1, -1):
        add_cyl(P, (x, s * half_w, 0), (x, s * half_w, height), 32, 28, "Extra", segs=14)
        add_cyl(P, (x, s * half_w, 0), (x, s * half_w, 45), 40, 40, "Dark", segs=14)
    beam(P, (x, -half_w - 90, height * 0.83), (x, half_w + 90, height * 0.83), 26, 30, "Extra")
    beam(P, (x, -half_w - 180, height + 12), (x, half_w + 180, height + 12), 40, 34, "Extra")
    beam(P, (x, -half_w - 200, height + 44), (x, half_w + 200, height + 44), 48, 22, "Dark")
    for s in (1, -1):
        beam(P, (x, s * (half_w + 200), height + 44), (x, s * (half_w + 290), height + 92), 46, 20, "Dark")
    open_box(P, (x - 14, -55, height * 0.83 + 18), (x + 14, 55, height - 6), "Dark")        # plaque (gakuzuka)
    open_box(P, (x + 14, -38, height * 0.83 + 30), (x + 16, 38, height - 18), "Accent")    # gold face


def nobori(P, x, y, h=430.0, slot="Extra"):
    add_cyl(P, (x, y, 0), (x, y, h), 4, 4, "Metal", segs=6)
    add_cyl(P, (x, y - 2, h - 12), (x, y + 62, h - 12), 2.5, 2.5, "Metal", segs=6)
    open_box(P, (x - 1.5, y + 4, h - 300), (x + 1.5, y + 62, h - 14), slot)


def fantasy_dressing(P):
    torii(P, -260.0, 440.0, 700.0)
    # lantern strings between the light towers (sides and behind the hoop), sagging between the masts
    for (a, b) in (((-430, -1120, 960), (1530, -1120, 960)), ((-430, 1120, 960), (1530, 1120, 960)),
                   ((-430, -1120, 960), (-430, 1120, 960))):
        lantern_string(P, a, b, 260.0)
    for i, x in enumerate((60.0, 360.0, 660.0, 960.0)):
        nobori(P, x, -1000.0, slot="Extra" if i % 2 == 0 else "Accent")
        nobori(P, x, 940.0, slot="Accent" if i % 2 == 0 else "Extra")


# ================================================================================================================
# SM_RooftopProps_Furnace
# ================================================================================================================
def chimney(P, x, y, zb, zt, rb, rt, rng):
    P.add(cylinder(zt - zb, rb, rt, 16), "Body", True, T(x, y, zb))
    for z in range(int(zb) + 1600, int(zt) - 300, 900):
        f = (z - zb) / (zt - zb)
        r = rb + (rt - rb) * f
        add_band(P, (x, y, z), (0, 0, 1), r - 1.0, r + 7.0, 40.0, "Dark", segs=16)
    add_band(P, (x, y, zt - 20), (0, 0, 1), rt - 18, rt + 10, 50.0, "Metal", segs=16)
    add_band(P, (x, y, zt - 80), (0, 0, 1), rt - 2.0, rt + 4.0, 60.0, "Glow", segs=16)       # heat ring
    P.add(lathe([(0, zt - 30), (rt - 16, zt - 30), (rt - 22, zt + 25), (rt * 0.45, zt + 70), (0, zt + 85)], 16),
          "Glow", True, T(x, y, 0))                                                          # ember plume
    # ladder cage stripe
    P.add(box(6, 40, zt - zb - 200), "Dark", False, T(x + rb * 0.98, y, (zb + zt) / 2))


def build_furnace():
    P = Part("SM_RooftopProps_Furnace")
    P.ue_space = True
    rng = random.Random(3)
    stacks = [(-1500.0, -2000.0, -1600.0, 3000.0, 180.0, 135.0), (-1700.0, 1800.0, -1600.0, 2500.0, 165.0, 120.0),
              (-3600.0, -1150.0, -2400.0, 3600.0, 120.0, 85.0), (2500.0, -2000.0, -1600.0, 2600.0, 160.0, 115.0),
              (1300.0, 2400.0, -1600.0, 1900.0, 140.0, 100.0)]
    for (x, y, zb, zt, rb, rt) in stacks:
        chimney(P, x, y, zb, zt, rb, rt, rng)
    # blast furnace body (behind / right of the hoop, below the slab level)
    fx, fy = -1900.0, -2700.0
    prof = [(0, -2000), (420, -2000), (420, -600), (470, -300), (380, 200), (260, 560), (180, 700), (0, 720)]
    P.add(lathe(prof, 20), "Dark", True, T(fx, fy, 0))
    for z in (-500.0, -150.0, 150.0):
        add_band(P, (fx, fy, z), (0, 0, 1), 440 - (z + 500) * 0.12, 470 - (z + 500) * 0.12, 30.0, "Metal", segs=20)
    add_band(P, (fx, fy, -260.0), (0, 0, 1), 455.0, 470.0, 26.0, "Glow", segs=20)          # tuyere glow ring
    for k in range(6):  # bustle pipes around the furnace
        a = 2 * math.pi * k / 6
        p0 = Vector((fx + 480 * math.cos(a), fy + 480 * math.sin(a), -260))
        add_cyl(P, p0, p0 + Vector((0, 0, 700)), 22, 22, "Metal", segs=10)
    add_cyl(P, (fx, fy, 700), (fx, fy, 1500), 90, 70, "Metal", segs=16)                       # downcomer
    add_tube(P, [(fx, fy, 1500), (fx + 100, fy - 300, 1700), (fx + 300, fy - 470, 1600)], 45, "Metal", segs=12)
    # pipe bridge from the furnace to the corner stack
    add_tube(P, [(fx + 300, fy + 200, 900), (-1700, -2300, 1150), (-1500, -2000, 1000)], 40, "Metal", segs=12)
    return P


def keep_out_violations(P, z_min=-5.0):
    """Count vertices of a UE-space part inside the play box (above the floor)."""
    k = KEEP_OUT
    bad = 0
    for v in P.bm.verts:
        x, y, z = v.co.x, v.co.y, v.co.z
        if z > z_min and k["x0"] < x < k["x1"] and k["y0"] < y < k["y1"]:
            bad += 1
    return bad
