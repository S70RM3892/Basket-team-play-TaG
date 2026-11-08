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

P = {
    # ----------------------------------------------------------------------------------------- heroes
    "Gantetsu": {
        "Body": dict(c="#C8901E", r=0.55, m=0.1),      # ochre construction-machine paint
        "Accent": dict(c="#E6E0CC", r=0.6),            # worn stencil white
        "Dark": dict(c="#2A2C31", r=0.55, m=0.35),     # gunmetal
        "Metal": dict(c="#9CA2A9", r=0.32, m=0.9),     # hydraulics / steel
        "Team": TEAM, "TaG": TAG,
        "Glow": dict(c="#FFB060", r=0.3, e="#FFB347", es=6.0),   # welding-visor slit
    },
    "Minamo": {
        "Body": dict(c="#1B2440", r=0.45),             # navy wetsuit
        "Accent": dict(c="#F2D43A", r=0.5),            # courier lemon yellow
        "Skin": dict(c="#F1C7A8", r=0.6),
        "Hair": dict(c="#1E2236", r=0.5),
        "Dark": dict(c="#18191D", r=0.6),
        "Metal": dict(c="#7F8C9A", r=0.3, m=0.85),     # mech legs / arm
        "Team": TEAM, "TaG": TAG,
        "Glow": dict(c="#BFE8FF", r=0.2, e="#BFE8FF", es=4.0),   # water-jet nozzles
    },
    "Rei": {
        "Body": dict(c="#2B2D33", r=0.8),              # charcoal hoodie
        "Accent": dict(c="#EDEDED", r=0.6),
        "Skin": dict(c="#EBC9B0", r=0.6),
        "Hair": dict(c="#D9DCE3", r=0.45),             # silver
        "Dark": dict(c="#121316", r=0.7),
        "Metal": dict(c="#B8BCC2", r=0.3, m=0.9),
        "Team": TEAM, "TaG": TAG,
        "Glow": dict(c="#7FE6FF", r=0.3, e="#7FE6FF", es=2.5),   # subtle cyan glitch
    },
    "Hayate": {
        "Body": dict(c="#EEF0F2", r=0.35),             # corporate white
        "Accent": dict(c="#D4A73C", r=0.4),            # gold trim
        "Skin": dict(c="#E4B996", r=0.6),
        "Hair": dict(c="#191B22", r=0.45),
        "Dark": dict(c="#23262E", r=0.5),
        "Metal": dict(c="#E0B85A", r=0.25, m=1.0),     # gold metal (scope)
        "Team": TEAM, "TaG": TAG,
        "Glow": dict(c="#FFF1C9", r=0.2, e="#FFF1C9", es=5.0),   # scope lens / emblem
    },
    "Tsumugi": {
        "Body": dict(c="#2E3A8C", r=0.75),             # indigo poncho
        "Accent": dict(c="#F0A81C", r=0.7),            # marigold weave stripes
        "Skin": dict(c="#F3CDAF", r=0.6),
        "Hair": dict(c="#17151A", r=0.5),
        "Dark": dict(c="#3B2A22", r=0.8),
        "Metal": dict(c="#C79A3A", r=0.35, m=0.9),     # brass spools
        "Team": TEAM, "TaG": TAG,
        "Glow": dict(c="#FFE2A0", r=0.4, e="#FFE2A0", es=2.5),   # conductive threads
    },
    "Hibiki": {
        "Jelly": dict(c="#A99BFF", r=0.15, m=0.0, e="#8A5CFF", es=0.6, o=0.45),
        "Glow": dict(c="#FF6BE6", r=0.3, e="#FF6BE6", es=6.0),   # inner organs
        "Body": dict(c="#3A1F6B", r=0.4),              # opaque inner core / soles
        "Accent": dict(c="#F4F0FF", r=0.3),
        "Dark": dict(c="#15121F", r=0.5),
        "Team": TEAM, "TaG": TAG,
    },
    # ----------------------------------------------------------------------------------------- ball
    "Ball": {
        "Body": dict(c="#E0701E", r=0.72),
        "Dark": dict(c="#1C1A18", r=0.8),
        "Glow": dict(c="#FFD27A", r=0.4, e="#FFD27A", es=1.0),   # core lines; C++ raises with the chain
    },
    # ----------------------------------------------------------------------------------------- court
    "Hoop": {
        "Metal": dict(c="#3A3F46", r=0.4, m=0.8),
        "Board": dict(c="#E9EEF2", r=0.2),
        "Accent": dict(c="#D9A52B", r=0.6),            # pole padding
        "TaG": dict(c="#3A2A20", r=0.3, e="#FFA040", es=4.0),    # rim = goal furnace
        "Glow": dict(c="#FFE6B0", r=0.3, e="#FFE6B0", es=2.0),   # shooter's square
        "Net": dict(c="#F2F2F2", r=0.7),
        "Dark": dict(c="#1E2024", r=0.6),
    },
    "Rooftop": {
        "Court": dict(c="#B07A48", r=0.7),             # warm painted wood / concrete
        "Paint": dict(c="#0F4A55", r=0.6),             # deep teal lane
        "Apron": dict(c="#3A3D42", r=0.85),
        "Dark": dict(c="#26282C", r=0.85),
    },
    "RooftopProps": {
        "Metal": dict(c="#4A4F57", r=0.45, m=0.7),
        "Body": dict(c="#6B6E73", r=0.8),
        "Dark": dict(c="#1F2125", r=0.8),
        "Accent": dict(c="#C9A227", r=0.6),
        "Glow": dict(c="#FFF4E0", r=0.2, e="#FFF4E0", es=12.0),  # light-tower lamps
        "Window": dict(c="#1B2330", r=0.2, e="#FFC27A", es=0.0),
    },
    "RooftopProps_Furnace": {
        "Body": dict(c="#4B4038", r=0.9),
        "Dark": dict(c="#1C1A19", r=0.85),
        "Metal": dict(c="#5A5048", r=0.6, m=0.5),
        "Glow": dict(c="#FF9A3C", r=0.4, e="#FF9A3C", es=8.0),   # furnace stack mouths
    },
    # ----------------------------------------------------------------------------------------- city
    "CityA": {   # ally district (-Y side): older lower / industrial blocks, warm light
        "Body": dict(c="#857565", r=0.9),
        "Accent": dict(c="#8E4E36", r=0.85),           # terracotta / rust
        "Dark": dict(c="#2B2A2E", r=0.8),
        "Metal": dict(c="#5B6068", r=0.5, m=0.6),
        "Window": dict(c="#1B2330", r=0.2, e="#FFC27A", es=0.0),  # C++ lights them
        "Glow": dict(c="#FFB15C", r=0.3, e="#FFB15C", es=5.0),    # lanterns / neon
    },
    "CityB": {   # rival district (+Y side): corporate glass towers, cool light
        "Body": dict(c="#6F7C8C", r=0.5, m=0.2),
        "Accent": dict(c="#B9C4D0", r=0.4),
        "Dark": dict(c="#23272F", r=0.7),
        "Metal": dict(c="#8A939E", r=0.35, m=0.8),
        "Window": dict(c="#18202C", r=0.15, e="#D8ECFF", es=0.0),
        "Glow": dict(c="#E6F2FF", r=0.3, e="#E6F2FF", es=5.0),
    },
    "Sea": {
        "Water": dict(c="#0B2A2E", r=0.06),
        "Glow": dict(c="#FFB05A", r=0.3, e="#FFB05A", es=6.0),
        "Dark": dict(c="#2A2622", r=0.8),
        "Body": dict(c="#5C4636", r=0.8),
    },
}


def base_material(owner, slot):
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
    if out["base"] != "M_TaGWater":
        out["Metallic"] = float(d.get("m", 0.0))
        out["Emissive"] = list(hexlin(d.get("e", "#000000")))
        out["EmissiveStrength"] = float(d.get("es", 0.0))
    if out["base"] == "M_TaGTranslucent":
        out["Opacity"] = float(d.get("o", 0.5))
    return out
