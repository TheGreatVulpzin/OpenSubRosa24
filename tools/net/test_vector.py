#!/usr/bin/env python3
"""Tests for vector.py. Run: python tools/net/test_vector.py (numpy is a user package: no -I)."""
import math
import os
import re
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import numpy as np
    import vector
except ImportError:                      # python -I does not see numpy
    np = None
import srpacket as sr  # noqa: E402
import viz  # noqa: E402

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


def plain(frame):
    return [ANSI.sub("", line) for line in frame.split("\n")]


def one_line(x0, y0, x1, y1, intensity=1.0, passes=1, W=40, H=30):
    """The float buffer after one line of the given screen coordinates."""
    seg = {"x0": np.array([x0]), "y0": np.array([y0]), "x1": np.array([x1]), "y1": np.array([y1]),
           "iz0": np.array([0.1]), "iz1": np.array([0.1]), "i0": np.array([intensity]), "i1": np.array([intensity]),
           "rgb": np.array([[1.0, 0.5, 0.25]]), "far": np.array([0.0]), "fade": np.array([1000.0])}
    buf = np.zeros((3, H, W), dtype=np.float32)
    vector.splat(buf, seg, 1, 0.1, 1.0, near_passes=passes)
    return buf


@unittest.skipIf(np is None, "numpy is missing")
class LineTests(unittest.TestCase):
    def test_horizontal_line_on_a_pixel_row(self):
        buf = one_line(5.0, 10.0, 25.0, 10.0)
        row = buf[0, 10]
        self.assertGreater(row[6:24].min(), 0.8)
        self.assertLess(row[6:24].max(), 1.35)
        self.assertEqual(float(buf[0, 8].sum()), 0.0)            # nothing two rows away
        self.assertEqual(float(buf[0, 12].sum()), 0.0)

    def test_light_between_two_rows_is_shared(self):
        buf = one_line(5.0, 10.5, 25.0, 10.5)
        a, b = buf[0, 10, 8:22], buf[0, 11, 8:22]
        self.assertTrue(np.allclose(a, b, atol=0.05))
        self.assertGreater(float((a + b).min()), 0.8)

    def test_diagonal_line_is_anti_aliased(self):
        buf = one_line(4.0, 4.0, 24.0, 14.0)
        # The line y = 4 + (x - 4) / 2. Pixels on it are lit, pixels far from it are dark.
        lit = 0
        for x in range(6, 22):
            y = 4 + (x - 4) / 2.0
            near = buf[0, int(y):int(y) + 2, x].sum()
            self.assertGreater(float(near), 0.5, "column %d" % x)
            lit += 1
            far = buf[0, :max(0, int(y) - 2), x].sum() + buf[0, int(y) + 4:, x].sum()
            self.assertLess(float(far), 0.01)
        self.assertEqual(lit, 16)
        # Both pixels of a column are partly lit: that is the anti-aliasing.
        partial = ((buf[0] > 0.1) & (buf[0] < 0.9)).sum()
        self.assertGreater(int(partial), 12)

    def test_total_light_follows_the_length(self):
        short = one_line(5.0, 5.0, 15.0, 5.0)
        long = one_line(5.0, 5.0, 35.0, 5.0)
        ratio = float(long[0].sum() / short[0].sum())
        self.assertAlmostEqual(ratio, 3.0, delta=0.25)

    def test_color_follows_the_rgb(self):
        buf = one_line(5.0, 10.0, 25.0, 10.0)
        self.assertAlmostEqual(float(buf[1].sum() / buf[0].sum()), 0.5, places=2)
        self.assertAlmostEqual(float(buf[2].sum() / buf[0].sum()), 0.25, places=2)

    def test_intensity_scales_the_light(self):
        a = one_line(5.0, 10.0, 25.0, 10.0, 1.0)
        b = one_line(5.0, 10.0, 25.0, 10.0, 0.5)
        self.assertAlmostEqual(float(b.sum() / a.sum()), 0.5, delta=0.03)

    def test_line_outside_draws_nothing(self):
        for args in ((100, 100, 200, 150), (-50, 2, -10, 2), (5, -30, 20, -5)):
            self.assertEqual(float(one_line(*args).sum()), 0.0)

    def test_huge_line_is_cut(self):
        buf = one_line(-1e6, 12.0, 1e6, 12.0)
        self.assertGreater(float(buf[0, 12, 5:35].min()), 0.8)

    def test_thick_line_is_wider(self):
        thin = one_line(5.0, 15.0, 30.0, 15.0, passes=1)
        thick = one_line(5.0, 15.0, 30.0, 15.0, passes=3)
        self.assertGreater(int((thick[0, :, 15] > 0.1).sum()), int((thin[0, :, 15] > 0.1).sum()))

    def test_discs_fill(self):
        cam = viz.Camera((0.0, 0.0, 0.0), 0.0, 0.0, 80, 60, math.radians(60))
        seg = vector.discs_to_segs([(0.0, 0.0, 10.0, 1.0, 255, 255, 255, 1.0, 1000.0)], cam, 0.1)
        buf = np.zeros((3, 60, 80), dtype=np.float32)
        vector.splat(buf, seg, 1, 0.1, 1.0)
        radius = 1.0 * cam.focal / 10.0
        self.assertGreater(radius, 3.0)
        cx, cy = 40, 30
        for dx, dy in ((0, 0), (int(radius * 0.6), 0), (0, int(radius * 0.6)), (-int(radius * 0.5), int(radius * 0.5))):
            self.assertGreater(float(buf[0, cy + dy, cx + dx]), 0.5, (dx, dy))
        self.assertEqual(float(buf[0, cy, cx + int(radius) + 4]), 0.0)

    def test_behind_the_camera_is_cut_at_the_near_plane(self):
        cam = viz.Camera((0.0, 0.0, 0.0), 0.0, 0.0, 80, 60, math.radians(60))
        segs = np.array([[0.0, 0.0, -5.0, 0.0, 0.0, 20.0, 255, 255, 255, 1.0, 1.0, 0.05, 100.0],
                         [0.0, 0.0, -5.0, 0.0, 0.0, -1.0, 255, 255, 255, 1.0, 1.0, 0.05, 100.0]])
        out = vector.project(segs, cam, 80, 60, 0.1)
        self.assertEqual(len(out["x0"]), 1)                       # the second line is gone
        self.assertTrue(np.isfinite(out["x0"]).all() and np.isfinite(out["y1"]).all())
        self.assertGreaterEqual(float(out["iz0"][0]), 1.0 / 20.0)


@unittest.skipIf(np is None, "numpy is missing")
class PostTests(unittest.TestCase):
    def test_tone_map_keeps_hue_and_range(self):
        post = vector.Post(persist=0.0, vignette=0.0, scan=1.0, whiten=0.0)
        lin = np.zeros((3, 12, 16), dtype=np.float32)
        lin[0, 6, 8], lin[1, 6, 8] = 3.0, 1.5                 # a very bright amber pixel
        img = post.run(lin, np.zeros_like(lin))
        self.assertEqual(img.shape, (3, 12, 16))
        self.assertEqual(img.dtype, np.uint8)
        r, g, b = (int(img[c, 6, 8]) for c in range(3))
        self.assertGreater(r, 200)
        self.assertEqual(b, 0)
        self.assertAlmostEqual(g / max(r, 1), 0.5, delta=0.08)

    def test_black_stays_black(self):
        post = vector.Post()
        z = np.zeros((3, 10, 10), dtype=np.float32)
        self.assertEqual(int(post.run(z, z).max()), 0)

    def test_moving_lines_leave_a_trail(self):
        post = vector.Post(persist=0.5, glow=((1, 0.0),), vignette=0.0, scan=1.0)
        z = np.zeros((3, 10, 10), dtype=np.float32)
        lit = z.copy()
        lit[:, 5, 5] = 1.0
        post.run(z, lit)
        after = post.run(z, z)                                # the line moved away
        self.assertGreater(int(after[:, 5, 5].max()), 40)

    def test_still_lines_have_no_trail(self):
        post = vector.Post(persist=0.9, glow=((1, 0.0),), vignette=0.0, scan=1.0)
        z = np.zeros((3, 10, 10), dtype=np.float32)
        lit = z.copy()
        lit[:, 5, 5] = 1.0
        post.run(lit, z)
        self.assertEqual(int(post.run(z, z)[:, 5, 5].max()), 0)

    def test_bloom_adds_light_around_a_point(self):
        post = vector.Post(persist=0.0, vignette=0.0, scan=1.0, glow=((1, 0.5),))
        lin = np.zeros((3, 20, 20), dtype=np.float32)
        lin[:, 10, 10] = 1.0
        img = post.run(lin, np.zeros_like(lin))
        self.assertGreater(int(img[0, 10, 11]), 5)
        self.assertEqual(int(img[0, 10, 18]), 0)


def gradient_image(H=36, W=50):
    img = np.zeros((3, H, W), dtype=np.uint8)
    for x in range(W):
        img[1, :, x] = int(255 * x / (W - 1))
        img[0, :, x] = int(40 * x / (W - 1))
        img[2, :, x] = int(70 * x / (W - 1))
    img[:, 5:10, 5:10] = 0
    img[0, 20:30, 3:12] = 255                                  # a red patch
    img[1, 20:30, 3:12] = 40
    img[2, 20:30, 3:12] = 0
    return img


@unittest.skipIf(np is None, "numpy is missing")
class SixelTests(unittest.TestCase):
    def test_round_trip_within_the_palette_error(self):
        img = gradient_image()
        text = vector.sixel_encode(img)
        self.assertTrue(text.startswith("\x1bPq"))
        self.assertTrue(text.endswith("\x1b\\"))
        back = vector.decode_sixel(text).transpose(2, 0, 1)
        self.assertEqual(back.shape, img.shape)
        err = np.abs(back.astype(int) - img.astype(int)).max(axis=0)
        # The palette has 12 brightness steps and three hue classes for each plane.
        self.assertLess(float(err.mean()), 14.0)
        self.assertLess(int(err.max()), 90)
        self.assertEqual(int(back[:, 6, 6].max()), 0)           # the black hole stays black
        self.assertGreater(int(back[0, 25, 6]), int(back[1, 25, 6]))   # the red patch stays red

    def test_height_not_a_multiple_of_six(self):
        img = gradient_image(37, 21)
        back = vector.decode_sixel(vector.sixel_encode(img))
        self.assertEqual(back.shape[:2], (37, 21))

    def test_runs_are_packed(self):
        img = np.zeros((3, 12, 400), dtype=np.uint8)
        img[1, :, :] = 200
        text = vector.sixel_encode(img)
        self.assertLess(len(text), 400)                         # "!400~" for each band, not 400 characters
        self.assertTrue(np.array_equal(vector.decode_sixel(text)[:, :, 1] > 100, np.ones((12, 400), dtype=bool)))

    def test_cursor_position_comes_first(self):
        text = vector.sixel_encode(gradient_image(), cursor=(6, 2))
        self.assertTrue(text.startswith("\x1b[6;2H\x1bPq"))

    def test_blank_image(self):
        text = vector.sixel_encode(np.zeros((3, 12, 12), dtype=np.uint8))
        self.assertTrue(text.startswith("\x1bPq"))
        self.assertEqual(int(vector.decode_sixel(text).max()), 0)


@unittest.skipIf(np is None, "numpy is missing")
class HalfBlockTests(unittest.TestCase):
    def test_cell_count(self):
        img = gradient_image(40, 30)
        rows = vector.halfblock(img)
        self.assertEqual(len(rows), 20)
        for row in rows:
            self.assertEqual(len(ANSI.sub("", row)), 30)

    def test_upper_pixel_is_the_foreground(self):
        img = np.zeros((3, 2, 1), dtype=np.uint8)
        img[:, 0, 0] = (200, 100, 40)                          # upper pixel
        img[:, 1, 0] = (20, 60, 240)                           # lower pixel
        row = vector.halfblock(img)[0]
        self.assertIn("▀", row)
        self.assertIn("38;2;200;100;40", row)
        self.assertIn("48;2;20;60;240", row)

    def test_equal_pixels_need_no_half_block(self):
        img = np.zeros((3, 2, 4), dtype=np.uint8)
        img[:, :, 1] = 100
        row = vector.halfblock(img)[0]
        self.assertNotIn("▀", row)

    def test_text_overlay(self):
        img = np.zeros((3, 4, 10), dtype=np.uint8)
        rows = vector.halfblock(img, {(1, 3): ("A", 0x66FFFF)})
        self.assertEqual(ANSI.sub("", rows[1])[3], "A")
        self.assertIn("38;2;102;255;255", rows[1])


def scene(n=3):
    humans = [sr.HumanBlock(i, pos=(i * 3.0, 25.0, 20.0), orient_b=(0.0, 0.0, -1.0)) for i in range(n)]
    objects = [sr.ObjectBlock(i, pos=(i * 6.0, 25.0, 30.0)) for i in range(n)]
    items = [sr.ItemBlock(i, pos=(i * 1.5, 24.3, 22.0)) for i in range(n)]
    return sr.State(tick=7, humans=humans, objects=objects, items=items)


@unittest.skipIf(np is None, "numpy is missing")
class ViewerTests(unittest.TestCase):
    def make(self, w=100, h=30, **kw):
        v = vector.VectorViewer(w, h, **kw)
        v.ground = 24.0
        return v

    def test_halfblock_frame_size(self):
        v = self.make(backend="halfblock")
        frame = v.render(scene(4), [((-5, 24, 10), (5, 40, 20), "base", (255, 100, 100))], 0)
        lines = plain(frame)
        self.assertEqual(len(lines), 30)
        for line in lines:
            self.assertLessEqual(len(line), 100)
        self.assertIn("TICK 7", lines[-1])
        self.assertEqual(v.last_image.shape, (3, 58, 100))

    def test_something_is_drawn(self):
        v = self.make()
        v.render(scene(4), [], 0)
        v.render(scene(4), [], 0)
        self.assertGreater(int((v.last_image.max(axis=0) > 30).sum()), 100)

    def test_label_text_is_in_the_cells(self):
        v = self.make()
        frame = v.render(scene(2), [((-5, 24, 8), (5, 30, 14), "bank", (255, 230, 90))], 0)
        self.assertTrue(any("bank" in line or "YOU" in line for line in plain(frame)[:-1]))

    def test_sixel_backend(self):
        v = self.make(60, 20, backend="sixel", cell=(8, 16))
        frame = v.render(scene(3), [], 0)
        self.assertEqual(len(plain(frame)), 20)
        self.assertTrue(v.sixel_data.startswith("\x1b[1;1H\x1bPq"))
        self.assertEqual(v.last_image.shape, (3, 19 * 16, 60 * 8))
        img = vector.decode_sixel(v.sixel_data[len("\x1b[1;1H"):])
        self.assertEqual(img.shape[:2], (19 * 16, 60 * 8))
        self.assertGreater(int(img.max()), 100)

    def test_theme_changes_the_hue(self):
        v = self.make(theme=lambda rgb: (0, 200, 0), post=vector.Post(whiten=0.0))
        v.render(scene(3), [], 0)
        v.render(scene(3), [], 0)
        img = v.last_image
        self.assertEqual(int(img[0].max()), 0)
        self.assertEqual(int(img[2].max()), 0)
        self.assertGreater(int(img[1].max()), 100)

    def test_trails_and_glow_toggles(self):
        v = self.make()
        state = scene(3)
        v.render(state, [], 0)
        v.glow = False
        v.trails = False
        v.render(state, [], 0)
        self.assertEqual(v.post.persist, 0.0)
        self.assertFalse(v.post.glow_on)
        v.key("glow")
        v.key("trails")
        v.render(state, [], 0)
        self.assertGreater(v.post.persist, 0.0)
        self.assertTrue(v.post.glow_on)

    def test_hollow_and_solid_frames_differ(self):
        boxes = [((-6.0, 24.0, 40.0), (6.0, 44.0, 52.0), "", (255, 255, 255))]
        images = []
        for hollow in (True, False):
            v = self.make()
            v.hollow = hollow
            v.glow = False
            v.trails = False
            v.render(scene(1), boxes, 0)
            v.render(scene(1), boxes, 0)
            images.append(v.last_image.copy())
        self.assertFalse(np.array_equal(images[0], images[1]))
        # Outline only: far fewer lit pixels than a filled picture would have.
        self.assertLess(float((images[0].max(axis=0) > 20).mean()), 0.35)

    def test_tiny_terminals_and_no_state(self):
        for w, h in ((8, 2), (9, 3), (30, 5)):
            self.assertEqual(len(plain(vector.VectorViewer(w, h).render(scene(2), [], 0))), h)
        self.assertEqual(len(plain(vector.VectorViewer(40, 12).render(None))), 12)

    def test_frame_time(self):
        v = vector.VectorViewer(140, 44)
        state = scene(10)
        state.objects = [sr.ObjectBlock(i, pos=(i * 6.0, 25.0, 30.0)) for i in range(10)]
        boxes = [((i * 30.0, 24.0, 40.0 + i * 5), (i * 30.0 + 20, 24.0 + 4 * (1 + i), 60.0 + i * 5), "", (120, 140, 190))
                 for i in range(20)]
        v.render(state, boxes, 0)
        best = 1.0
        for _ in range(3):
            start = time.perf_counter()
            for _ in range(5):
                v.render(state, boxes, 0)
            best = min(best, (time.perf_counter() - start) / 5)
        self.assertLess(best, 0.2, "frame took %.1f ms" % (best * 1000))


@unittest.skipIf(np is None, "numpy is missing")
class BrailleTests(unittest.TestCase):
    def test_a_line_is_thin_and_continuous(self):
        img = np.zeros((3, 16, 40), dtype=np.uint8)
        img[:, 8, 2:38] = 255                                  # one pixel row, bright
        rows = plain("\n".join(vector.braille(img)))
        marks = [ch for ch in rows[2] if ch != " "]
        self.assertEqual(len(marks), 18)                       # every cell of the line has dots
        self.assertTrue(all(0x2800 < ord(ch) <= 0x28FF for ch in marks))
        self.assertEqual(rows[0].strip(), "")                  # nothing off the line
        self.assertEqual(rows[3].strip(), "")

    def test_dark_pixels_draw_nothing(self):
        img = np.full((3, 16, 40), 20, dtype=np.uint8)
        self.assertTrue(all(line.strip() == "" for line in plain("\n".join(vector.braille(img)))))

    def test_dots_are_in_the_right_places(self):
        img = np.zeros((3, 8, 4), dtype=np.uint8)
        img[:, 0, 0] = 255                                     # top left dot of the first cell: bit 1
        img[:, 3, 3] = 255                                     # bottom right dot of the second cell: bit 128
        rows = plain("\n".join(vector.braille(img)))
        self.assertEqual(rows[0], chr(0x2801) + chr(0x2880))

    def test_cell_color_follows_the_pixels(self):
        img = np.zeros((3, 4, 2), dtype=np.uint8)
        img[0, :, :] = 255                                     # red
        text = vector.braille(img)[0]
        self.assertIn("\x1b[38;2;252;0;0m", text)

    def test_text_overlay(self):
        img = np.zeros((3, 8, 6), dtype=np.uint8)
        rows = vector.braille(img, {(1, 2): ("X", 0xFFFFFF)})
        self.assertIn("X", plain(rows[1])[0])

    def test_frame_size_and_resolution(self):
        v = vector.VectorViewer(100, 30, backend="braille")
        v.ground = 24.0
        frame = v.render(scene(4), [], 0)
        self.assertEqual(len(plain(frame)), 30)
        self.assertEqual(v.last_image.shape, (3, 29 * 4, 100 * 2))
        self.assertTrue(any(0x2800 < ord(ch) <= 0x28FF for ch in frame))


class DetectionTests(unittest.TestCase):
    def test_sixel_is_in_the_attribute_list(self):
        self.assertTrue(vector.sixel_in_reply("\x1b[?61;4;6;7;14;21;22c"))
        self.assertTrue(vector.sixel_in_reply("\x1b[?4c"))
        self.assertFalse(vector.sixel_in_reply("\x1b[?61;6;7c"))
        self.assertFalse(vector.sixel_in_reply("\x1b[?61;14;24c"))
        self.assertFalse(vector.sixel_in_reply(""))

    def test_no_terminal_no_sixel(self):
        self.assertEqual(vector.best_backend((8, 16)), ("braille", (8, 16)))

    @unittest.skipIf(np is None, "numpy is missing")
    def test_render_key_cycles_the_backend(self):
        v = vector.VectorViewer(100, 30)
        self.assertEqual(v.backend, "braille")                  # the default without a sixel terminal
        self.assertTrue(v.key("cycle_render"))
        self.assertEqual(v.backend, "halfblock")
        self.assertTrue(v.needs_clear)
        v.key("cycle_render")
        self.assertEqual(v.backend, "braille")


if __name__ == "__main__":
    unittest.main()
