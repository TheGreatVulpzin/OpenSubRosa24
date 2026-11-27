"""Check the type headers against each other and against the record sizes.

1. Each record of the frozen header of the lifted C (src/lifted/sr_types_lifted.h) has the same size
   in src/include/sr_types.h.
2. Each record of tools/re/record_tables.py has its size in src/include/sr_types.h, and the capacity
   of each array in tools/re/record_tables.py is the SR_MAX_ value of src/include/sr_limits.h.

The headers are plain source. Run this tool after a change to a record.

Usage: python -I tools/re/check_types.py
"""
import re
import runpy
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TABLES = runpy.run_path(str(ROOT / 'tools/re/record_tables.py'))

FROZEN_RECORD = re.compile(r'\}; uint8_t lf_raw\[(0x[0-9a-f]+)\]; \} (\w+);')
STATIC_SIZE = re.compile(r'SR_STATIC_ASSERT\(sizeof\((\w+)\) == (0x[0-9a-f]+)')
LIMIT = re.compile(r'^#define SR_MAX_(\w+?)\s+(\d+)\s*$', re.M)


def main():
    errors = []
    header = (ROOT / 'src/include/sr_types.h').read_text(encoding='utf-8')
    new = {name: int(size, 16) for size, name in FROZEN_RECORD.findall(header)}
    frozen = (ROOT / 'src/lifted/sr_types_lifted.h').read_text(encoding='utf-8')
    old = {name: int(size, 16) for size, name in FROZEN_RECORD.findall(frozen)}
    for name in sorted(old):
        if name in new and old[name] != new[name]:
            errors.append(f'{name}: 0x{old[name]:x} in sr_types_lifted.h, 0x{new[name]:x} in sr_types.h')
        elif name not in new:
            errors.append(f'{name}: in sr_types_lifted.h, not in sr_types.h')
    sizes = {name: int(size, 16) for name, size in STATIC_SIZE.findall(header)}
    for cname, (_glob, size) in TABLES['STRUCTS'].items():
        if sizes.get(cname) != size and new.get(cname) != size:
            errors.append(f'{cname}: record_tables.py says 0x{size:x}, sr_types.h says '
                          f'{hex(sizes.get(cname, new.get(cname, 0)))}')
    limits = {name.lower(): int(value) for name, value in
              LIMIT.findall((ROOT / 'src/include/sr_limits.h').read_text(encoding='utf-8'))}
    for name, count in TABLES['COUNTS'].items():
        if limits.get(name) != count:
            errors.append(f'{name}: record_tables.py says {count}, sr_limits.h says {limits.get(name)}')
    if errors:
        print('\n'.join(errors))
        return 1
    print(f'types ok: {len(old)} frozen records, {len(TABLES["STRUCTS"])} server records, {len(limits)} limits')
    return 0


sys.exit(main())
