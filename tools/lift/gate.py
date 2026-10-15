"""Run the harness on every recording and compare each function with a saved baseline.

Usage:
  python -I tools/lift/gate.py                 run, compare, print drops (exit 1 on a drop)
  python -I tools/lift/gate.py --update        the same, then save the results as the baseline
  python -I tools/lift/gate.py --only murilo   only the recordings whose name has this text
  python -I tools/lift/gate.py --from-files    compare the saved outputs again, no harness run

The rule of docs/lift.md: no "calls same" count may drop on any recording. A drop is a
function with fewer same calls than in the baseline, or a function that has calls in the
baseline and no line now. New differences and crashes are also errors.

Recordings: the list in tools/lift/gate_recordings.txt (folders in build/recordings). A
recording marked "synth" also runs with tools/recording/synth_calls.txt appended (the
hand-written calls), as "<name>+synth".
Outputs: build/gate/<name>.txt. Baseline: build/gate/baseline.json (local, like the recordings).
Run one harness at a time: never together with Ghidra or a server.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REC_DIR = ROOT / 'build' / 'recordings'
OUT_DIR = ROOT / 'build' / 'gate'
BASELINE = OUT_DIR / 'baseline.json'
HARNESS = ROOT / 'build' / 'hook' / 'harness.exe'
EXE = ROOT / 'build' / 'hook' / '024d_fixed.exe'
SYNTH = ROOT / 'tools' / 'recording' / 'synth_calls.txt'
LIST = ROOT / 'tools' / 'lift' / 'gate_recordings.txt'

LINE = re.compile(r'^\[ *\d+/\d+\] +(\w+) +(\d+) same +(\d+) different +(\d+) crashed +(\d+) skipped *(.*)$')
TOTAL = re.compile(r'^(\d+) functions tested: (\d+) calls same')


def recordings():
    """(folder, synth) for each line of gate_recordings.txt whose folder has the two files."""
    out = []
    for line in LIST.read_text().splitlines():
        f = line.split('#')[0].split()
        if not f:
            continue
        d = REC_DIR / f[0]
        if not (d / 'sv_snapshot.bin').exists() or not (d / 'sv_args.txt').exists():
            print(f'  {f[0]}: no recording here, skipped')
            continue
        out.append((d, 'synth' in f[1:]))
    return out


def parse(text):
    funcs, total = {}, None
    for line in text.splitlines():
        m = LINE.match(line.strip())
        if m:
            name, same, diff, crash, skipped, note = m.groups()
            funcs[name] = {'same': int(same), 'different': int(diff), 'crashed': int(crash),
                           'skipped': int(skipped), 'note': note.strip()}
            continue
        m = TOTAL.match(line.strip())
        if m:
            total = {'functions': int(m.group(1)), 'same': int(m.group(2))}
    return funcs, total


def run(name, snapshot, args):
    env = dict(os.environ, SV_UI_WIDTH='220')
    t0 = time.time()
    p = subprocess.run([str(HARNESS), str(EXE), str(snapshot), str(args)], capture_output=True, text=True,
                       errors='replace', env=env)
    text = p.stdout + p.stderr
    (OUT_DIR / f'{name}.txt').write_text(text, encoding='utf-8')
    print(f'  {name}: {time.time() - t0:.0f} s', flush=True)
    return text


def compare(name, funcs, base):
    errors = []
    for f, b in sorted(base.items()):
        now = funcs.get(f)
        if now is None:
            if b['same'] > 0:
                errors.append(f'{name}: {f} has no line now (baseline {b["same"]} same)')
        elif now['same'] < b['same']:
            errors.append(f'{name}: {f} {b["same"]} -> {now["same"]} same')
    for f, now in sorted(funcs.items()):
        b = base.get(f, {'different': 0, 'crashed': 0})
        if now['different'] > b['different'] or now['crashed'] > b['crashed']:
            errors.append(f'{name}: {f} {now["different"]} different, {now["crashed"]} crashed {now["note"]}')
    return errors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--update', action='store_true', help='save the results as the new baseline')
    ap.add_argument('--only', help='only recordings whose name contains this text')
    ap.add_argument('--from-files', action='store_true', help='read build/gate/*.txt, do not run the harness')
    ap.add_argument('--no-synth', action='store_true', help='skip the runs with synth_calls.txt')
    a = ap.parse_args()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    baseline = json.loads(BASELINE.read_text()) if BASELINE.exists() else {}
    results, errors = {}, []
    with tempfile.TemporaryDirectory() as tmp:
        for d, synth in recordings():
            if a.only and a.only not in d.name:
                continue
            runs = [(d.name, d / 'sv_args.txt')]
            if synth and not a.no_synth and SYNTH.exists():
                merged = Path(tmp) / f'{d.name}.txt'
                merged.write_bytes((d / 'sv_args.txt').read_bytes() + b'\n' + SYNTH.read_bytes())
                runs.append((d.name + '+synth', merged))
            for name, args in runs:
                if a.from_files:
                    f = OUT_DIR / f'{name}.txt'
                    if not f.exists():
                        continue
                    text = f.read_text(encoding='utf-8')
                else:
                    text = run(name, d / 'sv_snapshot.bin', args)
                funcs, total = parse(text)
                if total is None:
                    errors.append(f'{name}: no summary line (harness failed?)')
                    continue
                results[name] = {'total': total, 'funcs': funcs}
                old = baseline.get(name)
                errors += compare(name, funcs, old['funcs']) if old else []
                was = f' (baseline {old["total"]["same"]})' if old else ' (no baseline)'
                print(f'{name}: {total["functions"]} functions, {total["same"]} same{was}')

    for e in errors:
        print('DROP ' + e)
    if errors:
        print(f'{len(errors)} problem(s): baseline not updated')
        return 1
    if a.update:
        baseline.update(results)
        BASELINE.write_text(json.dumps(baseline, indent=1, sort_keys=True))
        print(f'baseline updated: {len(results)} run(s)')
    print('OK: no count dropped')
    return 0


if __name__ == '__main__':
    sys.exit(main())
