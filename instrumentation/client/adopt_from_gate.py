"""Choose the adopted client functions from the gate results.

analysis/client_win/adopted.txt lists the functions of the client that use the clean code of the
server. The gate compares each one with the original on the recorded calls. This script reads the
result files of the last gate run (build/gate/client_win/reset/*.txt) and sorts the listed functions:
  tested     at least one call same in some recording, none different or crashed
  untested   no recorded call reached the function
  failed     a call was different or crashed
By default it only reports. With --keep-tested it writes adopted.txt again with the tested ones
only: a function nobody tested stays lifted until a test covers it.

Usage: python -I instrumentation/client/adopt_from_gate.py [--keep-tested] [--drop-failed]
"""
import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LINE = re.compile(r'^\[\s*\d+/\d+\]\s+(\S+)\s+(\d+) same\s+(\d+) different\s+(\d+) crashed')


def results(folder):
    out = {}
    for f in sorted((ROOT / folder).glob('*.txt')):
        for line in f.read_text(encoding='utf-8', errors='replace').splitlines():
            m = LINE.match(line)
            if m:
                name, same, diff, crash = m.group(1), *map(int, m.groups()[1:])
                r = out.setdefault(name, [0, 0, 0])
                r[0] += same
                r[1] += diff
                r[2] += crash
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dir', default='build/gate/client_win/reset/all',
                    help='result files of the gate (all calls: reset/all, short calls: reset)')
    ap.add_argument('--file', default='analysis/client_win/adopted.txt', help='list of one group (adopted_C1.txt)')
    ap.add_argument('--keep-tested', action='store_true')
    ap.add_argument('--drop-failed', action='store_true')
    args = ap.parse_args()
    path = ROOT / args.file
    lines = path.read_text(encoding='utf-8').splitlines()
    res = results(args.dir)
    kinds = {'tested': [], 'untested': [], 'failed': []}
    keep = []
    for line in lines:
        if not line.strip() or line.startswith('#'):
            keep.append(line)
            continue
        name = line.split()[0]
        same, diff, crash = res.get(name, [0, 0, 0])
        kind = 'failed' if diff or crash else 'tested' if same else 'untested'
        kinds[kind].append(name)
        if kind == 'tested' or (kind == 'untested' and not args.keep_tested) or \
                (kind == 'failed' and not args.drop_failed and not args.keep_tested):
            keep.append(line)
    for kind, names in kinds.items():
        print(f'{kind}: {len(names)}')
        if kind != 'untested':
            print('  ' + ' '.join(names))
    if args.keep_tested:
        # The functions without a test wait here until a recording or a synthetic call reaches them.
        pending = [line for line in lines if line.split()[0] in kinds['untested']] if lines else []
        header = '# Candidates that no recorded call reached. They stay lifted until a test covers them.'
        path.with_name(path.stem + '_pending.txt').write_text(
            '\n'.join([header] + pending) + '\n', encoding='utf-8', newline='\n')
    if args.keep_tested or args.drop_failed:
        path.write_text('\n'.join(keep) + '\n', encoding='utf-8', newline='\n')


if __name__ == '__main__':
    main()
