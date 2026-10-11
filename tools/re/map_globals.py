"""Mapeia variáveis globais de um binário sem símbolos a partir de funções já casadas.

Para cada par de funções (alvo, referência) com o mesmo nome, cada acesso do
alvo ao endereço A e cada acesso da referência a "simbolo + offset" geram um
voto: "simbolo começa em A - offset no alvo". O endereço certo junta votos de
muitas funções. Os errados ficam espalhados.

O Mach-O i386 é a melhor referência: 32 bits, então os offsets de struct batem
com o Windows. O ELF x64 entra como segunda opinião.

Uso:
  python -I tools/re/map_globals.py --target build/features/ded024d.jsonl \
      --names analysis/024d/functions.csv \
      --ref build/features/mac_i386.jsonl --ref build/features/linux_x64.jsonl \
      --out analysis/024d/globals.csv
"""
import argparse
import bisect
import csv
import json
import os
from collections import Counter, defaultdict

# Símbolos que não são do jogo.
SKIP = ('__', '_GLEE', 'GLee', '_glee', 'stbi', 'z_', 'completed.', 'dtor_idx', 'object.', '_DYNAMIC',
        '_GLOBAL_OFFSET', 'environ', 'stdout', 'stderr', 'stdin')


def load_features(path):
    funcs = {}
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            f = json.loads(line)
            n = f['name']
            if n.startswith('_') and not n.startswith('__'):
                n = n[1:]
            f['name'] = n
            funcs[n] = f
    return funcs


class Symbols:
    def __init__(self, path):
        rows = []
        with open(path, encoding='utf-8') as fh:
            for line in fh:
                rva, name = line.rstrip('\n').split('\t')
                if name.startswith('_') and not name.startswith('__'):
                    name = name[1:]
                rows.append((int(rva, 16), name))
        rows.sort()
        self.addrs = [r[0] for r in rows]
        self.names = [r[1] for r in rows]

    def containing(self, addr):
        i = bisect.bisect_right(self.addrs, addr) - 1
        if i < 0:
            return None
        name = self.names[i]
        if name.startswith(SKIP):
            return None
        return name, addr - self.addrs[i]

    def size(self, name):
        i = self.names.index(name)
        return self.addrs[i + 1] - self.addrs[i] if i + 1 < len(self.addrs) else 0x1000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--target', required=True)
    ap.add_argument('--names', required=True, help='CSV do casador com os nomes do alvo')
    ap.add_argument('--ref', action='append', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument("--min-votes", type=float, default=3)
    args = ap.parse_args()

    target = {}
    with open(args.target, encoding='utf-8') as fh:
        for line in fh:
            f = json.loads(line)
            target[f['rva']] = f
    with open(args.names, encoding='utf-8') as fh:
        named = {r['rva']: r['name'] for r in csv.DictReader(fh) if r['name']}

    votes = defaultdict(Counter)  # símbolo -> {base no alvo: votos}
    sizes = {}
    origin = defaultdict(set)
    for ref_path in args.ref:
        ref = load_features(ref_path)
        syms = Symbols(ref_path + '.symbols.tsv')
        tag = 'mac' if 'mac' in ref_path else 'linux'
        ref_tag = os.path.basename(ref_path).split('.')[0]
        for rva, name in named.items():
            r = ref.get(name)
            if not r:
                continue
            tglob = {int(a, 16) for a in target[rva]['globals']}
            rglob = set()
            for a in r['globals']:
                c = syms.containing(int(a, 16))
                if c:
                    rglob.add(c)
            if not tglob or not rglob:
                continue
            # Cada par de funções vota no máximo uma vez em cada (símbolo, base).
            # Uma base certa recebe votos de muitas funções diferentes.
            # Uma base errada é coincidência e quase nunca se repete.
            pair_votes = set()
            for sym, off in rglob:
                for a in tglob:
                    pair_votes.add((sym, a - off))
                if sym not in sizes:
                    sizes[sym] = syms.size(sym)
            # O Mach-O é 32 bits como o Windows: os offsets dele são os mais confiáveis.
            weight = 1.5 if tag == 'mac' else 1.0
            for sym, base in pair_votes:
                votes[sym][base] += weight
                origin[sym].add(ref_tag)

    rows = []
    taken = {}
    for sym, cnt in votes.items():
        ranked = cnt.most_common()
        base, v = ranked[0]
        # Vizinhos (+4, +8...) recebem votos por diferença de offset entre 32 e 64 bits.
        # O concorrente real é o melhor candidato longe da base escolhida.
        second = next((c for b, c in ranked[1:] if abs(b - base) > 0x40), 0)
        if v >= args.min_votes and v >= 2 * second and base > 0:
            rows.append((base, sym, round(v, 2), round(second, 2), sizes[sym], '+'.join(sorted(origin[sym]))))

    # Resolve colisões: dois símbolos no mesmo endereço -> fica o de mais votos.
    rows.sort(key=lambda r: -r[2])
    final = []
    for r in rows:
        if r[0] in taken:
            continue
        taken[r[0]] = r[1]
        final.append(r)
    final.sort()

    with open(args.out, 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh, lineterminator='\n')
        w.writerow(['rva', 'name', 'votes', 'second', 'ref_size', 'refs'])
        for base, sym, v, second, size, refs in final:
            w.writerow([f'0x{base:x}', sym, v, second, f'0x{size:x}', refs])
    print(f'{len(final)} globais mapeadas')


if __name__ == '__main__':
    main()
