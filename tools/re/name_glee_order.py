"""Name the __GLeeLink_<extension> functions that the string match left open.

The 181 link functions of the extensions with no entry point are 10 bytes long ("return 2"), so no
string tells them apart. GLee defines the link functions in one fixed order, in the Windows client
and in the Linux client. The named link functions are anchors. Between two anchors, the Windows
functions that have no name take the Linux names between the same two anchors, in order. A gap
where the two counts differ gets no name. Only the GL extensions are used: the WGL links of the
Windows client have no Linux name.

Usage: python -I tools/re/name_glee_order.py --elf <elf_symbols.txt> --functions analysis/client_win/functions.csv \
    --out analysis/client_win/glee_order_names.csv
elf_symbols.txt is the output of tools/re/elf_symbols.py on original/client/subrosa.x64.
Output: CSV rva,name,evidence in the format of merge_names.py --manual.
"""
import argparse
import csv

ap = argparse.ArgumentParser()
ap.add_argument('--elf', required=True)
ap.add_argument('--functions', required=True)
ap.add_argument('--out', required=True)
ap.add_argument('--low', default='0x20000', help='first rva of the GLee code')
ap.add_argument('--high', default='0x30000', help='end of the GLee code')
args = ap.parse_args()

elf = []
for line in open(args.elf, encoding='utf-8'):
    p = line.split()
    # Only the GL extensions. The Linux client has GLX ones after them, the Windows client has WGL ones.
    if len(p) >= 3 and p[2].startswith('__GLeeLink_GL_'):
        elf.append((int(p[0], 16), p[2]))
elf.sort()
elf_names = [n for _, n in elf]
elf_index = {n: i for i, n in enumerate(elf_names)}

low, high = int(args.low, 16), int(args.high, 16)
with open(args.functions, encoding='utf-8') as fh:
    rows = [r for r in csv.DictReader(fh) if low <= int(r['rva'], 16) < high]
rows.sort(key=lambda r: int(r['rva'], 16))

# Walk the Windows functions in address order. An anchor is a named link function that the Linux
# client also has.
out = []
skipped = []
pending = []  # unnamed functions since the last anchor
last_anchor = -1
for r in rows + [None]:
    anchor = None
    if r is not None:
        if not r['name']:
            pending.append(r)
            continue
        anchor = elf_index.get(r['name'])
        if anchor is None:
            continue
    nxt = anchor if anchor is not None else len(elf_names)
    gap = elf_names[last_anchor + 1:nxt]
    if pending:
        # After the last anchor the Windows client has the rest of the GL links, then the WGL links.
        if r is None and len(pending) > len(gap):
            pending = pending[:len(gap)]
        if len(gap) == len(pending):
            for f, name in zip(pending, gap):
                out.append((f['rva'], name, 'Order of the GLee link functions: between %s and %s in the Linux client.' % (
                    elf_names[last_anchor] if last_anchor >= 0 else 'the start',
                    elf_names[nxt] if nxt < len(elf_names) else 'the end')))
        else:
            skipped.append((pending[0]['rva'], len(pending), len(gap)))
    pending = []
    if anchor is not None:
        last_anchor = anchor

with open(args.out, 'w', newline='', encoding='utf-8') as fh:
    w = csv.writer(fh)
    w.writerow(['rva', 'name', 'evidence'])
    w.writerows(out)
print(f'{len(out)} names written, {len(skipped)} gaps skipped (rva, windows count, linux count): {skipped}')
