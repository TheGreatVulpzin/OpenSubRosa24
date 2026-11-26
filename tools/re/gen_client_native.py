"""Make the lists that check the storage of the native client against the original executable.

The data of the client is plain source: src/client/data/*.c and src/include/sr_client_storage.h.
This tool does not write them. It reads analysis/client_win/storage.csv and writes two lists for
instrumentation/tests/test_client_storage.c (run it with tools/test_client_storage.bat):

  instrumentation/client/runtime/storage_regions.inc  the objects with start values: name, address, RVA, bytes
  instrumentation/client/runtime/storage_sizes.inc    every object: name, size of the C object, bytes in the original

The test fails when a start value or a size of a C object differs from the original. A name that
is in the CSV and not in the source fails to compile. Run it after each change to the data files.

Usage: python -I tools/re/gen_client_native.py          write the two lists
       python -I tools/re/gen_client_native.py --check  stop when a list is stale
"""
import csv
import re
import runpy
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REGIONS = ROOT / "instrumentation/client/runtime/storage_regions.inc"
SIZES = ROOT / "instrumentation/client/runtime/storage_sizes.inc"
EXE = ROOT / "original/client/subrosa.exe"
DUMMY_LIMIT = 32 << 20
STRING_TABLES = ("const char *", "char *")
SKIP_START = ("gl_",)

storage = runpy.run_path(str(ROOT / "tools/re/gen_client_storage.py"))
overlap = runpy.run_path(str(ROOT / "tools/re/client_overlap.py"))
texts = runpy.run_path(str(ROOT / "tools/re/gen_client_texts.py"))


class Image:
    """Bytes and relocations of the client executable."""

    def __init__(self):
        self.data = EXE.read_bytes()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        self.base = struct.unpack_from("<I", self.data, pe + 24 + 28)[0]
        self.secs = texts["sections"](self.data)
        self.relocs = set()
        count = struct.unpack_from("<H", self.data, pe + 6)[0]
        optional = struct.unpack_from("<H", self.data, pe + 20)[0]
        for i in range(count):
            o = pe + 24 + optional + 40 * i
            if self.data[o:o + 8].rstrip(b"\0") != b".reloc":
                continue
            vsize, va, rsize, roff = struct.unpack_from("<IIII", self.data, o + 8)
            raw, pos = self.data[roff:roff + rsize], 0
            while pos + 8 <= len(raw):
                page, size = struct.unpack_from("<II", raw, pos)
                if size < 8:
                    break
                for entry in struct.unpack_from(f"<{(size - 8) // 2}H", raw, pos + 8):
                    if entry >> 12 == 3:
                        self.relocs.add(page + (entry & 0xFFF))
                pos += size

    def read(self, rva, size):
        for va, vsize, raw in self.secs:
            if va <= rva < va + vsize:
                part = raw[rva - va:rva - va + size]
                return part.ljust(size, b"\0")
        return b"\0" * size

    def text(self, address):
        return texts["read_text"](self.secs, address - self.base, False)


def type_size(sizes, typ):
    typ = typ.strip()
    if typ.endswith("*"):
        return 4
    return overlap["BASE_SIZES"].get(typ) or sizes.get(typ)


def load():
    """Objects with their size in the executable, the alias list and the dummies."""
    objects, dummies = storage["collect"]()
    sizes = overlap["type_sizes"]()
    sizes.update(overlap["inline_struct_sizes"](sizes)[0])
    csv_size = {}
    for r in csv.DictReader((ROOT / "analysis/client_win/storage.csv").open(encoding="utf-8")):
        csv_size["sr_data_" + r["name"]] = int(r["size"])
    rows = []
    for name, typ, count, rva in objects:
        size = csv_size.get(name) or type_size(sizes, typ)
        if size is None:
            raise SystemExit(f"{name}: no size for type {typ}")
        rows.append({"name": name, "type": typ, "count": count, "rva": rva, "size": size,
                     "bytes": count * size})
    # Room after an array that the original executable fills past its end into an idle neighbor.
    slack = {}
    for s in csv.DictReader((ROOT / "analysis/client_win/native_slack.csv").open(encoding="utf-8")):
        slack["cl_data_" + s["name"]] = int(s["extra_bytes"])
    for r in rows:
        extra = slack.pop(r["name"], 0)
        if extra:
            r["count"] += -(-extra // r["size"])
            r["bytes"] = r["count"] * r["size"]
    if slack:
        raise SystemExit("native_slack.csv: unknown name " + ", ".join(slack))
    # A datum that GLee owns in the native build: an OpenGL function pointer slot or an extension flag.
    slots = {}
    for line in (ROOT / "analysis/client_win/gl_slots.csv").read_text(encoding="utf-8").splitlines()[1:]:
        rva, name = line.split(",")[:2]
        slots[int(rva, 16)] = "cl_" + name
    flags = {}
    for path in sorted((ROOT / "analysis/client_win").glob("group_globals_C*.csv")):
        for g in csv.DictReader(path.open(encoding="utf-8")):
            m = re.search(r"\bGL_(\w+)", g.get("evidence") or "")
            if g["name"].startswith("glee_") and m:
                flags["cl_data_" + g["name"]] = "_GLEE_" + m.group(1)
    for r in rows:
        r["glee"] = slots.get(r["rva"]) if r["name"].startswith("cl_data_gl_") else flags.get(r["name"])
    by_plain = {}
    for r in rows:
        plain = re.sub(r"^(sr|cl)_data_", "", r["name"])
        if plain in by_plain:
            raise SystemExit(f"two objects named {plain}")
        by_plain[plain] = r
    aliases, exprs = {}, {}
    path = ROOT / "analysis/client_win/storage_aliases.csv"
    for a in csv.DictReader(path.open(encoding="utf-8")):
        if a["inner"] not in by_plain or a["outer"] not in by_plain:
            raise SystemExit(f"alias {a['inner']} in {a['outer']}: unknown name")
        aliases[a["inner"]] = a["outer"]
        exprs[a["inner"]] = a["expr"]
    for r in rows:
        plain = re.sub(r"^(sr|cl)_data_", "", r["name"])
        root, seen = plain, set()
        while root in aliases:
            if root in seen:
                raise SystemExit(f"alias loop at {root}")
            seen.add(root)
            root = aliases[root]
        r["root"] = by_plain[root] if root != plain else None
        r["expr"] = exprs.get(plain)
    return rows, dummies


GUARD_SIZE = 256


def regions(rows):
    """The objects that start with data and no pointers, for the test of the start values."""
    image = Image()
    out = ["/* Generated by tools/re/gen_client_native.py. Do not edit. name, address, rva, bytes. */"]
    for r in rows:
        if r["glee"] or r["name"].startswith(tuple("cl_data_" + p for p in SKIP_START)):
            continue
        raw = image.read(r["rva"], r["bytes"])
        if not any(raw) or any(r["rva"] <= p < r["rva"] + r["bytes"] for p in image.relocs):
            continue
        address = r["name"] if r["count"] > 1 else "&" + r["name"]
        out.append(f'{{ "{r["name"]}", (const void *){address}, 0x{r["rva"]:x}u, {r["bytes"]}u }},')
    return "\n".join(out) + "\n"


def sizes(rows):
    """The size of each object in the original executable, to compare with the size of the C object."""
    out = ["/* Generated by tools/re/gen_client_native.py. Do not edit. name, size of the C object, bytes in the original. */"]
    for r in rows:
        if r["root"] is None and not r["glee"]:
            out.append(f'{{ "{r["name"]}", sizeof({r["name"]}), {r["bytes"]}u }},')
    return "\n".join(out) + "\n"


def main():
    rows, dummies = load()
    files = {REGIONS: regions(rows), SIZES: sizes(rows)}
    if "--check" in sys.argv[1:]:
        stale = [p.name for p, t in files.items() if not p.exists() or p.read_text(encoding="utf-8") != t]
        if stale:
            print("stale:", ", ".join(stale))
            sys.exit(1)
        print("native storage is current")
        return
    for path, text in files.items():
        path.write_text(text, encoding="utf-8", newline="\n")
    real = sum(1 for r in rows if r["root"] is None)
    print(f"{real} objects, {len(rows) - real} aliases, {len(dummies)} dummies")


if __name__ == "__main__":
    main()
