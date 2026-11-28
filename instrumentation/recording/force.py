"""Read and write the rules of the scenario forcer (instrumentation/hook/force.c).

A scenario file holds numbers, because the recorder reads it inside the game. This tool puts the names
of the fields back, so a person can read and check a scenario.

  python -I instrumentation/recording/force.py show <scenarios.txt> [name]
      Print each rule of a scenario (or of the whole file) in words, with the field names of
      analysis/structs/names.csv.
  python -I instrumentation/recording/force.py find <struct.field> [<struct.field> ...]
      Print the RVA and the size to write in a rule. Example: game.timer
  python -I instrumentation/recording/force.py float <number>
      Print the bit pattern of a float, to write a float field.

The tool exits with 1 when a rule has an address that no named field matches.
"""
import csv
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIZE = {'int': 4, 'uint32_t': 4, 'float': 4, 'int16_t': 2, 'char': 1, 'uint8_t': 1}


def load_globals():
    rows = {}
    for path in ('analysis/024d/globals.csv', 'analysis/024d/manual_globals.csv'):
        with open(ROOT / path, encoding='utf-8') as handle:
            for row in csv.DictReader(handle):
                rows[row['name']] = int(row['rva'], 16)
    return rows


def load_fields():
    """names.csv: struct -> offset -> (name, type)."""
    fields = {}
    with open(ROOT / 'analysis/structs/names.csv', encoding='utf-8') as handle:
        for row in csv.DictReader(handle):
            if row['count']:
                continue
            fields.setdefault(row['struct'], {})[int(row['offset'], 16)] = (row['name'], row['type'])
    return fields


GLOBALS = load_globals()
FIELDS = load_fields()


def field_name(rva):
    """Return a readable name for an RVA, or None when no named field has this address."""
    best = None
    for name, base in GLOBALS.items():
        if base <= rva and name in FIELDS and (best is None or base > best[1]):
            best = (name, base)
    if best is None:
        return None
    entry = FIELDS[best[0]].get(rva - best[1])
    return '%s.%s' % (best[0], entry[0]) if entry else None


def find(name):
    struct_name, _, field = name.partition('.')
    if struct_name not in GLOBALS or struct_name not in FIELDS:
        raise SystemExit('no global with fields: %s' % struct_name)
    for offset, (fname, ftype) in FIELDS[struct_name].items():
        if fname == field:
            return GLOBALS[struct_name] + offset, SIZE.get(ftype, 4), ftype
    raise SystemExit('no field %s in %s' % (field, struct_name))


def words(line):
    return line.split('#', 1)[0].split()


def section_lines(path, name):
    """Return (line number, text) of one [name] section. Without a name, return every line of the file."""
    lines = list(enumerate(Path(path).read_text(encoding='utf-8').splitlines(), 1))
    if name is None:
        return lines
    inside, found = False, False
    out = []
    for number, line in lines:
        if line.startswith('['):
            inside = line.strip() == '[%s]' % name
            found = found or inside
        elif inside:
            out.append((number, line))
    if not found:
        raise SystemExit('no section [%s] in %s' % (name, path))
    return out


def show(path, name=None):
    bad = 0
    for number, line in section_lines(path, name):
        w = words(line)
        if not w or w[0].startswith('['):
            continue
        if w[0] == 'ticks':
            print('%3d  tick counter: %s' % (number, field_name(int(w[1], 0)) or w[1]))
            continue
        i = 2
        when = ('at tick %s' if w[0] == 'at' else 'every %s ticks') % w[1]
        if w[i] == 'from':
            when += ' from tick %s' % w[i + 1]
            i += 2
        cond = ''
        while w[i] == 'if':
            rva = int(w[i + 1], 0)
            name = field_name(rva)
            bad += name is None
            cond += ' %s %s == %s' % ('if' if not cond else 'and', name or hex(rva), w[i + 3])
            i += 4
        rva = int(w[i + 1], 0)
        name = field_name(rva)
        bad += name is None
        print('%3d  %s%s: set %s (%s bytes) = %s' % (number, when, cond, name or ('UNKNOWN ' + hex(rva)), w[i + 2], w[i + 3]))
    return 1 if bad else 0


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    command = sys.argv[1]
    if command == 'show':
        return show(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
    if command == 'find':
        for name in sys.argv[2:]:
            rva, size, ftype = find(name)
            print('%s: rva 0x%x, %d bytes, %s' % (name, rva, size, ftype))
        return 0
    if command == 'float':
        print('0x%08x' % struct.unpack('<I', struct.pack('<f', float(sys.argv[2])))[0])
        return 0
    print(__doc__)
    return 2


sys.exit(main())
