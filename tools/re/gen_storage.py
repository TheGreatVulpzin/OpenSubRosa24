"""Generate independent C globals and the separate verification memory adapter.

The normal build uses the generated C definitions. Only the adapter uses addresses.
Run with python -I. The executable is input to generation, never to the server build.
"""
import argparse
import csv
import json
import re
import runpy
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TYPES = runpy.run_path(str(ROOT / 'tools/re/gen_types.py'))


def schema():
    addresses = {}
    for name in ('globals', 'manual_globals'):
        for r in csv.DictReader((ROOT / f'analysis/024d/{name}.csv').open(encoding='utf-8')):
            addresses[r['name']] = int(r['rva'], 16)
    rows = []
    overrides = {
        'area_sector': ('SrSectorStore', 1, 0x40 + 8192 * 0x6068),
        'track': ('TrackPiece', 4096, 0x578),
        'cityobject': ('CityObjectRec', 1024, 0x54),
        'cityobjecttype': ('CityObjectTypeRec', 256, 0x2418),
    }
    for typ, (name, size) in TYPES['STRUCTS'].items():
        if name in ('objectpacket', 'itemtype'):
            continue
        typ, count, size = overrides.get(name, (typ, TYPES['COUNTS'].get(name, 1), size))
        rows.append((addresses[name], name, typ, count, size))
    extra = [
        (0xf437b60, 'accounts', 'SrAccountStore', 1, 8 + 65536 * 0x4c),
        (0x9c1d8c0, 'landscape', 'SrLandscape', 1, 12 + 2049 * 2049 * 4),
        (0xb7e90a0, 'surface', 'SrSurface', 1, 0x140),
        (0x1d65a40, 'surface2', 'SrSurface', 1, 0x140),
        (0xb7e91e0, 'interior', 'SrInteriorStore', 1, 4 + 4096 * 0x2a30),
        (0x2043a0, 'floorplan', 'FloorplanCell', 262144, 0x48),
        (0x9bdd8a0, 'newspaper', 'Newspaper', 1, 0x40008),
        (0x16d50020, 'materials', 'SrMaterialStore', 1, 4 + 1024 * 0x58),
        (0x1b497a0, 'clientaccounts', 'SrClientAccountStore', 1, 4 + 65536 * 16),
        (0xf437320, 'bullettypes', 'BulletType', 4, 0x18),
        (0xeca4bc0, 'voice', 'VoiceChannel', 8, 0x29228),
        (0xf437940, 'camera', 'float', 136, 4),
        (0x1c497c0, 'pointtemp', 'vec3', 512, 12),
        (0x17529aa0, 'modellist', 'int', 0x7a4 // 4, 4),
        (0x13e22e80, 'texturelist', 'int', 0x1144 // 4, 4),
        (0xb0661c0, 'bezier_curve_dv', 'float', 12, 4),
        (0x761ea20, 'bezier_curve', 'float', 12, 4),
        (0x13c22d40, 'bezier_curve_uv', 'float', 8, 4),
        (0x12a80c44, 'level_marks', 'uint8_t', 4096, 1),
        (0x61780, 'manager_msg', 'char', 64, 1),
        (0x617c0, 'world_msg', 'char', 128, 1),
        (0x203ee0, 'world_digits', 'char', 64, 1),
        (0x203f20, 'newspaper_money', 'char', 128, 1),
        (0x203fa0, 'farbs_line', 'char', 128, 1),
        (0x61d48, 'farbs_digits', 'char', 128, 1),
        (0x60d40, 'newspaper_line', 'char', 128, 1),
        (0x61848, 'message', 'char', 64, 1),
        (0x5efec, 'filtered_word', 'char', 7, 1),
        (0x203de0, 'guard_used', 'int', 64, 4),
        (0x103485a0, 'packet_workspace', 'SrPacketWorkspace', 1, 0x28664),
        (0x91022a4, 'file_read', 'SrFileTransfer', 1, 4),
        (0x13bf2d00, 'file_write', 'SrFileTransfer', 1, 4),
        (0xac218e0, 'wsadata', 'uint8_t', 0x190, 1),
    ]
    scalars = {
        0x1ca5a30: 'numofdynamicbonds', 0x13c22d24: 'numofstaticbonds',
        0x90dc520: 'numoftracks', 0xf437318: 'numofevents',
        0x17179a28: 'numofbonds', 0x17529a40: 'numofbullets',
        0x1c4afc0: 'numofconnections',
        0xff47be0: 'view',
        0x916d2c0: 'random_counter', 0x204380: 'dedicated',
        0x10348524: 'slogfp', 0x61840: 'tzset_done',
        0x761e600: 'numofsprites', 0x12a81c44: 'numofsprite_models',
        0xb7e8ff4: 'tiremarkstart', 0xb7a8c80: 'tiremarkend',
        0xb7a8c84: 'startup_counter',
        0x761eb00: 'sprite_points', 0x766eb04: 'sprite_lines',
        0x76ceb08: 'sprite_quads', 0x7eceb0c: 'sprite_triangles',
    }
    for addr, name in scalars.items():
        typ = 'void *' if name == 'slogfp' else 'uint32_t' if name in ('numofevents', 'random_counter') else 'int'
        extra.append((addr, name, typ, 1, 4))
    existing = {r[1] for r in extra}
    for r in csv.DictReader((ROOT / 'analysis/024d/data_globals.csv').open(encoding='utf-8')):
        if r['name'] in existing:
            continue
        typ, count = r['type'], int(r['count'] or 1)
        if typ == 'FloorplanCell' or typ == 'Newspaper':
            continue
        size = {'char': 1, 'uint8_t': 1, 'float': 4, 'int': 4, 'uint32_t': 4, 'char*': 4, 'void*': 4}[typ]
        typ = {'char*': 'const char *', 'void*': 'void *'}.get(typ, typ)
        if r['name'] in ('xaxis', 'yaxis', 'zaxis'):
            typ, count, size = 'vec3', 1, 12
        if r['name'] in ('cubevertex', 'cubenormal', 'cubetangent', 'cubebinormal'):
            typ, count, size = 'vec3', count // 3, 12
        extra.append((int(r['rva'], 16), r['name'], typ, count, size))
    rows.extend(extra)
    rows.sort()
    for a, b in zip(rows, rows[1:]):
        if a[0] + a[3] * a[4] > b[0]:
            raise ValueError(f'Overlapping storage: {a} / {b}')
    return rows


def image():
    data = (ROOT / 'original/blackhouser/subrosadedicated024d.exe').read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    n = struct.unpack_from('<H', data, pe + 6)[0]
    opt = struct.unpack_from('<H', data, pe + 20)[0]
    sections = []
    for i in range(n):
        off = pe + 24 + opt + i * 40
        size, addr, rawsize, rawoff = struct.unpack_from('<IIII', data, off + 8)
        sections.append((addr, size, data[rawoff:rawoff + rawsize]))

    def read(addr, size):
        for base, extent, raw in sections:
            if base <= addr and addr + size <= base + extent:
                return (raw[addr - base:addr - base + size] + bytes(size))[:size]
        raise ValueError(hex(addr))
    return read


def declaration(row):
    _, name, typ, count, _ = row
    return f'{typ} sr_data_{name}' + (f'[{count}]' if count > 1 else '')


def initializer(row, read):
    addr, name, typ, count, size = row
    # Only initialized data can supply defaults. Large pools start as C zero storage.
    if addr >= 0x60e00:
        return ''
    raw = read(addr, size * count)
    if not any(raw):
        return ''
    if typ == 'const char *':
        values = []
        for ptr, in struct.iter_unpack('<I', raw):
            text = bytearray()
            while (b := read(ptr - 0x400000 + len(text), 1)[0]) != 0:
                text.append(b)
            values.append(json.dumps(text.decode('ascii')))
    elif typ in ('float', 'vec3'):
        values = []
        for val, in struct.iter_unpack('<f', raw):
            values.append(repr(val) + 'f')
        if typ == 'vec3':
            values = ['{' + ', '.join(values[i:i + 3]) + '}' for i in range(0, len(values), 3)]
    elif typ in ('char', 'uint8_t'):
        values = [str(b) for b in raw]
    elif typ in ('int', 'uint32_t'):
        fmt = '<i' if typ == 'int' else '<I'
        values = [str(x[0]) + ('u' if typ == 'uint32_t' else '') for x in struct.iter_unpack(fmt, raw)]
    else:
        raise ValueError(f'Unsupported default: {row}')
    if count == 1:
        return ' = ' + values[0]
    return ' = {\n    ' + ',\n    '.join(', '.join(values[i:i + 8]) for i in range(0, len(values), 8)) + '\n}'


def generate(check=False):
    rows = schema()
    read = image()
    header = ['/* Generated by tools/re/gen_storage.py. Do not edit. */',
              '#ifndef SR_STORAGE_H', '#define SR_STORAGE_H', '#include "sr_storage_types.h"', '']
    source = ['/* Generated by tools/re/gen_storage.py. Do not edit. */', '#include "sr_storage.h"', '']
    adapter = ['/* Generated by tools/re/gen_storage.py. Do not edit. */',
               '#ifndef SR_STORAGE_ADAPTER_H', '#define SR_STORAGE_ADAPTER_H',
               '#include "sr_storage_types.h"', '#include "sr_globals_024d.h"',
               'extern uintptr_t sr_base;', '#define SR_AT(type, rva) ((type *)(sr_base + (rva)))', '']
    manifest = ['/* Generated by tools/re/gen_storage.py. Do not edit. */',
                '/* Define SR_STORAGE_REGION before including this file. */']
    for row in rows:
        addr, name, typ, count, size = row
        header.append('extern ' + declaration(row) + ';')
        source.append(declaration(row) + initializer(row, read) + ';')
        expr = f'(({typ} *)(sr_base + 0x{addr:x}u))'
        adapter.append(f'#define sr_data_{name} ' + (expr if count > 1 else f'(*{expr})'))
        ptr = f'&sr_data_{name}[0]' if count > 1 else f'&sr_data_{name}'
        manifest.append(f'SR_STORAGE_REGION(0x{addr:x}u, {ptr}, {count * size}, '
                        f'{int(typ == "const char *")}, "{name}")')
        header.append(f'SR_STATIC_ASSERT(sizeof(sr_data_{name}) == {count * size}, "storage {name}");')
    aliases = [f'#define sr_data_{name} sr_data_packet_workspace.{name}'
               for name in ('kickedip', 'objectpacket', 'itemtype', 'numofkicked')]
    header.extend(['', *aliases, '', '#endif'])
    adapter.extend(['', *aliases, '', '#endif'])
    outputs = {
        'src/include/sr_storage.h': header,
        'src/server/storage.c': source,
        'instrumentation/runtime/storage_adapter.h': adapter,
        'instrumentation/runtime/storage_regions.inc': manifest,
    }
    for path, lines in outputs.items():
        p = ROOT / path
        content = '\n'.join(lines) + '\n'
        if check:
            if not p.exists() or p.read_text(encoding='utf-8') != content:
                raise SystemExit(f'Stale generated storage: {path}')
        elif not p.exists() or p.read_text(encoding='utf-8') != content:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(content, encoding='utf-8', newline='\n')
    print(f'{"Checked" if check else "Generated"} {len(rows)} independent storage objects, '
          f'{sum(r[3] * r[4] for r in rows)} bytes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true', help='Fail if a generated file is stale')
    generate(parser.parse_args().check)
