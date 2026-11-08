"""
tagmesh.py - small scripted-modeling toolkit shared by all Basket Team-play TaG asset builders.

Conventions (Blender side):
  * 1 Blender unit = 1 cm (scene scale_length = 0.01 when exporting).
  * +X = forward, +Y = the character's LEFT, +Z = up.  The FBX round trip maps Blender (x, y, z) -> UE (x, -y, z),
    so +Y here becomes -Y (= left) in Unreal.  Verified in tools/import/verify.py.
  * A Part is one rigid mesh: geometry (bmesh), an ordered list of material slot names and named sockets
    (positions in the part's local space, i.e. relative to its pivot).

Every primitive is built in a temporary bmesh and merged into the Part with a slot, a smooth flag and a matrix.
"""
import math
import bmesh
from mathutils import Vector, Matrix

# Canonical slot order (meshes list their slots in this order).
SLOT_ORDER = ["Body", "Accent", "Extra", "Skin", "Hair", "Dark", "Metal", "White", "Team", "TaG", "Glow", "Jelly",
              "Window", "Net", "Board", "Court", "Paint", "Apron", "Water", "Sky", "Outline"]


def vec(v):
    return v.copy() if isinstance(v, Vector) else Vector(v)


def frame(origin=(0, 0, 0), axis=(0, 0, 1), hint=None):
    """4x4 matrix whose local +Z points along `axis`, located at `origin`.
    `hint` is a direction the local +X should lean toward."""
    z = vec(axis).normalized()
    if hint is None:
        hint = Vector((1, 0, 0)) if abs(z.x) < 0.9 else Vector((0, 1, 0))
    h = vec(hint)
    x = h - z * h.dot(z)
    if x.length < 1e-6:
        h = Vector((0, 1, 0)) if abs(z.y) < 0.9 else Vector((1, 0, 0))
        x = h - z * h.dot(z)
    x.normalize()
    y = z.cross(x)
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = vec(origin)
    return m


def T(x=0.0, y=0.0, z=0.0):
    return Matrix.Translation(Vector((x, y, z)))


def R(deg, axis):
    return Matrix.Rotation(math.radians(deg), 4, axis)


def S(x, y=None, z=None):
    y = x if y is None else y
    z = x if z is None else z
    m = Matrix.Identity(4)
    m[0][0], m[1][1], m[2][2] = x, y, z
    return m


# ----------------------------------------------------------------------------------------------------------------
# Part
# ----------------------------------------------------------------------------------------------------------------
class Part:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.slots = []
        self.sockets = {}
        self.col = None          # optional per-corner colour layer (only used by window quads)
        self.collision = []      # list of (center, size) boxes -> UCX_ meshes (UE space when ue_space)
        self.ue_space = False    # True: geometry authored in UE coordinates, mirrored to Blender at finalize

    def mat(self, slot):
        if slot not in SLOT_ORDER:
            raise ValueError("unknown slot %r" % slot)
        if slot not in self.slots:
            self.slots.append(slot)
        return self.slots.index(slot)

    def _ensure_col(self):
        if self.col is None:
            self.col = self.bm.loops.layers.float_color.new("Col")
            for f in self.bm.faces:
                for lp in f.loops:
                    lp[self.col] = (1.0, 1.0, 1.0, 1.0)
        return self.col

    def add(self, src, slot, smooth=True, matrix=None, slots=None, color=None):
        """Merge temporary bmesh `src` (freed afterwards).  If `slots` is given, the source faces keep their own
        material_index, interpreted as an index into `slots`.  `color` = optional RGBA vertex colour."""
        mi = self.mat(slot) if slot else 0
        if color is not None:
            self._ensure_col()
        vmap = {}
        for v in src.verts:
            co = (matrix @ v.co) if matrix is not None else v.co.copy()
            vmap[v] = self.bm.verts.new(co)
        flip = matrix is not None and matrix.to_3x3().determinant() < 0
        cval = tuple(color) if color is not None else (1.0, 1.0, 1.0, 1.0)
        for f in src.faces:
            vs = [vmap[v] for v in f.verts]
            if flip:
                vs.reverse()
            try:
                nf = self.bm.faces.new(vs)
            except ValueError:
                continue
            if slots is not None:
                nf.material_index = self.mat(slots[f.material_index])
            else:
                nf.material_index = mi
            nf.smooth = smooth
            if self.col is not None:
                for lp in nf.loops:
                    lp[self.col] = cval
        src.free()
        return self

    def mirror_ue_to_blender(self):
        """Geometry authored in UE space (x, y, z) -> Blender space (x, -y, z) (winding flipped)."""
        for v in self.bm.verts:
            v.co.y = -v.co.y
        bmesh.ops.reverse_faces(self.bm, faces=self.bm.faces[:])
        self.sockets = {k: Vector((p.x, -p.y, p.z)) for k, p in self.sockets.items()}
        self.collision = [((c[0], -c[1], c[2]), sz) for (c, sz) in self.collision]
        self.ue_space = False
        return self

    def socket(self, name, pos):
        self.sockets[name] = vec(pos)

    def tri_count(self):
        return sum(len(f.verts) - 2 for f in self.bm.faces)

    def bounds(self):
        xs = [v.co for v in self.bm.verts]
        lo = Vector((min(p.x for p in xs), min(p.y for p in xs), min(p.z for p in xs)))
        hi = Vector((max(p.x for p in xs), max(p.y for p in xs), max(p.z for p in xs)))
        return lo, hi

    def finalize(self, sharp_deg=55.0):
        """Canonical slot order, triangulate, mark sharp edges (angle or material change)."""
        if self.ue_space:
            self.mirror_ue_to_blender()
        order = sorted(self.slots, key=SLOT_ORDER.index)
        remap = {i: order.index(s) for i, s in enumerate(self.slots)}
        for f in self.bm.faces:
            f.material_index = remap[f.material_index]
        self.slots = order
        bmesh.ops.triangulate(self.bm, faces=self.bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
        self.bm.normal_update()
        # box-projected UV0 (1 UV unit = 1/uv_scale cm): gives every triangle a valid tangent basis
        uvl = self.bm.loops.layers.uv.new("UVMap")
        sc = getattr(self, "uv_scale", 0.01)
        for f in self.bm.faces:
            nn = f.normal
            ax = max(range(3), key=lambda i: abs(nn[i]))
            sg = 1.0 if nn[ax] >= 0 else -1.0
            for lp in f.loops:
                c = lp.vert.co
                if ax == 0:
                    uv = (c.y * sg, c.z)
                elif ax == 1:
                    uv = (-c.x * sg, c.z)
                else:
                    uv = (c.x * sg, c.y)
                lp[uvl].uv = (uv[0] * sc, uv[1] * sc)
        thr = math.radians(sharp_deg)
        for e in self.bm.edges:
            lf = e.link_faces
            if len(lf) != 2:
                e.smooth = False
                continue
            a, b = lf
            if a.material_index != b.material_index or not (a.smooth and b.smooth):
                e.smooth = False
            elif a.normal.angle(b.normal, 0.0) > thr:
                e.smooth = False
            else:
                e.smooth = True
        return self


# ----------------------------------------------------------------------------------------------------------------
# Primitive builders (return a temporary bmesh in local space)
# ----------------------------------------------------------------------------------------------------------------
def _fix_normals(bm):
    """Recalculate outward normals when the temporary mesh is closed (every edge shared by two faces)."""
    if all(len(e.link_faces) == 2 for e in bm.edges):
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def _ring(bm, r, z, segs, phase=0.0):
    return [bm.verts.new((r * math.cos(2 * math.pi * i / segs + phase),
                          r * math.sin(2 * math.pi * i / segs + phase), z)) for i in range(segs)]


def lathe(profile, segs=12, closed=False, phase=0.0, cap=True, sx=1.0, sy=1.0):
    """Revolve profile [(r, z), ...] around +Z.  Points with r == 0 at the ends become poles.
    closed=True: the profile is a loop (counter-clockwise in the r/z plane) -> ring/torus, no caps.
    sx/sy scale the result (elliptic lathe)."""
    bm = bmesh.new()
    rings = []
    for (r, z) in profile:
        if r <= 1e-5:
            rings.append([bm.verts.new((0.0, 0.0, z))])
        else:
            rings.append(_ring(bm, r, z, segs, phase))
    pairs = [(k, k + 1) for k in range(len(rings) - 1)]
    if closed:
        pairs.append((len(rings) - 1, 0))
    for k0, k1 in pairs:
        A, B = rings[k0], rings[k1]
        if len(A) == 1 and len(B) == 1:
            continue
        for i in range(segs):
            j = (i + 1) % segs
            if len(A) == 1:
                bm.faces.new((A[0], B[j], B[i]))
            elif len(B) == 1:
                bm.faces.new((A[i], A[j], B[0]))
            else:
                bm.faces.new((A[i], A[j], B[j], B[i]))
    if cap and not closed:
        if len(rings[0]) > 1:
            bm.faces.new(list(reversed(rings[0])))
        if len(rings[-1]) > 1:
            bm.faces.new(rings[-1])
    if sx != 1.0 or sy != 1.0:
        for v in bm.verts:
            v.co.x *= sx
            v.co.y *= sy
    return _fix_normals(bm)


def sphere(r=1.0, segs=12, rings=8):
    prof = []
    for k in range(rings + 1):
        phi = math.pi * k / rings
        prof.append((r * math.sin(phi), -r * math.cos(phi)))
    prof[0] = (0.0, -r)
    prof[-1] = (0.0, r)
    return lathe(prof, segs)


def capsule_profile(L, r0, r1, cap_rings=3, mid=0):
    prof = []
    for k in range(cap_rings + 1):
        phi = (math.pi / 2) * k / cap_rings
        prof.append((r0 * math.sin(phi), -r0 * math.cos(phi)))
    for k in range(1, mid + 1):
        t = k / (mid + 1)
        prof.append((r0 + (r1 - r0) * t, L * t))
    for k in range(cap_rings + 1):
        phi = (math.pi / 2) * k / cap_rings
        prof.append((r1 * math.cos(phi), L + r1 * math.sin(phi)))
    prof[0] = (0.0, -r0)
    prof[-1] = (0.0, L + r1)
    return prof


def capsule(L, r0, r1, segs=12, cap_rings=3, mid=0):
    """Capsule along +Z from 0 to L with end-sphere radii r0 (at z=0) and r1 (at z=L)."""
    return lathe(capsule_profile(L, r0, r1, cap_rings, mid), segs)


def cylinder(L, r0, r1=None, segs=12, bevel=0.0):
    r1 = r0 if r1 is None else r1
    if bevel > 0:
        prof = [(0, 0), (r0 - bevel, 0), (r0, bevel), (r1, L - bevel), (r1 - bevel, L), (0, L)]
    else:
        prof = [(0, 0), (r0, 0), (r1, L), (0, L)]
    return lathe(prof, segs)


def tube_open(L, r0, r1=None, segs=12):
    r1 = r0 if r1 is None else r1
    return lathe([(r0, 0), (r1, L)], segs, cap=False)


def torus(R, r, segs=16, csegs=8):
    prof = [(R + r * math.cos(2 * math.pi * k / csegs), r * math.sin(2 * math.pi * k / csegs)) for k in range(csegs)]
    return lathe(prof, segs, closed=True)


def band(r_in, r_out, h, segs=16, round_=0.35):
    """Ring with a rounded-rectangle cross-section, centred on z=0 (height h)."""
    c = min(round_ * min(r_out - r_in, h) , (r_out - r_in) * 0.49, h * 0.49)
    hz = h / 2
    prof = [(r_in, -hz), (r_out - c, -hz), (r_out, -hz + c), (r_out, hz - c), (r_out - c, hz), (r_in, hz)]
    return lathe(prof, segs, closed=True)


def box(sx, sy, sz, bevel=0.0, bsegs=1):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * sx, v.co.y * sy, v.co.z * sz))
    if bevel > 0:
        b = min(bevel, sx * 0.49, sy * 0.49, sz * 0.49)
        bmesh.ops.bevel(bm, geom=bm.edges[:] + bm.verts[:], offset=b, offset_type='OFFSET', segments=bsegs,
                        profile=0.5, affect='EDGES', clamp_overlap=True)
    return bm


def se_points(a, b, n=2.0, k=16, phase=0.0):
    """Superellipse points in local XY (CCW): |x/a|^n + |y/b|^n = 1."""
    pts = []
    for i in range(k):
        t = 2 * math.pi * i / k + phase
        c, s = math.cos(t), math.sin(t)
        pts.append((a * math.copysign(abs(c) ** (2.0 / n), c), b * math.copysign(abs(s) ** (2.0 / n), s)))
    return pts


def loft(sections, k=16, cap0=True, cap1=True, pole0=None, pole1=None):
    """Loft superellipse sections stacked along +Z.
    sections: [(z, cx, cy, ax, by, n), ...] (ax = half size along X, by = half size along Y).
    pole0 / pole1: optional (x, y, z) points closing the ends with a fan."""
    bm = bmesh.new()
    rings = []
    for (z, cx, cy, ax, by, n) in sections:
        rings.append([bm.verts.new((cx + x, cy + y, z)) for (x, y) in se_points(ax, by, n, k)])
    for A, B in zip(rings[:-1], rings[1:]):
        for i in range(k):
            j = (i + 1) % k
            bm.faces.new((A[i], A[j], B[j], B[i]))
    if pole0 is not None:
        p = bm.verts.new(pole0)
        for i in range(k):
            bm.faces.new((rings[0][(i + 1) % k], rings[0][i], p))
    elif cap0:
        bm.faces.new(list(reversed(rings[0])))
    if pole1 is not None:
        p = bm.verts.new(pole1)
        for i in range(k):
            bm.faces.new((rings[-1][i], rings[-1][(i + 1) % k], p))
    elif cap1:
        bm.faces.new(rings[-1])
    return _fix_normals(bm)


def loft_pts(rings, cap0=True, cap1=True, pole0=None, pole1=None):
    """Loft explicit rings (lists of 3D points, same count, counter-clockwise about the loft direction)."""
    bm = bmesh.new()
    k = len(rings[0])
    vr = [[bm.verts.new(vec(p)) for p in ring] for ring in rings]
    for A, B in zip(vr[:-1], vr[1:]):
        for i in range(k):
            j = (i + 1) % k
            bm.faces.new((A[i], A[j], B[j], B[i]))
    if pole0 is not None:
        p = bm.verts.new(vec(pole0))
        for i in range(k):
            bm.faces.new((vr[0][(i + 1) % k], vr[0][i], p))
    elif cap0:
        bm.faces.new(list(reversed(vr[0])))
    if pole1 is not None:
        p = bm.verts.new(vec(pole1))
        for i in range(k):
            bm.faces.new((vr[-1][i], vr[-1][(i + 1) % k], p))
    elif cap1:
        bm.faces.new(vr[-1])
    return _fix_normals(bm)


def loft_x(sections, k=12, pole0=None, pole1=None, cap0=True, cap1=True):
    """Loft along +X: sections [(x, cy, cz, ay, bz, n)] (ay half-size along Y, bz half-size along Z)."""
    secs = [(x, cy, cz, ay, bz, n) for (x, cy, cz, ay, bz, n) in sections]
    bm = loft([(s[0], s[1], s[2], s[3], s[4], s[5]) for s in secs], k=k, cap0=cap0, cap1=cap1,
              pole0=None if pole0 is None else (pole0[1], pole0[2], pole0[0]),
              pole1=None if pole1 is None else (pole1[1], pole1[2], pole1[0]))
    # built with (x->cy.., y->cz.., z->x); rotate: local (u, v, w) = (y, z, x)
    for v in bm.verts:
        u, w, x = v.co.x, v.co.y, v.co.z
        v.co = Vector((x, u, w))
    return bm


def tube(points, radii, segs=6, cap0=True, cap1=True, pole0=None, pole1=None):
    """Sweep a circle along a polyline with parallel-transport frames.
    radii: float or list. pole0/pole1: extra distance to extend the end into a rounded/pointed tip."""
    pts = [vec(p) for p in points]
    n = len(pts)
    if not isinstance(radii, (list, tuple)):
        radii = [radii] * n
    tans = []
    for i in range(n):
        if i == 0:
            t = pts[1] - pts[0]
        elif i == n - 1:
            t = pts[-1] - pts[-2]
        else:
            t = (pts[i + 1] - pts[i]).normalized() + (pts[i] - pts[i - 1]).normalized()
        tans.append(t.normalized())
    # initial normal
    t0 = tans[0]
    ref = Vector((0, 0, 1)) if abs(t0.z) < 0.9 else Vector((1, 0, 0))
    nrm = (ref - t0 * ref.dot(t0)).normalized()
    bm = bmesh.new()
    rings = []
    for i in range(n):
        if i > 0:
            # transport nrm from tans[i-1] to tans[i]
            a, b = tans[i - 1], tans[i]
            axis = a.cross(b)
            if axis.length > 1e-8:
                ang = a.angle(b, 0.0)
                nrm = Matrix.Rotation(ang, 3, axis.normalized()) @ nrm
            nrm = (nrm - b * nrm.dot(b)).normalized()
        bi = tans[i].cross(nrm)
        ring = []
        for k in range(segs):
            ang = 2 * math.pi * k / segs
            d = nrm * math.cos(ang) + bi * math.sin(ang)
            ring.append(bm.verts.new(pts[i] + d * radii[i]))
        rings.append(ring)
    for A, B in zip(rings[:-1], rings[1:]):
        for k in range(segs):
            j = (k + 1) % segs
            bm.faces.new((A[k], A[j], B[j], B[k]))
    if pole0 is not None:
        p = bm.verts.new(pts[0] - tans[0] * pole0)
        for k in range(segs):
            bm.faces.new((rings[0][(k + 1) % segs], rings[0][k], p))
    elif cap0:
        bm.faces.new(list(reversed(rings[0])))
    if pole1 is not None:
        p = bm.verts.new(pts[-1] + tans[-1] * pole1)
        for k in range(segs):
            bm.faces.new((rings[-1][k], rings[-1][(k + 1) % segs], p))
    elif cap1:
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def prism(poly, depth, bevel=0.0, bsegs=1):
    """Extrude polygon [(x, y), ...] (counter-clockwise) along +Z from 0 to depth."""
    bm = bmesh.new()
    b = [bm.verts.new((x, y, 0.0)) for x, y in poly]
    t = [bm.verts.new((x, y, depth)) for x, y in poly]
    bm.faces.new(list(reversed(b)))
    bm.faces.new(t)
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((b[i], b[j], t[j], t[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=bm.edges[:] + bm.verts[:], offset=bevel, offset_type='OFFSET', segments=bsegs,
                        profile=0.5, affect='EDGES', clamp_overlap=True)
    return bm


def quad(p0, p1, p2, p3):
    bm = bmesh.new()
    vs = [bm.verts.new(vec(p)) for p in (p0, p1, p2, p3)]
    bm.faces.new(vs)
    return bm


def grid_plane(x0, x1, y0, y1, nx, ny, z=0.0):
    bm = bmesh.new()
    vs = [[bm.verts.new((x0 + (x1 - x0) * i / nx, y0 + (y1 - y0) * j / ny, z)) for j in range(ny + 1)]
          for i in range(nx + 1)]
    for i in range(nx):
        for j in range(ny):
            bm.faces.new((vs[i][j], vs[i + 1][j], vs[i + 1][j + 1], vs[i][j + 1]))
    return bm


# ----------------------------------------------------------------------------------------------------------------
# Convenience "add" helpers working directly on a Part
# ----------------------------------------------------------------------------------------------------------------
def add_capsule(P, p0, p1, r0, r1, slot, segs=12, cap_rings=3, smooth=True, mid=0, hint=None):
    p0, p1 = vec(p0), vec(p1)
    L = (p1 - p0).length
    P.add(capsule(L, r0, r1, segs, cap_rings, mid), slot, smooth, frame(p0, p1 - p0, hint))


def add_cyl(P, p0, p1, r0, r1=None, slot="Metal", segs=12, bevel=0.0, smooth=True, hint=None):
    p0, p1 = vec(p0), vec(p1)
    P.add(cylinder((p1 - p0).length, r0, r1, segs, bevel), slot, smooth, frame(p0, p1 - p0, hint))


def add_sphere(P, c, r, slot, segs=12, rings=8, smooth=True, rot=None):
    rr = (r, r, r) if not isinstance(r, (tuple, list)) else r
    m = T(*vec(c)) @ (rot if rot is not None else Matrix.Identity(4)) @ S(*rr)
    P.add(sphere(1.0, segs, rings), slot, smooth, m)


def add_box(P, c, size, slot, bevel=0.0, bsegs=1, rot=None, smooth=False):
    m = T(*vec(c)) @ (rot if rot is not None else Matrix.Identity(4))
    P.add(box(size[0], size[1], size[2], bevel, bsegs), slot, smooth, m)


def add_band(P, c, axis, r_in, r_out, h, slot="TaG", segs=16, smooth=True):
    P.add(band(r_in, r_out, h, segs), slot, smooth, frame(c, axis))


def add_torus(P, c, axis, R_, r, slot, segs=16, csegs=8, smooth=True, scale=None):
    m = frame(c, axis)
    if scale is not None:
        m = m @ S(*scale)
    P.add(torus(R_, r, segs, csegs), slot, smooth, m)


def add_tube(P, pts, radii, slot, segs=6, smooth=True, cap0=True, cap1=True, pole0=None, pole1=None):
    P.add(tube(pts, radii, segs, cap0, cap1, pole0, pole1), slot, smooth)


def add_loft(P, sections, slot, k=16, smooth=True, matrix=None, **kw):
    P.add(loft(sections, k, **kw), slot, smooth, matrix)


def add_prism(P, poly, depth, slot, matrix, bevel=0.0, bsegs=1, smooth=False):
    P.add(prism(poly, depth, bevel, bsegs), slot, smooth, matrix)


def mirror_y(m_points):
    return [(x, -y, z) for (x, y, z) in m_points]


def lerp(a, b, t):
    return a + (b - a) * t


def bezier(p0, p1, p2, p3, n):
    p0, p1, p2, p3 = vec(p0), vec(p1), vec(p2), vec(p3)
    out = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        out.append(p0 * (u * u * u) + p1 * (3 * u * u * t) + p2 * (3 * u * t * t) + p3 * (t * t * t))
    return out
