"""Make the guard data of the native client: build/guards/guard_data.h and guard_data.c.

The normal client holds its data as plain objects (src/client/data/*.c). The guard build puts a
256 byte guard after each object. A write past the end of an object changes its guard, and the
smoke test prints the name of the object. This tool reads the plain definitions and writes the
holder of each object, so nothing in src/ has guard code.

Usage: python -I instrumentation/client/gen_guards.py
The guard target of CMakeLists.txt needs the two files.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "src/client/data"
OUT = ROOT / "build/guards"
GUARD = 256

STATEMENT = re.compile(
    r"^(?P<type>[A-Za-z_][\w \*]*?)\s*(?P<name>(?:sr|cl)_data_\w+)(?P<dims>(?:\[[^\]]+\])*)\s*(?:=\s*(?P<init>.+?))?\s*;$", re.S)


def statements(text):
    """Yield the top level statements of a data file, without comments and preprocessor lines."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    current = []
    depth = 0
    for line in text.splitlines():
        stripped = line.strip()
        if not current and (not stripped or stripped.startswith("#")):
            continue
        current.append(line)
        depth += line.count("{") - line.count("}")
        if depth == 0 and stripped.endswith(";"):
            yield "\n".join(current).strip()
            current = []
    if current:
        raise SystemExit("data file ends inside a statement: " + current[0])


def objects():
    found = []
    for path in sorted(DATA.glob("*.c")):
        if path.name == "unplaced.c":
            continue
        for text in statements(path.read_text(encoding="utf-8")):
            match = STATEMENT.match(text)
            if not match:
                raise SystemExit("%s: cannot read this definition:\n%s" % (path.name, text))
            found.append(match.groupdict())
    return found


def main():
    objs = objects()
    OUT.mkdir(parents=True, exist_ok=True)
    header = ["/* Made by instrumentation/client/gen_guards.py. The guard build only. */"]
    source = ["/* Made by instrumentation/client/gen_guards.py. The guard build only. */",
              '#include "sr_client_storage.h"', "#include <string.h>", "",
              "#define CL_GUARD_SIZE %d" % GUARD, ""]
    table = []
    for o in objs:
        hold = "cl_hold_" + o["name"]
        typ = o["type"].strip()
        header.append("struct %s { %s obj%s; unsigned char guard[%d]; };" % (hold, typ, o["dims"], GUARD))
        header.append("extern struct %s %s;" % (hold, hold))
        header.append("#define %s (%s.obj)" % (o["name"], hold))
        init = o["init"]
        source.append("struct %s %s%s;" % (hold, hold, (" = { " + init + " }") if init else ""))
        table.append('    { "%s", %s.guard },' % (o["name"], hold))
    source += ["", "/* The guard after each object. A change in a guard means a write past the end of its object. */",
               "static const struct { const char *name; unsigned char *guard; } cl_guards[] = {"] + table + ["};", ""]
    source += [
        "/* Return the name of an object whose guard changed, and mend the guard. Return 0 when all are whole. */",
        "const char *cl_guards_scan(unsigned *first, unsigned *last)",
        "{",
        "    unsigned i, k;",
        "",
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
        "    return 0;",
        "}",
        "",
        "/* Fill the guards. Call it once, before the game starts. */",
        "void cl_storage_init(void)",
        "{",
        "    unsigned i;",
        "",
        "    for (i = 0; i < sizeof cl_guards / sizeof cl_guards[0]; i++)",
        "        memset(cl_guards[i].guard, 0xa5, CL_GUARD_SIZE);",
        "}",
        ""]
    (OUT / "guard_data.h").write_text("\n".join(header) + "\n", encoding="utf-8", newline="\n")
    (OUT / "guard_data.c").write_text("\n".join(source), encoding="utf-8", newline="\n")
    print("%d guarded objects -> %s" % (len(objs), OUT))


if __name__ == "__main__":
    sys.exit(main())
