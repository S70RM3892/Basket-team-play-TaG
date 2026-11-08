"""
sdfbake.py - turn sdf.Group shells into tagmesh Parts (Blender side).

  bake_part(P, groups, budget)   mesh every group, decimate to share `budget` triangles, merge with its slot
  add_outline(P, thickness)      inverted hull of everything in the Part, slot "Outline"
  scale_y(parts, f)              uniform Y scale of geometry and sockets (used to hit a width target exactly)
"""
import math
import bmesh
import bpy
import numpy as np
from mathutils import Vector

import sdf

_scene_ready = False


def _scene():
    sc = bpy.context.scene
    return sc


def bm_from_arrays(V, T):
    bm = bmesh.new()
    vs = [bm.verts.new((float(x), float(y), float(z))) for x, y, z in V]
    bm.verts.ensure_lookup_table()
    for a, b, c in T:
        try:
            f = bm.faces.new((vs[a], vs[b], vs[c]))
            f.smooth = True
        except ValueError:
            pass
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    return bm


def decimate(bm, target_tris):
    """Quadric collapse (Blender Decimate modifier) to about target_tris triangles. Returns a new bmesh."""
    cur = len(bm.faces)
    if cur <= target_tris or cur == 0:
        return bm
    me = bpy.data.meshes.new("_dec")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("_dec", me)
    sc = _scene()
    sc.collection.objects.link(ob)
    mod = ob.modifiers.new("dec", 'DECIMATE')
    mod.decimate_type = 'COLLAPSE'
    mod.ratio = max(0.002, min(1.0, target_tris / float(cur)))
    mod.use_collapse_triangulate = True
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    me2 = ev.to_mesh()
    out = bmesh.new()
    out.from_mesh(me2)
    ev.to_mesh_clear()
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.meshes.remove(me)
    for f in out.faces:
        f.smooth = True
    return out


def _area(V, T):
    if len(T) == 0:
        return 0.0
    a, b, c = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
    return float(0.5 * np.linalg.norm(np.cross(b - a, c - a), axis=1).sum())


def auto_voxel(group):
    lo, hi = group.bounds()
    ext = np.sort(hi - lo)
    # thin features need fine voxels; big shells can be coarse (decimation removes the excess anyway)
    return float(np.clip(ext[0] / 9.0, 0.22, 1.1))


def bake_part(P, groups, budget, min_tris=24, log=None):
    """Mesh all groups, give each a share of `budget` (area^0.75, at least min_tris), decimate, merge."""
    meshed = []
    for g in groups:
        if g is None or g.empty():
            continue
        h = g.voxel or auto_voxel(g)
        V, T = sdf.mesh_group(g, h)
        if len(T) == 0:
            if log:
                log("empty group %s/%s" % (P.name, g.name or g.slot))
            continue
        meshed.append((g, V, T, _area(V, T)))
    if not meshed:
        return
    w = [max(m[3], 1.0) ** 0.75 for m in meshed]
    wsum = sum(w)
    free = max(budget - min_tris * len(meshed), budget * 0.3)
    for (g, V, T, area), wi in zip(meshed, w):
        share = min_tris + free * wi / wsum
        tgt = int(g.budget if g.budget else share)
        bm = bm_from_arrays(V, T)
        bm = decimate(bm, tgt)
        P.add(bm, g.slot, smooth=True)


def add_outline(P, thickness, ratio=1.0):
    """Inverted hull: every shell pushed out along its vertex normals and flipped, slot 'Outline'."""
    src = P.bm.copy()
    src.normal_update()
    for v in src.verts:
        n = v.normal
        if n.length < 1e-6:
            continue
        v.co = v.co + n * thickness
    for f in src.faces:
        f.smooth = True
    bmesh.ops.reverse_faces(src, faces=src.faces[:])
    if ratio < 1.0:
        src = decimate(src, int(len(src.faces) * ratio))
    P.add(src, "Outline", smooth=True)


def scale_y(parts, f):
    for P in parts:
        for v in P.bm.verts:
            v.co.y *= f
        P.sockets = {k: Vector((p.x, p.y * f, p.z)) for k, p in P.sockets.items()}
