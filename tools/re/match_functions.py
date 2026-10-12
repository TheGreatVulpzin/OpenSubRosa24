"""Match functions of a binary without symbols against reference binaries with symbols.

Input: .jsonl files from tools/ghidra/ExportFunctionFeatures.java.
Output: CSV with rva, name, source, confidence and evidence.

Method:
  1. Anchors: compare strings, floats, imports and positive integers
     (struct offsets, record sizes). Each feature is weighted by the
     inverse of its frequency (IDF). MSVC inlines a lot, so a target
     function can contain several reference functions. For this reason
     the score measures how much of the reference appears in the target.
  2. Propagation: for each matched pair, align the internal calls and vote
     on caller -> callee pairs. Repeat until nothing changes.

Usage:
  python -I tools/re/match_functions.py --target build/features/ded024d.jsonl \
      --ref build/features/linux_x64.jsonl --ref build/features/mac_i386.jsonl \
      --out analysis/024d/functions.csv

Accuracy test (the target has names, so we can check):
  python -I tools/re/match_functions.py --target build/features/mac_i386.jsonl \
      --ref build/features/linux_x64.jsonl --evaluate
"""
import argparse
import bisect
import csv
import os
import json
import math
import re
from collections import Counter, defaultdict
from difflib import SequenceMatcher

# Names that are not from the game (embedded libraries, CRT, runtime).
LIB_NAME = re.compile(
    r'^(GLee|SDL|gl[A-Z]|stbi|__|_start|frame_dummy|call_gmon|register_tm|deregister_tm|_init$|_fini$'
    r'|speex|spx|png|jpeg|hdr_|bmp_|tga_|psd_|FUN_|thunk_|entry$|dyld|stub)')
MATH_EXT = ('sqrt', 'sin', 'cos', 'atan2', 'pow', 'acos', 'floor', 'fabs', 'asin', 'tan', 'ceil', 'exp', 'log')


class GlobalNames:
    """Translate data addresses into global variable names."""

    def __init__(self, rows):  # rows: [(start, end, name)]
        rows.sort()
        self.starts = [r[0] for r in rows]
        self.rows = rows

    def name(self, addr):
        i = bisect.bisect_right(self.starts, addr) - 1
        if i >= 0 and addr < self.rows[i][1]:
            return self.rows[i][2]
        return None

    @staticmethod
    def from_symbols(path):
        rows = []
        with open(path, encoding='utf-8') as fh:
            items = sorted((int(a, 16), n.lstrip('_')) for a, n in (l.split() for l in fh if l.strip()))
        for (a, n), nxt in zip(items, items[1:] + [(items[-1][0] + 0x1000, '')]):
            rows.append((a, nxt[0], n))
        return GlobalNames(rows)

    @staticmethod
    def from_csv(path):
        with open(path, encoding='utf-8') as fh:
            rows = [(int(r['rva'], 16), int(r['rva'], 16) + int(r['ref_size'], 16), r['name'])
                    for r in csv.DictReader(fh)]
        return GlobalNames(rows)


def attach_globals(funcs, gn):
    for f in funcs.values():
        f['gnames'] = sorted({n for a in f.get('globals', []) if (n := gn.name(int(a, 16)))})


def load(path, globals_csv=None, use_symbols=False):
    """Load features. Accepts "features.jsonl:names.csv" to use names from a matcher CSV.
    Global names: from the .symbols.tsv file next to it (binaries with symbols) or from globals_csv."""
    names = None
    if ':' in path[2:]:  # ignore the drive letter on Windows
        path, csv_path = path.rsplit(':', 1)
        with open(csv_path, encoding='utf-8') as fh:
            names = {r['rva']: r['name'] for r in csv.DictReader(fh) if r['name']}
    funcs = {}
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            f = json.loads(line)
            # Mach-O: C symbols start with "_".
            if f['name'].startswith('_') and not f['name'].startswith('__'):
                f['name'] = f['name'][1:]
            if names is not None:
                f['name'] = names.get(f['rva'], f['name'])
            funcs[f['rva']] = f
    sym_path = path + '.symbols.tsv'
    if globals_csv:
        attach_globals(funcs, GlobalNames.from_csv(globals_csv))
    elif use_symbols and names is None and os.path.exists(sym_path) and os.path.getsize(sym_path) > 0:
        attach_globals(funcs, GlobalNames.from_symbols(sym_path))
    return funcs


def norm_ext(name):
    """Normalize imports across CRTs: _CIsqrt / sqrtf / ___memset_chk -> sqrt / memset."""
    n = name.lstrip('_')
    if n.startswith('CI'):
        n = n[2:]
    if n.endswith('_chk'):
        n = n[:-4]
    if n.endswith('f') and n[:-1] in MATH_EXT:
        n = n[:-1]
    return n


def fingerprint(f, strict=False):
    """strict=True: target and reference come from the same compiler (MSVC 2010).
    In this case stack offsets and the code size are also comparable."""
    feats = Counter()
    if strict:
        for v in f['ints']:
            if v < 0:
                feats['n:%d' % v] += 1
        feats['z:%d' % (f['insns'] // 10)] += 1
        feats['b:%d' % (f['blocks'] // 3)] += 1
        feats['c:%d' % len(f['calls'])] += 1
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
    for g in f.get('gnames', []):
        feats['g:' + g] += 1
    return feats


class Scorer:
    def __init__(self, target_fps):
        df = Counter(k for fp in target_fps.values() for k in fp)
        n = len(target_fps)
        self.idf = {k: math.log(1 + n / c) for k, c in df.items()}
        self.default = math.log(1 + n)  # feature missing from the target
        # Kinds of clue the target has (s:, f:, x:, i:, g:...). A kind the target
        # does not have (e.g. named globals in an .exe without a globals map) does not count.
        self.kinds = {k[:2] for k in df}

    def weight(self, k, count):
        if k[:2] not in self.kinds:
            return 0.0
        w = self.idf.get(k, self.default)
        if k.startswith('s:'):
            w *= 3  # strings are the strongest evidence
        return w * min(count, 3)

    def score(self, ref_fp, tgt_fp):
        """Weighted fraction of the reference that appears in the target, with a penalty for the target's excess."""
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


def run(target, refs, min_anchor=0.45, margin=1.3, strict=False, seeds=None):
    tfp = {r: fingerprint(f, strict) for r, f in target.items()}
    scorer = Scorer(tfp)

    by_name = defaultdict(list)  # name -> [(ref, rva)]
    for ref in refs:
        for rva, f in ref.items():
            if not LIB_NAME.match(f['name']):
                by_name[f['name']].append((ref, rva))

    # Inverted index: feature -> target functions (speeds up the search).
    inv = defaultdict(set)
    for r, fp in tfp.items():
        for k in fp:
            inv[k].add(r)

    cands = []
    for name, entries in by_name.items():
        best = {}
        for ref, rva in entries:
            rfp = fingerprint(ref[rva], strict)
            if sum(scorer.weight(k, c) for k, c in rfp.items()) < 6:
                continue  # little evidence
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
    for t, name in (seeds or {}).items():  # hand-reviewed names go first
        if t in target:
            result[t] = (name, 'manual', 1.0, 'seed')
            used.add(name)
    for s, name, t in sorted(cands, reverse=True):
        if t in result or name in used:
            continue
        result[t] = (name, 'anchor', round(s, 3), 'features')
        used.add(name)

    # Propagation through the call graph.
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
    print(f'matched {len(result)} of {game} game functions | right {ok} | wrong {len(bad)} '
          f'| accuracy {ok / max(1, len(result)):.1%}')
    for src in ('anchor', 'callgraph'):
        sub = [r for r, v in result.items() if v[1] == src]
        good = sum(1 for r in sub if target[r]['name'] == result[r][0])
        print(f'  {src:9}: {len(sub)} matched, {good / max(1, len(sub)):.1%} right')
    for r, real, v in bad[:15]:
        print(f'  ERROR {r}: real={real} matched={v[0]} ({v[1]} {v[2]})')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--target', required=True)
    ap.add_argument('--ref', action='append', required=True)
    ap.add_argument('--out')
    ap.add_argument('--evaluate', action='store_true')
    ap.add_argument('--strict', action='store_true', help='target and references from the same compiler')
    ap.add_argument('--target-globals', help='CSV of target globals (tools/re/map_globals.py)')
    ap.add_argument('--seed', help='CSV rva,name: manual names used as propagation seeds')
    ap.add_argument('--keep', help='previous CSV of the target: its names have priority')
    args = ap.parse_args()

    target = load(args.target, args.target_globals)
    refs = [load(p, use_symbols=True) for p in args.ref]
    seeds = None
    if args.seed:
        with open(args.seed, encoding='utf-8') as fh:
            seeds = {r['rva']: r['name'] for r in csv.DictReader(fh)}
    result = run(target, refs, strict=args.strict, seeds=seeds)
    if args.keep:
        with open(args.keep, encoding='utf-8') as fh:
            prev = {r['rva']: r for r in csv.DictReader(fh) if r['name']}
        used = {v[0] for v in result.values()}
        conflicts = 0
        for rva, r in prev.items():
            if rva in result and result[rva][0] != r['name']:
                conflicts += 1
            if rva not in result or result[rva][0] != r['name']:
                if r['name'] not in used or result.get(rva, (None,))[0] == r['name']:
                    result[rva] = (r['name'], r['source'], r['confidence'], r['evidence'])
        print(f'kept {len(prev)} names from the previous CSV ({conflicts} conflicts resolved in its favor)')

    if args.evaluate:
        evaluate(target, result)
    if args.out:
        with open(args.out, 'w', newline='', encoding='utf-8') as fh:
            w = csv.writer(fh, lineterminator='\n')
            w.writerow(['rva', 'size', 'name', 'source', 'confidence', 'evidence'])
            for rva in sorted(target, key=lambda r: int(r, 16)):
                name, src, conf, ev = result.get(rva, ('', '', '', ''))
                w.writerow([rva, target[rva]['size'], name, src, conf, ev])
        print(f'{len(result)}/{len(target)} functions named '
              f'({sum(1 for v in result.values() if v[1] == "anchor")} anchors)')


if __name__ == '__main__':
    main()
