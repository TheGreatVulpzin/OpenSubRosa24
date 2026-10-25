"""Test city object allocation, type tables and newspaper boundaries.

Usage: python -I instrumentation/lift/synth_city_objects.py --run <recording> <work>
"""
import argparse
import copy
import re
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_bonds import Snapshot
from synth_area import run_files

CITY = 0xe299220
OBJECTS = 0x15e24020
TYPES = 0xf02e500
POINT = 0x13d22d60 + 8 + 0x80000


def cases():
    result = {}

    def allocate(full):
        def build(s):
            for i in range(1024):
                s.write(OBJECTS + i * 0x54, 'i', 1 if full or i != 7 else 0)
            s.write(POINT, 'fff', 1.25, -2.5, 3.75)
            return ['R 14600 V:2 M:%x V:3eaaaaab' % POINT], {}
        return build

    result['allocate_free'] = ('createcityobject', allocate(False))
    result['allocate_full'] = ('createcityobject', allocate(True))

    def elevator(kind):
        def build(s):
            s.write(CITY + 0x2f998, 'i', 2)
            for i in range(2):
                b = CITY + i * 0x9dc
                s.write(b + 0x2f9f8, 'fff', 0.0, 0.0, 0.0)
                s.write(b + 0x2fa04, 'fff', 8.0, 8.0, 8.0)
                s.write(b + 0x2fa10, 'i', 19 if kind == 'existing' else -1)
                s.write(b + 0x2fa14, 'iii', 2, 3, 4)
                s.write(b + 0x2fa20, 'i', 5)
            for i in range(1024):
                s.write(OBJECTS + i * 0x54, 'i', 1 if kind == 'full' else 0)
            pos = (2.0, 3.0, 4.0)
            if kind == 'outside':
                pos = (8.0, 3.0, 4.0)
            if kind == 'nan':
                pos = (float('nan'), 3.0, 4.0)
            s.write(POINT, 'fff', *pos)
            return ['R 154b0 M:%x' % POINT], {}
        return build

    for kind in ('new', 'existing', 'outside', 'nan', 'full'):
        result['elevator_' + kind] = ('city_createelevator', elevator(kind))

    def box(s):
        s.write(TYPES + 7 * 0x2418 + 4, 'i', 3)
        s.write(TYPES + 7 * 0x2418 + 0xc08, 'i', 2)
        return ['R 4bb0 V:7 V:3eaaaaab V:bf123457 V:3f765431 V:3f123457 V:3e765431 V:3e234567'], {}

    result['box_fractional'] = ('cityobject_addbox', box)

    def types(s):
        for i in (1, 2, 4, 5, 8, 9):
            s.write(TYPES + i * 0x2418 + 4, 'i', 4)
            s.write(TYPES + i * 0x2418 + 0xc08, 'i', 1)
        return ['R 155e0'], {}

    result['type_append'] = ('setupcityobjecttypes', types)
    result['building_types'] = ('setupbuildingtypes', lambda s: (['R 13450'], {}))

    def newspaper(lines, accounts):
        def build(s):
            s.write(0xf437b64, 'i', accounts)
            for i in range(accounts):
                s.write(0xf437b68 + i * 0x4c + 8, '32s', ('User%d' % i).encode())
                s.write(0xf437b68 + i * 0x4c + 0x2c, 'i', i * 12345678 - 1)
            return ['R 185a0'], {'readme.txt': b'News\r\n' * lines}
        return build

    result['newspaper_empty'] = ('newspaper_load', newspaper(0, 0))
    result['newspaper_ranking'] = ('newspaper_load', newspaper(2, 12))
    result['newspaper_overflow'] = ('newspaper_load', newspaper(1022, 10))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true', required=True)
    parser.add_argument('--no-cover', action='store_true', help=argparse.SUPPRESS)
    parser.add_argument('recording', type=Path)
    parser.add_argument('work', type=Path)
    args = parser.parse_args()
    base = Snapshot(args.recording / 'sv_snapshot.bin')
    bad = 0
    for name, (function, build) in cases().items():
        snapshot = copy.deepcopy(base)
        lines, files = build(snapshot)
        folder = args.work / name
        folder.mkdir(parents=True, exist_ok=True)
        snapshot.save(folder / 'sv_snapshot.bin')
        (folder / 'sv_args.txt').write_text('\n'.join(lines) + '\n')
        summary, file_ok = run_files(folder, function, files)
        ok = file_ok and summary.startswith('1 functions tested: %d calls same' % len(lines))
        ok = ok and not re.search(r'\b[1-9][0-9]* (?:different|known differences|(?:\w+ )?crashed|skipped)\b', summary)
        bad += not ok
        print('%s %s: %s%s' % (name, function, summary, '' if ok else ' CHECK'), flush=True)
        (args.work / (name + '.log')).write_text(summary + '\n')
        if ok:
            shutil.rmtree(folder)
    print('%d case(s) to check' % bad)
    return bool(bad)


if __name__ == '__main__':
    raise SystemExit(main())
