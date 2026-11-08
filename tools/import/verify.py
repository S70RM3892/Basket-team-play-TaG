"""
verify.py - runs INSIDE Unreal after import_all.py (same helper project):

  "C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/TaG/tools/assetproj/TaGAssets.uproject"
      -run=pythonscript -script="C:/TaG/tools/import/verify.py" -unattended -nop4 -nosplash -nullrhi

With -nullrhi no shader is compiled. To also check that every toon material compiles, replace -nullrhi with
  -AllowCommandletRendering -dx12               (PC: D3D12 SM6, the helper project targets SM6 like the game)
  -AllowCommandletRendering -FeatureLevelES31   (the mobile ES3.1 path that Android uses)
(the first run per platform compiles the engine's own shaders and takes a few minutes).

Loads every asset, reads sockets / slots / bounds / triangle counts / vertex data back from the imported UStaticMesh
and SoundWave assets, checks them against the contract and TaGTypes.h, and writes
  tools/art/verify_report.txt   (human readable, PASS/FAIL per check)
  tools/art/manifest.json       (everything C++ needs, all numbers read back from UE, cm, UE axes)
"""
import json
import math
import os
import re
import time
import unreal

TOOLS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
REPO = os.path.normpath(os.path.join(TOOLS, ".."))
FBX_JSON = os.path.join(TOOLS, "build", "fbx", "assets.json")
SFX_JSON = os.path.join(TOOLS, "build", "wav", "sfx.json")
ICONS_JSON = os.path.join(TOOLS, "build", "icons", "icons.json")
TAGTYPES = os.path.join(REPO, "game", "Source", "BasketTaG", "Core", "TaGTypes.h")
REPORT = os.path.join(TOOLS, "art", "verify_report.txt")
MANIFEST = os.path.join(TOOLS, "art", "manifest.json")

SLOTS = ["Body", "Accent", "Extra", "Skin", "Hair", "Dark", "Metal", "White", "Outline", "Team", "TaG", "Glow", "Jelly",
         "Window", "Net", "Board",
         "Court", "Paint", "Apron", "Water", "Sky"]
HERO_PARTS = ["Pelvis", "Torso", "Head", "UpperArm_L", "UpperArm_R", "LowerArm_L", "LowerArm_R", "Thigh_L", "Thigh_R",
              "Shin_L", "Shin_R", "Foot_L", "Foot_R"]
PART_SOCKETS = {"Pelvis": ["spine", "hip_l", "hip_r"], "Torso": ["neck", "shoulder_l", "shoulder_r"], "Head": ["eyes"],
                "UpperArm_L": ["elbow"], "UpperArm_R": ["elbow"], "LowerArm_L": ["hand"], "LowerArm_R": ["hand"],
                "Thigh_L": ["knee"], "Thigh_R": ["knee"], "Shin_L": ["ankle"], "Shin_R": ["ankle"],
                "Foot_L": ["sole", "toe"], "Foot_R": ["sole", "toe"]}
HERO_TRI_BUDGET = 9000
SFX_NAMES = ["Bounce", "Swish", "Rim", "Board", "ShotRelease", "Pass", "Catch", "Tackle", "Block", "Steal", "ReadWin",
             "ReadLose", "Whistle", "Buzzer", "Score", "Drive", "Dodge", "ContestHum", "TackleCue", "Step", "Down",
             "Revive", "ArtCue", "UIMove", "UIConfirm", "CityPowerUp", "AmbientLoop"]
RUNTIME_SLOTS = ["Team", "TaG", "Glow", "Window", "Outline"]
SKY_RADIUS = 160000.0
SKY_CENTRE = (550.0, 0.0, 0.0)
# toon material contract (import_all.py): name -> (blend mode, vector params, {scalar: default})
MATERIALS = {
    "M_TaGBase": ("BLEND_OPAQUE", ["Color", "Emissive", "RimColor"],
                  {"EmissiveStrength": 0.0, "Metallic": 0.0, "RimStrength": 0.0, "Roughness": 0.6}),
    "M_TaGTranslucent": ("BLEND_TRANSLUCENT", ["Color", "Emissive"],
                         {"EmissiveStrength": 0.0, "Metallic": 0.0, "Opacity": 0.5, "Roughness": 0.6}),
    "M_TaGWater": ("BLEND_OPAQUE", ["Color"], {"Roughness": 0.06}),
    "M_TaGOutline": ("BLEND_OPAQUE", ["Color"], {}),
    "M_TaGSky": ("BLEND_OPAQUE", [], {}),
    "M_TaGParticle": ("BLEND_ADDITIVE", [], {"Falloff": 1.6, "Intensity": 2.0}),
    "M_TaGParticleAlpha": ("BLEND_TRANSLUCENT", [], {"Falloff": 0.8, "Intensity": 1.0}),
}
MPC_VECTORS = ["Ambient", "RimColor", "ShadeColor", "SkyGround", "SkyHorizon", "SkyZenith", "SunColor", "SunDir"]
MPC_SCALARS = ["BandOffset", "BandSharpness", "RimPower", "RimStrength"]
# per-instance edge light of M_TaGBase (import_all.py HERO_RIM_STRENGTH / HERO_RIM_SKIP)
HERO_RIM_STRENGTH = 0.35
HERO_RIM_SKIP = ("Outline", "TaG", "Glow", "Jelly")

lines = []
stats = {"pass": 0, "fail": 0, "warn": 0}


def out(s=""):
    lines.append(s)


def check(cond, msg, warn_only=False):
    if cond:
        stats["pass"] += 1
        out("  [PASS] " + msg)
    elif warn_only:
        stats["warn"] += 1
        out("  [WARN] " + msg)
    else:
        stats["fail"] += 1
        out("  [FAIL] " + msg)
    return cond


def v3(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]


def add(a, b):
    return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]


def fmt(v):
    return "(%.2f, %.2f, %.2f)" % tuple(v)


def ename(v):
    return getattr(v, "name", str(v))


def mi_params(mi):
    mel = unreal.MaterialEditingLibrary
    out = {"parent": mi.get_editor_property("parent").get_path_name().split(".")[0]}
    base = out["parent"].rsplit("/", 1)[-1]
    if base in ("M_TaGSky",):
        return out
    for n in (("Color",) if base in ("M_TaGWater", "M_TaGOutline") else ("Color", "Emissive")):
        try:
            c = mel.get_material_instance_vector_parameter_value(mi, n)
            out[n] = [round(c.r, 4), round(c.g, 4), round(c.b, 4)]
        except Exception:
            pass
    names = (["Roughness"] if base == "M_TaGWater" else [] if base == "M_TaGOutline"
             else ["Roughness", "Metallic", "EmissiveStrength"])
    if out["parent"].endswith("M_TaGTranslucent"):
        names.append("Opacity")
    if base == "M_TaGBase":
        names.append("RimStrength")
    for n in names:
        try:
            out[n] = round(mel.get_material_instance_scalar_parameter_value(mi, n), 4)
        except Exception:
            pass
    return out


def tex_size(path, tx):
    """Texture size from the asset registry tag "Dimensions". With an RHI (-AllowCommandletRendering) the live texture
    may still be compiling and report the 32x32 placeholder."""
    try:
        dims = str(unreal.EditorAssetLibrary.find_asset_data(path).get_tag_value("Dimensions"))
        w, h = dims.lower().split("x")
        return int(w), int(h)
    except Exception:
        return tx.blueprint_get_size_x(), tx.blueprint_get_size_y()


def compile_mode():
    """Shaders are compiled only when the commandlet may render (-AllowCommandletRendering and no -nullrhi)."""
    try:
        cl = unreal.SystemLibrary.get_command_line().lower()
    except Exception:
        return None
    if "-allowcommandletrendering" not in cl or "-nullrhi" in cl:
        return None
    if "-featureleveles31" in cl:
        return "the mobile ES3.1 feature level (-FeatureLevelES31)"
    return "PC %s" % ("D3D12 SM6 (-dx12)" if "-dx12" in cl else "(default RHI)")


def tagtypes():
    vals = {}
    txt = open(TAGTYPES, encoding="utf-8").read()
    for m in re.finditer(r"constexpr\s+float\s+(\w+)\s*=\s*([-0-9.]+)f?", txt):
        vals[m.group(1)] = float(m.group(2))
    return vals


def sockets_of(sm, names):
    res = {}
    for n in names:
        s = sm.find_socket(n)
        if s is not None:
            res[n] = s
    return res


def slot_vertex_sets(sm):
    """{slot name: [positions]} read from the LOD0 mesh description (polygon group i <-> material slot i)."""
    d = sm.get_static_mesh_description(0)
    names = [str(m.material_slot_name) for m in sm.static_materials]
    ngroups = d.get_polygon_group_count()
    res = {n: set() for n in names}
    for p in range(d.get_polygon_count()):
        pid = unreal.PolygonID(p)
        g = d.get_polygon_polygon_group(pid)
        gi = g.get_editor_property("id_value") if hasattr(g, "get_editor_property") else int(g)
        for vid in d.get_polygon_vertices(pid):
            res[names[gi]].add(vid.get_editor_property("id_value"))
    pos = {}
    for n, ids in res.items():
        pos[n] = [d.get_vertex_position(unreal.VertexID(i)) for i in ids]
    return pos, ngroups, len(names)


def bbox(pts):
    return ([min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)],
            [max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)])


def mesh_info(sm):
    bb = sm.get_bounding_box()
    return {"path": sm.get_path_name().split(".")[0], "triangles": sm.get_num_triangles(0),
            "vertices": sm.get_num_vertices(0),
            "slots": [str(m.material_slot_name) for m in sm.static_materials],
            "material_instances": [m.material_interface.get_path_name().split(".")[0] if m.material_interface else None
                                   for m in sm.static_materials],
            "bounds_min": v3(bb.min), "bounds_max": v3(bb.max)}


def main():
    t0 = time.time()
    doc = json.load(open(FBX_JSON, encoding="utf-8"))
    sfx_src = json.load(open(SFX_JSON, encoding="utf-8")) if os.path.exists(SFX_JSON) else {}
    tt = tagtypes()
    entries = {e["name"]: e for e in doc["assets"]}
    manifest = {"generated": time.strftime("%Y-%m-%d %H:%M:%S"),
                "engine": str(unreal.SystemLibrary.get_engine_version()),
                "units": "cm; UE axes: +X forward, +Y right (left = -Y), +Z up",
                "notes": [], "materials": {}, "material_instances": {}, "heroes": {}, "ball": {}, "court": {},
                "city": {"A": [], "B": [], "clusters": {}}, "sea": {}, "sfx": {}, "all_assets": []}
    out("Basket Team-play TaG - asset verification report")
    out("generated %s by tools/import/verify.py inside %s" % (manifest["generated"], manifest["engine"]))
    out("source: tools/build/fbx/assets.json (expected values), %s" % os.path.relpath(TAGTYPES, REPO))
    out("")

    # ------------------------------------------------------------------------------------------- base materials
    out("== Toon materials (/Game/Art/Materials) ==")
    mel = unreal.MaterialEditingLibrary
    cmode = compile_mode()
    out("  shader compile check: %s" % (cmode or "skipped (with -nullrhi no shader is compiled: run with "
                                                 "-AllowCommandletRendering instead to compile the materials)"))
    for name, (blend, want_v, want_s) in MATERIALS.items():
        path = "/Game/Art/Materials/" + name
        m = unreal.load_asset(path)
        if not check(m is not None, "%s loads" % path):
            continue
        sc = sorted(str(x) for x in mel.get_scalar_parameter_names(m))
        ve = sorted(str(x) for x in mel.get_vector_parameter_names(m))
        info = {"path": path, "blend_mode": ename(m.get_editor_property("blend_mode")),
                "shading_model": ename(m.get_editor_property("shading_model")),
                "scalar_parameters": {n: round(mel.get_material_default_scalar_parameter_value(m, n), 4) for n in sc},
                "vector_parameters": {n: [round(c, 4) for c in (lambda c: (c.r, c.g, c.b))(
                    mel.get_material_default_vector_parameter_value(m, n))] for n in ve}}
        check(ve == sorted(want_v) and sc == sorted(want_s),
              "%s parameters vectors %s scalars %s (got %s %s)" % (name, sorted(want_v), sorted(want_s), ve, sc))
        d = info["scalar_parameters"]
        check(all(abs(d.get(k, -99) - v) < 1e-3 for k, v in want_s.items()),
              "%s scalar defaults %s (got %s)" % (name, want_s, d))
        check(info["blend_mode"] == blend and info["shading_model"] == "MSM_UNLIT",
              "%s %s, unlit (toon lighting from MPC_TaG) (got %s %s)" % (name, blend, info["blend_mode"],
                                                                        info["shading_model"]))
        if cmode:
            # waits for the shader map; a material that failed to compile (e.g. float3 * float4) has no instructions
            st = mel.get_statistics(m)
            ps, vs = st.num_pixel_shader_instructions, st.num_vertex_shader_instructions
            check(ps > 0 and vs > 0, "%s compiles for %s: PS %d / VS %d instructions" % (name, cmode, ps, vs))
        manifest["materials"][name] = info
        manifest["all_assets"].append(path)
    mpc = unreal.load_asset("/Game/Art/Materials/MPC_TaG")
    if check(mpc is not None, "/Game/Art/Materials/MPC_TaG loads"):
        vp = {str(p.get_editor_property("parameter_name")): [round(c, 4) for c in (lambda c: (c.r, c.g, c.b))(
            p.get_editor_property("default_value"))] for p in mpc.get_editor_property("vector_parameters")}
        sp = {str(p.get_editor_property("parameter_name")): round(p.get_editor_property("default_value"), 4)
              for p in mpc.get_editor_property("scalar_parameters")}
        check(sorted(vp) == MPC_VECTORS and sorted(sp) == MPC_SCALARS,
              "MPC_TaG vectors %s scalars %s" % (sorted(vp), sorted(sp)))
        manifest["materials"]["MPC_TaG"] = {"path": "/Game/Art/Materials/MPC_TaG", "vector_parameters": vp,
                                            "scalar_parameters": sp}
        manifest["all_assets"].append("/Game/Art/Materials/MPC_TaG")
    out("")

    # ------------------------------------------------------------------------------------------- every mesh
    out("== Static meshes: load, slots, material instances ==")
    meshes = {}
    mi_seen = set()
    for name, e in entries.items():
        sm = unreal.load_asset(e["package"])
        if not check(sm is not None and isinstance(sm, unreal.StaticMesh), "%s loads" % e["package"]):
            continue
        meshes[name] = sm
        info = mesh_info(sm)
        slots = info["slots"]
        ok_names = all(s in SLOTS for s in slots) and sorted(slots) == sorted(e["slots"])
        ok_mis = True
        for s, mi in zip(slots, info["material_instances"]):
            if s not in e["materials"]:
                ok_mis = False
                continue
            want = e["materials"][s]["instance"]
            if mi != want:
                ok_mis = False
            mi_seen.add(want)
        check(ok_names and ok_mis, "%s slots %s each with MI_%s_<Slot>" % (name, slots, e["owner"]))
        manifest["all_assets"].append(info["path"])
    for p in sorted(mi_seen):
        mi = unreal.load_asset(p)
        if mi is not None:
            manifest["material_instances"][p] = mi_params(mi)
            manifest["all_assets"].append(p)
    check(len(manifest["material_instances"]) == len(mi_seen), "all %d material instances load" % len(mi_seen))
    hero_rim = {e["materials"][s]["instance"] for e in entries.values() if e["category"] == "hero"
                for s in e["slots"] if s not in HERO_RIM_SKIP and e["materials"][s]["base"] == "M_TaGBase"}
    rim_bad = [p for p, prm in manifest["material_instances"].items() if "RimStrength" in prm and
               abs(prm["RimStrength"] - (HERO_RIM_STRENGTH if p in hero_rim else 0.0)) > 1e-3]
    check(not rim_bad and hero_rim <= set(manifest["material_instances"]),
          "edge light RimStrength %.2f on the %d hero instances (not %s), 0 on every other M_TaGBase instance%s" %
          (HERO_RIM_STRENGTH, len(hero_rim), "/".join(HERO_RIM_SKIP), (": wrong on %s" % rim_bad[:5]) if rim_bad else ""))
    out("")

    # ------------------------------------------------------------------------------------------- heroes
    out("== Heroes (rest pose assembled from the imported sockets) ==")
    for hid, hexp in doc["heroes"].items():
        out("-- %s (%s) --" % (hid, hexp["class"]))
        parts = {}
        socks = {}
        for pn in HERO_PARTS:
            sm = meshes.get("SM_%s_%s" % (hid, pn))
            if sm is None:
                check(False, "SM_%s_%s missing" % (hid, pn))
                continue
            parts[pn] = sm
            ss = sockets_of(sm, PART_SOCKETS[pn])
            okrot = all(abs(s.relative_rotation.pitch) + abs(s.relative_rotation.yaw) + abs(s.relative_rotation.roll)
                        < 1e-3 and abs(s.relative_scale.x - 1) + abs(s.relative_scale.y - 1) +
                        abs(s.relative_scale.z - 1) < 1e-4 for s in ss.values())
            check(len(ss) == len(PART_SOCKETS[pn]) and okrot,
                  "SM_%s_%s sockets %s present (rotation 0, scale 1): %s" %
                  (hid, pn, PART_SOCKETS[pn], ", ".join("%s%s" % (k, fmt(v3(s.relative_location))) for k, s in ss.items())))
            socks[pn] = {k: v3(s.relative_location) for k, s in ss.items()}
        if len(parts) != len(HERO_PARTS):
            continue
        S = socks
        # rest chain, pelvis at the origin first
        o = {"Pelvis": [0.0, 0.0, 0.0]}
        o["Torso"] = add(o["Pelvis"], S["Pelvis"]["spine"])
        o["Head"] = add(o["Torso"], S["Torso"]["neck"])
        for sd, s in (("L", "l"), ("R", "r")):
            o["UpperArm_" + sd] = add(o["Torso"], S["Torso"]["shoulder_" + s])
            o["LowerArm_" + sd] = add(o["UpperArm_" + sd], S["UpperArm_" + sd]["elbow"])
            o["Thigh_" + sd] = add(o["Pelvis"], S["Pelvis"]["hip_" + s])
            o["Shin_" + sd] = add(o["Thigh_" + sd], S["Thigh_" + sd]["knee"])
            o["Foot_" + sd] = add(o["Shin_" + sd], S["Shin_" + sd]["ankle"])
        sole_l = add(o["Foot_L"], S["Foot_L"]["sole"])
        sole_r = add(o["Foot_R"], S["Foot_R"]["sole"])
        floor = 0.5 * (sole_l[2] + sole_r[2])
        o = {k: [v[0], v[1], v[2] - floor] for k, v in o.items()}       # put the soles on z = 0
        eyes = add(o["Head"], S["Head"]["eyes"])
        hands = {sd: add(o["LowerArm_" + sd], S["LowerArm_" + sd]["hand"]) for sd in ("L", "R")}
        lo = [1e9, 1e9, 1e9]
        hi = [-1e9, -1e9, -1e9]
        tris = 0
        for pn, sm in parts.items():
            bb = sm.get_bounding_box()
            for i, (a, b) in enumerate(((bb.min.x, bb.max.x), (bb.min.y, bb.max.y), (bb.min.z, bb.max.z))):
                lo[i] = min(lo[i], a + o[pn][i])
                hi[i] = max(hi[i], b + o[pn][i])
            tris += sm.get_num_triangles(0)
        eye_h = eyes[2]
        width = hi[1] - lo[1]
        check(abs(sole_l[2] - sole_r[2]) < 0.01 and abs(sole_l[2] - floor) < 0.01,
              "soles level: left z %.2f right z %.2f (floor)" % (sole_l[2], sole_r[2]))
        check(abs(eye_h - hexp["target_eye_height"]) <= 0.03 * hexp["target_eye_height"],
              "floor-to-eyes %.2f cm (target %.0f +-3%%)" % (eye_h, hexp["target_eye_height"]))
        check(abs(width - hexp["target_width"]) <= 0.03 * hexp["target_width"],
              "overall width %.2f cm (target %.0f +-3%%)" % (width, hexp["target_width"]))
        check(tris <= HERO_TRI_BUDGET, "triangles %d <= %d" % (tris, HERO_TRI_BUDGET))
        check(S["Pelvis"]["hip_l"][1] < 0 and S["Pelvis"]["hip_r"][1] > 0 and S["Torso"]["shoulder_l"][1] < 0 and
              S["Torso"]["shoulder_r"][1] > 0,
              "left sockets negative Y: hip_l.y %.2f shoulder_l.y %.2f (right: %.2f, %.2f)" %
              (S["Pelvis"]["hip_l"][1], S["Torso"]["shoulder_l"][1], S["Pelvis"]["hip_r"][1], S["Torso"]["shoulder_r"][1]))
        for sd in ("L", "R"):
            check(S["UpperArm_" + sd]["elbow"][2] < 0 and S["LowerArm_" + sd]["hand"][2] < 0 and
                  hands[sd][2] < o["LowerArm_" + sd][2] < o["UpperArm_" + sd][2],
                  "%s arm hangs down: shoulder z %.1f > elbow z %.1f > hand z %.1f" %
                  (sd, o["UpperArm_" + sd][2], o["LowerArm_" + sd][2], hands[sd][2]))
            foot_min = parts["Foot_" + sd].get_bounding_box().min.z
            outline = 1.0   # the outline hull may reach a little below the sole
            check(-outline - 0.1 < foot_min - S["Foot_" + sd]["sole"][2] < 0.5,
                  "%s sole socket on the bottom of the foot: sole z %.2f, foot min z %.2f (rel. ankle)" %
                  (sd, S["Foot_" + sd]["sole"][2], foot_min))
            check(abs(S["Foot_" + sd]["toe"][2] - S["Foot_" + sd]["sole"][2]) < 0.01 and
                  S["Foot_" + sd]["toe"][0] > S["Foot_" + sd]["sole"][0],
                  "%s toe socket on the floor in front of the sole (toe x %.1f, sole x %.1f)" %
                  (sd, S["Foot_" + sd]["toe"][0], S["Foot_" + sd]["sole"][0]))
            check(S["Shin_" + sd]["ankle"][2] < 0, "%s ankle below knee (%.1f)" % (sd, S["Shin_" + sd]["ankle"][2]))
            check(S["Thigh_" + sd]["knee"][2] < 0, "%s knee below hip (%.1f)" % (sd, S["Thigh_" + sd]["knee"][2]))
        tag_ok = all("TaG" in mesh_info(parts[pn])["slots"] for pn in ("LowerArm_L", "LowerArm_R", "Shin_L", "Shin_R"))
        out_ok = all("Outline" in mesh_info(sm)["slots"] for sm in parts.values())
        check(tag_ok, "TaG slot on both wrists (LowerArm) and both ankles (Shin)")
        check(out_ok, "Outline slot on every part")
        # forward check: the eyes / toes point to +X
        check(S["Head"]["eyes"][0] > 0 and S["Foot_L"]["toe"][0] > 0, "faces +X (eyes x %.1f, toe x %.1f)" %
              (S["Head"]["eyes"][0], S["Foot_L"]["toe"][0]))
        hm = {"class": hexp["class"], "target_eye_height": hexp["target_eye_height"], "eye_height": round(eye_h, 2),
              "target_width": hexp["target_width"], "width": round(width, 2), "height": round(hi[2], 2),
              "depth": round(hi[0] - lo[0], 2), "triangles": tris, "pelvis_height": round(o["Pelvis"][2], 2),
              "eyes_in_rest_pose": [round(c, 2) for c in eyes],
              "hand_l_in_rest_pose": [round(c, 2) for c in hands["L"]],
              "hand_r_in_rest_pose": [round(c, 2) for c in hands["R"]],
              "palm_normal_rest": {"hand_l": [0, 1, 0], "hand_r": [0, -1, 0]},
              "parts": {}}
        for pn, sm in parts.items():
            mi = mesh_info(sm)
            hm["parts"][pn] = {"path": mi["path"], "sockets": socks[pn], "rest_pivot": [round(c, 2) for c in o[pn]],
                               "triangles": mi["triangles"], "slots": mi["slots"],
                               "material_instances": mi["material_instances"],
                               "bounds_min": mi["bounds_min"], "bounds_max": mi["bounds_max"]}
        manifest["heroes"][hid] = hm
        out("   eye height %.2f  width %.2f  height %.2f  triangles %d  pelvis height %.2f" %
            (eye_h, width, hi[2], tris, o["Pelvis"][2]))
    out("")
    out("== Portraits (/Game/Art/UI) ==")
    for hid in doc["heroes"]:
        path = "/Game/Art/UI/T_Portrait_" + hid
        tx = unreal.load_asset(path)
        ok = tx is not None and isinstance(tx, unreal.Texture2D)
        size = tex_size(path, tx) if ok else (0, 0)
        check(ok and size == (256, 256), "%s loads, %dx%d" % (path, size[0], size[1]))
        if ok and hid in manifest["heroes"]:
            manifest["heroes"][hid]["portrait"] = path
        if ok:
            manifest["all_assets"].append(path)
    out("")
    out("== HUD icons (/Game/Art/UI/Icons, tools/ui/make_icons.py) ==")
    manifest["ui"] = {"icons": {}, "icon_size": 128,
                      "icon_note": "white glyph on transparent (tint it in Slate); UserInterface2D, UI group, no mips"}
    if check(os.path.exists(ICONS_JSON), "tools/build/icons/icons.json exists"):
        idoc = json.load(open(ICONS_JSON, encoding="utf-8"))
        for ic in idoc["icons"]:
            tx = unreal.load_asset(ic["asset"])
            ok = tx is not None and isinstance(tx, unreal.Texture2D)
            size = tex_size(ic["asset"], tx) if ok else (0, 0)
            setup = ok and (ename(tx.get_editor_property("compression_settings")) == "TC_EDITOR_ICON" and
                            ename(tx.get_editor_property("lod_group")) == "TEXTUREGROUP_UI" and
                            ename(tx.get_editor_property("mip_gen_settings")) == "TMGS_NO_MIPMAPS" and
                            tx.get_editor_property("srgb"))
            check(ok and size == (128, 128) and setup, "%s loads, %dx%d, UserInterface2D, UI group, no mips" %
                  (ic["asset"], size[0], size[1]))
            if ok:
                manifest["ui"]["icons"][ic["name"]] = ic["asset"]
                manifest["all_assets"].append(ic["asset"])
    out("")

    # ------------------------------------------------------------------------------------------- ball
    out("== Ball ==")
    sm = meshes.get("SM_Ball")
    if sm:
        mi = mesh_info(sm)
        size = [mi["bounds_max"][i] - mi["bounds_min"][i] for i in range(3)]
        cen = [(mi["bounds_max"][i] + mi["bounds_min"][i]) / 2 for i in range(3)]
        check(all(abs(s - 24.0) < 0.1 for s in size) and all(abs(c) < 0.05 for c in cen),
              "SM_Ball diameter %s (24 cm), centred %s" % (fmt(size), fmt(cen)))
        check(mi["slots"] == ["Body", "Dark", "Glow"], "SM_Ball slots %s" % mi["slots"])
        manifest["ball"] = dict(mi, diameter=round(max(size), 3))
    out("")

    # ------------------------------------------------------------------------------------------- hoop
    out("== Hoop vs TaGTypes.h ==")
    sm = meshes.get("SM_Hoop")
    if sm:
        pos, ng, ns = slot_vertex_sets(sm)
        check(ng == ns, "polygon groups (%d) map 1:1 to material slots (%d)" % (ng, ns))
        blo, bhi = bbox(pos["Board"])
        exp = {"BoardFrontX": tt["BoardFrontX"], "BoardBackX": tt["BoardFrontX"] - tt["BoardThickness"],
               "BoardHalfWidth": tt["BoardHalfWidth"], "BoardBottomZ": tt["BoardBottomZ"], "BoardTopZ": tt["BoardTopZ"]}
        meas = {"BoardFrontX": bhi[0], "BoardBackX": blo[0], "BoardHalfWidth": max(bhi[1], -blo[1]),
                "BoardBottomZ": blo[2], "BoardTopZ": bhi[2]}
        for k in exp:
            check(abs(exp[k] - meas[k]) < 0.05, "%-15s TaGTypes %.2f  mesh %.2f" % (k, exp[k], meas[k]))
        # whole front face (board + border + shooter's square) must be the plane x = BoardFrontX
        front = [p for s in ("Board", "Dark", "Glow") for p in pos.get(s, []) if abs(p.x - tt["BoardFrontX"]) < 0.01]
        flo, fhi = bbox(front)
        check(abs(fhi[1] - tt["BoardHalfWidth"]) < 0.05 and abs(flo[1] + tt["BoardHalfWidth"]) < 0.05 and
              abs(flo[2] - tt["BoardBottomZ"]) < 0.05 and abs(fhi[2] - tt["BoardTopZ"]) < 0.05,
              "board front face at x=%.1f spans y %.2f..%.2f z %.2f..%.2f" %
              (tt["BoardFrontX"], flo[1], fhi[1], flo[2], fhi[2]))
        rim = pos["TaG"]
        cx = sum(p.x for p in rim) / len(rim)
        cy = sum(p.y for p in rim) / len(rim)
        cz = sum(p.z for p in rim) / len(rim)
        rad = [math.hypot(p.x - cx, p.y - cy) for p in rim]
        r_in, r_out = min(rad), max(rad)
        zmin, zmax = min(p.z for p in rim), max(p.z for p in rim)
        tube_c = 0.5 * (r_in + r_out)
        tube_r = 0.5 * (r_out - r_in)
        check(abs(cx - tt["RimX"]) < 0.05 and abs(cy) < 0.05 and abs(cz - tt["RimZ"]) < 0.05,
              "rim centre (%.2f, %.2f, %.2f) == (RimX %.1f, 0, RimZ %.1f)" % (cx, cy, cz, tt["RimX"], tt["RimZ"]))
        check(abs(tube_c - tt["RimRadius"]) < 0.05,
              "rim tube centre-line radius %.3f == RimRadius %.1f (as used by TaGBallSim)" % (tube_c, tt["RimRadius"]))
        check(abs(tube_r - tt["RimTube"]) < 0.05 and abs((zmax - zmin) / 2 - tt["RimTube"]) < 0.05,
              "rim tube radius %.3f (z %.2f..%.2f) == RimTube %.1f; inner edge r %.2f, outer r %.2f" %
              (tube_r, zmin, zmax, tt["RimTube"], r_in, r_out))
        check("Net" not in pos, "SM_Hoop has no Net slot (the net is SM_Net)")
        pole = [p for s in ("Metal", "Accent") for p in pos.get(s, []) if p.x < 0]
        plo, phi = bbox(pole)
        check(phi[0] <= -80.0, "pole/pad behind the baseline: max x %.1f <= -80" % phi[0])
        rimsock = sm.find_socket("rim")
        mi = mesh_info(sm)
        manifest["court"]["SM_Hoop"] = dict(mi, sockets={"rim": v3(rimsock.relative_location),
                                                         "net_bottom": v3(sm.find_socket("net_bottom").relative_location)},
                                            measured={"board_front_x": round(meas["BoardFrontX"], 3),
                                                      "board_back_x": round(meas["BoardBackX"], 3),
                                                      "board_y": [round(blo[1], 3), round(bhi[1], 3)],
                                                      "board_z": [round(blo[2], 3), round(bhi[2], 3)],
                                                      "rim_center": [round(cx, 3), round(cy, 3), round(cz, 3)],
                                                      "rim_tube_centerline_radius": round(tube_c, 3),
                                                      "rim_tube_radius": round(tube_r, 3),
                                                      "rim_inner_edge_radius": round(r_in, 3),
                                                      "simple_collision_boxes": len(entries["SM_Hoop"]["collision_boxes"])})
    # the net: its own mesh, pivot = rim centre, so the game can scale / wobble it from the top
    sm = meshes.get("SM_Net")
    if check(sm is not None, "SM_Net exists (the net, split out of SM_Hoop)"):
        pos, ng, ns = slot_vertex_sets(sm)
        mi = mesh_info(sm)
        net = pos.get("Net", [])
        nlo, nhi = bbox(net)
        pivot = [tt["RimX"], 0.0, tt["RimZ"]]
        top = [p for p in net if p.z > nhi[2] - 1.0]
        tcx = sum(p.x for p in top) / len(top)
        tcy = sum(p.y for p in top) / len(top)
        top_r = max(math.hypot(p.x - tcx, p.y - tcy) for p in top)
        check(mi["slots"] == ["Net"] and mi["material_instances"] == ["/Game/Art/Court/MI_Net_Net"],
              "SM_Net slot %s with %s" % (mi["slots"], mi["material_instances"]))
        check(entries["SM_Net"].get("pivot") == pivot,
              "SM_Net pivot (spawn location) %s == rim centre (RimX %.1f, 0, RimZ %.1f)" %
              (entries["SM_Net"].get("pivot"), tt["RimX"], tt["RimZ"]))
        check(abs(tcx) < 0.05 and abs(tcy) < 0.05 and -1.5 < nhi[2] <= 0.0,
              "net top ring centred on the pivot: centre (%.3f, %.3f), top z %.2f (just under the rim centre)" %
              (tcx, tcy, nhi[2]))
        check(top_r < tt["RimRadius"],
              "net top ring radius %.2f < RimRadius %.1f (hangs inside the rim tube)" % (top_r, tt["RimRadius"]))
        check(35.0 <= nhi[2] - nlo[2] <= 45.0, "net hangs %.1f cm below the rim" % (nhi[2] - nlo[2]))
        nb = sm.find_socket("net_bottom")
        check(nb is not None and abs(nb.relative_location.z - nlo[2]) < 1.0,
              "SM_Net socket net_bottom %s (pivot-relative)" % (fmt(v3(nb.relative_location)) if nb else "missing"))
        manifest["court"]["SM_Net"] = dict(mi, pivot_world=pivot, spawn_at="(RimX, 0, RimZ) from TaGTypes.h",
                                           sockets={"net_bottom": v3(nb.relative_location)} if nb else {},
                                           measured={"net_length": round(nhi[2] - nlo[2], 2),
                                                     "top_ring_radius": round(top_r, 3),
                                                     "top_z_relative": round(nhi[2], 3)})
    out("")

    # ------------------------------------------------------------------------------------------- rooftop / props
    out("== Rooftop and props ==")
    sm = meshes.get("SM_Rooftop")
    if sm:
        pos, ng, ns = slot_vertex_sets(sm)
        mi = mesh_info(sm)
        top = max(p.z for ps in pos.values() for p in ps)
        check(abs(top) < 1e-3, "SM_Rooftop top surface z = %.4f" % top)
        for s, want in (("Court", ([0, -750], [1100, 750])), ("Paint", ([0, -245], [580, 245])),
                        ("Apron", ([-600, -1300], [1700, 1300]))):
            lo_, hi_ = bbox([p for p in pos[s] if abs(p.z) < 1e-3])
            check(abs(lo_[0] - want[0][0]) < 0.05 and abs(lo_[1] - want[0][1]) < 0.05 and abs(hi_[0] - want[1][0]) < 0.05
                  and abs(hi_[1] - want[1][1]) < 0.05,
                  "slot %-5s top region x %.0f..%.0f y %.0f..%.0f" % (s, lo_[0], hi_[0], lo_[1], hi_[1]))
        check(abs(mi["bounds_min"][2] + 100.0) < 0.05, "slab thickness %.1f" % -mi["bounds_min"][2])
        manifest["court"]["SM_Rooftop"] = dict(mi, top_z=round(top, 4), simple_collision_boxes=1)
    for nm in ("SM_RooftopProps", "SM_RooftopProps_Furnace"):
        sm = meshes.get(nm)
        if not sm:
            continue
        pos, ng, ns = slot_vertex_sets(sm)
        inside = sum(1 for ps in pos.values() for p in ps
                     if p.z > -5 and -150 < p.x < 1250 and -900 < p.y < 900)
        check(inside == 0, "%s: %d vertices inside the play box x -150..1250, y -900..900 (above the floor)" % (nm, inside))
        manifest["court"][nm] = mesh_info(sm)
    out("")

    # ------------------------------------------------------------------------------------------- city / sea
    out("== City skyline ==")
    for name, e in sorted(entries.items()):
        if e["category"] != "city":
            continue
        sm = meshes.get(name)
        if not sm:
            continue
        mi = mesh_info(sm)
        c = [(mi["bounds_min"][i] + mi["bounds_max"][i]) / 2 for i in range(3)]
        dist = math.hypot(c[0] - 550.0, c[1]) / 100.0
        side_ok = (c[1] < 0) if e["side"] == "A" else (c[1] > 0)
        check(side_ok and 25.0 <= dist <= 260.0 and "Window" in mi["slots"],
              "%s side %s: centre %s, %.0f m from the court, z %.0f..%.0f, windows" %
              (name, e["side"], fmt(c), dist, mi["bounds_min"][2], mi["bounds_max"][2]))
        manifest["city"][e["side"]].append(mi["path"])
        manifest["city"]["clusters"][name] = dict(mi, side=e["side"], kind=e["meta"]["kind"],
                                                  azimuth_deg=e["meta"]["azimuth_deg"],
                                                  distance_m=e["meta"]["distance_m"])
    sm = meshes.get("SM_Sea")
    if sm:
        pos, ng, ns = slot_vertex_sets(sm)
        wz = set(round(p.z, 3) for p in pos["Water"])
        mi = mesh_info(sm)
        check(wz == {-4000.0}, "SM_Sea water plane z %s" % sorted(wz))
        manifest["sea"] = dict(mi, water_z=-4000.0, boats=entries["SM_Sea"]["meta"]["boats"],
                               piers=entries["SM_Sea"]["meta"]["piers"],
                               lanterns=entries["SM_Sea"]["meta"].get("lanterns", 0))
    out("")

    # ------------------------------------------------------------------------------------------- sky
    out("== Sky (spawn at identity, no collision, no shadows) ==")
    manifest["sky"] = {}
    sm = meshes.get("SM_SkyDome")
    if sm:
        mi = mesh_info(sm)
        c = [(mi["bounds_min"][i] + mi["bounds_max"][i]) / 2 for i in range(3)]
        r = [(mi["bounds_max"][i] - mi["bounds_min"][i]) / 2 for i in range(3)]
        check(all(abs(c[i] - SKY_CENTRE[i]) < 0.01 * SKY_RADIUS for i in range(3)) and
              all(abs(x - SKY_RADIUS) < 0.01 * SKY_RADIUS for x in r) and mi["slots"] == ["Sky"],
              "SM_SkyDome centre %s radius %s slot %s (M_TaGSky)" % (fmt(c), fmt(r), mi["slots"]))
        manifest["sky"]["SM_SkyDome"] = dict(mi, radius=SKY_RADIUS, centre=list(SKY_CENTRE))
    sm = meshes.get("SM_Clouds")
    if sm:
        mi = mesh_info(sm)
        pos, ng, ns = slot_vertex_sets(sm)
        reach = max(math.sqrt((p.x - SKY_CENTRE[0]) ** 2 + (p.y - SKY_CENTRE[1]) ** 2 + (p.z - SKY_CENTRE[2]) ** 2)
                    for ps in pos.values() for p in ps)
        check(mi["bounds_min"][2] > -4000.0 and reach < 0.98 * SKY_RADIUS,
              "SM_Clouds above the sea (z min %.0f) and inside the dome (farthest vertex %.0f of %.0f)" %
              (mi["bounds_min"][2], reach, SKY_RADIUS))
        manifest["sky"]["SM_Clouds"] = dict(mi, **entries["SM_Clouds"].get("meta", {}))
    out("")

    # ------------------------------------------------------------------------------------------- sounds
    out("== SFX (/Game/Audio/SFX) ==")
    for n in SFX_NAMES:
        path = "/Game/Audio/SFX/" + n
        sw = unreal.load_asset(path)
        if not check(sw is not None and isinstance(sw, unreal.SoundWave), "%s loads" % path):
            continue
        dur = sw.get_editor_property("duration")
        loop = sw.get_editor_property("looping")
        ok = dur > 0.03 and loop == (n == "AmbientLoop")
        if n == "ContestHum":
            ok = ok and abs(dur - 1.2) < 0.01
        if n == "TackleCue":
            ok = ok and abs(dur - 0.25) < 0.01
        if n == "AmbientLoop":
            ok = ok and 20.0 <= dur <= 30.0
        extra = ""
        if n in sfx_src:
            extra = ", source peak %.1f dBFS" % sfx_src[n]["peak_dbfs"]
        check(ok, "%-12s %.3f s looping=%s%s" % (n, dur, loop, extra))
        manifest["sfx"][n] = {"path": path, "duration_s": round(dur, 3), "looping": bool(loop),
                              "sample_rate": 44100, "channels": 1}
        manifest["all_assets"].append(path)
    out("")

    manifest["notes"] = [
        "Hero parts rotate rigidly around their pivots; every socket has zero rotation and unit scale.",
        "Rest pose: pelvis pivot at pelvis_height above the floor; chain Pelvis.spine -> Torso, Torso.neck -> Head, "
        "Torso.shoulder_l/r -> UpperArm_L/R, UpperArm.elbow -> LowerArm, Pelvis.hip_l/r -> Thigh_L/R, "
        "Thigh.knee -> Shin, Shin.ankle -> Foot_L/R. Foot pivot = ankle; Foot.sole (sole centre) and Foot.toe "
        "(front tip) lie on the floor plane in rest pose (sole flat on z = 0).",
        "LowerArm hand socket = palm surface centre; palms face the body midline in rest pose "
        "(left palm normal +Y, right palm normal -Y) so arms raised forward hold the ball between the palms.",
        "Head eyes socket = point between the eyes (first-person camera).",
        "Runtime-overridden slots: Team (Color), TaG (Emissive/EmissiveStrength), Glow (Emissive/EmissiveStrength), "
        "Window (EmissiveStrength; default 0 = lights off), Outline (Color: team tint of the inverted-hull outline).",
        "All materials are unlit toon materials lit from MPC_TaG (SunDir, SunColor, ShadeColor, RimColor, Ambient, "
        "BandSharpness, BandOffset, RimStrength, RimPower; sky: SkyZenith, SkyHorizon, SkyGround). Use manual "
        "exposure (EV100 0 / exposure compensation so an unlit 1.0 shows as 1.0); dynamic shadows are not received.",
        "M_TaGBase emissive = Color*(lerp(ShadeColor, SunColor, band)+Ambient) + rim + edge + Emissive * EmissiveStrength "
        "* VertexColor.R. Only window quads carry vertex colours (R 0.15..1.0, per-window variation); every other mesh "
        "reads 1.0.",
        "M_TaGBase edge light (per instance) = RimColor * RimStrength * Fresnel(exponent 3, base reflect 0.04) * band "
        "(lit side only). RimStrength defaults to 0; the hero instances set 0.35 (not Outline/TaG/Glow/Jelly).",
        "Every hero part has an Outline slot (inverted hull, M_TaGOutline, faces point inward): keep it in the same "
        "mesh component; it renders back faces only via its reversed winding.",
        "M_TaGParticle (additive) / M_TaGParticleAlpha (translucent) read PerInstanceCustomData 0..2 = RGB, 3 = alpha: "
        "set NumCustomDataFloats = 4 on the instanced static mesh component.",
        "SM_SkyDome (radius 160000 cm, centre (550, 0, 0)) and SM_Clouds are authored in court coordinates: spawn at "
        "identity, no collision, cast shadows off.",
        "Rim: torus with tube centre-line radius RimRadius (22.5) and tube radius RimTube (1.0), exactly the "
        "torus TaGBallSim collides against; inner edge radius is therefore 21.5.",
        "Court/city meshes are authored in court coordinates: spawn them at the identity transform, except SM_Net.",
        "SM_Net (the net, slot Net, MI_Net_Net) has its pivot at the rim centre: spawn it at (RimX, 0, RimZ) = "
        "(157.5, 0, 305) and scale / wobble it around that point; socket net_bottom is 40 cm below the pivot.",
        "Collision: SM_Rooftop has one simple box (x -600..1700, y -1300..1300, z -100..0); SM_Hoop has simple boxes "
        "for the pole and padding only; all other meshes have no simple collision.",
    ]
    manifest["all_assets"] = sorted(set(manifest["all_assets"]))
    # ------------------------------------------------------------------------------------------- totals
    tri = {"heroes": {h: v["triangles"] for h, v in manifest["heroes"].items()},
           "ball": manifest["ball"].get("triangles", 0),
           "court": {k: v["triangles"] for k, v in manifest["court"].items()},
           "city_A": sum(v["triangles"] for v in manifest["city"]["clusters"].values() if v["side"] == "A"),
           "city_B": sum(v["triangles"] for v in manifest["city"]["clusters"].values() if v["side"] == "B"),
           "sea": manifest["sea"].get("triangles", 0),
           "sky": {k: v["triangles"] for k, v in manifest["sky"].items()}}
    tri["court_city_total"] = sum(tri["court"].values()) + tri["city_A"] + tri["city_B"] + tri["sea"]
    tri["heroes_total"] = sum(tri["heroes"].values())
    manifest["triangles"] = tri
    out("== Triangle totals (LOD0, read from UE) ==")
    for h, n in tri["heroes"].items():
        out("  %-9s %6d" % (h, n))
    out("  heroes total %d (budget %d each)" % (tri["heroes_total"], HERO_TRI_BUDGET))
    out("  ball %d" % tri["ball"])
    for k, n in tri["court"].items():
        out("  %-24s %6d" % (k, n))
    out("  city A (12 meshes) %d, city B (12 meshes) %d, sea %d" % (tri["city_A"], tri["city_B"], tri["sea"]))
    out("  sky %s" % tri["sky"])
    check(tri["court_city_total"] <= 150000, "court + city + sea total %d (budget ~150k)" % tri["court_city_total"])
    out("")
    hdr = ["SUMMARY: %d checks, %d passed, %d failed, %d warnings  (%.1fs)" %
           (stats["pass"] + stats["fail"] + stats["warn"], stats["pass"], stats["fail"], stats["warn"], time.time() - t0),
           ""]
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "w", encoding="utf-8") as f:
        f.write("\n".join(lines[:3] + hdr + lines[3:]) + "\n")
    with open(MANIFEST, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1)
    unreal.log("[TaGVerify] " + hdr[0])


main()
