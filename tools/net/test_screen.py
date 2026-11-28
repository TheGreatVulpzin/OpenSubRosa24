import io
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import screen  # noqa: E402


class ScreenTests(unittest.TestCase):
    def make(self):
        out = io.StringIO()
        return screen.Screen(out), out

    def test_first_frame_is_written_whole(self):
        s, out = self.make()
        s.write("one\ntwo\nthree")
        text = out.getvalue()
        for row, word in ((1, "one"), (2, "two"), (3, "three")):
            self.assertIn("\x1b[%d;1H%s" % (row, word), text)
        self.assertTrue(text.startswith(screen.BEGIN))
        self.assertTrue(text.endswith(screen.END))

    def test_only_changed_lines_are_sent(self):
        s, out = self.make()
        s.write("one\ntwo\nthree")
        out.truncate(0)
        out.seek(0)
        s.write("one\nTWO\nthree")
        text = out.getvalue()
        self.assertIn("\x1b[2;1H", text)
        self.assertIn("TWO", text)
        self.assertNotIn("one", text)
        self.assertNotIn("three", text)

    def test_an_equal_frame_sends_no_line(self):
        s, out = self.make()
        s.write("a\nb")
        first = s.sent
        s.write("a\nb")
        self.assertLess(s.sent, first)
        self.assertEqual(s.sent, len(screen.BEGIN) + len(screen.RESET) + len(screen.END))

    def test_a_change_of_size_clears_and_writes_whole(self):
        s, out = self.make()
        s.write("a\nb")
        out.truncate(0)
        out.seek(0)
        s.write("a\nb\nc")
        text = out.getvalue()
        self.assertIn(screen.CLEAR, text)
        self.assertIn("\x1b[1;1Ha", text)
        self.assertIn("\x1b[3;1Hc", text)

    def test_reset_makes_the_next_frame_whole(self):
        s, out = self.make()
        s.write("a\nb")
        s.reset()
        out.truncate(0)
        out.seek(0)
        s.write("a\nb")
        self.assertIn("\x1b[1;1Ha", out.getvalue())

    def test_inside_a_line_only_the_changed_cells_are_sent(self):
        s, out = self.make()
        red, green = "\x1b[38;2;255;0;0m", "\x1b[38;2;0;255;0m"
        s.write(red + "A" * 60 + "\x1b[0m\x1b[K")
        out.truncate(0)
        out.seek(0)
        n = s.write(red + "A" * 30 + green + "B" + red + "A" * 29 + "\x1b[0m\x1b[K")
        text = out.getvalue()
        self.assertIn("\x1b[1;31H", text)                       # the cell at column 31
        self.assertIn(green + "B", text)
        self.assertLess(n, 60)
        self.assertEqual(text.count("A"), 0)

    def test_cells_keep_the_style_of_each_character(self):
        row = screen.cells("\x1b[0m\x1b[31mA\x1b[0mB\x1b[K")
        self.assertEqual(row, [("\x1b[31m", "A"), ("", "B")])

    def test_two_nearby_changes_make_one_run(self):
        s, out = self.make()
        s.write("a" * 40)
        out.truncate(0)
        out.seek(0)
        s.write("a" * 10 + "X" + "a" * 3 + "Y" + "a" * 25)
        text = out.getvalue()
        self.assertEqual(text.count("\x1b[1;"), 1)
        self.assertIn("XaaaY", text)

    def test_the_sixel_goes_after_the_lines(self):
        s, out = self.make()
        s.write("a", "\x1bPqSIXEL\x1b\\")
        text = out.getvalue()
        self.assertLess(text.index("\x1b[1;1Ha"), text.index("\x1bPq"))
        s.write("a")
        self.assertNotIn("\x1bPq", text[len(text):])


if __name__ == "__main__":
    unittest.main()
