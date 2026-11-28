"""Writes frames to a terminal with as little output as possible.

A frame is text with ANSI codes, one line after another. A terminal is slow when each frame rewrites every
line, and it can show a half drawn picture. This writer sends only what changed since the last frame: a line
that is equal is skipped, and in a line that changed only the runs of cells that differ are written, each
with a cursor move. Everything goes inside a synchronized update (DEC mode 2026, ignored by a terminal
that does not know it). A sixel image goes after the lines and is sent only when the caller gives one.

    screen = Screen(sys.stdout)
    screen.write(frame, sixel)      # returns the number of characters written
    screen.reset()                  # the next frame is written whole (after a clear, or a change of size)
"""
import re

BEGIN = "\x1b[?2026h"
END = "\x1b[?2026l"
CLEAR = "\x1b[2J"
RESET = "\x1b[0m"
TOKEN = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|.", re.S)
GAP = 6                        # two changed runs closer than this many cells are written as one


def cells(line):
    """The cells of a line: a list of (style, character). The style is the foreground, the background and
    the other attributes in force: a later color replaces an earlier one, a reset clears all three.
    A line must hold only SGR codes, the erase to the end of the line and plain characters."""
    out = []
    fg = bg = other = ""
    style = ""
    for tok in TOKEN.findall(line):
        if tok[0] == "":
            if tok == RESET:
                fg = bg = other = ""
            elif tok.startswith("[38;"):
                fg = tok
            elif tok.startswith("[48;"):
                bg = tok
            elif tok.endswith("m"):
                other += tok
            else:
                continue                                # the erase to the end of the line, and other codes
            style = bg + fg + other
            continue
        out.append((style, tok))
    return out


class Screen:
    def __init__(self, out):
        self.out = out
        self.prev = []               # the lines of the last frame
        self.prev_cells = []         # their cells, parsed when a line changes
        self.sent = 0                # characters written by the last call

    def reset(self, clear=True):
        """Forget the last frame. With clear the screen is cleared at the next write."""
        self.prev = []
        self.prev_cells = []
        if clear:
            self.out.write(CLEAR)

    def _runs(self, row, old, new):
        """The text that turns the cells of a line into the new ones, or None when it is not worth it."""
        if len(old) != len(new):
            return None
        diff = [i for i in range(len(new)) if old[i] != new[i]]
        if not diff:
            return ""
        runs = []
        start = last = diff[0]
        for i in diff[1:]:
            if i - last > GAP:
                runs.append((start, last))
                start = i
            last = i
        runs.append((start, last))
        parts = []
        for a, b in runs:
            parts.append("\x1b[%d;%dH" % (row + 1, a + 1))
            style = None
            for i in range(a, b + 1):
                st, ch = new[i]
                if st != style:
                    parts.append(RESET + st)
                    style = st
                parts.append(ch)
        return "".join(parts)

    def write(self, frame, sixel=None):
        lines = frame.split("\n")
        parts = [BEGIN]
        if len(lines) != len(self.prev):
            if self.prev:
                parts.append(CLEAR)                 # the size changed: the old lines are in the wrong rows
            self.prev = []
            self.prev_cells = []
        new_cells = []
        for row, line in enumerate(lines):
            if row < len(self.prev) and self.prev[row] == line:
                new_cells.append(self.prev_cells[row])
                continue
            now = cells(line)
            new_cells.append(now)
            text = None
            if row < len(self.prev):
                text = self._runs(row, self.prev_cells[row], now)
            if text is None:
                text = "\x1b[%d;1H%s" % (row + 1, line)
            parts.append(text)
        if sixel:
            parts.append(sixel)
        parts.append(RESET)
        parts.append(END)
        data = "".join(parts)
        self.out.write(data)
        self.out.flush()
        self.prev = lines
        self.prev_cells = new_cells
        self.sent = len(data)
        return self.sent
