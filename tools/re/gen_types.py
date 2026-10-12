"""Gera src/include/sr_types.h e src/include/sr_globals_024d.h.

Entradas:
  analysis/structs/names.csv     campos com nome (revisados à mão, com evidência)
  analysis/structs/mac/*.csv     offsets acessados (tools/re/struct_fields.py)
  analysis/024d/globals.csv      endereços (RVA) das globais no 024d

Campos sem nome viram unk_XXXX. Buracos viram pad_XXXX. O header usa
pack(1) e static_assert em cada campo com nome, então qualquer erro de
offset quebra a compilação.

Uso: python -I tools/re/gen_types.py
"""
import csv
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Tamanhos dos registros (iguais no Mach-O i386 e no PE do 024d).
STRUCTS = {
    'Player': ('player', 0x2408),
    'Human': ('human', 0x2b64),
    'Item': ('item', 0x1ac),
    'Object': ('object', 0x8230),
    'Connection': ('connection', 0xf170),
}
TYPE_SIZE = {'int': 4, 'uint32_t': 4, 'float': 4, 'char': 1, 'int16_t': 2, 'vec3': 12,
             'Bone': 0xe0, 'InventorySlot': 0x24}
CSV_TYPE = {'float': 'float', 'char': 'char', 'short': 'int16_t', 'undefined1': 'char',
            'byte': 'char', 'undefined2': 'int16_t', 'ushort': 'int16_t'}

HEADER = '''/* Gerado por tools/re/gen_types.py. Não edite à mão:
 * edite analysis/structs/names.csv e rode o gerador de novo.
 *
 * Layout de 32 bits do Sub Rosa 0.24 (igual no Mach-O i386 e no PE do 024d).
 * Campos unk_XXXX: o jogo acessa o offset, mas o significado ainda não é conhecido.
 */
#ifndef SR_TYPES_H
#define SR_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define SR_STATIC_ASSERT static_assert
#else
#define SR_STATIC_ASSERT _Static_assert
#endif

#pragma pack(push, 1)

typedef struct vec3 { float x, y, z; } vec3;

/* Osso do corpo (ragdoll). 15 por Human. Campos ainda não mapeados. */
typedef struct Bone { uint8_t raw[0xe0]; } Bone;

/* Posição do inventário. 8 por Human. */
typedef struct InventorySlot {
    int count;      /* +0x00 manager: quantidade */
    int item_id;    /* +0x04 manager: índice em items[] */
    uint8_t unk_08[0x1c];
} InventorySlot;

/* Conta salva em user/server.srk (docs/formatos.md). */
typedef struct Account {
    uint32_t id[2];     /* +0x00 igual a Player.account_id */
    char name[32];      /* +0x08 */
    int name_locked;    /* +0x28 não salvo no arquivo; createaccount grava 5 */
    int money;          /* +0x2c conta nova: 1000 */
    int unk_30;         /* +0x30 */
    int look_a;         /* +0x34 rand() % 3 */
    int look_b;         /* +0x38 rand() & 7 */
    int look_c;         /* +0x3c rand() & 3 */
    uint8_t unk_40[0xc];
} Account;

SR_STATIC_ASSERT(sizeof(vec3) == 12, "vec3");
SR_STATIC_ASSERT(sizeof(Bone) == 0xe0, "Bone");
SR_STATIC_ASSERT(sizeof(InventorySlot) == 0x24, "InventorySlot");
SR_STATIC_ASSERT(sizeof(Account) == 0x4c, "Account");
SR_STATIC_ASSERT(offsetof(Account, money) == 0x2c, "Account.money");
'''


def load_names():
    names = defaultdict(dict)
    with open(ROOT / 'analysis/structs/names.csv', encoding='utf-8') as fh:
        for r in csv.DictReader(fh):
            off = int(r['offset'], 16)
            count = int(r['count']) if r['count'] else 0
            names[r['struct']][off] = (r['name'], r['type'], count, r['evidence'])
    return names


def load_accessed(glob):
    p = ROOT / f'analysis/structs/mac/{glob}.csv'
    if not p.exists():
        return {}
    out = {}
    with open(p, encoding='utf-8') as fh:
        for r in csv.DictReader(fh):
            types = [t.split(':')[0] for t in r['types'].split()]
            typ = next((CSV_TYPE[t] for t in types if t in CSV_TYPE), 'int')
            out[int(r['offset'], 16)] = (typ, int(r['n_funcs']))
    return out


def gen_struct(cname, glob, size, named, accessed):
    fields = []  # (offset, size, decl, comment, asserted)
    for off, (name, typ, count, ev) in named.items():
        n = max(count, 1)
        decl = f'{typ} {name}' + (f'[{count}]' if count else '')
        fields.append((off, TYPE_SIZE[typ] * n, decl, ev, name))
    covered = [(o, o + s) for o, s, *_ in fields]
    for off, (typ, nf) in accessed.items():
        sz = TYPE_SIZE[typ]
        if off % min(sz, 4) or any(a <= off < b for a, b in covered) or off + sz > size:
            continue
        fields.append((off, sz, f'{typ} unk_{off:04x}', f'{nf} funções', None))
        covered.append((off, off + sz))
    fields.sort()

    lines = [f'/* {glob}[]: registro de 0x{size:x} bytes. */', f'typedef struct {cname} {{']
    pos = 0
    for off, sz, decl, comment, _ in fields:
        if off < pos:
            continue  # sobreposição: mantém o primeiro (os nomeados vêm antes no mesmo offset)
        if off > pos:
            lines.append(f'    uint8_t pad_{pos:04x}[0x{off - pos:x}];')
        lines.append(f'    {decl};'.ljust(40) + f' /* +0x{off:04x} {comment} */')
        pos = off + sz
    if pos < size:
        lines.append(f'    uint8_t pad_{pos:04x}[0x{size - pos:x}];')
    lines.append(f'}} {cname};')
    lines.append(f'SR_STATIC_ASSERT(sizeof({cname}) == 0x{size:x}, "{cname}");')
    for off, _sz, _decl, _c, name in fields:
        if name:
            lines.append(f'SR_STATIC_ASSERT(offsetof({cname}, {name}) == 0x{off:x}, "{cname}.{name}");')
    return '\n'.join(lines)


def gen_globals():
    rows = []
    with open(ROOT / 'analysis/024d/globals.csv', encoding='utf-8') as fh:
        rows = list(csv.DictReader(fh))
    types = {g: c for c, (g, _s) in STRUCTS.items()}
    lines = ['/* Gerado por tools/re/gen_types.py a partir de analysis/024d/globals.csv.',
             ' * RVAs das variáveis globais no subrosadedicated024d.exe',
             ' * (SHA-256 11e006786d945d5731cb664fba718a094d8f02648b13fcc20c26c8614ea603fd).',
             ' * Endereço real = base do módulo + RVA.',
             ' */',
             '#ifndef SR_GLOBALS_024D_H', '#define SR_GLOBALS_024D_H', '']
    for r in sorted(rows, key=lambda r: int(r['rva'], 16)):
        t = types.get(r['name'])
        note = f'  /* {t}[] */' if t else ''
        lines.append(f'#define SR_RVA_{r["name"].upper():<20} {r["rva"]}u{note}')
    lines += ['', '#endif', '']
    return '\n'.join(lines)


def main():
    names = load_names()
    parts = [HEADER]
    for cname, (glob, size) in STRUCTS.items():
        parts.append(gen_struct(cname, glob, size, names.get(glob, {}), load_accessed(glob)))
    parts.append('#pragma pack(pop)\n\n#endif /* SR_TYPES_H */\n')
    out = ROOT / 'src/include'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'sr_types.h').write_text('\n\n'.join(parts), encoding='utf-8', newline='\n')
    (out / 'sr_globals_024d.h').write_text(gen_globals(), encoding='utf-8', newline='\n')
    print('gerado: src/include/sr_types.h, src/include/sr_globals_024d.h')


if __name__ == '__main__':
    main()
