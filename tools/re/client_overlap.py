"""List the client storage objects that overlap another object.

Usage: python -I tools/re/client_overlap.py [--strict] [group_globals_C3.csv]

With a file name, the tool lists only the objects of that file. Run it after you add globals
and after a merge: two sessions can give two names to one address. An overlap with a large
storage object (game, area_sector, packet_workspace) can be normal. An overlap between two
group files is a duplicate name.

The size of a type comes from the static asserts of the type headers, or from the raw array
of a union type (lf_raw), or from the layout of a plain struct. A type with no size is an error.

--strict: the exit code is 1 when a type has no size or an overlap is not listed in
analysis/client_win/storage_aliases.csv (columns: inner, outer, note; the names are the
object names). The two names of a listed pair can overlap.
"""
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE_SIZES = {"int": 4, "float": 4, "uint32_t": 4, "uint8_t": 1, "char": 1, "int16_t": 2, "uint16_t": 2, "vec3": 12,
              "void*": 4, "void *": 4, "uint32_t*": 4, "char*": 4, "double": 8, "int8_t": 1}
TYPE_HEADERS = ("sr_types.h", "sr_image_types.h", "sr_audio_types.h", "sr_input_types.h")


# The client uses only the first 0x54 bytes of a TimerList. The server timers follow the client ones.
CLIENT_SIZES = {"timerlist": 0x54}


def type_sizes():
    sizes = {}
    for name in TYPE_HEADERS:
        text = (ROOT / "src/include" / name).read_text(encoding="utf-8")
        for m in re.finditer(r"SR_STATIC_ASSERT\(sizeof\((\w+)\) == (0x[0-9a-f]+|\d+)", text):
            sizes[m.group(1)] = int(m.group(2), 0)
        # A union type with a raw array: the array gives the size.
        for m in re.finditer(r"lf_raw\[(0x[0-9a-f]+|\d+)\];\s*\}\s*(\w+);", text):
            sizes.setdefault(m.group(2), int(m.group(1), 0))
    return sizes


def struct_size(body, known):
    """Size of a plain struct of base types, known types, pointers and arrays (32-bit layout,
    natural alignment, a double has 8 bytes of alignment). None when the body has a union, a
    bit field or a type that is not known."""
    if "{" in body or ":" in body:
        return None
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    offset, top = 0, 1
    for decl in filter(None, (d.strip() for d in body.split(";"))):
        head, _, names = decl.partition(" ")
        while head in ("unsigned", "const", "volatile", "struct"):
            head, _, names = names.strip().partition(" ")
        for item in names.split(","):
            item = item.strip()
            pointer = item.startswith("*")
            item = item.lstrip("* ")
            count = 1
            for dim in re.findall(r"\[(0x[0-9a-f]+|\d+)\]", item):
                count *= int(dim, 0)
            size = 4 if pointer else (BASE_SIZES.get(head) or known.get(head))
            if size is None:
                return None
            align = 4 if pointer else (8 if head == "double" else min(size, 4) if head not in known else 4)
            if size == 2 and not pointer:
                align = 2
            if size == 1 and not pointer:
                align = 1
            offset = (offset + align - 1) // align * align + size * count
            top = max(top, align)
    return (offset + top - 1) // top * top


def inline_struct_sizes(known):
    """Sizes of the plain structs that have no static assert, by repeated passes."""
    bodies = {}
    for name in TYPE_HEADERS:
        text = (ROOT / "src/include" / name).read_text(encoding="utf-8")
        for m in re.finditer(r"typedef struct \w+ \{([^{}]*)\} (\w+);", text):
            bodies[m.group(2)] = m.group(1)
    found = {}
    for _ in range(4):
        for name, body in bodies.items():
            if name not in known and name not in found:
                size = struct_size(body, {**known, **found})
                if size is not None:
                    found[name] = size
    return found, bodies


def listed_pairs():
    path = ROOT / "analysis/client_win/storage_aliases.csv"
    if not path.exists():
        return set()
    return {(r["inner"], r["outer"]) for r in csv.DictReader(path.open(encoding="utf-8"))}


def main():
    args = sys.argv[1:]
    strict = "--strict" in args
    names = [a for a in args if not a.startswith("--")]
    sizes = type_sizes()
    sizes.update(inline_struct_sizes(sizes)[0])
    problems = 0
    objects = []
    for row in csv.DictReader((ROOT / "analysis/client_win/storage.csv").open(encoding="utf-8")):
        objects.append(("storage", row["name"], int(row["client_rva"], 16), int(row["count"]) * int(row["size"])))
    for path in sorted((ROOT / "analysis/client_win").glob("group_globals_C*.csv")):
        for row in csv.DictReader(path.open(encoding="utf-8")):
            size = BASE_SIZES.get(row["type"]) or sizes.get(row["type"])
            if size is None:
                print("unknown size", path.name, row["name"], row["type"])
                problems += 1
                continue
            total = CLIENT_SIZES.get(row["name"]) or int(row["count"] or 1) * size
            objects.append((path.name, row["name"], int(row["client_rva"], 16), total))
    only = names[0] if names else None
    pairs = listed_pairs()
    for a in objects:
        if only and a[0] != only:
            continue
        for b in objects:
            if a is not b and a[2] < b[2] + b[3] and b[2] < a[2] + a[3]:
                listed = (a[1], b[1]) in pairs or (b[1], a[1]) in pairs
                note = " (listed)" if listed else ""
                print(f"{a[0]}:{a[1]} [{a[2]:#x}+{a[3]:#x}] overlaps {b[0]}:{b[1]} [{b[2]:#x}+{b[3]:#x}]{note}")
                if not listed:
                    problems += 1
    if strict:
        print(f"{problems} problem(s)")
        sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
