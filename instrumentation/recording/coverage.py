"""Checklist of the functions that a recording must still reach.

A recording (sv_args.txt) holds the calls of the functions that the hook recorder saw. The
checklist lists each function of the lifted table and counts its recorded calls over all the
recordings. A function with fewer calls than --min is missing.

Usage:
  python -I instrumentation/recording/coverage.py --side server|client
         [--min 2] [--dirs build/recordings] [--live <folder>] [--focus <folder>] [--watch]

  --side    server (src/lifted/lifted_table.c) or client (src/lifted/client/lifted_table.c)
  --dirs    folder with the recordings (a folder whose name starts with "client" is a client recording)
  --live    a folder that holds a running recording (its sv_args.txt counts too)
  --focus   write record_focus.txt there: the missing functions, hooked all the time in the next session
  --watch   repeat every 10 seconds (use it while someone plays)

The checklist is build/recordings/CHECKLIST-<side>.md. The text after each name comes from
instrumentation/recording/hints.csv, else from the comment above the function, else from its folder.
Calls of a recording are counted once: the result of each file is kept in build/recordings/_coverage_<side>.json.
"""
import argparse
import csv
import json
import re
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TABLES = {"server": ROOT / "src/lifted/lifted_table.c", "client": ROOT / "src/lifted/client/lifted_table.c"}
ENTRY = re.compile(r'^\s*\{ "(\w+)", 0x([0-9a-f]+), \(void \*\)\w+, (\d+), \w+, (\d+), "[pv]*", (\d+) \},')

# What to do in the game to reach the code of a folder. The key is the start of the path in src/.
AREA_HINTS = {
    "server": [
        ("src/net", "Join and leave the game with several players, chat, kick, send and read packets."),
        ("src/server/accounts", "Log in with an account, create one, let the server save the accounts."),
        ("src/server/players", "Players join, leave, die, respawn, buy, spend money."),
        ("src/server/main_loop", "Let the server run a round: start, play, end."),
        ("src/server", "Let the server run a round with players."),
        ("src/game/objects", "Players move, hold, throw and drop objects and items."),
        ("src/game/human", "Humans walk, jump, fall, fight and die."),
        ("src/game/car", "Drive cars, bump into things, use the horn and the doors."),
        ("src/game/wheels", "Drive cars on the street and on rails (trains)."),
        ("src/game/bond", "Humans or objects bump into things (physics of bonds)."),
        ("src/game", "Play a round: walk, shoot, drive, buy, use items."),
        ("src/common", "Any play: this is a helper used by many systems."),
    ],
    "client": [
        ("src/client/menu", "Open the menus: main menu, options, controls, voice options, credits."),
        ("src/client/editor", "Open the building, level and model editors, draw something and save."),
        ("src/client/render", "Look around in a city: walk, turn, enter buildings, look at the sky and the water."),
        ("src/client/audio", "Play with sound on: shoot, drive, talk with the voice chat, open the voice test."),
        ("src/client/input", "Use the keyboard, the mouse and a joystick if you have one, open the key settings."),
        ("src/client/image", "Take a screenshot, start a game that loads images."),
        ("src/client/textures", "Load a level and open the model editor (textures upload)."),
        ("src/client/setup", "Start the game, change the video mode, close the game."),
        ("src/game", "Join a game and play: walk, shoot, drive, buy, use items."),
        ("src/net", "Join a server, chat, leave, host a game."),
        ("src/common", "Any play: this is a helper used by many systems."),
    ],
}


def read_table(side):
    rows = {}
    for line in TABLES[side].read_text(encoding="utf-8", errors="replace").splitlines():
        m = ENTRY.match(line)
        if m:
            rows[int(m.group(2), 16)] = {"name": m.group(1), "side_flag": int(m.group(5))}
    return rows


def find_sources():
    """Map a function name to (file, comment) with one pass over the clean sources."""
    found = {}
    pattern = re.compile(r"^[A-Za-z_][\w \*]*?\b(\w+)\(")
    for folder in ("src/client", "src/game", "src/common", "src/net", "src/server"):
        for path in sorted((ROOT / folder).rglob("*.c")):
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            for i, line in enumerate(lines):
                m = pattern.match(line)
                if not m or line.rstrip().endswith(";") or m.group(1) in found:
                    continue
                comment = ""
                j = i - 1
                while j >= 0 and lines[j].strip() == "":
                    j -= 1
                if j >= 0 and lines[j].rstrip().endswith("*/"):
                    k = j
                    while k >= 0 and "/*" not in lines[k]:
                        k -= 1
                    block = " ".join(re.sub(r"^\s*/?\*+/?\s?|\s*\*/\s*$", "", s).strip() for s in lines[max(k, 0):j + 1])
                    comment = re.split(r"(?<=[.!?])\s", block.strip(), maxsplit=1)[0]
                found[m.group(1)] = (path.relative_to(ROOT).as_posix(), comment)
    return found


def read_hints(side):
    path = ROOT / "instrumentation/recording/hints.csv"
    hints = {}
    if path.exists():
        for row in csv.DictReader(path.open(encoding="utf-8")):
            if row["side"] == side:
                hints[row["function"]] = row["text"]
    return hints


def count_file(path):
    """Count the calls of each RVA in one sv_args.txt: a record starts with 'R <rva>'."""
    counts = {}
    with open(path, "rb") as stream:
        for line in stream:
            if line.startswith(b"R "):
                rva = int(line[2:].split()[0], 16)
                counts[rva] = counts.get(rva, 0) + 1
    return counts


def totals(side, dirs, live, cache_path):
    cache = {}
    if cache_path.exists():
        cache = json.loads(cache_path.read_text(encoding="utf-8"))
    files = []
    for d in dirs:
        base = ROOT / d
        if not base.is_dir():
            continue
        for rec in sorted(base.iterdir()):
            if not rec.is_dir() or rec.name.startswith("_"):
                continue
            if rec.name.startswith("client") != (side == "client"):
                continue
            # A recording of our own code tests itself: its inputs follow our code. Only the original game counts.
            if "subvulpescode" in rec.name or rec.name.endswith("-record"):
                continue
            session = rec / "session.ini"
            if session.exists() and "origin=own server" in session.read_text(encoding="utf-8", errors="replace"):
                continue
            if (rec / "sv_args.txt").exists():
                files.append(rec / "sv_args.txt")
    live_files = [Path(live) / "sv_args.txt"] if live and (Path(live) / "sv_args.txt").exists() else []
    total = {}
    for path in files + live_files:
        st = path.stat()
        key = str(path)
        entry = cache.get(key)
        if entry and entry["size"] == st.st_size and entry["mtime"] == int(st.st_mtime) and path not in live_files:
            counts = {int(k): v for k, v in entry["counts"].items()}
        else:
            counts = count_file(path)
            if path not in live_files:
                cache[key] = {"size": st.st_size, "mtime": int(st.st_mtime), "counts": {str(k): v for k, v in counts.items()}}
        for rva, n in counts.items():
            total[rva] = total.get(rva, 0) + n
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    cache_path.write_text(json.dumps(cache), encoding="utf-8")
    return total, len(files) + len(live_files)


def build(side, args):
    table = read_table(side)
    sources = find_sources()
    hints = read_hints(side)
    total, nfiles = totals(side, args.dirs, args.live, ROOT / ("build/recordings/_coverage_%s.json" % side))
    area_hints = AREA_HINTS[side]
    items = []
    for rva, row in sorted(table.items()):
        name = row["name"]
        src, comment = sources.get(name, ("", ""))
        group = src[4:].rsplit(".", 1)[0] if src else "unknown"
        hint = ""
        for prefix, text in area_hints:
            if src.startswith(prefix):
                hint = text
                break
        text = hints.get(name) or comment or "No comment in the code."
        items.append({"rva": rva, "name": name, "group": group, "calls": total.get(rva, 0), "text": text,
                      "hint": hint, "side_flag": row["side_flag"]})
    return items, nfiles


def write_checklist(side, items, nfiles, minimum, out):
    done = [i for i in items if i["calls"] >= minimum]
    missing = [i for i in items if i["calls"] < minimum]
    lines = ["# Recording checklist: %s" % side, "",
             "%d of %d functions have %d or more recorded calls. %d recording files counted. Made at %s." % (
                 len(done), len(items), minimum, nfiles, time.strftime("%Y-%m-%d %H:%M")),
             "", "## Missing", ""]
    groups = {}
    for i in missing:
        groups.setdefault(i["group"], []).append(i)
    for group in sorted(groups):
        first = groups[group][0]
        lines.append("### %s%s" % (group, (": " + first["hint"]) if first["hint"] else ""))
        for i in groups[group]:
            lines.append("- [ ] %s (%d of %d calls). %s" % (i["name"], i["calls"], minimum, i["text"]))
        lines.append("")
    lines += ["## Done", ""]
    for i in done:
        lines.append("- [x] %s (%d calls)" % (i["name"], i["calls"]))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return len(done), len(items)


def write_focus(items, minimum, folder, limit):
    missing = [i for i in items if i["calls"] < minimum][:limit]
    lines = ["# The functions that the checklist still misses (made by instrumentation/recording/coverage.py).",
             "# Format: rva class name"]
    lines += ["0x%x untested %s" % (i["rva"], i["name"]) for i in missing]
    (Path(folder) / "record_focus.txt").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return len(missing)


def export(path, args):
    """Write the list of the functions with their texts for the launcher (tools/launcher)."""
    rows = []
    for side in ("server", "client"):
        items, _ = build(side, args)
        for i in items:
            rows.append([side, "%x" % i["rva"], i["name"], i["group"], i["text"], i["hint"]])
    with open(path, "w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, quoting=csv.QUOTE_ALL, lineterminator="\n")
        writer.writerow(["side", "rva", "name", "group", "text", "hint"])
        writer.writerows(rows)
    print("%d functions -> %s" % (len(rows), path))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--export", help="write the function list for the launcher and stop")
    parser.add_argument("--side", choices=("server", "client"))
    parser.add_argument("--min", type=int, default=2, dest="minimum")
    parser.add_argument("--dirs", nargs="*", default=["build/recordings"])
    parser.add_argument("--live")
    parser.add_argument("--focus")
    parser.add_argument("--focus-max", type=int, default=150)
    parser.add_argument("--watch", action="store_true")
    args = parser.parse_args()
    if args.export:
        export(args.export, args)
        return 0
    if not args.side:
        parser.error("--side is needed")
    while True:
        items, nfiles = build(args.side, args)
        out = ROOT / ("build/recordings/CHECKLIST-%s.md" % args.side)
        done, total = write_checklist(args.side, items, nfiles, args.minimum, out)
        message = "%s: %d of %d functions done (%s)" % (args.side, done, total, out)
        if args.focus:
            message += ", %d in record_focus.txt" % write_focus(items, args.minimum, args.focus, args.focus_max)
        print(message, flush=True)
        if not args.watch:
            return 0
        time.sleep(10)


if __name__ == "__main__":
    sys.exit(main())
