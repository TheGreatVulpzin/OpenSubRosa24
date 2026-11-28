#!/usr/bin/env python3
"""Tests for viz.py. Run: python -I tools/net/test_viz.py"""
import math
import os
import re
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import srpacket as sr  # noqa: E402
import viz  # noqa: E402

UP, BACK = (0.0, 1.0, 0.0), (0.0, 0.0, -1.0)         # a human that stands and faces +z
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
    humans = [sr.HumanBlock(i, pos=(i * 2.0, 25.0, z), orient_a=UP, orient_b=BACK) for i in range(n)]
    objects = [sr.ObjectBlock(i, pos=(i * 3.0, 25.0, z + 5)) for i in range(n)]
    items = [sr.ItemBlock(i, pos=(i * 1.5, 24.3, z + 2)) for i in range(n)]
    city = [sr.CityObjectBlock(i, pos=(i * 2.5, 24.5, z + 3)) for i in range(n)]
    return sr.State(tick=5, humans=humans, objects=objects, items=items, cityobjects=city)


def extent(lines, axis):
    ends = [(s[axis], s[axis + 3]) for s in lines.segs]
    pts = [p for pair in ends for p in pair]
    return min(pts), max(pts)


class GeometryTests(unittest.TestCase):
    def test_body_axes_of_the_packet(self):
        h = sr.HumanBlock(0, orient_a=(0.0, 1.0, 0.0), orient_b=(1.0, 0.0, 0.0))
        right, up, fwd = viz.frame_of(h)
        self.assertEqual(up, (0.0, 1.0, 0.0))
        self.assertEqual(fwd, (-1.0, 0.0, 0.0))              # a human faces along minus b
        self.assertEqual(right, (0.0, 0.0, -1.0))            # the cross product of a and b

    def test_stick_figure_is_about_two_units_high(self):
        ln = viz.Lines()
        viz.add_figure(ln, (0.0, 25.0, 0.0), viz.frame_of(sr.HumanBlock(0, orient_a=UP, orient_b=BACK)),
                       (1, 1, 1), 0.0, 0.0, 1.0, False, 1.0)
        lo, hi = extent(ln, 1)
        self.assertAlmostEqual(lo, 25.0 - 0.9, delta=0.12)
        self.assertGreater(hi - lo, 1.7)
        self.assertLess(hi - lo, 2.1)
        self.assertEqual(len(ln.discs), 0)                     # outline only: the head is a circle of lines

    def test_walk_cycle_moves_the_legs(self):
        feet = []
        for phase in (0.0, math.pi / 2):
            ln = viz.Lines()
            viz.add_figure(ln, (0.0, 25.0, 0.0), viz.frame_of(sr.HumanBlock(0, orient_a=UP, orient_b=BACK)),
                           (1, 1, 1), phase, 0.8, 1.0, False, 1.0)
            feet.append(sorted(s[5] for s in ln.segs))
        self.assertNotEqual(feet[0], feet[1])

    def test_car_has_four_wheels_and_a_heading_arrow(self):
        ln = viz.Lines()
        viz.add_car(ln, (0.0, 25.0, 0.0), viz.frame_of(sr.ObjectBlock(0, orient_a=UP, orient_b=BACK)), (255, 160, 60), 1.0)
        self.assertEqual(len(ln.discs), 0)                     # outline only
        self.assertEqual(len(ln.segs), 24 + 4 * 9 + 3)         # two boxes, four wheel circles, the arrow
        # The arrow points along the heading: the car faces +z here.
        tip = max(ln.segs, key=lambda s: max(s[2], s[5]))
        self.assertGreater(max(tip[2], tip[5]), 3.0)

    def test_building_has_a_ring_for_each_floor(self):
        ln = viz.Lines()
        viz.add_building(ln, (0, 24, 0), (20, 24 + 20, 20), (255, 255, 255), 4.0)
        rings = {round(s[1], 3) for s in ln.segs if s[1] == s[4]}
        self.assertEqual(rings, {24.0, 28.0, 32.0, 36.0, 40.0, 44.0})
        flat = viz.Lines()
        viz.add_building(flat, (0, 24, 0), (20, 28, 20), (255, 255, 255), 4.0)
        self.assertEqual(len(flat.segs), 12)

    def test_diamond_has_twelve_edges(self):
        ln = viz.Lines()
        viz.add_diamond(ln, (0.0, 0.0, 0.0), 0.2, (255, 255, 0), 1.0)
        self.assertEqual(len(ln.segs), 12)


class SceneTests(unittest.TestCase):
    def view(self, state, **kw):
        v = viz.Viewer(100, 30)
        v.now = lambda: 0.0
        for key, val in kw.items():
            setattr(v, key, val)
        return v

    def test_ground_grid_is_local_and_fades(self):
        v = self.view(scene(1))
        pose, parts = v.scene(scene(1), me=0)
        grid = parts[0]
        self.assertGreater(len(grid.segs), 20)
        reach = 0.0
        for s in grid.segs:
            for p in ((s[0], s[2]), (s[3], s[5])):
                reach = max(reach, math.hypot(p[0] - pose.focus[0], p[1] - pose.focus[2]))
            # The ends of every grid line have no light.
            self.assertTrue(s[9] == 0.0 or s[10] == 0.0)
        self.assertLessEqual(reach, v.grid_radius + 1.0)       # no line to the horizon
        # The lines lie on 8 unit cells.
        xs = {round(s[0], 3) for s in grid.segs if s[0] == s[3]}
        self.assertTrue(all(abs(x / 8.0 - round(x / 8.0)) < 1e-6 for x in xs))
        self.assertLessEqual(max(s[9] for s in grid.segs), 0.4)   # dimmer than the rest

    def test_compass_has_the_north_marker(self):
        v = self.view(scene(1))
        pose, parts = v.scene(scene(1), me=0)
        compass = parts[1]
        north = [s for s in compass.segs if s[6:9] == viz.COLOR_NORTH]
        self.assertEqual(len(north), 3)
        self.assertGreater(min(s[2] for s in north), pose.focus[2] + 20.0)   # north is +z

    def test_buildings_only_inside_the_radius(self):
        v = self.view(scene(1))
        near = ((100.0, 24.0, 100.0), (120.0, 40.0, 120.0), "NEAR", (255, 0, 0))
        far = ((2000.0, 24.0, 2000.0), (2020.0, 40.0, 2020.0), "far", (255, 0, 0))
        state = sr.State(humans=[sr.HumanBlock(0, pos=(110.0, 25.0, 90.0))])
        pose, parts = v.scene(state, [near, far], me=0)
        mp = parts[2]
        self.assertTrue(mp.segs)
        self.assertTrue(all(s[0] < 500 for s in mp.segs))
        self.assertEqual([lab[1] for lab in mp.labels], ["NEAR"])

    def test_only_the_nearest_labels_are_drawn(self):
        v = self.view(scene(1))
        boxes = [((100.0 + 40 * i, 24.0, 100.0), (120.0 + 40 * i, 40.0, 120.0), "B%d" % i, (255, 0, 0)) for i in range(8)]
        state = sr.State(humans=[sr.HumanBlock(0, pos=(110.0, 25.0, 90.0))])
        pose, parts = v.scene(state, boxes, me=0)
        shown = [lab[1] for lab in v.picked_labels(pose, parts) if lab[3] == 0]
        self.assertEqual(shown, ["B0", "B1", "B2"])

    def test_roads_draw_two_edges(self):
        v = self.view(scene(1))
        v.roads = [((100.0, 24.0, 100.0), (100.0, 24.0, 228.0), 12.0)]
        state = sr.State(humans=[sr.HumanBlock(0, pos=(110.0, 25.0, 150.0))])
        pose, parts = v.scene(state, [], me=0)
        edges = parts[2].segs
        self.assertEqual(len(edges), 2)
        self.assertEqual(sorted(round(s[0]) for s in edges), [88, 112])

    def test_camera_orbits_behind_the_human(self):
        v = self.view(scene(1))
        state = sr.State(humans=[sr.HumanBlock(0, pos=(50.0, 25.0, 50.0), orient_a=(0.0, 1.0, 0.0),
                                               orient_b=(-1.0, 0.0, 0.0))])      # faces +x
        pose, _ = v.scene(state, me=0)
        self.assertAlmostEqual(pose.yaw, math.pi / 2, places=3)
        self.assertLess(pose.pos[0], 50.0 - 10.0)               # behind: smaller x
        self.assertGreater(pose.pos[1], 25.0 + 6.0)             # above
        self.assertGreater(math.degrees(-pose.pitch), 10.0)         # a low chase view, so the horizon shows
        self.assertLess(math.degrees(-pose.pitch), 25.0)
        dist = math.dist(pose.pos, pose.focus)
        self.assertGreater(dist, 30.0)
        self.assertLess(dist, 50.0)

    def test_yaw_follows_the_facing_smoothly(self):
        v = self.view(scene(1))
        t = [0.0]
        v.now = lambda: t[0]
        face = lambda a: sr.State(humans=[sr.HumanBlock(0, pos=(0.0, 25.0, 0.0), orient_b=(-math.sin(a), 0.0, -math.cos(a)))])
        v.scene(face(0.0), me=0)
        t[0] = 0.1
        pose, _ = v.scene(face(1.5), me=0)
        self.assertGreater(pose.yaw, 0.02)
        self.assertLess(pose.yaw, 1.0)                           # moved toward it, not there yet
        for step in range(40):
            t[0] += 0.1
            pose, _ = v.scene(face(1.5), me=0)
        self.assertAlmostEqual(pose.yaw, 1.5, places=1)

    def test_keys_change_the_camera(self):
        v = self.view(scene(1))
        state = sr.State(humans=[sr.HumanBlock(0, pos=(50.0, 25.0, 50.0))])
        base, _ = v.scene(state, me=0)
        v.key("left")
        turned, _ = v.scene(state, me=0)
        self.assertNotAlmostEqual(base.yaw, turned.yaw)
        v.key("plus")
        closer, _ = v.scene(state, me=0)
        self.assertLess(math.dist(closer.pos, closer.focus), math.dist(turned.pos, turned.focus))
        v.key("up")
        v.key("up")
        higher, _ = v.scene(state, me=0)
        self.assertLess(higher.pitch, closer.pitch)           # the camera rises and looks more downward
        v.key("down")
        v.key("down")
        v.key("down")
        self.assertGreater(v.scene(state, me=0)[0].pitch, higher.pitch)
        self.assertFalse(v.key("nothing"))

    def test_map_view_looks_straight_down(self):
        v = self.view(scene(1))
        state = sr.State(humans=[sr.HumanBlock(0, pos=(50.0, 25.0, 50.0))])
        v.key("top")
        pose, _ = v.scene(state, me=0)
        self.assertLess(pose.pitch, -1.4)
        self.assertAlmostEqual(pose.yaw, 0.0)
        self.assertAlmostEqual(pose.pos[0], 50.0, delta=6.0)
        v.key("top")
        self.assertGreater(v.scene(state, me=0)[0].pitch, -1.2)

    def test_moving_things_go_in_a_separate_list(self):
        v = self.view(scene(1))
        t = [0.0]
        v.now = lambda: t[0]
        state = sr.State(humans=[sr.HumanBlock(0, pos=(0.0, 25.0, 0.0)), sr.HumanBlock(1, pos=(5.0, 25.0, 0.0))])
        v.scene(state, me=0)
        t[0] = 0.5
        state.humans[1].pos = (5.0, 25.0, 4.0)                  # id 1 runs at 8 units a second
        pose, parts = v.scene(state, me=0)
        moving = [p for p in parts if p.moving]
        self.assertEqual(len(moving), 1)
        self.assertTrue(moving[0].segs)

    def test_position_only_objects_drive_along_their_motion(self):
        v = self.view(scene(1))
        t = [0.0]
        v.now = lambda: t[0]
        car = sr.ObjectBlock(3, mode=2, pos=(10.0, 25.0, 10.0))
        state = sr.State(objects=[car])
        v.scene(state)
        t[0] = 0.5
        car.pos = (10.0, 25.0, 16.0)
        pose, parts = v.scene(state)
        arrow = [s for p in parts for s in p.segs if p.moving]
        self.assertTrue(arrow)
        self.assertGreater(max(max(s[2], s[5]) for s in arrow), 18.0)       # the nose is ahead, along +z


class ModeAndToggleTests(unittest.TestCase):
    def view(self):
        v = viz.Viewer(100, 30)
        v.now = lambda: 0.0
        return v

    def state(self):
        return sr.State(humans=[sr.HumanBlock(0, pos=(50.0, 25.0, 50.0), orient_a=UP, orient_b=BACK)])

    def test_key_names_of_the_letters_and_the_function_keys(self):
        names = {"a": "rot_left", "d": "rot_right", "w": "tilt_up", "s": "tilt_down", "+": "plus", "-": "minus",
                 "m": "map", "f": "follow", "v": "orbit", "g": "free", "h": "hollow", "l": "labels", "r": "grid",
                 "b": "buildings", "t": "trails", "o": "glow", "c": "compass", "p": "snap", "q": "quit", "?": "help"}
        for ch, name in names.items():
            self.assertEqual(viz.key_name(ch), name)
            self.assertEqual(viz.key_name(ch.upper()), name if ch not in "adws" else "fast_" + name)
        self.assertIsNone(viz.key_name("x"))
        self.assertEqual(viz.key_name("\x03"), "quit")
        # The Windows codes behind a prefix: the arrows, then PF1 to PF9 and PF12 of the key bar.
        codes = {"K": "left", "M": "right", "H": "up", "P": "down", ";": "rot_right", "<": "pitch_up", "=": "plus",
                 ">": "map", "?": "follow", "@": "free", "A": "hollow", "B": "help", "C": "snap", chr(134): "quit"}
        for code, name in codes.items():
            self.assertEqual(viz.key_name(chr(0), code), name)
        for name in names.values():
            if name not in ("snap", "quit") and not name.startswith("fast_"):
                self.assertTrue(self.view().key(name), name)

    def test_toggles_flip_attributes(self):
        v = self.view()
        for name in ("hollow", "labels", "grid", "buildings", "trails", "glow", "compass", "help"):
            before = getattr(v, name)
            v.key(name)
            self.assertEqual(getattr(v, name), not before, name)
            v.key(name)
            self.assertEqual(getattr(v, name), before, name)
        self.assertTrue(v.hollow)                                # hollow is the default

    def test_toggles_change_the_scene(self):
        v = self.view()
        state = self.state()
        boxes = [((40.0, 24.0, 60.0), (60.0, 40.0, 80.0), "B", (255, 0, 0))]
        count = lambda: sum(len(p.segs) for p in v.scene(state, boxes, me=0)[1])
        full = count()
        v.key("grid")
        self.assertLess(count(), full)
        v.key("grid")
        v.key("compass")
        self.assertLess(count(), full)
        v.key("compass")
        v.key("buildings")
        self.assertLess(count(), full)
        v.key("buildings")
        pose, parts = v.scene(state, boxes, me=0)
        self.assertTrue(v.picked_labels(pose, parts))
        v.key("labels")
        self.assertEqual(v.picked_labels(pose, parts), [])

    def test_camera_modes(self):
        v = self.view()
        state = self.state()
        v.scene(state, me=0)
        self.assertEqual(v.mode, "follow")
        self.assertIn("CAM FOLLOW", v.cam_text())
        v.key("orbit")
        face = sr.State(humans=[sr.HumanBlock(0, pos=(50.0, 25.0, 50.0), orient_a=UP, orient_b=(-1.0, 0.0, 0.0))])
        pose, _ = v.scene(face, me=0)
        self.assertAlmostEqual(pose.yaw, 0.0)                    # the orbit does not turn with the human
        v.key("right")
        self.assertGreater(v.scene(face, me=0)[0].yaw, 0.05)
        v.key("map")
        self.assertEqual(v.mode, "map")
        v.key("map")
        self.assertEqual(v.mode, "follow")
        pose, _ = v.scene(state, me=0)
        v.key("free")
        before = tuple(v.scene(state, me=0)[0].pos)
        self.assertEqual(v.mode, "free")
        v.key("up")                                              # a free camera flies forward
        moved = v.scene(state, me=0)[0].pos
        self.assertNotEqual(before, tuple(moved))
        v.key("fast_up")
        self.assertGreater(math.dist(tuple(v.scene(state, me=0)[0].pos), tuple(moved)), 20.0)
        v.key("rot_right")                                       # A and D move a free camera sideways
        self.assertEqual(v.free_yaw, v.scene(state, me=0)[0].yaw)
        text = v.cam_text()
        self.assertRegex(text, r"^CAM FREE  AZ \d{3}  EL \d{3}  ZOOM \d{3}$")

    def test_cam_text_is_zero_padded(self):
        v = self.view()
        v.scene(self.state(), me=0)
        self.assertRegex(v.cam_text(), r"^CAM FOLLOW  AZ 000  EL 0\d\d  ZOOM 040$")


class SolidTests(unittest.TestCase):
    def box(self, solid, cam):
        ln = viz.Lines()
        viz.add_building(ln, (0, 24, 0), (20, 44, 20), (255, 255, 255), 4.0, solid_cam=cam if solid else None)
        return ln

    def test_solid_drops_the_hidden_edges_and_adds_light_lines(self):
        hollow = self.box(False, None)
        solid = self.box(True, (10.0, 60.0, -40.0))
        hidden = lambda ln: [s for s in ln.segs if s[2] > 19.9 and s[5] > 19.9]       # the far wall at z = 20
        self.assertTrue(hidden(hollow))
        far_edges = [s for s in solid.segs if s[2] > 19.9 and s[5] > 19.9 and s[1] == s[4] == 24 and s[9] >= 0.5]
        self.assertEqual(far_edges, [])                                    # the bottom edge of the far wall is hidden
        light = [s for s in solid.segs if s[9] < 0.5]
        self.assertGreater(len(light), 3)                                  # the floor lines and the roof lines

    def test_the_two_frames_differ_and_hollow_has_no_fill(self):
        state = sr.State(humans=[sr.HumanBlock(0, pos=(10.0, 25.0, -20.0), orient_a=UP, orient_b=BACK)])
        boxes = [((0.0, 24.0, 0.0), (20.0, 44.0, 20.0), "", (255, 255, 255))]
        frames = []
        for hollow in (True, False):
            v = viz.Viewer(100, 30)
            v.now = lambda: 0.0
            v.hollow = hollow
            v.grid = v.compass = False
            frames.append(plain(v.render(state, boxes, me=0)))
        self.assertNotEqual(frames[0], frames[1])
        # Outline only: the dots of the hollow frame stay on the edges, so the inside of the front face is empty.
        v = viz.Viewer(100, 30)
        v.now = lambda: 0.0
        v.grid = v.compass = False
        pose, parts = v.scene(state, boxes, me=0)
        self.assertTrue(all(s[9] > 0 for p in parts for s in p.segs if p is parts[1]))     # edges, no hatching
        self.assertEqual(len(parts[1].discs), 0)


class FrameTests(unittest.TestCase):
    def test_line_count_and_width(self):
        v = viz.Viewer(100, 30)
        frame = v.render(scene(5), boxes=[((-5, 24, 10), (5, 28, 20), "BASE 0", (255, 100, 100))], me=0)
        lines = plain(frame)
        self.assertEqual(len(lines), 30)
        for line in lines:
            self.assertLessEqual(len(line), 100)
        self.assertIn("TICK 5", lines[-1])

    def test_scene_is_drawn(self):
        v = viz.Viewer(100, 30)
        lines = plain(v.render(scene(3), me=0))
        drawn = sum(1 for line in lines[:-1] for ch in line if 0x2800 < ord(ch) <= 0x28FF)
        self.assertGreater(drawn, 30)

    def test_label_is_drawn(self):
        v = viz.Viewer(100, 30)
        v.grid = False
        state = sr.State(humans=[sr.HumanBlock(0, pos=(0.0, 25.0, 0.0))])
        lines = plain(v.render(state, boxes=[((-5, 24, 10), (5, 28, 20), "BASE 0", (255, 100, 100))], me=0))
        self.assertTrue(any("BASE 0" in line for line in lines[:-1]))

    def test_fully_clipped_scene_has_only_the_world(self):
        v = viz.Viewer(100, 30)
        v.grid = False
        state = scene(5, behind=True)       # the camera looks at +z, everything is behind it
        empty = sr.State(tick=5)
        full = plain(v.render(state, camera=((0.0, 25.0, 0.0), 0.0, 0.0)))
        v2 = viz.Viewer(100, 30)
        v2.grid = False
        bare = plain(v2.render(empty, camera=((0.0, 25.0, 0.0), 0.0, 0.0)))
        self.assertEqual(len(full), 30)
        self.assertEqual(full[:-1], bare[:-1])        # no human, car or item shows

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
        self.assertLess(each, 0.3, "frame took %.1f ms" % (each * 1000))

    def test_demo_scene_renders(self):
        v = viz.Viewer(120, 40)
        state, boxes, me = viz.demo_scene(2.0)
        self.assertEqual(len(plain(v.render(state, boxes, me))), 40)


if __name__ == "__main__":
    unittest.main()
