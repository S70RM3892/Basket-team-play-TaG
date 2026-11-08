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
    """Cel-shaded preview (EEVEE): 2 tone bands + rim light + emission; the outline hull is back-face culled.
    Mirrors the UE toon material closely enough to judge shapes and colours."""
    key = (owner, slot)
    if key in _preview_mats:
        return _preview_mats[key]
    pr = palette.params(owner, slot)
    col = pr["Color"]
    m = bpy.data.materials.new("%s.%s" % (owner, slot))
    m.use_nodes = True
    nt = m.node_tree
    N, L = nt.nodes, nt.links
    N.clear()
    out = N.new("ShaderNodeOutputMaterial")
    if slot == "Outline":
        em = N.new("ShaderNodeEmission")
        em.inputs["Color"].default_value = (col[0], col[1], col[2], 1.0)
        L.new(em.outputs[0], out.inputs["Surface"])
        m.use_backface_culling = True
        _preview_mats[key] = m
        return m
    diff = N.new("ShaderNodeBsdfDiffuse")
    diff.inputs["Color"].default_value = (1, 1, 1, 1)
    s2r = N.new("ShaderNodeShaderToRGB")
    L.new(diff.outputs[0], s2r.inputs[0])
    bw = N.new("ShaderNodeRGBToBW")
    L.new(s2r.outputs["Color"], bw.inputs[0])
    ramp = N.new("ShaderNodeValToRGB")
    cr = ramp.color_ramp
    cr.interpolation = 'EASE'
    cr.elements[0].position = 0.18
    cr.elements[0].color = (0.52, 0.48, 0.66, 1.0)     # cool dusk shade
    cr.elements[1].position = 0.24
    cr.elements[1].color = (1.0, 0.96, 0.9, 1.0)
    L.new(bw.outputs[0], ramp.inputs[0])
    base = N.new("ShaderNodeMix")
    base.data_type = 'RGBA'
    base.blend_type = 'MULTIPLY'
    base.inputs["Factor"].default_value = 1.0
    base.inputs[6].default_value = (col[0], col[1], col[2], 1.0)
    L.new(ramp.outputs[0], base.inputs[7])
    lw = N.new("ShaderNodeLayerWeight")
    lw.inputs["Blend"].default_value = 0.35
    rr = N.new("ShaderNodeValToRGB")
    rr.color_ramp.elements[0].position = 0.55
    rr.color_ramp.elements[0].color = (0, 0, 0, 1)
    rr.color_ramp.elements[1].position = 0.75
    rr.color_ramp.elements[1].color = (0.3, 0.28, 0.4, 1)
    L.new(lw.outputs["Facing"], rr.inputs[0])
    add = N.new("ShaderNodeMix")
    add.data_type = 'RGBA'
    add.blend_type = 'ADD'
    add.inputs["Factor"].default_value = 1.0
    L.new(base.outputs[2], add.inputs[6])
    L.new(rr.outputs[0], add.inputs[7])
    es = pr.get("EmissiveStrength", 0.0)
    final = add
    if es > 0 or slot == "Window":
        e = pr["Emissive"]
        add2 = N.new("ShaderNodeMix")
        add2.name = "EMIS"
        add2.data_type = 'RGBA'
        add2.blend_type = 'ADD'
        add2.inputs["Factor"].default_value = 1.0
        L.new(add.outputs[2], add2.inputs[6])
        add2.inputs[7].default_value = (e[0] * es * 0.5, e[1] * es * 0.5, e[2] * es * 0.5, 1.0)
        final = add2
    em = N.new("ShaderNodeEmission")
    L.new(final.outputs[2], em.inputs["Color"])
    if "Opacity" in pr:
        tr = N.new("ShaderNodeBsdfTransparent")
        mix = N.new("ShaderNodeMixShader")
        mix.inputs[0].default_value = pr["Opacity"]
        L.new(tr.outputs[0], mix.inputs[1])
        L.new(em.outputs[0], mix.inputs[2])
        L.new(mix.outputs[0], out.inputs["Surface"])
        try:
            m.surface_render_method = 'BLENDED'
        except Exception:
            pass
    else:
        L.new(em.outputs[0], out.inputs["Surface"])
    m.diffuse_color = (col[0], col[1], col[2], 1.0)
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
