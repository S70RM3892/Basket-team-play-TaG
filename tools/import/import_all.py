"""
import_all.py - runs INSIDE Unreal (content-only helper project tools/assetproj/TaGAssets.uproject):

  "C:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "C:/TaG/tools/assetproj/TaGAssets.uproject"
      -run=pythonscript -script="C:/TaG/tools/import/import_all.py" -unattended -nop4 -nosplash -nullrhi

1. materials  /Game/Art/Materials: MPC_TaG (toon light), M_TaGBase / M_TaGTranslucent (unlit cel shading lit from
   MPC_TaG; M_TaGBase also has the per-instance edge light RimStrength / RimColor), M_TaGWater (toon water),
   M_TaGOutline (inverted hull), M_TaGSky, M_TaGParticle (additive) and M_TaGParticleAlpha (per-instance custom
   data 0..3 = RGB, alpha)
2. every FBX listed in tools/build/fbx/assets.json -> UStaticMesh (legacy FBX importer, imported normals,
   no auto collision, UCX_ boxes become simple collision, Nanite off)
3. one MaterialInstanceConstant per owner+slot (MI_<Owner>_<Slot>) assigned to the slot of the same name
   (hero instances get RimStrength HERO_RIM_STRENGTH, except the slots in HERO_RIM_SKIP)
4. sockets: the FBX importer creates them from SOCKET_ empties; this script checks every expected socket,
   adds missing ones from assets.json and normalises rotation/scale
5. tools/build/wav/*.wav -> /Game/Audio/SFX/<Name> (SoundWave), AmbientLoop looping (skip with -nosounds)
   (-materialsonly: steps 1 and 3 only, for material fixes: -script="C:/TaG/tools/import/import_all.py -materialsonly")
   (-only=SM_Hoop,SM_Net or -only=court: steps 1-4 for just these meshes / categories; no sounds, portraits or icons)
   (-soundsonly: step 5 only, for new or changed sound effects)
6. tools/build/portraits/T_Portrait_<Key>.png -> /Game/Art/UI (UI textures, no mips)
7. tools/build/icons/T_Icon_<Name>.png (tools/ui/make_icons.py) -> /Game/Art/UI/Icons (UI textures, no mips)
   (-iconsonly: step 7 only, for new or changed HUD icons)
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
ICON_SRC = os.path.join(TOOLS, "build", "icons")
ICON_DIR = "/Game/Art/UI/Icons"

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


def _const(mat, v, x, y):
    e = _expr(mat, unreal.MaterialExpressionConstant, x, y)
    e.set_editor_property("r", float(v))
    return e


def _const3(mat, v, x, y):
    e = _expr(mat, unreal.MaterialExpressionConstant3Vector, x, y)
    e.set_editor_property("constant", lc(v))
    return e


def _op(mat, cls, a, b, x, y, ia="A", ib="B"):
    e = _expr(mat, cls, x, y)
    if a is not None:
        mel.connect_material_expressions(a[0], a[1], e, ia)
    if b is not None:
        mel.connect_material_expressions(b[0], b[1], e, ib)
    return e


def _mul(mat, a, b, x, y):
    return _op(mat, unreal.MaterialExpressionMultiply, a, b, x, y)


def _add(mat, a, b, x, y):
    return _op(mat, unreal.MaterialExpressionAdd, a, b, x, y)


def _mask(mat, src, pin, comps, x, y):
    """ComponentMask with every channel set explicitly (a new mask does not start with all channels off)."""
    e = _expr(mat, unreal.MaterialExpressionComponentMask, x, y)
    for ch in "rgba":
        e.set_editor_property(ch, ch in comps)
    mel.connect_material_expressions(src, pin, e, "")
    return e


def _cp(mat, mpc, name, x, y):
    e = _expr(mat, unreal.MaterialExpressionCollectionParameter, x, y)
    e.set_editor_property("collection", mpc)
    e.set_editor_property("parameter_name", name)
    if name in MPC_VECTORS:
        # A collection vector comes out as float4, while vector parameters and the toon maths are float3, and the
        # material compiler refuses float3 * float4 ("Arithmetic between types float3 and float4 are undefined").
        return _mask(mat, e, "", "rgb", x + 120, y)
    return e


# Toon lighting defaults (UE world space). SunDir points TO the light: from behind a player who attacks the hoop
# (who looks toward -X), from the right and above, so the faces players look at are lit.
MPC_VECTORS = {
    "SunDir": (0.52, 0.42, 0.74),
    "SunColor": (1.0, 0.93, 0.84),
    "ShadeColor": (0.44, 0.41, 0.64),
    "RimColor": (1.0, 0.72, 0.52),
    "Ambient": (0.05, 0.045, 0.07),
    "SkyZenith": (0.10, 0.12, 0.34),
    "SkyHorizon": (1.0, 0.56, 0.42),
    "SkyGround": (0.16, 0.12, 0.2),
}
MPC_SCALARS = {"BandSharpness": 7.0, "BandOffset": -0.05, "RimStrength": 0.42, "RimPower": 3.0}

# Edge light per material instance (M_TaGBase parameters RimStrength / RimColor, on top of the MPC rim above, which
# is tinted by the surface colour): RimColor * RimStrength * Fresnel(N, V) * band, so it only shows on the lit side.
# RimStrength defaults to 0 (court, city, ball unchanged); every hero instance gets HERO_RIM_STRENGTH except the slots
# in HERO_RIM_SKIP.
RIM_COLOR = (1.0, 0.92, 0.85)
RIM_EXPONENT = 3.0
RIM_BASE_REFLECT = 0.04
HERO_RIM_STRENGTH = 0.35
HERO_RIM_SKIP = ("Outline", "TaG", "Glow", "Jelly")


def make_mpc():
    path = "%s/MPC_TaG" % MAT_DIR
    if eal.does_asset_exist(path):
        mpc = unreal.load_asset(path)
    else:
        mpc = tools.create_asset("MPC_TaG", MAT_DIR, unreal.MaterialParameterCollection,
                                 unreal.MaterialParameterCollectionFactoryNew())
    vs = []
    for n, v in MPC_VECTORS.items():
        p = unreal.CollectionVectorParameter()
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", lc(v))
        vs.append(p)
    ss = []
    for n, v in MPC_SCALARS.items():
        p = unreal.CollectionScalarParameter()
        p.set_editor_property("parameter_name", n)
        p.set_editor_property("default_value", float(v))
        ss.append(p)
    mpc.set_editor_property("vector_parameters", vs)
    mpc.set_editor_property("scalar_parameters", ss)
    eal.save_loaded_asset(mpc)
    log("material parameter collection", path, list(MPC_VECTORS), list(MPC_SCALARS))
    return mpc


def _new_material(name):
    path = "%s/%s" % (MAT_DIR, name)
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    return mat, path


def _finish(mat, path, kind, usages=(unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,)):
    for u in usages:
        if hasattr(mel, "set_base_material_usage"):
            mel.set_base_material_usage(mat, u)
        else:
            mel.set_material_usage(mat, u)
    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("material", path, kind)     # shaders are not compiled with -nullrhi: verify.py checks them (see its header)
    return mat


def toon_light(mat, mpc, col, x0=-1400):
    """Returns (the expression producing Color * (lerp(Shade, Sun, band) + Ambient) + rim, the band expression)."""
    N = _expr(mat, unreal.MaterialExpressionVertexNormalWS, x0, 200)
    L = _cp(mat, mpc, "SunDir", x0, 320)
    Ln = _op(mat, unreal.MaterialExpressionNormalize, None, None, x0 + 180, 320)
    mel.connect_material_expressions(L, "", Ln, "VectorInput")
    ndl = _op(mat, unreal.MaterialExpressionDotProduct, (N, ""), (Ln, ""), x0 + 360, 260)
    off = _cp(mat, mpc, "BandOffset", x0 + 360, 380)
    sharp = _cp(mat, mpc, "BandSharpness", x0 + 360, 460)
    t1 = _op(mat, unreal.MaterialExpressionSubtract, (ndl, ""), (off, ""), x0 + 540, 280)
    t2 = _mul(mat, (t1, ""), (sharp, ""), x0 + 700, 300)
    half = _const(mat, 0.5, x0 + 700, 420)
    t3 = _add(mat, (t2, ""), (half, ""), x0 + 860, 320)
    band = _expr(mat, unreal.MaterialExpressionSaturate, x0 + 1020, 320)
    mel.connect_material_expressions(t3, "", band, "")
    shade = _cp(mat, mpc, "ShadeColor", x0 + 1020, 120)
    sun = _cp(mat, mpc, "SunColor", x0 + 1020, 200)
    lt = _op(mat, unreal.MaterialExpressionLinearInterpolate, (shade, ""), (sun, ""), x0 + 1200, 200)
    mel.connect_material_expressions(band, "", lt, "Alpha")
    amb = _cp(mat, mpc, "Ambient", x0 + 1200, 360)
    lsum = _add(mat, (lt, ""), (amb, ""), x0 + 1380, 240)
    lit = _mul(mat, (col, ""), (lsum, ""), x0 + 1560, 120)
    # rim: (1 - N.V)^RimPower * RimStrength * RimColor, a little stronger on the lit side
    V = _expr(mat, unreal.MaterialExpressionCameraVectorWS, x0, 600)
    ndv = _op(mat, unreal.MaterialExpressionDotProduct, (N, ""), (V, ""), x0 + 360, 600)
    sat = _expr(mat, unreal.MaterialExpressionSaturate, x0 + 540, 600)
    mel.connect_material_expressions(ndv, "", sat, "")
    one = _const(mat, 1.0, x0 + 540, 700)
    inv = _op(mat, unreal.MaterialExpressionSubtract, (one, ""), (sat, ""), x0 + 700, 620)
    rp = _cp(mat, mpc, "RimPower", x0 + 700, 740)
    pw = _op(mat, unreal.MaterialExpressionPower, (inv, ""), (rp, ""), x0 + 860, 640, ia="Base", ib="Exponent")
    rs = _cp(mat, mpc, "RimStrength", x0 + 860, 760)
    rc = _cp(mat, mpc, "RimColor", x0 + 860, 840)
    r1 = _mul(mat, (pw, ""), (rs, ""), x0 + 1020, 660)
    lb = _const(mat, 0.45, x0 + 1020, 780)
    bb = _mul(mat, (band, ""), (_const(mat, 0.55, x0 + 1020, 860), ""), x0 + 1200, 780)
    r2 = _add(mat, (lb, ""), (bb, ""), x0 + 1380, 780)
    r3 = _mul(mat, (r1, ""), (r2, ""), x0 + 1380, 660)
    r4 = _mul(mat, (r3, ""), (rc, ""), x0 + 1560, 660)
    r5 = _mul(mat, (r4, ""), (col, ""), x0 + 1720, 660)   # rim tinted by the surface colour (no white halo)
    r6 = _add(mat, (r5, ""), (r4, ""), x0 + 1880, 660) if False else r5
    return _add(mat, (lit, ""), (r6, ""), x0 + 1900, 300), band


def _connect(a, a_pin, b, b_pin):
    if not mel.connect_material_expressions(a, a_pin, b, b_pin):
        log("MATERIAL CONNECTION FAILED", type(a).__name__, a_pin, "->", type(b).__name__, b_pin)


def edge_rim(mat, band, x0=-760, y0=1400):
    """RimColor * RimStrength * Fresnel(exponent 3, base reflect 0.04) * band: a warm edge light on the lit side only.
    Fresnel is a float, band a float, RimColor's default pin float3 (RGB), so the result is float3."""
    N = _expr(mat, unreal.MaterialExpressionVertexNormalWS, x0, y0)
    fr = _expr(mat, unreal.MaterialExpressionFresnel, x0 + 180, y0)
    fr.set_editor_property("exponent", RIM_EXPONENT)
    fr.set_editor_property("base_reflect_fraction", RIM_BASE_REFLECT)
    _connect(N, "", fr, "Normal")
    rs = _sparam(mat, "RimStrength", 0.0, x0 + 180, y0 + 160)
    rc = _vparam(mat, "RimColor", RIM_COLOR, x0 + 180, y0 + 240)
    lit_side = _expr(mat, unreal.MaterialExpressionMultiply, x0 + 380, y0)
    _connect(fr, "", lit_side, "A")
    _connect(band, "", lit_side, "B")
    strength = _expr(mat, unreal.MaterialExpressionMultiply, x0 + 540, y0 + 40)
    _connect(lit_side, "", strength, "A")
    _connect(rs, "", strength, "B")
    rim = _expr(mat, unreal.MaterialExpressionMultiply, x0 + 700, y0 + 80)
    _connect(strength, "", rim, "A")
    _connect(rc, "", rim, "B")
    return rim


def make_toon_material(name, kind, mpc):
    mat, path = _new_material(name)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", False)
    if kind == "translucent":
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    else:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    col = _vparam(mat, "Color", (0.5, 0.5, 0.5), -1800, -200)
    rough = _sparam(mat, "Roughness", 0.6, -1800, -60)
    metal = _sparam(mat, "Metallic", 0.0, -1800, 20)
    emis = _vparam(mat, "Emissive", (1.0, 1.0, 1.0), -1800, 1000)
    stren = _sparam(mat, "EmissiveStrength", 0.0, -1800, 1160)
    vcol = _expr(mat, unreal.MaterialExpressionVertexColor, -1800, 1240)
    shaded, band = toon_light(mat, mpc, col)
    if kind == "opaque":
        rim = edge_rim(mat, band)
        with_rim = _expr(mat, unreal.MaterialExpressionAdd, -250, 700)
        _connect(shaded, "", with_rim, "A")
        _connect(rim, "", with_rim, "B")
        shaded = with_rim
    m1 = _mul(mat, (emis, ""), (stren, ""), -400, 1050)
    m2 = _mul(mat, (m1, ""), (vcol, "R"), -250, 1100)
    # keep Roughness / Metallic as parameters (instances set them) without affecting the toon result
    keep = _mul(mat, (_add(mat, (rough, ""), (metal, ""), -1500, -20), ""), (_const(mat, 0.0, -1500, 60), ""), -1300, 0)
    s1 = _add(mat, (shaded, ""), (m2, ""), -100, 500)
    s2 = _add(mat, (s1, ""), (keep, ""), 60, 500)
    mel.connect_material_property(s2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if kind == "translucent":
        op = _sparam(mat, "Opacity", 0.5, -100, 700)
        mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    return _finish(mat, path, "toon " + kind)


def make_water_material(mpc):
    mat, path = _new_material("M_TaGWater")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    col = _vparam(mat, "Color", (0.02, 0.09, 0.12), -1500, -200)
    rough = _sparam(mat, "Roughness", 0.06, -1500, -60)
    # stylized ripple bands from world position + time, lighter toward the horizon (fresnel), sun glint
    wp = _expr(mat, unreal.MaterialExpressionWorldPosition, -1500, 200)
    t = _expr(mat, unreal.MaterialExpressionTime, -1500, 360)
    mx = _mask(mat, wp, "", "r", -1300, 200)
    my = _mask(mat, wp, "", "g", -1300, 280)
    a = _mul(mat, (mx, ""), (_const(mat, 0.0009, -1300, 360), ""), -1100, 200)
    b = _mul(mat, (my, ""), (_const(mat, 0.0013, -1300, 440), ""), -1100, 280)
    tt = _mul(mat, (t, ""), (_const(mat, 0.35, -1300, 520), ""), -1100, 380)
    ab = _add(mat, (_add(mat, (a, ""), (b, ""), -950, 240), ""), (tt, ""), -800, 280)
    sn = _expr(mat, unreal.MaterialExpressionSine, -650, 280)
    mel.connect_material_expressions(ab, "", sn, "")
    b2 = _mul(mat, (my, ""), (_const(mat, 0.0021, -1300, 600), ""), -1100, 600)
    a2 = _mul(mat, (mx, ""), (_const(mat, -0.0016, -1300, 680), ""), -1100, 680)
    ab2 = _add(mat, (_add(mat, (a2, ""), (b2, ""), -950, 640), ""), (_mul(mat, (t, ""), (_const(mat, 0.5, -1300, 760), ""), -1100, 760), ""), -800, 680)
    sn2 = _expr(mat, unreal.MaterialExpressionSine, -650, 680)
    mel.connect_material_expressions(ab2, "", sn2, "")
    wave = _mul(mat, (sn, ""), (sn2, ""), -500, 460)
    stp = _op(mat, unreal.MaterialExpressionStep, (_const(mat, 0.55, -500, 560), ""), (wave, ""), -350, 480, ia="Y", ib="X")
    V = _expr(mat, unreal.MaterialExpressionCameraVectorWS, -1500, 900)
    vz = _mask(mat, V, "", "b", -1300, 900)
    fr = _op(mat, unreal.MaterialExpressionSubtract, (_const(mat, 1.0, -1300, 980), ""), (vz, ""), -1100, 920)
    frs = _expr(mat, unreal.MaterialExpressionSaturate, -950, 920)
    mel.connect_material_expressions(fr, "", frs, "")
    fr2 = _mul(mat, (frs, ""), (frs, ""), -800, 920)
    horizon = _cp(mat, mpc, "SkyHorizon", -800, 1040)
    deep = _mul(mat, (col, ""), (_const(mat, 0.8, -800, -120), ""), -650, -160)
    light = _add(mat, (_mul(mat, (col, ""), (_const(mat, 1.8, -800, -40), ""), -650, -60), ""), (_mul(mat, (horizon, ""), (_const(mat, 0.12, -650, 40), ""), -500, 20), ""), -350, -60)
    base = _op(mat, unreal.MaterialExpressionLinearInterpolate, (deep, ""), (light, ""), -200, -100)
    mel.connect_material_expressions(fr2, "", base, "Alpha")
    rip = _mul(mat, (stp, ""), (_mul(mat, (horizon, ""), (_const(mat, 0.10, -350, 640), ""), -200, 600), ""), -50, 520)
    s = _add(mat, (base, ""), (rip, ""), 100, 100)
    keep = _mul(mat, (rough, ""), (_const(mat, 0.0, -1300, -20), ""), -1100, -40)
    s2 = _add(mat, (s, ""), (keep, ""), 260, 100)
    mel.connect_material_property(s2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return _finish(mat, path, "toon water")


def make_outline_material():
    mat, path = _new_material("M_TaGOutline")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", False)
    col = _vparam(mat, "Color", (0.012, 0.008, 0.018), -400, 0)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return _finish(mat, path, "outline (inverted hull)")


def make_sky_material(mpc):
    mat, path = _new_material("M_TaGSky")
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", True)
    wp = _expr(mat, unreal.MaterialExpressionWorldPosition, -1600, 0)
    cam = _expr(mat, unreal.MaterialExpressionCameraPositionWS, -1600, 120)
    d = _op(mat, unreal.MaterialExpressionSubtract, (wp, ""), (cam, ""), -1400, 40)
    dn = _expr(mat, unreal.MaterialExpressionNormalize, -1250, 40)
    mel.connect_material_expressions(d, "", dn, "VectorInput")
    dz = _mask(mat, dn, "", "b", -1100, 40)
    up = _expr(mat, unreal.MaterialExpressionSaturate, -950, 0)
    mel.connect_material_expressions(_mul(mat, (dz, ""), (_const(mat, 1.6, -1100, 140), ""), -1000, 120), "", up, "")
    upp = _op(mat, unreal.MaterialExpressionPower, (up, ""), (_const(mat, 0.6, -950, 120), ""), -800, 40, ia="Base", ib="Exponent")
    zen = _cp(mat, mpc, "SkyZenith", -800, -200)
    hor = _cp(mat, mpc, "SkyHorizon", -800, -120)
    gnd = _cp(mat, mpc, "SkyGround", -800, 260)
    sky = _op(mat, unreal.MaterialExpressionLinearInterpolate, (hor, ""), (zen, ""), -600, -100)
    mel.connect_material_expressions(upp, "", sky, "Alpha")
    dn_ = _expr(mat, unreal.MaterialExpressionSaturate, -950, 300)
    mel.connect_material_expressions(_mul(mat, (dz, ""), (_const(mat, -6.0, -1100, 320), ""), -1000, 300), "", dn_, "")
    sky2 = _op(mat, unreal.MaterialExpressionLinearInterpolate, (sky, ""), (gnd, ""), -450, 60)
    mel.connect_material_expressions(dn_, "", sky2, "Alpha")
    sd = _cp(mat, mpc, "SunDir", -1250, 500)
    sdn = _expr(mat, unreal.MaterialExpressionNormalize, -1100, 500)
    mel.connect_material_expressions(sd, "", sdn, "VectorInput")
    dot = _op(mat, unreal.MaterialExpressionDotProduct, (dn, ""), (sdn, ""), -950, 460)
    ds = _expr(mat, unreal.MaterialExpressionSaturate, -800, 460)
    mel.connect_material_expressions(dot, "", ds, "")
    glow = _op(mat, unreal.MaterialExpressionPower, (ds, ""), (_const(mat, 6.0, -800, 560), ""), -650, 480, ia="Base", ib="Exponent")
    disc = _op(mat, unreal.MaterialExpressionStep, (_const(mat, 0.9992, -650, 600), ""), (ds, ""), -500, 600, ia="Y", ib="X")
    sc = _cp(mat, mpc, "SunColor", -650, 700)
    g1 = _mul(mat, (glow, ""), (_const(mat, 0.55, -500, 480), ""), -350, 480)
    g2 = _add(mat, (g1, ""), (_mul(mat, (disc, ""), (_const(mat, 3.0, -500, 680), ""), -350, 640), ""), -200, 540)
    g3 = _mul(mat, (g2, ""), (sc, ""), -50, 560)
    out = _add(mat, (sky2, ""), (g3, ""), 120, 200)
    mel.connect_material_property(out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return _finish(mat, path, "sky gradient")


def make_particle_material(name, additive):
    mat, path = _new_material(name)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE if additive else
                            unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", False)
    pc = []
    for i in range(4):
        e = _expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -1200, i * 120)
        e.set_editor_property("data_index", i)
        e.set_editor_property("const_default_value", 1.0)
        pc.append(e)
    ap = _expr(mat, unreal.MaterialExpressionAppendVector, -1000, 0)
    mel.connect_material_expressions(pc[0], "", ap, "A")
    mel.connect_material_expressions(pc[1], "", ap, "B")
    rgb = _expr(mat, unreal.MaterialExpressionAppendVector, -850, 40)
    mel.connect_material_expressions(ap, "", rgb, "A")
    mel.connect_material_expressions(pc[2], "", rgb, "B")
    N = _expr(mat, unreal.MaterialExpressionVertexNormalWS, -1200, 600)
    V = _expr(mat, unreal.MaterialExpressionCameraVectorWS, -1200, 700)
    ndv = _op(mat, unreal.MaterialExpressionDotProduct, (N, ""), (V, ""), -1000, 640)
    ndvs = _expr(mat, unreal.MaterialExpressionSaturate, -850, 640)
    mel.connect_material_expressions(ndv, "", ndvs, "")
    fall = _sparam(mat, "Falloff", 1.6 if additive else 0.8, -850, 760)
    soft = _op(mat, unreal.MaterialExpressionPower, (ndvs, ""), (fall, ""), -700, 660, ia="Base", ib="Exponent")
    a = _mul(mat, (pc[3], ""), (soft, ""), -550, 500)
    inten = _sparam(mat, "Intensity", 2.0 if additive else 1.0, -550, 160)
    c1 = _mul(mat, (rgb, ""), (inten, ""), -400, 80)
    if additive:
        c2 = _mul(mat, (c1, ""), (a, ""), -250, 200)
        mel.connect_material_property(c2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        mel.connect_material_property(c1, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.connect_material_property(a, "", unreal.MaterialProperty.MP_OPACITY)
    return _finish(mat, path, "particle " + ("additive" if additive else "alpha"),
                   usages=(unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,))


def import_ui_textures(src, files, dest, what):
    """PNG files -> UI textures in `dest`: UserInterface2D (TC_EditorIcon, uncompressed RGBA, crisp edges), UI texture
    group, no mips, never streamed, sRGB."""
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(src, f)
        t.destination_path = dest
        t.destination_name = os.path.splitext(f)[0]
        t.replace_existing = True
        t.automated = True
        t.save = False
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    out = []
    for f in files:
        p = "%s/%s" % (dest, os.path.splitext(f)[0])
        tx = unreal.load_asset(p)
        if tx is None:
            log("%s IMPORT FAILED" % what.upper(), f)
            continue
        tx.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tx.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tx.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tx.set_editor_property("never_stream", True)
        tx.set_editor_property("srgb", True)
        eal.save_loaded_asset(tx)
        out.append(p)
    log("imported %d / %d %s" % (len(out), len(files), what))
    return out


def import_portraits():
    """tools/build/portraits/T_Portrait_<Key>.png -> /Game/Art/UI (UI texture group, no mips)."""
    src = os.path.join(TOOLS, "build", "portraits")
    if not os.path.isdir(src):
        log("no portraits folder", src)
        return []
    files = sorted(f for f in os.listdir(src) if f.lower().endswith(".png"))
    return import_ui_textures(src, files, "/Game/Art/UI", "portraits")


def import_icons():
    """tools/build/icons/T_Icon_<Name>.png (tools/ui/make_icons.py) -> /Game/Art/UI/Icons (white glyphs, tinted in
    Slate). The list comes from icons.json, so stale PNGs in the folder are not imported."""
    if not os.path.isfile(os.path.join(ICON_SRC, "icons.json")):
        log("no icons (run tools/ui/make_icons.py)", ICON_SRC)
        return []
    with open(os.path.join(ICON_SRC, "icons.json"), encoding="utf-8") as f:
        doc = json.load(f)
    files = [i["file"] for i in doc["icons"]]
    missing = [f for f in files if not os.path.isfile(os.path.join(ICON_SRC, f))]
    if missing:
        log("ICON PNG MISSING", missing)
    return import_ui_textures(ICON_SRC, [f for f in files if f not in missing], ICON_DIR, "icons")


def make_mi(path, parent, prm):
    folder, name = path.rsplit("/", 1)
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = tools.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.clear_all_material_instance_parameters(mi)
    if prm["base"] == "M_TaGSky":           # the sky reads everything from MPC_TaG
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        return mi
    mel.set_material_instance_vector_parameter_value(mi, "Color", lc(prm["Color"]))
    if "Roughness" in prm and not prm["base"] == "M_TaGOutline":
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", prm["Roughness"])
    if "Metallic" in prm:
        mel.set_material_instance_scalar_parameter_value(mi, "Metallic", prm["Metallic"])
        mel.set_material_instance_vector_parameter_value(mi, "Emissive", lc(prm["Emissive"]))
        mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", prm["EmissiveStrength"])
    if "Opacity" in prm:
        mel.set_material_instance_scalar_parameter_value(mi, "Opacity", prm["Opacity"])
    if "RimStrength" in prm:
        mel.set_material_instance_scalar_parameter_value(mi, "RimStrength", prm["RimStrength"])
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
    # MusicMenu / MusicMatch also land in build/wav (tools/audio/synth_music.py) but go to /Game/Audio/Music
    wavs = sorted(f for f in os.listdir(WAV_DIR) if f.lower().endswith(".wav") and not f.startswith("Music"))
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
    if "-soundsonly" in sys.argv:
        import_sounds()
        eal.save_directory("/Game/Audio", only_if_is_dirty=False, recursive=True)
        log("sounds only: done in %.1fs" % (time.time() - t0))
        with open(LOG_PATH, "w", encoding="utf-8") as f:
            f.write("\n".join(_log) + "\n")
        return
    if "-iconsonly" in sys.argv:
        import_icons()
        eal.save_directory(ICON_DIR, only_if_is_dirty=False, recursive=True)
        log("icons only: done in %.1fs" % (time.time() - t0))
        with open(LOG_PATH, "w", encoding="utf-8") as f:
            f.write("\n".join(_log) + "\n")
        return
    mpc = make_mpc()
    base = {"M_TaGBase": make_toon_material("M_TaGBase", "opaque", mpc),
            "M_TaGTranslucent": make_toon_material("M_TaGTranslucent", "translucent", mpc),
            "M_TaGWater": make_water_material(mpc),
            "M_TaGOutline": make_outline_material(),
            "M_TaGSky": make_sky_material(mpc)}
    make_particle_material("M_TaGParticle", True)
    make_particle_material("M_TaGParticleAlpha", False)
    mis = {}
    rim = 0
    for e in entries:
        for slot, prm in e["materials"].items():
            key = (e["owner"], slot)
            if key not in mis:
                if e["category"] == "hero" and slot not in HERO_RIM_SKIP and prm["base"] == "M_TaGBase":
                    prm = dict(prm, RimStrength=HERO_RIM_STRENGTH)
                    rim += 1
                mis[key] = make_mi(prm["instance"], base[prm["base"]], prm)
    log("material instances:", len(mis), "(hero edge light RimStrength %.2f on %d)" % (HERO_RIM_STRENGTH, rim))
    if "-materialsonly" in sys.argv:
        eal.save_directory("/Game/Art", only_if_is_dirty=True, recursive=True)
        log("materials only: done in %.1fs" % (time.time() - t0))
        with open(LOG_PATH, "w", encoding="utf-8") as f:
            f.write("\n".join(_log) + "\n")
        return
    todo = [e for e in entries if only is None or e["name"] in only or e["category"] in only]
    import_meshes(todo)
    for e in todo:
        finish_mesh(e, mis)
    if only is not None:
        # just these meshes: sounds, portraits and icons are left alone, and only the packages touched above are
        # saved, so the other meshes keep their bytes (game/Content is in git)
        eal.save_directory("/Game/Art", only_if_is_dirty=True, recursive=True)
        log("only %s: %d meshes, done in %.1fs" % (",".join(only), len(todo), time.time() - t0))
        with open(LOG_PATH, "w", encoding="utf-8") as f:
            f.write("\n".join(_log) + "\n")
        return
    if "-nosounds" not in sys.argv:
        import_sounds()
    import_portraits()
    import_icons()
    eal.save_directory("/Game/Art", only_if_is_dirty=False, recursive=True)
    eal.save_directory("/Game/Audio", only_if_is_dirty=False, recursive=True)
    log("done in %.1fs" % (time.time() - t0))
    with open(LOG_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(_log) + "\n")


main()
