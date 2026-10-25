"""Run the harness on every recording of a target and compare each function with a baseline.

Usage:
  python -I instrumentation/lift/gate.py                      full run of the default target (ded024d)
  python -I instrumentation/lift/gate.py --changed            only the functions whose lifted C changed
  python -I instrumentation/lift/gate.py --quick              only the fewest recordings that test every function
  python -I instrumentation/lift/gate.py --all-calls          all recorded calls, not the short sv_args.min.txt
  python -I instrumentation/lift/gate.py --jobs 3             three harnesses at the same time
  python -I instrumentation/lift/gate.py --update             save the results as the new baseline
  python -I instrumentation/lift/gate.py --only murilo        only the recordings whose name has this text
  python -I instrumentation/lift/gate.py --funcs a,b          only these functions
  python -I instrumentation/lift/gate.py --from-files         compare the saved outputs again, no harness run
  python -I instrumentation/lift/gate.py --target client      another target (instrumentation/lift/targets/<name>.json)
  python -I instrumentation/lift/gate.py --reset              each function starts from the snapshot

The rule of docs/lift.md: no "calls same" count may drop on any recording. A drop is a
function with fewer same calls than in the baseline, or a function that has calls in the
baseline and no line now. New differences and crashes are also errors.

Target file (instrumentation/lift/targets/<name>.json): the harness, the image it loads, the lifted C
and its table, the recordings (folders with sv_snapshot.bin and sv_args.txt; "synth": true
also runs the recording with the hand-written calls appended), the output folder, and
"full_run_paths": files that every function depends on. The client or another game version
gets its own target file; this script does not change.

--changed: the harness calls other game functions through the image (the original code),
so a function whose lifted C did not change gives the same results. The script compares the
lifted C with the version in git (--ref, default HEAD), runs only the changed functions and
keeps the baseline values of the others. A change in a "full_run_paths" file runs everything.
Run a full gate before a push and after a change of many functions.

--quick: the fewest runs that together test every function of the baseline (a greedy set
cover: each step takes the run that adds the most functions for each second of run time).
The set follows the baseline, so a new recording joins it by itself when it tests a new
function. Without a baseline, the recordings with "quick" in the target are used. Use it
with --changed while you work.

Short calls: instrumentation/lift/min_calls.py writes <recording>/sv_args.min.txt with a few calls
for each code path of each function. The gate uses it when it exists. --all-calls runs
every recorded call. It has its own outputs and baseline in <out_dir>/all. Run it now and
then (for example once a week), not for each change.

--reset (or "reset": true in the target): the harness puts the snapshot back before each
function (HARNESS_RESET=1). Without it, the writes of the original runs stay, and a function
can fail because of the heap that earlier functions changed. With it, the counts do not
depend on the order, and --changed gives the same counts as a full run. The reset mode has
its own outputs and baseline in <out_dir>/reset.

Outputs: <out_dir>/<run>.txt and <run>.log. Baseline: <out_dir>/baseline.json (local, like
the recordings). Never run a gate together with Ghidra or a server.
"""
import argparse
import json
import os
import queue
import re
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TARGETS = ROOT / 'instrumentation' / 'lift' / 'targets'

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


def rename_baseline(baseline, pairs):
    renamed = {run: dict(result, funcs=dict(result['funcs'])) for run, result in baseline.items()}
    for pair in pairs:
        names = pair.split(':')
        if len(names) != 2 or not all(names) or names[0] == names[1]:
            raise ValueError('Use OLD:NEW for each baseline rename')
        old, new = names
        found = False
        for run, result in renamed.items():
            funcs = result['funcs']
            if old in funcs:
                if new in funcs:
                    raise ValueError(f'{run}: both {old} and {new} exist')
                funcs[new] = funcs.pop(old)
                found = True
        if not found and not any(new in result['funcs'] for result in renamed.values()):
            raise ValueError(f'No baseline function named {old}')
    return renamed


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


def quick_runs(baseline, names):
    """The fewest runs that test every function that any run tests (greedy set cover by
    new functions for each second). None when a run has no baseline."""
    if not names or any(n not in baseline for n in names):
        return None
    cover = {n: {f for f, v in baseline[n]['funcs'].items() if v['same']} for n in names}
    secs = {n: max(baseline[n].get('secs', 0), 1) for n in names}
    want = set().union(*cover.values())
    chosen, got = [], set()
    while got != want:
        n = max((n for n in names if n not in chosen), key=lambda n: len(cover[n] - got) / secs[n])
        chosen.append(n)
        got |= cover[n]
    return set(chosen)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--target', default='ded024d', help='instrumentation/lift/targets/<name>.json')
    ap.add_argument('--update', action='store_true', help='save the results as the new baseline')
    ap.add_argument('--rename-baseline', action='append', default=[], metavar='OLD:NEW',
                    help='rename a baseline function key before the count checks. Requires --update')
    ap.add_argument('--only', help='only recordings whose name contains this text')
    ap.add_argument('--funcs', help='only these functions (names separated by commas)')
    ap.add_argument('--changed', action='store_true', help='only the functions whose lifted C changed')
    ap.add_argument('--ref', default='HEAD', help='git version for --changed (default HEAD)')
    ap.add_argument('--jobs', type=int, default=2, help='harness runs at the same time (default 2)')
    ap.add_argument('--from-files', action='store_true', help='read the saved outputs, do not run the harness')
    ap.add_argument('--quick', action='store_true', help='only the fewest runs that test every function')
    ap.add_argument('--all-calls', action='store_true', help='all recorded calls (not sv_args.min.txt)')
    ap.add_argument('--no-synth', action='store_true', help='skip the runs with the hand-written calls')
    ap.add_argument('--reset', action='store_true', help='each function starts from the snapshot (HARNESS_RESET=1)')
    a = ap.parse_args()

    t = load_target(a.target)
    reset = a.reset or t.get('reset', False)
    out = t['out_dir'] / 'reset' if reset else t['out_dir']
    if a.all_calls:
        out = out / 'all'
    out.mkdir(parents=True, exist_ok=True)
    baseline_file = out / 'baseline.json'
    baseline = json.loads(baseline_file.read_text()) if baseline_file.exists() else {}
    if a.rename_baseline:
        if not a.update:
            ap.error('--rename-baseline requires --update')
        try:
            baseline = rename_baseline(baseline, a.rename_baseline)
        except ValueError as error:
            ap.error(str(error))

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
        args = d / 'sv_args.txt'
        if not a.all_calls and (d / 'sv_args.min.txt').exists():
            args = d / 'sv_args.min.txt'
        runs.append((r['name'], d, args, r.get('quick') is True))
        if r.get('synth') and not a.no_synth and t.get('synth') and t['synth'].exists():
            merged = Path(tmp.name) / f'{r["name"]}.txt'
            merged.write_bytes(args.read_bytes() + b'\n' + t['synth'].read_bytes())
            runs.append((r['name'] + '+synth', d, merged, r.get('quick') == 'synth'))
    if a.quick:
        keep = quick_runs(baseline, [x[0] for x in runs])
        if keep is None:
            print('--quick: no baseline for every run, using the "quick" marks of the target')
            keep = {x[0] for x in runs if x[3]}
        runs = [x for x in runs if x[0] in keep]
        print(f'--quick: {", ".join(x[0] for x in runs)}')
    runs = [x[:3] for x in runs]

    filter_arg = []
    if only is not None:
        lst = Path(tmp.name) / 'funcs.txt'
        lst.write_text('\n'.join(only) + '\n')
        filter_arg = ['@' + str(lst)]

    jobs = max(1, a.jobs)
    # Free sandbox folders (env0, env1...). A run takes one and gives it back, so two runs
    # at the same time never share a folder: the lifted run of one would see the files of the
    # other (saveaccountsserver got "fopen user\server.srk -> null").
    slots = queue.Queue()
    for k in range(jobs):
        slots.put(k)

    def run(job):
        name, d, args = job
        if a.from_files:
            f = out / f'{name}.txt'
            return name, f.read_text(encoding='utf-8') if f.exists() else '', 0.0
        k = slots.get()
        env = dict(os.environ, SV_UI_WIDTH='220', HARNESS_ENV_DIR=str(out / f'env{k}'),
                   SV_LOG_FILE=str(out / f'{name}.log'), HARNESS_RESET='1' if reset else '0')
        (out / f'{name}.log').unlink(missing_ok=True)
        t0 = time.time()
        try:
            p = subprocess.run([str(t['harness']), str(t['image']), str(d / 'sv_snapshot.bin'), str(args)] + filter_arg,
                               capture_output=True, text=True, errors='replace', env=env, cwd=ROOT)
        finally:
            slots.put(k)
        text = p.stdout + p.stderr
        if p.returncode not in (0, 3):  # 3: differences found (the summary says which)
            text += f'\nharness exit code {p.returncode:#x}\n'
        (out / f'{name}.txt').write_text(text, encoding='utf-8')
        return name, text, time.time() - t0

    t0 = time.time()
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        done = list(pool.map(run, runs))
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
        results[name] = {'total': total, 'funcs': funcs, 'secs': round(secs) or (old or {}).get('secs', 0)}
        was = f' (baseline {old["total"]["same"]})' if old else ' (no baseline)'
        took = f', {secs:.0f} s' if secs else ''
        print(f'{name}: {total["functions"]} functions, {total["same"]} same{was}{took}')
    print(f'{len(done)} run(s) in {time.time() - t0:.0f} s, {jobs} at a time' + (', reset mode' if reset else ''))

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
