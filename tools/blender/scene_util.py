"""
scene_util.py - turn tagmesh Parts into Blender objects (for FBX export and for preview renders).
"""
import bpy
import palette

_preview_mats = {}


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = 'METRIC'
    sc.unit_settings.scale_length = 0.01
    sc.unit_settings.length_unit = 'CENTIMETERS'
    _preview_mats.clear()
    return sc


def export_material(slot):
    """Materials used for FBX export are named exactly like the UE slot."""
    m = bpy.data.materials.get(slot)
    if m is None:
        m = bpy.data.materials.new(slot)
    return m


def preview_material(owner, slot):
    key = (owner, slot)
    if key in _preview_mats:
        return _preview_mats[key]
    pr = palette.params(owner, slot)
    m = bpy.data.materials.new("%s.%s" % (owner, slot))
    m.use_nodes = True
    nt = m.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    col = pr["Color"]
    bsdf.inputs["Base Color"].default_value = (col[0], col[1], col[2], 1.0)
    bsdf.inputs["Roughness"].default_value = pr["Roughness"]
    bsdf.inputs["Metallic"].default_value = pr.get("Metallic", 0.0)
    es = pr.get("EmissiveStrength", 0.0)
    if es > 0:
        e = pr["Emissive"]
        bsdf.inputs["Emission Color"].default_value = (e[0], e[1], e[2], 1.0)
        bsdf.inputs["Emission Strength"].default_value = es
    if "Opacity" in pr:
        bsdf.inputs["Alpha"].default_value = pr["Opacity"]
        try:
            m.surface_render_method = 'BLENDED'
        except Exception:
            pass
        try:
            m.blend_method = 'BLEND'
        except Exception:
            pass
    m.diffuse_color = (col[0], col[1], col[2], pr.get("Opacity", 1.0))
    _preview_mats[key] = m
    return m


def mesh_from_part(part, name, materials):
    me = bpy.data.meshes.new(name)
    part.bm.to_mesh(me)
    for m in materials:
        me.materials.append(m)
    return me


def instantiate(part, owner, name=None, location=(0, 0, 0), collection=None, preview=True):
    name = name or part.name
    mats = [preview_material(owner, s) if preview else export_material(s) for s in part.slots]
    me = mesh_from_part(part, name, mats)
    ob = bpy.data.objects.new(name, me)
    ob.location = location
    (collection or bpy.context.scene.collection).objects.link(ob)
    return ob
