"""Pair the globals of the server and the client through functions they share.

Both executables hold the same game code. For a function that has the same name in both, the
lifted C lists the globals in order of first use. The i-th global of the server function is the
i-th global of the client function when the two lists have the same length. Each pair is a vote.
A server global inside a known storage object (name, rva, size) turns the vote into a client
address of that object.

Usage: python -I tools/re/pair_client_globals.py [--server-rev 551941a] [--out analysis/client_win/storage.csv]
The server C is an old version from git history (the file in src/lifted is empty now).
Output: one row for each storage object of the server (tools/re/gen_storage.py) that the votes
place in the client: name, type, count, size, client RVA, votes, total, status.
Status: ok (3 votes or more, twice the next address), weak (fewer votes), checked (an independent
CSV agrees), conflict (another CSV names another address, the votes win), other (the other CSV wins, the votes are weak), dropped (overlaps an object with more votes).
"""
import argparse
import csv
import re
import runpy
import subprocess
from collections import Counter, defaultdict
from difflib import SequenceMatcher
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BLOCK = re.compile(r'^/\* -+ 0x([0-9a-f]+) \*/$', re.M)
DEFINE = re.compile(r'^#define ([A-Za-z_0-9]+) \(\*\((.+?) \*\)SR_VA\((0x[0-9a-f]+)\)\)$', re.M)
ADDR = re.compile(r'^#define ([A-Za-z_0-9]+) \(\(.+?\)SR_VA\((0x[0-9a-f]+)\)\)$', re.M)


def blocks(path):
    """Return {rva: text of the block}."""
    text = Path(path).read_text(errors='replace')
    marks = list(BLOCK.finditer(text))
    out = {}
    for i, m in enumerate(marks):
        end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
        out[int(m.group(1), 16)] = text[m.end():end]
    return out


LOCAL = re.compile(r'^(?:[a-z]{1,3}Var\d+|local_\w+|param_\d+|in_\w+|extraout_\w+|unaff_\w+|_?[A-Z][A-Za-z]*_\w+_\d+)$')


def tokens(block):
    """Tokens of the body. A global is ('G', virtual address), other names lose their identity."""
    defs = {}
    for m in DEFINE.finditer(block):
        defs[m.group(1)] = (int(m.group(3), 16), m.group(2))
    body = re.sub(r'^#(define|undef) .*$', '', block, flags=re.M)
    out = []
    for m in re.finditer(r'[A-Za-z_][A-Za-z_0-9]*|0x[0-9a-fA-F]+|\d+(?:\.\d+)?f?|\S', body):
        t = m.group(0)
        if t in defs:
            out.append(('G', defs[t]))
        elif t.startswith('FUN_') or t.startswith('lf_') or LOCAL.match(t):
            out.append(('V', t[:2]))
        else:
            out.append(('T', t))
    return out


def pairs(a, b):
    """Pairs of globals at the same place in the equal parts of two bodies."""
    ka = [t[0] + (str(t[1][1]) if t[0] == 'G' else str(t[1])) for t in tokens_key(a)]
    kb = [t[0] + (str(t[1][1]) if t[0] == 'G' else str(t[1])) for t in tokens_key(b)]
    sm = SequenceMatcher(None, ka, kb, autojunk=False)
    ta, tb = a, b
    out = []
    for i, j, n in sm.get_matching_blocks():
        for k in range(n):
            if ta[i + k][0] == 'G':
                out.append((ta[i + k][1][0], tb[j + k][1][0]))
    return out


def tokens_key(t):
    return t


def names(path):
    out = {}
    for r in csv.DictReader(open(path, encoding='utf-8')):
        if r['name']:
            out[r['name']] = int(r['rva'], 16)
    return out


def storage_rows():
    return runpy.run_path(str(ROOT / 'tools/re/gen_storage.py'))['schema']()


def independent():
    """Client globals that other evidence named: {rva: name}."""
    out = {}
    for f in ('globals_review', 'globals_checked'):
        for r in csv.DictReader(open(ROOT / f'analysis/client_win/{f}.csv', encoding='utf-8')):
            if r['status'] in ('confirmed', 'likely'):
                out[r['name']] = int(r['rva'], 16)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--server-rev', default='551941a', help='git revision with the full lifted C of the server')
    ap.add_argument('--server-c', help='file instead of the git revision')
    ap.add_argument('--client-c', default=str(ROOT / 'src/lifted/client/lifted_client.c'))
    ap.add_argument('--out', default=str(ROOT / 'analysis/client_win/storage.csv'))
    ap.add_argument('--votes', help='also write the raw pair votes here')
    args = ap.parse_args()
    if args.server_c:
        server_c = args.server_c
    else:
        server_c = ROOT / 'build/g72_server_lifted.c'
        server_c.parent.mkdir(exist_ok=True)
        server_c.write_bytes(subprocess.check_output(
            ['git', 'show', f'{args.server_rev}:src/lifted/lifted_024d.c'], cwd=ROOT))
    sn = names(ROOT / 'analysis/024d/functions.csv')
    cn = names(ROOT / 'analysis/client_win/functions.csv')
    sb, cb = blocks(server_c), blocks(args.client_c)
    votes = defaultdict(Counter)
    where = defaultdict(set)
    used = same = 0
    for name, srva in sn.items():
        crva = cn.get(name)
        if crva is None or srva not in sb or crva not in cb:
            continue
        used += 1
        ta, tb = tokens(sb[srva]), tokens(cb[crva])
        if len(ta) > 6000 or len(tb) > 6000:
            continue
        got = pairs(ta, tb)
        if got:
            same += 1
        for sv, cv in set(got):
            votes[sv][cv] += 1
            where[(sv, cv)].add(name)
    if args.votes:
        with open(args.votes, 'w', newline='', encoding='utf-8') as fh:
            w = csv.writer(fh)
            w.writerow(['server_va', 'client_va', 'votes', 'against', 'functions'])
            for sv in sorted(votes):
                total = sum(votes[sv].values())
                for cv, n in votes[sv].most_common():
                    w.writerow([hex(sv), hex(cv), n, total - n, ' '.join(sorted(where[(sv, cv)])[:4])])
    # One vote for each storage object: the client address of the base.
    rows = storage_rows()
    base_votes = defaultdict(Counter)
    for sv, c in votes.items():
        for a, name, typ, count, size in rows:
            va = a + 0x400000
            if va <= sv < va + count * size:
                for cv, n in c.items():
                    base_votes[name][cv - (sv - va)] += n
                break
    indep = independent()
    result = []
    for a, name, typ, count, size in rows:
        c = base_votes.get(name)
        if not c:
            continue
        best = c.most_common(2)
        top, n = best[0]
        second = best[1][1] if len(best) > 1 else 0
        total = sum(c.values())
        status = 'ok' if n >= 3 and n >= 2 * second else 'weak'
        ind = indep.get(name)
        if ind is not None and ind + 0x400000 == top:
            status = 'checked'
        elif ind is not None:
            print(f'conflict {name}: votes {top - 0x400000:#x} ({n} of {total}), other evidence {ind:#x}')
            if status != 'ok':
                top, status = ind + 0x400000, 'other'
            else:
                status = 'conflict'
        result.append([name, typ, count, size, top - 0x400000, n, total, status])
    # Two objects that overlap cannot both be right. The one with fewer votes is dropped.
    result.sort(key=lambda r: r[4])
    changed = True
    while changed:
        changed = False
        live = [r for r in result if r[7] != 'dropped']
        for r, nxt in zip(live, live[1:]):
            if r[4] + r[2] * r[3] > nxt[4]:
                loser = r if (r[7] != 'checked' and (nxt[7] == 'checked' or r[5] < nxt[5])) else nxt
                loser[7] = 'dropped'
                changed = True
                break
    with open(args.out, 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh)
        w.writerow(['name', 'type', 'count', 'size', 'client_rva', 'votes', 'total', 'status'])
        for r in result:
            w.writerow(r[:4] + [hex(r[4])] + r[5:])
    counts = Counter(r[7] for r in result)
    print(f'{used} shared functions, {same} paired, {len(votes)} server globals, '
          f'{len(result)} of {len(rows)} storage objects placed: {dict(counts)}')


if __name__ == '__main__':
    main()
