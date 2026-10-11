"""Casa funções de um binário sem símbolos com binários de referência com símbolos.

Entrada: arquivos .jsonl de tools/ghidra/ExportFunctionFeatures.java.
Saída: CSV com rva, nome, fonte, confiança e evidência.

Método:
  1. Âncoras: funções com conjunto de strings/constantes raro e quase igual.
  2. Propagação: para cada par casado, alinha as listas de chamadas internas
     e vota nos pares (chamador -> chamado). Repete até não mudar nada.

Uso:
  python -I tools/re/match_functions.py --target build/features/ded024d.jsonl \
      --ref build/features/linux_x64.jsonl --ref build/features/mac_i386.jsonl \
      --out analysis/024d/functions.csv
"""
import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from difflib import SequenceMatcher

# Nomes que não são do jogo (bibliotecas embutidas, CRT, código de runtime).
LIB_PREFIX = re.compile(
    r'^(GLee|SDL|gl[A-Z]|stbi|__|_start|frame_dummy|call_gmon|register_tm|deregister_tm'
    r'|speex|spx|z[a-z_]+$|png|jpeg|hdr_|bmp_|tga_|psd_|FUN_|thunk_|entry$)')


def load(path, strip_underscore=False):
    funcs = {}
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            f = json.loads(line)
            if strip_underscore and f['name'].startswith('_'):
                f['name'] = f['name'][1:]
            funcs[f['rva']] = f
    return funcs


def norm_ext(name):
    """Normaliza importações entre CRTs: _CIsqrt / sqrtf / sqrt -> sqrt."""
    n = name.lstrip('_')
    if n.startswith('CI'):
        n = n[2:]
    if n.endswith('f') and n[:-1] in ('sqrt', 'sin', 'cos', 'atan2', 'pow', 'acos', 'floor', 'fabs', 'asin', 'tan'):
        n = n[:-1]
    return n


def fingerprint(f):
    """Conjunto de características comparáveis entre arquiteturas."""
    feats = Counter()
    for s in f['strings']:
        feats['s:' + s] += 1
    for v in f['floats']:
        feats['f:' + v] += 1
    for c in f['calls']:
        if c.startswith('EXT:'):
            feats['x:' + norm_ext(c[4:])] += 1
    return feats


def similarity(a, b):
    if not a or not b:
        return 0.0
    inter = sum((a & b).values())
    union = sum((a | b).values())
    return inter / union


def rarity(feats, df, total):
    """Quanto mais raras as características, mais confiável é a âncora."""
    if not feats:
        return 0.0
    return sum(1.0 / df[k] for k in feats) / len(feats)


def internal_calls(f):
    return [c for c in f['calls'] if not c.startswith('EXT:')]


def run(target, refs, min_anchor=0.6):
    tfp = {r: fingerprint(f) for r, f in target.items()}
    df = Counter(k for fp in tfp.values() for k in fp)

    # name -> (ref_funcs, rva) para todas as referências
    by_name = {}
    for ref in refs:
        for rva, f in ref.items():
            if LIB_PREFIX.match(f['name']):
                continue
            by_name.setdefault(f['name'], []).append((ref, rva))

    result = {}  # rva alvo -> (nome, fonte, confiança, evidência)
    used = set()

    # 1. Âncoras por características
    cands = []
    for name, entries in by_name.items():
        rfp = Counter()
        for ref, rva in entries:
            rfp |= fingerprint(ref[rva])
        if sum(rfp.values()) < 2 or not any(k.startswith(('s:', 'f:')) for k in rfp):
            continue
        scored = sorted(((similarity(rfp, fp), r) for r, fp in tfp.items() if fp), reverse=True)[:2]
        if not scored:
            continue
        best, rva = scored[0]
        second = scored[1][0] if len(scored) > 1 else 0.0
        if best >= min_anchor and best - second >= 0.15:
            cands.append((best * (0.5 + rarity(tfp[rva], df, len(tfp))), best, name, rva))

    # Resolve conflitos: o melhor par fica com a função.
    for _, best, name, rva in sorted(cands, reverse=True):
        if rva in result or name in used:
            continue
        result[rva] = (name, 'anchor', round(best, 3), 'strings/floats/imports')
        used.add(name)

    # 2. Propagação pelo grafo de chamadas
    changed = True
    rounds = 0
    while changed and rounds < 50:
        changed = False
        rounds += 1
        votes = defaultdict(Counter)
        for trva, (name, *_rest) in list(result.items()):
            tcalls = internal_calls(target[trva])
            for ref, rrva in by_name.get(name, []):
                rcalls = [ref[c]['name'] for c in internal_calls(ref[rrva]) if c in ref]
                # Alinha as sequências pelo tamanho relativo do chamado (proxy estável entre compiladores).
                sm = SequenceMatcher(None, [bucket(target, c) for c in tcalls],
                                     [bucket_ref(ref, by_name, n) for n in rcalls], autojunk=False)
                for tag, i1, i2, j1, j2 in sm.get_opcodes():
                    if tag == 'equal' or (tag == 'replace' and i2 - i1 == j2 - j1):
                        for k in range(i2 - i1):
                            votes[tcalls[i1 + k]][rcalls[j1 + k]] += 2 if tag == 'equal' else 1
        for trva, cnt in votes.items():
            if trva in result:
                continue
            (name, v), *rest = cnt.most_common(2) + [(None, 0)]
            second = rest[0][1]
            if name and name not in used and not LIB_PREFIX.match(name) and v >= 2 and v > second * 2:
                result[trva] = (name, 'callgraph', round(min(1.0, v / 6), 3), f'votes={v}')
                used.add(name)
                changed = True
    return result


def bucket(funcs, rva):
    f = funcs.get(rva)
    return size_bucket(f['insns']) if f else 0


def bucket_ref(ref, by_name, name):
    for r, rva in by_name.get(name, []):
        if r is ref:
            return size_bucket(r[rva]['insns'])
    return 0


def size_bucket(n):
    # Escala logarítmica larga: compiladores diferentes mudam o tamanho, mas não a ordem de grandeza.
    b = 0
    while n > 4:
        n //= 3
        b += 1
    return b


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--target', required=True)
    ap.add_argument('--ref', action='append', required=True)
    ap.add_argument('--out', required=True)
    args = ap.parse_args()

    target = load(args.target)
    refs = [load(p, strip_underscore='mac' in p) for p in args.ref]
    result = run(target, refs)

    with open(args.out, 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh, lineterminator='\n')
        w.writerow(['rva', 'size', 'name', 'source', 'confidence', 'evidence'])
        for rva in sorted(target, key=lambda r: int(r, 16)):
            name, src, conf, ev = result.get(rva, ('', '', '', ''))
            w.writerow([rva, target[rva]['size'], name, src, conf, ev])

    named = len(result)
    print(f'{named}/{len(target)} funções nomeadas '
          f'({sum(1 for v in result.values() if v[1] == "anchor")} âncoras)')


if __name__ == '__main__':
    main()
