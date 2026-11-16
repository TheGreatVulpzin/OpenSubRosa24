"""Hash of the clean code that one adopted function runs.

The hash covers every clean source file that holds a function that the adopted function reaches
(directly or through other clean functions), and all clean headers. A test of the function gives
the same result while the hash is the same, so the gate and the synthetic cases can skip it.

Usage: python -I tools/re/clean_closure.py [name ...]    print the hash of each function
Library: hashes(names) -> {name: hex}, adopted_names() -> list of the adopted functions.
"""
import glob
import hashlib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEF = re.compile(r'^\w[^;{}#\n]*?\b(\w+)\([^;{}]*\)\s*\{', re.M)
CALL = re.compile(r'\b([A-Za-z_]\w*)\s*\(')
SOURCE_DIRS = ('src/common', 'src/net', 'src/game', 'src/client')
CONTEXT_FILES = ('instrumentation/client/runtime/storage_adapter.h', 'src/include')


def _index():
    """name -> (file, body) for each clean function, and file -> text."""
    funcs, texts = {}, {}
    for d in SOURCE_DIRS:
        for f in sorted(glob.glob(str(ROOT / d / '**/*.c'), recursive=True)):
            text = Path(f).read_text(encoding='utf-8')
            texts[f] = text
            marks = list(DEF.finditer(text))
            for i, m in enumerate(marks):
                end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
                funcs.setdefault(m.group(1), (f, text[m.start():end]))
    return funcs, texts


def _context_hash():
    """One hash for the headers and the storage adapter: a change there touches every function."""
    h = hashlib.sha256()
    paths = []
    for item in CONTEXT_FILES:
        p = ROOT / item
        if p.is_dir():
            paths += sorted(p.rglob('*.h'))
        elif p.exists():
            paths.append(p)
    for d in SOURCE_DIRS:
        paths += sorted((ROOT / d).rglob('*.h'))
    for p in sorted(set(paths)):
        h.update(p.relative_to(ROOT).as_posix().encode())
        h.update(p.read_bytes().replace(b'\r\n', b'\n'))
    return h.hexdigest()


def adopted_names():
    """The adopted functions: every analysis/client_win/adopted*.txt except the pending list."""
    names = []
    for p in sorted((ROOT / 'analysis/client_win').glob('adopted*.txt')):
        if p.name.endswith('_pending.txt'):
            continue
        for line in p.read_text(encoding='utf-8').splitlines():
            if line.strip() and not line.startswith('#'):
                names.append(line.split()[0])
    return names


def hashes(names=None):
    """name -> hash of the files that the function reaches, plus the context hash."""
    funcs, texts = _index()
    context = _context_hash()
    out = {}
    for name in (names if names is not None else adopted_names()):
        seen, todo, files = set(), [name], set()
        while todo:
            n = todo.pop()
            if n in seen or n not in funcs:
                continue
            seen.add(n)
            f, body = funcs[n]
            files.add(f)
            todo.extend(c for c in set(CALL.findall(body)) if c != n)
        h = hashlib.sha256(context.encode())
        for f in sorted(files):
            h.update(Path(f).relative_to(ROOT).as_posix().encode())
            h.update(texts[f].replace('\r\n', '\n').encode('utf-8'))
        out[name] = h.hexdigest()
    return out


if __name__ == '__main__':
    result = hashes(sys.argv[1:] or None)
    for k, v in result.items():
        print(f'{k} {v[:16]}')
    print(f'{len(result)} function(s)')
