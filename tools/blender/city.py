"""
city.py - Nagisa skyline around the rooftop court, authored in UE court coordinates (cm).

  SM_CityA_01..12  ally district  (-Y side and behind): older flooded blocks, Tetsuba industry, warm light
  SM_CityB_01..12  rival district (+Y side and behind): mid-rise blocks and upper-district glass towers
  SM_Sea           water plane at z = -4000 + lantern boats and stilt piers

Each cluster sits 40-250 m from the court centre (550, 0).  The rooftop is ~40 m above the sea, so lower-district
buildings rise from z = -4000; upper-district towers rise far above the court.  Windows are separate quads in slot
"Window" (C++ lights them as a team scores); every window quad carries a vertex colour (R = 0.15..1.0) that
M_TaGBase multiplies into the emissive, so a lit district shows natural variation instead of a flat panel.
"""
import math
import random
from mathutils import Vector, Matrix
from tagmesh import Part, T, R, S, box, cylinder, band, lathe, prism, quad, sphere, tube, frame

COURT_C = Vector((550.0, 0.0, 0.0))
# furnace complex footprints (x, y, radius) from court.build_furnace - city buildings keep clear of them
FURNACE_KEEP = [(-1500.0, -2000.0, 260.0), (-1700.0, 1800.0, 240.0), (-3600.0, -1150.0, 200.0), (2500.0, -2000.0, 240.0),
                (1300.0, 2400.0, 220.0), (-1900.0, -2700.0, 650.0)]
SEA_Z = -4000.0

# (azimuth deg from +X toward +Y, distance m, kind) -- B is the +Y half, A the -Y half.
CLUSTERS = {
    "B": [(172, 62, "lower"), (158, 100, "mid"), (168, 195, "tower"), (137, 58, "lower"), (124, 125, "mid"),
          (111, 215, "tower"), (98, 72, "industrial"), (84, 160, "tower"), (64, 92, "mid"), (44, 205, "tower"),
          (24, 64, "lower"), (9, 145, "tower")],
    "A": [(188, 58, "industrial"), (200, 92, "mid"), (212, 165, "mid"), (223, 64, "lower"),
          (236, 122, "industrial"), (194, 228, "tower"), (263, 76, "lower"), (278, 152, "mid"),
          (296, 96, "industrial"), (318, 212, "tower"), (338, 62, "lower"), (352, 142, "mid")],
}


class Cluster:
    def __init__(self, P, rng):
        self.P = P
        self.rng = rng
        self.footprints = []   # (x, y, radius) in UE space for the sea props

    # ------------------------------------------------------------------ primitives in a local frame M
    def box(self, M, c, size, slot):
        self.P.add(box(*size), slot, False, M @ T(*c))

    def window(self, M, a, b, h, lit=None):
        a, b = Vector(a), Vector(b)
        up = Vector((0, 0, h))
        v = lit if lit is not None else self.rng.choice((0.15, 0.3, 0.5, 0.7, 0.85, 1.0, 1.0))
        self.P.add(quad(a, b, b + up, a + up), "Window", False, M, color=(v, v, v, 1.0))

    def facades(self, w, d):
        """(centre offset, outward normal, tangent, width) for the 4 walls of a w x d box centred at 0."""
        out = []
        for n, wid in (((1, 0, 0), d), ((-1, 0, 0), d), ((0, 1, 0), w), ((0, -1, 0), w)):
            n = Vector(n)
            t = Vector((0, 0, 1)).cross(n)
            off = (w / 2 if n.x else d / 2)
            out.append((n * off, n, t, wid))
        return out

    def building(self, M, w, d, z0, z1, style, roof="flat", fh=350.0, slot="Body"):
        h = z1 - z0
        self.box(M, (0, 0, z0 + h / 2), (w, d, h), slot)
        floors = int((h - 150) // fh)
        rng = self.rng
        for (c, n, t, wid) in self.facades(w, d):
            if wid < 300:
                continue
            base = c + n * 10.0
            for f in range(floors):
                z = z0 + 120 + f * fh
                if z + fh > z1 - 60:
                    break
                if style == "ribbon":
                    m = 80.0
                    self.window(M, base - t * (wid / 2 - m) + Vector((0, 0, z + 90)),
                                base + t * (wid / 2 - m) + Vector((0, 0, z + 90)), fh * 0.5)
                elif style == "curtain":
                    m = 40.0
                    self.window(M, base - t * (wid / 2 - m) + Vector((0, 0, z + 25)),
                                base + t * (wid / 2 - m) + Vector((0, 0, z + 25)), fh - 60)
                else:  # grid of individual windows
                    ww, gap = 130.0, 110.0
                    ncol = max(1, int((wid - 120) // (ww + gap)))
                    span = ncol * ww + (ncol - 1) * gap
                    for k in range(ncol):
                        if rng.random() < 0.12:
                            continue
                        x0 = -span / 2 + k * (ww + gap)
                        self.window(M, base + t * x0 + Vector((0, 0, z + 90)),
                                    base + t * (x0 + ww) + Vector((0, 0, z + 90)), min(160.0, fh * 0.55))
        # roof
        if roof == "saw":
            return
        if roof == "pitched":
            ridge = min(w, d) * 0.35
            poly = [(-w / 2 - 30, 0), (w / 2 + 30, 0), (0, ridge)]
            self.P.add(prism(poly, d + 60), "Accent", False, M @ T(0, d / 2 + 30, z1) @ R(90, 'X'))
        else:
            self.box(M, (0, 0, z1 + 30), (w + 30, d + 30, 60), "Dark")
            if rng.random() < 0.6 and w > 500 and d > 500:
                tx, ty = rng.uniform(-w / 4, w / 4), rng.uniform(-d / 4, d / 4)
                self.P.add(cylinder(260, 140, 140, 10), "Dark", True, M @ T(tx, ty, z1 + 60))
                self.P.add(lathe([(0, 0), (150, 0), (0, 70)], 10), "Dark", True, M @ T(tx, ty, z1 + 320))
            for _ in range(rng.randint(0, 3)):
                ax, ay = rng.uniform(-w / 3, w / 3), rng.uniform(-d / 3, d / 3)
                self.box(M, (ax, ay, z1 + 110), (rng.uniform(120, 260), rng.uniform(120, 200), 100), "Metal")

    def neon(self, M, w, d, z0, z1):
        """Vertical blade sign sticking out of a facade; glowing panels on both faces."""
        rng = self.rng
        side = rng.choice((1, -1))
        H = rng.uniform(500, 1100)
        if rng.random() < 0.5:
            x, y = side * (w / 2 + 75), rng.uniform(-d / 4, d / 4)
            frame_sz, glow_sz = (140, 30, H + 30), (120, 36, H)
        else:
            x, y = rng.uniform(-w / 4, w / 4), side * (d / 2 + 75)
            frame_sz, glow_sz = (30, 140, H + 30), (36, 120, H)
        zc = min(z1 - H / 2 - 80, z0 + rng.uniform(0.45, 0.8) * (z1 - z0))
        self.box(M, (x, y, zc), frame_sz, "Dark")
        self.box(M, (x, y, zc), glow_sz, "Glow")

    def lantern_string(self, a, b, sag, n=9):
        a, b = Vector(a), Vector(b)
        self.P.add(tube([a.lerp(b, i / 12) - Vector((0, 0, sag * 4 * (i / 12) * (1 - i / 12))) for i in range(13)],
                        4.0, 3, False, False), "Dark", False)
        for i in range(1, n):
            t = i / n
            p = a.lerp(b, t) - Vector((0, 0, sag * 4 * t * (1 - t) + 40))
            self.P.add(sphere(32.0, 6, 3), "Glow", True, T(*p) @ S(1, 1, 1.4))

    def spire(self, M, z1, h):
        self.P.add(cylinder(h, 40, 8, 6), "Metal", False, M @ T(0, 0, z1))
        self.P.add(sphere(55, 6, 4), "Glow", True, M @ T(0, 0, z1 + h))

    # ------------------------------------------------------------------ cluster kinds
    def place(self, center, n, spread, size_rng, min_gap=300.0):
        rng = self.rng
        out = []
        tries = 0
        while len(out) < n and tries < 400:
            tries += 1
            a = rng.uniform(0, 2 * math.pi)
            r = spread * math.sqrt(rng.random())
            x, y = center.x + r * math.cos(a), center.y + r * math.sin(a)
            w, d = rng.uniform(*size_rng), rng.uniform(*size_rng)
            rad = 0.5 * math.hypot(w, d)
            # keep the rooftop area (slab + furnace complex) clear
            if abs(x - 550) < 2600 + rad and abs(y) < 2700 + rad:
                continue
            if any(math.hypot(x - fx, y - fy) < fr + rad + 300 for (fx, fy, fr) in FURNACE_KEEP):
                continue
            if all(math.hypot(x - ox, y - oy) > rad + orad + min_gap for (ox, oy, orad, _, _) in out):
                out.append((x, y, rad, w, d))
        return out

    def frame_at(self, x, y, yaw):
        return T(x, y, 0) @ R(yaw, 'Z')


def build_cluster(side, idx, az, dist, kind, seed):
    name = "SM_City%s_%02d" % (side, idx)
    P = Part(name)
    P.ue_space = True
    rng = random.Random(seed)
    C = Cluster(P, rng)
    a = math.radians(az)
    center = COURT_C + Vector((math.cos(a), math.sin(a), 0)) * (dist * 100.0)
    yaw = az + rng.uniform(-12, 12)
    warm = side == "A"
    if kind == "lower":
        sites = C.place(center, rng.randint(8, 11), 2500.0, (600.0, 1300.0), 250.0)
        tops = []
        for (x, y, rad, w, d) in sites:
            M = C.frame_at(x, y, yaw + rng.choice((0, 90)) + rng.uniform(-6, 6))
            z1 = SEA_Z + rng.uniform(800, 2300)
            C.building(M, w, d, SEA_Z, z1, "grid", roof="pitched" if rng.random() < 0.55 else "flat", fh=320.0)
            if rng.random() < 0.45:
                C.neon(M, w, d, SEA_Z, z1)
            tops.append(Vector((x, y, z1)))
            C.footprints.append((x, y, rad, SEA_Z, z1))
        for i in range(min(3, len(tops) - 1)):
            p, q = tops[i], tops[i + 1]
            z = min(p.z, q.z) - rng.uniform(150, 400)
            C.lantern_string((p.x, p.y, z), (q.x, q.y, z), rng.uniform(80, 200))
    elif kind == "mid":
        sites = C.place(center, rng.randint(5, 7), 3200.0, (1200.0, 2400.0), 400.0)
        for (x, y, rad, w, d) in sites:
            M = C.frame_at(x, y, yaw + rng.uniform(-5, 5))
            z1 = rng.uniform(-1200, 1800)
            style = "ribbon" if rng.random() < 0.7 else "grid"
            C.building(M, w, d, SEA_Z, z1, style, fh=350.0)
            if rng.random() < 0.6:
                C.neon(M, w, d, SEA_Z, z1)
            C.footprints.append((x, y, rad, SEA_Z, z1))
        if sites:  # rooftop billboard on the first block
            x, y, rad, w, d = sites[0]
            M = C.frame_at(x, y, yaw)
            ztop = max(f[4] for f in C.footprints[:1])
            C.box(M, (0, 0, ztop + 420), (60, w * 0.7, 520), "Dark")
            C.box(M, (-35, 0, ztop + 420), (10, w * 0.66, 480), "Glow")
            for s in (1, -1):
                C.box(M, (0, s * w * 0.25, ztop + 120), (30, 30, 200), "Metal")
    elif kind == "industrial":
        # factory hall with a sawtooth roof
        hx, hy = center.x, center.y
        M = C.frame_at(hx, hy, yaw)
        L, W, H = 4200.0, 1800.0, 1500.0
        z0 = SEA_Z + 1200.0
        C.box(M, (0, 0, (SEA_Z + z0) / 2), (L + 200, W + 200, z0 - SEA_Z), "Dark")      # quay / plinth
        C.building(M, L, W, z0, z0 + H, "ribbon", roof="saw", fh=500.0)
        teeth = 7
        for k in range(teeth):
            x0 = -L / 2 + k * L / teeth
            poly = [(0, 0), (L / teeth, 0), (L / teeth, 420)]
            P.add(prism(poly, W), "Metal", False, M @ T(x0, W / 2, z0 + H + 60) @ R(90, 'X'))
        C.footprints.append((hx, hy, 0.5 * math.hypot(L, W), SEA_Z, z0 + H))
        # chimneys with glowing rings
        for k in range(2):
            cx, cy = -L / 2 + 500 + k * 900, -W / 2 - 400
            zt = rng.uniform(0, 2000)
            P.add(cylinder(zt - SEA_Z, 160, 110, 12), "Dark", True, M @ T(cx, cy, SEA_Z))
            P.add(band(104, 122, 70, 12), "Glow", True, M @ T(cx, cy, zt - 60))
            P.add(band(150, 172, 60, 12), "Metal", True, M @ T(cx, cy, (SEA_Z + zt) / 2))
        # gas holder
        gx, gy = L / 2 + 1300, 0.0
        P.add(cylinder(2600, 950, 950, 20), "Metal", True, M @ T(gx, gy, SEA_Z))
        for z in range(int(SEA_Z) + 400, int(SEA_Z) + 2600, 450):
            P.add(band(948, 985, 50, 20), "Dark", True, M @ T(gx, gy, z))
        C.footprints.append((hx + gx, hy, 1000, SEA_Z, SEA_Z + 2600))
        # gantry crane
        for s in (1, -1):
            P.add(box(120, 120, 3600), "Metal", False, M @ T(-L / 2 - 600, s * 900, SEA_Z + 1800))
        P.add(box(400, 2200, 150), "Accent", False, M @ T(-L / 2 - 600, 0, SEA_Z + 3600))
        P.add(box(3000, 120, 120), "Accent", False, M @ T(-L / 2 - 600 + 1300, 0, SEA_Z + 3500))
        P.add(sphere(45, 6, 4), "Glow", True, M @ T(-L / 2 - 600, 0, SEA_Z + 3700))
        # a few support blocks around the plant
        for (x, y, rad, w, d) in C.place(center, 2, 3600.0, (1000.0, 1700.0), 600.0):
            Mb = C.frame_at(x, y, yaw + 90)
            z1 = rng.uniform(-2500, -500)
            C.building(Mb, w, d, SEA_Z, z1, "ribbon", fh=380.0)
            C.footprints.append((x, y, rad, SEA_Z, z1))
    elif kind == "tower":
        sites = C.place(center, rng.randint(4, 6), 4600.0, (2200.0, 3600.0), 900.0)
        for i, (x, y, rad, w, d) in enumerate(sites):
            M = C.frame_at(x, y, yaw + rng.uniform(-20, 20))
            z1 = rng.uniform(5000, 15000) if i < 2 else rng.uniform(2500, 9000)
            C.building(M, w, d, SEA_Z, z1, "curtain", fh=400.0)
            # setback crown + spire with beacon
            cw, cd = w * 0.62, d * 0.62
            C.box(M, (0, 0, z1 + 60 + 450), (cw, cd, 900), "Accent")
            for (c, n, t, wid) in C.facades(cw, cd):
                base = c + n * 10.0
                C.window(M, base - t * (wid / 2 - 40) + Vector((0, 0, z1 + 200)),
                         base + t * (wid / 2 - 40) + Vector((0, 0, z1 + 200)), 500, lit=1.0)
            C.spire(M, z1 + 960, rng.uniform(800, 2200))
            C.footprints.append((x, y, rad, SEA_Z, z1))
            if rng.random() < 0.5:
                C.neon(M, w, d, SEA_Z + 3000, z1)
    return P, C.footprints


def build_city():
    parts = {"A": [], "B": []}
    footprints = []
    for side in ("A", "B"):
        for i, (az, dist, kind) in enumerate(CLUSTERS[side]):
            P, fp = build_cluster(side, i + 1, az, dist, kind, seed=(1000 if side == "A" else 2000) + i * 17)
            P.meta = dict(side=side, azimuth_deg=az, distance_m=dist, kind=kind)
            parts[side].append(P)
            footprints.extend(fp)
    return parts, footprints


# ================================================================================================================
# SM_Sea
# ================================================================================================================
def build_sea(footprints):
    P = Part("SM_Sea")
    P.ue_space = True
    ext = 120000.0
    n = 12
    bm_quads = []
    for i in range(n):
        for j in range(n):
            x0 = -ext + 2 * ext * i / n + 550
            x1 = -ext + 2 * ext * (i + 1) / n + 550
            y0 = -ext + 2 * ext * j / n
            y1 = -ext + 2 * ext * (j + 1) / n
            P.add(quad((x0, y0, SEA_Z), (x1, y0, SEA_Z), (x1, y1, SEA_Z), (x0, y1, SEA_Z)), "Water", True)
    rng = random.Random(99)

    def clear(x, y, r):
        if abs(x - 550) < 1800 + r and abs(y) < 1900 + r:
            return False
        if any(math.hypot(x - fx, y - fy) < fr + r + 100 for (fx, fy, fr) in FURNACE_KEEP):
            return False
        return all(math.hypot(x - fx, y - fy) > fr + r + 150 for (fx, fy, fr, _, _) in footprints)

    boats = 0
    tries = 0
    while boats < 22 and tries < 3000:
        tries += 1
        a = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(3500, 11000)
        x, y = 550 + d * math.cos(a), d * math.sin(a)
        if not clear(x, y, 500):
            continue
        yaw = rng.uniform(0, 360)
        M = T(x, y, SEA_Z) @ R(yaw, 'Z')
        L, W = rng.uniform(600, 900), rng.uniform(170, 230)
        hull = [(-L / 2, -W / 2), (L / 2 - 120, -W / 2), (L / 2, 0), (L / 2 - 120, W / 2), (-L / 2, W / 2)]
        P.add(prism(hull, 70), "Body", False, M @ T(0, 0, -20))
        P.add(box(L * 0.35, W * 0.7, 90), "Dark", False, M @ T(-L * 0.15, 0, 95))
        P.add(cylinder(180, 3, 3, 4), "Dark", False, M @ T(L * 0.2, 0, 50))
        P.add(sphere(28, 6, 3), "Glow", True, M @ T(L * 0.2, 0, 245) @ S(1, 1, 1.3))
        boats += 1
    piers = 0
    tries = 0
    while piers < 6 and tries < 3000:
        tries += 1
        a = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(4000, 9000)
        x, y = 550 + d * math.cos(a), d * math.sin(a)
        if not clear(x, y, 1300):
            continue
        yaw = rng.uniform(0, 360)
        M = T(x, y, SEA_Z) @ R(yaw, 'Z')
        L = rng.uniform(1600, 2400)
        P.add(box(L, 240, 25), "Body", False, M @ T(0, 0, 160))
        for k in range(int(L // 300) + 1):
            px = -L / 2 + k * 300
            for s in (1, -1):
                P.add(cylinder(180, 9, 9, 5), "Dark", False, M @ T(px, s * 105, -20))
            if k % 2 == 0:
                P.add(cylinder(160, 3, 3, 4), "Dark", False, M @ T(px, 115, 170))
                P.add(sphere(24, 6, 3), "Glow", True, M @ T(px, 115, 345) @ S(1, 1, 1.35))
        piers += 1
    # floating sky lanterns rising from the bay (fantasy dressing), clear of the court and the buildings
    lanterns = 0
    tries = 0
    while lanterns < 90 and tries < 5000:
        tries += 1
        a = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(2600, 16000)
        x, y = 550 + d * math.cos(a), d * math.sin(a)
        if not clear(x, y, 200):
            continue
        z = SEA_Z + rng.uniform(300, 7000)
        sc = rng.uniform(0.8, 1.6)
        P.add(sphere(32 * sc, 8, 5), "Glow", True, T(x, y, z) @ S(1, 1, 1.35))
        lanterns += 1
    P.meta = dict(boats=boats, piers=piers, lanterns=lanterns)
    return P
