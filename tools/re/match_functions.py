"""Casa funções de um binário sem símbolos com binários de referência com símbolos.

Entrada: arquivos .jsonl de tools/ghidra/ExportFunctionFeatures.java.
Saída: CSV com rva, nome, fonte, confiança e evidência.

Método:
  1. Âncoras: compara strings, floats, importações e inteiros positivos
     (offsets de struct, tamanhos de registro). Cada característica pesa
     pelo inverso da frequência (IDF). O MSVC faz muito inlining, então a
     função do alvo pode conter várias funções da referência. Por isso a
     pontuação mede quanto da referência aparece no alvo.
  2. Propagação: para cada par casado, alinha as chamadas internas e vota
     nos pares chamador -> chamado. Repete até não mudar nada.

Uso:
  python -I tools/re/match_functions.py --target build/features/ded024d.jsonl \
      --ref build/features/linux_x64.jsonl --ref build/features/mac_i386.jsonl \
      --out analysis/024d/functions.csv

Teste de precisão (o alvo tem nomes, então dá para conferir):
  python -I tools/re/match_functions.py --target build/features/mac_i386.jsonl \
      --ref build/features/linux_x64.jsonl --evaluate
"""
import argparse
import csv
import json
import math
import re
from collections import Counter, defaultdict
from difflib import SequenceMatcher

# Nomes que não são do jogo (bibliotecas embutidas, CRT, runtime).
LIB_NAME = re.compile(
    r'^(GLee|SDL|gl[A-Z]|stbi|__|_start|frame_dummy|call_gmon|register_tm|deregister_tm|_init$|_fini$'
    r'|speex|spx|png|jpeg|hdr_|bmp_|tga_|psd_|FUN_|thunk_|entry$|dyld|stub)')
MATH_EXT = ('sqrt', 'sin', 'cos', 'atan2', 'pow', 'acos', 'floor', 'fabs', 'asin', 'tan', 'ceil', 'exp', 'log')


def load(path):
    funcs = {}
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            f = json.loads(line)
            # Mach-O: símbolos C começam com "_".
            if f['name'].startswith('_') and not f['name'].startswith('__'):
                f['name'] = f['name'][1:]
            funcs[f['rva']] = f
    return funcs


def norm_ext(name):
    """Normaliza importações entre CRTs: _CIsqrt / sqrtf / ___memset_chk -> sqrt / memset."""
    n = name.lstrip('_')
    if n.startswith('CI'):
        n = n[2:]
    if n.endswith('_chk'):
        n = n[:-4]
    if n.endswith('f') and n[:-1] in MATH_EXT:
        n = n[:-1]
    return n


def fingerprint(f):
    feats = Counter()
    for s in f['strings']:
        feats['s:' + s] += 1
    for v in f['floats']:
        if v not in ('0.0', '1.0', '-1.0', '0.5', '2.0'):
            feats['f:' + v] += 1
    for c in f['calls']:
        if c.startswith('EXT:'):
            feats['x:' + norm_ext(c[4:])] += 1
    for v in f['ints']:
        if v >= 32:
            feats['i:%d' % v] += 1
    return feats


class Scorer:
    def __init__(self, target_fps):
        df = Counter(k for fp in target_fps.values() for k in fp)
        n = len(target_fps)
        self.idf = {k: math.log(1 + n / c) for k, c in df.items()}
        self.default = math.log(1 + n)  # característica ausente no alvo

    def weight(self, k, count):
        w = self.idf.get(k, self.default)
        if k.startswith('s:'):
            w *= 3  # strings são a evidência mais forte
        return w * min(count, 3)

    def score(self, ref_fp, tgt_fp):
        """Fração (ponderada) da referência que aparece no alvo, com pena pelo excesso do alvo."""
        total = sum(self.weight(k, c) for k, c in ref_fp.items())
        if total == 0:
            return 0.0
        shared = sum(self.weight(k, min(c, tgt_fp[k])) for k, c in ref_fp.items() if k in tgt_fp)
        extra = sum(self.weight(k, c) for k, c in tgt_fp.items() if k not in ref_fp)
        return shared / total * (total / (total + 0.25 * extra)) ** 0.5


def internal_calls(f):
    return [c for c in f['calls'] if not c.startswith('EXT:')]


def size_bucket(n):
    b = 0
    while n > 4:
        n //= 3
        b += 1
    return b


def run(target, refs, min_anchor=0.45, margin=1.3):
    tfp = {r: fingerprint(f) for r, f in target.items()}
    scorer = Scorer(tfp)

    by_name = defaultdict(list)  # nome -> [(ref, rva)]
    for ref in refs:
        for rva, f in ref.items():
            if not LIB_NAME.match(f['name']):
                by_name[f['name']].append((ref, rva))

    # Índice invertido: característica -> funções do alvo (acelera a busca).
    inv = defaultdict(set)
    for r, fp in tfp.items():
        for k in fp:
            inv[k].add(r)

    cands = []
    for name, entries in by_name.items():
        best = {}
        for ref, rva in entries:
            rfp = fingerprint(ref[rva])
            if sum(scorer.weight(k, c) for k, c in rfp.items()) < 6:
                continue  # pouca evidência
            pool = set().union(*(inv[k] for k in rfp if k in inv)) if rfp else set()
            for t in pool:
                s = scorer.score(rfp, tfp[t])
                best[t] = max(best.get(t, 0), s)
        if not best:
            continue
        ranked = sorted(best.items(), key=lambda x: -x[1])
        (t1, s1), s2 = ranked[0], (ranked[1][1] if len(ranked) > 1 else 0.0)
        if s1 >= min_anchor and s1 >= s2 * margin:
            cands.append((s1, name, t1))

    result, used = {}, set()
    for s, name, t in sorted(cands, reverse=True):
        if t in result or name in used:
            continue
        result[t] = (name, 'anchor', round(s, 3), 'features')
        used.add(name)

    # Propagação pelo grafo de chamadas.
    for _ in range(50):
        votes = defaultdict(Counter)
        for trva, (name, *_r) in list(result.items()):
            tcalls = internal_calls(target[trva])
            for ref, rrva in by_name.get(name, []):
                rcalls = [ref[c]['name'] for c in internal_calls(ref[rrva]) if c in ref]
                rcalls = [n for n in rcalls if n in by_name]
                sm = SequenceMatcher(None,
                                     [size_bucket(target[c]['insns']) if c in target else 0 for c in tcalls],
                                     [size_bucket(ref[by_rva(ref, n)]['insns']) for n in rcalls],
                                     autojunk=False)
                for tag, i1, i2, j1, j2 in sm.get_opcodes():
                    if tag == 'equal' or (tag == 'replace' and i2 - i1 == j2 - j1):
                        for k in range(i2 - i1):
                            votes[tcalls[i1 + k]][rcalls[j1 + k]] += 2 if tag == 'equal' else 1
        changed = False
        for trva, cnt in votes.items():
            if trva in result or trva not in target:
                continue
            top = cnt.most_common(2)
            name, v = top[0]
            second = top[1][1] if len(top) > 1 else 0
            if name not in used and v >= 2 and v > second * 2:
                result[trva] = (name, 'callgraph', round(min(1.0, v / 6), 3), f'votes={v}')
                used.add(name)
                changed = True
        if not changed:
            break
    return result


_rva_cache = {}


def by_rva(ref, name):
    key = id(ref)
    if key not in _rva_cache:
        _rva_cache[key] = {f['name']: r for r, f in ref.items()}
    return _rva_cache[key][name]


def evaluate(target, result):
    ok = sum(1 for r, v in result.items() if target[r]['name'] == v[0])
    bad = [(r, target[r]['name'], v) for r, v in result.items() if target[r]['name'] != v[0]]
    game = sum(1 for f in target.values() if not LIB_NAME.match(f['name']))
    print(f'casadas {len(result)} de {game} funções do jogo | certas {ok} | erradas {len(bad)} '
          f'| precisão {ok / max(1, len(result)):.1%}')
    for src in ('anchor', 'callgraph'):
        sub = [r for r, v in result.items() if v[1] == src]
        good = sum(1 for r in sub if target[r]['name'] == result[r][0])
        print(f'  {src:9}: {len(sub)} casadas, {good / max(1, len(sub)):.1%} certas')
    for r, real, v in bad[:15]:
        print(f'  ERRO {r}: real={real} casado={v[0]} ({v[1]} {v[2]})')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--target', required=True)
    ap.add_argument('--ref', action='append', required=True)
    ap.add_argument('--out')
    ap.add_argument('--evaluate', action='store_true')
    args = ap.parse_args()

    target = load(args.target)
    refs = [load(p) for p in args.ref]
    result = run(target, refs)

    if args.evaluate:
        evaluate(target, result)
    if args.out:
        with open(args.out, 'w', newline='', encoding='utf-8') as fh:
            w = csv.writer(fh, lineterminator='\n')
            w.writerow(['rva', 'size', 'name', 'source', 'confidence', 'evidence'])
            for rva in sorted(target, key=lambda r: int(r, 16)):
                name, src, conf, ev = result.get(rva, ('', '', '', ''))
                w.writerow([rva, target[rva]['size'], name, src, conf, ev])
        print(f'{len(result)}/{len(target)} funções nomeadas '
              f'({sum(1 for v in result.values() if v[1] == "anchor")} âncoras)')


if __name__ == '__main__':
    main()
