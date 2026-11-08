"""
sky.py - the fantasy sky around the stacked sea-city (UE coordinates, cm, court origin; spawn at identity).

  SM_SkyDome  inverted sphere (radius SKY_RADIUS) centred over the court, slot "Sky" -> M_TaGSky (gradient from
              MPC_TaG SkyZenith / SkyHorizon / SkyGround + a sun glow along MPC_TaG SunDir). No collision, and C++
              should switch off its shadows.
  SM_Clouds   cel-shaded cumulus puffs on a ring around the city, three small floating islands with lanterns and a
              shrine gate, and a pale moon. Toon-lit like everything else (M_TaGBase), slots Body / Accent / Hair /
              Glow / Extra / Outline.
"""
import math
import random

import bmesh
import numpy as np
from mathutils import Vector

import sdf
import sdfbake
from sdf import Group, Sphere, Ellipsoid, RoundBox, Cylinder, Cone, Torus, HalfSpace, Displace
from tagmesh import Part

SKY_RADIUS = 160000.0
CENTRE = (550.0, 0.0, 0.0)


def build_skydome():
    P = Part("SM_SkyDome")
    P.ue_space = True
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=SKY_RADIUS)
    for v in bm.verts:
        v.co = v.co + Vector(CENTRE)
    bmesh.ops.reverse_faces(bm, faces=bm.faces[:])       # seen from inside
    for f in bm.faces:
        f.smooth = True
    P.add(bm, "Sky", smooth=True)
    return P


def _group_to_part(P, g, h, tris):
    V, T = sdf.mesh_group(g, h)
    if len(T) == 0:
        return
    bm = sdfbake.bm_from_arrays(V, T)
    bm = sdfbake.decimate(bm, tris)
    P.add(bm, g.slot, smooth=True)


def build_clouds():
    """Cumulus puffs, floating islands and the moon. Geometry is authored in UE space."""
    P = Part("SM_Clouds")
    P.ue_space = True
    rng = random.Random(23)
    # --- clouds: flat-bottomed bumpy puffs on a ring 80-140 m out, 60-260 m above the rooftop
    for i in range(11):
        a = math.radians(i * (360.0 / 11) + rng.uniform(-10, 10))
        d = rng.uniform(80000, 140000)
        c = np.array([CENTRE[0] + math.cos(a) * d, math.sin(a) * d, rng.uniform(6000, 26000)])
        s = rng.uniform(0.8, 1.6)
        g = Group("Body", k=1800 * s)
        for j in range(rng.randint(4, 6)):
            off = np.array([rng.uniform(-2600, 2600), rng.uniform(-2600, 2600), rng.uniform(0, 1400)]) * s
            g.u(Displace(Sphere(c + off, rng.uniform(1500, 2400) * s), 160 * s, 0.0016 / s, seed=rng.random() * 6))
        g.i(HalfSpace(c - [0, 0, 300 * s], (0, 0, -1)))
        _group_to_part(P, g, 140.0 * s, 260)
    # --- floating islands: rocky cone under a grassy top, a little shrine gate and lanterns
    for i, (az, d, z, s) in enumerate(((132, 52000, 9000, 1.0), (205, 64000, 14000, 0.7), (58, 70000, 11000, 0.85))):
        a = math.radians(az)
        c = np.array([CENTRE[0] + math.cos(a) * d, math.sin(a) * d, z])
        rock = Group("Accent", k=500 * s)
        rock.u(Displace(Cone(c - [0, 0, 2200 * s], 200 * s, 3600 * s, 2200 * s), 260 * s, 0.0012 / s, seed=i))
        rock.u(Ellipsoid(c, (3800 * s, 3500 * s, 900 * s)))
        rock.i(HalfSpace(c + [0, 0, 400 * s], (0, 0, 1)))
        _group_to_part(P, rock, 110.0 * s, 300)
        grass = Group("Hair", k=300 * s)
        grass.u(Displace(Ellipsoid(c + [0, 0, 500 * s], (3700 * s, 3400 * s, 600 * s)), 120 * s, 0.002 / s, seed=i + 3))
        grass.i(HalfSpace(c + [0, 0, 350 * s], (0, 0, -1)))
        _group_to_part(P, grass, 90.0 * s, 220)
        gate = Group("Extra", k=40 * s)
        for sy in (-1, 1):
            gate.u(Cylinder(c + [0, sy * 900 * s, 1700 * s], 110 * s, 650 * s, 30 * s))
        gate.u(RoundBox(c + [0, 0, 2400 * s], (140 * s, 1400 * s, 90 * s), 40 * s))
        gate.u(RoundBox(c + [0, 0, 2050 * s], (100 * s, 1050 * s, 60 * s), 30 * s))
        _group_to_part(P, gate, 45.0 * s, 160)
        lan = Group("Glow", k=10)
        for j in range(6):
            b = math.radians(j * 60 + 20)
            lan.u(Ellipsoid(c + [math.cos(b) * 2600 * s, math.sin(b) * 2400 * s, 1300 * s + (j % 2) * 600 * s],
                            (220 * s, 220 * s, 300 * s)))
        _group_to_part(P, lan, 40.0 * s, 160)
    # --- pale moon high over the far side of the bay
    moon = Group("White", k=0)
    mc = np.array([CENTRE[0] - 105000, 42000, 52000])
    moon.u(Sphere(mc, 7000))
    _group_to_part(P, moon, 400.0, 260)
    P.meta = dict(clouds=11, islands=3)
    return P
