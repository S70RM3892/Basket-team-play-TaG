"""
sdf.py - signed-distance-field modelling for the stylized ("clay / designer toy") look.

A Group is ONE material island: primitives combined with smooth union / smooth subtraction / smooth intersection.
Groups are meshed separately (surface nets on a voxel grid, vertices projected onto the exact surface, then
decimated by Blender's quadric collapse), so every colour region is its own closed, crisp shell. Interpenetrating
shells are fine (toy assembly): the inverted-hull outline then also draws an ink line where two shells meet.

Units: cm. Space: the Part's local space in Blender axes (+X forward, +Y = LEFT, +Z up).
Everything is numpy; points are (N, 3) float64 arrays.
"""
import math
import numpy as np

# ------------------------------------------------------------------------------------------------------------------
# small vector helpers
# ------------------------------------------------------------------------------------------------------------------


def _v(p):
    return np.asarray(p, dtype=np.float64).reshape(3)


def _len(a):
    return np.sqrt(np.einsum("ij,ij->i", a, a))


def rot_matrix(rx=0.0, ry=0.0, rz=0.0):
    """Rotation (degrees) applied X then Y then Z (extrinsic). Returns 3x3 local->part."""
    a, b, c = (math.radians(x) for x in (rx, ry, rz))
    Rx = np.array([[1, 0, 0], [0, math.cos(a), -math.sin(a)], [0, math.sin(a), math.cos(a)]])
    Ry = np.array([[math.cos(b), 0, math.sin(b)], [0, 1, 0], [-math.sin(b), 0, math.cos(b)]])
    Rz = np.array([[math.cos(c), -math.sin(c), 0], [math.sin(c), math.cos(c), 0], [0, 0, 1]])
    return Rz @ Ry @ Rx


def axis_matrix(axis, hint=(1.0, 0.0, 0.0)):
    """3x3 whose local +Z points along `axis` (local->part)."""
    z = _v(axis)
    z = z / max(np.linalg.norm(z), 1e-9)
    h = _v(hint)
    if abs(np.dot(h, z)) > 0.95:
        h = np.array([0.0, 1.0, 0.0]) if abs(z[1]) < 0.9 else np.array([1.0, 0.0, 0.0])
    x = h - z * np.dot(h, z)
    x /= np.linalg.norm(x)
    y = np.cross(z, x)
    return np.stack([x, y, z], axis=1)


# ------------------------------------------------------------------------------------------------------------------
# primitives: each has dist(P) and a conservative bounding box
# ------------------------------------------------------------------------------------------------------------------
class Prim:
    def __init__(self, center=(0, 0, 0), rot=None):
        self.c = _v(center)
        self.R = np.eye(3) if rot is None else np.asarray(rot, dtype=np.float64)

    def local(self, P):
        return (P - self.c) @ self.R          # part -> local (R is orthonormal, local->part)

    def extent(self):                         # half extent in local space (for bounds)
        raise NotImplementedError

    def bounds(self):
        e = np.abs(self.R) @ self.extent()
        return self.c - e, self.c + e


class Sphere(Prim):
    def __init__(self, c, r):
        super().__init__(c)
        self.r = float(r)

    def dist(self, P):
        return _len(P - self.c) - self.r

    def extent(self):
        return np.array([self.r] * 3)


class Ellipsoid(Prim):
    """Axis radii (local). Inigo Quilez' bound-corrected approximation."""

    def __init__(self, c, radii, rot=None):
        super().__init__(c, rot)
        self.r = _v(radii)

    def dist(self, P):
        q = self.local(P)
        k0 = _len(q / self.r)
        k1 = _len(q / (self.r * self.r))
        return np.where(k1 > 1e-9, k0 * (k0 - 1.0) / np.maximum(k1, 1e-9), -self.r.min())

    def extent(self):
        return self.r.copy()


class RoundCone(Prim):
    """Capsule with different end radii between points a and b (Quilez sdRoundCone, arbitrary orientation)."""

    def __init__(self, a, b, ra, rb=None):
        super().__init__((0, 0, 0))
        self.a, self.b = _v(a), _v(b)
        self.ra = float(ra)
        self.rb = float(ra if rb is None else rb)

    def dist(self, P):
        a, b, r1, r2 = self.a, self.b, self.ra, self.rb
        ba = b - a
        l2 = float(np.dot(ba, ba))
        if l2 < 1e-9:
            return _len(P - a) - max(r1, r2)
        rr = r1 - r2
        a2 = l2 - rr * rr
        il2 = 1.0 / l2
        pa = P - a
        y = pa @ ba
        z = y - l2
        xv = pa * l2 - np.outer(y, ba)
        x2 = np.einsum("ij,ij->i", xv, xv)
        y2 = y * y * l2
        z2 = z * z * l2
        k = math.copysign(1.0, rr) * rr * rr * x2 if rr != 0 else np.zeros_like(x2)
        d_b = np.sqrt(x2 + z2) * il2 - r2
        d_a = np.sqrt(x2 + y2) * il2 - r1
        d_m = (np.sqrt(np.maximum(x2 * a2 * il2, 0.0)) + y * rr) * il2 - r1
        out = np.where(np.sign(z) * a2 * z2 > k, d_b, np.where(np.sign(y) * a2 * y2 < k, d_a, d_m))
        return out

    def bounds(self):
        r = max(self.ra, self.rb)
        lo = np.minimum(self.a - self.ra, self.b - self.rb)
        hi = np.maximum(self.a + self.ra, self.b + self.rb)
        return lo - 0.0 * r, hi + 0.0 * r


def Capsule(a, b, r):
    return RoundCone(a, b, r, r)


class RoundBox(Prim):
    """Box with half sizes `half` (local) and rounding radius `r` (inside the half sizes)."""

    def __init__(self, c, half, r=0.0, rot=None):
        super().__init__(c, rot)
        self.h = _v(half)
        self.r = float(r)

    def dist(self, P):
        q = np.abs(self.local(P)) - (self.h - self.r)
        outside = _len(np.maximum(q, 0.0))
        inside = np.minimum(q.max(axis=1), 0.0)
        return outside + inside - self.r

    def extent(self):
        return self.h.copy()


class Torus(Prim):
    """Ring of radius R and tube radius r around local +Z; `sx`, `sy` stretch the ring (not the tube)."""

    def __init__(self, c, R, r, rot=None, sx=1.0, sy=1.0):
        super().__init__(c, rot)
        self.Rr, self.rr, self.sx, self.sy = float(R), float(r), float(sx), float(sy)

    def dist(self, P):
        q = self.local(P)
        rad = np.sqrt((q[:, 0] / self.sx) ** 2 + (q[:, 1] / self.sy) ** 2) * min(self.sx, self.sy)
        rad = np.sqrt((q[:, 0] / self.sx) ** 2 + (q[:, 1] / self.sy) ** 2)
        d2 = np.stack([(rad - self.Rr) * min(self.sx, self.sy), q[:, 2]], axis=1)
        return _len(d2) - self.rr

    def extent(self):
        e = self.Rr * max(self.sx, self.sy) + self.rr
        return np.array([self.Rr * self.sx + self.rr, self.Rr * self.sy + self.rr, self.rr])


class Cylinder(Prim):
    """Capped cylinder along local Z, radius r, half height h, edge rounding e."""

    def __init__(self, c, r, h, e=0.0, rot=None):
        super().__init__(c, rot)
        self.r, self.h, self.e = float(r), float(h), float(e)

    def dist(self, P):
        q = self.local(P)
        dx = np.sqrt(q[:, 0] ** 2 + q[:, 1] ** 2) - (self.r - self.e)
        dz = np.abs(q[:, 2]) - (self.h - self.e)
        out = _len(np.stack([np.maximum(dx, 0), np.maximum(dz, 0)], axis=1))
        return np.minimum(np.maximum(dx, dz), 0.0) + out - self.e

    def extent(self):
        return np.array([self.r, self.r, self.h])


class Cone(Prim):
    """Rounded truncated cone along local Z from z=-h (radius r0) to z=+h (radius r1)."""

    def __init__(self, c, r0, r1, h, rot=None):
        super().__init__(c, rot)
        self.inner = RoundCone((0, 0, -h), (0, 0, h), r0, r1)
        self.h, self.rmax = float(h), max(r0, r1)

    def dist(self, P):
        return self.inner.dist(self.local(P))

    def extent(self):
        return np.array([self.rmax, self.rmax, self.h + self.rmax])


class HalfSpace(Prim):
    """Inside = the side opposite to the normal n (use with intersect / subtract to cut flat)."""

    def __init__(self, c, n):
        super().__init__(c)
        self.n = _v(n) / np.linalg.norm(_v(n))

    def dist(self, P):
        return (P - self.c) @ self.n

    def bounds(self):
        return np.array([-1e5] * 3), np.array([1e5] * 3)


class Displace:
    """Wraps a primitive and adds a cheap smooth 3D ripple (bumps for moss, rock, clouds)."""

    def __init__(self, prim, amp, freq, seed=0.0):
        self.p, self.amp, self.f, self.s = prim, float(amp), float(freq), float(seed)

    def dist(self, P):
        f, s = self.f, self.s
        n = (np.sin(P[:, 0] * f + s) * np.sin(P[:, 1] * f * 1.13 + 2.1 * s) * np.sin(P[:, 2] * f * 0.87 + 0.7 + s)
             + 0.5 * np.sin(P[:, 0] * f * 2.3 + 1.7 + s) * np.sin(P[:, 1] * f * 2.1 + 0.3) * np.sin(P[:, 2] * f * 2.7 + s))
        return self.p.dist(P) + self.amp * n

    def bounds(self):
        lo, hi = self.p.bounds()
        return lo - abs(self.amp) * 1.5, hi + abs(self.amp) * 1.5


# ------------------------------------------------------------------------------------------------------------------
# smooth operators (polynomial)
# ------------------------------------------------------------------------------------------------------------------
def smin(a, b, k):
    if k <= 0:
        return np.minimum(a, b)
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return b + (a - b) * h - k * h * (1.0 - h)


def smax(a, b, k):
    return -smin(-a, -b, k)


class Group:
    """One material island. ops: ("u", prim, k) union, ("s", prim, k) subtract, ("i", prim, k) intersect.
    The first op must be a union. `mirror_y=True` mirrors every primitive across y = 0 (both sides in one shell)."""

    def __init__(self, slot, k=2.0, mirror_y=False, voxel=None, budget=None, name=""):
        self.slot = slot
        self.k = float(k)
        self.ops = []
        self.mirror = mirror_y
        self.voxel = voxel
        self.budget = budget
        self.name = name

    def u(self, prim, k=None):
        self.ops.append(("u", prim, self.k if k is None else float(k)))
        return self

    def s(self, prim, k=None):
        self.ops.append(("s", prim, self.k * 0.5 if k is None else float(k)))
        return self

    def i(self, prim, k=None):
        self.ops.append(("i", prim, 0.0 if k is None else float(k)))
        return self

    def empty(self):
        return not any(o[0] == "u" for o in self.ops)

    def dist(self, P):
        if self.mirror:
            P = P.copy()
            P[:, 1] = np.abs(P[:, 1])
        d = None
        for op, prim, k in self.ops:
            pd = prim.dist(P)
            if d is None:
                d = pd
            elif op == "u":
                d = smin(d, pd, k)
            elif op == "s":
                d = smax(d, -pd, k)
            else:
                d = smax(d, pd, k)
        return d

    def bounds(self):
        lo = np.array([1e9] * 3)
        hi = np.array([-1e9] * 3)
        for op, prim, k in self.ops:
            if op != "u":
                continue
            a, b = prim.bounds()
            lo = np.minimum(lo, a - k)
            hi = np.maximum(hi, b + k)
        if self.mirror:
            m = max(abs(lo[1]), abs(hi[1]))
            lo[1], hi[1] = -m, m
        return lo, hi


# ------------------------------------------------------------------------------------------------------------------
# surface nets
# ------------------------------------------------------------------------------------------------------------------
def surface_nets(fn, lo, hi, h):
    """Mesh the zero level set of fn over the box [lo, hi] with voxel size h.
    Returns (verts (V,3), tris (T,3)) with outward (counter-clockwise from outside) winding."""
    lo = np.asarray(lo, float) - 2.0 * h
    hi = np.asarray(hi, float) + 2.0 * h
    n = np.maximum(np.ceil((hi - lo) / h).astype(int) + 1, 3)
    xs = lo[0] + np.arange(n[0]) * h
    ys = lo[1] + np.arange(n[1]) * h
    zs = lo[2] + np.arange(n[2]) * h
    G = np.stack(np.meshgrid(xs, ys, zs, indexing="ij"), axis=-1).reshape(-1, 3)
    F = fn(G).reshape(n[0], n[1], n[2])
    inside = F < 0.0
    # ---- edge crossings on the three edge families
    sums = np.zeros((n[0] - 1, n[1] - 1, n[2] - 1, 3))
    cnt = np.zeros((n[0] - 1, n[1] - 1, n[2] - 1))
    grid = np.stack(np.meshgrid(xs, ys, zs, indexing="ij"), axis=-1)
    for ax in range(3):
        sl0 = [slice(None)] * 3
        sl1 = [slice(None)] * 3
        sl0[ax] = slice(0, -1)
        sl1[ax] = slice(1, None)
        f0, f1 = F[tuple(sl0)], F[tuple(sl1)]
        m = (f0 < 0) != (f1 < 0)
        t = np.where(m, f0 / np.where(m, f0 - f1, 1.0), 0.0)
        p0 = grid[tuple(sl0)]
        pos = p0.copy()
        pos[..., ax] = pos[..., ax] + t * h
        pos = pos * m[..., None]
        mf = m.astype(float)
        # an edge along `ax` belongs to the 4 cells around it (offsets in the two other axes)
        o1, o2 = [a for a in range(3) if a != ax]
        for d1 in (0, 1):
            for d2 in (0, 1):
                src = [slice(None)] * 3
                src[o1] = slice(d1, d1 + n[o1] - 1)
                src[o2] = slice(d2, d2 + n[o2] - 1)
                sums += pos[tuple(src)]
                cnt += mf[tuple(src)]
    active = cnt > 0
    idx = -np.ones(active.shape, dtype=np.int64)
    idx[active] = np.arange(int(active.sum()))
    verts = sums[active] / cnt[active][:, None]
    # ---- quads: one per sign-changing edge, connecting the 4 cells around it
    tris = []
    for ax in range(3):
        o1, o2 = [a for a in range(3) if a != ax]
        sl0 = [slice(None)] * 3
        sl1 = [slice(None)] * 3
        sl0[ax] = slice(0, -1)
        sl1[ax] = slice(1, None)
        a_in = inside[tuple(sl0)]
        m = a_in != inside[tuple(sl1)]
        # interior edges only (need cells at -1 and 0 in both other axes)
        keep = np.zeros_like(m)
        ks = [slice(None)] * 3
        ks[o1] = slice(1, n[o1] - 1)
        ks[o2] = slice(1, n[o2] - 1)
        keep[tuple(ks)] = True
        m &= keep
        e = np.argwhere(m)
        if len(e) == 0:
            continue
        flip = a_in[m]

        def cell(d1, d2):
            c = e.copy()
            c[:, o1] -= d1
            c[:, o2] -= d2
            return idx[c[:, 0], c[:, 1], c[:, 2]]

        c00, c10, c11, c01 = cell(1, 1), cell(0, 1), cell(0, 0), cell(1, 0)
        q = np.stack([c00, c10, c11, c01], axis=1)
        # orientation: the right-handed (o1, o2, ax) frame; reverse when the inside is on the far side
        sign_ok = (o1, o2, ax) in ((0, 1, 2), (1, 2, 0), (2, 0, 1))
        rev = ~flip if sign_ok else flip
        q[rev] = q[rev][:, ::-1]
        tris.append(np.concatenate([q[:, [0, 1, 2]], q[:, [0, 2, 3]]], axis=0))
    tris = np.concatenate(tris, axis=0) if tris else np.zeros((0, 3), dtype=np.int64)
    good = np.all(tris >= 0, axis=1)
    return verts, tris[good]


def project(fn, V, h, steps=3):
    """Newton steps onto the zero level set (central-difference gradient), clamped to half a voxel each."""
    e = h * 0.25
    for _ in range(steps):
        d = fn(V)
        g = np.stack([(fn(V + [e, 0, 0]) - fn(V - [e, 0, 0])),
                      (fn(V + [0, e, 0]) - fn(V - [0, e, 0])),
                      (fn(V + [0, 0, e]) - fn(V - [0, 0, e]))], axis=1) / (2 * e)
        gl2 = np.maximum(np.einsum("ij,ij->i", g, g), 1e-9)
        step = (d / gl2)[:, None] * g
        sl = _len(step)
        s = np.minimum(1.0, (0.5 * h) / np.maximum(sl, 1e-9))
        V = V - step * s[:, None]
    return V


def normals_of(fn, V, h):
    e = h * 0.25
    g = np.stack([(fn(V + [e, 0, 0]) - fn(V - [e, 0, 0])),
                  (fn(V + [0, e, 0]) - fn(V - [0, e, 0])),
                  (fn(V + [0, 0, e]) - fn(V - [0, 0, e]))], axis=1)
    return g / np.maximum(_len(g), 1e-9)[:, None]


def mesh_group(group, h):
    lo, hi = group.bounds()
    V, Tr = surface_nets(group.dist, lo, hi, h)
    if len(V):
        V = project(group.dist, V, h)
    return V, Tr
