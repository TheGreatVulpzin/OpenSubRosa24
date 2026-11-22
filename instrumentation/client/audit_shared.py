"""Collect shared client functions and their storage and call dependencies."""
import csv
import json
import re
import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]

def body(text, name):
    match = re.search(r'^\w[^\n;{}=]*?\b' + re.escape(name) + r'\s*\([^;{]*\)\s*\{', text, re.M)
    if not match:
        return ''
    start = text.index('{', match.start())
    depth, end = 1, start + 1
    while depth and end < len(text):
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]

def main():
    out = ROOT / 'build/agent-k/audit17'
    out.mkdir(parents=True, exist_ok=True)
    reference = subprocess.check_output(['git', 'show', '73f63c4:src/lifted/client/lifted_client.c'], cwd=ROOT).decode('utf-8')
    (out / 'reference.c').write_text(reference, encoding='utf-8')
    chunks = re.split(r'^/\* -+ (0x[0-9a-f]+) \*/$', reference, flags=re.M)
    chunks = {int(chunks[i], 16): chunks[i + 1] for i in range(1, len(chunks), 2)}
    groups = {r['name']: r for r in csv.DictReader((ROOT / 'analysis/client_win/groups.csv').open())}
    names = {int(r['rva'], 16): r['name'] for r in csv.DictReader((ROOT / 'analysis/client_win/functions.csv').open())}
    server_names = {r['name'] for r in csv.DictReader((ROOT / 'analysis/024d/functions.csv').open())}
    sources, adopted = {}, {}
    for path in sorted((ROOT / 'analysis/client_win').glob('adopted*.txt')):
        if path.name.endswith('_pending.txt'):
            continue
        for line in path.read_text().splitlines():
            parts = line.split()
            if len(parts) >= 2 and not line.startswith('#'):
                adopted[parts[0]] = (parts[1], parts[2] if len(parts) > 2 else parts[0], path.name)
    rows = []
    for name, (path, symbol, adoption) in adopted.items():
        if not path.startswith(('src/game/', 'src/net/', 'src/common/')):
            continue
        source = sources.setdefault(path, (ROOT / path).read_text())
        clean = body(source, symbol)
        macros = dict(re.findall(r'^#define\s+(\w+)\s+([^\n]+)', source, re.M))
        expanded = clean
        for _ in range(5):
            expanded = re.sub(r'\b\w+\b', lambda m: macros.get(m[0], m[0]), expanded)
        group = groups[name]
        chunk = chunks.get(int(group['rva'], 16), '')
        calls = {names.get(int(v, 16) - 0x400000, v) for v in re.findall(r'SR_FN\(0x([0-9a-f]+)\)', chunk)}
        missing = sorted(c for c in calls if c not in server_names and c != name and c not in source)
        row = dict(name=name, path=path, symbol=symbol, adoption=adoption, rva=group['rva'], recorded_calls=int(group['recorded_calls'] or 0), storage=sorted(set(re.findall(r'\bsr_data_(\w+)', expanded))), missing_calls=missing, body_found=bool(clean), reference_found=bool(chunk))
        rows.append(row)
        if not row['recorded_calls'] or name == 'leveleditor_load':
            (out / (name + '.reference.c')).write_text(chunk, encoding='utf-8')
            (out / (name + '.shared.c')).write_text(clean, encoding='utf-8')
    (out / 'inventory.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
    print('Shared:', len(rows), 'No recorded calls:', sum(not r['recorded_calls'] for r in rows))
    for r in rows:
        if not r['recorded_calls'] or r['missing_calls']:
            print(r['name'], r['recorded_calls'], r['path'], 'storage=' + ','.join(r['storage']), 'missing=' + ','.join(r['missing_calls']), 'bodies=' + str((r['body_found'], r['reference_found'])))

if __name__ == '__main__':
    main()
