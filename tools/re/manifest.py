"""Write/verify SHA-256 manifest of the original (untracked) game files.

Usage: python -I tools/re/manifest.py write|check
"""
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / 'original'
MANIFEST = ROOT / 'MANIFEST.sha256'
SKIP = {'MANIFEST.sha256', 'README.md'}


def files():
    for p in sorted(ROOT.rglob('*')):
        if p.is_file() and p.name not in SKIP:
            yield p


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


if sys.argv[1:] == ['write']:
    lines = [f'{digest(p)}  {p.relative_to(ROOT).as_posix()}' for p in files()]
    MANIFEST.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f'{len(lines)} files')
elif sys.argv[1:] == ['check']:
    bad = 0
    for line in MANIFEST.read_text(encoding='utf-8').splitlines():
        h, rel = line.split('  ', 1)
        p = ROOT / rel
        if not p.exists() or digest(p) != h:
            print('MISMATCH', rel)
            bad += 1
    print('OK' if not bad else f'{bad} mismatches')
    sys.exit(1 if bad else 0)
else:
    sys.exit(__doc__)
