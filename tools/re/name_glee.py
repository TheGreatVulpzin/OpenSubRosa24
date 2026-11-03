"""Name the GLee functions of the Windows client.

The client links GLee statically. Each __GLeeLink_<extension> function stores
GetProcAddress("glXxx") into a pointer global. Each GLee_Lazy_glXxx stub calls
through that pointer. Both patterns are fixed machine code, so the names follow
from the bytes. The link functions get the name of the Linux function that
loads the same strings.

Usage: python -I tools/re/name_glee.py --exe original/client/subrosa.exe \
    --target build/features/client_win.jsonl --ref build/features/linux_x64.jsonl \
    --out analysis/client_win/glee_names.csv
Output: CSV rva,name,evidence in the format of merge_names.py --manual.
With --globals, also a CSV of the pointers, named GLeeFuncPtr_<function>.
"""
import argparse
import csv
import json
import struct

ap = argparse.ArgumentParser()
ap.add_argument('--exe', required=True)
ap.add_argument('--target', required=True)
ap.add_argument('--ref', required=True)
ap.add_argument('--out', required=True)
ap.add_argument('--globals', help='CSV of the pointer globals (rva,name,count,evidence)')
args = ap.parse_args()

data = open(args.exe, 'rb').read()
pe = struct.unpack_from('<I', data, 0x3C)[0]
nsec = struct.unpack_from('<H', data, pe + 6)[0]
opt_size = struct.unpack_from('<H', data, pe + 20)[0]
base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
sections = []
for i in range(nsec):
    _, vsize, va, rsize, roff = struct.unpack_from('<8sIIII', data, pe + 24 + opt_size + 40 * i)
    sections.append((va, max(vsize, rsize), roff))


def file_offset(rva):
    for va, size, roff in sections:
        if va <= rva < va + size:
            return roff + rva - va
    return None


def read_cstr(rva):
    off = file_offset(rva)
    if off is None:
        return None
    end = data.index(b'\0', off)
    return data[off:end].decode('latin1')


def load(path):
    with open(path, encoding='utf-8') as fh:
        return [json.loads(line) for line in fh]


target = load(args.target)
by_rva = {int(f['rva'], 16): f for f in target}

# Link functions: push <string>; call <GetProcAddress>; add esp,4; mov [global],eax.
global_name = {}
link_strings = {}
for f in target:
    rva = int(f['rva'], 16)
    if not any(s.startswith(('gl', 'wgl')) for s in f['strings']):
        continue
    code = data[file_offset(rva):file_offset(rva) + f['size']]
    pairs = []
    for i in range(len(code) - 16):
        if code[i] == 0x68 and code[i + 5] == 0xE8 and code[i + 10:i + 13] == b'\x83\xc4\x04' and code[i + 13] == 0xA3:
            string_va, = struct.unpack_from('<I', code, i + 1)
            global_va, = struct.unpack_from('<I', code, i + 14)
            name = read_cstr(string_va - base)
            if name and name.startswith(('gl', 'wgl')):
                pairs.append((name, global_va - base))
    if len(pairs) >= 1:
        link_strings[rva] = {n for n, _ in pairs}
        for name, g in pairs:
            global_name.setdefault(g, name)

# Stubs: a function that checks the extension, then calls through one of those globals.
stub_rows = []
seen = {}
for f in target:
    rva = int(f['rva'], 16)
    if f['size'] > 400 or rva in link_strings:
        continue
    code = data[file_offset(rva):file_offset(rva) + f['size']]
    # Stub prologue: call the extension check, then test its result.
    if code[:4] != bytes([0x55, 0x8B, 0xEC, 0xE8]) or code[8:14] != bytes([0x0F, 0xB6, 0xC0, 0x85, 0xC0, 0x74]):
        continue
    hits = set()
    for i in range(len(code) - 5):
        if code[i] == 0xFF and code[i + 1] == 0x15:
            g, = struct.unpack_from('<I', code, i + 2)
            if g - base in global_name:
                hits.add(g - base)
    if len(hits) == 1:
        g = hits.pop()
        name = 'GLee_Lazy_' + global_name[g]
        if name in seen:
            print('duplicate stub', hex(rva), hex(seen[name]), name)
            continue
        seen[name] = rva
        stub_rows.append((rva, name, f'Calls through the pointer loaded with "{global_name[g]}".'))

# Link functions: match the Linux function with the largest overlap of gl strings.
# The wgl functions exist only on Windows and stay unnamed.
ref = load(args.ref)
ref_links = [r for r in ref if r['name'].startswith('__GLeeLink_')]
link_rows = []
used_ref = set()
for rva, strs in sorted(link_strings.items()):
    if all(n.startswith('wgl') for n in strs):
        continue
    best = None
    for r in ref_links:
        rs = {s for s in r['strings'] if s.startswith('gl')}
        if not rs or r['name'] in used_ref:
            continue
        inter = len(strs & rs)
        score = inter / len(strs | rs)
        if best is None or score > best[0]:
            best = (score, r['name'], inter, len(strs), len(rs))
    if best and best[0] >= 0.8:
        used_ref.add(best[1])
        link_rows.append((rva, best[1], f'Loads the same {best[2]} gl strings as {best[1]} in the Linux client.'))
    else:
        print('link function without name', hex(rva), len(strs), best)

rows = sorted(stub_rows + link_rows)
with open(args.out, 'w', newline='', encoding='utf-8') as fh:
    w = csv.writer(fh, lineterminator='\n')
    w.writerow(['rva', 'name', 'evidence'])
    for rva, name, ev in rows:
        w.writerow([hex(rva), name, ev])
if args.globals:
    with open(args.globals, 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh, lineterminator='\n')
        w.writerow(['rva', 'name', 'count', 'evidence'])
        for g, name in sorted(global_name.items()):
            w.writerow([hex(g), 'GLeeFuncPtr_' + name, 1, f'Pointer stored from GetProcAddress("{name}").'])
print(f'{len(stub_rows)} stubs, {len(link_rows)} link functions, {len(link_strings)} link candidates, {len(global_name)} pointers')
