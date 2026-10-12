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
    'ItemType': ('itemtype', 0x198),
    'ObjectType': ('objecttype', 0x828c),
    'Bond': ('bond', 0xec),
    'Event': ('event', 0x80),
    'Bullet': ('bullet', 0x5c),
    'Body': ('body', 0x40),
}
# Capacidade de cada array (tamanho da global no ELF 0.24c / tamanho do registro).
COUNTS = {'player': 256, 'human': 512, 'item': 1024, 'object': 1024, 'connection': 256,
          'bond': 16384, 'bullet': 16384, 'event': 65536, 'particle': 4096,
          'objecttype': 128, 'itemtype': 64, 'body': 32, 'account': 65536}
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

/* Osso do corpo (ragdoll). 15 por Human, a partir de Human+0xc8.
 * Fonte: createhuman e bondsimulation2 (Mach-O). */
typedef struct Bone {
    vec3 pos;           /* +0x00 createhuman: posição */
    vec3 pos_prev;      /* +0x0c createhuman: cópia da posição */
    uint8_t unk_18[0x18];
    vec3 rot[3];        /* +0x30 createhuman: matriz do parâmetro 2; bondsimulation2 usa as 3 linhas */
    uint8_t unk_54[0x38];
    float mass;         /* +0x8c bondsimulation2: proporção de massa com Item.mass */
    uint8_t unk_90[0x50];
} Bone;

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

/* Estado da partida: global "game" (uma só, não é array). 0x25d64 bytes.
 * Layout do servidor 0.24c (ELF) e do 024d. O 0.24b (Mac) tem um campo de 4 bytes
 * a menos no início: lá o tick fica em +0x34. Fonte: dedicatedserverloop,
 * logicsimulation_round e resetgame do 0.24c, conferidos no 024d. */
typedef struct Game {
    uint8_t unk_00[0x8];
    int gametype;       /* +0x08 config.txt "gametype=" (0 a 4) */
    uint8_t unk_0c[0x10];
    int round_number;   /* +0x1c resetgame soma 1 (provável) */
    int state;          /* +0x20 1 e 2; manager: 2 = partida rodando */
    int timer;          /* +0x24 diminui 1 por tick; 0 muda o estado */
    int wait_time;      /* +0x28 dedicatedserverloop grava 0xe10 (3600 ticks) */
    int round_time;     /* +0x2c config.txt "roundtime=" * 3600 */
    uint8_t unk_30[0x4];
    int unk_34;         /* +0x34 dedicatedserverloop grava 0x4ec */
    int ticks;          /* +0x38 soma 1 por tick; usado em Event.tick */
    uint8_t unk_3c[0x25d64 - 0x3c];
} Game;

SR_STATIC_ASSERT(sizeof(Game) == 0x25d64, "Game");
SR_STATIC_ASSERT(offsetof(Game, gametype) == 0x8, "Game.gametype");
SR_STATIC_ASSERT(offsetof(Game, state) == 0x20, "Game.state");
SR_STATIC_ASSERT(offsetof(Game, round_time) == 0x2c, "Game.round_time");
SR_STATIC_ASSERT(offsetof(Game, ticks) == 0x38, "Game.ticks");

SR_STATIC_ASSERT(sizeof(vec3) == 12, "vec3");
SR_STATIC_ASSERT(sizeof(Bone) == 0xe0, "Bone");
SR_STATIC_ASSERT(offsetof(Bone, rot) == 0x30, "Bone.rot");
SR_STATIC_ASSERT(offsetof(Bone, mass) == 0x8c, "Bone.mass");
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
    emitted = set()
    for off, sz, decl, comment, name in fields:
        if off < pos:
            if name:
                raise SystemExit(f'ERRO: {cname}.{name} (+0x{off:x}) sobrepõe o campo anterior. Corrija names.csv.')
            continue  # unk_ sobreposto a um campo com nome: descarta
        if off > pos:
            lines.append(f'    uint8_t pad_{pos:04x}[0x{off - pos:x}];')
        lines.append(f'    {decl};'.ljust(40) + f' /* +0x{off:04x} {comment} */')
        emitted.add(name)
        pos = off + sz
    if pos < size:
        lines.append(f'    uint8_t pad_{pos:04x}[0x{size - pos:x}];')
    lines.append(f'}} {cname};')
    lines.append(f'SR_STATIC_ASSERT(sizeof({cname}) == 0x{size:x}, "{cname}");')
    for off, _sz, _decl, _c, name in fields:
        if name and name in emitted:
            lines.append(f'SR_STATIC_ASSERT(offsetof({cname}, {name}) == 0x{off:x}, "{cname}.{name}");')
    return '\n'.join(lines)


def gen_globals():
    # Globais confirmadas à mão têm prioridade sobre o mapa automático.
    rows = {}
    with open(ROOT / 'analysis/024d/globals.csv', encoding='utf-8') as fh:
        for r in csv.DictReader(fh):
            rows[r['name']] = r
    with open(ROOT / 'analysis/024d/manual_globals.csv', encoding='utf-8') as fh:
        for r in csv.DictReader(fh):
            rows[r['name']] = r
    rows = list(rows.values())
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
    lines.append('')
    lines.append('/* Capacidade de cada array (tamanho da global no ELF 0.24c / tamanho do registro). */')
    for g, n in COUNTS.items():
        lines.append(f'#define SR_MAX_{g.upper():<20} {n}')
    lines += ['', '#endif', '']
    return '\n'.join(lines)


def gen_global_table():
    """Tabela {rva, nome, tamanho do registro, quantidade} para o log do verificador."""
    rows = {}
    for path in ('analysis/024d/globals.csv', 'analysis/024d/manual_globals.csv'):
        with open(ROOT / path, encoding='utf-8') as fh:
            for r in csv.DictReader(fh):
                rows[r['name']] = int(r['rva'], 16)
    sizes = {g: s for (g, s) in STRUCTS.values()}
    sizes['account'] = 0x4c
    lines = ['/* Gerado por tools/re/gen_types.py. Globais do 024d para mensagens de diagnóstico. */',
             '#ifndef SR_GLOBAL_TABLE_H', '#define SR_GLOBAL_TABLE_H', '',
             'typedef struct SrGlobalInfo { unsigned rva; const char *name; unsigned elem_size; unsigned count; } SrGlobalInfo;',
             '', 'static const SrGlobalInfo sr_global_table[] = {']
    for name, rva in sorted(rows.items(), key=lambda x: x[1]):
        size = sizes.get(name, 0)
        count = COUNTS.get(name, 0)
        if name == 'account':
            rva += 8  # registros depois da chave e do contador
        lines.append(f'    {{ 0x{rva:x}u, "{name}", 0x{size:x}u, {count}u }},')
    lines += ['};', '', '#endif', '']
    return chr(10).join(lines)


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
    (out / 'sr_global_table.h').write_text(gen_global_table(), encoding='utf-8', newline='\n')
    print('gerado: src/include/sr_types.h, src/include/sr_globals_024d.h')


if __name__ == '__main__':
    main()
