"""
build_assets.py - build every mesh procedurally and export one FBX per UStaticMesh + build/fbx/assets.json.

Run (from anywhere):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup -P C:/TaG/tools/blender/build_assets.py -- --out C:/TaG/tools/build/fbx

FBX settings (verified round trip, see tools/import/verify.py): scene unit scale 0.01 (1 BU = 1 cm), axis -Z fwd /
Y up with bake_space_transform -> UE gets (x, -y, z) in cm, sockets (empties named SOCKET_<name> parented to the mesh)
come through with zero rotation and unit scale.  UCX_<mesh>_NN boxes become simple collision.
"""
import os
import sys
import json
import time

sys.dont_write_bytecode = True   # keep tools/blender free of __pycache__
HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402
import scene_util  # noqa: E402
import palette  # noqa: E402
import heroes  # noqa: E402
import ball  # noqa: E402
import court  # noqa: E402
import city  # noqa: E402

TAGTYPES = os.path.normpath(os.path.join(HERE, "..", "..", "game", "Source", "BasketTaG", "Core", "TaGTypes.h"))


class Asset:
    def __init__(self, part, folder, owner, category, **kw):
        self.part = part
        self.name = part.name
        self.folder = folder            # e.g. /Game/Art/Heroes/Rei
        self.owner = owner              # palette owner -> MI_<Owner>_<Slot>
        self.category = category
        self.extra = kw

    @property
    def package(self):
        return "%s/%s" % (self.folder, self.name)


def ue(v):
    return [round(v[0], 3), round(-v[1], 3), round(v[2], 3)]


def build_everything(log=print):
    t0 = time.time()
    court.sync_tagtypes(TAGTYPES)
    assets = []
    rigs = heroes.build_all()
    for hid in heroes.HERO_IDS:
        rig = rigs[hid]
        origins = rig.rest_origins()
        for pn in heroes.PARTS:
            assets.append(Asset(rig.parts[pn], "/Game/Art/Heroes/%s" % hid, hid, "hero", hero=hid, part_name=pn,
                                rest_origin=origins[pn]))
    b = ball.build_ball()
    b.finalize()
    assets.append(Asset(b, "/Game/Art/Ball", "Ball", "ball"))
    for fn, owner in ((court.build_hoop, "Hoop"), (court.build_rooftop, "Rooftop"),
                      (court.build_props, "RooftopProps"), (court.build_furnace, "RooftopProps_Furnace")):
        P = fn()
        if owner in ("RooftopProps", "RooftopProps_Furnace"):
            P.keep_out_violations = court.keep_out_violations(P)
        P.uv_scale = 0.01 if owner == "Hoop" else 0.001
        P.finalize()
        assets.append(Asset(P, "/Game/Art/Court", owner, "court"))
    parts, footprints = city.build_city()
    for side in ("A", "B"):
        for P in parts[side]:
            P.uv_scale = 0.001
            P.finalize()
            assets.append(Asset(P, "/Game/Art/City", "City" + side, "city", side=side, meta=P.meta))
    sea = city.build_sea(footprints)
    sea.uv_scale = 0.0001
    sea.finalize()
    assets.append(Asset(sea, "/Game/Art/City", "Sea", "sea", meta=sea.meta))
    log("built %d meshes in %.1fs" % (len(assets), time.time() - t0))
    return rigs, assets


def export_fbx(asset, path):
    P = asset.part
    sc = bpy.context.scene
    objs = []
    me = scene_util.mesh_from_part(P, P.name, [scene_util.export_material(s) for s in P.slots])
    ob = bpy.data.objects.new(P.name, me)
    sc.collection.objects.link(ob)
    objs.append(ob)
    for sn, pos in P.sockets.items():
        e = bpy.data.objects.new("SOCKET_" + sn, None)
        sc.collection.objects.link(e)
        e.parent = ob
        e.location = pos
        e.empty_display_size = 5.0
        objs.append(e)
    from tagmesh import box as tbox, T as tT
    for i, (c, size) in enumerate(P.collision):
        cm = bpy.data.meshes.new("UCX_%s_%02d" % (P.name, i))
        bm = tbox(*size)
        for v in bm.verts:
            v.co = v.co + Vector(c)
        bm.to_mesh(cm)
        bm.free()
        co = bpy.data.objects.new("UCX_%s_%02d" % (P.name, i), cm)
        sc.collection.objects.link(co)
        objs.append(co)
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = ob
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={'MESH', 'EMPTY'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', global_scale=1.0,
        axis_forward='-Z', axis_up='Y', bake_space_transform=True,
        mesh_smooth_type='FACE', use_mesh_modifiers=False, use_triangles=True,
        colors_type='LINEAR' if P.col is not None else 'NONE',
        add_leaf_bones=False, use_custom_props=False, path_mode='STRIP', embed_textures=False)
    for o in objs:
        data = o.data
        bpy.data.objects.remove(o, do_unlink=True)
        if data is not None:
            bpy.data.meshes.remove(data)


def describe(asset, fbx_rel):
    P = asset.part
    lo, hi = P.bounds()
    d = {
        "name": P.name,
        "package": asset.package,
        "folder": asset.folder,
        "fbx": fbx_rel,
        "owner": asset.owner,
        "category": asset.category,
        "slots": P.slots,
        "materials": {s: dict(palette.params(asset.owner, s), instance="%s/MI_%s_%s" % (asset.folder, asset.owner, s))
                      for s in P.slots},
        "sockets": {k: ue(v) for k, v in P.sockets.items()},
        "triangles": P.tri_count(),
        "bounds_min": [round(lo.x, 2), round(-hi.y, 2), round(lo.z, 2)],
        "bounds_max": [round(hi.x, 2), round(-lo.y, 2), round(hi.z, 2)],
        "vertex_colors": P.col is not None,
        "collision_boxes": [{"center": ue(c), "size": list(sz)} for (c, sz) in P.collision],
    }
    for k, v in asset.extra.items():
        if k == "rest_origin":
            d[k] = ue(v)
        else:
            d[k] = v
    if hasattr(P, "keep_out_violations"):
        d["keep_out_violations"] = P.keep_out_violations
    return d


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.normpath(os.path.join(HERE, "..", "build", "fbx"))
    only = None
    for i, a in enumerate(argv):
        if a == "--out":
            out = argv[i + 1]
        if a == "--only":
            only = argv[i + 1].split(",")
    scene_util.reset_scene()
    rigs, assets = build_everything()
    entries = []
    for a in assets:
        rel = "%s/%s.fbx" % (a.folder.replace("/Game/", ""), a.name)
        if only is None or a.name in only or a.category in only:
            export_fbx(a, os.path.join(out, rel))
        entries.append(describe(a, rel))
    hero_info = {}
    for hid, rig in rigs.items():
        m = rig.measures()
        tgt = heroes.TARGETS[hid]
        hero_info[hid] = {
            "class": tgt[2], "target_eye_height": tgt[0], "target_width": tgt[1],
            "eye_height": round(m["eye_height"], 2), "width": round(m["width"], 2),
            "height": round(m["height"], 2), "triangles": m["tris"],
            "pelvis_height": round(rig.sk["pelvis"].z, 2),
            "rest_origins": {pn: ue(o) for pn, o in rig.rest_origins().items()},
        }
    doc = {
        "generated": time.strftime("%Y-%m-%d %H:%M:%S"),
        "units": "cm, UE axes (+X forward, +Y right, +Z up)",
        "tagtypes": court.TAG,
        "assets": entries,
        "heroes": hero_info,
        "city": {s: [e["package"] for e in entries if e.get("side") == s] for s in ("A", "B")},
    }
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "assets.json"), "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1)
    tot = {}
    for e in entries:
        tot[e["category"]] = tot.get(e["category"], 0) + e["triangles"]
    print("EXPORTED %d meshes to %s" % (len(entries), out))
    print("TRIANGLES per category: %s" % tot)
    for hid, h in hero_info.items():
        print("HERO %-9s eye %.1f width %.1f tris %d" % (hid, h["eye_height"], h["width"], h["triangles"]))
    for e in entries:
        if "keep_out_violations" in e:
            print("KEEP-OUT %s: %d vertices inside the play box" % (e["name"], e["keep_out_violations"]))


if __name__ == "__main__":
    main()
