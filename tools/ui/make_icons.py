"""
make_icons.py - flat HUD icons for Basket Team-play TaG: pure white glyphs on a transparent background (the game
tints them in Slate, so there is no colour and no baked outline). Original pictograms, drawn with PIL.

  "C:/Users/monns/AppData/Local/Programs/Python/Python312/python.exe" C:/TaG/tools/ui/make_icons.py [--only A,B]

-> tools/build/icons/T_Icon_<Name>.png   128 x 128 RGBA, RGB = white everywhere, alpha = the glyph
-> tools/build/icons/icons.json           names, groups and the /Game paths (read by import_all.py and verify.py)
-> tools/art/previews/icons.png           contact sheet: every icon tinted, at 128 px and at 40 px, with its name

Each glyph is drawn at 512 x 512 (40 px of margin = 10 px at 128) and downsampled with LANCZOS. Strokes are 34-50 px
at 512, i.e. 3-4 px when the icon is shown at 40 px. Shapes that overlap are separated by a knocked-out gap (G).
Imported by tools/import/import_all.py (-iconsonly runs only this import) as /Game/Art/UI/Icons/T_Icon_<Name>.
"""
import colorsys
import json
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFont, ImageOps

TOOLS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT_DIR = os.path.join(TOOLS, "build", "icons")
SHEET = os.path.join(TOOLS, "art", "previews", "icons.png")
GAME_DIR = "/Game/Art/UI/Icons"

S = 512          # drawing size
N = 128          # output size
C = (256, 256)   # centre
W = 44           # main stroke
G = 22           # knock-out gap between overlapping shapes
RS = Image.Resampling


# ------------------------------------------------------------------------------------------------- geometry helpers
def arc_pts(c, rx, a0, a1, ry=None, step=2.0):
    """Points on an ellipse arc, angles in degrees, screen convention (0 = right, 90 = down)."""
    ry = rx if ry is None else ry
    n = max(2, int(abs(a1 - a0) / step))
    out = []
    for i in range(n + 1):
        a = math.radians(a0 + (a1 - a0) * i / n)
        out.append((c[0] + rx * math.cos(a), c[1] + ry * math.sin(a)))
    return out


def bez(p0, p1, p2, p3=None, n=48):
    out = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        if p3 is None:
            out.append((u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0],
                        u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1]))
        else:
            out.append((u ** 3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t ** 3 * p3[0],
                        u ** 3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t ** 3 * p3[1]))
    return out


def head_pts(tip, ang, hl, hw):
    """Arrowhead triangle with its tip at `tip`, pointing along `ang` (degrees)."""
    a = math.radians(ang)
    ux, uy = math.cos(a), math.sin(a)
    bx, by = tip[0] - ux * hl, tip[1] - uy * hl
    return [tip, (bx - uy * hw, by + ux * hw), (bx + uy * hw, by - ux * hw)]


def star_pts(c, ro, ri, n=5, rot=-90.0):
    out = []
    for i in range(2 * n):
        r = ro if i % 2 == 0 else ri
        a = math.radians(rot + 180.0 * i / n)
        out.append((c[0] + r * math.cos(a), c[1] + r * math.sin(a)))
    return out


def sparkle_pts(c, r, p=2.4, n=96):
    """Four-pointed sparkle with concave sides (a superellipse with exponent < 1)."""
    out = []
    for i in range(n):
        t = 2 * math.pi * i / n
        cs, sn = math.cos(t), math.sin(t)
        out.append((c[0] + r * math.copysign(abs(cs) ** p, cs), c[1] + r * math.copysign(abs(sn) ** p, sn)))
    return out


def shield_pts(cx, top, w, h, n=28):
    hw = w / 2
    side = top + 0.38 * h
    pts = [(cx - hw, top), (cx + hw, top), (cx + hw, side)]
    pts += bez((cx + hw, side), (cx + hw, top + 0.80 * h), (cx, top + h), n=n)[1:]
    pts += bez((cx, top + h), (cx - hw, top + 0.80 * h), (cx - hw, side), n=n)[1:-1]
    return pts


def drop_pts(c, size, bend=0.0, n=96):
    """Flame / drop: point up, round bottom. size = half height; bend pushes the tip sideways."""
    out = []
    for i in range(n):
        t = 2 * math.pi * i / n
        x = math.sin(t) * math.sin(t / 2) * 0.82
        y = math.cos(t)
        k = ((y + 1) / 2) ** 3
        out.append((c[0] + (x + bend * k) * size, c[1] - y * size))
    return out


def smooth(pts, iters=3, sharp=()):
    """Closed Chaikin corner cutting; vertices whose index is in `sharp` stay pointed."""
    sharp = set(sharp)
    for _ in range(iters):
        out, osharp = [], set()
        n = len(pts)
        for i in range(n):
            p, q = pts[i], pts[(i + 1) % n]
            if i in sharp:
                osharp.add(len(out))
                out.append(p)
            out.append((0.75 * p[0] + 0.25 * q[0], 0.75 * p[1] + 0.25 * q[1]))
            out.append((0.25 * p[0] + 0.75 * q[0], 0.25 * p[1] + 0.75 * q[1]))
        pts, sharp = out, osharp
    return pts


def taper_pts(pts, w0, w1):
    """Polygon around a polyline whose width goes from w0 to w1 (a root, a tongue of flame...)."""
    acc = path_len(pts)
    left, right = [], []
    for i, p in enumerate(pts):
        a = pts[max(0, i - 1)]
        b = pts[min(len(pts) - 1, i + 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / ln, dx / ln
        w = (w0 + (w1 - w0) * acc[i] / acc[-1]) / 2
        left.append((p[0] + nx * w, p[1] + ny * w))
        right.append((p[0] - nx * w, p[1] - ny * w))
    return left + right[::-1]


FOOT = [(-62, -110), (-40, -140), (0, -150), (40, -138), (66, -108), (78, -60), (74, 0), (66, 60), (60, 110),
        (44, 150), (12, 170), (-22, 164), (-44, 138), (-50, 96), (-38, 50), (-32, 10), (-46, -40), (-62, -80)]
TOES = [((-38, -198), 31), ((10, -208), 23), ((46, -196), 19), ((74, -174), 16), ((92, -146), 13)]
FLAME = [(288, 40), (340, 118), (392, 196), (420, 280), (410, 360), (366, 428), (300, 466), (220, 468), (150, 436),
         (104, 372), (96, 296), (118, 222), (150, 152), (184, 222), (206, 252), (226, 180), (256, 112)]


def path_len(pts):
    acc = [0.0]
    for a, b in zip(pts[:-1], pts[1:]):
        acc.append(acc[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    return acc


def sub_path(pts, s0, s1):
    """The piece of a polyline between arc lengths s0 and s1."""
    acc = path_len(pts)
    s0, s1 = max(0.0, s0), min(acc[-1], s1)
    out = []

    def at(s):
        for i in range(1, len(acc)):
            if acc[i] >= s:
                f = (s - acc[i - 1]) / max(1e-9, acc[i] - acc[i - 1])
                a, b = pts[i - 1], pts[i]
                return (a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f)
        return pts[-1]
    out.append(at(s0))
    for i in range(1, len(acc) - 1):
        if s0 < acc[i] < s1:
            out.append(pts[i])
    out.append(at(s1))
    return out


# ------------------------------------------------------------------------------------------------- the glyph canvas
class Glyph:
    """An L-mode 512 x 512 mask. v=255 draws, v=0 erases; grow > 0 draws the shape expanded (for knock-out halos)."""

    def __init__(self):
        self.im = Image.new("L", (S, S), 0)
        self.d = ImageDraw.Draw(self.im)

    def _set(self, im):
        self.im = im
        self.d = ImageDraw.Draw(self.im)

    def union(self, other):
        self._set(ImageChops.lighter(self.im, other.im))

    def cut(self, other):
        self._set(ImageChops.subtract(self.im, other.im))

    def place(self, other, at=C, rot=0.0, mirror=False, v=255):
        """Paste a layer drawn around the canvas centre, rotated (degrees, counter-clockwise) and moved to `at`."""
        im = ImageOps.mirror(other.im) if mirror else other.im
        im = im.rotate(rot, resample=RS.BICUBIC, center=C, translate=(at[0] - C[0], at[1] - C[1]))
        self._set(ImageChops.lighter(self.im, im) if v else ImageChops.subtract(self.im, im))

    # primitives
    def disc(self, c, r, v=255, grow=0):
        r += grow
        if r > 0:
            self.d.ellipse([c[0] - r, c[1] - r, c[0] + r, c[1] + r], fill=v)

    def oval(self, c, rx, ry, v=255, grow=0):
        rx, ry = rx + grow, ry + grow
        self.d.ellipse([c[0] - rx, c[1] - ry, c[0] + rx, c[1] + ry], fill=v)

    def ring(self, c, r, w, v=255, grow=0, ry=None):
        ry = r if ry is None else ry
        w = w + 2 * grow
        self.d.ellipse([c[0] - r - w / 2, c[1] - ry - w / 2, c[0] + r + w / 2, c[1] + ry + w / 2], outline=v,
                       width=int(round(w)))

    def poly(self, pts, v=255, grow=0):
        pts = [(float(x), float(y)) for x, y in pts]
        self.d.polygon(pts, fill=v)
        if grow > 0:
            self.d.line(pts + [pts[0]], fill=v, width=int(round(2 * grow)), joint="curve")
            for p in pts:
                self.disc(p, grow, v)

    def line(self, pts, w, v=255, grow=0, caps=True):
        pts = [(float(x), float(y)) for x, y in pts]
        ww = w + 2 * grow
        if len(pts) >= 2:
            self.d.line(pts, fill=v, width=int(round(ww)), joint="curve")
        if caps:
            self.disc(pts[0], ww / 2, v)
            self.disc(pts[-1], ww / 2, v)

    def rrect(self, box, r, v=255, grow=0, corners=None):
        x0, y0, x1, y1 = box
        self.d.rounded_rectangle([x0 - grow, y0 - grow, x1 + grow, y1 + grow], radius=max(0, r + grow), fill=v,
                                 corners=corners)

    def arrow(self, pts, w, hl, hw, v=255, grow=0):
        """Polyline arrow; the last point is the tip."""
        tip, p = pts[-1], pts[-2]
        ang = math.atan2(tip[1] - p[1], tip[0] - p[0])
        end = (tip[0] - math.cos(ang) * hl * 0.6, tip[1] - math.sin(ang) * hl * 0.6)
        self.line(list(pts[:-1]) + [end], w, v, grow)
        self.poly(head_pts(tip, math.degrees(ang), hl, hw), v, grow)

    def curve_arrow(self, pts, w, hl, hw, v=255, grow=0):
        """The path ends at the base of the head; the tip continues along the end tangent."""
        b = pts[-1]
        p = pts[-4] if len(pts) > 4 else pts[0]
        ang = math.atan2(b[1] - p[1], b[0] - p[0])
        tip = (b[0] + math.cos(ang) * hl, b[1] + math.sin(ang) * hl)
        self.line(pts, w, v, grow)
        self.poly(head_pts(tip, math.degrees(ang), hl, hw), v, grow)
        return tip

    def dashed(self, pts, w, dash, gap, offset=0.0, v=255):
        total = path_len(pts)[-1]
        s = offset
        while s < total:
            if s + dash > 0:
                self.line(sub_path(pts, s, s + dash), w, v)
            s += dash + gap

    # shapes
    def ball(self, c, r, v=255, grow=0):
        if v == 0 or grow:
            self.disc(c, r, v, grow)
            return
        L = Glyph()
        L.disc(c, r)
        sw = max(18.0, r * 0.13)
        L.line([(c[0], c[1] - r - 5), (c[0], c[1] + r + 5)], sw, 0, caps=False)
        L.line([(c[0] - r - 5, c[1]), (c[0] + r + 5, c[1])], sw, 0, caps=False)
        for s in (-1, 1):
            L.ring((c[0] + s * r * 1.32, c[1]), r * 0.98, sw, 0)
        self.union(L)

    def person(self, cx, top, h, v=255, grow=0):
        hr = h * 0.2
        self.disc((cx, top + hr), hr, v, grow)
        bt = top + 2 * hr + h * 0.07
        bw = h * 0.33
        self.rrect((cx - bw, bt, cx + bw, top + h), h * 0.25, v, grow, corners=(True, True, False, False))

    def person_outline(self, cx, top, h, s, v=255):
        L = Glyph()
        L.person(cx, top, h)
        L.person(cx, top, h, 0, -s)
        if v:
            self.union(L)
        else:
            self.cut(L)

    def shield(self, cx, top, w, h, v=255, grow=0):
        self.poly(shield_pts(cx, top, w, h), v, grow)

    def bricks(self, box, rows, joints, mortar=22, r=18):
        """Filled block with knocked-out mortar lines. joints[i] = x positions of the vertical joints in row i."""
        x0, y0, x1, y1 = box
        L = Glyph()
        L.rrect(box, r)
        rh = (y1 - y0) / rows
        for i in range(1, rows):
            y = y0 + rh * i
            L.line([(x0 - 10, y), (x1 + 10, y)], mortar, 0, caps=False)
        for i in range(rows):
            for x in joints[i % len(joints)]:
                L.line([(x, y0 + rh * i - 2), (x, y0 + rh * (i + 1) + 2)], mortar, 0, caps=False)
        self.union(L)

    def hoop_side(self, rim_x0, rim_y, board_x, board_y0, board_y1, net_h=110, v=255):
        """Side view: backboard on the right, rim sticking out to the left, net below the rim."""
        self.rrect((board_x, board_y0, board_x + 36, board_y1), 12, v)
        self.line([(rim_x0, rim_y), (board_x + 4, rim_y)], 30, v)
        x1 = board_x - 14
        nl0, nr0 = rim_x0 + 6, x1
        nl1, nr1 = rim_x0 + 30, x1 - 22
        yb = rim_y + net_h
        self.line([(nl0, rim_y), (nl1, yb)], 22, v)
        self.line([(nr0, rim_y), (nr1, yb)], 22, v)
        self.line([(nl1, yb), (nr1, yb)], 22, v)
        self.line([(nl0, rim_y), (nr1, yb)], 18, v)
        self.line([(nr0, rim_y), (nl1, yb)], 18, v)

    def hoop_front(self, cx, rim_y, rx, net_h=150, v=255):
        """Front view: perspective rim (ellipse ring) with the net hanging below it."""
        ry = rx * 0.26
        bx = rx * 0.62
        yb = rim_y + net_h
        self.line([(cx - rx + 10, rim_y + 8), (cx - bx, yb)], 24, v)
        self.line([(cx + rx - 10, rim_y + 8), (cx + bx, yb)], 24, v)
        self.line([(cx - bx, yb), (cx + bx, yb)], 24, v)
        self.line([(cx - rx * 0.45, rim_y + ry), (cx + bx, yb)], 20, v)
        self.line([(cx + rx * 0.45, rim_y + ry), (cx - bx, yb)], 20, v)
        self.ring((cx, rim_y), rx, 32, v, ry=ry)

    def hand(self, k=1.0, v=255, grow=0, c=C):
        """Open hand, fingers up, palm centre at c (draw on a layer, then place() it rotated)."""
        cx, cy = c
        fw, sp = 40 * k, 58 * k
        for x, t in zip((-1.5, -0.5, 0.5, 1.5), (-212, -250, -232, -180)):
            self.line([(cx + x * sp, cy - 10 * k), (cx + x * sp, cy + t * k + fw / 2)], fw, v, grow)
        half = 1.5 * sp + fw / 2
        self.rrect((cx - half, cy - 40 * k, cx + half, cy + 150 * k), 56 * k, v, grow)
        self.line([(cx - half + 22 * k, cy + 95 * k), (cx - half - 70 * k, cy - 30 * k)], 46 * k, v, grow)

    def foot(self, c, k=1.0, v=255, grow=0):
        """Bare footprint (right foot, toes up): sole with the arch on the inside, five toes."""
        cx, cy = c
        self.poly([(cx + x * k, cy + y * k) for x, y in smooth(FOOT, 3)], v, grow)
        for (x, y), r in TOES:
            self.disc((cx + x * k, cy + y * k), r * k, v, grow)

    def chevron(self, c, half_w, h, t, up=True, v=255):
        """Solid chevron (45-degree arms when h == half_w), thickness t."""
        cx, cy = c
        s = -1 if up else 1
        ax, ay = cx, cy + s * h / 2          # apex
        lx, ly = cx - half_w, cy - s * h / 2
        rx_, ry_ = cx + half_w, cy - s * h / 2
        ang = math.atan2(h, half_w)
        dx, dy = t * math.sin(ang), t * math.cos(ang)
        inner = t / math.cos(ang)
        self.poly([(ax, ay), (rx_, ry_), (rx_ - dx, ry_ - s * dy), (ax, ay - s * inner),
                   (lx + dx, ly - s * dy), (lx, ly)], v)


# ------------------------------------------------------------------------------------------------- the icons
def i_drive(g):
    g.poly([(176, 204), (300, 204), (300, 100), (468, 256), (300, 412), (300, 308), (176, 308)])
    g.line([(98, 150), (232, 150)], 36)
    g.line([(64, 256), (136, 256)], 36)
    g.line([(98, 362), (232, 362)], 36)


def i_dodge(g):
    g.disc(C, 60)
    g.curve_arrow(arc_pts(C, 148, 140, 395), W, 108, 80)


def i_tackle(g):
    for x0 in (70, 190):
        g.line([(x0, 128), (x0 + 112, 256), (x0, 384)], 52)
    g.rrect((384, 64, 436, 448), 26)


def i_contest(g):
    L = Glyph()
    L.hand(0.98)
    g.place(L, at=(302, 302))


def i_step(g):
    g.foot((334, 270), 0.92)
    g.arrow([(128, 70), (128, 456)], W, 112, 90)


def i_pass(g):
    c, r = (148, 364), 104
    ang = math.radians(-42)
    d0 = r + G + 24
    start = (c[0] + math.cos(ang) * d0, c[1] + math.sin(ang) * d0)
    g.arrow([start, (458, 64)], W, 130, 100)
    g.ball(c, r)


def i_steal(g):
    L = Glyph()
    L.hand(0.74)
    g.place(L, at=(186, 330), rot=-45)
    c, r = (338, 168), 118
    g.ball(c, r, 0, G)
    g.ball(c, r)


def i_shoot(g):
    g.hoop_side(296, 214, 432, 68, 300, net_h=104)
    g.ball((108, 392), 70)
    path = bez((150, 322), (222, 22), (360, 168), n=60)
    for s in (0, 70, 140, 210, 280):
        acc = path_len(path)
        if s <= acc[-1]:
            p = sub_path(path, s, s)[0]
            g.disc(p, 19)


def i_jump(g):
    g.line([(70, 438), (442, 438)], 40)
    g.poly([(256, 44), (396, 196), (300, 196), (300, 370), (212, 370), (212, 196), (116, 196)])


def i_cut(g):
    path = [(64, 132), (214, 366), (420, 176)]
    acc = path_len(path)
    seg1 = acc[1]
    dash, gap = 62, 34
    # one dash centred on the corner, the others laid out from it
    offs = seg1 - dash / 2
    while offs > 0:
        offs -= dash + gap
    end = acc[-1] - 70
    g.dashed(sub_path(path, 0, end), 40, dash, gap, offset=offs)
    tip = path[-1]
    ang = math.degrees(math.atan2(tip[1] - path[1][1], tip[0] - path[1][0]))
    a = math.radians(ang)
    tip2 = (tip[0] + math.cos(a) * 40, tip[1] + math.sin(a) * 40)
    g.poly(head_pts(tip2, ang, 118, 92))


def i_leap(g):
    g.hoop_side(296, 262, 432, 116, 348, net_h=104)
    path = bez((78, 446), (66, 168), (190, 128), n=60)
    g.curve_arrow(path, W, 104, 84)


def i_role_wall(g):
    g.bricks((48, 92, 464, 420), 3, [(186, 326), (118, 256, 394)], mortar=24)


def i_role_scorer(g):
    g.poly(star_pts((256, 140), 96, 42))
    g.hoop_front(256, 286, 150, net_h=150)


def i_role_playmaker(g):
    hub = (256, 262)
    outs = [(96, 116), (416, 116), (256, 424)]
    for i, o in enumerate(outs):
        g.line([hub, o], 30)
        g.line([o, outs[(i + 1) % 3]], 30)
    for o in outs:
        g.disc(o, 46 + G, 0)
        g.disc(o, 46)
    g.disc(hub, 70 + G, 0)
    g.disc(hub, 70)


def i_guard(g):
    pts = shield_pts(256, 70, 300, 380)
    g.line(pts + [pts[0]], W)


def i_zone(g):
    L = Glyph()
    L.poly(smooth(FLAME, 3, sharp=(0, 12)))
    L.poly(drop_pts((262, 378), 86, bend=0.12), 0)
    g.union(L)


def i_ball(g):
    g.ball(C, 212)


def i_down(g):
    g.line([(64, 440), (448, 440)], 32)
    g.disc((116, 336), 56)
    g.rrect((194, 288, 452, 384), 44)
    for c, r in (((88, 196), 44), ((190, 150), 52), ((300, 186), 40)):
        g.poly(star_pts(c, r, r * 0.45, 5))


def i_clock(g):
    c = (256, 290)
    g.ring(c, 152, W)
    g.rrect((206, 44, 306, 84), 18)
    g.rrect((236, 84, 276, 126), 6)
    L = Glyph()
    L.rrect((236, 50, 276, 100), 8)
    g.place(L, at=(256 + 160, 104 + 50), rot=-45)
    g.d.pieslice([c[0] - 108, c[1] - 108, c[0] + 108, c[1] + 108], -90, -18, fill=255)
    g.disc(c, 26)


def i_ally(g):
    g.chevron((256, 262), 200, 200, 96, up=True)


def i_enemy(g):
    g.chevron((256, 250), 200, 200, 96, up=False)


# ---- art kinds
def k_dash(g):
    g.poly([(184, 104), (328, 256), (184, 408)])
    g.poly([(324, 104), (468, 256), (324, 408)])
    g.line([(78, 166), (150, 166)], 34)
    g.line([(60, 256), (132, 256)], 34)
    g.line([(78, 346), (150, 346)], 34)


def k_leap(g):
    g.person(256, 296, 166)
    path = arc_pts((256, 452), 196, 182, 318, ry=352)
    g.curve_arrow(path, 40, 98, 78)


def k_shell(g):
    L = Glyph()
    L.d.pieslice([56, 172, 456, 572], 180, 360, fill=255)
    L.ring((256, 372), 118, 22, 0)
    for a in (225, 270, 315):
        r0, r1 = 118, 240
        L.line([(256 + math.cos(math.radians(a)) * r0, 372 + math.sin(math.radians(a)) * r0),
                (256 + math.cos(math.radians(a)) * r1, 372 + math.sin(math.radians(a)) * r1)], 22, 0, caps=False)
    L.rrect((0, 372, S, S), 0, 0)
    g.union(L)
    g.rrect((40, 394, 472, 444), 25)


def k_phase(g):
    # a hollow (intangible) body with an attack passing straight through it
    g.person_outline(256, 52, 404, 28)
    g.line([(60, 340), (380, 340)], 40, 0, G)
    g.poly(head_pts((468, 340), 0, 100, 76), 0, G)
    g.arrow([(60, 340), (468, 340)], 40, 100, 76)


def k_counter(g):
    g.rrect((52, 66, 102, 446), 25)
    pts = [(450, 150), (262, 150)] + arc_pts((262, 256), 106, 270, 90)[1:] + [(262, 362), (360, 362)]
    g.curve_arrow(pts, W, 102, 84)


def k_decoy(g):
    # the hollow decoy left behind, the real one beside it
    g.person_outline(148, 116, 300, 30)
    g.person(352, 116, 300, 0, G)
    g.person(352, 116, 300)


def k_shotbuff(g):
    g.ball((206, 300), 156)
    g.poly(sparkle_pts((376, 136), 88), 0, G)
    g.poly(sparkle_pts((376, 136), 88))
    g.poly(sparkle_pts((428, 276), 38), 0, 12)
    g.poly(sparkle_pts((428, 276), 38))


def k_wall(g):
    g.bricks((52, 56, 460, 236), 2, [(188, 324), (120, 256, 392)], mortar=22)
    g.arrow([(256, 448), (256, 276)], W, 104, 88)


def k_shieldfollow(g):
    g.person(150, 112, 300)
    g.shield(316, 64, 264, 390, 0, G)
    g.shield(316, 64, 264, 390)


def k_root(g):
    # standing on the ground line, roots growing down from under the feet
    g.person(256, 40, 250)
    g.line([(64, 312), (448, 312)], 36)
    for s in (-1, 1):
        g.poly(taper_pts(bez((256 + s * 36, 320), (256 + s * 70, 372), (256 + s * 190, 398), n=24), 44, 12))
        g.poly(taper_pts(bez((256 + s * 20, 320), (256 + s * 40, 400), (256 + s * 104, 460), n=24), 40, 12))
        g.poly(taper_pts(bez((256 + s * 118, 384), (256 + s * 140, 420), (256 + s * 150, 456), n=12), 22, 8))
    g.poly(taper_pts([(256, 320), (256, 466)], 44, 12))


def k_tackle(g):
    for x0 in (66, 166):
        g.line([(x0, 140), (x0 + 96, 256), (x0, 372)], 48)
    g.poly(star_pts((378, 256), 92, 46, 8, rot=-90))


def k_cone(g):
    apex = (84, 256)
    L = Glyph()
    L.d.pieslice([apex[0] - 386, apex[1] - 386, apex[0] + 386, apex[1] + 386], -31, 31, fill=255)
    L.disc(apex, 112, 0)
    g.union(L)
    g.disc(apex, 44)


def k_aura(g):
    # a burst ring around the user
    g.person(256, 160, 180)
    pts = star_pts(C, 196, 156, 12, rot=-90)
    g.line(pts + [pts[0]], 30)


def k_bolt(g):
    g.ring((392, 256), 64, 30)
    g.disc((392, 256), 24)
    g.arrow([(72, 256), (296, 256)], 40, 96, 64)
    g.line([(100, 176), (204, 176)], 30)
    g.line([(100, 336), (204, 336)], 30)


def k_field(g):
    g.ring((256, 368), 196, 40, ry=74)
    g.line([(176, 210), (176, 290)], 34)
    g.line([(256, 132), (256, 292)], 34)
    g.line([(336, 210), (336, 290)], 34)


def k_teamaura(g):
    g.line(arc_pts((256, 300), 196, 200, 340), 34)
    g.person(120, 238, 190)
    g.person(392, 238, 190)
    g.person(256, 196, 262, 0, G)
    g.person(256, 196, 262)


def k_teamone(g):
    g.person(128, 214, 236)
    g.person(384, 214, 236)
    g.curve_arrow(arc_pts((256, 240), 140, 200, 312), 36, 80, 64)
    g.poly(sparkle_pts((414, 104), 46))


def k_pass(g):
    g.person(270, 288, 170)
    path = arc_pts((262, 424), 168, 196, 326, ry=322)
    g.curve_arrow(path, 38, 90, 72)
    g.ball((108, 392), 62, 0, G)
    g.ball((108, 392), 62)


def k_conteststance(g):
    L = Glyph()
    L.hand(0.6)
    g.place(L, at=(180, 300), rot=8)
    g.place(L, at=(332, 300), rot=-8, mirror=True)


def k_contestat(g):
    g.ring((304, 222), 152, 34)
    g.ball((304, 222), 82)
    L = Glyph()
    L.hand(0.48)
    big = Glyph()
    big.hand(0.48, grow=G)
    g.place(big, at=(150, 372), v=0)
    g.place(L, at=(150, 372))


# ---- the twelve hand-written arts
def a_shouldercharge(g):
    g.rrect((150, 106, 364, 406), 150, corners=(False, True, True, False))
    g.line(arc_pts((214, 256), 222, -44, 44), 34)
    g.line([(62, 170), (118, 170)], 32)
    g.line([(62, 256), (118, 256)], 32)
    g.line([(62, 342), (118, 342)], 32)


def a_ironwall(g):
    # a steel barrier slab with hazard stripes, set down on two feet
    L = Glyph()
    L.rrect((52, 72, 460, 352), 24)
    L.rrect((92, 112, 420, 312), 6, 0)
    for k in range(-2, 4):
        x = 92 + k * 104
        L.poly([(x, 312), (x + 52, 312), (x + 252, 112), (x + 200, 112)])
    g.union(L)
    cut = Glyph()
    cut.rrect((52, 72, 460, 352), 24)
    cut.im = ImageChops.invert(cut.im)
    g.cut(cut)
    for x in (130, 382):
        g.poly([(x - 30, 352), (x + 30, 352), (x + 56, 440), (x - 56, 440)])


def a_pull(g):
    g.person(390, 140, 230)
    pts = []
    for i in range(121):
        x = 310 - i * (166 / 120)
        pts.append((x, 300 + 44 * math.sin((x - 310) / 166 * 2 * math.pi)))
    g.curve_arrow(pts, 42, 100, 84)


def a_slowfield(g):
    pts = []
    turns = 2.25
    for i in range(240):
        t = i / 239
        a = math.radians(-90 + 360 * turns * t)
        r = 26 + 170 * t
        pts.append((256 + r * math.cos(a), 270 + r * 0.62 * math.sin(a)))
    g.line(pts, 38)


def a_blink(g):
    g.person_outline(128, 160, 260, 24)
    for x, r in ((232, 13), (268, 17), (308, 21)):
        g.disc((x, 300), r)
    g.person(384, 160, 260)
    g.poly(sparkle_pts((428, 98), 46), 0, 12)
    g.poly(sparkle_pts((428, 98), 46))


def a_afterimage(g):
    g.person_outline(150, 100, 320, 18)
    g.person(240, 100, 320, 0, 14)
    g.person_outline(240, 100, 320, 22)
    g.person(356, 100, 320, 0, G)
    g.person(356, 100, 320)


def a_scope(g):
    g.ring(C, 150, 40)
    for a in (0, 90, 180, 270):
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        g.line([(256 + ca * 76, 256 + sa * 76), (256 + ca * 196, 256 + sa * 196)], 36)
    g.disc(C, 26)


def a_spotup(g):
    g.ring((256, 398), 190, 30, ry=54)
    g.oval((256, 398), 72, 20)
    g.ball((256, 206), 100)
    g.line([(256, 62), (256, 76)], 30)
    g.line([(166, 80), (180, 100)], 28)
    g.line([(346, 80), (332, 100)], 28)


def a_threadpass(g):
    g.line([(70, 336), (456, 336)], 22)
    g.disc((70, 336), 28)
    g.ball((350, 236), 88, 0, G)
    g.ball((350, 236), 88)
    g.line([(130, 196), (226, 196)], 30)
    g.line([(96, 252), (220, 252)], 30)


def a_tether(g):
    g.person(128, 160, 250)
    g.person(384, 160, 250)
    g.line(bez((196, 300), (256, 410), (316, 300), n=40), 30)


def a_sonicwave(g):
    g.disc(C, 52)
    for r in (118, 192):
        g.line(arc_pts(C, r, -38, 38), 36)
        g.line(arc_pts(C, r, 142, 218), 36)


def a_resync(g):
    g.curve_arrow(arc_pts(C, 168, 196, 318), W, 92, 76)
    g.curve_arrow(arc_pts(C, 168, 16, 138), W, 92, 76)
    g.line([(256, 196), (256, 316)], 44)
    g.line([(196, 256), (316, 256)], 44)


ICONS = [
    # basic actions
    ("Drive", "action", i_drive), ("Dodge", "action", i_dodge), ("Tackle", "action", i_tackle),
    ("Contest", "action", i_contest), ("Step", "action", i_step), ("Pass", "action", i_pass),
    ("Steal", "action", i_steal), ("Shoot", "action", i_shoot), ("Jump", "action", i_jump),
    ("Cut", "action", i_cut), ("Leap", "action", i_leap),
    # roles
    ("Role_Wall", "role", i_role_wall), ("Role_Scorer", "role", i_role_scorer),
    ("Role_Playmaker", "role", i_role_playmaker),
    # misc
    ("Guard", "misc", i_guard), ("Zone", "misc", i_zone), ("Ball", "misc", i_ball), ("Down", "misc", i_down),
    ("Clock", "misc", i_clock), ("Ally", "misc", i_ally), ("Enemy", "misc", i_enemy),
    # ETaGArtKind (all but Custom), in header order
    ("ArtKind_Dash", "artkind", k_dash), ("ArtKind_Leap", "artkind", k_leap), ("ArtKind_Shell", "artkind", k_shell),
    ("ArtKind_Phase", "artkind", k_phase), ("ArtKind_Counter", "artkind", k_counter),
    ("ArtKind_Decoy", "artkind", k_decoy), ("ArtKind_ShotBuff", "artkind", k_shotbuff),
    ("ArtKind_Wall", "artkind", k_wall), ("ArtKind_ShieldFollow", "artkind", k_shieldfollow),
    ("ArtKind_Root", "artkind", k_root), ("ArtKind_Tackle", "artkind", k_tackle), ("ArtKind_Cone", "artkind", k_cone),
    ("ArtKind_Aura", "artkind", k_aura), ("ArtKind_Bolt", "artkind", k_bolt), ("ArtKind_Field", "artkind", k_field),
    ("ArtKind_TeamAura", "artkind", k_teamaura), ("ArtKind_TeamOne", "artkind", k_teamone),
    ("ArtKind_Pass", "artkind", k_pass), ("ArtKind_ContestStance", "artkind", k_conteststance),
    ("ArtKind_ContestAt", "artkind", k_contestat),
    # the twelve hand-written arts (ETaGArt order)
    ("Art_ShoulderCharge", "art", a_shouldercharge), ("Art_IronWall", "art", a_ironwall), ("Art_Pull", "art", a_pull),
    ("Art_SlowField", "art", a_slowfield), ("Art_Blink", "art", a_blink), ("Art_Afterimage", "art", a_afterimage),
    ("Art_Scope", "art", a_scope), ("Art_SpotUp", "art", a_spotup), ("Art_ThreadPass", "art", a_threadpass),
    ("Art_Tether", "art", a_tether), ("Art_SonicWave", "art", a_sonicwave), ("Art_Resync", "art", a_resync),
]


# ------------------------------------------------------------------------------------------------- output
def render(fn):
    g = Glyph()
    fn(g)
    mask = g.im.resize((N, N), RS.LANCZOS)
    rgba = Image.new("RGBA", (N, N), (255, 255, 255, 0))
    rgba.putalpha(mask)
    return rgba, g.im


def margin_violation(big):
    """Pixels drawn inside the 40 px border of the 512 canvas (should be none)."""
    bb = big.point(lambda p: 255 if p > 24 else 0).getbbox()
    if bb is None:
        return "empty"
    x0, y0, x1, y1 = bb
    if x0 < 36 or y0 < 36 or x1 > S - 36 or y1 > S - 36:
        return "bbox %s" % (bb,)
    return ""


def font(size):
    for name in ("segoeuib.ttf", "arialbd.ttf", "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def contact_sheet(items):
    cols = 8
    cw, ch = 200, 186
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * cw + 20, rows * ch + 60), (40, 41, 46, 255))
    d = ImageDraw.Draw(sheet)
    d.text((14, 14), "Basket Team-play TaG HUD icons: 128 px and 40 px, tinted (the PNGs are white on transparent)",
           fill=(220, 220, 225, 255), font=font(18))
    f = font(14)
    for i, (name, img) in enumerate(items):
        r, c = divmod(i, cols)
        x, y = 10 + c * cw, 50 + r * ch
        hue = (i * 0.618034) % 1.0
        rgb = tuple(int(v * 255) for v in colorsys.hsv_to_rgb(hue, 0.5, 1.0))
        d.rectangle([x + 4, y + 4, x + cw - 4, y + ch - 4], fill=(32, 33, 37, 255))
        tint = Image.new("RGBA", img.size, rgb + (255,))
        tint.putalpha(img.getchannel("A"))
        sheet.alpha_composite(tint, (x + 12, y + 10))
        small = img.resize((40, 40), RS.LANCZOS)
        tint2 = Image.new("RGBA", (40, 40), rgb + (255,))
        tint2.putalpha(small.getchannel("A"))
        sheet.alpha_composite(tint2, (x + 148, y + 54))
        d.text((x + 12, y + 146), name, fill=(230, 230, 235, 255), font=f)
    os.makedirs(os.path.dirname(SHEET), exist_ok=True)
    sheet.save(SHEET)
    return SHEET


def main():
    only = None
    if "--only" in sys.argv:
        only = set(sys.argv[sys.argv.index("--only") + 1].split(","))
    os.makedirs(OUT_DIR, exist_ok=True)
    items = []
    meta = []
    bad = []
    for name, group, fn in ICONS:
        img, big = render(fn)
        if only is None or name in only:
            img.save(os.path.join(OUT_DIR, "T_Icon_%s.png" % name))
        items.append((name, img))
        meta.append({"name": name, "group": group, "file": "T_Icon_%s.png" % name,
                     "asset": "%s/T_Icon_%s" % (GAME_DIR, name)})
        v = margin_violation(big)
        if v:
            bad.append((name, v))
    with open(os.path.join(OUT_DIR, "icons.json"), "w", encoding="utf-8") as f:
        json.dump({"size": N, "folder": GAME_DIR, "icons": meta}, f, indent=1)
    sheet = contact_sheet(items)
    print("ICONS %d -> %s" % (len(items), OUT_DIR))
    print("SHEET %s" % sheet)
    for name, v in bad:
        print("MARGIN %s: %s" % (name, v))


if __name__ == "__main__":
    main()
