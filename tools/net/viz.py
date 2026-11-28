#!/usr/bin/env python3
"""Terminal viewer for the Sub Rosa world: a 3D wireframe scene, drawn as braille dots.

The module has two parts. The scene part turns a decoded `srpacket.State` and a list of boxes
into 3D lines (`Lines`): a ground grid, roads, buildings, a compass ring, humans as stick figures,
vehicles with wheels, items and city objects. The braille part draws those lines on a dot canvas.
`tools/net/vector.py` draws the same lines with numpy, as an anti-aliased glowing picture. Both
use `Viewer.scene`. The module makes no network call and no system call.

    v = viz.Viewer()
    sys.stdout.write("\\x1b[H" + v.render(state, boxes=[(lo, hi, "bank", (255, 90, 90))], me=human_id))

`boxes` are tuples (lo, hi, label, rgb) or (lo, hi, label, rgb, floor_step). `viewer.roads` is a
list of (start, end, half_width) with 3D points. Keys: `viewer.key("left")` and so on.

Axes: y is up. A body has three axes in the packet: `a` is up, `b` points to the back and `c` is
the cross product of the two (the right side). A human faces along minus `b`.

    python -I tools/net/viz.py --demo     animated fake scene, Ctrl+C to stop
    python -I tools/net/viz.py --frame    one frame of the demo
"""
from __future__ import annotations

import math
import os
import shutil
import sys
import time

NEAR = 0.3
# Braille dot bit for the dot at (x & 1, y & 3): index is (y & 3) * 2 + (x & 1).
BIT = (0x01, 0x08, 0x02, 0x10, 0x04, 0x20, 0x40, 0x80)
CUBE_EDGES = tuple((i, i ^ b) for i in range(8) for b in (1, 2, 4) if not i & b)

COLOR_ME = (110, 255, 255)
COLOR_HUMAN = (120, 255, 120)
COLOR_HURT = (255, 220, 80)
COLOR_DYING = (255, 80, 80)
COLOR_OBJECT = (255, 160, 60)
COLOR_ITEM = (255, 255, 90)
COLOR_CITY = (170, 140, 235)
COLOR_BOX = (255, 110, 110)
COLOR_GRID = (70, 115, 150)
COLOR_ROAD = (130, 140, 165)
COLOR_RING = (90, 150, 190)
COLOR_NORTH = (255, 90, 90)
# Building colors by kind: the kind comes from the name of the building file.
KIND_COLORS = {"base": (255, 95, 95), "safe": (255, 170, 170), "shop": (255, 180, 60), "bank": (255, 230, 90),
               "medical": (90, 255, 170), "tower": (120, 165, 255), "other": (120, 140, 190)}
CHARS = [chr(0x2800 + m) for m in range(256)]
COLORS = ["\x1b[38;5;%dm" % c for c in range(256)]
HUD_KEYS = "ARROWS TURN  W S TILT  + - ZOOM  T MAP  Q QUIT"
RESET_K = "\x1b[0m\x1b[K"


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _unit(a, fallback):
    n = math.sqrt(_dot(a, a))
    return (a[0] / n, a[1] / n, a[2] / n) if n > 1e-9 else fallback


def forward(yaw, pitch):
    """Look direction. Yaw 0 looks along +z, yaw pi/2 along +x. Negative pitch looks down."""
    cp = math.cos(pitch)
    return (cp * math.sin(yaw), math.sin(pitch), cp * math.cos(yaw))


def wrap(angle):
    return (angle + math.pi) % (2 * math.pi) - math.pi


class Camera:
    __slots__ = ("pos", "r", "u", "f", "focal", "cx", "cy")

    def __init__(self, pos, yaw, pitch, wd, hd, fov):
        pitch = max(-1.5, min(1.5, pitch))
        self.pos = pos
        self.f = forward(yaw, pitch)
        self.r = _unit((self.f[2], 0.0, -self.f[0]), (1.0, 0.0, 0.0))
        self.u = _cross(self.f, self.r)
        self.focal = (wd / 2) / math.tan(fov / 2)
        self.cx = wd / 2
        self.cy = hd / 2

    def to_cam(self, p):
        d = (p[0] - self.pos[0], p[1] - self.pos[1], p[2] - self.pos[2])
        return (_dot(d, self.r), _dot(d, self.u), _dot(d, self.f))


class Pose:
    """Where the camera is, and what it looks at."""
    __slots__ = ("pos", "yaw", "pitch", "focus", "ground")

    def __init__(self, pos, yaw, pitch, focus, ground):
        self.pos, self.yaw, self.pitch, self.focus, self.ground = pos, yaw, pitch, focus, ground


# --------------------------------------------------------------------------- line list

class Lines:
    """3D lines, discs and labels. A line is (ax, ay, az, bx, by, bz, r, g, b, i0, i1, width, fade).

    r, g, b are 0..255. i0 and i1 are the intensity at the two ends. Width is in world units.
    Fade is the depth where the intensity has gone down to the floor."""

    def __init__(self, tint=None):
        self.segs = []
        self.discs = []          # (x, y, z, radius, r, g, b, intensity, fade)
        self.labels = []         # (position, text, rgb, priority)
        self.tint = tint or (lambda rgb: rgb)
        self.cache = None        # the renderer keeps converted arrays here
        self.moving = False      # lines of things that move: the vector renderer gives them a phosphor trail

    def add(self, a, b, rgb, i0=1.0, i1=None, w=0.06, fade=320.0):
        c = self.tint(rgb)
        self.segs.append((a[0], a[1], a[2], b[0], b[1], b[2], c[0], c[1], c[2], i0, i0 if i1 is None else i1, w, fade))

    def poly(self, pts, rgb, closed=False, **kw):
        for i in range(len(pts) - 1):
            self.add(pts[i], pts[i + 1], rgb, **kw)
        if closed and len(pts) > 2:
            self.add(pts[-1], pts[0], rgb, **kw)

    def disc(self, c, r, rgb, inten=1.0, fade=320.0):
        t = self.tint(rgb)
        self.discs.append((c[0], c[1], c[2], r, t[0], t[1], t[2], inten, fade))

    def label(self, pos, text, rgb, prio=0):
        self.labels.append((pos, text.upper(), self.tint(rgb), prio))

    def extend(self, other):
        self.segs.extend(other.segs)
        self.discs.extend(other.discs)
        self.labels.extend(other.labels)


def frame_of(e):
    """(right, up, forward) unit vectors of a body of the packet."""
    a = _unit(getattr(e, "orient_a", (0.0, 1.0, 0.0)), (0.0, 1.0, 0.0))
    b = _unit(getattr(e, "orient_b", (1.0, 0.0, 0.0)), (1.0, 0.0, 0.0))
    c = _cross(a, b)
    return c, a, (-b[0], -b[1], -b[2])


def at(pos, frame, r, u, f):
    """The world point at (r, u, f) of a body frame."""
    (r0, r1, r2), (u0, u1, u2), (f0, f1, f2) = frame
    return (pos[0] + r0 * r + u0 * u + f0 * f, pos[1] + r1 * r + u1 * u + f1 * f, pos[2] + r2 * r + u2 * u + f2 * f)


def box_edges(lines, pts, rgb, **kw):
    for i, j in CUBE_EDGES:
        lines.add(pts[i], pts[j], rgb, **kw)


def local_box(pos, frame, lo, hi):
    """The 8 corners of a box in a body frame. lo and hi are (right, up, forward)."""
    return [at(pos, frame, hi[0] if i & 1 else lo[0], hi[1] if i & 2 else lo[1], hi[2] if i & 4 else lo[2])
            for i in range(8)]


def circle(pos, frame, r, u, f, radius, plane, n=10):
    """A circle around the point (r, u, f). plane is the pair of body axes it lies in, 0 right, 1 up, 2 forward."""
    pts = []
    for k in range(n):
        ang = 2 * math.pi * k / n
        c = [r, u, f]
        c[plane[0]] += math.cos(ang) * radius
        c[plane[1]] += math.sin(ang) * radius
        pts.append(at(pos, frame, c[0], c[1], c[2]))
    return pts


def walk_pose(phase, amp):
    """Joint offsets of a figure (right, up, forward) for a walk phase. amp 0 stands still."""
    pts = {}
    for side, sg in (("l", -1.0), ("r", 1.0)):
        sw = math.sin(phase + (0.0 if sg < 0 else math.pi)) * amp          # leg swing
        lift = max(0.0, math.cos(phase + (0.0 if sg < 0 else math.pi))) * amp
        hip = (0.12 * sg, 0.0, 0.0)
        knee = (hip[0], -0.45 * math.cos(sw * 0.9), 0.45 * math.sin(sw * 0.9) + 0.0)
        foot = (hip[0], knee[1] - 0.5 * math.cos(sw * 0.4) + 0.12 * lift, knee[2] - 0.5 * math.sin(sw * 0.9 + 0.5 * lift) * 0.8)
        pts["hip" + side], pts["knee" + side], pts["foot" + side] = hip, knee, foot
        aw = -sw * 0.9                                                      # arm swing, opposite to the leg
        sh = (0.2 * sg, 0.52, 0.0)
        elbow = (0.25 * sg, sh[1] - 0.28 * math.cos(aw), 0.28 * math.sin(aw))
        hand = (0.25 * sg, elbow[1] - 0.26 * math.cos(aw + 0.4 + amp * 0.5), elbow[2] + 0.26 * math.sin(aw + 0.4 + amp * 0.5))
        pts["sh" + side], pts["elbow" + side], pts["hand" + side] = sh, elbow, hand
    return pts


def add_figure(lines, pos, frame, rgb, phase, amp, scale, seated, bright):
    """An articulated stick figure about 1.9 units high: head circle, spine, arms, legs."""
    def P(p):
        return at(pos, frame, p[0] * scale, p[1] * scale, p[2] * scale)

    w = 0.07 * scale
    i = bright
    if seated:
        lines.poly(circle(pos, frame, 0.0, 0.86 * scale, 0.0, 0.15 * scale, (0, 1), 8), rgb, closed=True, i0=i, w=w)
        lines.add(P((0, 0.7, 0)), P((0, 0.1, 0)), rgb, i, w=w)
        for sg in (-1, 1):
            lines.add(P((0.2 * sg, 0.5, 0)), P((0.25 * sg, 0.3, 0.3)), rgb, i, w=w * 0.8)
            lines.add(P((0.25 * sg, 0.3, 0.3)), P((0.1 * sg, 0.35, 0.5)), rgb, i, w=w * 0.8)
            lines.add(P((0.12 * sg, 0.0, 0)), P((0.14 * sg, 0.0, 0.5)), rgb, i, w=w * 0.8)
            lines.add(P((0.14 * sg, 0.0, 0.5)), P((0.14 * sg, -0.5, 0.5)), rgb, i, w=w * 0.8)
        return
    j = walk_pose(phase, amp)
    lean = 0.08 * amp
    neck = P((0.0, 0.62, lean))
    pelvis = P((0.0, 0.05, 0.0))
    lines.poly(circle(pos, frame, 0.0, 0.8 * scale, lean * 1.4 * scale, 0.15 * scale, (0, 1), 8), rgb, closed=True, i0=i, w=w)
    lines.add(neck, pelvis, rgb, i, w=w * 1.2)
    lines.add(P((-0.2, 0.52, 0)), P((0.2, 0.52, 0)), rgb, i, w=w)
    lines.add(P((-0.12, 0.05, 0)), P((0.12, 0.05, 0)), rgb, i, w=w)
    for s in "lr":
        lines.add(P(j["sh" + s]), P(j["elbow" + s]), rgb, i, w=w * 0.9)
        lines.add(P(j["elbow" + s]), P(j["hand" + s]), rgb, i, w=w * 0.9)
        h = j["hip" + s]
        lines.add(P((h[0], 0.05, h[2])), P((j["knee" + s][0], j["knee" + s][1] + 0.05, j["knee" + s][2])), rgb, i, w=w)
        lines.add(P((j["knee" + s][0], j["knee" + s][1] + 0.05, j["knee" + s][2])),
                  P((j["foot" + s][0], j["foot" + s][1] + 0.05, j["foot" + s][2])), rgb, i, w=w)
        f = j["foot" + s]
        lines.add(P((f[0], f[1] + 0.05, f[2])), P((f[0], f[1] + 0.05, f[2] + 0.14)), rgb, i, w=w * 0.8)


def add_car(lines, pos, frame, rgb, bright, solid_cam=None):
    """A body box, a cabin box, four wheels and an arrow on the hood that shows the heading."""
    body = local_box(pos, frame, (-0.95, -0.55, -2.2), (0.95, 0.1, 2.2))
    cab = local_box(pos, frame, (-0.8, 0.1, -1.2), (0.8, 0.68, 0.6))
    if solid_cam is not None:
        add_solid_box(lines, body, rgb, solid_cam, bright, 0.0)
        add_solid_box(lines, cab, rgb, solid_cam, bright * 0.8, 0.0)
    else:
        box_edges(lines, body, rgb, i0=bright, w=0.1)
        box_edges(lines, cab, rgb, i0=bright * 0.8, w=0.07)
    for sx in (-0.95, 0.95):
        for sz in (-1.4, 1.4):
            ring = circle(pos, frame, sx, -0.6, sz, 0.42, (1, 2), 9)
            lines.poly(ring, rgb, closed=True, i0=bright, w=0.09)
    tip = at(pos, frame, 0.0, 0.1, 3.4)
    lines.add(at(pos, frame, 0.0, 0.1, 1.2), tip, rgb, bright, w=0.12)
    lines.add(tip, at(pos, frame, -0.35, 0.1, 2.7), rgb, bright, w=0.12)
    lines.add(tip, at(pos, frame, 0.35, 0.1, 2.7), rgb, bright, w=0.12)


def add_diamond(lines, pos, size, rgb, bright):
    top, bot = (pos[0], pos[1] + size * 1.4, pos[2]), (pos[0], pos[1] - size * 1.4, pos[2])
    ring = [(pos[0] + size, pos[1], pos[2]), (pos[0], pos[1], pos[2] + size),
            (pos[0] - size, pos[1], pos[2]), (pos[0], pos[1], pos[2] - size)]
    for p in ring:
        lines.add(top, p, rgb, bright, w=0.05, fade=200.0)
        lines.add(bot, p, rgb, bright, w=0.05, fade=200.0)
    lines.poly(ring, rgb, closed=True, i0=bright, w=0.05, fade=200.0)


def add_building(lines, lo, hi, rgb, step, bright=0.62, floors=True, solid_cam=None):
    """A box with its four walls and one ring for each floor. With solid_cam the hidden lines go."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    if solid_cam is not None:
        corners = [(x1 if i & 1 else x0, y1 if i & 2 else y0, z1 if i & 4 else z0) for i in range(8)]
        add_solid_box(lines, corners, rgb, solid_cam, bright, step if floors else 0)
        return
    w = 0.09
    base = [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]
    top = [(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)]
    lines.poly(base, rgb, closed=True, i0=bright, w=w, fade=360.0)
    lines.poly(top, rgb, closed=True, i0=bright, w=w, fade=360.0)
    for p, q in zip(base, top):
        lines.add(p, q, rgb, bright * 0.9, w=w, fade=360.0)
    if step and y1 - y0 > step * 1.5 and floors:
        y = y0 + step
        while y < y1 - step * 0.4:
            ring = [(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)]
            lines.poly(ring, rgb, closed=True, i0=bright * 0.28, w=0.05, fade=160.0)
            y += step


# --------------------------------------------------------------------------- themes

def _lum(rgb):
    return (0.3 * rgb[0] + 0.59 * rgb[1] + 0.11 * rgb[2]) / 255.0


def make_theme(hue, accent, warn, danger, me):
    """A theme: scene colors become shades of one hue. Cars, shops and bases get the accent hue."""
    warm = {COLOR_OBJECT, KIND_COLORS["base"], KIND_COLORS["safe"], KIND_COLORS["shop"], KIND_COLORS["bank"], COLOR_ITEM}
    fixed = {COLOR_ME: me, COLOR_HURT: warn, COLOR_DYING: danger, COLOR_NORTH: warn}

    def theme(rgb):
        if rgb in fixed:
            return fixed[rgb]
        base = accent if rgb in warm else hue
        k = min(1.0, _lum(rgb) * 1.25) ** 0.85
        return (int(base[0] * k), int(base[1] * k), int(base[2] * k))
    return theme


# Green-cyan like a color vector tube, and amber like an old monochrome monitor.
THEMES = {"tube": make_theme((40, 255, 170), (255, 190, 70), (255, 190, 70), (255, 90, 70), (210, 255, 255)),
          "amber": make_theme((255, 176, 40), (255, 215, 120), (255, 120, 40), (255, 70, 40), (255, 235, 170))}


LIGHT = (0.35, 0.8, 0.45)          # direction toward the light for the flat shading of solid boxes


def add_solid_box(lines, pts, rgb, cam, bright, step=4.0):
    """A box without hidden lines: only the edges next to a face that looks at the camera, and light hatching
    on those faces (one line for each floor on the walls, a few lines on the roof). `pts` come from local_box."""
    center = tuple(sum(p[k] for p in pts) / 8.0 for k in range(3))
    light = _unit(LIGHT, (0.0, 1.0, 0.0))
    faces = {}
    for axis in range(3):
        for side in (0, 1):
            idx = [i for i in range(8) if (i >> axis & 1) == side]
            fc = tuple(sum(pts[i][k] for i in idx) / 4.0 for k in range(3))
            nrm = _unit((fc[0] - center[0], fc[1] - center[1], fc[2] - center[2]), (0.0, 1.0, 0.0))
            front = _dot(nrm, (cam[0] - fc[0], cam[1] - fc[1], cam[2] - fc[2])) > 0
            faces[(axis, side)] = (idx, nrm, front)
    for i, j in CUBE_EDGES:
        axis = (i ^ j).bit_length() - 1
        others = [a for a in range(3) if a != axis]
        if any(faces[(a, i >> a & 1)][2] for a in others):
            lines.add(pts[i], pts[j], rgb, bright, w=0.09, fade=360.0)

    def blend(a, b, f):
        return tuple(a[c] + (b[c] - a[c]) * f for c in range(3))

    for (axis, side), (idx, nrm, front) in faces.items():
        if not front:
            continue
        shade = 0.3 + 0.7 * max(0.0, _dot(nrm, light))
        if axis == 1:                                           # the roof: a few lines across
            lo = [i for i in idx if not i & 4]
            hi = [i for i in idx if i & 4]
            count = 4
            axis_split = (lo, hi)
        else:                                                   # a wall: lines along the horizontal edge, one for each floor
            lo = [i for i in idx if not i & 2]
            hi = [i for i in idx if i & 2]
            height = abs(pts[hi[0]][1] - pts[lo[0]][1])
            count = max(1, int(round(height / step))) if step else 1
            axis_split = (lo, hi)
        lo, hi = axis_split
        for k in range(1, count):
            f = k / count
            lines.add(blend(pts[lo[0]], pts[hi[0]], f), blend(pts[lo[1]], pts[hi[1]], f), rgb,
                      bright * shade * 0.45, w=0.05, fade=200.0)


# --------------------------------------------------------------------------- braille canvas

class Canvas:
    """A braille dot grid. Each cell keeps its dots, the color and the depth of the nearest line."""

    def __init__(self, cols, rows):
        self.cols, self.rows = cols, rows
        self.wd, self.hd = cols * 2, rows * 4
        self.mask = [0] * (cols * rows)
        self.color = [0] * (cols * rows)
        self.depth = [math.inf] * (cols * rows)
        self.text = {}

    def line(self, a, b, color):
        """Draw the segment a to b. Points are (x, y, depth) in dots. The segment is cut to the canvas."""
        x0, y0, z0 = a
        x1, y1, z1 = b
        dx, dy = x1 - x0, y1 - y0
        t0, t1 = 0.0, 1.0
        for p, q in ((-dx, x0), (dx, self.wd - 1 - x0), (-dy, y0), (dy, self.hd - 1 - y0)):
            if p == 0:
                if q < 0:
                    return
                continue
            t = q / p
            if p < 0:
                if t > t1:
                    return
                if t > t0:
                    t0 = t
            else:
                if t < t0:
                    return
                if t < t1:
                    t1 = t
        sx, sy, sz = x0 + dx * t0, y0 + dy * t0, z0 + (z1 - z0) * t0
        ex, ey, ez = x0 + dx * t1, y0 + dy * t1, z0 + (z1 - z0) * t1
        n = max(int(abs(ex - sx)), int(abs(ey - sy)))
        if n == 0:
            self._dot(sx, sy, sz, color)
            return
        stx, sty, stz = (ex - sx) / n, (ey - sy) / n, (ez - sz) / n
        cols, mask, depth, col = self.cols, self.mask, self.depth, self.color
        x, y, z = sx, sy, sz
        for _ in range(n + 1):
            xi, yi = int(x), int(y)
            i = (yi >> 2) * cols + (xi >> 1)
            mask[i] |= BIT[((yi & 3) << 1) | (xi & 1)]
            if z < depth[i]:
                depth[i] = z
                col[i] = color
            x += stx
            y += sty
            z += stz

    def _dot(self, x, y, z, color):
        xi, yi = int(x), int(y)
        i = (yi >> 2) * self.cols + (xi >> 1)
        self.mask[i] |= BIT[((yi & 3) << 1) | (xi & 1)]
        if z < self.depth[i]:
            self.depth[i] = z
            self.color[i] = color

    def put_text(self, cx, cy, text, color):
        if not 0 <= cy < self.rows:
            return
        for k, ch in enumerate(text):
            if 0 <= cx + k < self.cols:
                self.text[cy * self.cols + cx + k] = (ch, color)

    def lines(self):
        out = []
        cols, mask, col, text = self.cols, self.mask, self.color, self.text
        for ry in range(self.rows):
            base = ry * cols
            row = mask[base:base + cols]
            if not any(row) and not text:
                out.append(" " * cols + RESET_K)
                continue
            parts, cur = [], -1
            for cx in range(cols):
                i = base + cx
                if text and i in text:
                    ch, c = text[i]
                else:
                    m = row[cx]
                    if not m:
                        parts.append(" ")
                        continue
                    ch, c = CHARS[m], col[i]
                if c != cur:
                    parts.append(COLORS[c])
                    cur = c
                parts.append(ch)
            parts.append(RESET_K)
            out.append("".join(parts))
        return out


def palette(rgb, fade=1.0):
    """Nearest color of the 6x6x6 cube of the 256 color table. Fade scales the brightness."""
    r, g, b = (min(5, int(c * fade * 6 / 256)) for c in rgb)
    return 16 + 36 * r + 6 * g + b


# --------------------------------------------------------------------------- the viewer

# Keys of the user. A letter gives a name for Viewer.key, or snap and quit for the program. A capital letter
# of the four steering keys moves five times as far. The function keys come as a prefix character and a code
# (Windows console: prefix 0 for F1 to F10, prefix 224 for F11, F12 and the arrows).
LETTER_KEYS = {"a": "rot_left", "d": "rot_right", "w": "tilt_up", "s": "tilt_down", "+": "plus", "=": "plus",
               "-": "minus", "_": "minus", "m": "map", "f": "follow", "v": "orbit", "g": "free", "h": "hollow",
               "l": "labels", "r": "grid", "b": "buildings", "t": "trails", "o": "glow", "c": "compass",
               "p": "snap", "q": "quit", "?": "help", "e": "cycle_render", "n": "cycle_camera"}
CODE_KEYS = {"K": "left", "M": "right", "H": "up", "P": "down",
             ";": "help", "<": "cycle_camera", "=": "hollow", ">": "cycle_render",
             "C": "snap", chr(134): "quit"}                                               # F1 to F4, F9, F12


def key_name(ch, code=None):
    """The key name of a character, or of a prefix and a code. None for an unknown key."""
    if code is not None:
        return CODE_KEYS.get(code)
    if ch in "ADWS":
        return "fast_" + LETTER_KEYS[ch.lower()]
    return LETTER_KEYS.get(ch.lower()) if ch not in ("\x03",) else "quit"


TOGGLES = ("hollow", "labels", "grid", "buildings", "trails", "glow", "compass", "help")


class Viewer:
    """Builds the scene and draws it with braille dots. `VectorViewer` of vector.py draws the same scene."""

    def __init__(self, width=None, height=None):
        self.width, self.height = width, height
        self.fov = math.radians(60)
        self.theme = None            # a function from a scene color to a display color
        self.now = time.monotonic
        self.roads = []              # (start, end, half_width)
        self.ground = 0.0            # ground height when no human is followed
        self.fade_floor = 0.12
        # camera
        self.mode = "follow"         # orbit, follow, free or map (straight down, north up)
        self.az = 0.0                # the azimuth of the orbit mode
        self.free_pos = None         # the place and the angles of the free camera
        self.free_yaw = 0.0
        self.free_pitch = -0.3
        self.hollow = True           # outline only. False: hidden lines go and faces get light hatching
        self.labels = True
        self.buildings = True
        self.compass = True
        self.trails = True           # phosphor persistence of the vector view
        self.glow = True             # bloom of the vector view
        self.help = False            # the key bar shows the help line
        self.last_pose = None
        self.yaw_off = 0.0           # turn the user adds to the facing of the human
        self.pitch = -0.27
        self.dist = 40.0
        self.top_dist = 170.0
        self.top_yaw = 0.0
        self.follow = True
        self._yaw_s = None
        self._focus_s = None
        self._t_pose = None
        # drawing options
        self.grid = True
        self.grid_step = 16.0
        self.grid_radius = 170.0
        self.map_radius = 250.0
        self.label_count = 3
        self._cache = {}
        self._motion = {}
        self._tint_memo = {}

    @property
    def top(self):
        return self.mode == "map"

    @top.setter
    def top(self, value):
        self.mode = "map" if value else "follow"

    # -- terminal size and camera
    def size(self):
        if self.width and self.height:
            return max(8, self.width), max(2, self.height)
        ts = shutil.get_terminal_size((80, 24))
        return max(8, self.width or ts.columns), max(2, self.height or ts.lines)

    def make_camera(self, pos, yaw, pitch, size=None):
        cols, rows = size or self.size()
        return Camera(pos, yaw, pitch, cols * 2, (rows - 1) * 4, self.fov)

    @staticmethod
    def project(p, cam):
        """Point to (x, y, depth) in canvas dots, or None when it is behind the near plane."""
        x, y, z = cam.to_cam(p)
        if z < NEAR:
            return None
        return (cam.cx + x / z * cam.focal, cam.cy - y / z * cam.focal, z)

    def key(self, name):
        """A key of the user. Camera: left, right, up, down, tilt_up, tilt_down, plus, minus, map, follow, orbit, free.
        Toggles: hollow, labels, grid, buildings, trails, glow, compass, help. A name with the prefix fast_ moves
        five times as far. Returns True when the name is known."""
        fast = 1.0
        if name.startswith("fast_"):
            name, fast = name[5:], 5.0
        if name in TOGGLES:
            setattr(self, name, not getattr(self, name))
            return True
        if name == "cycle_camera":
            order = ("follow", "orbit", "free", "map")
            return self.key(order[(order.index(self.mode) + 1) % len(order)] if self.mode in order else "follow")
        if name == "cycle_render":
            cycle = getattr(self, "cycle_backend", None)
            if cycle is not None:
                cycle()
            return True
        if name in ("rot_left", "rot_right"):      # the A and D keys turn, but a free camera moves sideways
            name = ("strafe_" if self.mode == "free" else "") + name[4:]
        if name in ("tilt_up", "tilt_down") and self.mode == "free":      # the W and S keys tilt, a free camera rises
            name = "rise" if name == "tilt_up" else "fall"
        if name in ("map", "top"):
            self.mode = "follow" if self.mode == "map" else "map"
        elif name in ("follow", "orbit"):
            self.mode = name
        elif name == "free":
            if self.mode != "free":
                if self.last_pose is not None:
                    self.free_pos = tuple(self.last_pose.pos)
                    self.free_yaw, self.free_pitch = self.last_pose.yaw, self.last_pose.pitch
                self.mode = "free"
            else:
                self.mode = "follow"
        elif name in ("left", "right"):
            step = 0.12 * fast * (-1.0 if name == "left" else 1.0)
            if self.mode == "free":
                self.free_yaw = wrap(self.free_yaw + step)
            elif self.mode == "map":
                self.top_yaw = wrap(self.top_yaw + step)
            elif self.mode == "orbit":
                self.az = wrap(self.az + step)
            else:
                self.yaw_off = wrap(self.yaw_off + step)
        elif name in ("up", "down") and self.mode == "free":
            self._fly(8.0 * fast * (1.0 if name == "up" else -1.0), 0.0)
        elif name in ("up", "tilt_up", "pitch_up"):          # the camera rises, the view goes toward straight down
            self._tilt(-0.07 * fast)
        elif name in ("down", "tilt_down", "pitch_down"):
            self._tilt(0.07 * fast)
        elif name in ("strafe_left", "strafe_right") and self.mode == "free":
            self._fly(0.0, 8.0 * fast * (1.0 if name == "strafe_right" else -1.0))
        elif name in ("rise", "fall") and self.mode == "free":
            p = self.free_pos or (0.0, 30.0, 0.0)
            self.free_pos = (p[0], p[1] + 5.0 * fast * (1.0 if name == "rise" else -1.0), p[2])
        elif name == "plus":
            self._zoom(1 / 1.15)
        elif name == "minus":
            self._zoom(1.15)
        else:
            return False
        return True

    def _tilt(self, delta):
        if self.mode == "free":
            self.free_pitch = max(-1.4, min(1.4, self.free_pitch + delta))
        else:
            self.pitch = max(-1.3, min(-0.05, self.pitch + delta))

    def _zoom(self, factor):
        if self.mode == "map":
            self.top_dist = max(30.0, min(400.0, self.top_dist * factor))
        elif self.mode == "free":
            self._fly(8.0 * (1.0 / factor - 1.0) * 2.0, 0.0)
        else:
            self.dist = max(8.0, min(120.0, self.dist * factor))

    def _fly(self, forward_step, side_step):
        f = forward(self.free_yaw, self.free_pitch)
        p = self.free_pos or (0.0, 30.0, 0.0)
        self.free_pos = (p[0] + f[0] * forward_step + f[2] * side_step, p[1] + f[1] * forward_step,
                         p[2] + f[2] * forward_step - f[0] * side_step)

    def cam_text(self):
        """The camera line of the main view: mode, azimuth, elevation and zoom, with zero padded numbers."""
        pose = self.last_pose
        if pose is None:
            return "CAM %s" % self.mode.upper()
        zoom = 0.0 if self.mode == "free" else math.dist(pose.pos, pose.focus)
        return "CAM %s  AZ %03d  EL %03d  ZOOM %03d" % (self.mode.upper(), math.degrees(pose.yaw) % 360,
                                                         max(0, math.degrees(-pose.pitch)), min(999, zoom))

    def tint(self, rgb):
        """The color of the theme for a scene color. Without a theme the scene color stays."""
        return self.theme(rgb) if self.theme else rgb

    def _tint(self, rgb):
        t = self._tint_memo.get(rgb)
        if t is None:
            t = self._tint_memo[rgb] = self.tint(rgb)
        return t

    def _lines(self):
        return Lines(self._tint)

    # -- the camera pose
    def _pose(self, state, me, humans, camera):
        if camera is not None:
            pos, yaw, pitch = camera
            f = forward(yaw, pitch)
            norm = math.hypot(f[0], f[2]) or 1.0
            focus = (pos[0] + f[0] / norm * 30, self.ground, pos[2] + f[2] / norm * 30)
            return Pose(pos, yaw, pitch, focus, self.ground)
        target = None
        for h in humans:
            if me is not None and h.id == me:
                target = h
        now = self.now()
        dt = 0.0 if self._t_pose is None else min(0.5, max(0.0, now - self._t_pose))
        self._t_pose = now
        if target is not None:
            want = (target.pos[0], target.pos[1], target.pos[2])
            ground = target.pos[1] - 0.95
            _r, _u, fwd = frame_of(target)
            face = math.atan2(fwd[0], fwd[2]) if math.hypot(fwd[0], fwd[2]) > 0.2 else None
        else:
            want = (0.0, self.ground + 1.0, 0.0)
            ground = self.ground
            face = None
        fs = self._focus_s
        if fs is None or math.dist(fs, want) > 40.0:
            fs = want
        else:
            k = 1.0 - math.exp(-dt / 0.12) if dt > 0 else 0.0
            fs = tuple(fs[i] + (want[i] - fs[i]) * k for i in range(3))
        self._focus_s = fs
        if face is not None and self.follow:
            if self._yaw_s is None:
                self._yaw_s = face
            else:
                k = 1.0 - math.exp(-dt / 0.7) if dt > 0 else 0.0
                self._yaw_s = wrap(self._yaw_s + wrap(face - self._yaw_s) * k)
        base = self._yaw_s if self._yaw_s is not None else 0.0
        if self.mode == "free" and self.free_pos is not None:
            f = forward(self.free_yaw, self.free_pitch)
            pos = self.free_pos
            focus = (pos[0] + f[0] * 30.0, ground + 0.5, pos[2] + f[2] * 30.0)
            return Pose(pos, self.free_yaw, self.free_pitch, focus, ground)
        if self.mode == "map":
            yaw, pitch, dist = self.top_yaw, -1.5, self.top_dist
        elif self.mode == "orbit":
            yaw, pitch, dist = self.az, self.pitch, self.dist
        else:
            yaw, pitch, dist = wrap(base + self.yaw_off), self.pitch, self.dist
        f = forward(yaw, pitch)
        focus = (fs[0], fs[1] + 0.5, fs[2])
        pos = (focus[0] - f[0] * dist, max(ground + 1.5, focus[1] - f[1] * dist), focus[2] - f[2] * dist)
        return Pose(pos, yaw, pitch, focus, ground)

    # -- static parts, kept until the focus or the data change
    def _map_lines(self, boxes, focus, ground, cam=None):
        solid = None if self.hollow else cam
        sig = (hash(tuple(boxes)), id(self.roads), len(self.roads), self.top, self.buildings, solid is not None)
        key = (sig, int(focus[0] // 48), int(focus[2] // 48),
               (int(cam[0] // 2), int(cam[1] // 2), int(cam[2] // 2)) if solid is not None else None)
        hit = self._cache.get("map")
        if hit and hit[0] == key:
            return hit[1]
        ln = self._lines()
        rad = self.map_radius + 48.0
        for box in (boxes if self.buildings else ()):
            lo, hi, label, rgb = box[:4]
            step = box[4] if len(box) > 4 else 4.0
            cx, cz = (lo[0] + hi[0]) / 2, (lo[2] + hi[2]) / 2
            if math.hypot(cx - focus[0], cz - focus[2]) - math.hypot(hi[0] - lo[0], hi[2] - lo[2]) / 2 > rad:
                continue
            col = rgb if isinstance(rgb, tuple) else COLOR_BOX
            add_building(ln, lo, hi, col, step, floors=not self.top, solid_cam=solid)
            if label:
                ln.label((cx, hi[1], cz), label, col, 0)
        for start, end, half in self.roads:
            if math.hypot((start[0] + end[0]) / 2 - focus[0], (start[2] + end[2]) / 2 - focus[2]) > rad + 80:
                continue
            dx, dz = end[0] - start[0], end[2] - start[2]
            n = math.hypot(dx, dz) or 1.0
            ux, uz = dx / n, dz / n
            trim = half
            if n < trim * 2 + 1:
                continue
            for side in (-1, 1):
                ox, oz = -uz * half * side, ux * half * side
                ln.add((start[0] + ux * trim + ox, start[1], start[2] + uz * trim + oz),
                       (end[0] - ux * trim + ox, end[1], end[2] - uz * trim + oz), COLOR_ROAD, 0.55, w=0.07, fade=300.0)
        self._cache["map"] = (key, ln)
        return ln

    def _grid_lines(self, focus, ground):
        step, rad = self.grid_step, self.grid_radius
        gx = round(focus[0] / step) * step
        gz = round(focus[2] / step) * step
        key = (gx, gz, round(ground, 2))
        hit = self._cache.get("grid")
        if hit and hit[0] == key:
            return hit[1]
        ln = self._lines()
        n = int(rad // step)
        for k in range(-n, n + 1):
            for axis in (0, 1):
                fixed = (gx if axis == 0 else gz) + k * step
                off = fixed - (focus[0] if axis == 0 else focus[2])
                if abs(off) >= rad:
                    continue
                half = math.sqrt(rad * rad - off * off)
                mid = (focus[2] if axis == 0 else focus[0])
                inten = 0.38 * (1.0 - abs(off) / rad) ** 0.7
                pts = [(fixed, ground, mid + s * half) if axis == 0 else (mid + s * half, ground, fixed)
                       for s in (-1.0, 0.0, 1.0)]
                ln.add(pts[0], pts[1], COLOR_GRID, 0.0, inten, w=0.04, fade=400.0)
                ln.add(pts[1], pts[2], COLOR_GRID, inten, 0.0, w=0.04, fade=400.0)
        self._cache["grid"] = (key, ln)
        return ln

    def _mountains(self, focus, ground):
        """A ring of jagged hills far away, so the picture has a horizon."""
        key = (round(focus[0] / 64), round(focus[2] / 64), round(ground, 1))
        hit = self._cache.get("hills")
        if hit and hit[0] == key:
            return hit[1]
        ln = self._lines()
        n = 120
        pts = []
        for k in range(n):
            a = 2 * math.pi * k / n
            h = 10 + 38 * max(0.0, math.sin(3 * a + 1.0) * math.sin(7 * a + 2.0)) + 14 * abs(math.sin(11 * a)) * (k % 2)
            pts.append((focus[0] + math.sin(a) * 700.0, ground + h, focus[2] + math.cos(a) * 700.0))
        ln.poly(pts, COLOR_RING, closed=True, i0=0.55, w=0.1, fade=2500.0)
        self._cache["hills"] = (key, ln)
        return ln

    def _compass(self, focus, ground):
        ln = self._lines()
        rad = 40.0 if not self.top else 70.0
        pts = [(focus[0] + math.sin(a) * rad, ground, focus[2] + math.cos(a) * rad)
               for a in (2 * math.pi * k / 48 for k in range(48))]
        ln.poly(pts, COLOR_RING, closed=True, i0=0.55, w=0.06, fade=500.0)
        for k in range(24):
            a = 2 * math.pi * k / 24
            s, c = math.sin(a), math.cos(a)
            big = k % 6 == 0
            inner = rad - (3.0 if big else 1.4)
            ln.add((focus[0] + s * inner, ground, focus[2] + c * inner),
                   (focus[0] + s * rad, ground, focus[2] + c * rad), COLOR_RING, 0.9 if big else 0.6, w=0.06, fade=500.0)
        # The letter N lies on the ground beyond the north tick (north is +z).
        nx, nz = focus[0], focus[2] + rad + 5.0
        ln.poly([(nx - 1.4, ground, nz - 2.2), (nx - 1.4, ground, nz + 2.2), (nx + 1.4, ground, nz - 2.2),
                 (nx + 1.4, ground, nz + 2.2)], COLOR_NORTH, i0=1.0, w=0.2, fade=600.0)
        for name, dx, dz in (("E", 1, 0), ("S", 0, -1), ("W", -1, 0)):
            x, z = focus[0] + dx * (rad + 3.5), focus[2] + dz * (rad + 3.5)
            ln.add((x - 0.7 * (dz != 0), ground, z - 0.7 * (dx != 0)), (x + 0.7 * (dz != 0), ground, z + 0.7 * (dx != 0)),
                   COLOR_RING, 0.8, w=0.1, fade=600.0)
        return ln

    # -- moving parts
    def _track(self, kind, ident, pos, now, vel_axes=True):
        """Speed and walk phase of an entity from its position change. Returns the record."""
        key = (kind, ident)
        rec = self._motion.get(key)
        if rec is None:
            rec = self._motion[key] = {"pos": pos, "t": now, "speed": 0.0, "phase": ident * 1.7, "head": None, "seen": now}
            return rec
        dt = now - rec["t"]
        rec["seen"] = now
        if dt >= 0.04:
            dx, dz = pos[0] - rec["pos"][0], pos[2] - rec["pos"][2]
            dist = math.hypot(dx, dz)
            if dist < 30.0:
                rec["speed"] += (dist / dt - rec["speed"]) * 0.5
                rec["phase"] += dist * 2.6
                if dist > 0.05:
                    rec["head"] = (dx / dist, dz / dist)
            rec["pos"], rec["t"] = pos, now
        return rec

    def _prune(self, now):
        if len(self._motion) > 400:
            self._motion = {k: v for k, v in self._motion.items() if now - v["seen"] < 5.0}

    def _entities(self, state, me, pose, now):
        """Two line lists: things that stand still, and things that move."""
        still, move = self._lines(), self._lines()
        move.moving = True
        humans = getattr(state, "humans", None) or ()
        objects = getattr(state, "objects", None) or ()
        items = getattr(state, "items", None) or ()
        city = getattr(state, "cityobjects", None) or ()
        cam = pose.pos
        focus = pose.focus
        far = self.map_radius * 1.3
        for e in city:
            if math.hypot(e.pos[0] - focus[0], e.pos[2] - focus[2]) > far:
                continue
            box_edges(still, local_box(e.pos, frame_of(e), (-0.5, -0.5, -0.5), (0.5, 0.5, 0.5)), COLOR_CITY, i0=0.8, w=0.07)
        for e in items:
            add_diamond(still, e.pos, 0.22, COLOR_ITEM, 1.0)
        for e in objects:
            if math.hypot(e.pos[0] - focus[0], e.pos[2] - focus[2]) > far:
                continue
            rec = self._track("o", e.id, e.pos, now)
            if getattr(e, "mode", 0) == 2:
                # A position only record has no turn: keep the last known body frame, or drive along the motion.
                frame = rec.get("frame")
                if rec["head"] is not None:
                    hx, hz = rec["head"]
                    frame = ((-hz, 0.0, hx), (0.0, 1.0, 0.0), (hx, 0.0, hz))
                elif frame is None:
                    frame = ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
            else:
                frame = rec["frame"] = frame_of(e)
            add_car(move if rec["speed"] > 0.4 else still, e.pos, frame, COLOR_OBJECT, 0.75, None if self.hollow else cam)
        for e in humans:
            mine = me is not None and e.id == me
            hp = getattr(e, "health", 100)
            rgb = COLOR_ME if mine else COLOR_HUMAN if hp > 60 else COLOR_HURT if hp > 25 else COLOR_DYING
            rec = self._track("h", e.id, e.pos, now)
            d = math.dist(cam, e.pos)
            scale = 2.8 + min(1.5, max(0.0, (d - 30.0) / 30.0))
            seated = getattr(e, "vehicle", -1) not in (-1, None)
            speed = rec["speed"]
            amp = 0.0 if seated else min(1.0, speed / 4.0) * 0.75
            if amp < 0.05:
                amp = 0.0
            frame = frame_of(e)
            ln = move if speed > 0.4 and not seated else still
            add_figure(ln, e.pos, frame, rgb, rec["phase"], amp, scale, seated, 1.0 if mine else 0.9)
            if mine:
                ring = [(e.pos[0] + math.sin(a) * 1.1, pose.ground + 0.05, e.pos[2] + math.cos(a) * 1.1)
                        for a in (2 * math.pi * k / 16 for k in range(16))]
                ln.poly(ring, COLOR_ME, closed=True, i0=0.9, w=0.08)
            if d < 55.0 and not seated:
                ln.label((e.pos[0], e.pos[1] + 1.4 * scale, e.pos[2]), "YOU" if mine else "#%d" % e.id, rgb, 5 if mine else 3)
        self._prune(now)
        return still, move

    def scene(self, state, boxes=(), me=None, camera=None):
        """The pose and the lines of one frame: (pose, [Lines, ...])."""
        humans = list(getattr(state, "humans", None) or ())
        pose = self._pose(state, me, humans, camera)
        self.last_pose = pose
        now = self.now()
        parts = []
        if self.grid:
            parts.append(self._grid_lines(pose.focus, pose.ground))
        if self.compass:
            parts.append(self._compass(pose.focus, pose.ground))
        parts.append(self._map_lines(boxes, pose.focus, pose.ground, pose.pos))
        if not self.top:
            parts.append(self._mountains(pose.focus, pose.ground))
        parts.extend(self._entities(state, me, pose, now))
        return pose, parts

    def picked_labels(self, pose, parts):
        """Labels to draw: every entity tag, and the few buildings nearest to the focus."""
        if not self.labels:
            return []
        out = []
        near = []
        for ln in parts:
            for lab in ln.labels:
                if lab[3] == 0:
                    near.append((math.hypot(lab[0][0] - pose.focus[0], lab[0][2] - pose.focus[2]), lab))
                else:
                    out.append(lab)
        near.sort(key=lambda t: t[0])
        out.extend(lab for d, lab in near[:self.label_count] if d < 160.0)
        return out

    # -- braille output
    def _color(self, rgb, inten):
        return palette(rgb, max(0.4, min(1.0, inten * 1.25)))

    def size_canvas(self, size):
        cols, rows = size
        return cols * 2, (rows - 1) * 4

    def render(self, state, boxes=(), me=None, camera=None):
        cols, rows = self.size()
        cv = Canvas(cols, rows - 1)
        pose, parts = self.scene(state, boxes, me, camera)
        cam = Camera(pose.pos, pose.yaw, pose.pitch, cv.wd, cv.hd, self.fov)
        self.draw_braille(cv, cam, parts)
        for pos, text, rgb, _prio in self.picked_labels(pose, parts):
            p = self.project(pos, cam)
            if p and 0 <= p[0] < cv.wd and 0 <= p[1] < cv.hd:
                cv.put_text(int(p[0] // 2) - len(text) // 2, max(0, int(p[1] // 4) - 1), text, self._color(rgb, 1.0))
        out = cv.lines()
        out.append(self._hud(state, cols, state))
        return "\n".join(out)

    def draw_braille(self, cv, cam, parts):
        px, py, pz = cam.pos
        r0, r1, r2 = cam.r
        u0, u1, u2 = cam.u
        f0, f1, f2 = cam.f
        cx, cy, focal = cam.cx, cam.cy, cam.focal
        floor = self.fade_floor
        line = cv.line
        for ln in parts:
            for ax, ay, az, bx, by, bz, cr, cg, cb, i0, i1, _w, fade in ln.segs:
                dax, day, daz = ax - px, ay - py, az - pz
                dbx, dby, dbz = bx - px, by - py, bz - pz
                za = dax * f0 + day * f1 + daz * f2
                zb = dbx * f0 + dby * f1 + dbz * f2
                if za < NEAR and zb < NEAR:
                    continue
                if za > fade and zb > fade:
                    continue
                xa, ya = dax * r0 + day * r1 + daz * r2, dax * u0 + day * u1 + daz * u2
                xb, yb = dbx * r0 + dby * r1 + dbz * r2, dbx * u0 + dby * u1 + dbz * u2
                if za < NEAR:
                    t = (NEAR - za) / (zb - za)
                    xa, ya, za = xa + (xb - xa) * t, ya + (yb - ya) * t, NEAR
                    i0 += (i1 - i0) * t
                elif zb < NEAR:
                    t = (NEAR - za) / (zb - za)
                    xb, yb, zb = xa + (xb - xa) * t, ya + (yb - ya) * t, NEAR
                    i1 = i0 + (i1 - i0) * t
                z = (za + zb) * 0.5
                inten = (i0 + i1) * 0.5 * max(floor, 1.0 - z / fade)
                if inten < 0.06:
                    continue
                line((cx + xa / za * focal, cy - ya / za * focal, za),
                     (cx + xb / zb * focal, cy - yb / zb * focal, zb), self._color((cr, cg, cb), inten))
        for x, y, z, rad, cr, cg, cb, inten, fade in (d for ln in parts for d in ln.discs):
            p = self.project((x, y, z), cam)
            if p is None or p[2] > fade:
                continue
            code = self._color((cr, cg, cb), inten)
            r = max(0.0, rad * cam.focal / p[2])
            for dy in (-r, 0.0, r):
                cv.line((p[0] - r, p[1] + dy, p[2]), (p[0] + r, p[1] + dy, p[2]), code)

    @staticmethod
    def _hud(state, width, _state=None):
        teams = "/".join(str(t) for t in getattr(state, "team_counts", ()))
        count = lambda name: len(getattr(state, name, None) or ())
        text = ("%s  %s  %s  %s  %s  %s" % (
            "TICK %d" % getattr(state, "tick", 0), "PHASE %d" % getattr(state, "phase", 0),
            "TIMER %d" % getattr(state, "phase_timer", 0), "TEAMS " + teams, "MONEY %d" % getattr(state, "money", 0),
            "MENU %d" % getattr(state, "menu", 0)))
        text = " %s  SLOT %d  HUMANS %d  VEHICLES %d  ITEMS %d  OBJECTS %d  %s" % (
            text, getattr(state, "player", 0), count("humans"), count("objects"), count("items"), count("cityobjects"),
            HUD_KEYS)
        return "\x1b[48;5;236m\x1b[38;5;252m" + text[:width].ljust(width) + "\x1b[0m"


# --------------------------------------------------------------------------- demo

def demo_scene(t):
    """A fake State at time t seconds: humans that walk in circles, cars on a road and some boxes."""
    import srpacket
    humans = []
    for k in range(6):
        ang = t * (0.08 + 0.02 * k) + k * 1.7
        rad = 8 + 5 * k
        heading = ang + math.pi / 2
        hx, hz = math.cos(heading), math.sin(heading)
        # Packet axes: a is up, b points to the back. The human faces its walk direction.
        humans.append(srpacket.HumanBlock(
            k, pos=(400 + math.cos(ang) * rad, 25.0, 700 + math.sin(ang) * rad),
            orient_a=(0.0, 1.0, 0.0), orient_b=(-hx, 0.0, -hz), health=100 - 25 * (k % 4)))
    objects = []
    for k in range(3):
        x = 380 + (t * (3 + k) + k * 40) % 120
        objects.append(srpacket.ObjectBlock(k, pos=(x, 25.05, 735 - 6 * k), orient_a=(0.0, 1.0, 0.0),
                                            orient_b=(-1.0, 0.0, 0.0)))
    items = [srpacket.ItemBlock(i, pos=(392 + 4 * i, 24.3, 712)) for i in range(3)]
    city = [srpacket.CityObjectBlock(i, pos=(414 + 5 * i, 24.5, 690)) for i in range(2)]
    state = srpacket.State(tick=int(t * 62), phase=2, phase_timer=int(t * 62) % 3600, team_counts=(1, 0, 0),
                           player=32, money=1250, menu=3, humans=humans, objects=objects, items=items,
                           cityobjects=city)
    boxes = [((380.0, 24.0, 740.0), (440.0, 28.0, 780.0), "BASE 0", KIND_COLORS["base"]),
             ((420.0, 24.0, 650.0), (450.0, 64.0, 680.0), "TOWER", KIND_COLORS["tower"], 4.0),
             ((350.0, 24.0, 690.0), (370.0, 40.0, 710.0), "SHOP", KIND_COLORS["shop"], 4.0)]
    return state, boxes, 0


def _main(argv):
    here = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, here)
    width = height = None
    for i, a in enumerate(argv):
        if a == "--width":
            width = int(argv[i + 1])
        elif a == "--height":
            height = int(argv[i + 1])
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    viewer = Viewer(width, height)
    if "--vector" in argv:
        import vector
        viewer = vector.VectorViewer(width, height)
    if "--frame" in argv:
        state, boxes, me = demo_scene(1.0)
        sys.stdout.write(viewer.render(state, boxes, me) + "\n")
        return 0
    if "--demo" not in argv:
        print(__doc__)
        return 2
    if os.name == "nt":
        os.system("")                      # turns on ANSI escapes in the Windows console
    sys.stdout.write("\x1b[?25l\x1b[2J")
    start = time.time()
    try:
        while True:
            t = time.time() - start
            state, boxes, me = demo_scene(t)
            sys.stdout.write("\x1b[H" + viewer.render(state, boxes, me))
            sys.stdout.flush()
            time.sleep(max(0.0, 0.05 - (time.time() - start - t)))
    except KeyboardInterrupt:
        pass
    finally:
        sys.stdout.write("\x1b[0m\x1b[?25h\n")
    return 0


if __name__ == "__main__":
    sys.exit(_main(sys.argv[1:]))
