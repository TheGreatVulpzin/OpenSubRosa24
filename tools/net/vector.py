#!/usr/bin/env python3
"""A vector display renderer for the wireframe scene of viz.py. It needs numpy.

The scene comes as 3D lines (`viz.Lines`). The renderer draws them like the beam of a vector
display. Each line is anti-aliased and adds its light to a float RGB buffer, so crossings glow.
A line fades with depth and gets thicker when it is near. Heads and wheels are filled discs.
Then the picture goes through a post process (all vectorized):

  1. a box filter down from the supersampled buffer to the output pixels,
  2. phosphor persistence: the buffer keeps the last frame times `persist`,
  3. bloom: two blurs of different radius are added back,
  4. faint scanlines, a vignette and a tone map to 8 bits.

Two outputs. `halfblock` prints each cell as U+2580 with the upper pixel as foreground and the
lower pixel as background, in truecolor. `sixel` makes a DCS sixel image from the pixels.
The module makes no network call and no system call, except `query_cell_size`, which talks to
the terminal.

    v = vector.VectorViewer(140, 44)
    sys.stdout.write("\\x1b[H" + v.render(state, boxes, me))

    python tools/net/vector.py --frame [--sixel]    one frame of the demo scene, with timing
"""
from __future__ import annotations

import math
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viz  # noqa: E402

NEAR = viz.NEAR
STEP = 0.6                   # distance between two samples of a line, in pixels of the buffer

# --------------------------------------------------------------------------- line rasterizer


def to_array(lines):
    """The (N, 13) float array of a `viz.Lines`. The array is kept until the line count changes."""
    c = lines.cache
    if c is not None and c[0] == len(lines.segs):
        return c[1]
    arr = np.array(lines.segs, dtype=np.float64) if lines.segs else np.zeros((0, 13))
    lines.cache = (len(lines.segs), arr)
    return arr


def project(segs, cam, W, H, floor):
    """Segments in world space to screen space.

    Returns a dict of arrays: x0, y0, x1, y1 (pixels), iz0, iz1 (one over the depth), i0, i1 (the
    intensity), rgb (N, 3), w (pixel width of the line), fade (depth where the line is dim)."""
    n = len(segs)
    if n == 0:
        return None
    pos = np.array(cam.pos)
    mat = np.array([cam.r, cam.u, cam.f]).T                      # (3, 3): world to camera
    A = (segs[:, 0:3] - pos) @ mat
    B = (segs[:, 3:6] - pos) @ mat
    za, zb = A[:, 2], B[:, 2]
    fade = segs[:, 12]
    keep = ((za >= NEAR) | (zb >= NEAR)) & ((za < fade) | (zb < fade))
    if not keep.any():
        return None
    if not keep.all():
        segs, A, B, za, zb, fade = segs[keep], A[keep], B[keep], za[keep], zb[keep], fade[keep]
    i0, i1 = segs[:, 9].copy(), segs[:, 10].copy()
    A0, B0 = A.copy(), B.copy()
    ma = za < NEAR
    if ma.any():
        t = np.where(ma, (NEAR - za) / np.where(ma, zb - za, 1.0), 0.0)
        A = A0 + (B0 - A0) * t[:, None]
        i0 = i0 + (i1 - i0) * t
    mb = zb < NEAR
    if mb.any():
        t = np.where(mb, (NEAR - zb) / np.where(mb, za - zb, 1.0), 0.0)
        B = B0 + (A0 - B0) * t[:, None]
        i1 = segs[:, 10] + (segs[:, 9] - segs[:, 10]) * t
    za, zb = np.maximum(A[:, 2], NEAR), np.maximum(B[:, 2], NEAR)
    focal, cx, cy = cam.focal, cam.cx, cam.cy
    x0 = cx + A[:, 0] / za * focal
    y0 = cy - A[:, 1] / za * focal
    x1 = cx + B[:, 0] / zb * focal
    y1 = cy - B[:, 1] / zb * focal
    zmid = (za + zb) * 0.5
    far = zmid / fade                                       # how deep the line is, as a share of its fade depth
    return {"x0": x0, "y0": y0, "x1": x1, "y1": y1, "iz0": 1.0 / za, "iz1": 1.0 / zb, "i0": i0, "i1": i1,
            "rgb": segs[:, 6:9] / 255.0, "far": far, "fade": fade}


def discs_to_segs(discs, cam, floor):
    """Filled discs as horizontal chords in screen space. `discs` is the list of viz.Lines.discs."""
    if not discs:
        return None
    d = np.array(discs, dtype=np.float64)
    pos = np.array(cam.pos)
    mat = np.array([cam.r, cam.u, cam.f]).T
    P = (d[:, 0:3] - pos) @ mat
    z = P[:, 2]
    ok = (z >= NEAR) & (z < d[:, 8])
    if not ok.any():
        return None
    d, P, z = d[ok], P[ok], z[ok]
    cxp = cam.cx + P[:, 0] / z * cam.focal
    cyp = cam.cy - P[:, 1] / z * cam.focal
    rad = np.maximum(d[:, 3] * cam.focal / z, 0.9)
    counts = np.minimum(np.ceil(rad * 2.0 / 0.7).astype(np.int64) + 1, 60)
    idx = np.repeat(np.arange(len(d)), counts)
    start = np.cumsum(counts) - counts
    k = np.arange(counts.sum()) - np.repeat(start, counts)
    t = k / np.maximum(counts[idx] - 1, 1)
    dy = (t * 2 - 1) * rad[idx]
    hw = np.sqrt(np.maximum(rad[idx] ** 2 - dy * dy, 0.0))
    y = cyp[idx] + dy
    iz = 1.0 / z[idx]
    inten = d[idx, 7]
    return {"x0": cxp[idx] - hw, "y0": y, "x1": cxp[idx] + hw, "y1": y, "iz0": iz, "iz1": iz, "i0": inten, "i1": inten,
            "rgb": d[idx, 4:7] / 255.0, "far": np.zeros(len(idx)), "fade": d[idx, 8]}


def concat(parts):
    parts = [p for p in parts if p is not None]
    if not parts:
        return None
    if len(parts) == 1:
        return parts[0]
    return {k: np.concatenate([p[k] for p in parts]) for k in parts[0]}


def splat(buf, seg, ss, floor, gain, near_passes=None):
    """Add the light of the projected segments to the float buffer `buf` (3, H, W), one plane for each color.

    `ss` is the supersampling of the buffer. A line of intensity 1 has the value `gain` in the
    output pixels after the box filter. Lines in the near half of their fade depth are `near_passes`
    parallel passes wide (default: about one output pixel). Far lines are one pass: thinner."""
    _, H, W = buf.shape
    x0, y0, x1, y1 = seg["x0"], seg["y0"], seg["x1"], seg["y1"]
    n = len(x0)
    dx, dy = x1 - x0, y1 - y0
    # Cut to the screen (Liang and Barsky), keeping the parameter range of the original segment.
    t0 = np.zeros(n)
    t1 = np.ones(n)
    ok = np.ones(n, dtype=bool)
    xmax, ymax = W - 1.01, H - 1.01
    with np.errstate(divide="ignore", invalid="ignore"):
        for p, q in ((-dx, x0), (dx, xmax - x0), (-dy, y0), (dy, ymax - y0)):
            par = p == 0
            ok &= ~(par & (q < 0))
            r = q / p
            neg = (p < 0) & ~par
            pos_ = (p > 0) & ~par
            t0 = np.where(neg, np.maximum(t0, r), t0)
            t1 = np.where(pos_, np.minimum(t1, r), t1)
    ok &= t0 <= t1
    if not ok.all():
        sel = np.flatnonzero(ok)
        if len(sel) == 0:
            return
        seg = {k: v[sel] for k, v in seg.items()}
        x0, y0, dx, dy, t0, t1 = x0[sel], y0[sel], dx[sel], dy[sel], t0[sel], t1[sel]
        n = len(sel)
    length = np.hypot(dx, dy) * (t1 - t0)
    if near_passes is None:
        near_passes = max(1, int(round(ss / 1.4)))
    passes = np.where(seg["far"] < 0.45, near_passes, 1).astype(np.int64)
    if (passes > 1).any():
        rep = np.repeat(np.arange(n), passes)
        pstart = np.cumsum(passes) - passes
        k = np.arange(passes.sum()) - np.repeat(pstart, passes)
        off = (k - (passes[rep] - 1) / 2.0) * 0.9
        ln = np.maximum(np.hypot(dx, dy), 1e-6)[rep]
        x0 = x0[rep] - dy[rep] / ln * off
        y0 = y0[rep] + dx[rep] / ln * off
        dx, dy, t0, t1, length = dx[rep], dy[rep], t0[rep], t1[rep], length[rep]
        seg = {kk: v[rep] for kk, v in seg.items()}
        passes = passes[rep]
        n = len(rep)
    step = STEP
    counts = np.minimum(np.ceil(length / step).astype(np.int64) + 1, 4000)
    total = int(counts.sum())
    idx = np.repeat(np.arange(n), counts)
    start = np.cumsum(counts) - counts
    local = np.arange(total) - np.repeat(start, counts)
    t = t0[idx] + (t1[idx] - t0[idx]) * local / np.maximum(counts[idx] - 1, 1)
    x = np.clip(x0[idx] + dx[idx] * t, 0.0, W - 1.01)
    y = np.clip(y0[idx] + dy[idx] * t, 0.0, H - 1.01)
    iz = seg["iz0"][idx] + (seg["iz1"][idx] - seg["iz0"][idx]) * t
    inten = seg["i0"][idx] + (seg["i1"][idx] - seg["i0"][idx]) * t
    zs = 1.0 / iz
    dep = np.clip(1.0 - zs / seg["fade"][idx], floor, 1.0) * np.clip(zs * (1.0 / 11.0), 0.2, 1.0)
    # A sample carries the light of one step of the line. The thin lines of the far scene get dimmer.
    e = inten * dep * step * gain * ss / np.sqrt(passes[idx])
    xi, yi = x.astype(np.int64), y.astype(np.int64)
    fx, fy = x - xi, y - yi
    base = yi * W + xi
    ii = np.concatenate([base, base + 1, base + W, base + W + 1])
    w4 = np.concatenate([(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy])
    rgb = seg["rgb"][idx]
    w4e = w4 * np.tile(e, 4)
    for c in range(3):
        buf[c] += np.bincount(ii, weights=w4e * np.tile(rgb[:, c], 4), minlength=H * W).astype(np.float32).reshape(H, W)


# --------------------------------------------------------------------------- post process

def _blur5(a):
    """Separable binomial blur [1 4 6 4 1] / 16 of a (3, H, W) array. The border rows and columns stay as they are."""
    b = a.copy()
    if a.shape[1] > 4:
        b[:, 2:-2] = (a[:, :-4] + 4 * a[:, 1:-3] + 6 * a[:, 2:-2] + 4 * a[:, 3:-1] + a[:, 4:]) * (1.0 / 16.0)
    c = b.copy()
    if a.shape[2] > 4:
        c[:, :, 2:-2] = (b[:, :, :-4] + 4 * b[:, :, 1:-3] + 6 * b[:, :, 2:-2] + 4 * b[:, :, 3:-1] + b[:, :, 4:]) * (1.0 / 16.0)
    return c


def _blur3(a):
    b = a.copy()
    if a.shape[1] > 2:
        b[:, 1:-1] = (a[:, :-2] + 2 * a[:, 1:-1] + a[:, 2:]) * 0.25
    c = b.copy()
    if a.shape[2] > 2:
        c[:, :, 1:-1] = (b[:, :, :-2] + 2 * b[:, :, 1:-1] + b[:, :, 2:]) * 0.25
    return c


def _down2(a):
    _, H, W = a.shape
    h, w = H // 2, W // 2
    return (a[:, 0:2 * h:2, 0:2 * w:2] + a[:, 1:2 * h:2, 0:2 * w:2] + a[:, 0:2 * h:2, 1:2 * w:2]
            + a[:, 1:2 * h:2, 1:2 * w:2]) * 0.25


def _up2(a, H, W):
    b = np.repeat(np.repeat(a, 2, axis=1), 2, axis=2)
    if b.shape[1] != H or b.shape[2] != W:
        b = np.pad(b, ((0, 0), (0, max(0, H - b.shape[1])), (0, max(0, W - b.shape[2]))), mode="edge")[:, :H, :W]
    return b


def bloom(img, levels):
    """Light blurred at several radii, for a (3, H, W) image. `levels` is a list of (pyramid level, gain).
    Level n is the image cut down n times by two and blurred. The levels are added from the coarse one
    to the fine one, so the picture is scaled up once for each step."""
    small = min(img.shape[1], img.shape[2])
    gains = {lv: g for lv, g in levels if small >> lv >= 2}      # a level needs at least two pixels each way
    if not gains:
        return np.zeros_like(img)
    top = max(gains)
    pyr = [img]
    for _ in range(top):
        pyr.append(_down2(pyr[-1]))
    cur = None
    for lv in range(top, -1, -1):
        if cur is not None:
            cur = _up2(cur, pyr[lv].shape[1], pyr[lv].shape[2])
            if pyr[lv].shape[1] * pyr[lv].shape[2] < 60000:
                cur = _blur3(cur)
        if lv in gains:
            s = _blur5(pyr[lv]) * gains[lv]
            cur = s if cur is None else cur + s
    return cur


def downsample(buf, ss):
    if ss == 1:
        return buf
    if ss == 2:
        return _down2(buf)
    _, H, W = buf.shape
    h, w = H // ss, W // ss
    out = np.zeros((3, h, w), dtype=np.float32)
    for dy in range(ss):
        for dx in range(ss):
            out += buf[:, dy:h * ss:ss, dx:w * ss:ss]
    return out * (1.0 / (ss * ss))


class Post:
    """Persistence, bloom, scanlines, vignette and tone map."""

    def __init__(self, persist=0.6, glow=((0, 0.22), (1, 0.14)), exposure=1.7, scan=1.0, vignette=0.25,
                 gamma=0.85, floor=0.012, whiten=0.35, flicker=0.0):
        self.persist, self.glow, self.exposure = persist, glow, exposure
        self.whiten, self.flicker = whiten, flicker
        self.glow_on = True
        self._rng = np.random.default_rng(7)
        self._wlut = None
        self.scan, self.vignette, self.gamma, self.floor = scan, vignette, gamma, floor
        self.acc = None
        self._mask = None
        self._mask_key = None
        self._lut = None

    def _fixed(self, H, W, scanlines):
        key = (H, W, self.scan, self.vignette, scanlines)
        if self._mask_key != key:
            yy = np.linspace(-1, 1, H)[:, None]
            xx = np.linspace(-1, 1, W)[None, :]
            vig = 1.0 - self.vignette * np.clip((xx * xx + yy * yy) / 2.0, 0, 1)
            if scanlines:
                sc = np.ones((H, 1))
                sc[1::2] = self.scan
                vig = vig * sc
            self._mask = (vig * 255.0).astype(np.float32)
            self._mask_key = key
        return self._mask

    def _tone(self):
        if self._lut is None:
            m = (np.arange(2048) + 0.5) / 512.0                  # light values 0 to 4
            self._lut = np.power(1.0 - np.exp(-self.exposure * m), self.gamma).astype(np.float32)
        return self._lut

    def _white(self):
        """Share of white that a pixel gets by its light: the core of a bright line goes toward white."""
        if self._wlut is None:
            tone = self._tone()
            self._wlut = (self.whiten * np.clip((tone - 0.45) / 0.5, 0.0, 1.0) ** 1.5).astype(np.float32)
        return self._wlut

    def run(self, still, moving, scanlines=True, glow_shift=0):
        """Float images (3, H, W) of the still and the moving lines to uint8 (3, H, W). The moving lines
        leave a trail. glow_shift moves the bloom levels for a larger picture."""
        _, H, W = still.shape
        if self.acc is None or self.acc.shape != still.shape:
            self.acc = moving.astype(np.float32)
        else:
            self.acc *= self.persist
            np.maximum(self.acc, moving, out=self.acc)
        base = still + self.acc
        img = base + bloom(base, [(lv + glow_shift, g) for lv, g in self.glow] if self.glow_on else [])
        m = np.maximum(np.maximum(img[0], img[1]), img[2])
        # The tone map scales the three planes by the same factor, so the hue stays.
        tone = np.take(self._tone(), np.minimum((m * 512.0).astype(np.int32), 2047))
        idx = np.minimum((m * 512.0).astype(np.int32), 2047)
        mask = self._fixed(H, W, scanlines)
        if self.flicker:
            mask = mask * (1.0 + self.flicker * (self._rng.random() * 2.0 - 1.0))
        fac = tone / (m + 1e-6) * mask
        img *= fac
        if self.whiten:
            white = np.take(self._white(), idx)
            img *= 1.0 - white
            img += white * (tone * mask)
        img += 0.5
        out = img.astype(np.uint8)
        out[:, (m * fac) < int(self.floor * 255) + 1] = 0
        return out


# --------------------------------------------------------------------------- half block output

_SGR = {}


def _fg_bg(f, b):
    s = _SGR.get((f, b))
    if s is None:
        if len(_SGR) > 60000:
            _SGR.clear()
        s = _SGR[(f, b)] = "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm" % (f >> 16, f >> 8 & 255, f & 255, b >> 16, b >> 8 & 255, b & 255)
    return s


def _bg(b):
    s = _SGR.get(b)
    if s is None:
        s = _SGR[b] = "\x1b[48;2;%d;%d;%dm" % (b >> 16, b >> 8 & 255, b & 255)
    return s


def _fg(f):
    s = _SGR.get(-f - 1)
    if s is None:
        s = _SGR[-f - 1] = "\x1b[38;2;%d;%d;%dm" % (f >> 16, f >> 8 & 255, f & 255)
    return s


def pack(img):
    """(3, H, W) uint8 to (H, W) ints 0xRRGGBB, with the low two bits of each channel dropped."""
    q = (img & 0xFC).astype(np.int32)
    return (q[0] << 16) | (q[1] << 8) | q[2]


def halfblock(img, texts=None):
    """Rows of text for an image with two pixel rows for each cell row. texts is {(row, col): (char, rgb)}."""
    _, H, W = img.shape
    packed = pack(img)
    top, bot = packed[0::2].tolist(), packed[1::2].tolist()
    rows = []
    for ry in range(H // 2):
        out = []
        fg = bg = -1
        rowtext = None
        if texts:
            rowtext = {c: v for (r, c), v in texts.items() if r == ry} or None
        for cx, (t, b) in enumerate(zip(top[ry], bot[ry])):
            if rowtext is not None and cx in rowtext:
                ch, col = rowtext[cx]
                bb = ((b >> 1) & 0x7E7E7E) | 0                      # half of the pixel below, so text reads on a glow
                if col != fg:
                    out.append(_fg(col))
                    fg = col
                if bb != bg:
                    out.append(_bg(bb))
                    bg = bb
                out.append(ch)
                continue
            if t == b:
                if b != bg:
                    out.append(_bg(b))
                    bg = b
                out.append(" ")
            else:
                if t != fg and b != bg:
                    out.append(_fg_bg(t, b))
                elif t != fg:
                    out.append(_fg(t))
                elif b != bg:
                    out.append(_bg(b))
                fg, bg = t, b
                out.append("▀")
        out.append("\x1b[0m\x1b[K")
        rows.append("".join(out))
    return rows


# --------------------------------------------------------------------------- sixel output

_HUE_TABLES = {}


def _hue_table(n):
    """Class 0 to n - 1 of a color plane against the brightest plane, rounded. Index is brightest * 256 + plane."""
    tab = _HUE_TABLES.get(n)
    if tab is None:
        vv, pp = np.meshgrid(np.arange(256), np.arange(256), indexing="ij")
        tab = _HUE_TABLES[n] = (((pp * (n - 1) * 2 + vv) // np.maximum(vv * 2, 1)).clip(0, n - 1)).astype(np.uint8).ravel()
    return tab


def sixel_encode(img, levels=12, cursor=None):
    """A DCS sixel string for an image of three uint8 planes (3, H, W). Black is the background and is
    not drawn. Each pixel gets a brightness step (`levels` of them) and a hue class (each plane against
    the brightest one, in 5 steps), so a picture of one hue needs only `levels` colors. A picture with
    more than 255 colors gets coarser hue classes."""
    _, H, W = img.shape
    Hp = (H + 5) // 6 * 6
    if Hp != H:
        img = np.pad(img, ((0, 0), (0, Hp - H), (0, 0)))
    v = np.maximum(np.maximum(img[0], img[1]), img[2])
    base = v.astype(np.uint16) << 8
    for n, lv_count in ((5, levels), (3, levels), (2, 8)):
        lut = np.clip(np.rint((np.arange(256) / 255.0) ** 0.7 * lv_count), 0, lv_count).astype(np.uint8)
        lut[:6] = 0
        lvl = lut[v]
        tab = _hue_table(n)
        hue = (tab[base | img[0]] + n * tab[base | img[1]].astype(np.int32) + n * n * tab[base | img[2]].astype(np.int32))
        key = np.where(lvl > 0, hue * (lv_count + 1) + lvl, 0)
        # Register 0 is the background. The other registers go to the keys that the image uses.
        used = np.bincount(key.ravel(), minlength=n ** 3 * (lv_count + 1)) > 0
        used[0] = False
        if used.sum() < 255:
            break
    levels = lv_count
    reg_lut = (np.cumsum(used) * used).astype(np.uint8)
    index = reg_lut[key]                                        # (Hp, W) register numbers, 0 = background
    keys = np.flatnonzero(used)
    nreg = len(keys)
    B = Hp // 6
    out = [b"\x1bPq\"1;1;%d;%d" % (W, H)]
    for r, u in enumerate(keys.tolist(), 1):
        h, lv = divmod(u, levels + 1)
        top = (lv / levels) ** (1 / 0.7) * 100.0
        rgb = [top * (h % n) / (n - 1), top * (h // n % n) / (n - 1), top * (h // (n * n)) / (n - 1)]
        out.append(b"#%d;2;%d;%d;%d" % (r, round(rgb[0]), round(rgb[1]), round(rgb[2])))
    # Masks: one row of W sixel values for each (band, register). Bit n is the pixel row n of the band.
    idx6 = index.reshape(B, 6, W).astype(np.int32)
    place = (np.arange(B, dtype=np.int32)[:, None, None] * nreg + idx6 - 1) * W + np.arange(W, dtype=np.int32)[None, None, :]
    bits = np.broadcast_to((1 << np.arange(6, dtype=np.int32))[None, :, None], idx6.shape)
    drawn = idx6 > 0
    M = np.bincount(place[drawn], weights=bits[drawn], minlength=B * nreg * W).astype(np.uint8).reshape(B, nreg, W)
    present = M.any(axis=2)                                     # (B, nreg)
    bi, ri = np.nonzero(present)
    rows = M[bi, ri]                                            # (P, W)
    P = len(rows)
    if P == 0:
        return (b"".join(out) + b"\x1b\\").decode("ascii")
    chg = np.ones((P, W), dtype=bool)
    chg[:, 1:] = rows[:, 1:] != rows[:, :-1]
    pos = np.flatnonzero(chg.ravel())
    vals = rows.ravel()[pos]
    ends = np.append(pos[1:], P * W)
    row_of = pos // W
    row_end = (row_of + 1) * W
    ends = np.minimum(ends, row_end)
    lens = ends - pos
    drop = (vals == 0) & (ends == row_end)                      # trailing background of a row
    pos, vals, lens, row_of = pos[~drop], vals[~drop], lens[~drop], row_of[~drop]
    chars = (vals + 63).astype(np.uint8)
    long_ = lens >= 4
    nd = 1 + (lens >= 10) + (lens >= 100) + (lens >= 1000)
    size = np.where(long_, 2 + nd, lens)
    off = np.cumsum(size) - size
    buf = np.zeros(int(size.sum()), dtype=np.uint8)
    for k in range(3):                                          # short runs: the character 1 to 3 times
        sel = (~long_) & (lens > k)
        buf[off[sel] + k] = chars[sel]
    ls = np.flatnonzero(long_)
    if len(ls):
        buf[off[ls]] = 33                                       # "!"
        ln, nl = lens[ls], nd[ls]
        for j in range(4):
            sel = nl > j
            digit = (ln // np.power(10, np.maximum(nl - 1 - j, 0))) % 10
            buf[(off[ls] + 1 + j)[sel]] = (48 + digit)[sel]
        buf[off[ls] + 1 + nl] = chars[ls]
    # Cut the token bytes by row.
    first = np.searchsorted(row_of, np.arange(P), side="left")
    last = np.searchsorted(row_of, np.arange(P), side="right")
    ends_b = np.append(off, len(buf))
    pieces = []
    band_prev = -1
    bl, rl = bi.tolist(), (ri + 1).tolist()
    for k in range(P):
        if bl[k] != band_prev and band_prev != -1:
            pieces[-1] = pieces[-1][:-1] + b"-"                 # the last row of a band ends with a new line
        band_prev = bl[k]
        pieces.append(b"#%d" % rl[k] + buf[ends_b[first[k]]:ends_b[last[k]]].tobytes() + b"$")
    pieces[-1] = pieces[-1][:-1]
    pre = b"" if cursor is None else ("\x1b[%d;%dH" % cursor).encode("ascii")
    return pre.decode("ascii") + (b"".join(out) + b"".join(pieces) + b"\x1b\\").decode("ascii")


def decode_sixel(text):
    """Decode the output of sixel_encode to (H, W, 3) uint8. A small reader for tests, not fast."""
    s = text.encode("ascii") if isinstance(text, str) else text
    i = s.index(b"q") + 1
    # The raster attributes "Pan;Pad;W;H
    j = i
    assert s[j:j + 1] == b'"'
    j += 1
    k = j
    while s[k:k + 1] not in (b"#", b"?") and (s[k:k + 1].isdigit() or s[k:k + 1] == b";"):
        k += 1
    attrs = [int(x) for x in s[j:k].split(b";")]
    W, H = attrs[2], attrs[3]
    img = np.zeros(((H + 5) // 6 * 6, W, 3), dtype=np.uint8)
    pal = {}
    x = y = 0
    cur = 0
    n = len(s)
    while k < n:
        c = s[k:k + 1]
        if c == b"\x1b":
            break
        if c == b"#":
            k += 1
            e = k
            while e < n and (s[e:e + 1].isdigit() or s[e:e + 1] == b";"):
                e += 1
            nums = [int(v) for v in s[k:e].split(b";")]
            if len(nums) == 1:
                cur = nums[0]
            else:
                cur = nums[0]
                pal[cur] = tuple(int(round(v * 255 / 100)) for v in nums[2:5])
            k = e
            continue
        rep = 1
        if c == b"!":
            k += 1
            e = k
            while s[e:e + 1].isdigit():
                e += 1
            rep = int(s[k:e])
            k = e
            c = s[k:k + 1]
        if c == b"$":
            x = 0
        elif c == b"-":
            x = 0
            y += 6
        else:
            val = c[0] - 63
            for r in range(6):
                if val >> r & 1:
                    img[y + r, x:x + rep] = pal[cur]
            x += rep
        k += 1
    return img[:H]


# --------------------------------------------------------------------------- bitmap font for sixel labels

_FONT = {
    "A": "01110 10001 10001 11111 10001 10001 10001", "B": "11110 10001 10001 11110 10001 10001 11110",
    "C": "01110 10001 10000 10000 10000 10001 01110", "D": "11110 10001 10001 10001 10001 10001 11110",
    "E": "11111 10000 10000 11110 10000 10000 11111", "F": "11111 10000 10000 11110 10000 10000 10000",
    "G": "01110 10001 10000 10111 10001 10001 01111", "H": "10001 10001 10001 11111 10001 10001 10001",
    "I": "01110 00100 00100 00100 00100 00100 01110", "J": "00111 00010 00010 00010 00010 10010 01100",
    "K": "10001 10010 10100 11000 10100 10010 10001", "L": "10000 10000 10000 10000 10000 10000 11111",
    "M": "10001 11011 10101 10101 10001 10001 10001", "N": "10001 11001 10101 10011 10001 10001 10001",
    "O": "01110 10001 10001 10001 10001 10001 01110", "P": "11110 10001 10001 11110 10000 10000 10000",
    "Q": "01110 10001 10001 10001 10101 10010 01101", "R": "11110 10001 10001 11110 10100 10010 10001",
    "S": "01111 10000 10000 01110 00001 00001 11110", "T": "11111 00100 00100 00100 00100 00100 00100",
    "U": "10001 10001 10001 10001 10001 10001 01110", "V": "10001 10001 10001 10001 10001 01010 00100",
    "W": "10001 10001 10001 10101 10101 11011 10001", "X": "10001 10001 01010 00100 01010 10001 10001",
    "Y": "10001 10001 01010 00100 00100 00100 00100", "Z": "11111 00001 00010 00100 01000 10000 11111",
    "0": "01110 10001 10011 10101 11001 10001 01110", "1": "00100 01100 00100 00100 00100 00100 01110",
    "2": "01110 10001 00001 00110 01000 10000 11111", "3": "11110 00001 00001 01110 00001 00001 11110",
    "4": "00010 00110 01010 10010 11111 00010 00010", "5": "11111 10000 11110 00001 00001 10001 01110",
    "6": "00110 01000 10000 11110 10001 10001 01110", "7": "11111 00001 00010 00100 01000 01000 01000",
    "8": "01110 10001 10001 01110 10001 10001 01110", "9": "01110 10001 10001 01111 00001 00010 01100",
    "#": "01010 11111 01010 01010 11111 01010 00000", " ": "00000 00000 00000 00000 00000 00000 00000",
}
_GLYPH = {k: np.array([[c == "1" for c in row] for row in v.split()], dtype=bool) for k, v in _FONT.items()}


def draw_text(buf, x, y, text, rgb, scale=2):
    """Stamp a label into the float buffer with the 5x7 font. (x, y) is the top left corner in pixels."""
    _, H, W = buf.shape
    for ch in text.upper():
        g = _GLYPH.get(ch)
        if g is not None:
            gg = np.kron(g, np.ones((scale, scale), dtype=bool))
            h, w = gg.shape
            x0, y0 = max(x, 0), max(y, 0)
            x1, y1 = min(x + w, W), min(y + h, H)
            if x1 > x0 and y1 > y0:
                sub = gg[y0 - y:y1 - y, x0 - x:x1 - x]
                for c in range(3):
                    buf[c, y0:y1, x0:x1][sub] += rgb[c] * 0.9
        x += (5 + 1) * scale


def query_cell_size(timeout=0.3):
    """Ask the terminal for the pixel size of a cell (CSI 16 t). Returns (width, height), or None."""
    try:
        import msvcrt
    except ImportError:
        return None
    sys.stdout.write("\x1b[16t")
    sys.stdout.flush()
    end = time.time() + timeout
    got = ""
    while time.time() < end:
        if msvcrt.kbhit():
            ch = msvcrt.getwch()
            got += ch
            if ch == "t":
                break
        else:
            time.sleep(0.005)
    try:
        nums = got.split("[")[1].rstrip("t").split(";")
        if nums[0] == "6":
            return int(nums[2]), int(nums[1])
    except (IndexError, ValueError):
        pass
    return None


# --------------------------------------------------------------------------- the viewer

class VectorViewer(viz.Viewer):
    """The scene of viz.Viewer, drawn as a glowing vector picture."""

    def __init__(self, width=None, height=None, backend="halfblock", ss=3, cell=(10, 20), post=None, theme=None):
        super().__init__(width, height)
        self.backend = backend
        self.ss = ss
        self.cell = cell
        self.post = post or Post()
        self.trail_decay = self.post.persist
        self.theme = viz.THEMES[theme] if isinstance(theme, str) else theme or viz.THEMES["tube"]
        self.gain = 0.9
        self.sixel_data = None           # the sixel string of the last frame, when the backend is sixel
        self.sixel_origin = (1, 1)       # row and column (from 1) where the image starts
        self.last_ms = {}

    def _label_color(self, rgb):
        m = max(rgb) or 1
        k = 255.0 / m
        return (min(255, int(rgb[0] * k)), min(255, int(rgb[1] * k)), min(255, int(rgb[2] * k)))

    def picture_size(self, cols, rows):
        """Pixel size of the final picture for a view of cols by rows cells."""
        if self.backend == "sixel":
            return cols * self.cell[0], rows * self.cell[1]
        return cols, rows * 2

    def picture(self, state, boxes, me, camera, cols, rows):
        """The 8 bit image (3, H, W) of a view of cols by rows cells, and the label list."""
        t0 = time.perf_counter()
        pose, parts = self.scene(state, boxes, me, camera)
        Wo, Ho = self.picture_size(cols, rows)
        ss = 1 if self.backend == "sixel" else self.ss
        W, H = Wo * ss, Ho * ss
        cam = viz.Camera(pose.pos, pose.yaw, pose.pitch, W, H, self.fov)
        t1 = time.perf_counter()
        bufs = []
        for moving in (False, True):
            projected, discs = [], []
            for ln in parts:
                if ln.moving != moving:
                    continue
                arr = to_array(ln)
                if len(arr):
                    projected.append(project(arr, cam, W, H, self.fade_floor))
                discs.extend(ln.discs)
            projected.append(discs_to_segs(discs, cam, self.fade_floor))
            seg = concat(projected)
            buf = np.zeros((3, H, W), dtype=np.float32)
            if seg is not None:
                splat(buf, seg, ss, self.fade_floor, self.gain)
            bufs.append(buf)
        t2 = time.perf_counter()
        lin = downsample(bufs[0], ss)
        trail = downsample(bufs[1], ss)
        labels = self.picked_labels(pose, parts)
        if self.backend == "sixel":
            size = max(1, Wo // 480)
            for pos, text, rgb, _p in labels:
                p = self.project(pos, cam)
                if p:
                    col = self._label_color(rgb)
                    draw_text(lin, int(p[0]) - len(text) * 6 * size // 2, int(p[1]) - 10 * size, text,
                              tuple(c / 255.0 for c in col), size)
        self.post.persist = self.trail_decay if self.trails else 0.0
        self.post.glow_on = self.glow
        img = self.post.run(lin, trail, scanlines=self.backend != "sixel", glow_shift=max(0, int(round(math.log2(Wo / 140.0)))) if self.backend == "sixel" else 0)
        t3 = time.perf_counter()
        self.last_ms = {"scene": (t1 - t0) * 1000, "raster": (t2 - t1) * 1000, "post": (t3 - t2) * 1000}
        return img, labels, cam

    def render(self, state, boxes=(), me=None, camera=None):
        cols, rows = self.size()
        view_rows = rows - 1
        img, labels, cam = self.picture(state, boxes, me, camera, cols, view_rows)
        self.last_image = img
        t0 = time.perf_counter()
        if self.backend == "sixel":
            self.sixel_data = sixel_encode(img, cursor=self.sixel_origin)
            body = [" " * cols + viz.RESET_K for _ in range(view_rows)]
        else:
            texts = {}
            for pos, text, rgb, _p in labels:
                p = self.project(pos, cam)
                if p and 0 <= p[0] < cam.cx * 2 and 0 <= p[1] < cam.cy * 2:
                    px = int(p[0] * cols / (cam.cx * 2))
                    py = max(0, int(p[1] * view_rows / (cam.cy * 2)) - 1)
                    col = self._label_color(rgb)
                    packed = (col[0] << 16) | (col[1] << 8) | col[2]
                    for k, ch in enumerate(text):
                        x = px - len(text) // 2 + k
                        if 0 <= x < cols and 0 <= py < view_rows:
                            texts[(py, x)] = (ch, packed)
            body = halfblock(img, texts)
        self.last_ms["encode"] = (time.perf_counter() - t0) * 1000
        body.append(self._hud(state, cols))
        return "\n".join(body)


def _main(argv):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    width = height = None
    for i, a in enumerate(argv):
        if a == "--width":
            width = int(argv[i + 1])
        elif a == "--height":
            height = int(argv[i + 1])
    backend = "sixel" if "--sixel" in argv else "halfblock"
    v = VectorViewer(width or 140, height or 44, backend=backend)
    state, boxes, me = viz.demo_scene(1.0)
    if "--frame" in argv:
        frame = v.render(state, boxes, me)
        sys.stdout.write(frame + "\n")
        if v.sixel_data:
            sys.stdout.write(v.sixel_data)
        return 0
    if "--demo" in argv:
        if os.name == "nt":
            os.system("")
        sys.stdout.write("\x1b[?25l\x1b[2J")
        start = time.time()
        try:
            while True:
                t = time.time() - start
                state, boxes, me = viz.demo_scene(t)
                sys.stdout.write("\x1b[H" + v.render(state, boxes, me))
                if v.sixel_data:
                    sys.stdout.write(v.sixel_data)
                sys.stdout.flush()
                time.sleep(0.04)
        except KeyboardInterrupt:
            pass
        finally:
            sys.stdout.write("\x1b[0m\x1b[?25h\n")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(_main(sys.argv[1:]))
