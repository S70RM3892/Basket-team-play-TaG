"""
render_previews.py - preview renders (Blender EEVEE, cel-shaded preview materials) into tools/art/previews/:

  heroes_lineup.png  all 30 heroes, one row per role (walls / scorers / playmakers), 3/4 view, same scale
  heroes/<Key>.png   front, 3/4 and back of each hero
  portraits.png      the 30 UI portraits (also written to tools/build/portraits for import_all.py)
  ball.png           SM_Ball close-up
  court_view.png     player's eye (175 cm) at the top of the key looking at the hoop, evening; district A lit
  court_aerial.png   overview of the rooftop, furnace complex and both districts

Run (optionally only some of heroes,portraits,turnarounds,ball,court):
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup -P C:/TaG/tools/blender/render_previews.py [-- portraits]
Court lines, hero placement on the court and the lights are preview-only (C++ draws lines and spawns lights).
"""
import math
import os
import sys

sys.dont_write_bytecode = True   # keep tools/blender free of __pycache__
HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import bpy  # noqa: E402
import bmesh  # noqa: E402
from mathutils import Vector  # noqa: E402
import scene_util  # noqa: E402
import palette  # noqa: E402
import build_assets  # noqa: E402
import heroes  # noqa: E402
from tagmesh import Part, quad, T  # noqa: E402

OUT = os.path.normpath(os.path.join(HERE, "..", "art", "previews"))
W, H = 1280, 720
LINEUP = ["Tsumugi", "Rei", "Hibiki", "Hayate", "Minamo", "Gantetsu"]   # heroes shown on the court preview


# ------------------------------------------------------------------------------------------------ scene helpers
def new_scene(name):
    sc = bpy.data.scenes.new(name)
    sc.unit_settings.system = 'METRIC'
    sc.unit_settings.scale_length = 0.01
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = W, H
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = 'PNG'
    sc.render.film_transparent = False
    try:
        sc.eevee.taa_render_samples = 64
        sc.eevee.use_shadows = True
    except Exception:
        pass
    try:
        sc.view_settings.view_transform = 'AgX'
        sc.view_settings.look = 'AgX - Medium High Contrast'
    except Exception:
        pass
    return sc


def link(sc, ob):
    sc.collection.objects.link(ob)
    return ob


def camera(sc, loc, target, lens_deg=None, ortho=None):
    cam = bpy.data.cameras.new("Cam_" + sc.name)
    cam.clip_start = 5.0
    cam.clip_end = 300000.0
    if ortho:
        cam.type = 'ORTHO'
        cam.ortho_scale = ortho
    else:
        cam.lens_unit = 'FOV'
        cam.angle = math.radians(lens_deg or 60)
    co = link(sc, bpy.data.objects.new("Cam_" + sc.name, cam))
    co.location = loc
    co.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    sc.camera = co
    return co


def sun(sc, rot_deg, energy, color=(1, 1, 1), angle=2.0, name="Sun"):
    l = bpy.data.lights.new(name + sc.name, 'SUN')
    l.energy = energy
    l.color = color
    l.angle = math.radians(angle)
    o = link(sc, bpy.data.objects.new(name + sc.name, l))
    o.rotation_euler = [math.radians(a) for a in rot_deg]
    return o


def area(sc, loc, target, power, size, color=(1, 1, 1), name="Area"):
    l = bpy.data.lights.new(name + sc.name, 'AREA')
    l.energy = power
    l.color = color
    l.size = size
    o = link(sc, bpy.data.objects.new(name + sc.name, l))
    o.location = loc
    o.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    return o


def gradient_world(sc, stops, strength=1.0, name="World"):
    """stops: [(position 0..1 = horizon..zenith, (r,g,b))]."""
    w = bpy.data.worlds.new(name + sc.name)
    sc.world = w
    w.use_nodes = True
    nt = w.node_tree
    nt.nodes.clear()
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.inputs["From Min"].default_value = -0.15
    mr.inputs["From Max"].default_value = 0.85
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    el = ramp.color_ramp.elements
    el[0].position, el[0].color = stops[0][0], (*stops[0][1], 1)
    el[1].position, el[1].color = stops[-1][0], (*stops[-1][1], 1)
    for p, c in stops[1:-1]:
        e = el.new(p)
        e.color = (*c, 1)
    bg = nt.nodes.new("ShaderNodeBackground")
    bg.inputs["Strength"].default_value = strength
    outn = nt.nodes.new("ShaderNodeOutputWorld")
    nt.links.new(tc.outputs["Generated"], sep.inputs[0])
    nt.links.new(sep.outputs["Z"], mr.inputs["Value"])
    nt.links.new(mr.outputs["Result"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bg.inputs["Color"])
    nt.links.new(bg.outputs["Background"], outn.inputs["Surface"])
    return w


def add_world_fog(w, density, color=(0.6, 0.55, 0.6), anisotropy=0.2):
    nt = w.node_tree
    vol = nt.nodes.new("ShaderNodeVolumePrincipled")
    vol.inputs["Density"].default_value = density
    vol.inputs["Color"].default_value = (*color, 1)
    vol.inputs["Anisotropy"].default_value = anisotropy
    outn = [n for n in nt.nodes if n.type == 'OUTPUT_WORLD'][0]
    nt.links.new(vol.outputs["Volume"], outn.inputs["Volume"])


def bloom(sc, threshold=1.0, size=7):
    """Glare/bloom in the compositor (API differs between Blender versions; failures are ignored)."""
    try:
        ng = bpy.data.node_groups.new("Comp_" + sc.name, "CompositorNodeTree")
        rl = ng.nodes.new("CompositorNodeRLayers")
        rl.scene = sc
        gl = ng.nodes.new("CompositorNodeGlare")
        for attr, val in (("glare_type", 'BLOOM'), ("quality", 'HIGH'), ("threshold", threshold), ("size", size)):
            try:
                setattr(gl, attr, val)
            except Exception:
                pass
        for key, val in (("Type", 'Bloom'), ("Threshold", threshold), ("Size", size / 9.0), ("Strength", 0.6)):
            try:
                if key in gl.inputs:
                    gl.inputs[key].default_value = val
            except Exception:
                pass
        try:
            go = ng.nodes.new("NodeGroupOutput")
            ng.interface.new_socket("Image", in_out='OUTPUT', socket_type='NodeSocketColor')
            ng.links.new(rl.outputs["Image"], gl.inputs["Image"])
            ng.links.new(gl.outputs["Image"], go.inputs[0])
            sc.compositing_node_group = ng
        except Exception:
            sc.use_nodes = True
            nt = sc.node_tree
            nt.nodes.clear()
            rl2 = nt.nodes.new("CompositorNodeRLayers")
            gl2 = nt.nodes.new("CompositorNodeGlare")
            gl2.glare_type = 'BLOOM'
            comp = nt.nodes.new("CompositorNodeComposite")
            nt.links.new(rl2.outputs["Image"], gl2.inputs["Image"])
            nt.links.new(gl2.outputs["Image"], comp.inputs["Image"])
        sc.render.use_compositing = True
    except Exception as ex:  # noqa
        print("bloom not available:", ex)


def emissive_mat(name, color, strength):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Emission Color"].default_value = (*color, 1)
    b.inputs["Emission Strength"].default_value = strength
    return m


def flat_mat(name, color, rough=0.8):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    return m


def text(sc, body, loc, rot, size, mat, align='CENTER'):
    cu = bpy.data.curves.new("Txt", 'FONT')
    cu.body = body
    cu.size = size
    cu.align_x = align
    o = link(sc, bpy.data.objects.new("Txt", cu))
    o.location = loc
    o.rotation_euler = [math.radians(a) for a in rot]
    cu.materials.append(mat)
    return o


def box_obj(sc, c, size, mat, name="Box"):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((c[0] + v.co.x * size[0], c[1] + v.co.y * size[1], c[2] + v.co.z * size[2]))
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    return link(sc, bpy.data.objects.new(name, me))


def window_vertex_colour_materials(owner_strength):
    """Preview 'Window' materials: emission x per-window vertex colour like M_TaGBase does in UE."""
    for (owner, slot), m in list(scene_util._preview_mats.items()):
        if slot != "Window":
            continue
        nt = m.node_tree
        em = nt.nodes.get("EMIS")
        if em is None:
            continue
        attr = nt.nodes.new("ShaderNodeVertexColor")
        attr.layer_name = "Col"
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        mul = nt.nodes.new("ShaderNodeMix")
        mul.data_type = 'RGBA'
        mul.blend_type = 'MULTIPLY'
        mul.inputs["Factor"].default_value = 1.0
        e = palette.params(owner, slot)["Emissive"]
        k = owner_strength.get(owner, 0.0) * 0.5
        mul.inputs[6].default_value = (e[0] * k, e[1] * k, e[2] * k, 1)
        nt.links.new(attr.outputs["Color"], sep.inputs[0])
        nt.links.new(sep.outputs[0], mul.inputs[7])
        nt.links.new(mul.outputs[2], em.inputs[7])


def instantiate_with_colors(part, owner, name, loc=(0, 0, 0), sc=None):
    ob = scene_util.instantiate(part, owner, name, loc, collection=sc.collection if sc else None)
    return ob


# ------------------------------------------------------------------------------------------------ renders
def render(sc, filename, folder=None):
    d = folder or OUT
    os.makedirs(d, exist_ok=True)
    sc.render.filepath = os.path.join(d, filename)
    bpy.ops.render.render(write_still=True, scene=sc.name)
    print("RENDERED", sc.render.filepath)


def place_hero(sc, rig, hid, off, yaw=0.0, suffix=""):
    o = rig.rest_origins()
    par = bpy.data.objects.new("H_%s%s" % (hid, suffix), None)
    link(sc, par)
    par.location = off
    par.rotation_euler = (0, 0, math.radians(yaw))
    for pn, part in rig.parts.items():
        ob = scene_util.instantiate(part, hid, "%s_%s%s" % (hid, pn, suffix), o[pn], collection=sc.collection)
        ob.parent = par
    return par


def toon_lights(sc):
    sun(sc, (50, 0, 145), 3.0, (1.0, 0.97, 0.92), 3.0, "Key")


def hero_lineup(rigs):
    """All 30 heroes, one row per role (walls / scorers / playmakers), 3/4 view, same scale."""
    sc = new_scene("Heroes")
    sc.view_settings.view_transform = 'Standard'
    sc.render.resolution_x, sc.render.resolution_y = 2400, 1500
    gradient_world(sc, [(0.0, (0.36, 0.38, 0.47)), (1.0, (0.22, 0.24, 0.32))], 1.0)
    toon_lights(sc)
    rows = {"Wall": [], "Scorer": [], "Playmaker": []}
    for hid in heroes.HERO_IDS:
        if hid in rigs:
            rows[heroes.SPEC[hid]["role"]].append(hid)
    d = Vector((math.cos(math.radians(-30)), math.sin(math.radians(-30)), 0.0))
    side_v = Vector((-d.y, d.x, 0))
    row_gap = 330.0
    maxw = 0
    for r, (role, ids) in enumerate(rows.items()):
        x = 0.0
        for hid in ids:
            m = rigs[hid].measures()
            x += m["width"] * 0.5 + 34
            off = side_v * (-x) + Vector((0, 0, -r * row_gap))
            place_hero(sc, rigs[hid], hid, off)
            x += m["width"] * 0.5 + 34
        maxw = max(maxw, x)
    tgt = side_v * (-maxw * 0.5) + Vector((0, 0, -row_gap + 130))
    cam = camera(sc, tgt + (d + Vector((0, 0, 0.12))).normalized() * 6000, tgt, ortho=maxw * 1.04)
    render(sc, "heroes_lineup.png")


def hero_turnarounds(rigs):
    """tools/art/previews/heroes/<Key>.png: front, 3/4 and back of one hero."""
    folder = os.path.join(OUT, "heroes")
    for hid in heroes.HERO_IDS:
        if hid not in rigs:
            continue
        sc = new_scene("T_" + hid)
        sc.view_settings.view_transform = 'Standard'
        sc.render.resolution_x, sc.render.resolution_y = 1200, 700
        gradient_world(sc, [(0.0, (0.36, 0.38, 0.47)), (1.0, (0.22, 0.24, 0.32))], 1.0)
        toon_lights(sc)
        m = rigs[hid].measures()
        gap = max(m["width"], m["depth"]) * 1.25 + 30
        for i, yaw in enumerate((0, 35, 180)):
            place_hero(sc, rigs[hid], hid, Vector((0, (1 - i) * gap, 0)), yaw=yaw, suffix="_%d" % i)
        h = m["height"]
        camera(sc, (5000, 0, h * 0.5), (0, 0, h * 0.5), ortho=max(h * 1.12 * 1200 / 700, gap * 3.2))
        render(sc, "%s.png" % hid, folder)


PORTRAITS = os.path.normpath(os.path.join(HERE, "..", "build", "portraits"))


def portraits(rigs):
    """256x256 bust per hero (3/4 view, transparent) -> tools/build/portraits/T_Portrait_<Key>.png (imported by
    import_all.py as /Game/Art/UI/T_Portrait_<Key>) and a contact sheet in the previews."""
    import numpy as np
    os.makedirs(PORTRAITS, exist_ok=True)
    tiles = []
    for hid in heroes.HERO_IDS:
        if hid not in rigs:
            continue
        rig = rigs[hid]
        sc = new_scene("P_" + hid)
        sc.view_settings.view_transform = 'Standard'
        sc.render.resolution_x = sc.render.resolution_y = 256
        sc.render.film_transparent = True
        sc.render.image_settings.color_mode = 'RGBA'
        gradient_world(sc, [(0.0, (0.5, 0.5, 0.55)), (1.0, (0.4, 0.42, 0.5))], 1.0)
        toon_lights(sc)
        place_hero(sc, rig, hid, Vector((0, 0, 0)), yaw=-28)
        o = rig.rest_origins()
        eye = o["Head"] + rig.parts["Head"].sockets["eyes"]
        lo, hi = rig.parts["Head"].bounds()
        top = o["Head"].z + hi.z
        size = max(top - (eye.z - (top - eye.z) * 1.7), 40.0 * eye.z / 175.0)
        c = Vector((0, 0, top - size * 0.48))
        camera(sc, c + Vector((1, 0, 0.08)).normalized() * 3000, c, ortho=size * 1.08)
        fp = "T_Portrait_%s.png" % hid
        render(sc, fp, PORTRAITS)
        tiles.append(os.path.join(PORTRAITS, fp))
    cols = 10
    rows = (len(tiles) + cols - 1) // cols
    sheet = np.zeros((rows * 256, cols * 256, 4), dtype=np.float32)
    sheet[..., :3] = 0.25
    sheet[..., 3] = 1.0
    for i, fp in enumerate(tiles):
        img = bpy.data.images.load(fp)
        px = np.array(img.pixels[:], dtype=np.float32).reshape(256, 256, 4)
        r, c_ = i // cols, i % cols
        dst = sheet[(rows - 1 - r) * 256:(rows - r) * 256, c_ * 256:(c_ + 1) * 256]
        a = px[..., 3:4]
        dst[..., :3] = px[..., :3] * a + dst[..., :3] * (1 - a)
        bpy.data.images.remove(img)
    out = bpy.data.images.new("portraits", cols * 256, rows * 256, alpha=True)
    out.pixels = sheet.ravel()
    out.filepath_raw = os.path.join(OUT, "portraits.png")
    out.file_format = 'PNG'
    out.save()
    print("RENDERED", out.filepath_raw)


def stack_images(top, bottom, out_name):
    import numpy as np
    a = bpy.data.images.load(os.path.join(OUT, top))
    b = bpy.data.images.load(os.path.join(OUT, bottom))
    w, h = a.size
    pa = np.empty(w * h * 4, np.float32)
    pb = np.empty(w * h * 4, np.float32)
    a.pixels.foreach_get(pa)
    b.pixels.foreach_get(pb)
    img = bpy.data.images.new("Lineup", w, h * 2, alpha=False)
    img.pixels.foreach_set(np.concatenate([pb, pa]))   # Blender images are bottom-up: bottom image first
    img.filepath_raw = os.path.join(OUT, out_name)
    img.file_format = 'PNG'
    img.save()
    print("RENDERED", img.filepath_raw)


def ball_render(assets):
    sc = new_scene("Ball")
    gradient_world(sc, [(0.0, (0.10, 0.09, 0.12)), (1.0, (0.02, 0.02, 0.03))], 1.0)
    ball = [a for a in assets if a.name == "SM_Ball"][0]
    ob = scene_util.instantiate(ball.part, "Ball", "Ball", (0, 0, 12), collection=sc.collection)
    ob.rotation_euler = (math.radians(20), math.radians(-25), math.radians(35))
    ob2 = scene_util.instantiate(ball.part, "Ball", "Ball2", (-38, 32, 12), collection=sc.collection)
    ob2.rotation_euler = (math.radians(-60), math.radians(10), math.radians(-20))
    court = flat_mat("BallFloor", palette.params("Rooftop", "Court")["Color"], 0.7)
    box_obj(sc, (0, 0, -1), (2000, 2000, 2), court, "Floor")
    camera(sc, (78, -20, 32), (-6, 10, 12), lens_deg=38)
    sun(sc, (50, 0, 140), 3.5, (1.0, 0.88, 0.75), 4.0, "Key")
    sun(sc, (60, 0, -60), 1.2, (0.6, 0.75, 1.0), 8.0, "Rim")
    bloom(sc, 1.0, 6)
    text(sc, "SM_Ball  24 cm", (10, -26, 0.5), (0, 0, 75), 4, emissive_mat("BallLbl", (0.9, 0.9, 0.9), 1.0))
    render(sc, "ball.png")


def court_lines(sc):
    """Preview-only court lines (C++ draws the real ones)."""
    P = Part("Lines")
    lw = 5.0

    def seg(a, b):
        a, b = Vector((a[0], -a[1], 0.3)), Vector((b[0], -b[1], 0.3))
        d = (b - a)
        n = Vector((-d.y, d.x, 0)).normalized() * (lw / 2)
        P.add(quad(a - n, b - n, b + n, a + n), "Body", False)
    seg((0, -750), (0, 750))
    seg((1100, -750), (1100, 750))
    seg((0, -750), (1100, -750))
    seg((0, 750), (1100, 750))
    seg((0, -245), (580, -245))
    seg((0, 245), (580, 245))
    seg((580, -245), (580, 245))
    cx = 157.5 + math.sqrt(675.0 ** 2 - 660.0 ** 2)
    seg((0, -660), (cx, -660))
    seg((0, 660), (cx, 660))
    a0 = math.asin(660.0 / 675.0)
    prev = None
    for i in range(41):
        a = -a0 + 2 * a0 * i / 40
        p = (157.5 + 675.0 * math.cos(a), 675.0 * math.sin(a))
        if prev:
            seg(prev, p)
        prev = p
    me = bpy.data.meshes.new("Lines")
    P.bm.to_mesh(me)
    me.materials.append(flat_mat("LineMat", (0.85, 0.85, 0.82), 0.6))
    link(sc, bpy.data.objects.new("Lines", me))


def court_scene(assets, rigs, a_lit=3.0, b_lit=0.0):
    sc = new_scene("Court")
    w = gradient_world(sc, [(0.0, (0.95, 0.42, 0.22)), (0.10, (0.75, 0.33, 0.30)), (0.25, (0.30, 0.18, 0.32)),
                            (0.55, (0.07, 0.07, 0.16)), (1.0, (0.02, 0.025, 0.06))], 0.9)
    add_world_fog(w, 0.0000025, (0.75, 0.55, 0.55), 0.3)
    try:
        sc.eevee.volumetric_start = 200.0
        sc.eevee.volumetric_end = 60000.0
        sc.eevee.volumetric_tile_size = '8'
    except Exception:
        pass
    for a in assets:
        if a.category in ("court", "city", "sea") or a.name == "SM_Clouds":
            p = a.extra.get("pivot", (0.0, 0.0, 0.0))     # SM_Net: spawned at the rim centre (UE x, y -> Blender x, -y)
            scene_util.instantiate(a.part, a.owner, a.name, (p[0], -p[1], p[2]), collection=sc.collection)
    window_vertex_colour_materials({"CityA": a_lit, "CityB": b_lit, "RooftopProps": 0.8})
    court_lines(sc)
    # a few heroes and the ball on the court for scale (preview only; UE: x, y -> Blender x, -y)
    for hid, (x, y, yaw) in (("Gantetsu", (200, 215, -20)), ("Tsumugi", (330, 105, -25)), ("Rei", (330, -150, 20)),
                             ("Hibiki", (130, -300, 30)), ("Ankou", (420, 330, 15)), ("Yatai", (650, -380, -10))):
        rig = rigs[hid]
        o = rig.rest_origins()
        par = bpy.data.objects.new("H_" + hid, None)
        link(sc, par)
        par.location = (x, -y, 0)
        par.rotation_euler = (0, 0, math.radians(-yaw))
        for pn, part in rig.parts.items():
            ob = scene_util.instantiate(part, hid, "%s_%s_c" % (hid, pn), o[pn], collection=sc.collection)
            ob.parent = par
    ball = [a for a in assets if a.name == "SM_Ball"][0]
    scene_util.instantiate(ball.part, "Ball", "BallC", (560, 60, 12), collection=sc.collection)
    # low evening sun from behind-left of the hoop + cool sky fill + floodlight fill over the court
    sun(sc, (82, 0, -38), 2.2, (1.0, 0.58, 0.32), 1.5, "Sun")
    sun(sc, (35, 0, 150), 0.25, (0.45, 0.55, 1.0), 20.0, "SkyFill")
    for (lx, ly) in ((-430, -1120), (-430, 1120), (1530, -1120), (1530, 1120)):
        c = Vector((lx, -ly, 1300.0))
        c = c + (Vector((550.0, 0.0, 1300.0)) - c).normalized() * 260.0   # just in front of the lamp heads
        area(sc, c, (550, 0, 0), 1.4e7, 250, (1.0, 0.93, 0.82), "Flood")
    bloom(sc, 0.9, 7)
    return sc


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    what = set(argv[0].split(",")) if argv else {"heroes", "portraits", "turnarounds", "ball", "court"}
    scene_util.reset_scene()
    rigs, assets = build_assets.build_everything()
    if "heroes" in what:
        hero_lineup(rigs)
    if "portraits" in what:
        portraits(rigs)
    if "turnarounds" in what:
        hero_turnarounds(rigs)
    if "ball" in what:
        ball_render(assets)
    if "court" in what:
        sc = court_scene(assets, rigs)
        camera(sc, (720, 0, 175), (157.5, 0, 290), lens_deg=78)
        render(sc, "court_view.png")
        camera(sc, (13000, 15000, 21000), (-1200, 0, -2500), lens_deg=50)
        render(sc, "court_aerial.png")
        camera(sc, (1150, 250, 170), (150, 30, 520), lens_deg=84)
        render(sc, "court_sky.png")


if __name__ == "__main__":
    main()
