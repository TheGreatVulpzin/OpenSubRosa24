"""Find adopted client functions that reach data the client does not have a place for.

analysis/client_win/adopted.txt lists the functions of the client that use the clean code of the
server. A clean function reads sr_data_<name>. When the name has no client address
(instrumentation/client/runtime/unplaced.txt), the client build gives the function a private dummy
object: the function runs, but on wrong data. This script follows the calls of each adopted
function through the clean sources and reports each one that reaches such a name.

Usage: python -I tools/re/check_adopted.py [--remove]
--remove writes adopted.txt again without those functions.
"""
import argparse
import glob
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEF = re.compile(r'^\w[^;{}#\n]*?\b(\w+)\([^;{}]*\)\s*\{', re.M)
CALL = re.compile(r'\b([A-Za-z_]\w*)\s*\(')


def clean_functions():
    """name -> body text, for every function of the clean sources."""
    out = {}
    for d in ('src/common', 'src/net', 'src/game'):
        for f in sorted(glob.glob(str(ROOT / d / '*.c'))):
            text = Path(f).read_text(encoding='utf-8')
            marks = list(DEF.finditer(text))
            for i, m in enumerate(marks):
                end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
                out.setdefault(m.group(1), text[m.start():end])
    return out


def aliases():
    """Macro name -> the sr_data names its definition uses (sr_bonds is sr_data_bond)."""
    out = {}
    for f in glob.glob(str(ROOT / 'src/**/*.h'), recursive=True):
        for m in re.finditer(r'^#define\s+(\w+)(?:\([^)]*\))?\s+(.*)$', Path(f).read_text(encoding='utf-8'), re.M):
            used = set(re.findall(r'sr_data_(\w+)', m.group(2)))
            if used and not m.group(1).startswith('sr_data_'):
                out.setdefault(m.group(1), set()).update(used)
    return out


def server_functions():
    """Functions of the server only (src/server): the client does not link them."""
    out = set()
    for f in glob.glob(str(ROOT / 'src/server/*.c')):
        out |= set(DEF.findall(Path(f).read_text(encoding='utf-8')))
    out.discard('main')
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--remove', action='store_true')
    args = ap.parse_args()
    path = ROOT / 'analysis/client_win/adopted.txt'
    lines = path.read_text(encoding='utf-8').splitlines()
    unplaced = set((ROOT / 'instrumentation/client/runtime/unplaced.txt').read_text(encoding='utf-8').split())
    funcs = clean_functions()
    alias = aliases()
    server = server_functions()
    reach = {}

    def uses(name, seen):
        """Unplaced objects that the function reaches."""
        if name in reach:
            return reach[name]
        if name in seen or name not in funcs:
            return set()
        seen.add(name)
        body = funcs[name]
        found = {x for x in re.findall(r'\bsr_data_(\w+)', body) if x in unplaced}
        for word in set(re.findall(r'\b[A-Za-z_]\w*\b', body)):
            found |= alias.get(word, set()) & unplaced
            if word in server and re.search(r'\b' + word + r'\s*\(', body):
                found.add(word + '()')
        for c in set(CALL.findall(body)):
            if c != name and c in funcs:
                found |= uses(c, seen)
        reach[name] = found
        return found

    keep, bad = [], []
    for line in lines:
        if not line.strip() or line.startswith('#'):
            keep.append(line)
            continue
        name = line.split()[0]
        found = uses(name, set())
        if found:
            bad.append((name, sorted(found)))
        else:
            keep.append(line)
    for name, found in bad:
        print(f'{name}: {" ".join(found)}')
    print(f'{len(bad)} adopted functions reach data without a client address or a server only function')
    if args.remove and bad:
        path.write_text('\n'.join(keep) + '\n', encoding='utf-8', newline='\n')


if __name__ == '__main__':
    main()
