#!/usr/bin/env python3
"""Terminal viewer for the Sub Rosa world: ANSI colors and 3D wireframes on a braille canvas.

The viewer draws a decoded `srpacket.State` (humans, objects, items, city objects) and extra
boxes (bases, buildings). It makes no network call and no system call. `render` returns one
frame as a string. The caller prints it after a cursor home (`\\x1b[H`).

    v = viz.Viewer()
    sys.stdout.write("\\x1b[H" + v.render(state, boxes=[(lo, hi, "base 0", (255, 90, 90))], me=human_id))

Axes: y is up. A body or box has three axes: `a` is orient_a, `b` is orient_b and `c` is the
cross product of the two. A human stands along `b`.

    python -I tools/net/viz.py --demo     animated fake scene, Ctrl+C to stop
    python -I tools/net/viz.py --frame    one frame of the demo
"""
from __future__ import annotations

import math
import os
import shutil
import sys
import time

NEAR = 0.1
# Braille dot bit for the dot at (x & 1, y & 3): index is (y & 3) * 2 + (x & 1).
BIT = (0x01, 0x08, 0x02, 0x10, 0x04, 0x20, 0x40, 0x80)
CUBE_EDGES = tuple((i, i ^ b) for i in range(8) for b in (1, 2, 4) if not i & b)
# Local points of a stick figure (a, b, c units) and its lines.
FIG_POINTS = ((0, 0, 0), (0, 0.7, 0), (-0.55, 0.25, 0), (0.55, 0.25, 0), (-0.22, -0.95, 0),
              (0.22, -0.95, 0), (0.15, 0.95, 0), (0, 1.1, 0), (-0.15, 0.95, 0), (0, 0.8, 0))
FIG_EDGES = ((0, 1), (1, 2), (1, 3), (0, 4), (0, 5), (6, 7), (7, 8), (8, 9), (9, 6))
CROSS_POINTS = ((-0.25, 0, 0), (0.25, 0, 0), (0, -0.25, 0), (0, 0.25, 0), (0, 0, -0.25), (0, 0, 0.25))
CROSS_EDGES = ((0, 1), (2, 3), (4, 5))
CAR_HALF = (0.9, 0.6, 2.0)
CITY_HALF = (0.5, 0.5, 0.5)

COLOR_ME = (80, 255, 255)
COLOR_HUMAN = (120, 255, 120)
COLOR_HURT = (255, 220, 80)
COLOR_DYING = (255, 80, 80)
COLOR_OBJECT = (255, 160, 60)
COLOR_ITEM = (255, 255, 90)
COLOR_CITY = (130, 150, 210)
COLOR_BOX = (255, 110, 110)
COLOR_GRID = (80, 95, 110)
CHARS = [chr(0x2800 + m) for m in range(256)]
COLORS = ["[38;5;%dm" % c for c in range(256)]


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
                out.append(" " * cols + "[0m[K")
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
            parts.append("[0m[K")
            out.append("".join(parts))
        return out


def palette(rgb, fade=1.0):
    """Nearest color of the 6x6x6 cube of the 256 color table. Fade scales the brightness."""
    r, g, b = (min(5, int(c * fade * 6 / 256)) for c in rgb)
    return 16 + 36 * r + 6 * g + b


def box_points(center, a, b, c, half):
    pts = []
    for i in range(8):
        sx = half[0] if i & 1 else -half[0]
        sy = half[1] if i & 2 else -half[1]
        sz = half[2] if i & 4 else -half[2]
        pts.append(tuple(center[k] + a[k] * sx + b[k] * sy + c[k] * sz for k in range(3)))
    return pts


class Viewer:
    def __init__(self, width=None, height=None):
        self.width, self.height = width, height
        self.yaw = 0.0
        self.pitch = -0.35
        self.dist = 14.0
        self.fov = math.radians(75)
        self.grid = True
        self.grid_step = 10.0
        self.grid_radius = 50.0
        self.ground = 0.0          # grid height when no human is followed
        self.fade_dist = 150.0
        self._cache = {}

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

    def _color(self, rgb, z):
        if isinstance(rgb, int):
            return rgb
        fade = max(0.3, 1.0 - z / self.fade_dist)
        key = (rgb, int(fade * 16))
        c = self._cache.get(key)
        if c is None:
            c = self._cache[key] = palette(rgb, fade)
        return c

    def _edges(self, cv, cam, pts, edges, rgb):
        cp = [cam.to_cam(p) for p in pts]
        cx, cy, focal = cam.cx, cam.cy, cam.focal
        for i, j in edges:
            ax, ay, az = cp[i]
            bx, by, bz = cp[j]
            if az < NEAR:
                if bz < NEAR:
                    continue
                t = (NEAR - az) / (bz - az)
                ax, ay, az = ax + (bx - ax) * t, ay + (by - ay) * t, NEAR
            elif bz < NEAR:
                t = (NEAR - az) / (bz - az)
                bx, by, bz = ax + (bx - ax) * t, ay + (by - ay) * t, NEAR
            z = (az + bz) * 0.5
            cv.line((cx + ax / az * focal, cy - ay / az * focal, az),
                    (cx + bx / bz * focal, cy - by / bz * focal, bz), self._color(rgb, z))

    @staticmethod
    def _axes(e):
        a = _unit(getattr(e, "orient_a", (1.0, 0.0, 0.0)), (1.0, 0.0, 0.0))
        b = _unit(getattr(e, "orient_b", (0.0, 1.0, 0.0)), (0.0, 1.0, 0.0))
        return a, b, _cross(a, b)

    def _local(self, pos, axes, points):
        a, b, c = axes
        return [tuple(pos[k] + a[k] * p[0] + b[k] * p[1] + c[k] * p[2] for k in range(3)) for p in points]

    def _draw_grid(self, cv, cam, center, y):
        step, rad = self.grid_step, self.grid_radius
        gx = math.floor(center[0] / step) * step
        gz = math.floor(center[2] / step) * step
        pieces = 4
        for k in range(-int(rad // step), int(rad // step) + 1):
            for axis in (0, 1):
                fixed = (gx if axis == 0 else gz) + k * step
                lo = (gz if axis == 0 else gx) - rad
                pts = []
                for s in range(pieces + 1):
                    v = lo + 2 * rad * s / pieces
                    pts.append((fixed, y, v) if axis == 0 else (v, y, fixed))
                self._edges(cv, cam, pts, [(s, s + 1) for s in range(pieces)], COLOR_GRID)

    def render(self, state, boxes=(), me=None, camera=None):
        cols, rows = self.size()
        cv = Canvas(cols, rows - 1)
        humans = list(getattr(state, "humans", None) or ())
        objects = getattr(state, "objects", None) or ()
        items = getattr(state, "items", None) or ()
        city = getattr(state, "cityobjects", None) or ()

        target = None
        for h in humans:
            if me is not None and h.id == me:
                target = h.pos
        if camera is not None:
            cam = self.make_camera(camera[0], camera[1], camera[2], (cols, rows))
            f = cam.f
            norm = math.hypot(f[0], f[2]) or 1.0
            center = (cam.pos[0] + f[0] / norm * 30, 0.0, cam.pos[2] + f[2] / norm * 30)
            ground = self.ground
        else:
            look = target if target is not None else (0.0, 0.0, 0.0)
            f = forward(self.yaw, self.pitch)
            pos = (look[0] - f[0] * self.dist, look[1] + 0.5 - f[1] * self.dist, look[2] - f[2] * self.dist)
            cam = self.make_camera(pos, self.yaw, self.pitch, (cols, rows))
            center = look
            ground = look[1] - 1.0 if target is not None else self.ground

        if self.grid:
            self._draw_grid(cv, cam, center, ground)
        for e in city:
            self._edges(cv, cam, box_points(e.pos, *self._axes(e), CITY_HALF), CUBE_EDGES, COLOR_CITY)
        for e in items:
            self._edges(cv, cam, self._local(e.pos, self._axes(e), CROSS_POINTS), CROSS_EDGES, COLOR_ITEM)
        for e in objects:
            self._edges(cv, cam, box_points(e.pos, *self._axes(e), CAR_HALF), CUBE_EDGES, COLOR_OBJECT)
        for e in humans:
            if me is not None and e.id == me:
                rgb = COLOR_ME
            else:
                hp = getattr(e, "health", 100)
                rgb = COLOR_HUMAN if hp > 60 else COLOR_HURT if hp > 25 else COLOR_DYING
            self._edges(cv, cam, self._local(e.pos, self._axes(e), FIG_POINTS), FIG_EDGES, rgb)
        for lo, hi, label, rgb in boxes:
            center_b = tuple((lo[k] + hi[k]) / 2 for k in range(3))
            half = tuple((hi[k] - lo[k]) / 2 for k in range(3))
            unit = ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
            self._edges(cv, cam, box_points(center_b, *unit, half), CUBE_EDGES, rgb or COLOR_BOX)
            top = self.project((center_b[0], hi[1], center_b[2]), cam)
            if top and label:
                cv.put_text(int(top[0] // 2) - len(label) // 2, int(top[1] // 4) - 1, label,
                            palette(rgb, 1.0) if isinstance(rgb, tuple) else (rgb or palette(COLOR_BOX)))

        out = cv.lines()
        out.append(self._hud(state, cols, len(humans), len(objects), len(items), len(city)))
        return "\n".join(out)

    @staticmethod
    def _hud(state, width, nh, no, ni, nc):
        teams = "/".join(str(t) for t in getattr(state, "team_counts", ()))
        text = " tick %d  phase %d  timer %d  teams %s  money %d  menu %d  slot %d  H%d O%d I%d C%d" % (
            getattr(state, "tick", 0), getattr(state, "phase", 0), getattr(state, "phase_timer", 0), teams,
            getattr(state, "money", 0), getattr(state, "menu", 0), getattr(state, "player", 0), nh, no, ni, nc)
        return "\x1b[48;5;236m\x1b[38;5;252m" + text[:width].ljust(width) + "\x1b[0m"


# --------------------------------------------------------------------------- demo

def demo_scene(t):
    """A fake State at time t seconds: humans that walk in circles, a car and one base box."""
    import srpacket
    humans = []
    for k in range(4):
        ang = t * (0.5 + 0.1 * k) + k * 1.7
        rad = 6 + 3 * k
        heading = ang + math.pi / 2
        humans.append(srpacket.HumanBlock(
            k, pos=(math.cos(ang) * rad, 1.0 + 0.08 * abs(math.sin(t * 6 + k)), math.sin(ang) * rad),
            orient_a=(math.cos(heading + math.pi / 2), 0.0, math.sin(heading + math.pi / 2)),
            health=100 - 30 * k))
    car = srpacket.ObjectBlock(0, pos=((t * 4) % 60 - 30, 0.6, -12), orient_a=(0.0, 0.0, 1.0), orient_b=(0.0, 1.0, 0.0))
    items = [srpacket.ItemBlock(i, pos=(-8 + 4 * i, 0.3, 8)) for i in range(3)]
    city = [srpacket.CityObjectBlock(i, pos=(14 + 5 * i, 0.5, -4)) for i in range(2)]
    state = srpacket.State(tick=int(t * 62), phase=2, phase_timer=int(t * 62) % 3600, team_counts=(1, 0, 0),
                           player=32, money=1250, menu=3, humans=humans, objects=[car], items=items,
                           cityobjects=city)
    boxes = [((-10.0, 0.0, 14.0), (10.0, 5.0, 24.0), "base 0", (255, 110, 110))]
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
            viewer.yaw = 0.3 * t
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
