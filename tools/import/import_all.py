"""
import_all.py - runs INSIDE Unreal (content-only helper project tools/assetproj/TaGAssets.uproject):

  "C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/TaG/tools/assetproj/TaGAssets.uproject"
      -run=pythonscript -script="C:/TaG/tools/import/import_all.py" -unattended -nop4 -nosplash -nullrhi

1. base materials  /Game/Art/Materials/M_TaGBase, M_TaGTranslucent, M_TaGWater
2. every FBX listed in tools/build/fbx/assets.json -> UStaticMesh (legacy FBX importer, imported normals,
   no auto collision, UCX_ boxes become simple collision, Nanite off)
3. one MaterialInstanceConstant per owner+slot (MI_<Owner>_<Slot>) assigned to the slot of the same name
4. sockets: the FBX importer creates them from SOCKET_ empties; this script checks every expected socket,
   adds missing ones from assets.json and normalises rotation/scale
5. tools/build/wav/*.wav -> /Game/Audio/SFX/<Name> (SoundWave), AmbientLoop looping
"""
import json
import os
import sys
import time
import unreal

TOOLS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
FBX_DIR = os.path.join(TOOLS, "build", "fbx")
WAV_DIR = os.path.join(TOOLS, "build", "wav")
LOG_PATH = os.path.join(TOOLS, "build", "import_log.txt")
MAT_DIR = "/Game/Art/Materials"
SFX_DIR = "/Game/Audio/SFX"

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
_log = []


def log(*a):
    line = " ".join(str(x) for x in a)
    _log.append(line)
    unreal.log("[TaGImport] " + line)


def lc(c, a=1.0):
    return unreal.LinearColor(float(c[0]), float(c[1]), float(c[2]), a)


# ---------------------------------------------------------------------------------------------------------- materials
def _expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


def _vparam(mat, name, default, x, y):
    e = _expr(mat, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", lc(default))
    return e


def _sparam(mat, name, default, x, y):
    e = _expr(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", float(default))
    return e


def make_base_material(name, kind):
    path = "%s/%s" % (MAT_DIR, name)
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    if kind == "translucent":
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        # "Surface Translucency Volume": surface lighting mode supported by Lumen (front layer reflections, GI)
        mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    else:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", False)
    if kind == "water":
        col = _vparam(mat, "Color", (0.004, 0.024, 0.027), -700, -200)
        rough = _sparam(mat, "Roughness", 0.06, -700, 0)
        # a touch of view-dependent brightening so the plane reads as water even without textures
        fres = _expr(mat, unreal.MaterialExpressionFresnel, -700, 150)
        lerp = _expr(mat, unreal.MaterialExpressionLinearInterpolate, -350, -150)
        bright = _expr(mat, unreal.MaterialExpressionMultiply, -500, -50)
        bright.set_editor_property("const_b", 2.5)
        mel.connect_material_expressions(col, "", bright, "A")
        mel.connect_material_expressions(col, "", lerp, "A")
        mel.connect_material_expressions(bright, "", lerp, "B")
        mel.connect_material_expressions(fres, "", lerp, "Alpha")
        mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        spec = _expr(mat, unreal.MaterialExpressionConstant, -350, 100)
        spec.set_editor_property("r", 0.6)
        mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    else:
        col = _vparam(mat, "Color", (0.5, 0.5, 0.5), -800, -300)
        rough = _sparam(mat, "Roughness", 0.6, -800, -120)
        metal = _sparam(mat, "Metallic", 0.0, -800, -20)
        emis = _vparam(mat, "Emissive", (1.0, 1.0, 1.0), -800, 100)
        stren = _sparam(mat, "EmissiveStrength", 0.0, -800, 280)
        vcol = _expr(mat, unreal.MaterialExpressionVertexColor, -800, 380)
        mul = _expr(mat, unreal.MaterialExpressionMultiply, -500, 150)
        mul2 = _expr(mat, unreal.MaterialExpressionMultiply, -300, 200)
        mel.connect_material_expressions(emis, "", mul, "A")
        mel.connect_material_expressions(stren, "", mul, "B")
        mel.connect_material_expressions(mul, "", mul2, "A")
        mel.connect_material_expressions(vcol, "R", mul2, "B")   # meshes without vertex colours read 1.0
        mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
        mel.connect_material_property(mul2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if kind == "translucent":
            op = _sparam(mat, "Opacity", 0.5, -500, 400)
            mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    if hasattr(mel, "set_base_material_usage"):
        mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    else:
        mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("material", path, kind)
    return mat


def make_mi(path, parent, prm):
    folder, name = path.rsplit("/", 1)
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = tools.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    mel.set_material_instance_vector_parameter_value(mi, "Color", lc(prm["Color"]))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", prm["Roughness"])
    if "Metallic" in prm:
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", prm["Metallic"])
        mel.set_material_instance_vector_parameter_value(mi, "Emissive", lc(prm["Emissive"]))
        mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", prm["EmissiveStrength"])
    if "Opacity" in prm:
        mel.set_material_instance_scalar_parameter_value(mi, "Opacity", prm["Opacity"])
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


# ---------------------------------------------------------------------------------------------------------- meshes
def fbx_options():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("automated_import_should_detect_type", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.static_mesh_import_data
    d.set_editor_property("combine_meshes", True)
    d.set_editor_property("auto_generate_collision", False)
    d.set_editor_property("one_convex_hull_per_ucx", True)
    d.set_editor_property("generate_lightmap_u_vs", False)
    d.set_editor_property("build_nanite", False)
    d.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property("remove_degenerates", False)
    d.set_editor_property("convert_scene", True)
    d.set_editor_property("force_front_x_axis", False)
    d.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    d.set_editor_property("import_uniform_scale", 1.0)
    return ui


def import_meshes(entries):
    # Re-importing over an existing UStaticMesh keeps stale material slots, so start from a clean asset.
    for e in entries:
        if eal.does_asset_exist(e["package"]):
            eal.delete_asset(e["package"])
    tasks = []
    for e in entries:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(FBX_DIR, e["fbx"])
        t.destination_path = e["folder"]
        t.destination_name = e["name"]
        t.replace_existing = True
        t.automated = True
        t.save = False
        t.options = fbx_options()
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    ok = 0
    for t, e in zip(tasks, entries):
        paths = list(t.imported_object_paths)
        if not paths:
            log("IMPORT FAILED", e["fbx"])
        else:
            ok += 1
    log("imported %d / %d meshes" % (ok, len(entries)))


def finish_mesh(e, mis):
    sm = unreal.load_asset(e["package"])
    if sm is None:
        log("MISSING after import", e["package"])
        return
    smes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    # materials by slot name
    names = [str(m.material_slot_name) for m in sm.static_materials]
    for i, n in enumerate(names):
        key = (e["owner"], n)
        if key not in mis:
            log("WARNING unexpected slot", e["name"], n)
            continue
        sm.set_material(i, mis[key])
    if sorted(names) != sorted(e["slots"]):
        log("WARNING slot mismatch", e["name"], names, e["slots"])
    # sockets
    added, fixed = [], []
    for sn, pos in e["sockets"].items():
        s = sm.find_socket(sn)
        want = unreal.Vector(pos[0], pos[1], pos[2])
        if s is None:
            s = unreal.StaticMeshSocket(sm)
            s.set_editor_property("socket_name", sn)
            s.set_editor_property("relative_location", want)
            sm.add_socket(s)
            added.append(sn)
        else:
            loc = s.get_editor_property("relative_location")
            if (loc - want).length() > 0.05:
                fixed.append((sn, [loc.x, loc.y, loc.z]))
                s.set_editor_property("relative_location", want)
        s.set_editor_property("relative_rotation", unreal.Rotator(0, 0, 0))
        s.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
    if added:
        log("sockets ADDED (not created by the FBX importer)", e["name"], added)
    if fixed:
        log("sockets MOVED to the expected location", e["name"], fixed)
    # build settings: no distance field for the (infinite-looking) sea plane, lightmap index 0
    try:
        bs = smes.get_lod_build_settings(sm, 0)
        bs.set_editor_property("generate_lightmap_u_vs", False)
        if e["name"] == "SM_Sea":
            bs.set_editor_property("distance_field_resolution_scale", 0.0)
        smes.set_lod_build_settings(sm, 0, bs)
    except Exception as ex:  # noqa
        log("build settings not changed for", e["name"], ex)
    sm.set_editor_property("light_map_coordinate_index", 0)
    eal.save_loaded_asset(sm)


# ---------------------------------------------------------------------------------------------------------- audio
def import_sounds():
    if not os.path.isdir(WAV_DIR):
        log("no WAV directory", WAV_DIR)
        return []
    wavs = sorted(f for f in os.listdir(WAV_DIR) if f.lower().endswith(".wav"))
    tasks = []
    for f in wavs:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(WAV_DIR, f)
        t.destination_path = SFX_DIR
        t.destination_name = os.path.splitext(f)[0]
        t.replace_existing = True
        t.automated = True
        t.save = False
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    out = []
    for t, f in zip(tasks, wavs):
        name = os.path.splitext(f)[0]
        path = "%s/%s" % (SFX_DIR, name)
        sw = unreal.load_asset(path)
        if sw is None:
            log("SOUND IMPORT FAILED", f)
            continue
        sw.set_editor_property("looping", name == "AmbientLoop")
        eal.save_loaded_asset(sw)
        out.append(path)
    log("imported %d / %d sounds" % (len(out), len(wavs)))
    return out


def main():
    t0 = time.time()
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    with open(os.path.join(FBX_DIR, "assets.json"), encoding="utf-8") as f:
        doc = json.load(f)
    entries = doc["assets"]
    only = None
    for a in sys.argv:
        if a.startswith("-only="):
            only = a.split("=", 1)[1].split(",")
    base = {"M_TaGBase": make_base_material("M_TaGBase", "opaque"),
            "M_TaGTranslucent": make_base_material("M_TaGTranslucent", "translucent"),
            "M_TaGWater": make_base_material("M_TaGWater", "water")}
    mis = {}
    for e in entries:
        for slot, prm in e["materials"].items():
            key = (e["owner"], slot)
            if key not in mis:
                mis[key] = make_mi(prm["instance"], base[prm["base"]], prm)
    log("material instances:", len(mis))
    todo = [e for e in entries if only is None or e["name"] in only or e["category"] in only]
    import_meshes(todo)
    for e in todo:
        finish_mesh(e, mis)
    import_sounds()
    eal.save_directory("/Game/Art", only_if_is_dirty=False, recursive=True)
    eal.save_directory("/Game/Audio", only_if_is_dirty=False, recursive=True)
    log("done in %.1fs" % (time.time() - t0))
    with open(LOG_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(_log) + "\n")


main()
