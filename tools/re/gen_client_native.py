"""Generate the native storage of the client: every datum as a real C object.

The test build reads each datum at sr_base plus an RVA (instrumentation/client/runtime/
storage_adapter.h, from tools/re/gen_client_storage.py). The native build has no image.
This tool writes the other form of the same names:

  src/include/sr_client_storage.h  extern declarations, the alias macros, the hook header
  src/client/storage.c             one definition per object and the start values
  instrumentation/client/runtime/storage_regions.inc  the objects with start values, for the test

An alias (analysis/client_win/storage_aliases.csv) is an object that lies inside a bigger
object. It becomes a macro that points into the bigger one. The compiler checks that the alias
fits (static assert). The start values come from the .data of the client executable. A text
pointer table starts with C strings. The OpenGL pointer slots start empty: the native build gets
them from GLee.

Usage: python -I tools/re/gen_client_native.py          write the two files
       python -I tools/re/gen_client_native.py --check  stop when a file is stale
"""
import csv
import re
import runpy
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "src/include/sr_client_storage.h"
SOURCE = ROOT / "src/client/storage.c"
REGIONS = ROOT / "instrumentation/client/runtime/storage_regions.inc"
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
    by_plain = {}
    for r in rows:
        plain = re.sub(r"^(sr|cl)_data_", "", r["name"])
        if plain in by_plain:
            raise SystemExit(f"two objects named {plain}")
        by_plain[plain] = r
    aliases = {}
    path = ROOT / "analysis/client_win/storage_aliases.csv"
    for a in csv.DictReader(path.open(encoding="utf-8")):
        if a["inner"] not in by_plain or a["outer"] not in by_plain:
            raise SystemExit(f"alias {a['inner']} in {a['outer']}: unknown name")
        aliases[a["inner"]] = a["outer"]
    for r in rows:
        plain = re.sub(r"^(sr|cl)_data_", "", r["name"])
        root, seen = plain, set()
        while root in aliases:
            if root in seen:
                raise SystemExit(f"alias loop at {root}")
            seen.add(root)
            root = aliases[root]
        r["root"] = by_plain[root] if root != plain else None
        if r["root"] is not None:
            r["offset"] = r["rva"] - r["root"]["rva"]
            if r["offset"] < 0:
                raise SystemExit(f"{r['name']}: starts before {r['root']['name']}")
    return rows, dummies


def declaration(r):
    arr = f"[{r['count']}]" if r["count"] > 1 else ""
    return f"{r['type']} {r['name']}{arr}"


def alias_macro(r):
    base = f"((char *)&{r['root']['name']} + 0x{r['offset']:x})"
    if r["count"] > 1:
        return f"#define {r['name']} (({r['type']} *){base})"
    return f"#define {r['name']} (*({r['type']} *){base})"


def header(rows, dummies):
    out = ["/* Generated by tools/re/gen_client_native.py. Do not edit. */",
           "#ifndef SR_CLIENT_STORAGE_H", "#define SR_CLIENT_STORAGE_H",
           '#include "sr_storage_types.h"',
           "#define sr_voice_channel_type VoiceChannelClient",
           '#include "sr_image_types.h"', '#include "sr_audio_types.h"', '#include "sr_input_types.h"', ""]
    for r in rows:
        if r["root"] is None:
            out.append("extern " + declaration(r) + ";")
    out.append("")
    for r in rows:
        if r["root"] is not None:
            out.append(alias_macro(r))
    out.append("")
    for name, typ, count, size in dummies:
        arr = f"[{count}]" if count > 1 else ""
        out.append(f"extern {typ} sr_unplaced_{name}{arr};")
        out.append(f"#define sr_data_{name} sr_unplaced_{name}")
    names = {r["name"] for r in rows}
    out.append('#include "client_build.h"')
    for field in storage["PACKET_FIELDS"]:
        if "sr_data_" + field not in names:
            out.append(f"#define sr_data_{field} sr_data_packet_workspace.{field}")
    out += ["", "#endif", ""]
    return "\n".join(out)


def start_values(image, r):
    """C text that gives the object its start value, or None for a zero object."""
    if r["name"].startswith(tuple("cl_data_" + p for p in SKIP_START)):
        return None
    raw = image.read(r["rva"], r["bytes"])
    if not any(raw):
        return None
    ptrs = [p for p in sorted(image.relocs) if r["rva"] <= p < r["rva"] + r["bytes"]]
    if ptrs:
        if r["type"] not in STRING_TABLES or len(ptrs) != r["count"]:
            raise SystemExit(f"{r['name']}: pointers that are not a text table")
        lines = []
        for p in ptrs:
            value = struct.unpack_from("<I", raw, p - r["rva"])[0]
            index = (p - r["rva"]) // 4
            place = r["name"] if r["count"] == 1 else f"{r['name']}[{index}]"
            lines.append(f"    {place} = {texts['c_string'](image.text(value), False)};")
        return "\n".join(lines)
    table = ", ".join(f"0x{b:02x}" for b in raw)
    target = r["name"] if r["count"] > 1 else "&" + r["name"]
    return (f"    {{\n        static const unsigned char value[] = {{ {table} }};\n"
            f"        memcpy({target}, value, sizeof value);\n    }}")


def regions(rows):
    """The objects that start with data and no pointers, for the test of the start values."""
    image = Image()
    out = ["/* Generated by tools/re/gen_client_native.py. Do not edit. name, address, rva, bytes. */"]
    for r in rows:
        if r["name"].startswith(tuple("cl_data_" + p for p in SKIP_START)):
            continue
        raw = image.read(r["rva"], r["bytes"])
        if not any(raw) or any(r["rva"] <= p < r["rva"] + r["bytes"] for p in image.relocs):
            continue
        address = r["name"] if r["count"] > 1 else "&" + r["name"]
        out.append(f'{{ "{r["name"]}", (const void *){address}, 0x{r["rva"]:x}u, {r["bytes"]}u }},')
    return "\n".join(out) + "\n"


def source(rows, dummies):
    image = Image()
    out = ["/* Generated by tools/re/gen_client_native.py. Do not edit. */",
           "#ifdef SR_CLIENT_NATIVE", '#include "sr_runtime.h"', "#include <string.h>", "",
           "/* The data of the game. Each object has the size that the clean code expects. */"]
    for r in rows:
        if r["root"] is None:
            out.append(declaration(r) + ";")
            out.append(f'SR_STATIC_ASSERT(sizeof({r["name"]}) == {r["count"]} * {r["bytes"] // r["count"]}, "{r["name"]}");')
    out += ["", "/* An object that lies inside a bigger object must fit in it. */"]
    for r in rows:
        if r["root"] is not None:
            elems = f"{r['count']} * " if r["count"] > 1 else ""
            out.append(f'SR_STATIC_ASSERT(0x{r["offset"]:x} + {elems}sizeof({r["type"]}) <= sizeof({r["root"]["name"]}), "{r["name"]}");')
    out += ["", "/* Objects that the client has no place for. */"]
    for name, typ, count, size in dummies:
        if count * size <= DUMMY_LIMIT:
            arr = f"[{count}]" if count > 1 else ""
            out.append(f"{typ} sr_unplaced_{name}{arr};")
    out += ["int sr_client_end_time;", "int sr_client_gametype;", "",
            "/* Give the objects their start values. Call it once, before the game starts. */",
            "void cl_storage_init(void)", "{"]
    for r in rows:
        text = start_values(image, r)
        if text:
            out.append(text)
    out += ["}", "", "#endif", ""]
    return "\n".join(out)


def main():
    rows, dummies = load()
    files = {HEADER: header(rows, dummies), SOURCE: source(rows, dummies), REGIONS: regions(rows)}
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
