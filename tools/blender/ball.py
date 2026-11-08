"""
ball.py - SM_Ball: 24 cm core ball, origin at the centre.

Base sphere + raised seam ridges (slot Dark) following the classic 8-panel layout (two perpendicular great circles
and two curved seams = small circles around the +-X axis) + thin raised "core lines" (slot Glow) that C++ can light.
The outermost seam ridges touch r = 12.0 cm exactly, so the bounding diameter is 24 cm.
"""
import math
import bmesh
from mathutils import Vector
from tagmesh import Part, sphere

R_BALL = 12.0


def _circle(axis_pt, u, v, alpha, n):
    """Small circle on the unit sphere: centre direction axis_pt, angular radius alpha, basis u/v."""
    c0 = Vector(axis_pt).normalized()
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        p = c0 * math.cos(alpha) + (Vector(u) * math.cos(t) + Vector(v) * math.sin(t)) * math.sin(alpha)
        pts.append(p.normalized())
    return pts


def _ridge(P, pts, r_in, r_top, w_base, w_top, slot, flat=False):
    bm = bmesh.new()
    n = len(pts)
    rings = []
    for i in range(n):
        c = pts[i]
        t = (pts[(i + 1) % n] - pts[i - 1]).normalized()
        b = c.cross(t).normalized()
        if flat:
            ring = [c * r_top - b * (w_top / 2), c * r_top + b * (w_top / 2)]
        else:
            ring = [c * r_in - b * (w_base / 2), c * r_top - b * (w_top / 2), c * r_top + b * (w_top / 2),
                    c * r_in + b * (w_base / 2)]
        rings.append([bm.verts.new(p) for p in ring])
    for i in range(n):
        A, B = rings[i], rings[(i + 1) % n]
        if flat:
            bm.faces.new((A[0], B[0], B[1], A[1]))
        else:
            bm.faces.new((A[0], B[0], B[1], A[1]))
            bm.faces.new((A[1], B[1], B[2], A[2]))
            bm.faces.new((A[2], B[2], B[3], A[3]))
    P.add(bm, slot, True)


def build_ball():
    P = Part("SM_Ball")
    R = 11.85
    P.add(sphere(R, 32, 18), "Body", True)
    X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)
    # great circle A (plane y = 0) and B (plane x = 0) at slightly different heights (no coplanar overlap)
    _ridge(P, _circle(Y, Z, X, math.pi / 2, 72), R - 0.15, R_BALL, 0.75, 0.55, "Dark")
    _ridge(P, _circle(X, Y, Z, math.pi / 2, 72), R - 0.15, R_BALL - 0.02, 0.75, 0.55, "Dark")
    # curved seams: small circles around +X and -X
    for ax in (X, (-1, 0, 0)):
        _ridge(P, _circle(ax, Y, Z, math.radians(55.0), 60), R - 0.15, R_BALL - 0.04, 0.75, 0.55, "Dark")
    # glowing core lines (flat raised ribbons)
    for ax in (X, (-1, 0, 0)):
        _ridge(P, _circle(ax, Y, Z, math.radians(30.0), 40), R, R + 0.05, 0.3, 0.3, "Glow", flat=True)
        _ridge(P, _circle(ax, Y, Z, math.radians(76.0), 64), R, R + 0.05, 0.3, 0.3, "Glow", flat=True)
    return P
