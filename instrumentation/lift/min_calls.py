"""Write a shorter sv_args file for each recording: a few calls for each code path.

Many recorded calls run the same path through a function (the option reader runs the same
blocks dozens of times at start-up). The gate gets nothing from the copies. This script
runs the harness in coverage mode (HARNESS_COVER, an INT3 on each basic block of the
original) and keeps, for each function, the first --keep calls of each path. A call where
the original crashed counts as its own path.

The count is for all recordings together: a path that an earlier recording (in the order of
the target file) already keeps is not kept again. The recordings of the same map run the
same level code (createlevel, resetgame: about 20 s each), so most of them drop it.
--per-recording counts each recording alone.

Output: <recording>/sv_args.min.txt, next to sv_args.txt. gate.py uses it when it exists
(--all-calls uses sv_args.txt). The hand-written calls (instrumentation/recording/synth_calls.txt) are
not in it: gate.py appends them as before.

The blocks file comes from instrumentation/ghidra/ExportBlocks.java (build/lift/blocks.txt). Run this
script again after a new recording, or after a change that adds lifted functions.

Usage:
  python -I instrumentation/lift/min_calls.py [--keep 2] [--only NAME] [--target ded024d]
"""
import argparse
import json
import os
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'instrumentation' / 'lift'))
from gate import load_target  # noqa: E402


def call_blocks(lines):
    """Split sv_args lines into (preamble, [call blocks]). Call j is the j-th "R <rva>" line,
    as in load_calls() of the harness. A block runs to the line before the next R line."""
    pre, blocks = [], []
    for line in lines:
        if line.startswith('R'):
            if len(line.split()) >= 2:
                blocks.append([line])
                continue
            # "R" without an RVA: the harness skips it and does not count it.
        (blocks[-1] if blocks else pre).append(line)
    return pre, blocks


def cover(t, rec, blocks_file, reuse):
    """The cover lines of one recording (saved in <out_dir>/cover/<recording>.txt)."""
    d = t['recordings_dir'] / rec
    saved = t['out_dir'] / 'cover' / f'{rec}.txt'
    if reuse and saved.exists():
        return saved.read_text(encoding='utf-8')
    env = dict(os.environ, HARNESS_COVER=str(blocks_file), SV_UI_WIDTH='220',
               HARNESS_ENV_DIR=str(t['out_dir'] / 'env_cover'))
    p = subprocess.run([str(t['harness']), str(t['image']), str(d / 'sv_snapshot.bin'), str(d / 'sv_args.txt')],
                       capture_output=True, text=True, errors='replace', env=env, cwd=ROOT)
    saved.parent.mkdir(parents=True, exist_ok=True)
    saved.write_text(p.stdout, encoding='utf-8')
    return p.stdout


def minimize(t, rec, keep, blocks_file, seen, reuse):
    d = t['recordings_dir'] / rec
    args = d / 'sv_args.txt'
    chosen = set()
    tested = 0
    paths = set()
    for line in cover(t, rec, blocks_file, reuse).splitlines():
        if not line.startswith('cover '):
            continue
        f = line.split()
        j, rva, path = int(f[1]), f[2], f[3] + ('c' if len(f) > 5 else '')
        tested += 1
        paths.add((rva, path))
        if seen[rva, path] < keep:
            seen[rva, path] += 1
            chosen.add(j)
    if not tested:
        print(f'{rec}: no cover line (see {t["out_dir"] / "cover" / (rec + ".txt")})')
        return
    text = args.read_text(encoding='utf-8', errors='replace').splitlines(keepends=True)
    pre, blocks = call_blocks(text)
    out = pre + [line for j, b in enumerate(blocks) if j in chosen for line in b]
    (d / 'sv_args.min.txt').write_text(''.join(out), encoding='utf-8')
    print(f'{rec}: {tested} tested calls, {len(paths)} paths, kept {len(chosen)} '
          f'({100 * len(chosen) / tested:.0f}%), {len(blocks)} -> {len(chosen)} R lines')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--target', default='ded024d')
    ap.add_argument('--keep', type=int, default=2, help='calls kept for each path (default 2)')
    ap.add_argument('--only', help='only recordings whose name has this text')
    ap.add_argument('--blocks', default=str(ROOT / 'build/lift/blocks.txt'))
    ap.add_argument('--per-recording', action='store_true', help='count the paths of each recording alone')
    ap.add_argument('--reuse', action='store_true', help='use the saved cover output, do not run the harness')
    a = ap.parse_args()
    t = load_target(a.target)
    (t['out_dir'] / 'env_cover').mkdir(parents=True, exist_ok=True)
    seen = defaultdict(int)
    for r in t['recordings']:
        if a.only and a.only not in r['name']:
            continue
        if not (t['recordings_dir'] / r['name'] / 'sv_args.txt').exists():
            continue
        minimize(t, r['name'], a.keep, Path(a.blocks), defaultdict(int) if a.per_recording else seen, a.reuse)
    return 0


if __name__ == '__main__':
    sys.exit(main())
