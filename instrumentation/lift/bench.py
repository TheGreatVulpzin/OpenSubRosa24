"""Speed of the C version of each function against the original, on the recorded calls.

The harness runs with HARNESS_TIME=1: each recorded call runs once in each version, with no
comparison, and the harness prints the cycles of each function. This script adds the cycles
of all the recordings, one run at a time (two runs at the same time disturb the times).

  python -I instrumentation/lift/bench.py              the rewritten functions (src/common, src/net, src/game)
  python -I instrumentation/lift/bench.py --all        every function in the table
  python -I instrumentation/lift/bench.py --all-calls  every recorded call, not the short files (much slower)
  python -I instrumentation/lift/bench.py --only NAME  only the recordings with NAME in their name
  python -I instrumentation/lift/bench.py --funcs a,b  only these functions (the harness runs only their calls)

The short call files (sv_args.min.txt) have 2 calls for each code path of each function, so
they cover the same functions as the full files in a small part of the time.

A ratio above 1 means that the C version is slower. Functions with few calls give noisy
ratios: look at the cycles too. Run it at the end of each lot of the rewrite.
"""
import argparse
import os
import subprocess
import sys
import tempfile
import time
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import gate  # noqa: E402
import lift  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--target', default='ded024d')
    ap.add_argument('--all', action='store_true', help='every function, not only the rewritten ones')
    ap.add_argument('--all-calls', action='store_true', help='sv_args.txt in place of sv_args.min.txt')
    ap.add_argument('--only', help='only the recordings with this text in the name')
    ap.add_argument('--warn', type=float, default=1.05, help='mark a ratio above this (default 1.05)')
    ap.add_argument('--funcs', help='only these functions (names separated by commas)')
    a = ap.parse_args()

    t = gate.load_target(a.target)
    names = lift.load_names()
    rewritten = {names.get('0x%x' % rva, 'lf_%05x' % rva) for rva in lift.rewritten_rvas(names)}
    total = defaultdict(lambda: [0, 0, 0])  # name -> calls, cycles of the C version, of the original
    out = Path(t['out_dir']) / 'bench'
    out.mkdir(parents=True, exist_ok=True)
    only = a.funcs.split(',') if a.funcs else None
    filter_arg = []
    if only:
        # Same filter file as gate.py: one name for each line, passed as @file.
        lst = out / 'funcs.txt'
        lst.write_text('\n'.join(only) + '\n')
        filter_arg = ['@' + str(lst)]
    t0 = time.time()
    for r in t['recordings']:
        d = t['recordings_dir'] / r['name']
        if a.only and a.only not in r['name']:
            continue
        args = d / 'sv_args.txt'
        if not a.all_calls and (d / 'sv_args.min.txt').exists():
            args = d / 'sv_args.min.txt'
        if not (d / 'sv_snapshot.bin').exists() or not args.exists():
            continue
        with tempfile.TemporaryDirectory() as env_dir:
            env = dict(os.environ, HARNESS_TIME='1', HARNESS_ENV_DIR=env_dir, SV_UI_WIDTH='220',
                       SV_LOG_FILE=str(out / f'{r["name"]}.log'))
            p = subprocess.run([str(t['harness']), str(t['image']), str(d / 'sv_snapshot.bin'), str(args)] + filter_arg,
                               capture_output=True, text=True, errors='replace', env=env, cwd=ROOT)
        (out / f'{r["name"]}.txt').write_text(p.stdout + p.stderr, encoding='utf-8')
        n = 0
        for line in p.stdout.splitlines():
            parts = line.split()
            if len(parts) == 5 and parts[0] == 'time':
                acc = total[parts[1]]
                acc[0] += int(parts[2])
                acc[1] += int(parts[3])
                acc[2] += int(parts[4])
                n += 1
        print(f'{r["name"]}: {n} functions')
    print(f'{time.time() - t0:.0f} s\n')

    rows = [(name, c, new, orig) for name, (c, new, orig) in total.items()
            if (name in only if only else a.all or name in rewritten)]
    rows.sort(key=lambda x: -(x[2] / x[3] if x[3] else 0))
    print(f'{"function":32s} {"calls":>7s} {"C cyc/call":>11s} {"orig cyc/call":>13s} {"ratio":>6s}')
    slow = 0
    sum_new = sum_orig = 0
    for name, c, new, orig in rows:
        ratio = new / orig if orig else 0.0
        mark = '  <- slower' if ratio > a.warn else ''
        slow += bool(mark)
        sum_new += new
        sum_orig += orig
        print(f'{name:32s} {c:7d} {new / c:11.0f} {orig / c:13.0f} {ratio:6.2f}{mark}')
    if sum_orig:
        print(f'\n{len(rows)} functions, all cycles: C {sum_new} / original {sum_orig} = {sum_new / sum_orig:.3f}')
    print(f'{slow} function(s) above {a.warn}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
