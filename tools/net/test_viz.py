#!/usr/bin/env python3
"""Tests for viz.py. Run: python -I tools/net/test_viz.py"""
import os
import re
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import srpacket as sr  # noqa: E402
import viz  # noqa: E402

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


def plain(frame):
    return [ANSI.sub("", line) for line in frame.split("\n")]


class ProjectionTests(unittest.TestCase):
    def test_point_ahead_is_in_the_center(self):
        v = viz.Viewer(80, 25)
        cam = v.make_camera((0, 0, 0), 0.0, 0.0)
        sx, sy, z = v.project((0, 0, 10), cam)
        self.assertAlmostEqual(sx, 80.0)       # 160 dots wide
        self.assertAlmostEqual(sy, 48.0)       # 24 canvas rows, 96 dots high
        self.assertAlmostEqual(z, 10.0)

    def test_offset_scales_with_depth(self):
        v = viz.Viewer(80, 25)
        cam = v.make_camera((0, 0, 0), 0.0, 0.0)
        near = v.project((1, 0, 5), cam)
        far = v.project((1, 0, 10), cam)
        self.assertAlmostEqual((near[0] - 80.0) / (far[0] - 80.0), 2.0)
        up = v.project((0, 1, 10), cam)
        self.assertLess(up[1], 48.0)           # up is toward row 0

    def test_yaw_turns_the_view(self):
        v = viz.Viewer(80, 25)
        cam = v.make_camera((0, 0, 0), 1.5707963, 0.0)       # looks along +x
        sx, sy, z = v.project((10, 0, 0), cam)
        self.assertAlmostEqual(sx, 80.0, places=3)
        self.assertAlmostEqual(z, 10.0, places=3)

    def test_behind_the_camera_has_no_projection(self):
        v = viz.Viewer(80, 25)
        cam = v.make_camera((0, 0, 0), 0.0, 0.0)
        self.assertIsNone(v.project((0, 0, -5), cam))


class CanvasTests(unittest.TestCase):
    def test_horizontal_line_sets_dots(self):
        cv = viz.Canvas(4, 1)
        cv.line((0, 0, 1), (7, 0, 1), 9)
        self.assertEqual(cv.mask, [0x09, 0x09, 0x09, 0x09])

    def test_line_outside_draws_nothing(self):
        cv = viz.Canvas(4, 1)
        cv.line((100, 100, 1), (200, 150, 1), 9)
        cv.line((-50, 2, 1), (-10, 2, 1), 9)
        self.assertEqual(cv.mask, [0, 0, 0, 0])

    def test_huge_line_is_cut(self):
        cv = viz.Canvas(4, 2)
        cv.line((-1e6, 3, 1), (1e6, 3, 1), 9)
        self.assertTrue(all(cv.mask[i] for i in range(4)))

    def test_nearer_line_wins_the_color(self):
        cv = viz.Canvas(1, 1)
        cv.line((0, 0, 5), (1, 0, 5), 1)
        cv.line((0, 1, 2), (1, 1, 2), 2)
        cv.line((0, 2, 9), (1, 2, 9), 3)
        self.assertEqual(cv.color[0], 2)


def scene(n=1, behind=False):
    z = -20.0 if behind else 20.0
    humans = [sr.HumanBlock(i, pos=(i * 2.0, 1.0, z)) for i in range(n)]
    objects = [sr.ObjectBlock(i, pos=(i * 3.0, 0.6, z + 5)) for i in range(n)]
    items = [sr.ItemBlock(i, pos=(i * 1.5, 0.3, z + 2)) for i in range(n)]
    city = [sr.CityObjectBlock(i, pos=(i * 2.5, 0.5, z + 3)) for i in range(n)]
    return sr.State(tick=5, humans=humans, objects=objects, items=items, cityobjects=city)


class FrameTests(unittest.TestCase):
    def test_line_count_and_width(self):
        v = viz.Viewer(100, 30)
        frame = v.render(scene(5), boxes=[((-5, 0, 10), (5, 4, 20), "base 0", (255, 100, 100))], me=0)
        lines = plain(frame)
        self.assertEqual(len(lines), 30)
        for line in lines:
            self.assertLessEqual(len(line), 100)
        self.assertIn("tick 5", lines[-1])

    def test_scene_is_drawn(self):
        v = viz.Viewer(100, 30)
        lines = plain(v.render(scene(3), me=0))
        drawn = sum(1 for line in lines[:-1] for ch in line if 0x2800 < ord(ch) <= 0x28FF)
        self.assertGreater(drawn, 30)

    def test_label_is_drawn(self):
        v = viz.Viewer(100, 30)
        v.grid = False
        lines = plain(v.render(sr.State(), boxes=[((-5, 0, 10), (5, 4, 20), "base 0", (255, 100, 100))]))
        self.assertTrue(any("base 0" in line for line in lines[:-1]))

    def test_fully_clipped_scene_is_empty(self):
        v = viz.Viewer(100, 30)
        v.grid = False
        state = scene(5, behind=True)       # the camera looks at +z, everything is behind it
        lines = plain(v.render(state, camera=((0.0, 2.0, 0.0), 0.0, 0.0)))
        self.assertEqual(len(lines), 30)
        for line in lines[:-1]:
            self.assertEqual(line.strip(), "")

    def test_small_terminal_does_not_fail(self):
        v = viz.Viewer(8, 2)
        self.assertEqual(len(plain(v.render(scene(2), me=0))), 2)

    def test_speed(self):
        v = viz.Viewer(160, 50)
        state = scene(25)
        v.render(state, me=0)
        start = time.perf_counter()
        for _ in range(5):
            v.render(state, me=0)
        each = (time.perf_counter() - start) / 5
        self.assertLess(each, 0.1, "frame took %.1f ms" % (each * 1000))

    def test_demo_scene_renders(self):
        v = viz.Viewer(120, 40)
        state, boxes, me = viz.demo_scene(2.0)
        self.assertEqual(len(plain(v.render(state, boxes, me))), 40)


if __name__ == "__main__":
    unittest.main()
