"""
palette.py - material parameters per owner and slot (single source of truth for Blender previews and the
UE MaterialInstanceConstants MI_<Owner>_<Slot>).

Colours are written as sRGB hex and converted to linear for both Blender and Unreal (FLinearColor).
Keys per slot: c (base colour), r (roughness), m (metallic), e (emissive colour), es (EmissiveStrength),
o (Opacity, translucent slots only).  Base material per slot is chosen in base_material().

Team colours are NOT baked into any hero colour: ally teal / enemy red-orange come only from the runtime
"Team" (Color) and "TaG" (Emissive) overrides, so hero palettes avoid teal and red-orange.
"""


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hexlin(h):
    h = h.lstrip("#")
    return tuple(round(srgb_to_linear(int(h[i:i + 2], 16) / 255.0), 5) for i in (0, 2, 4))


# Shared defaults for the runtime-driven slots.
TEAM = dict(c="#D9DADC", r=0.45, m=0.0)                        # C++ overrides Color with the team colour
TAG = dict(c="#1A1C20", r=0.35, m=0.0, e="#F4FBFF", es=3.0)    # C++ overrides Emissive (team colour) / strength

# ------------------------------------------------------------------------------------------------- heroes
# Generated from tools/art/roster.json: Body = primary, Accent = secondary, Extra = extra, Glow = accent (emissive).
# HERO_SLOTS overrides per hero (skin tone, hair, which colour goes where). Outline = dark ink (C++ sets the team
# colour at runtime).
import json as _json
import os as _os

_ROSTER = _json.load(open(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "art", "roster.json"),
                          encoding="utf-8"))["heroes"]
SKIN = {"light": "#F6D3B8", "warm": "#EEC09C", "tan": "#D9A07A", "deep": "#9C6646"}
HERO_SLOTS = {
    "Gantetsu": dict(Metal="#8E949E"),
    "Minamo": dict(Hair="#3FA8C9", Skin=SKIN["warm"], Metal="#9FB6C9", Extra="#E8EEF2", Accent="#FF7A6B",
                   Glow=("#7FF6FF", 5.0)),
    "Rei": dict(Hair="#ECEFF7", Skin=SKIN["light"], Extra="#3A3D4E", Glow=("#3EE6FF", 5.0)),
    "Hayate": dict(Hair="#1E2233", Skin=SKIN["warm"], Glow=("#5C8BFF", 4.0)),
    "Tsumugi": dict(Hair="#3B2C4F", Skin=SKIN["light"], Glow=("#FFE27A", 5.0)),
    "Hibiki": dict(Jelly=("#B9A2FF", "#8C5CF2", 1.2, 0.55)),
    "Suzu": dict(Hair="#2A2026", Skin=SKIN["light"], Extra="#2B2B33"),
    "Hibana": dict(Hair="#5A2A22", Skin=SKIN["warm"], Glow=("#FFD84A", 6.0)),
    "Koyomi": dict(Hair="#1B1F33", Skin=SKIN["light"]),
    "Hari": dict(Hair="#4A4458", Skin=SKIN["tan"], Glow=("#E3C066", 3.0)),
    "Ushio": dict(Hair="#2A2420", Skin=SKIN["tan"], Glow=("#59D97A", 1.2)),
    "Kotetsu": dict(Glow=("#8FE0FF", 6.0)),
    "Keibi": dict(Glow=("#FF4A4A", 5.0)),
    "Iwakura": dict(Skin=SKIN["warm"], Glow=("#F2B33D", 4.0)),
    "Kawara": dict(Glow=("#FF6A45", 4.5)),
    "Amagi": dict(Glow=("#8FD3FF", 5.0)),
    "Tsurube": dict(Glow=("#FF6A4A", 5.0)),
    "Aoji": dict(Hair="#3F6A3A", Glow=("#5FE0E6", 3.5)),
    "Kokemaru": dict(Glow=("#9CF2C2", 4.0)),
    "Yadori": dict(Glow=("#6FE0C0", 4.0)),
    "Mugura": dict(Glow=("#6FF2FF", 5.0)),
    "Ankou": dict(Glow=("#FFF06A", 6.0)),
    "Hotaru": dict(Jelly=("#FFF1B8", "#F2E07A", 1.6, 0.6), Glow=("#E8FF7A", 6.0)),
    "Tsubame": dict(Glow=("#FF5A4A", 4.0)),
    "Kazami": dict(Glow=("#FFD86B", 4.5)),
    "Tomoshibi": dict(Glow=("#FFC36A", 5.0)),
    "Yatai": dict(Glow=("#FFC870", 2.6)),
    "Oboro": dict(Hair="#B8A273", Jelly=("#C9C4E6", "#9A8FD0", 0.6, 0.45), Glow=("#F2F2FF", 4.0)),
    "Nikoichi": dict(Extra="#FFE04F", Accent="#FF8A3D", Hair="#3A2A22", Skin=SKIN["light"], Glow=("#FFB24A", 4.0)),
    "Tsurara": dict(Hair="#D6ECFF", Skin=SKIN["light"], Jelly=("#CFF0FF", "#9FDCFF", 1.0, 0.55), Glow=("#4A6AF2", 2.5)),
}
OUTLINE_INK = "#1B1626"


def _hero_palette(h):
    pal = h["palette"]
    ov = HERO_SLOTS.get(h["key"], {})
    d = {
        "Body": dict(c=pal["primary"], r=0.6),
        "Accent": dict(c=pal["secondary"], r=0.55),
        "Extra": dict(c=pal["extra"], r=0.6),
        "Glow": dict(c=pal["accent"], r=0.3, e=pal["accent"], es=4.0),
        "Hair": dict(c="#2B2230", r=0.5),
        "Skin": dict(c=SKIN["warm"], r=0.6),
        "Dark": dict(c="#26252D", r=0.6),
        "Metal": dict(c="#A9B2BC", r=0.3, m=0.8),
        "White": dict(c="#FAF8F2", r=0.4),
        "Jelly": dict(c=pal["accent"], r=0.2, e=pal["accent"], es=0.8, o=0.5),
        "Team": TEAM, "TaG": TAG,
        "Outline": dict(c=OUTLINE_INK, r=1.0),
    }
    for slot, v in ov.items():
        if slot == "Glow":
            d["Glow"] = dict(c=v[0], r=0.3, e=v[0], es=v[1])
        elif slot == "Jelly":
            d["Jelly"] = dict(c=v[0], r=0.2, e=v[1], es=v[2], o=v[3])
        else:
            d[slot] = dict(d.get(slot, {}), c=v)
    return d


P = {h["key"]: _hero_palette(h) for h in _ROSTER}
P.update({
    # ----------------------------------------------------------------------------------------- ball
    "Ball": {
        "Body": dict(c="#E87A2A", r=0.72),
        "Dark": dict(c="#2A2228", r=0.8),
        "Glow": dict(c="#FFD27A", r=0.4, e="#FFD27A", es=1.0),   # core lines; C++ raises with the chain
    },
    # ----------------------------------------------------------------------------------------- court (fantasy dusk)
    "Hoop": {
        "Metal": dict(c="#4A4F60", r=0.4, m=0.8),
        "Board": dict(c="#F4F1EA", r=0.2),
        "Accent": dict(c="#E0B24A", r=0.6),            # pole padding
        "TaG": dict(c="#3A2A20", r=0.3, e="#FFA040", es=4.0),    # rim = goal furnace
        "Glow": dict(c="#FFE6B0", r=0.3, e="#FFE6B0", es=2.0),   # shooter's square
        "Net": dict(c="#FFF6EC", r=0.7),
        "Dark": dict(c="#2A2633", r=0.6),
    },
    "Rooftop": {
        "Court": dict(c="#C8814A", r=0.7),             # warm lacquered wood
        "Paint": dict(c="#1E6F78", r=0.6),             # deep teal lane
        "Apron": dict(c="#4A4458", r=0.85),            # dusky stone
        "Dark": dict(c="#2A2633", r=0.85),
    },
    "RooftopProps": {
        "Metal": dict(c="#5A5F70", r=0.45, m=0.7),
        "Body": dict(c="#8C8296", r=0.8),              # lilac concrete
        "Dark": dict(c="#2B2733", r=0.8),
        "Accent": dict(c="#E0B24A", r=0.6),            # gold rails, banners
        "Extra": dict(c="#D9452F", r=0.6),             # vermilion shrine gate, banners
        "White": dict(c="#FFB85A", r=0.5, e="#FFA444", es=5.0),   # paper lanterns
        "Glow": dict(c="#FFF0D0", r=0.2, e="#FFF0D0", es=10.0),   # light-tower lamps, gate plaque
        "Window": dict(c="#1B2330", r=0.2, e="#FFC27A", es=0.0),
    },
    "RooftopProps_Furnace": {
        "Body": dict(c="#5C4A4A", r=0.9),
        "Dark": dict(c="#221E24", r=0.85),
        "Metal": dict(c="#6A5A55", r=0.6, m=0.5),
        "Glow": dict(c="#FF8A3C", r=0.4, e="#FF8A3C", es=8.0),   # furnace stack mouths
    },
    # ----------------------------------------------------------------------------------------- city
    "CityA": {   # ally district (-Y side): terracotta walls, teal tile roofs, warm light
        "Body": dict(c="#C9967A", r=0.9),
        "Accent": dict(c="#2F6F6A", r=0.85),
        "Dark": dict(c="#33303D", r=0.8),
        "Metal": dict(c="#6A6F80", r=0.5, m=0.6),
        "Window": dict(c="#1B2330", r=0.2, e="#FFC27A", es=0.0),  # C++ lights them
        "Glow": dict(c="#FFB15C", r=0.3, e="#FFB15C", es=5.0),    # lanterns / neon
    },
    "CityB": {   # rival district (+Y side): pale towers with indigo trims, cool light
        "Body": dict(c="#B9C2D9", r=0.5, m=0.2),
        "Accent": dict(c="#5A6FA8", r=0.4),
        "Dark": dict(c="#2A2E3D", r=0.7),
        "Metal": dict(c="#8A93A6", r=0.35, m=0.8),
        "Window": dict(c="#18202C", r=0.15, e="#D8ECFF", es=0.0),
        "Glow": dict(c="#E6F2FF", r=0.3, e="#E6F2FF", es=5.0),
    },
    "Sea": {
        "Water": dict(c="#1B5866", r=0.06),
        "Glow": dict(c="#FFB05A", r=0.3, e="#FFB05A", es=6.0),   # boat / pier / sky lanterns
        "Dark": dict(c="#2A2622", r=0.8),
        "Body": dict(c="#8A5A3C", r=0.8),
    },
    # ----------------------------------------------------------------------------------------- sky
    "SkyDome": {
        "Sky": dict(c="#FFFFFF", r=1.0),
    },
    "Clouds": {
        "Body": dict(c="#FCE6EE", r=1.0),              # dusk cumulus
        "Accent": dict(c="#7A6A78", r=1.0),            # island rock
        "Hair": dict(c="#5FA86A", r=1.0),              # island grass
        "Extra": dict(c="#D9452F", r=0.6),             # shrine gates
        "Glow": dict(c="#FFD08A", r=0.3, e="#FFB860", es=4.0),   # island lanterns
        "White": dict(c="#F4F1FF", r=0.3, e="#F4F1FF", es=1.6),  # the moon
    },
})


def base_material(owner, slot):
    if slot == "Sky":
        return "M_TaGSky"
    if slot == "Outline":
        return "M_TaGOutline"
    if slot == "Jelly":
        return "M_TaGTranslucent"
    if slot == "Water":
        return "M_TaGWater"
    return "M_TaGBase"


def params(owner, slot):
    d = dict(P[owner][slot])
    out = {
        "base": base_material(owner, slot),
        "Color": list(hexlin(d.get("c", "#808080"))),
        "Roughness": float(d.get("r", 0.6)),
    }
    if out["base"] not in ("M_TaGWater", "M_TaGOutline", "M_TaGSky"):
        out["Metallic"] = float(d.get("m", 0.0))
        out["Emissive"] = list(hexlin(d.get("e", "#000000")))
        out["EmissiveStrength"] = float(d.get("es", 0.0))
    if out["base"] == "M_TaGTranslucent":
        out["Opacity"] = float(d.get("o", 0.5))
    return out
