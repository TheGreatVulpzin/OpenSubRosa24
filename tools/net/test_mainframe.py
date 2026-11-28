#!/usr/bin/env python3
"""Tests for mainframe.py. Run: python -I tools/net/test_mainframe.py"""
import io
import os
import re
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mainframe  # noqa: E402
import srpacket as sr  # noqa: E402

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
SGR = re.compile(r"\x1b\[([0-9;]*)m")
CUBE = (0, 95, 135, 175, 215, 255)


def plain(frame):
    return [ANSI.sub("", line) for line in frame.split("\n")]


def rgb_of(code):
    if code >= 232:
        v = 8 + 10 * (code - 232)
        return v, v, v
    if code >= 16:
        n = code - 16
        return CUBE[n // 36], CUBE[(n // 6) % 6], CUBE[n % 6]
    raise AssertionError("basic color %d" % code)


def is_green(rgb):
    r, g, b = rgb
    return g >= r and g >= b and r <= 215 and b <= 215


def is_amber(rgb):
    r, g, b = rgb
    return r > g > b and b <= 20 and g >= 0.4 * r


def colors_of(frame):
    """Every (kind, rgb) set by a 38 or 48 code of the frame."""
    found = []
    for params in SGR.findall(frame):
        p = [int(x) for x in params.split(";") if x]
        i = 0
        while i < len(p):
            if p[i] in (38, 48) and i + 2 < len(p) and p[i + 1] == 5:
                found.append((p[i], rgb_of(p[i + 2])))
                i += 3
            elif p[i] in (38, 48) and i + 4 < len(p) and p[i + 1] == 2:
                found.append((p[i], tuple(p[i + 2:i + 5])))
                i += 5
            else:
                if p[i] not in (0, 1, 22, 39, 49):
                    raise AssertionError("unexpected code %d" % p[i])
                i += 1
    return found


def scene(n=3):
    humans = [sr.HumanBlock(i, pos=(i * 2.0, 1.0, 20.0), health=100 - i * 7) for i in range(n)]
    objects = [sr.ObjectBlock(0, pos=(0.0, 0.6, 25.0))]
    items = [sr.ItemBlock(0, pos=(1.0, 0.3, 22.0))]
    events = [sr.Event(sr.EV_MESSAGE, 7, {"text": b"hello there"}), sr.Event(sr.EV_UPDATEITEM, 9)]
    return sr.State(tick=5, phase=2, phase_timer=100, team_counts=(1, 2, 0), humans=humans, objects=objects,
                    items=items, events=events)


EXTRA = {"tps": 61.2, "clock": 12345, "teams": [{"manager": 2, "members": 3, "missions": 1}],
         "calls": {"alpha": 10, "beta": 40, "gamma": 5}, "log": ["one", "two"], "recorder": True, "bot": "b1"}


class FrameTests(unittest.TestCase):
    def check_size(self, width, height, state=None, extra=None):
        mf = mainframe.Mainframe(width, height)
        lines = plain(mf.render(state if state is not None else scene(), [((-5, 0, 10), (5, 4, 20), "base", (255, 100, 100))],
                                0, extra))
        self.assertEqual(len(lines), height)
        for line in lines:
            self.assertLessEqual(len(line), width)
        return lines

    def test_sizes(self):
        for w, h in ((100, 30), (120, 40), (160, 50), (101, 31)):
            lines = self.check_size(w, h, extra=EXTRA)
            self.assertIn("SUB ROSA OPERATIONS CONSOLE", lines[1])
            self.assertEqual(len(lines[5]), w)

    def test_panels_are_there(self):
        text = "\n".join(self.check_size(120, 40, extra=EXTRA))
        for name in ("MAIN VIEW", "PLAYERS", "TEAMS", "EVENT LOG", "CALL COUNTERS", "TICK 5", "TPS 61.2",
                     "WORLD CLOCK 12345", "HELLO THERE", "BETA", "LINK", "RECORDER"):
            self.assertIn(name, text)

    def test_missing_extra_and_state(self):
        self.check_size(100, 30, extra=None)
        mf = mainframe.Mainframe(100, 30)
        lines = plain(mf.render(None))
        self.assertEqual(len(lines), 30)
        self.assertIn("NO LINK", "\n".join(lines))

    def test_too_small(self):
        mf = mainframe.Mainframe(99, 29)
        lines = plain(mf.render(scene()))
        self.assertEqual(len(lines), 29)
        self.assertIn("TERMINAL TOO SMALL", "\n".join(lines))
        for line in lines:
            self.assertLessEqual(len(line), 99)
        for w, h in ((10, 3), (1, 1)):
            lines = plain(mainframe.Mainframe(w, h).render(scene()))
            self.assertEqual(len(lines), h)
            for line in lines:
                self.assertLessEqual(len(line), w)

    def test_colors_are_green_or_amber(self):
        for state in (scene(), None):
            frame = mainframe.Mainframe(120, 40).render(state, [], 0, EXTRA)
            found = colors_of(frame)
            self.assertGreater(len(found), 100)
            for kind, rgb in found:
                self.assertTrue(rgb == (0, 0, 0) or is_green(rgb) or is_amber(rgb), (kind, rgb))
        small = mainframe.Mainframe(50, 10).render(scene())
        for kind, rgb in colors_of(small):
            self.assertTrue(rgb == (0, 0, 0) or is_green(rgb) or is_amber(rgb), (kind, rgb))

    def test_viewer_colors_become_green(self):
        mf = mainframe.Mainframe(120, 40)
        state = scene(6)
        state.humans[1].health = 10          # a hurt human is drawn amber
        frame = mf.render(state, [((-5, 0, 10), (5, 4, 20), "base 0", (255, 100, 100))], 0, EXTRA)
        for kind, rgb in colors_of(frame):
            self.assertTrue(rgb == (0, 0, 0) or is_green(rgb) or is_amber(rgb), (kind, rgb))
        self.assertTrue(any(is_amber(rgb) for _, rgb in colors_of(frame)))

    def test_scanlines(self):
        self.assertEqual(mainframe._dim_row("\x1b[38;5;46mx"), "\x1b[38;5;40mx")
        frame = mainframe.Mainframe(100, 30).render(scene(), [], 0, EXTRA)
        rows = frame.split("\n")
        self.assertNotIn("38;5;46m", rows[3])        # odd row: the bright shade is dimmed

    def test_every_line_clears_first(self):
        for row in mainframe.Mainframe(100, 30).render(scene()).split("\n"):
            self.assertIn("\x1b[K", row[:20])
            self.assertFalse(row.endswith("\x1b[K"))

    def test_players_marks_the_bot(self):
        lines = self.check_size(120, 40, extra=EXTRA)
        mine = [ln for ln in lines if ">  0 " in ln]
        self.assertEqual(len(mine), 1)

    def test_many_players_keep_me_visible(self):
        humans = [sr.HumanBlock(i, pos=(float(i), 1.0, 20.0)) for i in range(60)]
        state = sr.State(tick=1, humans=humans)
        mf = mainframe.Mainframe(100, 30)
        text = "\n".join(plain(mf.render(state, [], 55)))
        self.assertIn(">", text)
        self.assertIn(" 55 ", text)
        self.assertIn("MORE", text)

    def test_event_text(self):
        state = scene()
        pairs = mainframe.event_lines(state.events)
        self.assertEqual(pairs[0][0], "msg")
        self.assertIn("hello there", pairs[0][1])
        self.assertIn("UPDATEITEM", pairs[1][1])

    def test_lamp_levels(self):
        mf = mainframe.Mainframe(100, 30)
        t = mf.now()
        lamps = dict(mf._lamps(scene(), {"recorder": True}, t))
        self.assertEqual((lamps["LINK"], lamps["WORLD"], lamps["ROUND"], lamps["RECORDER"], lamps["BOT"]),
                         (2, 2, 2, 2, 0))
        lamps = dict(mf._lamps(scene(), {}, t + 10))       # same tick for 10 s: link stalled
        self.assertEqual(lamps["LINK"], 1)
        self.assertEqual(dict(mf._lamps(None, {}, t))["LINK"], 0)

    def test_reel_turns(self):
        a, b = mainframe.reel(0.0), mainframe.reel(0.5)
        self.assertEqual(len(a), 2)
        self.assertTrue(all(len(r) == 3 for r in a))
        self.assertNotEqual(a, b)

    def test_trail_marks_old_positions(self):
        mf = mainframe.Mainframe(100, 30)
        state = scene(1)
        t = [0.0]
        mf.now = lambda: t[0]
        for step in range(4):
            state.humans[0].pos = (step * 2.0, 1.0, 20.0)
            t[0] += 0.2
            trails = mf._trails(state, t[0])
        self.assertEqual(len(trails), 3)

    def timed(self, move):
        humans = [sr.HumanBlock(i, pos=(i % 10 * 3.0, 1.0, 10.0 + i // 10 * 3.0)) for i in range(50)]
        state = sr.State(tick=1, phase=2, humans=humans, objects=[sr.ObjectBlock(0, pos=(0, 0.6, 15))])
        mf = mainframe.Mainframe(120, 40)
        mf.render(state, [], 0, EXTRA)
        best = 1.0
        for _ in range(3):                      # the best of three runs, to ignore a noisy moment
            start = time.perf_counter()
            for k in range(10):
                state.tick += 1
                if move:
                    for h in humans:
                        h.pos = (h.pos[0] + 0.4, h.pos[1], h.pos[2])
                mf.render(state, [], 0, EXTRA)
            best = min(best, (time.perf_counter() - start) / 10)
        return best

    def test_frame_time(self):
        each = self.timed(False)
        self.assertLess(each, 0.030, "frame took %.1f ms" % (each * 1000))

    def test_frame_time_with_trails(self):
        each = self.timed(True)
        self.assertLess(each, 0.040, "frame took %.1f ms" % (each * 1000))


class BootTests(unittest.TestCase):
    def test_boot_text_and_skip(self):
        mf = mainframe.Mainframe(100, 30)
        mf.sleep = lambda s: None
        out = io.StringIO()
        mf.boot(out)
        text = ANSI.sub("", out.getvalue())
        for word in ("MEMORY TEST", "00640K", "LOADING SUBROSA24D.EXE ... OK", "SYSTEM READY"):
            self.assertIn(word, text)
        out = io.StringIO()
        mf.boot(out, skip=True)
        self.assertEqual(out.getvalue(), "")

    def test_boot_takes_about_two_seconds(self):
        mf = mainframe.Mainframe(100, 30)
        total = []
        mf.sleep = total.append
        mf.boot(io.StringIO())
        self.assertGreater(sum(total), 1.5)
        self.assertLess(sum(total), 2.6)


class DemoTests(unittest.TestCase):
    def test_demo_data_renders(self):
        mf = mainframe.Mainframe(120, 40)
        state, boxes, me, extra = mainframe.demo_data(3.0)
        lines = plain(mf.render(state, boxes, me, extra))
        self.assertEqual(len(lines), 40)
        self.assertIn("HUMANSIMULATION", "\n".join(lines))


if __name__ == "__main__":
    unittest.main()
