"""Map the data addresses of one lifted client function to storage objects and fields.

Usage: python -I tools/re/client_resolve.py <rva of the function, for example 0x75f10> [lifted C file]

A function that the client table already adopts is no longer in src/lifted/client/lifted_client.c.
For such a function, save an older copy of the file (git show <commit>:src/lifted/client/lifted_client.c)
and give its path as the second argument.

The lifted C names a global DAT_xxxxxxxx. The high 16 bits of the address carry an offset
of 0x40 that the RVA does not have, so this tool subtracts it. For each address it prints the
storage object that holds it (analysis/client_win/storage.csv and group_globals_C*.csv), the
index in the object, the offset in the record and the field name from src/include/sr_types.h.
"""
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def load_objects():
    """Return (name, type, count, size, rva) for each placed object."""
    objects = []
    for row in csv.DictReader((ROOT / "analysis/client_win/storage.csv").open(encoding="utf-8")):
        objects.append((row["name"], row["type"], int(row["count"]), int(row["size"]), int(row["client_rva"], 16)))
    for path in sorted((ROOT / "analysis/client_win").glob("group_globals_C*.csv")):
        for row in csv.DictReader(path.open(encoding="utf-8")):
            objects.append((row["name"], row["type"], int(row["count"] or 1), 0, int(row["client_rva"], 16)))
    return objects


def load_types():
    """Return the fields of each struct (offset, name, type, evidence) and the struct sizes."""
    header = (ROOT / "src/include/sr_types.h").read_text(encoding="utf-8").split("\n")
    fields = {}
    current = None
    for line in header:
        start = re.match(r"typedef (?:union|struct) (\w+) \{", line)
        if start:
            current = start.group(1)
            fields[current] = []
            continue
        if current:
            field = re.match(r"\s+([\w\s\*\[\]]+?)\s+(\w+)(\[[^\]]*\])?;\s*/\*\s*\+0x([0-9a-f]+)\s*(.*?)\s*\*/", line)
            if field:
                fields[current].append((int(field.group(4), 16), field.group(2), field.group(1).strip(), field.group(5)[:70]))
            if line.startswith("}") and ("lf_raw" in line or re.match(r"\} (\w+);", line)):
                current = None
    sizes = {}
    for line in header:
        size = re.match(r"SR_STATIC_ASSERT\(sizeof\((\w+)\) == (0x[0-9a-f]+)", line)
        if size:
            sizes[size.group(1)] = int(size.group(2), 16)
    return fields, sizes


def va_to_rva(va):
    if va >= 0x1000000:
        return (((va >> 16) - 0x40) << 16) | (va & 0xFFFF)
    return va - 0x400000


def main():
    want = int(sys.argv[1], 16)
    source = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "src/lifted/client/lifted_client.c"
    lifted = source.read_text(encoding="utf-8", errors="replace").split("\n")
    start = next((i for i, line in enumerate(lifted) if re.match(r"/\* -+ 0x%x \*/" % want, line)), None)
    if start is None:
        sys.exit(f"function 0x{want:x} is not in {source}. Give an older copy of the file as the second argument.")
    end = next(i for i in range(start + 1, len(lifted)) if lifted[i].startswith("/* ----"))
    body = "\n".join(lifted[start:end])
    addresses = sorted(set(int(m, 16) for m in re.findall(r"\bDAT_([0-9a-f]{8})\b", body)))
    objects = load_objects()
    fields, sizes = load_types()
    for va in addresses:
        rva = va_to_rva(va)
        hit = None
        for name, typ, count, size, base in objects:
            size = size or sizes.get(typ, 4)
            if base <= rva < base + max(count, 1) * size:
                index, inner = divmod(rva - base, size)
                hit = (name, typ, index, inner)
                break
        if not hit:
            print(f"{va:#010x} rva {rva:#x}: no object")
            continue
        name, typ, index, inner = hit
        field = ""
        for offset, field_name, field_type, evidence in fields.get(typ, []):
            if offset == inner:
                field = f"{field_name} ({field_type}) {evidence}"
        print(f"{va:#010x} rva {rva:#x}: {name}[{index}]+{inner:#x} of {typ}: {field}")


if __name__ == "__main__":
    main()
