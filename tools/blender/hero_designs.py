"""
hero_designs.py - one recipe per hero (tools/art/roster.json). Coordinates are ABSOLUTE rest-pose cm
(+X forward, +Y = left, +Z up, soles on z = 0); see heroes.py for the library (human_head, torso_human, ...).

Each entry: key -> (skeleton(E, W) -> joint dict, recipe(D)).
"""
import math

import numpy as np

from heroes import (skel, human_head, visor_head, torso_human, pelvis_shorts, arm_standard, mitten, leg_standard,
                    shoe, foot_mech, hair_cap, tag_band, bean_eyes, smile, ell_front_x, side, _np, tufts, spikes,
                    ponytail, kasa, goggles, side_mask, coat_skirt, cape, pauldron, backpack)

RECIPES = {}


def recipe(key, skeleton):
    def deco(fn):
        RECIPES[key] = (skeleton, fn)
        return fn
    return deco


def P(*v):
    return np.array(v, dtype=np.float64)


# ================================================================================================================
# Rei - M scorer from Urakaze: hooded long coat breaking into floating shards, white hair, cyan light lines
# ================================================================================================================
@recipe("Rei", lambda E, W: skel(E, leg=1.03, shoulder_y=18.6, hip_y=8.8))
def rei(D):
    k = D.k
    E = D.E
    face = human_head(D, brow=None, lid="Hair")
    CX, cz, RX, RY, RZ = face
    # white messy hair with long bangs
    h = hair_cap(D, face, "Hair", grow=1.1, front_cut=0.32)
    for i, (y, z, ln) in enumerate([(-6.0, 9.5, 7.5), (-2.6, 10.8, 9.0), (1.0, 10.6, 9.5), (4.6, 9.8, 8.0),
                                    (7.8, 8.0, 6.0)]):
        a = P(CX + RX * 0.55, y * k, cz + z * k)
        b = P(CX + RX * 0.92, y * 1.1 * k, cz + (z - ln) * k)
        h.cap(a, b, 2.4 * k, 0.6 * k, k=1.0 * k)
    for s in (1, -1):
        h.cap(P(CX + 2 * k, s * RY * 0.95, cz + 2 * k), P(CX + 4 * k, s * RY * 1.0, cz - 9.5 * k), 2.6 * k, 0.7 * k)
    # hood behind the head, its point drooping down the back
    hood = D.g("Head", "Body", k=3 * k)
    hood.ell(P(CX - 4.0 * k, 0, cz + 0.5 * k), (RX * 1.08, RY * 1.22, RZ * 1.12))
    hood.half(P(CX - 1.5 * k, 0, 0), (1, 0, 0), op="i")
    hood.cap(P(CX - RX * 1.0, 0, cz + 2 * k), P(CX - RX * 1.55, 0, cz - 13 * k), 4.5 * k, 1.2 * k)
    hood_rim = D.g("Head", "Glow", voxel=0.3)
    hood_rim.torus(P(CX - 1.3 * k, 0, cz + 0.8 * k), RY * 1.06, 0.5 * k, axis=(1, 0, 0.1), sx=RZ / RY * 1.06, sy=1.0)
    hood_rim.half(P(CX - 1.3 * k, 0, cz - 6 * k), (0, 0, -1), op="i")
    # coat body
    torso_human(D, slot="Body", chest=1.0, belly=0.95, depth=1.0)
    t = D.g("Torso", "Glow", voxel=0.3)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    t.cap(P(10.3 * k, 0, sp + 2 * k), P(10.6 * k, 0, nk - 6 * k), 0.45 * k)                 # zip line
    col = D.g("Torso", "Body", k=1.5 * k)
    col.torus(P(-0.5 * k, 0, nk - 1.0 * k), 6.8 * k, 2.4 * k, sx=1.15, sy=1.05)            # high collar
    # coat skirt on the pelvis, flaring over the thighs, and the shards that float off its hem
    pz = D.J("pelvis")[2]
    hy = abs(D.J("hip")[1])
    sk = D.g("Pelvis", "Body", k=4 * k)
    sk.cone(P(0, 0, pz - 12 * k), 13.0 * k, 10.5 * k, 13 * k)
    sk.ell(P(0, 0, pz + 1 * k), (10.4 * k, hy + 5.5 * k, 7 * k))
    sk.half(P(0, 0, pz - 23 * k), (0, 0, -1), op="i")
    sk.box(P(13 * k, 0, pz - 16 * k), (5 * k, 2.4 * k, 9 * k), 1 * k, op="s", k=1.2 * k)     # front split
    hem = D.g("Pelvis", "Glow", voxel=0.3)
    hem.torus(P(0, 0, pz - 22.6 * k), 13.6 * k, 0.45 * k, sx=1.0, sy=1.0)
    hem.box(P(13 * k, 0, pz - 16 * k), (5.6 * k, 3.0 * k, 9.6 * k), 1 * k, op="s", k=0.5 * k)
    shards = D.g("Pelvis", "Body", k=0.6 * k, voxel=0.35)
    for ang, dz, sz in [(20, -27, 1.6), (65, -30, 1.2), (115, -26.5, 1.4), (160, -29.5, 1.0), (205, -27.5, 1.5),
                        (250, -31, 1.1), (295, -26.8, 1.3), (335, -29.8, 1.0)]:
        a = math.radians(ang)
        c = P(math.cos(a) * 13.5 * k, math.sin(a) * 13.0 * k, pz + dz * k)
        shards.box(c, (sz * k, sz * 0.7 * k, sz * 1.5 * k), 0.35 * k, rot=(ang * 0.7, 25, ang))
    sg = D.g("Pelvis", "Glow", voxel=0.25)
    for ang, dz in [(40, -33.5), (140, -34), (230, -33), (320, -34.5)]:
        a = math.radians(ang)
        sg.box(P(math.cos(a) * 14.5 * k, math.sin(a) * 14.0 * k, pz + dz * k), (0.6 * k, 0.6 * k, 0.9 * k), 0.25 * k,
               rot=(45, 30, ang))
    pelvis_shorts(D, slot="Extra", belt=None)
    for s in (1, -1):
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Skin", sleeve_len=1.0, ua_r=(4.6, 4.2),
                     fa_r=(4.4, 4.0))
        sd = side(s)
        cuff = D.g("LowerArm_" + sd, "Body", k=1 * k)
        w = D.J("elbow", s) + [0, 0, -15 * k]
        cuff.cone(w, 4.3 * k, 5.6 * k, 3.2 * k)
        line = D.g("UpperArm_" + sd, "Glow", voxel=0.25)
        sh = D.J("shoulder", s)
        line.cap(sh + [-1 * k, s * 4.4 * k, -6 * k], sh + [-0.5 * k, s * 4.0 * k, -20 * k], 0.4 * k)
        leg_standard(D, s, pants="Extra", shin="Extra", th_r=(6.4, 5.2), sh_r=(5.0, 4.0))
        shoe(D, s, upper="Body", sole="Dark", toe="Glow", length=1.0, width=0.95, collar="Glow")


# ================================================================================================================
# Gantetsu - XL wall, demolition machine with a soul: huge rounded body, tiny visor head, hammer fists, chimney
# ================================================================================================================
def _sk_gantetsu(E, W):
    d = skel(E, leg=0.78, neck_drop=11.0, shoulder_y=27.0, hip_y=14.0, upper=22.0, shoulder_drop=6.0,
             torso_back=0.0)
    return d


@recipe("Gantetsu", _sk_gantetsu)
def gantetsu(D):
    k = D.k
    E = D.E
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    # big rounded chassis
    body = D.g("Torso", "Body", k=6 * k)
    body.box(P(0, 0, (sp + nk) * 0.5 + 1 * k), (15 * k, 17 * k, (nk - sp) * 0.5 + 2 * k), 9 * k)
    body.ell(P(3 * k, 0, nk - 6 * k), (15.5 * k, 20 * k, 12 * k))
    plate = D.g("Torso", "Dark", k=2 * k)
    plate.box(P(13.5 * k, 0, (sp + nk) * 0.5 - 2 * k), (3.5 * k, 11 * k, 12 * k), 3 * k)
    stripe = D.g("Torso", "Accent", voxel=0.4)
    for i in range(-2, 3):
        stripe.box(P(16.2 * k, i * 4.0 * k, sp + 6 * k), (0.8 * k, 1.1 * k, 3.0 * k), 0.4 * k, rot=(30, 0, 0))
    riv = D.g("Torso", "Metal", voxel=0.35)
    for y in (-12, 12):
        for z in (nk - 6 * k, sp + 6 * k):
            riv.sphere(P(14.0 * k, y * k, z), 1.2 * k)
    # back chimney with an ember
    ch = D.g("Torso", "Metal")
    ch.cyl(P(-15 * k, -9 * k, nk + 2 * k), 3.6 * k, 9 * k, 0.8 * k)
    ch.torus(P(-15 * k, -9 * k, nk + 10.8 * k), 3.7 * k, 0.9 * k)
    em = D.g("Torso", "Glow", voxel=0.3)
    em.sphere(P(-15 * k, -9 * k, nk + 10.5 * k), 2.6 * k)
    # tiny visor head
    fx = visor_head(D, slot="Dark", c=(5.5 * k, 0, E - 1.0 * k), half=(7, 8.5, 6.5), r=3.5, visor_w=6.2,
                    visor_h=1.1, neck_slot="Metal")
    hel = D.g("Head", "Body", k=2 * k)
    hel.ell(P(4.5 * k, 0, E + 4.5 * k), (8.5 * k, 9.5 * k, 4.6 * k))
    hel.half(P(0, 0, E + 3.0 * k), (0, 0, -1), op="i")
    ant = D.g("Head", "Accent", voxel=0.3)
    ant.cap(P(0, 6 * k, E + 7 * k), P(-2 * k, 7.5 * k, E + 13 * k), 0.6 * k)
    ant.sphere(P(-2 * k, 7.5 * k, E + 13.5 * k), 1.2 * k)
    # pelvis block with skirt plates
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Dark", k=4 * k)
    pv.box(P(0, 0, pz - 2 * k), (12 * k, 17 * k, 8 * k), 5 * k)
    sk = D.g("Pelvis", "Body", k=1.5 * k)
    for y in (-11, 0, 11):
        sk.box(P(12.5 * k, y * k, pz - 8 * k), (1.8 * k, 5.0 * k, 7 * k), 1.5 * k, rot=(0, -12, 0))
    for s in (1, -1):
        sd = side(s)
        sh = D.J("shoulder", s)
        el = D.J("elbow", s)
        pad = D.g("UpperArm_" + sd, "Body", k=3 * k)
        pad.ell(sh + [0, s * 2.5 * k, 1 * k], (11 * k, 10.5 * k, 9 * k))
        pad.half(sh + [0, 0, -4 * k], (0, 0, -1), op="i", k=2 * k)
        hz = D.g("UpperArm_" + sd, "Accent", voxel=0.35)
        hz.torus(sh + [0, s * 1.5 * k, -3.2 * k], 10.2 * k, 0.9 * k, sx=1.0, sy=1.05)
        U = D.g("UpperArm_" + sd, "Metal")
        U.cap(sh + [0, 0, -3 * k], el + [0, 0, 2 * k], 6.4 * k, 5.6 * k)
        # forearm flares into the hammer fist
        F = D.g("LowerArm_" + sd, "Body", k=3 * k)
        F.cap(el + [0, 0, -1 * k], el + [0, 0, -16 * k], 7.0 * k, 9.0 * k)
        tag_band(D, "LowerArm_" + sd, el + [0, 0, -1.0 * k], 7.6 * k, 1.5 * k)
        tag_band(D, "LowerArm_" + sd, el + [0, 0, -19.5 * k], 9.6 * k, 1.6 * k)
        fist = D.g("LowerArm_" + sd, "Dark", k=2 * k)
        fc = el + [1.5 * k, -s * 1.0 * k, -29 * k]
        fist.box(fc, (9.4 * k, 8.4 * k, 8.6 * k), 4.6 * k)
        for x in (-3.6, 0.0, 3.6):
            fist.sphere(fc + [x * k + 1.0 * k, -s * 3.0 * k, -7.2 * k], 2.8 * k)
        D.socket("LowerArm_" + sd, "hand", fc + [0, -s * 7.8 * k, 0])
        leg_standard(D, s, pants="Metal", shin="Body", th_r=(8.6, 7.8), sh_r=(10.0, 9.0), band=False)
        tag_band(D, "Shin_" + sd, D.J("ankle", s) + [0, 0, 6.0 * k], 9.4 * k, 1.5 * k)
        kn = D.g("Shin_" + sd, "Accent", voxel=0.4)
        kn.ell(D.J("knee", s) + [7.5 * k, 0, -2 * k], (3.2 * k, 6.0 * k, 4.2 * k))
        foot_mech(D, s, slot="Dark", sole="Metal", glow="Accent", length=1.0, width=1.25, claws=0)


# ================================================================================================================
# Tsumugi - S playmaker, weaver girl: spool backpack, glowing threads from the fingers, two buns with needles
# ================================================================================================================
@recipe("Tsumugi", lambda E, W: skel(E, leg=0.92, neck_drop=19.0, shoulder_y=14.6, hip_y=7.4))
def tsumugi(D):
    k = D.k
    E = D.E
    face = human_head(D, rx=14.8, ry=14.0, rz=16.6, eye_y=5.6, eye_size=1.8, brow="Hair", up=3.0)
    CX, cz, RX, RY, RZ = face
    h = hair_cap(D, face, "Hair", grow=1.09, front_cut=0.38)
    for y in (-5.0, -1.6, 1.8, 5.2):
        h.ell(P(CX + RX * 0.66, y * k, cz + 7.8 * k), (3.4 * k, 3.0 * k, 4.4 * k), rot=(0, 25, 0), k=1.2 * k)
    for s in (1, -1):
        h.sphere(P(CX - 3 * k, s * 10.5 * k, cz + 10.5 * k), 5.2 * k, k=2 * k)                  # buns
        h.cap(P(CX + 3 * k, s * RY * 0.9, cz - 1 * k), P(CX + 3.5 * k, s * RY * 0.86, cz - 11 * k), 2.2 * k, 1.2 * k)
    nd = D.g("Head", "Accent", voxel=0.25)
    for s in (1, -1):
        nd.cap(P(CX - 5 * k, s * 8.5 * k, cz + 17 * k), P(CX - 1 * k, s * 13 * k, cz + 5 * k), 0.55 * k)
        nd.sphere(P(CX - 5.2 * k, s * 8.3 * k, cz + 17.6 * k), 1.0 * k)
    # kimono-ish top with a wide obi
    torso_human(D, slot="Body", chest=0.95, belly=1.0, depth=1.0, top=0.95)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    ob = D.g("Torso", "Accent", k=1.5 * k)
    ob.torus(P(0.2 * k, 0, sp + 8 * k), 10.6 * k, 3.4 * k, sx=0.98, sy=1.15)
    knot = D.g("Torso", "Accent")
    knot.ell(P(-11.5 * k, 0, sp + 9 * k), (3 * k, 6.5 * k, 3.5 * k))
    lap = D.g("Torso", "Extra", voxel=0.3)
    for s in (1, -1):
        lap.cap(P(7.8 * k, s * 4.6 * k, nk - 2 * k), P(10.4 * k, -s * 1.5 * k, sp + 13 * k), 1.0 * k)
    # spool backpack with wound thread
    sp_c = P(-14.5 * k, 0, sp + 21 * k)
    sp_m = D.g("Torso", "Accent")
    sp_m.cyl(sp_c, 7.6 * k, 1.2 * k, 0.6 * k, axis=(0, 1, 0))
    sp_m.cyl(sp_c, 3.2 * k, 9.5 * k, 0.6 * k, axis=(0, 1, 0))
    sp_m.cyl(sp_c + [0, 7.6 * k, 0], 7.6 * k, 1.2 * k, 0.6 * k, axis=(0, 1, 0))
    sp_m.cyl(sp_c - [0, 7.6 * k, 0], 7.6 * k, 1.2 * k, 0.6 * k, axis=(0, 1, 0))
    th = D.g("Torso", "Body", k=0.5 * k)
    th.cyl(sp_c, 6.2 * k, 6.0 * k, 1.6 * k, axis=(0, 1, 0))
    th.torus(sp_c, 6.2 * k, 1.1 * k, axis=(0, 1, 0))
    strap = D.g("Torso", "Dark", voxel=0.35)
    for s in (1, -1):
        strap.cap(P(-10 * k, s * 8 * k, nk - 3 * k), P(-9 * k, s * 8.5 * k, sp + 12 * k), 1.1 * k)
    # pleated short hakama
    pz = D.J("pelvis")[2]
    hk = D.g("Pelvis", "Extra", k=2.5 * k)
    hk.cone(P(0, 0, pz - 8 * k), 13.2 * k, 10.2 * k, 9 * k)
    hk.half(P(0, 0, pz - 16 * k), (0, 0, -1), op="i")
    hk_s = D.g("Pelvis", "Accent", voxel=0.3)
    hk_s.torus(P(0, 0, pz - 15.7 * k), 13.5 * k, 0.6 * k)
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Skin", hand="Skin", ua_r=(4.6, 4.4), fa_r=(3.9, 3.4),
                     hand_size=1.1)
        sl = D.g("UpperArm_" + sd, "Body", k=2 * k)
        el = D.J("elbow", s)
        sl.ell(el + [-1.5 * k, 0, 1.5 * k], (6.5 * k, 5.6 * k, 8.8 * k))          # wide kimono sleeve
        sl.half(el + [0, 0, -2 * k], (0, 0, -1), op="i", k=1 * k)
        rim = D.g("UpperArm_" + sd, "Accent", voxel=0.3)
        rim.torus(el + [-1.5 * k, 0, -1.9 * k], 5.4 * k, 0.6 * k, sx=1.15, sy=1.0)
        # glowing threads trailing from the fingers
        thr = D.g("LowerArm_" + sd, "Glow", voxel=0.2)
        w = el + [0, 0, -21 * k - 1.2 * k]
        for x, dz in ((2.4, 17), (0.0, 21), (-2.4, 15)):
            a = w + [x * k, -s * 1.7 * k, -11.6 * k]
            thr.cap(a, a + [x * 0.4 * k - 1.0 * k, -s * 1.0 * k, -dz * k], 0.28 * k)
        leg_standard(D, s, pants="Skin", shin="Extra", th_r=(5.6, 4.6), sh_r=(4.4, 3.6))
        shoe(D, s, upper="Body", sole="Dark", length=0.92, width=0.92, height=0.9, collar="Accent")


# ================================================================================================================
# Minamo - L wall, cyborg canal courier: heavy aqua mech legs with jets, one mech arm, satchel, fin clips
# ================================================================================================================
@recipe("Minamo", lambda E, W: skel(E, leg=0.98, shoulder_y=25.0, hip_y=12.5))
def minamo(D):
    k = D.k
    E = D.E
    face = human_head(D, brow="Hair", mouth_open=True)
    CX, cz, RX, RY, RZ = face
    h = hair_cap(D, face, "Hair", grow=1.1, front_cut=0.3)
    for i, ang in enumerate(range(-60, 61, 30)):
        a = math.radians(ang)
        h.ell(P(CX + RX * 0.55 * math.cos(a) + 1.5 * k, RY * 0.7 * math.sin(a), cz + 8.5 * k), (3.6 * k, 3.4 * k,
                                                                                               3.2 * k), k=1.2 * k)
    for s in (1, -1):
        h.ell(P(CX - 2 * k, s * RY * 0.95, cz - 6 * k), (5.5 * k, 3.2 * k, 6.5 * k), k=2 * k)   # wavy bob
        h.ell(P(CX - 8 * k, s * RY * 0.55, cz - 8 * k), (4.5 * k, 3.6 * k, 6 * k), k=2 * k)
    fin = D.g("Head", "Accent", voxel=0.3)
    for s in (1, -1):
        fin.box(P(CX - 2 * k, s * (RY + 2.6 * k), cz + 3 * k), (3.6 * k, 0.6 * k, 2.4 * k), 0.4 * k, rot=(0, -30, 0))
    # courier jacket (white) with blue panels
    torso_human(D, slot="Accent" if False else "Extra", chest=1.0, belly=1.0)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    pan = D.g("Torso", "Body", k=1.5 * k)
    pan.box(P(9.2 * k, 0, sp + 21 * k), (2.6 * k, 9.5 * k, 10 * k), 2.5 * k)
    strap = D.g("Torso", "Accent", voxel=0.35)
    strap.cap(P(6 * k, 15 * k, nk - 2 * k), P(10 * k, -12 * k, sp + 2 * k), 1.4 * k)
    strap.cap(P(-6 * k, 15 * k, nk - 2 * k), P(-9 * k, -12 * k, sp + 2 * k), 1.4 * k)
    pelvis_shorts(D, slot="Body", belt="Accent")
    pz = D.J("pelvis")[2]
    bag = D.g("Pelvis", "Accent", k=1.5 * k)
    bag.box(P(2 * k, -17 * k, pz - 4 * k), (7 * k, 3.4 * k, 6 * k), 2.6 * k)
    flap = D.g("Pelvis", "Extra", voxel=0.3)
    flap.box(P(2 * k, -20.3 * k, pz - 1 * k), (7.2 * k, 0.6 * k, 3.2 * k), 0.5 * k)
    for s in (1, -1):
        sd = side(s)
        if s > 0:
            arm_standard(D, s, sleeve="Extra", forearm="Skin", hand="Skin", ua_r=(5.6, 5.0), fa_r=(4.8, 4.2))
        else:
            # mechanical right arm
            sh = D.J("shoulder", s)
            el = D.J("elbow", s)
            U = D.g("UpperArm_" + sd, "Metal")
            U.sphere(sh + [0, 0, -1 * k], 6.6 * k)
            U.cap(sh + [0, 0, -3 * k], el + [0, 0, 1 * k], 4.6 * k, 4.4 * k)
            arm_p = D.g("UpperArm_" + sd, "Body")
            arm_p.ell(sh + [0, s * 1.5 * k, -1 * k], (7.4 * k, 7.0 * k, 6.0 * k))
            arm_p.half(sh + [0, 0, -3 * k], (0, 0, -1), op="i", k=1 * k)
            F = D.g("LowerArm_" + sd, "Metal")
            F.cap(el, el + [0, 0, -21 * k], 4.6 * k, 4.6 * k)
            fp = D.g("LowerArm_" + sd, "Body")
            fp.box(el + [2.5 * k, 0, -10 * k], (2.6 * k, 4.2 * k, 7 * k), 2 * k)
            jets = D.g("LowerArm_" + sd, "Glow", voxel=0.25)
            jets.cyl(el + [-4.2 * k, 0, -12 * k], 1.3 * k, 1.5 * k, 0.4 * k, axis=(1, 0, 0))
            tag_band(D, "LowerArm_" + sd, el + [0, 0, -20 * k], 4.9 * k, 1.25 * k)
            mitten(D, s, el + [0, 0, -22.2 * k], slot="Metal", size=1.05, fingers=3)
        # heavy mech legs
        hp = D.J("hip", s)
        kn = D.J("knee", s)
        an = D.J("ankle", s)
        T = D.g("Thigh_" + sd, "Metal")
        T.cap(hp + [0, 0, 1 * k], kn + [0, 0, 1.5 * k], 6.8 * k, 6.0 * k)
        tp = D.g("Thigh_" + sd, "Body")
        tp.ell(hp + [2.0 * k, s * 1.0 * k, -16 * k], (6.2 * k, 6.4 * k, 11 * k))
        Sx = D.g("Shin_" + sd, "Body", k=3 * k)
        Sx.cap(kn + [0, 0, -1 * k], an + [0, 0, 6 * k], 7.2 * k, 9.4 * k)
        Sx.ell(an + [-3.0 * k, 0, 13 * k], (6.5 * k, 8.4 * k, 10 * k))
        wh = D.g("Shin_" + sd, "Extra", voxel=0.35)
        wh.ell(kn + [6.0 * k, 0, -2 * k], (3.0 * k, 5.8 * k, 4.0 * k))
        jt = D.g("Shin_" + sd, "Glow", voxel=0.3)
        for dz in (8, 14):
            jt.cyl(an + [-9.6 * k, 0, dz * k], 1.6 * k, 1.6 * k, 0.4 * k, axis=(1, 0, 0))
        fn = D.g("Shin_" + sd, "Accent", voxel=0.3)
        fn.box(an + [-6 * k, s * 8 * k, 16 * k], (4 * k, 0.6 * k, 3 * k), 0.4 * k, rot=(0, 35, 0))
        tag_band(D, "Shin_" + sd, an + [0, 0, 3.6 * k], 7.4 * k, 1.4 * k)
        foot_mech(D, s, slot="Metal", sole="Dark", glow="Glow", length=1.05, width=1.15, claws=0)


# ================================================================================================================
# Hayate - tall M scorer, corporate ace: white-and-gold sports armour, scope monocle, high ponytail, wing fins
# ================================================================================================================
@recipe("Hayate", lambda E, W: skel(E, leg=1.08, shoulder_y=16.4, hip_y=8.6))
def hayate(D):
    k = D.k
    face = human_head(D, brow="Hair", eye_size=1.5, rz=16.0)
    CX, cz, RX, RY, RZ = face
    h = hair_cap(D, face, "Hair", grow=1.08, front_cut=0.36)
    tufts(h, face, k, [(-6.5, 10, 8, 0.6, 30), (-3.0, 11.5, 9, 0.62, 40), (1.0, 11.6, 8, 0.64, 45),
                       (5.0, 10.8, 6.5, 0.6, 40)], r=2.6)
    for s in (1, -1):
        h.cap(_np((CX + 2 * k, s * RY * 0.98, cz + 3 * k)), _np((CX + 3.5 * k, s * RY * 0.98, cz - 7 * k)),
              2.0 * k, 0.6 * k)
    ponytail(h, face, k, length=26, r=3.6, high=1.5)
    # scope monocle over the right eye
    s = -1
    fx = ell_front_x(CX, cz, RX, RY, RZ, s * 4.7 * k, D.E)
    mono = D.g("Head", "Accent", voxel=0.25)
    mono.torus(_np((fx + 0.9 * k, s * 4.7 * k, D.E)), 2.6 * k, 0.55 * k, axis=(1, 0, 0))
    mono.cap(_np((fx + 0.5 * k, s * 7.2 * k, D.E)), _np((CX, s * (RY + 0.8 * k), D.E + 1 * k)), 0.5 * k)
    mono.box(_np((CX - 1 * k, s * (RY + 1.0 * k), D.E + 1 * k)), (2.2 * k, 0.9 * k, 2.6 * k), 0.6 * k)
    lens = D.g("Head", "Glow", voxel=0.2)
    lens.cyl(_np((fx + 1.0 * k, s * 4.7 * k, D.E)), 2.1 * k, 0.15 * k, 0.1 * k, axis=(1, 0, 0))
    # armour
    torso_human(D, slot="Body", chest=1.0, belly=0.9, depth=1.0)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    tr = D.g("Torso", "Accent", voxel=0.3)
    tr.torus(_np((0.3 * k, 0, sp + 12 * k)), 9.8 * k, 0.8 * k, sx=1.05, sy=1.25)
    tr.cap(_np((10.3 * k, -6 * k, nk - 4 * k)), _np((10.9 * k, 0, sp + 18 * k)), 0.7 * k)
    tr.cap(_np((10.9 * k, 0, sp + 18 * k)), _np((10.3 * k, 6 * k, nk - 4 * k)), 0.7 * k)
    em = D.g("Torso", "Glow", voxel=0.25)
    em.ell(_np((11.0 * k, 0, sp + 25 * k)), (0.8 * k, 2.6 * k, 2.6 * k))
    pelvis_shorts(D, slot="Extra", belt="Accent")
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Extra", ua_r=(5.2, 4.6), fa_r=(4.6, 4.0))
        pauldron(D, s, "Body", size=0.85, trim="Accent")
        fin = D.g("UpperArm_" + sd, "Accent", voxel=0.3)
        sh = D.J("shoulder", s)
        fin.box(sh + [-3 * k, s * 4.8 * k, 4.5 * k], (4.0 * k, 0.5 * k, 1.6 * k), 0.4 * k, rot=(s * 25, -25, 0))
        fin.box(sh + [-5 * k, s * 4.4 * k, 2.0 * k], (3.2 * k, 0.5 * k, 1.3 * k), 0.4 * k, rot=(s * 25, -35, 0))
        leg_standard(D, s, pants="Extra", shin="Body", knee_pad="Accent", th_r=(7.0, 5.8), sh_r=(5.6, 4.6))
        shoe(D, s, upper="Body", sole="Extra", toe="Accent", collar="Glow")


# ================================================================================================================
# Hibiki - M playmaker, jellyfish colony DJ: translucent bell head, tentacle hair, stacked glowing jelly rings
# ================================================================================================================
@recipe("Hibiki", lambda E, W: skel(E, leg=1.0, shoulder_y=17.6, hip_y=8.6))
def hibiki(D):
    k = D.k
    E = D.E
    CX, cz = 1.0 * k, E + 2.5 * k
    RX, RY, RZ = 12.5 * k, 12.5 * k, 13.0 * k
    core = D.g("Head", "Body", k=3 * k)
    core.ell(_np((CX - 1.5 * k, 0, cz - 2.5 * k)), (RX * 0.78, RY * 0.78, RZ * 0.8))
    core.cap(_np((D.J("neck")[0], 0, D.J("neck")[2] - 2 * k)), _np((0, 0, cz - RZ * 0.6)), 4.2 * k, 3.6 * k)
    bell = D.g("Head", "Jelly", k=2 * k)
    bell.ell(_np((CX, 0, cz + 1.0 * k)), (RX, RY, RZ))
    bell.half(_np((0, 0, cz - 5.5 * k)), (0, 0, -1), op="i", k=1.5 * k)
    bell.ell(_np((CX, 0, cz + 1.0 * k)), (RX * 0.8, RY * 0.8, RZ * 0.8), op="s", k=1 * k)
    rim = D.g("Head", "Accent", voxel=0.3)
    rim.torus(_np((CX, 0, cz - 5.3 * k)), RX * 0.93, 0.9 * k, sx=1.0, sy=1.0)
    for i in range(9):
        a = math.radians(-160 + i * 40)
        if abs(math.degrees(a)) < 45:
            continue
        x0 = CX + math.cos(a) * RX * 0.8
        y0 = math.sin(a) * RY * 0.8
        t = D.g("Head", "Jelly", k=1 * k)
        t.cap(_np((x0, y0, cz - 5 * k)), _np((x0 - 2 * k, y0 * 1.1, cz - 20 * k - (i % 3) * 3 * k)), 2.0 * k,
              0.6 * k)
    organs = D.g("Head", "Glow", voxel=0.3)
    organs.sphere(_np((CX - 1 * k, 0, cz + 2.5 * k)), 3.4 * k)
    organs.torus(_np((CX - 1 * k, 0, cz + 2.5 * k)), 5.5 * k, 0.7 * k, axis=(0.3, 0, 1))
    bean_eyes(D, cx=CX - 1.5 * k, cz=cz - 2.5 * k, rx=RX * 0.78, ry=RY * 0.78, rz=RZ * 0.8, y=4.4 * k, z=E,
              size=1.25, slot="Accent", hi="White", shell=core)
    smile(D, x=(core.surf_x(0, E - 6 * k) or CX + 8 * k) - 0.1 * k, z=E - 6 * k, slot="Extra")
    hp = D.g("Head", "Extra", voxel=0.35)
    hp.torus(_np((CX - 2 * k, 0, cz + 2 * k)), RY * 1.02, 1.3 * k, axis=(1, 0, 0))
    for s in (1, -1):
        hp.cyl(_np((CX - 2 * k, s * RY * 1.03, cz - 2 * k)), 4.2 * k, 1.8 * k, 0.8 * k, axis=(0, 1, 0))
    cup = D.g("Head", "Accent", voxel=0.3)
    for s in (1, -1):
        cup.cyl(_np((CX - 2 * k, s * (RY * 1.03 + 1.9 * k), cz - 2 * k)), 2.8 * k, 0.3 * k, 0.2 * k, axis=(0, 1, 0))
    D.socket("Head", "eyes", _np(((core.surf_x(0, E) or CX + 8 * k) - 1.0 * k, 0, E)))
    # body: core with stacked jelly rings
    torso_human(D, slot="Body", chest=0.92, belly=0.85, depth=0.9)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    for i, z in enumerate((sp + 6 * k, sp + 17 * k, sp + 28 * k)):
        r = D.g("Torso", "Jelly", k=1 * k)
        r.torus(_np((0, 0, z)), 11.5 * k - i * 0.4 * k, 2.6 * k, sx=0.95, sy=1.15)
        gl = D.g("Torso", "Glow", voxel=0.3)
        gl.torus(_np((0, 0, z - 0.3 * k)), 11.5 * k - i * 0.4 * k, 0.6 * k, sx=1.08, sy=1.27)
    pelvis_shorts(D, slot="Extra", belt="Accent")
    pz = D.J("pelvis")[2]
    fr = D.g("Pelvis", "Jelly", k=1 * k)
    fr.cone(_np((0, 0, pz - 6 * k)), 15 * k, 12 * k, 5 * k)
    fr.half(_np((0, 0, pz - 11 * k)), (0, 0, -1), op="i")
    fr.ell(_np((0, 0, pz - 6 * k)), (11 * k, 13.5 * k, 9 * k), op="s", k=1 * k)
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Body", ua_r=(4.6, 4.2), fa_r=(4.0, 3.4),
                     fingers=3)
        fl = D.g("UpperArm_" + sd, "Jelly", k=1 * k)
        fl.torus(D.J("shoulder", s) + [0, 0, -4 * k], 5.6 * k, 1.8 * k)
        leg_standard(D, s, pants="Body", shin="Body", th_r=(6.6, 5.6), sh_r=(5.2, 4.2))
        lr = D.g("Shin_" + sd, "Jelly", k=1 * k)
        lr.torus(D.J("knee", s) + [0, 0, -6 * k], 5.8 * k, 1.6 * k)
        shoe(D, s, upper="Extra", sole="Glow", collar="Accent", length=1.05)


# ================================================================================================================
# Suzu - S scorer, festival bell-seller kid: red happi coat, fox mask on the side, golden bells
# ================================================================================================================
@recipe("Suzu", lambda E, W: skel(E, leg=0.9, neck_drop=19.0, shoulder_y=13.6, hip_y=7.0))
def suzu(D):
    k = D.k
    face = human_head(D, rx=14.8, ry=14.0, rz=16.4, eye_y=5.6, eye_size=1.8, brow="Hair", up=3.0,
                      mouth_open=True)
    CX, cz, RX, RY, RZ = face
    h = hair_cap(D, face, "Hair", grow=1.1, front_cut=0.4)
    tufts(h, face, k, [(-6, 9, 6.5, 0.62, 20), (-2.2, 10.5, 7.5, 0.64, 30), (2.0, 10.5, 7, 0.64, 30),
                       (6, 9, 6, 0.62, 20)], r=2.8)
    for s in (1, -1):
        h.ell(_np((CX - 1 * k, s * RY * 0.92, cz - 7 * k)), (5.0 * k, 3.4 * k, 6.0 * k), k=2 * k)
    h.sphere(_np((CX - 4 * k, 0, cz + RZ * 1.05)), 4.6 * k, k=1.5 * k)          # topknot
    bell = D.g("Head", "Glow", voxel=0.25)
    bell.sphere(_np((CX - 4 * k, 0, cz + RZ * 1.05 + 5.8 * k)), 2.2 * k)
    rib = D.g("Head", "Body", voxel=0.3)
    rib.torus(_np((CX - 4 * k, 0, cz + RZ * 1.05 + 2.5 * k)), 3.6 * k, 0.9 * k)
    side_mask(D, face, slot="White", mark="Body", s=1)
    torso_human(D, slot="Body", chest=0.95, belly=1.0, depth=1.0)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    col = D.g("Torso", "Accent", voxel=0.3)
    for s in (1, -1):
        col.cap(_np((6.0 * k, s * 6.5 * k, nk - 1 * k)), _np((11.0 * k, -s * 1.0 * k, sp + 10 * k)), 1.6 * k)
    obi = D.g("Torso", "Extra", voxel=0.35)
    obi.torus(_np((0.2 * k, 0, sp + 5 * k)), 10.0 * k, 2.4 * k, sx=1.0, sy=1.15)
    mon = D.g("Torso", "Accent", voxel=0.25)
    mon.torus(_np((-11.6 * k, 0, sp + 24 * k)), 3.4 * k, 0.8 * k, axis=(1, 0, 0))
    coat_skirt(D, "Body", length=12, flare=1.05, split=True, hem="Accent")
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Skin", hand="Skin", ua_r=(4.6, 4.4), fa_r=(3.9, 3.4),
                     hand_size=1.45)
        sl = D.g("UpperArm_" + sd, "Body", k=2 * k)
        el = D.J("elbow", s)
        sl.ell(el + [-1 * k, 0, 2 * k], (5.6 * k, 5.4 * k, 7.4 * k))
        sl.half(el + [0, 0, -1.5 * k], (0, 0, -1), op="i", k=1 * k)
        rim = D.g("UpperArm_" + sd, "Accent", voxel=0.3)
        rim.torus(el + [-1 * k, 0, -1.3 * k], 5.1 * k, 0.55 * k)
        b2 = D.g("UpperArm_" + sd, "Glow", voxel=0.25)
        b2.sphere(el + [-1 * k, s * 5.2 * k, -2.6 * k], 1.8 * k)
        leg_standard(D, s, pants="Skin", shin="Skin", th_r=(5.2, 4.4), sh_r=(4.2, 3.6))
        shoe(D, s, upper="Extra", sole="Body", collar="Accent", length=0.95, width=0.95, height=0.9)


# ================================================================================================================
# Hibana - S scorer, welder girl: goggles up, spiky hair, orange jumpsuit, black apron, sparks at fists and heels
# ================================================================================================================
@recipe("Hibana", lambda E, W: skel(E, leg=0.95, neck_drop=19.0, shoulder_y=14.6, hip_y=7.4))
def hibana(D):
    k = D.k
    face = human_head(D, rx=14.6, ry=13.8, rz=16.2, eye_y=5.5, eye_size=1.75, brow="Hair", up=2.8, mouth_open=True)
    CX, cz, RX, RY, RZ = face
    h = hair_cap(D, face, "Hair", grow=1.1, front_cut=0.38)
    spikes(h, face, k, [((0.2, 0, 1), 9), ((-0.5, 0.4, 0.9), 9), ((-0.5, -0.4, 0.9), 9), ((-0.9, 0.2, 0.4), 8),
                        ((-0.9, -0.2, 0.4), 8), ((0.4, 0.6, 0.7), 7), ((0.4, -0.6, 0.7), 7), ((-0.7, 0, -0.2), 7)],
           r=3.4)
    tufts(h, face, k, [(-4, 10, 6, 0.66, 40), (0.5, 11, 7, 0.68, 50), (4.5, 10, 6, 0.66, 40)], r=2.6)
    goggles(D, face, strap="Accent", frame="Metal", lens="Glow", up=True)
    torso_human(D, slot="Body", chest=0.95, belly=1.0, depth=1.0)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    ap = D.g("Torso", "Accent", k=1 * k)
    ap.box(_np((9.4 * k, 0, sp + 13 * k)), (1.8 * k, 8.5 * k, 13 * k), 1.6 * k)
    ap.cap(_np((9.0 * k, -7 * k, nk - 3 * k)), _np((9.0 * k, 7 * k, nk - 3 * k)), 1.0 * k)
    pk = D.g("Torso", "Metal", voxel=0.3)
    pk.box(_np((11.4 * k, 0, sp + 14 * k)), (0.6 * k, 4.4 * k, 3.0 * k), 0.4 * k)
    pelvis_shorts(D, slot="Body", belt="Accent")
    pz = D.J("pelvis")[2]
    ap2 = D.g("Pelvis", "Accent", k=1 * k)
    ap2.box(_np((10.6 * k, 0, pz - 7 * k)), (1.6 * k, 8.5 * k, 9 * k), 1.6 * k)
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Skin", hand="Accent", ua_r=(4.8, 4.4), fa_r=(4.0, 3.5),
                     hand_size=1.5)
        roll = D.g("UpperArm_" + sd, "Extra", voxel=0.35)
        roll.torus(D.J("elbow", s) + [0, 0, 3 * k], 4.9 * k, 1.5 * k)
        sp_ = D.g("LowerArm_" + sd, "Glow", voxel=0.2)
        w = D.J("elbow", s) + [0, 0, -22 * k]
        for i, (dx, dy, dz) in enumerate(((4, 3, -3), (2, -4, -7), (-3, 4, -5), (5, -1, -11))):
            c = w + [dx * k, s * dy * k, dz * k]
            sp_.ell(c, (0.4 * k, 0.4 * k, 1.6 * k), rot=(i * 40, i * 25, 0))
            sp_.ell(c, (1.6 * k, 0.4 * k, 0.4 * k), rot=(i * 40, i * 25, 0))
        leg_standard(D, s, pants="Body", shin="Body", th_r=(6.0, 5.0), sh_r=(4.8, 4.0), roll="Extra")
        shoe(D, s, upper="Accent", sole="Glow", toe="Metal", boot=6.0, length=1.0, width=1.05)


# ================================================================================================================
# Koyomi - M playmaker, corporate analyst: neat bob, long navy jacket, floating holo calendar panels, tablet
# ================================================================================================================
@recipe("Koyomi", lambda E, W: skel(E, leg=1.02, shoulder_y=17.0, hip_y=8.2))
def koyomi(D):
    k = D.k
    face = human_head(D, brow="Hair", eye_size=1.2, lid=None)
    CX, cz, RX, RY, RZ = face
    h = hair_cap(D, face, "Hair", grow=1.12, front_cut=0.3)
    for s in (1, -1):
        h.ell(_np((CX + 1 * k, s * RY * 0.9, cz - 4 * k)), (7.5 * k, 3.6 * k, 9.5 * k), k=2 * k)   # bob sides
    h.ell(_np((CX - 4 * k, 0, cz - 4 * k)), (9 * k, RY * 0.95, 9 * k), k=2 * k)
    h.box(_np((CX + RX * 0.62, 0, cz + 6.5 * k)), (3.0 * k, RY * 0.78, 3.0 * k), 2.6 * k, k=1.5 * k)  # blunt fringe
    gl = D.g("Head", "Metal", voxel=0.2)
    for s in (1, -1):
        fx = ell_front_x(CX, cz, RX, RY, RZ, s * 4.7 * k, D.E)
        gl.torus(_np((fx + 1.2 * k, s * 4.7 * k, D.E)), 2.5 * k, 0.32 * k, axis=(1, 0, 0), sx=1.0, sy=1.15)
    gl.cap(_np((ell_front_x(CX, cz, RX, RY, RZ, 1.8 * k, D.E) + 1.2 * k, 2.0 * k, D.E)),
           _np((ell_front_x(CX, cz, RX, RY, RZ, -1.8 * k, D.E) + 1.2 * k, -2.0 * k, D.E)), 0.3 * k)
    torso_human(D, slot="Body", chest=0.95, belly=0.95)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    sh = D.g("Torso", "Accent", k=1 * k)
    sh.ell(_np((9.6 * k, 0, nk - 7 * k)), (2.0 * k, 4.0 * k, 7 * k))
    tie = D.g("Torso", "Extra", voxel=0.3)
    tie.box(_np((11.0 * k, 0, nk - 10 * k)), (0.7 * k, 1.4 * k, 6 * k), 0.5 * k)
    panels = D.g("Torso", "Glow", voxel=0.25)
    for ang, z, tilt in ((35, nk - 2, 15), (100, sp + 18, -10), (160, nk - 8, 25), (220, sp + 6, -20),
                         (290, nk - 4, 10)):
        a = math.radians(ang)
        c = _np((math.cos(a) * 21 * k, math.sin(a) * 21 * k, z * 1.0))
        panels.box(c, (0.3 * k, 3.4 * k, 2.4 * k), 0.4 * k, rot=(tilt, 0, ang))
    coat_skirt(D, "Body", length=24, flare=1.0, split=True, hem="Glow")
    pelvis_shorts(D, slot="Extra", belt=None)
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Skin", ua_r=(4.8, 4.4), fa_r=(4.3, 3.8),
                     cuff="Accent")
        if s > 0:
            tb = D.g("LowerArm_" + sd, "Extra", voxel=0.3)
            w = D.J("elbow", s) + [0, 0, -24 * k]
            tb.box(w + [3.5 * k, -s * 2.5 * k, -4 * k], (5.0 * k, 0.6 * k, 6.5 * k), 0.8 * k, rot=(0, -20, 0))
            sc = D.g("LowerArm_" + sd, "Glow", voxel=0.2)
            sc.box(w + [3.5 * k, -s * 3.2 * k, -4 * k], (4.2 * k, 0.15 * k, 5.6 * k), 0.4 * k, rot=(0, -20, 0))
        leg_standard(D, s, pants="Extra", shin="Extra", th_r=(6.6, 5.4), sh_r=(5.0, 4.0))
        shoe(D, s, upper="Body", sole="Dark", collar="Glow", length=1.05, height=1.0)


# ================================================================================================================
# Hari - M playmaker, acupuncturist: long white coat with jade trim, round glasses, needle quiver, needle arm
# ================================================================================================================
@recipe("Hari", lambda E, W: skel(E, leg=1.0, shoulder_y=17.8, hip_y=8.6))
def hari(D):
    k = D.k
    face = human_head(D, brow="Hair", eye_size=1.15, lid="Skin")
    CX, cz, RX, RY, RZ = face
    h = hair_cap(D, face, "Hair", grow=1.09, front_cut=0.34)
    tufts(h, face, k, [(-5.5, 10.5, 6, 0.6, 30), (-1.5, 11.5, 5, 0.62, 30), (5.0, 10, 7.5, 0.6, 20)], r=2.6)
    h.sphere(_np((CX - RX * 0.95, 0, cz + 6 * k)), 4.6 * k, k=1.5 * k)          # bun
    pin = D.g("Head", "Glow", voxel=0.2)
    pin.cap(_np((CX - RX * 0.95 - 3 * k, -5 * k, cz + 10 * k)), _np((CX - RX * 0.95 + 1 * k, 5 * k, cz + 3 * k)),
            0.45 * k)
    gl = D.g("Head", "Glow", voxel=0.2)
    for s in (1, -1):
        fx = ell_front_x(CX, cz, RX, RY, RZ, s * 4.7 * k, D.E)
        gl.torus(_np((fx + 1.2 * k, s * 4.7 * k, D.E)), 2.7 * k, 0.32 * k, axis=(1, 0, 0))
    gl.cap(_np((ell_front_x(CX, cz, RX, RY, RZ, 1.8 * k, D.E) + 1.3 * k, 2.0 * k, D.E)),
           _np((ell_front_x(CX, cz, RX, RY, RZ, -1.8 * k, D.E) + 1.3 * k, -2.0 * k, D.E)), 0.3 * k)
    torso_human(D, slot="Accent", chest=0.98, belly=0.98)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    tr = D.g("Torso", "Body", voxel=0.3)
    for s in (1, -1):
        tr.cap(_np((7.0 * k, s * 6.5 * k, nk - 1 * k)), _np((10.8 * k, -s * 1.0 * k, sp + 6 * k)), 1.2 * k)
    q = D.g("Torso", "Extra", k=1 * k)
    q.cap(_np((-12 * k, 7 * k, nk + 2 * k)), _np((-12.5 * k, -6 * k, sp + 6 * k)), 3.2 * k, 3.6 * k)
    nd = D.g("Torso", "Glow", voxel=0.2)
    for i in range(5):
        a = _np((-12.2 * k, (7 - i * 1.0) * k, nk + 2.5 * k + i * 0.3 * k))
        nd.cap(a, a + _np((0.5 * k, (2.5 - i) * k, 9 * k)), 0.4 * k)
    coat_skirt(D, "Accent", length=30, flare=1.08, split=True, hem="Body")
    pelvis_shorts(D, slot="Extra", belt="Body")
    for s in (1, -1):
        sd = side(s)
        if s > 0:
            arm_standard(D, s, sleeve="Accent", forearm="Accent", hand="Skin", ua_r=(5.0, 4.6), fa_r=(4.6, 4.0),
                         cuff="Body")
        else:
            sh = D.J("shoulder", s)
            el = D.J("elbow", s)
            U = D.g("UpperArm_" + sd, "Accent")
            U.sphere(sh + [0, 0, -1 * k], 6.2 * k)
            U.cap(sh + [0, 0, -2.5 * k], el + [0, 0, 1 * k], 5.0 * k, 4.6 * k)
            F = D.g("LowerArm_" + sd, "Metal")
            F.cap(el, el + [0, 0, -21 * k], 4.2 * k, 3.6 * k)
            jade = D.g("LowerArm_" + sd, "Body", voxel=0.3)
            jade.torus(el + [0, 0, -6 * k], 4.3 * k, 1.0 * k)
            jade.torus(el + [0, 0, -12 * k], 4.0 * k, 1.0 * k)
            tag_band(D, "LowerArm_" + sd, el + [0, 0, -20 * k], 3.9 * k, 1.25 * k)
            mitten(D, s, el + [0, 0, -22.2 * k], slot="Metal", size=1.3, fingers=3)
            nf = D.g("LowerArm_" + sd, "Glow", voxel=0.2)
            w = el + [0, 0, -22.2 * k]
            for x in (2.5, 0.0, -2.5):
                a = w + [x * 1.3 * k, -s * 1.7 * 1.3 * k, -11.6 * 1.3 * k]
                nf.cap(a, a + [0, -s * 0.6 * k, -4.5 * k], 0.3 * k)
        leg_standard(D, s, pants="Extra", shin="Extra", th_r=(6.6, 5.4), sh_r=(5.0, 4.0))
        shoe(D, s, upper="Body", sole="Extra", collar="Glow", length=1.02)


# ================================================================================================================
# Ushio - M scorer, fisherman: straw kasa, navy raincoat with yellow trim, a friendly eel coiled on the shoulders
# ================================================================================================================
@recipe("Ushio", lambda E, W: skel(E, leg=1.0, shoulder_y=19.6, hip_y=9.6))
def ushio(D):
    k = D.k
    E = D.E
    face = human_head(D, brow="Hair", eye_size=1.1, lid=None, cheeks=1.1, chin=1.2)
    CX, cz, RX, RY, RZ = face
    h = hair_cap(D, face, "Hair", grow=1.06, front_cut=0.42)
    for s in (1, -1):
        h.ell(_np((CX + 3 * k, s * RY * 0.95, cz - 7 * k)), (2.6 * k, 1.6 * k, 4.0 * k), k=1 * k)     # sideburns
    kasa(D, face, "Extra", r=22, h=8.5, band="Body")
    torso_human(D, slot="Body", chest=1.02, belly=1.05)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    tr = D.g("Torso", "Accent", voxel=0.3)
    tr.cap(_np((10.4 * k, 0, nk - 4 * k)), _np((11.0 * k, 0, sp + 2 * k)), 0.9 * k)
    tr.torus(_np((-0.5 * k, 0, nk - 1.5 * k)), 7.6 * k, 1.5 * k, sx=1.1, sy=1.1)
    # the eel: a fat green tube from the back over the left shoulder, head resting on the chest
    pts = [(-12, -12, sp + 26), (-10, 4, nk + 3), (-1, 15, nk + 6), (9, 13, nk - 2), (12, 4, sp + 26)]
    eel = D.g("Torso", "Glow", k=2.2 * k)
    for a, b in zip(pts[:-1], pts[1:]):
        eel.cap(_np(a) * [k, k, 1], _np(b) * [k, k, 1], 3.4 * k, 3.6 * k)
    head = _np((13.5 * k, 2.0 * k, sp + 24 * k))
    eel.ell(head, (4.6 * k, 4.0 * k, 3.6 * k))
    st = D.g("Torso", "Accent", voxel=0.25)
    for a, b in zip(pts[:-1], pts[1:]):
        m = (_np(a) + _np(b)) * 0.5 * [k, k, 1]
        st.sphere(m + [0, 0, 3.3 * k], 1.2 * k)
    ey = D.g("Torso", "Dark", voxel=0.2)
    for s in (1, -1):
        ey.sphere(head + [2.6 * k, s * 2.4 * k, 1.6 * k], 0.9 * k)
    coat_skirt(D, "Body", length=26, flare=1.06, split=True, hem="Accent")
    pelvis_shorts(D, slot="Extra", belt="Dark")
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Skin", ua_r=(5.6, 5.0), fa_r=(5.0, 4.4),
                     cuff="Accent", hand_size=1.45)
        leg_standard(D, s, pants="Extra", shin="Accent", th_r=(7.0, 6.0), sh_r=(6.2, 5.2))
        shoe(D, s, upper="Accent", sole="Dark", boot=8.0, length=1.08, width=1.12)


# ================================================================================================================
# Kotetsu - S wall, small round welding robot from Gantetsu's spare parts: goggle eyes, torch arm, antenna
# ================================================================================================================
@recipe("Kotetsu", lambda E, W: skel(E, leg=1.0, neck_drop=19.0, shoulder_y=16.0, hip_y=8.5, upper=24.0,
                                     torso_back=0.0))
def kotetsu(D):
    k = D.k
    E = D.E
    hc = _np((1.0 * k, 0, E - 1.0 * k))
    hd = D.g("Head", "Body", k=2 * k)
    hd.ell(hc, (16.5 * k, 17.5 * k, 16.0 * k))
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 8 * k], 4.0 * k)
    for s in (1, -1):
        fx = hd.surf_x(s * 5.8 * k, E) or hc[0] + 11 * k
        fr = D.g("Head", "Extra", voxel=0.3)
        fr.cyl(_np((fx + 0.4 * k, s * 5.8 * k, E)), 4.6 * k, 1.6 * k, 0.8 * k, axis=(1, 0, 0))
        fr.cyl(_np((fx - 0.6 * k, s * 5.8 * k, E)), 5.4 * k, 0.7 * k, 0.3 * k, axis=(1, 0, 0))
        ln = D.g("Head", "White", voxel=0.25)
        ln.cyl(_np((fx + 1.8 * k, s * 5.8 * k, E)), 3.8 * k, 0.4 * k, 0.3 * k, axis=(1, 0, 0))
        pu = D.g("Head", "Dark", voxel=0.2)
        pu.ell(_np((fx + 2.1 * k, s * 5.2 * k, E - 0.3 * k)), (0.5 * k, 1.6 * k, 2.1 * k))
        hl = D.g("Head", "White", voxel=0.18)
        hl.sphere(_np((fx + 2.5 * k, s * 4.6 * k, E + 0.9 * k)), 0.6 * k)
    ant = D.g("Head", "Accent", voxel=0.3)
    ant.cap(hc + [-2 * k, 3 * k, 14 * k], hc + [-4 * k, 5 * k, 23 * k], 0.7 * k)
    tip = D.g("Head", "Glow", voxel=0.25)
    tip.sphere(hc + [-4 * k, 5 * k, 23.8 * k], 1.8 * k)
    bolt = D.g("Head", "Extra", voxel=0.3)
    for s in (1, -1):
        bolt.cyl(hc + [0, s * 17.0 * k, 0], 3.0 * k, 1.2 * k, 0.5 * k, axis=(0, 1, 0))
    D.socket("Head", "eyes", _np(((hd.surf_x(0, E) or hc[0] + 12 * k) - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    t = D.g("Torso", "Body", k=4 * k)
    t.ell(_np((0.5 * k, 0, (sp + nk) * 0.5)), (11.5 * k, 13.0 * k, (nk - sp) * 0.6))
    bp = D.g("Torso", "Accent", k=1.5 * k)
    bp.ell(_np((9.5 * k, 0, (sp + nk) * 0.5 - 2 * k)), (4.0 * k, 9.0 * k, 9.0 * k))
    st = D.g("Torso", "Glow", voxel=0.25)
    st.cyl(_np((13.1 * k, 0, (sp + nk) * 0.5 - 1 * k)), 2.6 * k, 0.4 * k, 0.3 * k, axis=(1, 0, 0))
    rv = D.g("Torso", "Extra", voxel=0.3)
    for y in (-9, 9):
        rv.sphere(_np((10.5 * k, y * k, nk - 4 * k)), 1.1 * k)
    tank = D.g("Torso", "Extra")
    tank.cap(_np((-13 * k, 5 * k, sp + 6 * k)), _np((-13 * k, 5 * k, nk - 2 * k)), 3.6 * k)
    tank.cap(_np((-13 * k, -5 * k, sp + 6 * k)), _np((-13 * k, -5 * k, nk - 2 * k)), 3.6 * k)
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Accent", k=2 * k)
    pv.ell(_np((0, 0, pz - 2 * k)), (9.5 * k, 12 * k, 7 * k))
    for s in (1, -1):
        sd = side(s)
        sh = D.J("shoulder", s)
        el = D.J("elbow", s)
        U = D.g("UpperArm_" + sd, "Accent")
        U.sphere(sh + [0, 0, -1 * k], 5.6 * k)
        U.cap(sh + [0, 0, -2 * k], el + [0, 0, 1 * k], 3.6 * k, 3.4 * k)
        F = D.g("LowerArm_" + sd, "Body")
        F.cap(el, el + [0, 0, -16 * k], 4.6 * k, 5.2 * k)
        tag_band(D, "LowerArm_" + sd, el + [0, 0, -15 * k], 5.2 * k, 1.3 * k)
        if s > 0:
            mitten(D, s, el + [0, 0, -17.5 * k], slot="Accent", size=1.5, fingers=3)
        else:
            tor = D.g("LowerArm_" + sd, "Extra")
            w = el + [0, 0, -17.5 * k]
            tor.cap(w, w + [3 * k, 0, -8 * k], 3.2 * k, 2.4 * k)
            tor.cone(w + [4.6 * k, 0, -11.5 * k], 2.2 * k, 1.1 * k, 2.2 * k, axis=(0.35, 0, -1))
            fl = D.g("LowerArm_" + sd, "Glow", voxel=0.2)
            fl.cone(w + [6.0 * k, 0, -15.5 * k], 1.2 * k, 0.2 * k, 2.4 * k, axis=(0.35, 0, -1))
            D.socket("LowerArm_" + sd, "hand", w + [1.0 * k, -s * 3.0 * k, -5 * k])
        leg_standard(D, s, pants="Accent", shin="Body", th_r=(5.0, 4.4), sh_r=(5.8, 6.2), band=False, calf=0)
        tag_band(D, "Shin_" + sd, D.J("ankle", s) + [0, 0, 4.0 * k], 6.0 * k, 1.3 * k)
        foot_mech(D, s, slot="Accent", sole="Dark", glow="Glow", length=0.95, width=1.15)


# ================================================================================================================
# Keibi - L wall, retired security robot: rounded police-box body, siren cap, white face plate, shield bracers
# ================================================================================================================
@recipe("Keibi", lambda E, W: skel(E, leg=0.86, neck_drop=12.0, shoulder_y=25.0, hip_y=12.0, torso_back=0.0))
def keibi(D):
    k = D.k
    E = D.E
    hc = _np((1.5 * k, 0, E + 1.0 * k))
    hd = D.g("Head", "Accent", k=2 * k)
    hd.box(hc, (11 * k, 12 * k, 10.5 * k), 6 * k)
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 8 * k], 5.0 * k)
    bean_eyes(D, cx=hc[0], cz=hc[2], rx=11 * k, ry=12 * k, rz=10.5 * k, y=5.0 * k, z=E, size=1.6, slot="Dark",
              brow="Dark", shell=hd)
    mx = (hd.surf_x(0, E - 6 * k) or hc[0] + 11 * k)
    smile(D, x=mx - 0.1 * k, z=E - 6 * k, slot="Dark")
    cap = D.g("Head", "Body", k=1.5 * k)
    cap.cyl(hc + [-0.5 * k, 0, 9.5 * k], 12.5 * k, 3.4 * k, 2.0 * k)
    brim = D.g("Head", "Dark", voxel=0.3)
    brim.box(hc + [10 * k, 0, 6.6 * k], (5 * k, 10 * k, 0.6 * k), 0.5 * k, rot=(0, -8, 0))
    badge = D.g("Head", "Extra", voxel=0.25)
    badge.cyl(hc + [12.3 * k, 0, 10 * k], 2.0 * k, 0.4 * k, 0.3 * k, axis=(1, 0, 0))
    sir_r = D.g("Head", "Glow", voxel=0.3)
    sir_r.sphere(hc + [-0.5 * k, 3.2 * k, 13.5 * k], 3.2 * k)
    sir_r.half(hc + [0, 0, 13 * k], (0, 0, -1), op="i")
    sir_b = D.g("Head", "Extra", voxel=0.3)
    sir_b.sphere(hc + [-0.5 * k, -3.2 * k, 13.5 * k], 3.2 * k)
    sir_b.half(hc + [0, 0, 13 * k], (0, 0, -1), op="i")
    D.socket("Head", "eyes", _np(((hd.surf_x(0, E) or hc[0] + 11 * k) - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    t = D.g("Torso", "Body", k=4 * k)
    t.box(_np((0.5 * k, 0, (sp + nk) * 0.5)), (13 * k, 19 * k, (nk - sp) * 0.52), 9 * k)
    band = D.g("Torso", "Accent", voxel=0.35)
    band.box(_np((0.5 * k, 0, nk - 9 * k)), (13.4 * k, 19.4 * k, 2.4 * k), 3 * k)
    win = D.g("Torso", "Extra", voxel=0.3)
    for y in (-8, 0, 8):
        win.box(_np((13.6 * k, y * k, sp + 18 * k)), (0.6 * k, 3.0 * k, 4.4 * k), 0.8 * k)
    wh = D.g("Torso", "Metal", voxel=0.25)
    wh.torus(_np((10 * k, 0, nk - 2 * k)), 8.5 * k, 0.4 * k, axis=(0.6, 0, 1))
    wh.cap(_np((13.5 * k, -3 * k, sp + 26 * k)), _np((14 * k, -6 * k, sp + 24 * k)), 1.0 * k)
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Body", k=3 * k)
    pv.box(_np((0, 0, pz - 2 * k)), (12 * k, 17 * k, 8 * k), 5 * k)
    bl = D.g("Pelvis", "Dark", voxel=0.35)
    bl.torus(_np((0, 0, pz + 4 * k)), 13 * k, 1.5 * k, sx=1.0, sy=1.35)
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Dark", ua_r=(6.4, 5.8), fa_r=(5.8, 5.2),
                     hand_size=1.6)
        sh_ = D.g("LowerArm_" + sd, "Accent", k=1 * k)
        el = D.J("elbow", s)
        sh_.box(el + [0, s * 5.2 * k, -10 * k], (5.5 * k, 1.4 * k, 8 * k), 1.2 * k)
        sh_.cone(el + [0, s * 5.2 * k, -18.5 * k], 4.5 * k, 0.6 * k, 2.4 * k, axis=(0, 0, -1))
        leg_standard(D, s, pants="Body", shin="Body", th_r=(8.5, 7.5), sh_r=(7.5, 6.6))
        foot_mech(D, s, slot="Dark", sole="Accent", glow="Glow", length=1.05, width=1.2)


# ================================================================================================================
# Iwakura - M wall, security chief in a bulky powered suit with a tall tower shield on the left forearm
# ================================================================================================================
@recipe("Iwakura", lambda E, W: skel(E, leg=0.9, shoulder_y=18.6, hip_y=10.0))
def iwakura(D):
    k = D.k
    E = D.E
    face = human_head(D, brow="Dark", eye_size=1.3, lid="Skin", ears=False)
    CX, cz, RX, RY, RZ = face
    hel = D.g("Head", "Body", k=1.5 * k)
    hel.ell(_np((CX - 1.2 * k, 0, cz + 1 * k)), (RX * 1.1, RY * 1.12, RZ * 1.1))
    hel.half(_np((CX + RX * 0.3, 0, cz - 2 * k)), (-1.0, 0, 0.75), op="s", k=1.0 * k)
    ch = D.g("Head", "Accent", k=1 * k)
    for s in (1, -1):
        ch.ell(_np((CX + 2 * k, s * RY * 1.0, cz - 6 * k)), (6 * k, 2.4 * k, 6.5 * k))
    vis = D.g("Head", "Glow", voxel=0.3)
    vis.ell(_np((CX + RX * 0.45, 0, cz + RZ * 0.62)), (5.5 * k, 9 * k, 2.2 * k), rot=(0, -25, 0))
    ant = D.g("Head", "Metal", voxel=0.25)
    ant.cap(_np((CX - 6 * k, -RY * 0.9, cz + 6 * k)), _np((CX - 9 * k, -RY * 1.05, cz + 22 * k)), 0.6 * k)
    torso_human(D, slot="Body", chest=1.12, belly=1.08, depth=1.12)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    pl = D.g("Torso", "Accent", k=1.5 * k)
    pl.box(_np((10.5 * k, 0, sp + 30 * k)), (2.6 * k, 10 * k, 8 * k), 2.6 * k)
    em = D.g("Torso", "Glow", voxel=0.25)
    em.cyl(_np((13.2 * k, 0, sp + 30 * k)), 2.4 * k, 0.4 * k, 0.3 * k, axis=(1, 0, 0))
    pack = backpack(D, "Extra", half=(4, 9, 11), straps=None)
    pelvis_shorts(D, slot="Extra", belt="Accent")
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Extra", ua_r=(6.2, 5.8), fa_r=(5.8, 5.2),
                     hand_size=1.55)
        pauldron(D, s, "Accent", size=1.05, trim="Body")
        leg_standard(D, s, pants="Extra", shin="Body", knee_pad="Accent", th_r=(8.6, 7.4), sh_r=(7.2, 6.2))
        shoe(D, s, upper="Extra", sole="Dark", toe="Accent", boot=7.0, width=1.3)
    # tower shield in front of the left forearm
    el = D.J("elbow", 1)
    sh = D.g("LowerArm_L", "Accent", k=1 * k)
    c = el + [8.5 * k, 2.0 * k, -12 * k]
    sh.box(c, (1.3 * k, 12.5 * k, 22 * k), 2.5 * k)
    rim = D.g("LowerArm_L", "Body", voxel=0.35)
    rim.box(c + [1.1 * k, 0, 0], (0.7 * k, 13.1 * k, 22.6 * k), 2.6 * k)
    rim.box(c + [1.4 * k, 0, 0], (0.9 * k, 11.2 * k, 20.8 * k), 2.0 * k, op="s", k=0.3 * k)
    mk = D.g("LowerArm_L", "Glow", voxel=0.25)
    mk.box(c + [1.6 * k, 0, 4 * k], (0.3 * k, 1.2 * k, 7 * k), 0.4 * k)
    mk.box(c + [1.6 * k, 0, 4 * k], (0.3 * k, 6 * k, 1.2 * k), 0.4 * k)


# ================================================================================================================
# Kawara - M wall, roof-tile robot: tile-scale armour, onigawara demon-tile mask, ridge crest
# ================================================================================================================
@recipe("Kawara", lambda E, W: skel(E, leg=0.88, shoulder_y=18.8, hip_y=10.0, torso_back=0.0))
def kawara(D):
    k = D.k
    E = D.E
    hc = _np((1.5 * k, 0, E + 1.5 * k))
    hd = D.g("Head", "Body", k=2 * k)
    hd.box(hc, (11.5 * k, 12.5 * k, 11.5 * k), 6.5 * k)
    for s in (1, -1):
        hd.cone(hc + [-1 * k, s * 9 * k, 13 * k], 2.8 * k, 0.6 * k, 4.5 * k, axis=(0, s * 0.35, 1))   # horns
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 8 * k], 5.0 * k)
    crest = D.g("Head", "Accent", k=1 * k)
    crest.box(hc + [-2 * k, 0, 12.5 * k], (12 * k, 2.2 * k, 2.4 * k), 1.8 * k)
    crest.sphere(hc + [10.5 * k, 0, 13 * k], 3.0 * k)
    crest.sphere(hc + [-14.5 * k, 0, 13 * k], 3.0 * k)
    fx = hd.surf_x(0, E) or hc[0] + 11.5 * k
    eye = D.g("Head", "Glow", voxel=0.22)
    for s in (1, -1):
        eye.ell(_np((fx + 0.1 * k, s * 5.0 * k, E)), (1.0 * k, 2.6 * k, 1.8 * k), rot=(s * 12, 0, 0))
    brow = D.g("Head", "Accent", voxel=0.25)
    for s in (1, -1):
        brow.cap(_np((fx + 0.6 * k, s * 1.8 * k, E + 2.4 * k)), _np((fx - 0.1 * k, s * 8.2 * k, E + 4.6 * k)), 1.1 * k)
    mouth = D.g("Head", "Dark", voxel=0.25)
    mouth.box(_np((fx + 0.2 * k, 0, E - 6.5 * k)), (0.8 * k, 6.0 * k, 1.8 * k), 1.2 * k)
    fang = D.g("Head", "Extra", voxel=0.2)
    for s in (1, -1):
        fang.cone(_np((fx + 0.7 * k, s * 3.6 * k, E - 5.8 * k)), 0.9 * k, 0.2 * k, 1.3 * k, axis=(0, 0, -1))
    D.socket("Head", "eyes", _np((fx - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    t = D.g("Torso", "Body", k=4 * k)
    t.box(_np((0.5 * k, 0, (sp + nk) * 0.5)), (12 * k, 15 * k, (nk - sp) * 0.52), 8 * k)
    rid = D.g("Torso", "Accent", voxel=0.35)
    for i in range(5):
        z = sp + 6 * k + i * (nk - sp - 12 * k) / 4.0
        rid.torus(_np((0.5 * k, 0, z)), 12.5 * k, 1.0 * k, sx=1.0, sy=1.24)
    vm = D.g("Torso", "Glow", voxel=0.25)
    vm.sphere(_np((12.4 * k, 0, nk - 6 * k)), 2.2 * k)
    coat_skirt(D, "Body", length=20, flare=1.12, split=True, hem="Accent")
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Extra", hand="Dark", ua_r=(6.2, 5.6), fa_r=(5.6, 5.0),
                     hand_size=1.55)
        pd = pauldron(D, s, "Body", size=1.05, trim="Accent")
        tr = D.g("UpperArm_" + sd, "Accent", voxel=0.35)
        tr.torus(D.J("shoulder", s) + [0, s * 1.6 * k, 1.5 * k], 6.5 * k, 0.8 * k, sx=1.04, sy=1.0)
        leg_standard(D, s, pants="Accent", shin="Body", th_r=(8.4, 7.2), sh_r=(7.0, 6.0))
        foot_mech(D, s, slot="Body", sole="Dark", glow="Glow", length=1.0, width=1.15)


# ================================================================================================================
# Amagi - M scorer, porcelain prototype android: doll joints, smooth face with one visor line, floating halo
# ================================================================================================================
@recipe("Amagi", lambda E, W: skel(E, leg=1.02, shoulder_y=17.4, hip_y=8.2))
def amagi(D):
    k = D.k
    E = D.E
    hc = _np((1.0 * k, 0, E + 2.5 * k))
    hd = D.g("Head", "Body", k=3 * k)
    hd.ell(hc, (12.5 * k, 11.2 * k, 14.8 * k))
    hd.ell(hc + [4.5 * k, 0, -8.5 * k], (6.5 * k, 6.0 * k, 5.0 * k), k=3 * k)
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 10 * k], 3.6 * k)
    fx = hd.surf_x(0, E) or hc[0] + 12 * k
    vis = D.g("Head", "Glow", voxel=0.2)
    vis.torus(_np((hc[0] + 0.2 * k, 0, E)), 12.0 * k, 0.55 * k, sx=1.0, sy=0.93)
    vis.half(_np((hc[0] + 4 * k, 0, 0)), (-1, 0, 0), op="i")
    ear = D.g("Head", "Accent", voxel=0.3)
    for s in (1, -1):
        ear.cyl(_np((hc[0] - 0.5 * k, s * 11.0 * k, E)), 3.0 * k, 0.9 * k, 0.5 * k, axis=(0, 1, 0))
    seam = D.g("Head", "Accent", voxel=0.25)
    seam.torus(hc + [0, 0, 6.5 * k], 10.0 * k, 0.35 * k, axis=(0.35, 0, 1), sx=1.1, sy=0.98)
    halo = D.g("Head", "Glow", voxel=0.25)
    halo.torus(hc + [-15 * k, 0, 8 * k], 9.5 * k, 0.7 * k, axis=(1, 0, 0.2))
    D.socket("Head", "eyes", _np((fx - 1.0 * k, 0, E)))
    torso_human(D, slot="Body", chest=0.92, belly=0.85, depth=0.92, waist=0.85)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    core = D.g("Torso", "Glow", voxel=0.25)
    core.ell(_np((10.6 * k, 0, sp + 30 * k)), (0.6 * k, 2.4 * k, 2.4 * k))
    ln = D.g("Torso", "Accent", voxel=0.25)
    ln.torus(_np((0.2 * k, 0, sp + 14 * k)), 9.0 * k, 0.45 * k, sx=1.0, sy=1.22)
    pelvis_shorts(D, slot="Accent", belt=None, w=0.95)
    for s in (1, -1):
        sd = side(s)
        sh = D.J("shoulder", s)
        el = D.J("elbow", s)
        U = D.g("UpperArm_" + sd, "Body")
        U.cap(sh + [0, 0, -3.5 * k], el + [0, 0, 1.5 * k], 4.6 * k, 4.2 * k)
        J = D.g("UpperArm_" + sd, "Accent", voxel=0.3)
        J.sphere(sh + [0, 0, -0.5 * k], 4.6 * k)
        F = D.g("LowerArm_" + sd, "Body")
        F.cap(el + [0, 0, -2.5 * k], el + [0, 0, -21 * k], 4.2 * k, 3.6 * k)
        EJ = D.g("LowerArm_" + sd, "Accent", voxel=0.3)
        EJ.sphere(el + [0, 0, -0.5 * k], 3.9 * k)
        tag_band(D, "LowerArm_" + sd, el + [0, 0, -20.2 * k], 3.9 * k, 1.2 * k)
        mitten(D, s, el + [0, 0, -22.4 * k], slot="Body", size=1.4, fingers=4)
        hp = D.J("hip", s)
        kn = D.J("knee", s)
        an = D.J("ankle", s)
        T = D.g("Thigh_" + sd, "Body")
        T.cap(hp + [0, 0, -1 * k], kn + [0, 0, 2.5 * k], 6.4 * k, 5.2 * k)
        S = D.g("Shin_" + sd, "Body")
        S.cap(kn + [0, 0, -2.2 * k], an + [0, 0, 2.5 * k], 5.2 * k, 4.2 * k)
        KJ = D.g("Shin_" + sd, "Accent", voxel=0.3)
        KJ.sphere(kn, 4.8 * k)
        tag_band(D, "Shin_" + sd, an + [0, 0, 4.0 * k], 4.5 * k, 1.25 * k)
        shoe(D, s, upper="Body", sole="Accent", toe="Glow", length=1.1, width=1.05, height=1.0)


# ================================================================================================================
# Tsurube - XL scorer, former crane: lattice-boom arms, hook hand, little operator-cabin head with lit windows
# ================================================================================================================
@recipe("Tsurube", lambda E, W: skel(E, leg=1.0, neck_drop=10.0, shoulder_y=31.5, hip_y=12.0, upper=33.0,
                                     shoulder_drop=4.0, torso_back=0.0))
def tsurube(D):
    k = D.k
    E = D.E
    hc = _np((2.5 * k, 0, E + 1.0 * k))
    hd = D.g("Head", "Body", k=1.5 * k)
    hd.box(hc, (8.5 * k, 8.0 * k, 7.5 * k), 2.5 * k)
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 6 * k], 3.6 * k)
    roof = D.g("Head", "Accent", voxel=0.35)
    roof.box(hc + [0, 0, 7.8 * k], (9.2 * k, 8.8 * k, 1.0 * k), 0.8 * k)
    win = D.g("Head", "White", voxel=0.25)
    fx = hc[0] + 8.5 * k
    win.box(_np((fx, 0, E + 0.5 * k)), (0.5 * k, 6.2 * k, 3.4 * k), 1.0 * k)
    for s in (1, -1):
        win.box(hc + [0, s * 8.0 * k, -0.5 * k], (5.0 * k, 0.5 * k, 3.0 * k), 1.0 * k)
    pu = D.g("Head", "Dark", voxel=0.2)
    for s in (1, -1):
        pu.ell(_np((fx + 0.4 * k, s * 2.8 * k, E)), (0.3 * k, 1.1 * k, 1.6 * k))
    bc = D.g("Head", "Glow", voxel=0.25)
    bc.sphere(hc + [-2 * k, 0, 10.5 * k], 2.0 * k)
    D.socket("Head", "eyes", _np((fx - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    t = D.g("Torso", "Body", k=3 * k)
    t.box(_np((0, 0, (sp + nk) * 0.5)), (10 * k, 13.5 * k, (nk - sp) * 0.52), 5 * k)
    stp = D.g("Torso", "Accent", voxel=0.3)
    for i in range(4):
        z = sp + 8 * k + i * 10 * k
        stp.box(_np((10.3 * k, 0, z)), (0.6 * k, 12.5 * k, 1.6 * k), 0.4 * k, rot=(35, 0, 0))
    cw = D.g("Torso", "Extra", k=1.5 * k)
    cw.box(_np((-14 * k, 0, nk - 10 * k)), (5 * k, 12 * k, 9 * k), 3 * k)
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Accent", k=2 * k)
    pv.box(_np((0, 0, pz - 2 * k)), (9 * k, 13 * k, 7 * k), 4 * k)
    for s in (1, -1):
        sd = side(s)
        sh = D.J("shoulder", s)
        el = D.J("elbow", s)
        U = D.g("UpperArm_" + sd, "Body")
        U.sphere(sh + [0, 0, -1 * k], 5.4 * k)
        U.box((sh + el) * 0.5, (3.6 * k, 3.6 * k, (sh[2] - el[2]) * 0.5), 1.4 * k)
        lat = D.g("UpperArm_" + sd, "Accent", voxel=0.25)
        n = 5
        for i in range(n):
            za = sh[2] - 3 * k - i * (sh[2] - el[2] - 6 * k) / n
            zb = za - (sh[2] - el[2] - 6 * k) / n
            lat.cap(_np((sh[0] + 3.7 * k, sh[1] - 3.0 * k, za)), _np((sh[0] + 3.7 * k, sh[1] + 3.0 * k, zb)),
                    0.45 * k)
        F = D.g("LowerArm_" + sd, "Body")
        F.box(el + [0, 0, -14 * k], (3.4 * k, 3.4 * k, 14 * k), 1.3 * k)
        lat2 = D.g("LowerArm_" + sd, "Accent", voxel=0.25)
        for i in range(4):
            za = el[2] - 2 * k - i * 6.5 * k
            lat2.cap(_np((el[0] + 3.5 * k, el[1] - 2.8 * k, za)), _np((el[0] + 3.5 * k, el[1] + 2.8 * k, za - 6.5 * k)),
                     0.42 * k)
        tag_band(D, "LowerArm_" + sd, el + [0, 0, -26 * k], 4.4 * k, 1.3 * k)
        w = el + [0, 0, -28.5 * k]
        if s < 0:
            hk = D.g("LowerArm_" + sd, "Extra", voxel=0.35)
            hk.cyl(w + [0, 0, -2 * k], 1.4 * k, 3 * k, 0.4 * k)
            hk.torus(w + [0, 0, -10 * k], 4.5 * k, 1.5 * k, axis=(0, 1, 0))
            hk.box(w + [0, 0, -5 * k], (6 * k, 6 * k, 1.0 * k), 0, op="s", k=0)
            hk.half(w + [2.0 * k, 0, -10 * k], (1, 0, 0.5), op="s", k=0.5 * k)
            D.socket("LowerArm_" + sd, "hand", w + [0, -s * 1.5 * k, -6 * k])
        else:
            mitten(D, s, w, slot="Accent", size=1.5, fingers=3)
        leg_standard(D, s, pants="Accent", shin="Body", th_r=(6.6, 5.8), sh_r=(6.4, 5.8), calf=0.6)
        foot_mech(D, s, slot="Accent", sole="Extra", glow="Glow", length=1.1, width=1.1, claws=3)


# ================================================================================================================
# Aoji - XL wall, old floodgate machine: sluice-gate chest, valve wheels on the shoulders, moss, glowing water
# ================================================================================================================
@recipe("Aoji", lambda E, W: skel(E, leg=0.72, neck_drop=9.0, shoulder_y=35.0, hip_y=16.0, upper=24.0,
                                  shoulder_drop=8.0, torso_back=0.0))
def aoji(D):
    k = D.k
    E = D.E
    hc = _np((6.0 * k, 0, E))
    hd = D.g("Head", "Extra", k=1.5 * k)
    hd.sphere(hc, 8.5 * k)
    hd.half(hc + [0, 0, -4 * k], (0, 0, -1), op="i", k=1 * k)
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 3 * k], 5.0 * k)
    fx = hd.surf_x(0, E) or hc[0] + 8 * k
    for s in (1, -1):
        rim = D.g("Head", "Accent", voxel=0.3)
        rim.torus(_np((hc[0] + 6.6 * k, s * 3.6 * k, E)), 2.3 * k, 0.7 * k, axis=(1, 0, 0))
        port = D.g("Head", "Glow", voxel=0.25)
        port.sphere(_np((hc[0] + 6.0 * k, s * 3.6 * k, E)), 2.0 * k)
    D.socket("Head", "eyes", _np((fx - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    t = D.g("Torso", "Body", k=6 * k)
    t.box(_np((0, 0, (sp + nk) * 0.5)), (16 * k, 23 * k, (nk - sp) * 0.55), 10 * k)
    moss = D.g("Torso", "Hair", k=2 * k)
    moss.bumpy(sdf_ell(_np((-2 * k, 0, nk + 1 * k)) - D.O["Torso"], (14 * k, 22 * k, 5 * k)), 1.2 * k, 0.25 / k)
    gate = D.g("Torso", "Accent", k=1.5 * k)
    gate.box(_np((15 * k, 0, (sp + nk) * 0.5)), (2.2 * k, 12 * k, (nk - sp) * 0.38), 1.6 * k)
    rails = D.g("Torso", "Extra", voxel=0.35)
    for s in (1, -1):
        rails.box(_np((16 * k, s * 13.5 * k, (sp + nk) * 0.5)), (1.4 * k, 1.4 * k, (nk - sp) * 0.45), 0.6 * k)
    for z in (0.35, 0.55, 0.75):
        rails.box(_np((17 * k, 0, sp + (nk - sp) * z)), (0.7 * k, 11 * k, 0.7 * k), 0.3 * k)
    water = D.g("Torso", "Glow", k=1 * k, voxel=0.3)
    for y in (-7, 0, 7):
        water.cap(_np((17.8 * k, y * k, sp + (nk - sp) * 0.3)), _np((18.6 * k, y * k, sp + 2 * k)), 1.0 * k, 1.6 * k)
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Accent", k=3 * k)
    pv.box(_np((0, 0, pz - 2 * k)), (13 * k, 19 * k, 8 * k), 5 * k)
    for s in (1, -1):
        sd = side(s)
        sh = D.J("shoulder", s)
        el = D.J("elbow", s)
        U = D.g("UpperArm_" + sd, "Accent")
        U.sphere(sh + [0, 0, -1 * k], 9.5 * k)
        U.cap(sh + [0, 0, -3 * k], el + [0, 0, 2 * k], 7.4 * k, 6.8 * k)
        vw = D.g("UpperArm_" + sd, "Extra", voxel=0.35)
        c = sh + [0, s * 9.8 * k, 2 * k]
        vw.torus(c, 6.0 * k, 1.1 * k, axis=(0, 1, 0))
        vw.box(c, (5.5 * k, 0.8 * k, 0.8 * k), 0.3 * k)
        vw.box(c, (0.8 * k, 0.8 * k, 5.5 * k), 0.3 * k)
        F = D.g("LowerArm_" + sd, "Body", k=2.5 * k)
        F.cap(el + [0, 0, -1 * k], el + [0, 0, -15 * k], 7.6 * k, 9.0 * k)
        tag_band(D, "LowerArm_" + sd, el + [0, 0, -16.5 * k], 8.6 * k, 1.5 * k)
        fist = D.g("LowerArm_" + sd, "Accent", k=2 * k)
        fc = el + [1.5 * k, -s * 1.0 * k, -26 * k]
        fist.box(fc, (8.6 * k, 7.8 * k, 8.2 * k), 4.0 * k)
        D.socket("LowerArm_" + sd, "hand", fc + [0, -s * 7.8 * k, 0])
        leg_standard(D, s, pants="Accent", shin="Body", th_r=(9.5, 9.0), sh_r=(10.5, 10.0), band=False, calf=0)
        tag_band(D, "Shin_" + sd, D.J("ankle", s) + [0, 0, 5.5 * k], 10.2 * k, 1.5 * k)
        foot_mech(D, s, slot="Accent", sole="Dark", glow="Glow", length=1.05, width=1.4)


def sdf_ell(c, radii):
    from sdf import Ellipsoid
    return Ellipsoid(c, radii)


def _sel(c, radii, rot=None):
    from sdf import Ellipsoid, rot_matrix
    return Ellipsoid(c, radii, rot_matrix(*rot) if rot else None)


def _motes(D, part, slot, center, n, rad, size, seed=1.0, zspread=1.0):
    g = D.g(part, slot, voxel=0.25)
    for i in range(n):
        a = i * 2.39996 + seed
        r = rad * (0.75 + 0.25 * math.sin(i * 1.7 + seed))
        c = _np(center) + _np((math.cos(a) * r, math.sin(a) * r, math.sin(i * 2.3 + seed) * rad * 0.6 * zspread))
        g.sphere(c, size * (0.7 + 0.3 * math.cos(i * 1.3)))
    return g


# ================================================================================================================
# Kokemaru - XL wall, moss-and-mushroom giant: mossy boulder body, mushroom caps, root feet, glowing spores
# ================================================================================================================
@recipe("Kokemaru", lambda E, W: skel(E, leg=0.74, neck_drop=10.0, shoulder_y=31.8, hip_y=16.0, upper=24.0,
                                      shoulder_drop=7.0, torso_back=0.0))
def kokemaru(D):
    k = D.k
    E = D.E
    hc = _np((4.0 * k, 0, E - 1.0 * k))
    face = D.g("Head", "Accent", k=2.5 * k)
    face.ell(hc, (10.5 * k, 11.5 * k, 10 * k))
    face.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 6 * k], 6.0 * k)
    cap_ = D.g("Head", "Extra", k=2 * k)
    cap_.ell(hc + [-1.5 * k, 0, 9.5 * k], (19 * k, 20 * k, 9.5 * k))
    cap_.ell(hc + [-1.5 * k, 0, 4.5 * k], (17 * k, 18 * k, 5 * k), op="s", k=1.5 * k)
    spots = D.g("Head", "Glow", voxel=0.3)
    for a, r, dz in ((0, 9, 15.5), (70, 13, 13.5), (150, 11, 14.5), (220, 14, 12.5), (300, 10, 15.0), (0, 0, 18.4)):
        ar = math.radians(a)
        spots.sphere(hc + [-1.5 * k + math.cos(ar) * r * k, math.sin(ar) * r * k, dz * k], 2.0 * k)
    bean_eyes(D, cx=hc[0], cz=hc[2], rx=10.5 * k, ry=11.5 * k, rz=10 * k, y=4.4 * k, z=E, size=1.5, slot="Dark",
              brow=None, shell=face)
    smile(D, x=(face.surf_x(0, E - 5 * k) or hc[0] + 10 * k) - 0.1 * k, z=E - 5 * k, slot="Dark", size=1.3)
    D.socket("Head", "eyes", _np(((face.surf_x(0, E) or hc[0] + 10 * k) - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    body = D.g("Torso", "Accent", k=6 * k)
    body.ell(_np((1.0 * k, 0, (sp + nk) * 0.5)), (17 * k, 25 * k, (nk - sp) * 0.62))
    moss = D.g("Torso", "Body", k=3 * k)
    moss.bumpy(_sel(_np((-1.0 * k, 0, nk - 6 * k)) - D.O["Torso"], (18.5 * k, 26 * k, 14 * k)), 1.6 * k,
               0.22 / k, seed=0.7)
    moss.half(_np((0, 0, nk - 16 * k)), (0, 0, -1), op="i", k=4 * k)
    _motes(D, "Torso", "Glow", _np((0, 0, nk + 6 * k)), 7, 26 * k, 1.1 * k, seed=0.4)
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Accent", k=3 * k)
    pv.ell(_np((0, 0, pz - 2 * k)), (14 * k, 21 * k, 10 * k))
    for s in (1, -1):
        sd = side(s)
        sh = D.J("shoulder", s)
        el = D.J("elbow", s)
        U = D.g("UpperArm_" + sd, "Accent", k=3 * k)
        U.sphere(sh + [0, 0, -1 * k], 9 * k)
        U.cap(sh + [0, 0, -3 * k], el + [0, 0, 2 * k], 7.6 * k, 7.0 * k)
        mc = D.g("UpperArm_" + sd, "Extra", k=1.5 * k)
        mc.ell(sh + [0, s * 2 * k, 7.5 * k], (10 * k, 10 * k, 4.5 * k))
        mc.ell(sh + [0, s * 2 * k, 4.5 * k], (9 * k, 9 * k, 3 * k), op="s", k=1 * k)
        mst = D.g("UpperArm_" + sd, "Glow", voxel=0.3)
        mst.sphere(sh + [3 * k, s * 4 * k, 11.0 * k], 1.6 * k)
        mst.sphere(sh + [-4 * k, s * 1 * k, 11.5 * k], 1.4 * k)
        F = D.g("LowerArm_" + sd, "Accent", k=3 * k)
        F.cap(el + [0, 0, -1 * k], el + [0, 0, -15 * k], 7.4 * k, 8.4 * k)
        fm = D.g("LowerArm_" + sd, "Body", k=2 * k)
        fm.bumpy(_sel(el + [1.5 * k, s * 1.5 * k, -6 * k] - D.O["LowerArm_" + sd], (6 * k, 6 * k, 6 * k)), 0.9 * k,
                 0.4 / k, seed=s)
        tag_band(D, "LowerArm_" + sd, el + [0, 0, -16.5 * k], 8.2 * k, 1.5 * k)
        mitten(D, s, el + [0, 0, -18 * k], slot="Accent", size=2.2, fingers=3)
        leg_standard(D, s, pants="Accent", shin="Accent", th_r=(10, 9.5), sh_r=(10.5, 11), band=False, calf=0)
        tag_band(D, "Shin_" + sd, D.J("ankle", s) + [0, 0, 6.5 * k], 10.6 * k, 1.5 * k)
        an = D.J("ankle", s)
        ft = D.g("Foot_" + sd, "Accent", k=2.5 * k)
        ft.sphere(an, 8 * k)
        ft.ell(an + [4 * k, 0, -2.5 * k], (12 * k, 9 * k, 5 * k))
        for i, (dx, dy) in enumerate(((14, 5), (15, -4), (6, 9), (5, -9), (-6, 4))):
            ft.cap(an + [dx * 0.4 * k, dy * 0.4 * k, -3 * k], an + [dx * k, dy * k, -an[2] + 1.2 * k], 2.6 * k, 1.2 * k)
        D.socket("Foot_" + sd, "sole", (an[0] + 4 * k, an[1], 0.0))
        D.socket("Foot_" + sd, "toe", (an[0] + 16 * k, an[1], 0.0))


# ================================================================================================================
# Yadori - L wall, hermit-crab person carrying a wrecked boat hull, one big claw, eye stalks, barnacles
# ================================================================================================================
@recipe("Yadori", lambda E, W: skel(E, leg=0.8, neck_drop=12.0, shoulder_y=31.5, hip_y=13.0, torso_back=0.0))
def yadori(D):
    k = D.k
    E = D.E
    hc = _np((3.0 * k, 0, E - 7.0 * k))
    hd = D.g("Head", "Body", k=3 * k)
    hd.ell(hc, (11 * k, 12.5 * k, 8.5 * k))
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc, 6.0 * k)
    for s in (1, -1):
        hd.cap(hc + [3 * k, s * 4.5 * k, 4 * k], _np((hc[0] + 4.5 * k, s * 5.5 * k, E - 1.5 * k)), 1.6 * k, 1.4 * k,
               k=1 * k)
    ey = D.g("Head", "Dark", voxel=0.22)
    hl = D.g("Head", "White", voxel=0.18)
    for s in (1, -1):
        c = _np((hc[0] + 4.8 * k, s * 5.5 * k, E))
        ey.ell(c, (2.4 * k, 2.4 * k, 2.8 * k))
        hl.sphere(c + [2.0 * k, s * 0.4 * k, 1.0 * k], 0.7 * k)
    md = D.g("Head", "Extra", voxel=0.25)
    for s in (1, -1):
        md.cap(hc + [10 * k, s * 2.5 * k, -3 * k], hc + [12.5 * k, s * 1.0 * k, -6 * k], 1.0 * k, 0.5 * k)
    D.socket("Head", "eyes", _np((hc[0] + 6.5 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    t = D.g("Torso", "Body", k=4 * k)
    t.ell(_np((2.0 * k, 0, (sp + nk) * 0.5)), (12 * k, 18 * k, (nk - sp) * 0.58))
    seg = D.g("Torso", "Extra", voxel=0.35)
    for i in range(3):
        seg.torus(_np((2.0 * k, 0, sp + 10 * k + i * 9 * k)), 11.0 * k, 0.8 * k, sx=1.05, sy=1.6)
    hull = D.g("Torso", "Accent", k=2 * k)
    hc2 = _np((-17 * k, 0, (sp + nk) * 0.5 + 4 * k))
    hull.ell(hc2, (11 * k, 17 * k, 30 * k), rot=(0, -8, 0))
    hull.half(hc2 + [2 * k, 0, 0], (1, 0, 0), op="i", k=1 * k)
    hull.ell(hc2 + [1 * k, 0, 3 * k], (8.5 * k, 14 * k, 26 * k), op="s", k=1 * k)
    rail = D.g("Torso", "Extra", voxel=0.35)
    rail.cap(hc2 + [1 * k, 0, 29 * k], hc2 + [-4 * k, 0, 46 * k], 1.0 * k)
    rail.cap(hc2 + [-1 * k, -8 * k, 38 * k], hc2 + [-1 * k, 8 * k, 38 * k], 0.7 * k)
    for i, (y, z) in enumerate(((-10, -10), (8, -16), (-4, 4), (12, 10), (-12, 18))):
        rail.sphere(hc2 + [-9.5 * k, y * k, z * k], 1.8 * k)
    lan = D.g("Torso", "Glow", voxel=0.3)
    lan.sphere(hc2 + [-4 * k, 0, 38 * k], 2.4 * k)
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Body", k=3 * k)
    pv.ell(_np((1 * k, 0, pz - 2 * k)), (10 * k, 15 * k, 9 * k))
    for s in (1, -1):
        sd = side(s)
        sh = D.J("shoulder", s)
        el = D.J("elbow", s)
        U = D.g("UpperArm_" + sd, "Body")
        U.sphere(sh + [0, 0, -1 * k], 6.5 * k)
        U.cap(sh + [0, 0, -3 * k], el + [0, 0, 1 * k], 5.0 * k, 4.6 * k)
        F = D.g("LowerArm_" + sd, "Body")
        F.cap(el, el + [0, 0, -18 * k], 5.0 * k, 6.0 * k)
        tag_band(D, "LowerArm_" + sd, el + [0, 0, -18.5 * k], 6.0 * k, 1.3 * k)
        big = s < 0
        cl = D.g("LowerArm_" + sd, "Body", k=1.5 * k)
        w = el + [0, 0, -20 * k]
        sz = 1.6 if big else 0.9
        cl.ell(w + [2 * k, 0, -8 * k * sz], (6.5 * k * sz, 4.6 * k * sz, 8.5 * k * sz))
        cl.ell(w + [5.5 * k * sz, 0, -16 * k * sz], (2.6 * k * sz, 3.0 * k * sz, 7.0 * k * sz), rot=(0, 20, 0))
        cl.ell(w + [-1 * k * sz, 0, -15 * k * sz], (2.2 * k * sz, 2.6 * k * sz, 6.0 * k * sz), rot=(0, -25, 0))
        tipc = D.g("LowerArm_" + sd, "Extra", voxel=0.25)
        tipc.sphere(w + [7.0 * k * sz, 0, -21.5 * k * sz], 1.3 * k * sz)
        D.socket("LowerArm_" + sd, "hand", w + [2 * k, -s * 4.6 * k * sz, -8 * k * sz])
        leg_standard(D, s, pants="Body", shin="Body", th_r=(7.6, 6.8), sh_r=(6.6, 5.6), calf=0.5)
        sg = D.g("Shin_" + sd, "Extra", voxel=0.35)
        sg.torus(D.J("knee", s) + [0, 0, -8 * k], 6.6 * k, 0.8 * k)
        foot_mech(D, s, slot="Body", sole="Accent", glow="Glow", length=1.0, width=1.15, claws=3)


# ================================================================================================================
# Mugura - L scorer, electric-eel person: long body, fin crest, wide flat face with big round eyes, stripes
# ================================================================================================================
@recipe("Mugura", lambda E, W: skel(E, leg=0.95, neck_drop=20.0, shoulder_y=35.5, hip_y=12.0))
def mugura(D):
    k = D.k
    E = D.E
    hc = _np((4.0 * k, 0, E + 1.0 * k))
    hd = D.g("Head", "Body", k=4 * k)
    hd.ell(hc, (15 * k, 15.5 * k, 10.5 * k))
    hd.ell(hc + [-6 * k, 0, -10 * k], (9 * k, 10 * k, 12 * k))
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [6 * k, 0, 12 * k], 7.0 * k)
    bel = D.g("Head", "Extra", k=2 * k)
    bel.ell(hc + [6 * k, 0, -6.5 * k], (9 * k, 12 * k, 4 * k))
    bean_eyes(D, cx=hc[0], cz=hc[2], rx=15 * k, ry=15.5 * k, rz=10.5 * k, y=7.5 * k, z=E, size=1.9, slot="Dark",
              brow=None, shell=hd)
    smile(D, x=(hd.surf_x(0, E - 5 * k) or hc[0] + 14 * k) - 0.2 * k, y=4.5, z=E - 5 * k, slot="Dark", size=1.2)
    fin = D.g("Head", "Accent", k=1 * k)
    fin.ell(hc + [-6 * k, 0, 10 * k], (12 * k, 1.4 * k, 6 * k), rot=(0, 10, 0))
    D.socket("Head", "eyes", _np(((hd.surf_x(0, E) or hc[0] + 14 * k) - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    torso_human(D, slot="Body", chest=1.0, belly=1.05, depth=1.05)
    tb = D.g("Torso", "Extra", k=2 * k)
    tb.ell(_np((8.5 * k, 0, sp + 20 * k)), (4 * k, 11 * k, 20 * k))
    st = D.g("Torso", "Accent", voxel=0.3)
    for i, z in enumerate(range(4)):
        zz = sp + 8 * k + z * 10 * k
        st.torus(_np((-2 * k, 0, zz)), 13.5 * k, 0.9 * k, axis=(0.15, 0, 1), sx=0.95, sy=1.4)
        st.half(_np((4 * k, 0, 0)), (1, 0, 0), op="i") if False else None
    crest = D.g("Torso", "Accent", k=1 * k)
    crest.ell(_np((-12.5 * k, 0, (sp + nk) * 0.5)), (5 * k, 1.3 * k, (nk - sp) * 0.48))
    zap = D.g("Torso", "Glow", voxel=0.2)
    for i in range(4):
        a = _np((-13 * k, (-1) ** i * 2 * k, sp + 12 * k + i * 9 * k))
        zap.cap(a, a + [-3 * k, (-1) ** i * 2.5 * k, 3.5 * k], 0.35 * k)
    pelvis_shorts(D, slot="Body", belt="Accent")
    pz = D.J("pelvis")[2]
    tail = D.g("Pelvis", "Body", k=2 * k)
    tail.cap(_np((-9 * k, 0, pz - 3 * k)), _np((-22 * k, 0, pz - 18 * k)), 6 * k, 1.5 * k)
    tf = D.g("Pelvis", "Accent", k=1 * k)
    tf.ell(_np((-19 * k, 0, pz - 15 * k)), (6 * k, 1.0 * k, 4 * k), rot=(0, 45, 0))
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Extra", ua_r=(5.6, 5.0), fa_r=(5.0, 4.4),
                     hand_size=1.5)
        af = D.g("LowerArm_" + sd, "Accent", k=1 * k)
        el = D.J("elbow", s)
        af.ell(el + [-4.5 * k, s * 1 * k, -9 * k], (5 * k, 0.9 * k, 7 * k), rot=(0, -20, 0))
        leg_standard(D, s, pants="Body", shin="Body", th_r=(8.2, 7.0), sh_r=(6.6, 5.6))
        shoe(D, s, upper="Accent", sole="Dark", toe="Glow", length=1.25, width=1.3)


# ================================================================================================================
# Ankou - L wall, round anglerfish person: huge grinning mouth, tiny eyes, glowing lure on an antenna, fin hands
# ================================================================================================================
@recipe("Ankou", lambda E, W: skel(E, leg=0.76, neck_drop=13.0, shoulder_y=34.0, hip_y=13.0, torso_back=0.0))
def ankou(D):
    k = D.k
    E = D.E
    hc = _np((3.0 * k, 0, E - 4.0 * k))
    hd = D.g("Head", "Body", k=3 * k)
    hd.ell(hc, (16 * k, 17 * k, 14 * k))
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 6 * k], 8.0 * k)
    hd.box(hc + [13 * k, 0, -6 * k], (6 * k, 12 * k, 3.4 * k), 3.4 * k, op="s", k=1.5 * k)
    mouth = D.g("Head", "Dark", k=1 * k)
    mouth.box(hc + [11.5 * k, 0, -6 * k], (4 * k, 11.5 * k, 3.0 * k), 3.0 * k)
    teeth = D.g("Head", "White", voxel=0.22)
    for i in range(-4, 5):
        y = i * 2.6 * k
        teeth.cone(hc + [14.0 * k, y, -3.4 * k], 0.9 * k, 0.2 * k, 1.3 * k, axis=(0, 0, -1))
        if i % 2 == 0:
            teeth.cone(hc + [14.0 * k, y + 1.3 * k, -8.8 * k], 0.8 * k, 0.2 * k, 1.1 * k, axis=(0, 0, 1))
    for s in (1, -1):
        e = D.g("Head", "Dark", voxel=0.2)
        c = _np((hc[0] + 9.0 * k, s * 6.5 * k, E + 1.5 * k))
        fx = hd.surf_x(c[1], c[2]) or c[0]
        e.sphere(_np((fx - 0.4 * k, c[1], c[2])), 1.4 * k)
        h = D.g("Head", "White", voxel=0.16)
        h.sphere(_np((fx + 0.6 * k, c[1] + s * 0.3 * k, c[2] + 0.6 * k)), 0.45 * k)
    ant = D.g("Head", "Accent", voxel=0.3)
    a0 = hc + [0, 0, 13.5 * k]
    a1 = a0 + [6 * k, 0, 10 * k]
    a2 = a1 + [10 * k, 0, 1 * k]
    a3 = a2 + [5 * k, 0, -6 * k]
    ant.cap(a0, a1, 1.2 * k, 0.9 * k)
    ant.cap(a1, a2, 0.9 * k, 0.7 * k)
    ant.cap(a2, a3, 0.7 * k, 0.5 * k)
    lure = D.g("Head", "Glow", voxel=0.25)
    lure.sphere(a3 + [0, 0, -2.4 * k], 2.8 * k)
    spots = D.g("Head", "Accent", voxel=0.3)
    for (y, z) in ((-10, 6), (11, 4), (-6, 11), (7, 10)):
        spots.sphere(hc + [-6 * k, y * k, z * k], 1.6 * k)
    D.socket("Head", "eyes", _np(((hd.surf_x(0, E) or hc[0] + 15 * k) - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    body = D.g("Torso", "Body", k=6 * k)
    body.ell(_np((1.5 * k, 0, (sp + nk) * 0.5)), (16 * k, 25 * k, (nk - sp) * 0.62))
    bel = D.g("Torso", "Extra", k=2 * k)
    bel.ell(_np((10 * k, 0, (sp + nk) * 0.5 - 3 * k)), (8 * k, 15 * k, (nk - sp) * 0.45))
    dfin = D.g("Torso", "Accent", k=1 * k)
    dfin.ell(_np((-15 * k, 0, nk - 8 * k)), (6 * k, 1.4 * k, 9 * k), rot=(0, -25, 0))
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Body", k=3 * k)
    pv.ell(_np((1 * k, 0, pz - 1 * k)), (13 * k, 18 * k, 9 * k))
    for s in (1, -1):
        sd = side(s)
        sh = D.J("shoulder", s)
        el = D.J("elbow", s)
        U = D.g("UpperArm_" + sd, "Body")
        U.sphere(sh + [0, 0, -1 * k], 6.8 * k)
        U.cap(sh + [0, 0, -3 * k], el + [0, 0, 1 * k], 5.6 * k, 5.0 * k)
        F = D.g("LowerArm_" + sd, "Body")
        F.cap(el, el + [0, 0, -17 * k], 5.0 * k, 4.6 * k)
        tag_band(D, "LowerArm_" + sd, el + [0, 0, -17 * k], 4.9 * k, 1.3 * k)
        fn = D.g("LowerArm_" + sd, "Accent", k=1 * k)
        w = el + [0, 0, -19 * k]
        fn.ell(w + [1 * k, 0, -7 * k], (6.5 * k, 2.4 * k, 9 * k), rot=(0, -15, 0))
        for dx in (-3, 0, 3):
            fn.cap(w + [dx * k + 1 * k, 0, -12 * k], w + [dx * 1.5 * k + 1 * k, 0, -17 * k], 1.2 * k, 0.5 * k)
        D.socket("LowerArm_" + sd, "hand", w + [1 * k, -s * 2.6 * k, -8 * k])
        leg_standard(D, s, pants="Body", shin="Body", th_r=(8.6, 7.6), sh_r=(7.2, 6.2), calf=0.4)
        an = D.J("ankle", s)
        ft = D.g("Foot_" + sd, "Accent", k=2 * k)
        ft.sphere(an, 5.5 * k)
        ft.ell(an + [6 * k, 0, -an[2] * 0.55], (12 * k, 8 * k, 3.8 * k))
        ft.box(an + [6 * k, 0, -an[2] - 2 * k], (20 * k, 20 * k, 2 * k), 0, op="s", k=0)
        D.socket("Foot_" + sd, "sole", (an[0] + 6 * k, an[1], 0.0))
        D.socket("Foot_" + sd, "toe", (an[0] + 17.5 * k, an[1], 0.0))


# ================================================================================================================
# Hotaru - S playmaker, firefly swarm in a paper-lantern body, leaf cape, glowing motes
# ================================================================================================================
@recipe("Hotaru", lambda E, W: skel(E, leg=0.9, neck_drop=19.0, shoulder_y=13.0, hip_y=7.0))
def hotaru(D):
    k = D.k
    E = D.E
    hc = _np((1.0 * k, 0, E + 1.5 * k))
    hd = D.g("Head", "Jelly", k=2 * k)
    hd.ell(hc, (14 * k, 14 * k, 15.5 * k))
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 12 * k], 4.0 * k)
    rib = D.g("Head", "Body", voxel=0.3)
    for dz in (-9, -3, 3, 9):
        rr = 14 * k * math.sqrt(max(0.05, 1 - (dz * k / (15.5 * k)) ** 2))
        rib.torus(hc + [0, 0, dz * k], rr, 0.55 * k)
    rib.cyl(hc + [0, 0, 15.6 * k], 5 * k, 1.2 * k, 0.5 * k)
    rib.cyl(hc + [0, 0, -15.0 * k], 4.5 * k, 1.0 * k, 0.5 * k)
    core = D.g("Head", "Glow", voxel=0.3)
    core.sphere(hc + [-1 * k, 0, 0], 5.5 * k)
    bean_eyes(D, cx=hc[0], cz=hc[2], rx=14 * k, ry=14 * k, rz=15.5 * k, y=5.0 * k, z=E, size=1.6, slot="Dark",
              brow=None, shell=hd)
    lf = D.g("Head", "Extra", k=1 * k)
    lf.ell(hc + [-2 * k, 3 * k, 18.5 * k], (6 * k, 2.4 * k, 1.0 * k), rot=(20, 0, 30))
    lf.ell(hc + [-3 * k, -3 * k, 18 * k], (5 * k, 2.0 * k, 1.0 * k), rot=(-20, 0, -40))
    D.socket("Head", "eyes", _np(((hd.surf_x(0, E) or hc[0] + 13 * k) - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    t = D.g("Torso", "Jelly", k=3 * k)
    t.ell(_np((0.5 * k, 0, (sp + nk) * 0.5)), (9.5 * k, 11 * k, (nk - sp) * 0.56))
    trib = D.g("Torso", "Body", voxel=0.3)
    for f in (0.3, 0.55, 0.8):
        z = sp + (nk - sp) * f
        trib.torus(_np((0.5 * k, 0, z)), 10.5 * k, 0.5 * k, sx=0.92, sy=1.06)
    flies = _motes(D, "Torso", "Glow", _np((0.5 * k, 0, (sp + nk) * 0.5)), 6, 5.5 * k, 1.2 * k, seed=1.3)
    cape(D, "Extra", length=(nk - sp) * 1.25 / k, width=1.1, flare=1.0, tatter=4)
    _motes(D, "Torso", "Glow", _np((-4 * k, 0, nk + 2 * k)), 6, 22 * k, 0.9 * k, seed=2.1)
    pelvis_shorts(D, slot="Body", belt=None, w=0.95)
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Body", ua_r=(3.8, 3.4), fa_r=(3.4, 3.0),
                     hand_size=1.35)
        tip = D.g("LowerArm_" + sd, "Glow", voxel=0.2)
        tip.sphere(D.J("elbow", s) + [2 * k, -s * 1 * k, -38 * k], 1.0 * k)
        leg_standard(D, s, pants="Body", shin="Body", th_r=(4.8, 4.0), sh_r=(4.0, 3.4), calf=0.6)
        shoe(D, s, upper="Extra", sole="Body", toe="Accent", length=1.0, width=1.0, height=1.0)


# ================================================================================================================
# Tsubame - S playmaker, flock of swallow drones in a humanoid shape: parcel-box chest, wing tips, orbiting drones
# ================================================================================================================
def _swallow(g, c, dirn, sz, k):
    d = _np(dirn) / np.linalg.norm(_np(dirn))
    side_v = np.cross(d, [0, 0, 1])
    side_v = side_v / max(np.linalg.norm(side_v), 1e-6)
    g.ell(c, (2.6 * sz * k, 1.2 * sz * k, 1.1 * sz * k), rot=(0, 0, math.degrees(math.atan2(d[1], d[0]))))
    for sgn in (1, -1):
        g.cap(c, c + side_v * sgn * 3.6 * sz * k - d * 1.2 * sz * k, 0.6 * sz * k, 0.25 * sz * k)


@recipe("Tsubame", lambda E, W: skel(E, leg=0.95, neck_drop=19.0, shoulder_y=13.8, hip_y=7.2))
def tsubame(D):
    k = D.k
    E = D.E
    hc = _np((1.0 * k, 0, E + 2.0 * k))
    hd = D.g("Head", "Body", k=2 * k)
    hd.ell(hc, (14 * k, 13.5 * k, 14.5 * k))
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 11 * k], 3.8 * k)
    for s in (1, -1):
        hd.cone(hc + [-12 * k, s * 4 * k, 2 * k], 2.4 * k, 0.4 * k, 6 * k, axis=(-1, s * 0.5, -0.2))
    face = D.g("Head", "Accent", k=1.5 * k)
    face.ell(hc + [6.5 * k, 0, -2 * k], (8.5 * k, 10.5 * k, 9.5 * k))
    bean_eyes(D, cx=hc[0] + 6.5 * k, cz=hc[2] - 2 * k, rx=8.5 * k, ry=10.5 * k, rz=9.5 * k, y=4.6 * k, z=E, size=1.6,
              slot="Dark", brow=None, shell=face)
    beak = D.g("Head", "Glow", voxel=0.25)
    bx = face.surf_x(0, E - 3.5 * k) or hc[0] + 14 * k
    beak.cone(_np((bx + 0.8 * k, 0, E - 3.5 * k)), 1.4 * k, 0.3 * k, 1.4 * k, axis=(1, 0, -0.2))
    D.socket("Head", "eyes", _np(((face.surf_x(0, E) or hc[0] + 14 * k) - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    t = D.g("Torso", "Body", k=3 * k)
    t.ell(_np((0, 0, (sp + nk) * 0.5)), (9 * k, 11 * k, (nk - sp) * 0.55))
    box = D.g("Torso", "Accent", k=1 * k)
    box.box(_np((6.5 * k, 0, (sp + nk) * 0.5)), (5.5 * k, 9.5 * k, 9.5 * k), 1.2 * k)
    tape = D.g("Torso", "Glow", voxel=0.25)
    tape.box(_np((12.1 * k, 0, (sp + nk) * 0.5)), (0.25 * k, 1.4 * k, 9.6 * k), 0.2 * k)
    tape.box(_np((12.1 * k, 0, (sp + nk) * 0.5)), (0.25 * k, 9.6 * k, 1.4 * k), 0.2 * k)
    dr = D.g("Torso", "Body", voxel=0.25)
    for i, (ang, z, r) in enumerate(((30, nk + 4, 20), (140, sp + 20, 21), (250, nk - 6, 19), (320, sp + 8, 20))):
        a = math.radians(ang)
        c = _np((math.cos(a) * r * k, math.sin(a) * r * k, z * 1.0))
        _swallow(dr, c, (-math.sin(a), math.cos(a), 0), 1.2, k)
    pelvis_shorts(D, slot="Extra", belt="Accent")
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Accent", ua_r=(4.4, 4.0), fa_r=(3.8, 3.4),
                     hand_size=1.4)
        wing = D.g("UpperArm_" + sd, "Body", k=1 * k)
        sh = D.J("shoulder", s)
        el = D.J("elbow", s)
        wing.ell(sh + [-7 * k, s * 2.5 * k, -12 * k], (8 * k, 1.2 * k, 14 * k), rot=(0, -25, 0))
        wing.cone(sh + [-14 * k, s * 2.5 * k, -27 * k], 2.4 * k, 0.4 * k, 6 * k, axis=(-0.5, 0, -1))
        wt = D.g("UpperArm_" + sd, "Accent", voxel=0.25)
        wt.ell(sh + [-5 * k, s * 3.6 * k, -9 * k], (5 * k, 0.4 * k, 7 * k), rot=(0, -25, 0))
        leg_standard(D, s, pants="Body", shin="Extra", th_r=(5.2, 4.4), sh_r=(4.4, 3.8))
        foot_mech(D, s, slot="Accent", sole="Extra", glow="Glow", length=0.9, width=0.95, claws=3)


# ================================================================================================================
# Kazami - M playmaker, weather-station swarm: anemometer crown, weather-vane fins, cloud scarf, sun sensors
# ================================================================================================================
@recipe("Kazami", lambda E, W: skel(E, leg=0.98, shoulder_y=17.0, hip_y=8.4))
def kazami(D):
    k = D.k
    E = D.E
    hc = _np((1.0 * k, 0, E + 2.0 * k))
    hd = D.g("Head", "Accent", k=2.5 * k)
    hd.ell(hc, (13 * k, 12.5 * k, 13.5 * k))
    hd.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 10 * k], 3.8 * k)
    band = D.g("Head", "Extra", k=1 * k)
    band.torus(_np((hc[0], 0, E)), 12.6 * k, 2.4 * k, sx=1.03, sy=0.97)
    band.half(_np((hc[0] + 2 * k, 0, 0)), (-1, 0, 0), op="i", k=1 * k)
    ey = D.g("Head", "Glow", voxel=0.2)
    for s in (1, -1):
        fx = hd.surf_x(s * 4.6 * k, E) or hc[0] + 12 * k
        ey.sphere(_np((fx + 1.6 * k, s * 4.6 * k, E)), 1.5 * k)
    post = D.g("Head", "Extra", voxel=0.3)
    post.cap(hc + [-1 * k, 0, 12 * k], hc + [-1 * k, 0, 21 * k], 0.8 * k)
    for i in range(3):
        a = math.radians(i * 120 + 15)
        tip = hc + [-1 * k + math.cos(a) * 7 * k, math.sin(a) * 7 * k, 21 * k]
        post.cap(hc + [-1 * k, 0, 21 * k], tip, 0.45 * k)
        cup = D.g("Head", "Glow", voxel=0.25)
        cup.sphere(tip, 1.8 * k)
        cup.sphere(tip + [math.cos(a + 1.57) * 1.0 * k, math.sin(a + 1.57) * 1.0 * k, 0], 1.5 * k, op="s", k=0.2 * k)
    D.socket("Head", "eyes", _np(((hd.surf_x(0, E) or hc[0] + 12 * k) - 1.0 * k, 0, E)))
    torso_human(D, slot="Body", chest=0.95, belly=0.95)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    pnl = D.g("Torso", "Accent", k=1 * k)
    pnl.box(_np((10.0 * k, 0, sp + 20 * k)), (2.0 * k, 7 * k, 9 * k), 2 * k)
    gl = D.g("Torso", "Glow", voxel=0.25)
    for z in (sp + 16 * k, sp + 21 * k, sp + 26 * k):
        gl.sphere(_np((12.1 * k, 0, z)), 1.0 * k)
    cloud = D.g("Torso", "Accent", k=2.5 * k)
    for i, (x, y, z, r) in enumerate(((2, 9, nk - 1, 6), (-4, 7, nk + 1, 6.5), (-8, 0, nk, 6), (-4, -8, nk + 1, 6.5),
                                      (3, -10, nk - 2, 5.5), (-14, 4, nk - 8, 6), (-18, -3, nk - 16, 5.5),
                                      (-20, 3, nk - 25, 4.5))):
        cloud.sphere(_np((x * k, y * k, z if isinstance(z, float) else z)), r * k)
    pelvis_shorts(D, slot="Extra", belt="Accent")
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Accent", ua_r=(5.0, 4.6), fa_r=(4.6, 4.0))
        vane = D.g("LowerArm_" + sd, "Extra", voxel=0.3)
        el = D.J("elbow", s)
        vane.box(el + [-5 * k, s * 1 * k, -10 * k], (4.2 * k, 0.6 * k, 5 * k), 0.4 * k, rot=(0, -15, 0))
        vane.cone(el + [5.5 * k, s * 0.5 * k, -10 * k], 2.4 * k, 0.4 * k, 2.2 * k, axis=(1, 0, 0))
        leg_standard(D, s, pants="Body", shin="Body", th_r=(6.8, 5.6), sh_r=(5.4, 4.4))
        shoe(D, s, upper="Accent", sole="Extra", toe="Glow", length=1.15)


# ================================================================================================================
# Tomoshibi - L playmaker, old cast-iron street lamp come alive: amber lantern head, scrolls, long cape
# ================================================================================================================
@recipe("Tomoshibi", lambda E, W: skel(E, leg=1.02, neck_drop=12.0, shoulder_y=22.0, hip_y=9.0, torso_back=0.0))
def tomoshibi(D):
    k = D.k
    E = D.E
    hc = _np((1.0 * k, 0, E + 1.0 * k))
    fr = D.g("Head", "Body", k=1 * k)
    fr.cyl(hc + [0, 0, -9 * k], 9.5 * k, 1.4 * k, 0.6 * k)
    fr.cone(hc + [0, 0, 12 * k], 12 * k, 1.5 * k, 3.5 * k)
    fr.cyl(hc + [0, 0, 8.4 * k], 10.5 * k, 1.0 * k, 0.4 * k)
    for i in range(6):
        a = math.radians(i * 60 + 30)
        fr.cap(hc + [math.cos(a) * 9 * k, math.sin(a) * 9 * k, -8 * k], hc + [math.cos(a) * 9.8 * k,
                                                                             math.sin(a) * 9.8 * k, 8 * k], 0.7 * k)
    fr.sphere(hc + [0, 0, 17 * k], 1.8 * k)
    fr.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 10 * k], 3.0 * k)
    glass = D.g("Head", "Glow", k=1.5 * k)
    glass.cyl(hc, 8.6 * k, 8.0 * k, 1.5 * k)
    pu = D.g("Head", "Extra", voxel=0.22)
    for s in (1, -1):
        pu.ell(_np((hc[0] + 9.0 * k, s * 3.6 * k, E)), (0.6 * k, 1.3 * k, 2.0 * k))
    scr = D.g("Head", "Body", voxel=0.3)
    for s in (1, -1):
        scr.torus(hc + [0, s * 12.5 * k, -6 * k], 3 * k, 0.6 * k, axis=(1, 0, 0))
    D.socket("Head", "eyes", _np((hc[0] + 8.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    t = D.g("Torso", "Body", k=3 * k)
    t.cap(_np((0, 0, sp + 2 * k)), _np((0, 0, nk - 3 * k)), 8.5 * k, 10 * k)
    t.ell(_np((0, 0, nk - 6 * k)), (10 * k, 15 * k, 6 * k))
    rings = D.g("Torso", "Accent", voxel=0.3)
    for f in (0.15, 0.5, 0.85):
        z = sp + (nk - sp) * f
        rings.torus(_np((0, 0, z)), 9.2 * k, 0.9 * k)
    cape(D, "Extra", length=(nk - sp) * 1.6 / k, width=1.05, flare=1.5, hem="Accent", tatter=0)
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Body", k=2 * k)
    pv.ell(_np((0, 0, pz - 1 * k)), (8.5 * k, 11.5 * k, 8 * k))
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Body", ua_r=(4.4, 4.0), fa_r=(4.0, 3.6),
                     hand_size=1.35, fingers=4)
        sc = D.g("UpperArm_" + sd, "Accent", voxel=0.3)
        sc.torus(D.J("shoulder", s) + [0, s * 5 * k, -10 * k], 2.6 * k, 0.55 * k, axis=(1, 0, 0))
        leg_standard(D, s, pants="Body", shin="Body", th_r=(5.4, 4.6), sh_r=(4.4, 3.8), calf=0.3)
        an = D.J("ankle", s)
        ft = D.g("Foot_" + sd, "Body", k=1.5 * k)
        ft.cone(an + [2 * k, 0, -an[2] * 0.5], 9 * k, 4.6 * k, an[2] * 0.5)
        ft.box(an + [2 * k, 0, -an[2] - 20 * k], (20 * k, 20 * k, 20 * k), 0, op="s", k=0)
        bt = D.g("Foot_" + sd, "Accent", voxel=0.3)
        bt.torus(an + [2 * k, 0, -an[2] + 1.6 * k], 8.6 * k, 0.8 * k)
        D.socket("Foot_" + sd, "sole", (an[0] + 2 * k, an[1], 0.0))
        D.socket("Foot_" + sd, "toe", (an[0] + 11 * k, an[1], 0.0))


# ================================================================================================================
# Yatai - XL playmaker, walking food stall: lacquer cart body, little tiled roof, noren, paper-lantern face
# ================================================================================================================
@recipe("Yatai", lambda E, W: skel(E, leg=0.72, neck_drop=14.0, shoulder_y=44.0, hip_y=18.0, upper=24.0,
                                   shoulder_drop=8.0, torso_back=0.0))
def yatai(D):
    k = D.k
    E = D.E
    hc = _np((3.0 * k, 0, E + 0.5 * k))
    lan = D.g("Head", "Glow", k=2 * k)
    lan.ell(hc, (11 * k, 11 * k, 13.5 * k))
    lan.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 11 * k], 4.0 * k)
    bands = D.g("Head", "Dark", voxel=0.3)
    bands.cyl(hc + [0, 0, 13.5 * k], 6.5 * k, 1.4 * k, 0.5 * k)
    bands.cyl(hc + [0, 0, -13.2 * k], 6.0 * k, 1.4 * k, 0.5 * k)
    for dz in (-7, 0, 7):
        rr = 11 * k * math.sqrt(max(0.05, 1 - (dz / 13.5) ** 2))
        bands.torus(hc + [0, 0, dz * k], rr, 0.45 * k)
    bean_eyes(D, cx=hc[0], cz=hc[2], rx=11 * k, ry=11 * k, rz=13.5 * k, y=4.5 * k, z=E, size=1.6, slot="Dark",
              brow="Dark", shell=lan)
    smile(D, x=(lan.surf_x(0, E - 5.5 * k) or hc[0] + 10 * k) - 0.1 * k, z=E - 5.5 * k, slot="Dark", size=1.3,
          open_=True)
    D.socket("Head", "eyes", _np(((lan.surf_x(0, E) or hc[0] + 10 * k) - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    cart = D.g("Torso", "Accent", k=2.5 * k)
    cart.box(_np((0, 0, (sp + nk) * 0.5 - 3 * k)), (16 * k, 30 * k, (nk - sp) * 0.45), 3 * k)
    lac = D.g("Torso", "Body", voxel=0.35)
    lac.box(_np((0, 0, nk - 6 * k)), (16.6 * k, 27.6 * k, 2.2 * k), 1.5 * k)
    lac.box(_np((0, 0, sp + 4 * k)), (16.6 * k, 27.6 * k, 2.2 * k), 1.5 * k)
    roof = D.g("Torso", "Body", k=1.5 * k)
    for s in (1, -1):
        roof.box(_np((-8 * k, s * 15 * k, nk + 16 * k)), (12 * k, 16 * k, 1.4 * k), 1.0 * k, rot=(s * 22, 0, 0))
    post = D.g("Torso", "Accent", voxel=0.35)
    for s in (1, -1):
        post.cap(_np((-16 * k, s * 25 * k, nk - 5 * k)), _np((-16 * k, s * 25 * k, nk + 14 * k)), 1.2 * k)
    noren = D.g("Torso", "Extra", k=0.5 * k)
    for i in range(-2, 3):
        noren.box(_np((16.8 * k, i * 9.5 * k, nk - 12 * k)), (0.6 * k, 4.4 * k, 7 * k), 0.5 * k)
    mark = D.g("Torso", "Body", voxel=0.25)
    mark.torus(_np((17.6 * k, 0, nk - 11 * k)), 2.4 * k, 0.6 * k, axis=(1, 0, 0))
    pot = D.g("Torso", "Dark", k=1 * k)
    pot.cyl(_np((-6 * k, 21 * k, nk + 0.5 * k)), 5 * k, 3.2 * k, 1.2 * k)
    steam = D.g("Torso", "White", k=2 * k, voxel=0.35)
    for i in range(3):
        steam.sphere(_np((-6 * k + i * 0.5 * k, 21 * k + (-1) ** i * 1.2 * k, nk + 6 * k + i * 4 * k)), (2.6 - i * 0.5) * k)
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Accent", k=2 * k)
    pv.box(_np((0, 0, pz - 2 * k)), (13 * k, 22 * k, 7 * k), 3 * k)
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Accent", hand="Extra", ua_r=(6.4, 6.0), fa_r=(6.0, 5.4),
                     hand_size=1.9)
        lad = D.g("LowerArm_" + sd, "Accent", voxel=0.3)
        w = D.J("elbow", s) + [0, 0, -22.2 * k]
        lad.cap(w + [6 * k, -s * 3 * k, -6 * k], w + [10 * k, -s * 3 * k, -28 * k], 0.9 * k)
        lad.sphere(w + [10.5 * k, -s * 3 * k, -30 * k], 3.0 * k)
        leg_standard(D, s, pants="Accent", shin="Accent", th_r=(7.0, 6.4), sh_r=(6.0, 5.6), calf=0)
        an = D.J("ankle", s)
        wh = D.g("Foot_" + sd, "Dark", k=1 * k)
        wh.cyl(an + [3 * k, 0, -an[2] * 0.35], an[2] * 0.64, 3.0 * k, 1.0 * k, axis=(0, 1, 0))
        hub = D.g("Foot_" + sd, "Body", voxel=0.3)
        hub.cyl(an + [3 * k, s * 3.2 * k, -an[2] * 0.35], 2.4 * k, 0.6 * k, 0.3 * k, axis=(0, 1, 0))
        ft = D.g("Foot_" + sd, "Accent", k=1.5 * k)
        ft.box(an + [6 * k, 0, -an[2] * 0.75], (13 * k, 6.5 * k, an[2] * 0.25), 1.2 * k)
        D.socket("Foot_" + sd, "sole", (an[0] + 5 * k, an[1], 0.0))
        D.socket("Foot_" + sd, "toe", (an[0] + 18 * k, an[1], 0.0))


# ================================================================================================================
# Oboro - M playmaker from Urakaze: ragged straw cloak, misty body around a lantern core, crescent-moon mask
# ================================================================================================================
@recipe("Oboro", lambda E, W: skel(E, leg=1.0, shoulder_y=17.6, hip_y=8.4))
def oboro(D):
    k = D.k
    E = D.E
    hc = _np((1.0 * k, 0, E + 1.5 * k))
    mist = D.g("Head", "Jelly", k=3 * k)
    mist.ell(hc + [-2 * k, 0, 0], (12.5 * k, 12 * k, 14 * k))
    mist.cap(_np((0, 0, D.J("neck")[2] - 2 * k)), hc - [0, 0, 10 * k], 5.0 * k)
    mask = D.g("Head", "White", k=1.5 * k)
    mask.ell(hc + [7.0 * k, 0, -1 * k], (5.5 * k, 9.5 * k, 12 * k))
    mask.ell(hc + [2.0 * k, 0, -1 * k], (5.5 * k, 9.0 * k, 11.5 * k), op="s", k=0.8 * k)
    fx = mask.surf_x(0, E) or hc[0] + 12 * k
    cres = D.g("Head", "Glow", voxel=0.2)
    cres.torus(_np((fx + 0.1 * k, 0, E + 6 * k)), 2.4 * k, 0.6 * k, axis=(1, 0, 0))
    cres.sphere(_np((fx + 0.1 * k, 0.9 * k, E + 6.6 * k)), 2.2 * k, op="s", k=0.2 * k)
    slit = D.g("Head", "Dark", voxel=0.2)
    for s in (1, -1):
        fxe = mask.surf_x(s * 4.0 * k, E) or fx
        slit.ell(_np((fxe + 0.1 * k, s * 4.0 * k, E)), (0.6 * k, 2.2 * k, 0.8 * k), rot=(s * 10, 0, 0))
    hood = D.g("Head", "Hair", k=2 * k)
    hood.ell(hc + [-3 * k, 0, 3 * k], (14 * k, 14 * k, 15.5 * k))
    hood.half(hc + [4 * k, 0, 0], (-1, 0, 0.3), op="s", k=1 * k)
    for i in range(7):
        a = math.radians(-150 + i * 50)
        hood.cone(hc + [math.cos(a) * 11 * k - 3 * k, math.sin(a) * 12 * k, -14 * k], 2.6 * k, 0.4 * k, 4 * k,
                  axis=(0, 0, -1), k=1 * k)
    D.socket("Head", "eyes", _np((fx - 1.0 * k, 0, E)))
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    core = D.g("Torso", "Extra", k=3 * k)
    core.ell(_np((0, 0, (sp + nk) * 0.5)), (8.5 * k, 11 * k, (nk - sp) * 0.5))
    m2 = D.g("Torso", "Jelly", k=4 * k)
    for (x, y, z, r) in ((3, 8, 0.75, 8), (2, -9, 0.7, 8), (-3, 0, 0.45, 9), (4, 0, 0.25, 8), (-4, 9, 0.2, 7),
                         (-4, -9, 0.3, 7)):
        m2.sphere(_np((x * k, y * k, sp + (nk - sp) * z)), r * k)
    lan = D.g("Torso", "Glow", voxel=0.3)
    lan.sphere(_np((4 * k, 0, sp + (nk - sp) * 0.55)), 4.0 * k)
    cl = cape(D, "Hair", length=(nk - sp) * 1.45 / k, width=1.1, flare=1.6, tatter=6)
    cl.ell(_np((2 * k, 0, nk - 6 * k)), (12 * k, 18 * k, 7 * k))
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Jelly", k=3 * k)
    pv.ell(_np((0, 0, pz - 2 * k)), (10 * k, 14 * k, 9 * k))
    trail = D.g("Pelvis", "Jelly", k=3 * k)
    for i in range(4):
        trail.sphere(_np((-8 * k - i * 5 * k, (-1) ** i * 3 * k, pz - 12 * k - i * 9 * k)), (6 - i) * k)
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Hair", forearm="Body", hand="Body", ua_r=(5.4, 5.0), fa_r=(4.0, 3.6),
                     hand_size=1.4)
        leg_standard(D, s, pants="Body", shin="Body", th_r=(6.2, 5.2), sh_r=(4.8, 4.0))
        shoe(D, s, upper="Extra", sole="Dark", collar="Glow", length=1.05, width=1.0)


# ================================================================================================================
# Nikoichi - L scorer, round teal robot with a kid in a yellow raincoat riding its shoulders (the kid is the head)
# ================================================================================================================
@recipe("Nikoichi", lambda E, W: skel(E, leg=0.84, neck_drop=24.0, shoulder_y=26.0, hip_y=12.0, torso_back=0.0))
def nikoichi(D):
    k = D.k
    E = D.E
    # the kid: big round head in a yellow hood, little body and arms hugging the robot's dome
    face = human_head(D, rx=11.0, ry=10.4, rz=12.5, eye_y=4.4, eye_size=1.5, brow="Hair", up=1.5,
                      mouth_open=True, neck_r=3.6)
    CX, cz, RX, RY, RZ = face
    hood = D.g("Head", "Extra", k=2 * k)
    hood.ell(_np((CX - 1.6 * k, 0, cz + 1.4 * k)), (RX * 1.12, RY * 1.16, RZ * 1.12))
    hood.half(_np((CX + RX * 0.32, 0, cz)), (-1.0, 0, 0.55), op="s", k=1 * k)
    for s in (1, -1):
        hood.sphere(_np((CX - 2 * k, s * 6 * k, cz + RZ * 1.0)), 3.0 * k)       # little ears on the hood
    kb = D.g("Head", "Extra", k=2 * k)
    kb.ell(_np((CX - 2 * k, 0, cz - RZ - 6 * k)), (8 * k, 9 * k, 7.5 * k))
    for s in (1, -1):
        kb.cap(_np((CX - 1 * k, s * 7 * k, cz - RZ - 3 * k)), _np((CX + 6 * k, s * 10 * k, cz - RZ - 12 * k)),
               2.4 * k, 2.2 * k)
    kh = D.g("Head", "Skin", voxel=0.3)
    for s in (1, -1):
        kh.sphere(_np((CX + 7 * k, s * 10.5 * k, cz - RZ - 13 * k)), 2.6 * k)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    body = D.g("Torso", "Body", k=6 * k)
    body.ell(_np((1 * k, 0, (sp + nk) * 0.5)), (15 * k, 19 * k, (nk - sp) * 0.62))
    dome = D.g("Torso", "Accent", k=1.5 * k)
    dome.ell(_np((0, 0, nk - 1 * k)), (11 * k, 12 * k, 5 * k))
    scr = D.g("Torso", "Dark", k=1 * k)
    scr.box(_np((14.0 * k, 0, (sp + nk) * 0.5 + 2 * k)), (1.6 * k, 9 * k, 6 * k), 2.5 * k)
    rf = D.g("Torso", "Glow", voxel=0.22)
    for s in (1, -1):
        rf.ell(_np((15.5 * k, s * 3.6 * k, (sp + nk) * 0.5 + 3.5 * k)), (0.4 * k, 1.2 * k, 1.8 * k))
    rf.torus(_np((15.5 * k, 0, (sp + nk) * 0.5 + 0.0 * k)), 2.4 * k, 0.4 * k, axis=(1, 0, 0))
    rf.half(_np((0, 0, (sp + nk) * 0.5)), (0, 0, 1), op="i")
    pz = D.J("pelvis")[2]
    pv = D.g("Pelvis", "Body", k=3 * k)
    pv.ell(_np((0, 0, pz - 1 * k)), (12 * k, 16 * k, 8 * k))
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Accent", ua_r=(6.6, 6.0), fa_r=(6.2, 5.6),
                     hand_size=2.0)
        leg_standard(D, s, pants="Body", shin="Body", th_r=(8.4, 7.6), sh_r=(7.4, 6.6), calf=0)
        foot_mech(D, s, slot="Accent", sole="Dark", glow="Glow", length=1.05, width=1.25)


# ================================================================================================================
# Tsurara - M scorer, cryo cyborg skater: ice prosthetic legs with skate blades, frost crystals, long scarf
# ================================================================================================================
@recipe("Tsurara", lambda E, W: skel(E, leg=1.02, shoulder_y=17.4, hip_y=8.4))
def tsurara(D):
    k = D.k
    E = D.E
    face = human_head(D, brow="Hair", eye_size=1.5, lid="Hair")
    CX, cz, RX, RY, RZ = face
    h = hair_cap(D, face, "Hair", grow=1.1, front_cut=0.34)
    tufts(h, face, k, [(-6.5, 9.5, 8.5, 0.6, 25), (-2.8, 11, 10, 0.62, 30), (1.0, 11.2, 9.5, 0.64, 30),
                       (4.8, 10.4, 8, 0.62, 25), (7.6, 8.0, 6.5, 0.58, 20)], r=2.6)
    for s in (1, -1):
        h.cap(_np((CX - 2 * k, s * RY * 0.92, cz)), _np((CX - 4 * k, s * RY * 0.8, cz - 22 * k)), 4.0 * k, 1.5 * k)
    h.cap(_np((CX - RX * 0.8, 0, cz)), _np((CX - RX * 1.1, 0, cz - 24 * k)), 7 * k, 2.5 * k)
    torso_human(D, slot="Body", chest=0.96, belly=0.95)
    sp, nk = D.J("spine")[2], D.J("neck")[2]
    sc = D.g("Torso", "Glow", k=1.5 * k)
    sc.torus(_np((0, 0, nk - 2 * k)), 7.6 * k, 2.6 * k, sx=1.1, sy=1.1)
    sc.cap(_np((-8 * k, 4 * k, nk - 3 * k)), _np((-14 * k, 6 * k, sp + 4 * k)), 2.6 * k, 1.8 * k)
    sc.cap(_np((-8 * k, -3 * k, nk - 3 * k)), _np((-16 * k, -6 * k, sp + 12 * k)), 2.4 * k, 1.6 * k)
    zip_ = D.g("Torso", "Accent", voxel=0.3)
    zip_.cap(_np((10.4 * k, 0, sp + 3 * k)), _np((10.8 * k, 0, nk - 8 * k)), 0.6 * k)
    pelvis_shorts(D, slot="Accent", belt="Extra")
    for s in (1, -1):
        sd = side(s)
        arm_standard(D, s, sleeve="Body", forearm="Body", hand="Accent", ua_r=(5.2, 4.8), fa_r=(4.6, 4.0),
                     cuff="Extra")
        cr = D.g("UpperArm_" + sd, "Jelly", k=0.6 * k)
        sh = D.J("shoulder", s)
        for i, (dx, dy, dz, ln) in enumerate(((0, 3, 6, 7), (-3, 5, 4, 6), (3, 5, 3, 5.5), (-1, 7, 1, 5))):
            a = sh + [dx * k, s * dy * k, dz * k]
            cr.cone(a, 1.6 * k, 0.2 * k, ln * 0.5 * k, axis=(dx * 0.1, s * 0.6, 1))
        hp = D.J("hip", s)
        kn = D.J("knee", s)
        an = D.J("ankle", s)
        T = D.g("Thigh_" + sd, "Accent")
        T.cap(hp + [0, 0, 1 * k], kn + [0, 0, 1.5 * k], 7.8 * k, 6.4 * k)
        S = D.g("Shin_" + sd, "Extra", k=2 * k)
        S.cap(kn + [0, 0, -1 * k], an + [0, 0, 2.5 * k], 6.0 * k, 4.4 * k)
        S.ell(kn + [3 * k, 0, -6 * k], (4 * k, 6 * k, 7 * k))
        kj = D.g("Shin_" + sd, "Metal", voxel=0.3)
        kj.sphere(kn, 5.2 * k)
        tag_band(D, "Shin_" + sd, an + [0, 0, 4.0 * k], 4.7 * k, 1.3 * k)
        shoe(D, s, upper="Body", sole="Accent", length=1.05, width=1.05, boot=5.0)
        bl = D.g("Foot_" + sd, "Metal", voxel=0.25)
        bl.box(an + [5 * k, 0, 0.5 * k], (12.5 * k, 0.5 * k, 0.6 * k), 0.25 * k)
        for dx in (-3, 11):
            bl.box(an + [dx * k, 0, 1.6 * k], (0.6 * k, 0.4 * k, 1.4 * k), 0.2 * k)
