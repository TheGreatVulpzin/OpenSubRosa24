"""Run the harness on every recording of a target and compare each function with a baseline.

Usage:
  python -I tools/lift/gate.py                      full run of the default target (ded024d)
  python -I tools/lift/gate.py --changed            only the functions whose lifted C changed
  python -I tools/lift/gate.py --jobs 3             three harnesses at the same time
  python -I tools/lift/gate.py --update             save the results as the new baseline
  python -I tools/lift/gate.py --only murilo        only the recordings whose name has this text
  python -I tools/lift/gate.py --funcs a,b          only these functions
  python -I tools/lift/gate.py --from-files         compare the saved outputs again, no harness run
  python -I tools/lift/gate.py --target client      another target (tools/lift/targets/<name>.json)

The rule of docs/lift.md: no "calls same" count may drop on any recording. A drop is a
function with fewer same calls than in the baseline, or a function that has calls in the
baseline and no line now. New differences and crashes are also errors.

Target file (tools/lift/targets/<name>.json): the harness, the image it loads, the lifted C
and its table, the recordings (folders with sv_snapshot.bin and sv_args.txt; "synth": true
also runs the recording with the hand-written calls appended), the output folder, and
"full_run_paths": files that every function depends on. The client or another game version
gets its own target file; this script does not change.

--changed: the harness calls other game functions through the image (the original code),
so a function whose lifted C did not change gives the same results. The script compares the
lifted C with the version in git (--ref, default HEAD), runs only the changed functions and
keeps the baseline values of the others. A change in a "full_run_paths" file runs everything.
Run a full gate before a push and after a change of many functions.

Outputs: <out_dir>/<run>.txt and <run>.log. Baseline: <out_dir>/baseline.json (local, like
the recordings). Never run a gate together with Ghidra or a server.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TARGETS = ROOT / 'tools' / 'lift' / 'targets'

LINE = re.compile(r'^\[ *\d+/\d+\] +(\w+) +(\d+) same +(\d+) different +(\d+) crashed +(\d+) skipped *(.*)$')
TOTAL = re.compile(r'^(\d+) functions tested: (\d+) calls same')
BLOCK = re.compile(r'^/\* -+ 0x([0-9a-f]+) \*/$', re.M)
NAME = re.compile(r'^\s*\{ "(\w+)", 0x([0-9a-f]+),', re.M)


def load_target(name):
    t = json.loads((TARGETS / f'{name}.json').read_text(encoding='utf-8'))
    for k in ('harness', 'image', 'lifted_c', 'table', 'recordings_dir', 'synth', 'out_dir'):
        if t.get(k):
            t[k] = ROOT / t[k]
    return t


def git_show(ref, path):
    rel = path.relative_to(ROOT).as_posix()
    p = subprocess.run(['git', 'show', f'{ref}:{rel}'], cwd=ROOT, capture_output=True)
    return p.stdout.decode('utf-8', 'replace') if p.returncode == 0 else None


def blocks(text):
    marks = list(BLOCK.finditer(text))
    out = {}
    for i, m in enumerate(marks):
        end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
        out[int(m.group(1), 16)] = text[m.start():end].replace('\r\n', '\n')
    return out


def changed_functions(t, ref):
    """(names, reason): the functions to run, or (None, reason) for a full run."""
    p = subprocess.run(['git', 'diff', '--name-only', ref, '--'] + t['full_run_paths'], cwd=ROOT,
                       capture_output=True, text=True)
    if p.returncode != 0:
        return None, f'git diff failed: {p.stderr.strip()}'
    if p.stdout.strip():
        return None, 'changed: ' + ', '.join(p.stdout.split())
    old = git_show(ref, t['lifted_c'])
    if old is None:
        return None, f'{t["lifted_c"].name} is not in {ref}'
    names = {int(r, 16): n for n, r in NAME.findall(t['table'].read_text(encoding='utf-8'))}
    a, b = blocks(old), blocks(t['lifted_c'].read_text(encoding='utf-8'))
    rvas = {r for r in set(a) | set(b) if a.get(r) != b.get(r)}
    return sorted(names.get(r, f'0x{r:x}') for r in rvas), f'{len(rvas)} function(s) changed since {ref}'


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


def compare(name, funcs, base, only):
    errors = []
    for f, b in sorted(base.items()):
        if only is not None and f not in only:
            continue
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
    ap.add_argument('--target', default='ded024d', help='tools/lift/targets/<name>.json')
    ap.add_argument('--update', action='store_true', help='save the results as the new baseline')
    ap.add_argument('--only', help='only recordings whose name contains this text')
    ap.add_argument('--funcs', help='only these functions (names separated by commas)')
    ap.add_argument('--changed', action='store_true', help='only the functions whose lifted C changed')
    ap.add_argument('--ref', default='HEAD', help='git version for --changed (default HEAD)')
    ap.add_argument('--jobs', type=int, default=2, help='harness runs at the same time (default 2)')
    ap.add_argument('--from-files', action='store_true', help='read the saved outputs, do not run the harness')
    ap.add_argument('--no-synth', action='store_true', help='skip the runs with the hand-written calls')
    a = ap.parse_args()

    t = load_target(a.target)
    out = t['out_dir']
    out.mkdir(parents=True, exist_ok=True)
    baseline_file = out / 'baseline.json'
    baseline = json.loads(baseline_file.read_text()) if baseline_file.exists() else {}

    only = None
    if a.funcs:
        only = a.funcs.split(',')
    elif a.changed:
        only, why = changed_functions(t, a.ref)
        print(f'--changed: {why}' + ('' if only is None else f': {", ".join(only) or "nothing to run"}'))
        if only is not None and not baseline:
            print('--changed needs a baseline: run a full gate first')
            return 1
        if only == []:
            print('OK: no lifted function changed')
            return 0

    runs = []  # (run name, recording folder, args file)
    tmp = tempfile.TemporaryDirectory()
    for r in t['recordings']:
        d = t['recordings_dir'] / r['name']
        if a.only and a.only not in r['name']:
            continue
        if not (d / 'sv_snapshot.bin').exists() or not (d / 'sv_args.txt').exists():
            print(f'  {r["name"]}: no recording here, skipped')
            continue
        runs.append((r['name'], d, d / 'sv_args.txt'))
        if r.get('synth') and not a.no_synth and t.get('synth') and t['synth'].exists():
            merged = Path(tmp.name) / f'{r["name"]}.txt'
            merged.write_bytes((d / 'sv_args.txt').read_bytes() + b'\n' + t['synth'].read_bytes())
            runs.append((r['name'] + '+synth', d, merged))

    filter_arg = []
    if only is not None:
        lst = Path(tmp.name) / 'funcs.txt'
        lst.write_text('\n'.join(only) + '\n')
        filter_arg = ['@' + str(lst)]

    def run(job):
        k, (name, d, args) = job
        if a.from_files:
            f = out / f'{name}.txt'
            return name, f.read_text(encoding='utf-8') if f.exists() else '', 0.0
        env = dict(os.environ, SV_UI_WIDTH='220', HARNESS_ENV_DIR=str(out / f'env{k}'),
                   SV_LOG_FILE=str(out / f'{name}.log'))
        (out / f'{name}.log').unlink(missing_ok=True)
        t0 = time.time()
        p = subprocess.run([str(t['harness']), str(t['image']), str(d / 'sv_snapshot.bin'), str(args)] + filter_arg,
                           capture_output=True, text=True, errors='replace', env=env, cwd=ROOT)
        text = p.stdout + p.stderr
        (out / f'{name}.txt').write_text(text, encoding='utf-8')
        return name, text, time.time() - t0

    t0 = time.time()
    jobs = max(1, a.jobs)
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        # Each worker slot gets its own sandbox folder (env0, env1...): job k uses k % jobs.
        done = list(pool.map(run, [(i % jobs, r) for i, r in enumerate(runs)]))
    tmp.cleanup()

    results, errors = {}, []
    for name, text, secs in done:
        funcs, total = parse(text)
        if total is None:
            errors.append(f'{name}: no summary line (harness failed? see {out / (name + ".txt")})')
            continue
        old = baseline.get(name)
        errors += compare(name, funcs, old['funcs'], only) if old else []
        if only is not None and old:
            merged = dict(old['funcs'])
            merged.update(funcs)
            funcs = merged
            total = {'functions': len([f for f in funcs.values() if f['same'] or f['different'] or f['crashed']]),
                     'same': sum(f['same'] for f in funcs.values())}
        results[name] = {'total': total, 'funcs': funcs}
        was = f' (baseline {old["total"]["same"]})' if old else ' (no baseline)'
        took = f', {secs:.0f} s' if secs else ''
        print(f'{name}: {total["functions"]} functions, {total["same"]} same{was}{took}')
    print(f'{len(done)} run(s) in {time.time() - t0:.0f} s, {jobs} at a time')

    for e in errors:
        print('DROP ' + e)
    if errors:
        print(f'{len(errors)} problem(s): baseline not updated')
        return 1
    if a.update:
        baseline.update(results)
        baseline_file.write_text(json.dumps(baseline, indent=1, sort_keys=True))
        print(f'baseline updated: {len(results)} run(s)')
    print('OK: no count dropped')
    return 0


if __name__ == '__main__':
    sys.exit(main())
