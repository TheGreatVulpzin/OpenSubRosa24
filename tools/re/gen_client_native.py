"""Generate the native storage of the client: every datum as a real C object.

The test build reads each datum at sr_base plus an RVA (instrumentation/client/runtime/
storage_adapter.h, from tools/re/gen_client_storage.py). The native build has no image.
This tool writes the other form of the same names:

  src/include/sr_client_storage.h  extern declarations, the alias macros, the hook header
  src/client/storage.c             one definition per object and the start values
  instrumentation/client/runtime/storage_regions.inc  the objects with start values, for the test

A name in analysis/client_win/storage_aliases.csv is an object that has no storage of its own.
It becomes a macro for the C expression of its "expr" column (a field of a bigger object). The
start values come from the .data of the client executable. A text
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


def declaration(r):
    arr = f"[{r['count']}]" if r["count"] > 1 else ""
    return f"{r['type']} {r['name']}{arr}"


GUARD_SIZE = 256


def hold_name(r):
    return "cl_hold_" + re.sub(r"^(sr|cl)_data_", "", r["name"])


def holder(r):
    """A struct with the object and a guard after it. A write past the object changes the guard."""
    arr = f"[{r['count']}]" if r["count"] > 1 else ""
    return f"struct {hold_name(r)} {{ {r['type']} obj{arr}; unsigned char guard[{GUARD_SIZE}]; }};"


def alias_macro(r):
    return f"#define {r['name']} ({r['expr']})"


def header(rows, dummies):
    out = ["/* Generated by tools/re/gen_client_native.py. Do not edit. */",
           "#ifndef SR_CLIENT_STORAGE_H", "#define SR_CLIENT_STORAGE_H",
           '#include "sr_storage_types.h"',
           "#define sr_voice_channel_type VoiceChannelClient",
           '#include "sr_image_types.h"', '#include "sr_audio_types.h"', '#include "sr_input_types.h"',
           '#include "gl_slots.h"', ""]
    for r in rows:
        if r["glee"]:
            if r["glee"].startswith("_GLEE_"):
                out.append(f'extern uint8_t {r["glee"]};')
            out.append(f'#define {r["name"]} {r["glee"]}')
        elif r["root"] is None:
            out.append("#ifdef SR_STORAGE_GUARDS")
            out.append(holder(r))
            out.append(f"extern struct {hold_name(r)} {hold_name(r)};")
            out.append(f"#define {r['name']} ({hold_name(r)}.obj)")
            out.append("#else")
            out.append("extern " + declaration(r) + ";")
            out.append("#endif")
    out.append("")
    for r in rows:
        if r["root"] is not None and not r["glee"]:
            out.append(alias_macro(r))
    out.append("")
    for name, typ, count, size in dummies:
        arr = f"[{count}]" if count > 1 else ""
        out.append(f"extern {typ} sr_unplaced_{name}{arr};")
        out.append(f"#define sr_data_{name} sr_unplaced_{name}")
    names = {r["name"] for r in rows}
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
        if r["glee"] or r["name"].startswith(tuple("cl_data_" + p for p in SKIP_START)):
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
        if r["root"] is None and not r["glee"]:
            out.append("#ifdef SR_STORAGE_GUARDS")
            out.append(f"struct {hold_name(r)} {hold_name(r)};")
            out.append("#else")
            out.append(declaration(r) + ";")
            out.append("#endif")
            out.append(f'SR_STATIC_ASSERT(sizeof({r["name"]}) == {r["count"]} * {r["bytes"] // r["count"]}, "{r["name"]}");')
    out += ["", "/* Objects that the client has no place for. */"]
    for name, typ, count, size in dummies:
        if count * size <= DUMMY_LIMIT:
            arr = f"[{count}]" if count > 1 else ""
            out.append(f"{typ} sr_unplaced_{name}{arr};")
    out += ["int sr_client_end_time;", "int sr_client_gametype;", ""]
    out += ["#ifdef SR_STORAGE_GUARDS", "#define CL_GUARD_SIZE %d" % GUARD_SIZE,
            "/* The guard after each object. A change in a guard means a write past the end of its object. */",
            "static const struct { const char *name; unsigned char *guard; } cl_guards[] = {"]
    for r in rows:
        if r["root"] is None and not r["glee"]:
            out.append(f'    {{ "{r["name"]}", {hold_name(r)}.guard }},')
    out += ["};", "",
            "/* Return the name of an object whose guard changed, and mend the guard. Return 0 when all are whole. */",
            "const char *cl_guards_scan(unsigned *first, unsigned *last)", "{",
            "    unsigned i, k;", "",
            "    for (i = 0; i < sizeof cl_guards / sizeof cl_guards[0]; i++)",
            "        for (k = 0; k < CL_GUARD_SIZE; k++)",
            "            if (cl_guards[i].guard[k] != 0xa5) {",
            "                unsigned end = CL_GUARD_SIZE;",
            "",
            "                while (end > k && cl_guards[i].guard[end - 1] == 0xa5)",
            "                    end--;",
            "                *first = k;",
            "                *last = end - 1;",
            "                memset(cl_guards[i].guard, 0xa5, CL_GUARD_SIZE);",
            "                return cl_guards[i].name;",
            "            }",
            "            ",
            "    return 0;", "}", "#endif", ""]
    out += ["/* Give the objects their start values. Call it once, before the game starts. */",
            "void cl_storage_init(void)", "{",
            "#ifdef SR_STORAGE_GUARDS",
            "    unsigned i;", "",
            "    for (i = 0; i < sizeof cl_guards / sizeof cl_guards[0]; i++)",
            "        memset(cl_guards[i].guard, 0xa5, CL_GUARD_SIZE);",
            "#endif"]
    for r in rows:
        if r["glee"]:
            continue
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
