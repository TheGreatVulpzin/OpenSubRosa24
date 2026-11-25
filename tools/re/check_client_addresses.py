"""Check that the client adapter keeps the address of each name.

Usage: python -I tools/re/check_client_addresses.py [git revision, default HEAD]

Run tools/re/gen_client_storage.py first. The tool reads the generated
instrumentation/client/runtime/storage_adapter.h and the same file at the git revision.
It finds the client RVA of each sr_data_ and cl_data_ name in both files. A name of the form
sr_data_<object>.<field> gets the RVA of the object plus the field offset from the static
asserts of src/include. Each name in both files must have the same RVA.

A name that has an RVA only in the old file must be a dropped row of storage.csv.
The exit code is 1 on a different RVA, a lost address or a name that the tool cannot resolve.
"""
import csv
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ADAPTER = 'instrumentation/client/runtime/storage_adapter.h'
DEFINE = re.compile(r'#define ((?:sr|cl)_data_\w+) (.+)$')
AT_RVA = re.compile(r'^\(?\*?\(\((\w+) \*\)\(sr_base \+ (0x[0-9a-f]+)u\)\)\)?$')
MEMBER = re.compile(r'^((?:sr|cl)_data_\w+)\.(\w+)$')


def field_offsets():
    """Offsets of fields from the offsetof static asserts of the type headers."""
    offsets = {}
    for path in (ROOT / 'src/include').glob('*.h'):
        text = path.read_text(encoding='utf-8', errors='replace')
        for m in re.finditer(r'offsetof\((\w+), (\w+)\) == (0x[0-9a-fA-F]+|\d+)', text):
            offsets[(m.group(1), m.group(2))] = int(m.group(3), 0)
    return offsets


def addresses(text, offsets):
    """Return {name: rva or None} and the names the tool cannot resolve."""
    found, types, members = {}, {}, {}
    for line in text.splitlines():
        m = DEFINE.match(line)
        if not m:
            continue
        name, expr = m.groups()
        at = AT_RVA.match(expr)
        member = MEMBER.match(expr)
        if at:
            types[name] = at.group(1)
            found[name] = int(at.group(2), 16)
        elif member:
            members[name] = member.groups()
        else:
            found[name] = None
    bad = []
    for name, (base, field) in members.items():
        offset = offsets.get((types.get(base), field))
        if found.get(base) is None or offset is None:
            bad.append(name)
            found[name] = None
        else:
            found[name] = found[base] + offset
    return found, bad


def main():
    rev = sys.argv[1] if len(sys.argv) > 1 else 'HEAD'
    old_text = subprocess.run(['git', 'show', f'{rev}:{ADAPTER}'], cwd=ROOT, capture_output=True, text=True,
                              check=True, encoding='utf-8').stdout
    new_text = (ROOT / ADAPTER).read_text(encoding='utf-8')
    offsets = field_offsets()
    old, old_bad = addresses(old_text, offsets)
    new, new_bad = addresses(new_text, offsets)
    dropped = {f'sr_data_{r["name"]}' for r in csv.DictReader((ROOT / 'analysis/client_win/storage.csv').open(encoding='utf-8'))
               if r['status'] == 'dropped'}
    problems = [f'cannot resolve {n} ({rev})' for n in old_bad] + [f'cannot resolve {n} (new)' for n in new_bad]
    same = 0
    for name in sorted(set(old) & set(new)):
        a, b = old[name], new[name]
        if a is not None and b is not None:
            if a != b:
                problems.append(f'{name}: {a:#x} in {rev}, {b:#x} now')
            else:
                same += 1
        elif a is not None and name not in dropped:
            problems.append(f'{name}: {a:#x} in {rev}, no address now')
    gone = sorted(n for n in set(old) - set(new) if old[n] is not None)
    added = sorted(n for n in set(new) - set(old) if new[n] is not None)
    for name in gone:
        print(f'only in {rev}: {name} {old[name]:#x}')
    for name in added:
        print(f'new: {name} {new[name]:#x}')
    for line in problems:
        print('ERROR', line)
    print(f'{same} names keep their RVA, {len(gone)} only in {rev}, {len(added)} new, {len(problems)} error(s)')
    sys.exit(1 if problems else 0)


if __name__ == '__main__':
    main()
