"""List the client storage objects that overlap another object.

Usage: python -I tools/re/client_overlap.py [group_globals_C3.csv]

With a file name, the tool lists only the objects of that file. Run it after you add globals
and after a merge: two sessions can give two names to one address. An overlap with a large
storage object (game, area_sector, packet_workspace) can be normal. An overlap between two
group files is a duplicate name.
"""
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE_SIZES = {"int": 4, "float": 4, "uint32_t": 4, "uint8_t": 1, "char": 1, "int16_t": 2, "uint16_t": 2, "vec3": 12,
              "void*": 4, "void *": 4, "uint32_t*": 4, "char*": 4, "double": 8, "int8_t": 1}


def main():
    header = (ROOT / "src/include/sr_types.h").read_text(encoding="utf-8")
    sizes = {m.group(1): int(m.group(2), 16)
             for m in re.finditer(r"SR_STATIC_ASSERT\(sizeof\((\w+)\) == (0x[0-9a-f]+)", header)}
    objects = []
    for row in csv.DictReader((ROOT / "analysis/client_win/storage.csv").open(encoding="utf-8")):
        objects.append(("storage", row["name"], int(row["client_rva"], 16), int(row["count"]) * int(row["size"])))
    for path in sorted((ROOT / "analysis/client_win").glob("group_globals_C*.csv")):
        for row in csv.DictReader(path.open(encoding="utf-8")):
            size = BASE_SIZES.get(row["type"]) or sizes.get(row["type"])
            if size is None:
                print("unknown size", path.name, row["name"], row["type"])
                continue
            objects.append((path.name, row["name"], int(row["client_rva"], 16), int(row["count"] or 1) * size))
    only = sys.argv[1] if len(sys.argv) > 1 else None
    for a in objects:
        if only and a[0] != only:
            continue
        for b in objects:
            if a is not b and a[2] < b[2] + b[3] and b[2] < a[2] + a[3]:
                print(f"{a[0]}:{a[1]} [{a[2]:#x}+{a[3]:#x}] overlaps {b[0]}:{b[1]} [{b[2]:#x}+{b[3]:#x}]")


if __name__ == "__main__":
    main()
