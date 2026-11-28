#!/usr/bin/env python3
"""Full screen terminal dashboard in the style of an 80s mainframe operator console.

It draws a decoded `srpacket.State` with the wireframe world of `viz.Viewer` and a set of panels:
players, teams, event log, call counters and a clock line. All colors are green phosphor shades
plus a dim amber for warnings. The module makes no network call and no system call.
`render` returns one frame as a string of `height` lines. Each line starts with a clear to end of
line, so the caller prints the frame after a cursor home (`\\x1b[H`) and nothing else.

    mf = mainframe.Mainframe()
    sys.stdout.write("\\x1b[H" + mf.render(state, boxes, me, extra={"tps": 61.2, "calls": {...}}))

`extra` is an optional dict. All keys are optional:
    tps (float), clock (int), teams (list of {'manager', 'members', 'missions'}),
    calls (dict name -> count), log (list of str), recorder (bool), bot (str)

    python tools/net/mainframe.py --demo        animated fake data, Ctrl+C to stop (--no-boot skips the boot)
                                                The main view uses numpy (tools/net/vector.py) when it is there, and
                                                braille dots when it is not (python -I never finds numpy).
                                                --sixel draws the view as a sixel image, --braille as braille dots,
                                                --half as half blocks, --dots as the old dots without numpy.
    python tools/net/mainframe.py --frame       one frame of the demo
    python tools/net/mainframe.py --boot        only the boot sequence
"""
# References for the look. Only the ideas are used. No text or art is copied.
#   IBM 3270 operator information area (status line at the bottom, input inhibited lamp)
#       https://learn.microsoft.com/en-us/host-integration-server/core/oia-inidcators2
#       https://help.hcl-software.com/zie/zieweb/2.0/doc/troubleshoot/oia.html
#   DEC VT100 technical manual (80 by 24 character grid, upper case labels)
#       https://vt100.net/docs/vt100-tm/chapter2.html
#   cool-retro-term (scanlines, slow phosphor persistence)
#       https://github.com/Swordfish90/cool-retro-term
#   Alien title design notes (green monochrome ship computer)
#       https://typesetinthefuture.com/2014/12/01/alien/
#   WarGames screens (small fixed character grids, plain status text)
#       https://www.cio.com/article/3404461/the-technology-of-wargames.html
#   Braille terminal graphics and depth shaded wireframes
#       https://danieledapo.github.io/post/terminal-graphics-braille
#   btop (panels with meters that change color by value)
#       https://github.com/aristocratos/btop
# Details used:
#   1. The bottom line is a status line like the 3270 operator information area.
#      A blinking amber "X INPUT INHIBITED" lamp shows when the link is not live.
#   2. Upper case labels in fixed columns, as on a 80 column terminal.
#   3. Dotted leaders between a name and its value in the call counters.
#   4. Phosphor persistence: moving things leave a trail (the vector view keeps the last picture,
#      the braille view draws old positions of humans as dim marks).
#   5. Depth shading: far lines get a darker green, near lines a brighter green.
#   6. Meters change shade by value (bright when high), and every other row is dimmer (scanlines).
from __future__ import annotations

import math
import os
import re
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viz  # noqa: E402

# Every word the console prints is in this table. Upper case, short noun phrases, no sentences.
TXT = {
    "title": "SUB ROSA OPERATIONS CONSOLE",
    "version": "VERSION 24D",
    "node": "NODE 01",
    "uptime": "UPTIME",
    "lamps": {"LINK": "LINK", "WORLD": "SIM", "ROUND": "ROUND", "RECORDER": "RECORDER"},     # the BOT lamp has no label: not shown
    "player": "PLAYER",
    "no_link": "ERR 04 NO LINK",
    "stalled": "STAT HOLD",
    "no_round": "STAT READY",
    "nominal": "STAT ACTIVE",
    "stat_ready": "STAT READY",
    "view": "MAIN VIEW",
    "players": "PLAYERS",
    "players_head": ("SLOT", "HP", "X", "Z", "TM"),
    "more": "MORE",
    "teams": "TEAMS",
    "teams_head": ("TEAM", "MGR", "MBR", "MSN"),
    "log": "OPERATOR LOG",
    "message": "SRC060I",         # message id of a chat line in the log
    "event_unknown": "SRC099I EVENT TYPE",
    "calls": "CALL COUNTERS",
    "no_recording": "REC INACTIVE",
    "clock": "SIM CLOCK",
    "tick": "TICK",
    "tps": "TPS",
    "phase": "PHASE",
    "timer": "TIMER",
    "phases": {1: "WAITING", 2: "ROUND", 3: "ROUND OVER"},
    "wire": ("HOLLOW", "SOLID"),
    "keys": "F1 HELP  F2 CAMERA  F3 WIRE  F4 RENDER  F9 SNAP  F12 QUIT   ARROWS A D TURN  W S TILT  + - ZOOM  M MAP",
    "help": "H HOLLOW  L LABELS  R GRID  B BLDG  T TRAILS  O GLOW  C COMPASS  V ORBIT  G FREE  F FOLLOW  M MAP  E RENDER  P SNAP  Q QUIT",
    "unknown": "UNKNOWN",
    "no_data": "NO DATA",
    "ready": "READY",
    "inhibited": "X INPUT INHIBITED",
    "too_small": "ERR 01 DISPLAY TOO SMALL",
    "need": "NEED",
    "have": "HAVE",
    "bios": "BIOS VERSION 24D",
    "boot_sub": "SERVER OPERATOR CONSOLE",
    "memtest": "MEMORY TEST",
    "device": "DEVICE",
    "devices": ("I/O 00  PACKET INTERFACE", "I/O 01  RECORDER UNIT", "I/O 02  WIREFRAME DISPLAY",
                "CPU 00  X87 COPROCESSOR"),
    "loading": "LOADING SUBROSA24D.EXE",
    "init": "INITIALIZING SIMULATION",
    "system_ready": "SYSTEM READY",
}
MIN_COLS, MIN_ROWS = 100, 30
# Phosphor trail: marks kept per human, seconds between marks, marks drawn at most, and two gray codes that map to
# the darkest and the next green shade.
TRAIL_LEN, TRAIL_GAP, TRAIL_MAX = 3, 0.12, 10
TRAIL_DIM, TRAIL_MID = 234, 238

# Shades of the 256 color table. Green ramp from dark to bright, a white-green highlight, dim amber.
DK, LO, MID, HI, BR = 22, 28, 34, 40, 46
WH = 194
WH_DIM = 157
AMB, AMB_DIM = 136, 94
BLACK = 16

RESET = "\x1b[0m"
_BG = "\x1b[48;5;%dm" % BLACK


def _fg(n):
    return RESET + _BG + "\x1b[38;5;%dm" % n


S_DK, S_LO, S_MID, S_HI, S_BR, S_WH = (_fg(n) for n in (DK, LO, MID, HI, BR, WH))
S_AMB, S_AMB_DIM = _fg(AMB), _fg(AMB_DIM)
S_BASE = S_MID
S_BAR = RESET + "\x1b[48;5;%dm\x1b[38;5;%dm" % (MID, BLACK)       # reverse video bar
S_ME = RESET + "\x1b[48;5;%dm\x1b[38;5;%dm" % (DK, WH)            # the bot's own line
S_BAR_AMB = RESET + "\x1b[48;5;%dm\x1b[38;5;%dm" % (AMB, BLACK)

# One step dimmer, for every other row (the scanline effect).
DIM = {WH: WH_DIM, WH_DIM: BR, BR: HI, HI: MID, MID: LO, LO: DK, DK: DK, AMB: AMB_DIM, AMB_DIM: AMB_DIM}
_FG_CODE = re.compile(r"\x1b\[38;5;(\d+)m")
_DIM_SEQ = {str(k): "\x1b[38;5;%dm" % v for k, v in DIM.items()}

# Frame characters.
H2, V2, TL, TR, BL, BRC = "═", "║", "╔", "╗", "╚", "╝"
FULL, LIGHT = "█", "░"
EIGHTHS = " ▏▎▍▌▋▊▉█"


def _dim_row(row):
    return _FG_CODE.sub(lambda m: _DIM_SEQ.get(m.group(1), m.group(0)), row)


# --------------------------------------------------------------------------- viewer colors

_LEVELS = (0, 95, 135, 175, 215, 255)


def _cube_rgb(n):
    if n >= 232:
        v = 8 + 10 * (n - 232)
        return v, v, v
    if n >= 16:
        n -= 16
        return _LEVELS[n // 36], _LEVELS[(n // 6) % 6], _LEVELS[n % 6]
    return 128, 128, 128


def _lum(rgb):
    return (0.3 * rgb[0] + 0.59 * rgb[1] + 0.11 * rgb[2]) / 255.0


def _shade(lum):
    if lum < 0.25:
        return DK
    if lum < 0.42:
        return LO
    if lum < 0.58:
        return MID
    if lum < 0.74:
        return HI
    return BR


def _build_view_map():
    """Table from any 256 color code of the viewer to a green shade, by brightness."""
    table = [_shade(_lum(_cube_rgb(n))) for n in range(256)]
    for k in range(101):
        fade = 0.3 + 0.7 * k / 100
        code = viz.palette(viz.COLOR_GRID, fade)
        table[code] = min(_shade(_lum(viz.COLOR_GRID) * fade), LO)
        for rgb in (viz.COLOR_HURT, viz.COLOR_DYING):
            table[viz.palette(rgb, fade)] = AMB if fade >= 0.6 else AMB_DIM
        table[viz.palette(viz.COLOR_ME, fade)] = WH if fade >= 0.6 else BR
    return table


_VIEW_MAP = _build_view_map()
_VIEW_SEQ = {str(n): "\x1b[38;5;%dm" % _VIEW_MAP[n] for n in range(256)}
_VIEW_END = RESET + "\x1b[K"


def green_row(row):
    """One row of the viewer frame with every color mapped to a green shade."""
    if row.endswith(_VIEW_END):
        row = row[:-len(_VIEW_END)]
    return _FG_CODE.sub(lambda m: _VIEW_SEQ.get(m.group(1), m.group(0)), row)


# --------------------------------------------------------------------------- phosphor theme

AMBER_RGB = (255, 176, 20)
ME_RGB = (170, 255, 200)
_AMBER_SOURCES = {viz.COLOR_HURT, viz.COLOR_DYING}


def phosphor(rgb):
    """A scene color in the green phosphor theme: brightness stays, hue becomes green.
    Hurt humans are amber. The human of the bot is a white green."""
    if rgb in _AMBER_SOURCES:
        return AMBER_RGB
    if rgb == viz.COLOR_ME:
        return ME_RGB
    g = 255.0 * min(1.0, _lum(rgb) * 1.25) ** 0.85
    return (int(g * 0.10), int(g), int(g * 0.28))


class FastViewer(viz.Viewer):
    """The braille viewer in the phosphor theme. Colors go straight to the shades of the console."""

    def tint(self, rgb):
        return phosphor(rgb)

    def _color(self, rgb, inten):
        if rgb == ME_RGB:
            return WH if inten > 0.6 else BR
        level = min(1.0, inten * 1.25)
        if rgb[0] > rgb[1]:
            return AMB if level >= 0.6 else AMB_DIM
        return _shade(_lum(rgb) * level * 1.1)


def make_viewer(vector, sixel, cell, backend=None):
    """The vector viewer when numpy is there (and vector is not False), else the dots viewer. The backend is
    sixel, braille or halfblock; None picks sixel when the terminal has it, else braille."""
    if vector is not False:
        try:
            import vector as vector_mod
            return vector_mod.VectorViewer(backend="sixel" if sixel else backend or "auto", cell=cell, theme=phosphor)
        except ImportError:
            pass
    return FastViewer()


# --------------------------------------------------------------------------- text helpers

def _line(segs, width):
    """Join (style, text) pairs to a string of exactly `width` visible characters."""
    out = []
    n = 0
    for style, text in segs:
        if n >= width:
            break
        if len(text) > width - n:
            text = text[:width - n]
        out.append(style)
        out.append(text)
        n += len(text)
    out.append(S_BASE)
    if n < width:
        out.append(" " * (width - n))
    out.append(RESET)
    return "".join(out)


def _split_line(left, right, width):
    """Left segments at the left edge, right segments at the right edge."""
    rl = sum(len(t) for _, t in right)
    ll = sum(len(t) for _, t in left)
    gap = max(1, width - ll - rl)
    return _line(left + [(S_BASE, " " * gap)] + right, width)


def _fit(text, width):
    return text if len(text) <= width else text[:width]


def _clock(seconds):
    s = int(max(0, seconds))
    return "%02d:%02d:%02d" % (s // 3600 % 100, s // 60 % 60, s % 60)


def _box(title, info, rows, width, height):
    """A double line frame. Content rows must be exactly width - 2 visible characters."""
    inner = width - 2
    head = " %s " % title
    tail = " %s " % info if info else ""
    fill = max(0, inner - 1 - len(head) - len(tail) - (1 if tail else 0))
    top = [S_MID + TL + H2, S_BAR + head, S_MID + H2 * fill]
    if tail:
        top += [S_WH + tail, S_MID + H2]
    out = ["".join(top) + S_MID + TR + RESET]
    for i in range(height - 2):
        body = rows[i] if i < len(rows) else " " * inner
        out.append(S_MID + V2 + body + S_MID + V2 + RESET)
    out.append(S_MID + BL + H2 * inner + BRC + RESET)
    return out


def _bar(frac, width):
    frac = min(1.0, max(0.0, frac))
    eighths = int(round(frac * width * 8))
    full, part = divmod(eighths, 8)
    text = FULL * full + (EIGHTHS[part] if part and full < width else "")
    return text + " " * (width - len(text))


# --------------------------------------------------------------------------- reel

def reel(angle):
    """A 3 by 2 cell braille tape reel with three spokes. Returns two strings."""
    dots = set()
    cx, cy, rad = 2.5, 3.5, 2.8
    for k in range(20):
        a = 2 * math.pi * k / 20
        dots.add((int(round(cx + rad * math.cos(a))), int(round(cy + rad * math.sin(a)))))
    for s in range(3):
        a = angle + 2 * math.pi * s / 3
        for r in (0.6, 1.4, 2.2):
            dots.add((int(round(cx + r * math.cos(a))), int(round(cy + r * math.sin(a)))))
    rows = []
    for ry in range(2):
        cells = []
        for rx in range(3):
            m = 0
            for yy in range(4):
                for xx in range(2):
                    if (rx * 2 + xx, ry * 4 + yy) in dots:
                        m |= viz.BIT[(yy << 1) | xx]
            cells.append(chr(0x2800 + m))
        rows.append("".join(cells))
    return rows


# --------------------------------------------------------------------------- event text

def _text_of(value):
    if isinstance(value, (bytes, bytearray)):
        value = value.decode("ascii", "replace")
    return "".join(ch if 32 <= ord(ch) < 127 else "?" for ch in str(value))


def _stamp(seconds):
    s = int(max(0, seconds))
    return "%02d:%02d:%02d" % (s // 3600 % 100, s // 60 % 60, s % 60)


# Known events: type -> (message id, text, fields). The text may hold %(name)s fields of the event.
EVENT_TEXT = {
    0: ("SRC040I", "BULLET FIRED  SLOT %(shooter)03d"),
    1: ("SRC041W", "BULLET HIT  TARGET %(target)03d"),
    3: ("SRC042I", "OBJECT %(object)04d CREATED  TYPE %(object_type)03d"),
    4: ("SRC043I", "OBJECT %(object)04d UPDATED"),
    5: ("SRC044I", "ITEM %(item_key)05d UPDATED"),
    8: ("SRC045I", "FINANCE UPDATED  SLOT %(player)03d"),
    9: ("SRC046I", "SOUND %(sound)03d"),
    11: ("SRC047I", "SCENARIO UPDATED"),
    12: ("SRC031I", "MISSION UPDATED"),
    13: ("SRC032I", "STOCKS UPDATED"),
    14: ("SRC033I", "CAR MARKET UPDATED"),
    17: ("SRC014I", "HUMAN %(human)03d UPDATED"),
}


class _Fields(dict):
    """Event fields for a format: a missing or odd value is 0."""

    def __missing__(self, key):
        return 0

    def __getitem__(self, key):
        v = dict.get(self, key, 0)
        return v if isinstance(v, int) else 0


def event_lines(events):
    """Operator log lines for decoded events: (kind, text). kind is 'msg' for a chat message.
    The clock stamp counts the server ticks (62 each second)."""
    out = []
    for ev in events:
        tick = getattr(ev, "tick", 0)
        stamp = _stamp(tick / 62.0)
        kind = getattr(ev, "type", None)
        fields = getattr(ev, "fields", None) or {}
        if kind == 2:
            out.append(("msg", "%s %s MSG %s" % (stamp, TXT["message"], _text_of(fields.get("text", b"")))))
        elif kind == 10:
            out.append(("evt", "%s SRC048I DOOR %03d %s" % (stamp, fields.get("door", 0), "OPEN" if fields.get("open") else "CLOSED")))
        elif kind in EVENT_TEXT:
            ident, text = EVENT_TEXT[kind]
            text = text % _Fields(fields) if "%(" in text else text
            out.append(("evt", "%s %s %s" % (stamp, ident, text)))
        else:
            out.append(("evt", "%s %s 0x%02X" % (stamp, TXT["event_unknown"], int(kind or 0))))
    return out


# --------------------------------------------------------------------------- the console

class Mainframe:
    def __init__(self, width=None, height=None, vector=None, sixel=False, cell=(10, 20), backend=None):
        self.width, self.height = width, height
        self.viewer = make_viewer(vector, sixel, cell, backend)
        self.vector = hasattr(self.viewer, "picture")      # true for the numpy viewer
        self.sleep = time.sleep
        self.now = time.monotonic
        self._t0 = self.now()
        self._last_tick = None
        self._last_change = self._t0
        self._trail = {}
        self.view_top = 4                                   # rows above the main view
        self.sixel = None

    def size(self):
        if self.width and self.height:
            return self.width, self.height
        ts = shutil.get_terminal_size((80, 24))
        return self.width or ts.columns, self.height or ts.lines

    # -- lamps
    def _lamps(self, state, extra, now):
        """Returns a list of (label, level) with level 2 = on, 1 = warning, 0 = off."""
        link = 0
        if state is not None:
            tick = getattr(state, "tick", 0)
            if tick != self._last_tick:
                self._last_tick = tick
                self._last_change = now
            link = 2 if now - self._last_change < 3.0 else 1
        world = 0
        if state is not None and (getattr(state, "world", 0) or getattr(state, "humans", None)
                                  or getattr(state, "objects", None) or getattr(state, "cityobjects", None)
                                  or getattr(state, "items", None)):
            world = 2
        rnd = 0
        if state is not None:
            phase = getattr(state, "phase", 0)
            rnd = 2 if phase == 2 else 1 if phase in (1, 3) else 0
        return [("LINK", link), ("WORLD", world), ("ROUND", rnd),
                ("RECORDER", 2 if extra.get("recorder") else 0), ("BOT", 2 if extra.get("bot") else 0)]

    # -- phosphor persistence
    def _trails(self, state, now):
        """Small dim boxes at the old positions of humans that moved. They fade over a few frames.
        The number of marks is capped, newest first, so a crowd does not slow the frame."""
        humans = (getattr(state, "humans", None) or ()) if state is not None else ()
        seen = set()
        tiers = [[] for _ in range(TRAIL_LEN)]
        for h in humans:
            seen.add(h.id)
            hist = self._trail.setdefault(h.id, [])
            x, y, z = h.pos
            if not hist or (abs(hist[-1][0] - x) + abs(hist[-1][1] - y) + abs(hist[-1][2] - z) > 0.6
                            and now - hist[-1][3] >= TRAIL_GAP):
                hist.append((x, y, z, now))
                del hist[:-TRAIL_LEN - 1]
            for k, (px, py, pz, _t) in enumerate(reversed(hist[:-1])):
                if abs(px - x) + abs(py - y) + abs(pz - z) >= 0.3:
                    tiers[k].append((px, py, pz, TRAIL_MID if k == 0 else TRAIL_DIM))
        for key in [k for k in self._trail if k not in seen]:
            del self._trail[key]
        out = []
        for tier in tiers:
            for px, py, pz, code in tier:
                if len(out) >= TRAIL_MAX:
                    return out
                out.append(((px - 0.15, py - 0.15, pz - 0.15), (px + 0.15, py + 0.15, pz + 0.15), "", code))
        return out

    # -- panels
    def _header(self, state, extra, lamps, width, now, tick):
        inner = width - 2
        blink = int((now - self._t0) * 2) % 2 == 0
        up = _clock(now - self._t0)
        r0, r1 = reel(tick * 0.31), reel(tick * 0.23)
        left1 = [(S_BR, " %s      %s      %s " % (TXT["title"], TXT["version"], TXT["node"])), (S_WH, FULL if blink else " ")]
        right1 = [(S_LO, TXT["uptime"] + " "), (S_HI, up), (S_BASE, "  "), (S_BR, r0[0] + " " + r1[0]), (S_BASE, " ")]
        left2 = [(S_BASE, " ")]
        for label, level in lamps:
            if label not in TXT["lamps"]:
                continue
            lamp, style = ("■", S_BR) if level == 2 else ("■", S_AMB) if level == 1 else ("□", S_DK)
            left2 += [(S_LO, TXT["lamps"].get(label, label) + " "), (style, lamp), (S_BASE, "   ")]
        if state is None:
            status = [(S_AMB, TXT["no_link"])]
        elif lamps[0][1] == 1:
            status = [(S_AMB, TXT["stalled"])]
        elif lamps[2][1] == 1:
            status = [(S_AMB, TXT["no_round"])]
        else:
            status = [(S_LO, TXT["nominal"])]
        status = [(S_BASE, "   ")] + status
        if extra.get("bot"):
            status = [(S_LO, TXT["player"] + " "), (S_HI, _text_of(extra["bot"])[:12].upper() + "  ")] + status
        right2 = status + [(S_BASE, "  "), (S_BR, r0[1] + " " + r1[1]), (S_BASE, " ")]
        out = [S_MID + TL + H2 * inner + TR + RESET]
        for lf, rt in ((left1, right1), (left2, right2)):
            out.append(S_MID + V2 + _split_line(lf, rt, inner) + S_MID + V2 + RESET)
        out.append(S_MID + BL + H2 * inner + BRC + RESET)
        return out

    def _view(self, state, boxes, me, camera, width, height):
        inner_w, inner_h = width - 2, height - 2
        v = self.viewer
        v.width, v.height = inner_w, inner_h + 1
        if self.vector:
            v.sixel_origin = (self.view_top + 2, 2)             # the first cell inside the frame, counted from 1
        rows = v.render(state, boxes, me, camera).split("\n")[:inner_h]
        if self.vector:
            self.sixel = getattr(v, "sixel_data", None)
            rows = [S_BASE + (r[:-len(_VIEW_END)] if r.endswith(_VIEW_END) else r) + RESET for r in rows]
        else:
            rows = [S_BASE + green_row(r) + RESET for r in rows]
        info = "%s  WIRE %s" % (v.cam_text(), TXT["wire"][0 if v.hollow else 1])
        room = inner_w - len(TXT["view"]) - 8
        if len(info) > room:
            info = v.cam_text()                              # the camera line has the first claim on the room
        return _box(TXT["view"], info[:max(0, room)], rows, width, height)

    def _players(self, state, me, width, height, extra=None):
        inner = width - 2
        humans = sorted(getattr(state, "humans", None) or (), key=lambda h: h.id) if state is not None else []
        teams_of = (extra or {}).get("teams_of") or {}
        cap = height - 3
        shown = humans[:cap]
        if len(humans) > cap:
            shown = humans[:cap - 2]
            mine = [h for h in humans if me is not None and h.id == me]
            if mine and mine[0] not in shown:
                shown.append(mine[0])
        head = TXT["players_head"]
        rows = [_line([(S_LO, "  %-4s  %3s  %5s  %5s  %3s" % head)], inner)]
        for h in shown:
            hp = int(getattr(h, "health", 0))
            mine = me is not None and h.id == me
            x, _y, z = (int(round(c)) for c in h.pos)
            team = teams_of.get(h.id)
            text = "%s %03d  %03d  %05d  %05d  %3s" % (">" if mine else " ", h.id, max(0, min(999, hp)), x, z,
                                                       "%02d" % team if team is not None and team < 7 else "--")
            if mine:
                style = S_ME
            else:
                style = S_BR if hp > 60 else S_HI if hp > 25 else S_AMB
            rows.append(_line([(style, " " + text)], inner))
        hidden = len(humans) - len(shown)
        if hidden > 0 and len(rows) < height - 2:
            rows.append(_line([(S_AMB_DIM, " +%03d %s" % (hidden, TXT["more"]))], inner))
        return _box(TXT["players"], "%03d" % len(humans), rows, width, height)

    def _teams(self, state, extra, width, height):
        inner = width - 2
        teams = extra.get("teams")
        if not teams:
            counts = getattr(state, "team_counts", ()) if state is not None else ()
            teams = [{"manager": -1, "members": c, "missions": 0} for c in counts]
        fmt = " %-4s   %5s  %5s  %5s"
        rows = [_line([(S_LO, fmt % TXT["teams_head"])], inner)]
        for i, t in enumerate(teams[:height - 3]):
            mgr = t.get("manager", -1)
            rows.append(_line([(S_HI, fmt % ("%02d" % i, "---" if mgr is None or mgr < 0 else "%03d" % mgr,
                                              "%03d" % t.get("members", 0), "%03d" % t.get("missions", 0)))], inner))
        return _box(TXT["teams"], "", rows, width, height)

    def _log(self, state, extra, width, height):
        inner = width - 2
        items = [("log", _text_of(s).upper()) for s in (extra.get("log") or ())]
        if state is not None:
            items += [(kind, text.upper()) for kind, text in event_lines(getattr(state, "events", None) or ())]
        keep = min(12, height - 2)
        rows = []
        style = {"msg": S_BR, "evt": S_LO, "log": S_HI}
        for kind, text in items[-keep:]:
            rows.append(_line([(S_MID, " "), (style[kind], text)], inner))
        return _box(TXT["log"], "%03d" % len(items), rows, width, height)

    def _calls(self, extra, width, height):
        inner = width - 2
        calls = extra.get("calls") or {}
        top = sorted(calls.items(), key=lambda kv: (-kv[1], kv[0]))[:min(8, height - 2)]
        rows = []
        namew = 18
        barw = max(2, inner - (namew + 10))
        peak = top[0][1] if top else 1
        for name, count in top:
            frac = count / peak if peak else 0
            label = (str(name).upper()[:namew - 2] + " ").ljust(namew, ".")
            rows.append(_line([(S_HI, " " + label + " "), (S_BR, "%7s " % ("%03d" % count)),
                               (S_BR if frac >= 0.66 else S_HI if frac >= 0.33 else S_MID, _bar(frac, barw))], inner))
        if not top:
            rows.append(_line([(S_AMB_DIM, " " + TXT["no_recording"])], inner))
        return _box(TXT["calls"], "", rows, width, height)

    def _clock_line(self, state, extra, lamps, width, now):
        if state is None:
            text = " " + TXT["no_data"]
        else:
            phase = TXT["phases"].get(getattr(state, "phase", 0), TXT["unknown"])
            tps = extra.get("tps")
            text = " %s %s   %s %06d   %s %s   %s %s   %s %05d" % (
                TXT["clock"], "%d" % extra["clock"] if extra.get("clock") is not None else "--",
                TXT["tick"], getattr(state, "tick", 0), TXT["tps"], "%05.1f" % tps if tps is not None else "--",
                TXT["phase"], phase, TXT["timer"], getattr(state, "phase_timer", 0))
        # Right end: the status area. A blinking amber lamp shows when the link is not live.
        if lamps[0][1] == 2:
            right, style = TXT["ready"] + " ", S_BAR
        elif int(now * 2) % 2 == 0:
            right, style = TXT["inhibited"] + " ", S_BAR_AMB
        else:
            right, style = " " * 18, S_BAR
        return _line([(S_BAR, text.ljust(width - len(right))), (style, right)], width)

    # -- frames
    def _too_small(self, width, height):
        msgs = [TXT["too_small"], "%s %dX%d   %s %dX%d" % (TXT["need"], MIN_COLS, MIN_ROWS, TXT["have"], width, height)]
        w = max(len(m) for m in msgs) + 6
        lines = [S_MID + TL + H2 * (w - 2) + TR + RESET]
        for i, m in enumerate(msgs):
            style = S_AMB if i == 0 else S_HI
            lines.append(S_MID + V2 + _line([(style, m.center(w - 2))], w - 2) + S_MID + V2 + RESET)
        lines.append(S_MID + BL + H2 * (w - 2) + BRC + RESET)
        pad = max(0, (width - w) // 2)
        top = max(0, (height - len(lines)) // 2)
        plain = [len(re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", s)) for s in lines]
        rows = []
        for s, n in zip(lines, plain):
            if n > width:                       # a very narrow terminal: cut the plain text
                s = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", s)[:width]
            else:
                s = " " * pad + s
            rows.append(s)
        blank = ""
        frame = [blank] * top + rows
        frame = (frame + [blank] * height)[:height]
        return "\n".join("\x1b[48;5;%dm\x1b[K" % BLACK + r + RESET for r in frame)

    def _key_bar(self, width):
        """The last line: the PF keys in the style of a 3270 screen, or the help line of the toggles."""
        text = TXT["help"] if self.viewer.help else TXT["keys"]
        return _line([(S_WH if self.viewer.help else S_HI, " " + text)], width)

    def key(self, name):
        """A key of the user, as a name of viz.Viewer.key. Returns True when the name is known."""
        return self.viewer.key(name)

    def render(self, state, boxes=(), me=None, extra=None, camera=None):
        extra = extra or {}
        width, height = self.size()
        if width < MIN_COLS or height < MIN_ROWS:
            return self._too_small(width, height)
        now = self.now()
        tick = getattr(state, "tick", 0) if state is not None else int((now - self._t0) * 20)
        lamps = self._lamps(state, extra, now)
        boxes = list(boxes) if self.vector else list(boxes) + self._trails(state, now)
        rows = self._header(state, extra, lamps, width, now, tick)
        body = height - 6
        lw = max(60, int(width * 0.6))
        rw = width - lw
        log_h = min(14, max(5, body - 12))
        view_h = body - log_h
        calls_h, teams_h = 10, 6
        players_h = body - calls_h - teams_h
        left = self._view(state, boxes, me, camera, lw, view_h) + self._log(state, extra, lw, log_h)
        right = (self._players(state, me, rw, players_h, extra) + self._teams(state, extra, rw, teams_h)
                 + self._calls(extra, rw, calls_h))
        rows += [a + b for a, b in zip(left, right)]
        rows.append(self._clock_line(state, extra, lamps, width, now))
        rows.append(self._key_bar(width))
        out = []
        for i, row in enumerate(rows):
            if i & 1:
                row = _dim_row(row)
            out.append("\x1b[48;5;%dm\x1b[K" % BLACK + row)
        frame = "\n".join(out)
        if self.vector and self.sixel:
            frame += self.sixel                              # the image goes over the empty view area
        return frame

    # -- boot sequence
    def boot(self, out, skip=False):
        """Print an old style boot sequence of about 2 seconds. skip=True prints nothing."""
        if skip:
            return

        def put(text, style=S_HI):
            out.write(style + text + RESET)
            if hasattr(out, "flush"):
                out.flush()

        put(TXT["title"] + "   " + TXT["bios"] + "\r\n", S_BR)
        put(TXT["boot_sub"] + "\r\n\r\n", S_LO)
        self.sleep(0.2)
        steps = 16
        for i in range(steps + 1):
            put("\r" + TXT["memtest"] + " %05dK" % (640 * i // steps))
            self.sleep(0.5 / steps)
        put("  OK\r\n", S_BR)
        for dev in TXT["devices"]:
            self.sleep(0.12)
            put(TXT["device"] + " %-26s ... " % dev, S_LO)
            put("OK\r\n", S_BR)
        self.sleep(0.15)
        put("\r\n" + TXT["loading"] + " ... ", S_HI)
        self.sleep(0.15)
        put("OK\r\n", S_BR)
        put(TXT["init"] + "  [", S_HI)
        bar = 30
        for i in range(bar):
            put(FULL, S_BR)
            self.sleep(0.4 / bar)
        put("]\r\n\r\n", S_HI)
        put(TXT["system_ready"] + "\r\n", S_WH)
        self.sleep(0.15)


# --------------------------------------------------------------------------- demo

_CALL_RATES = {"humansimulation": 62, "bondsimulation2": 31, "objectsimulation": 48, "serversend": 62,
               "unpackevent": 14, "packevent": 11, "itemsimulation": 40, "createevent_updateitem": 3,
               "physics_step": 120, "sendpacket": 22}
_LOG_LINES = ("SRC011I PLAYER 003 JOINED  TEAM 2", "SRC012I MISSION 004 STARTED", "SRC013W BULLET HIT  SLOT 005",
              "SRC014I ITEM 0017 PICKUP", "SRC015I VEHICLE 002 ENTERED  SLOT 006", "SRC016I DOOR 0009 OPEN",
              "SRC017I ROUND TIMER SYNC", "SRC018I SNAPSHOT WRITTEN", "SRC019I PLAYER 008 LEFT", "SRC020I STOCKS UPDATED")


def demo_data(t):
    """Fake (state, boxes, me, extra) at time t seconds."""
    import srpacket
    state, boxes, me = viz.demo_scene(t)
    for k in range(10):
        ang = t * (0.2 + 0.03 * k) + k
        state.humans.append(srpacket.HumanBlock(
            10 + k, pos=(math.cos(ang) * (20 + 2 * k), 1.0, math.sin(ang) * (20 + 2 * k)),
            orient_a=(math.cos(ang), 0.0, math.sin(ang)), health=100 - (k * 17) % 90))
    state.team_counts = (3, 4, 2)
    sec = int(t)
    state.events = [srpacket.Event(srpacket.EV_MESSAGE, state.tick - 40, {"text": b"welcome to the console"}),
                    srpacket.Event(srpacket.EV_UPDATEITEM, state.tick - 20),
                    srpacket.Event(srpacket.EV_BULLET, state.tick - 3)]
    extra = {
        "tps": 61.5 + 1.5 * math.sin(t),
        "clock": 40000 + int(t * 62),
        "teams": [{"manager": 3, "members": 3, "missions": 1 + sec // 9 % 3},
                  {"manager": 8, "members": 4, "missions": 2},
                  {"manager": -1, "members": 2, "missions": 0}],
        "calls": {n: int(r * t * (1 + 0.1 * math.sin(t + i))) for i, (n, r) in enumerate(_CALL_RATES.items())},
        "log": ["%s" % _LOG_LINES[s % len(_LOG_LINES)] for s in range(max(0, sec - 14), sec + 1)],
        "recorder": True,
        "bot": "bot01",
    }
    return state, boxes, me, extra


def _main(argv):
    width = height = None
    for i, a in enumerate(argv):
        if a == "--width":
            width = int(argv[i + 1])
        elif a == "--height":
            height = int(argv[i + 1])
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if (width is None or height is None) and not sys.stdout.isatty():
        width, height = width or 120, height or 40
    backend = "braille" if "--braille" in argv else "halfblock" if "--half" in argv else None
    mf = Mainframe(width, height, vector=False if "--dots" in argv else None, sixel="--sixel" in argv, backend=backend)
    if "--frame" in argv:
        state, boxes, me, extra = demo_data(1.0)
        sys.stdout.write(mf.render(state, boxes, me, extra) + "\n")
        return 0
    if "--boot" in argv:
        if os.name == "nt":
            os.system("")
        try:
            mf.boot(sys.stdout)
        except KeyboardInterrupt:
            pass
        sys.stdout.write(RESET)
        return 0
    if "--demo" not in argv:
        print(__doc__)
        return 2
    if os.name == "nt":
        os.system("")                       # turns on ANSI escapes in the Windows console
    sys.stdout.write("\x1b[?1049h\x1b[?25l\x1b[2J")
    try:
        mf.boot(sys.stdout, skip="--no-boot" in argv)
        sys.stdout.write("\x1b[2J")
        start = time.time()
        last = None
        while True:
            t = time.time() - start
            size = mf.size()
            if size != last:
                sys.stdout.write("\x1b[2J")
                last = size
            state, boxes, me, extra = demo_data(t)
            mf.viewer.yaw_off = 0.3 * t
            sys.stdout.write("\x1b[H" + mf.render(state, boxes, me, extra))
            sys.stdout.flush()
            time.sleep(max(0.0, 1 / 15 - (time.time() - start - t)))
    except KeyboardInterrupt:
        pass
    finally:
        sys.stdout.write(RESET + "\x1b[?25h\x1b[?1049l\n")
    return 0


if __name__ == "__main__":
    sys.exit(_main(sys.argv[1:]))
