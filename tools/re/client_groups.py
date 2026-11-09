"""Sort the functions of the client table into the rewrite groups C1 to C11 (docs/plano.md, 7.6).

Reads the table (src/lifted/client/lifted_table.c), the names (analysis/client_win/functions.csv),
the functions already adopted from the server (analysis/client_win/adopted.txt) and the recorded
calls (build/recordings/*/sv_args.txt, if present). Names decide the group. A function without a
name goes to C11. Output: analysis/client_win/groups.csv (rva, name, size, group, recorded calls).

Usage: python -I tools/re/client_groups.py
"""
import csv
import re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

RULES = [
    ('C9', r'^(__)?GLee|^initGL|^wgl|^GLee'),
    ('C8', r'^leveleditor|^leveledit|^buildingedit|^model_editor|^model_viewer|^level_checkeditor|^editor'),
    ('C6', r'speex|^sound|audio|^testvoice|voice|^openal|^al[A-Z]|^deletesound|^enablesocket'),
    ('C1', r'^(do_)?png|^parse_png|bmp|jpeg|idct|huffman|^hdr_|^ldr_|^tga_|^psd_|zlib|^zbuild|^zhuffman|^zreceive|'
           r'resample|^YCbCr|convert_format|^decode_(block|jpeg)|^extend_receive|^grow_buffer|^process_(marker|scan|frame)|'
           r'^compute_(y|transparency|huffman)|^expand|^paeth|^start_(file|mem)|^fill_bits|^write_pixels|^create_png|'
           r'^get_chunk|^check_png|^load_jpeg|^cleanup_jpeg|^init_defaults|^bitcount|^bitreverse|^high_bit|^shiftsigned|'
           r'^zstbi|^stbi|^freadswap|^fwriteswap|^at_eof|^write8|^writef|^outfile|^decode$|^parse_(entropy|huffman|uncompressed)|font'),
    ('C5', r'menu|^drawmenu|^createmenubutton|^checkmenuitems|^formattextbox|^deleteselectedtext|^text_getlength'),
    ('C4', r'^getinputs|^checkkeyboard|^checkmouse|keyboard|^movecamera|^movewindow|^debug_getinputs|^mouse'),
    ('C3', r'^render|^draw|^frame_|sprite|^tiremark|^glstate|^compileprogram|^setupprogram|shader|^reloadopengl|'
           r'^enabletwosided|^disabletwosided|^debug_render|^screenshot|^boxinsidefrustum|^pointinsidefrustum|^model_render|'
           r'^listvideomodes|^displaydebuginfo|^hudsimulation|^raycast_|^cityobject_simulation'),
    ('C2', r'^generate|^createmodel|^model_|^landscape_|^tempmesh|^modeltemp|^createhumanmodel|^createhairmodel|'
           r'noise|^lerp|^serp|^cerp|^engine_bezier|^hermite|^copymodel|^calculatepointscale|^deletesprite_model|'
           r'^objecttype_|^createmodel_'),
    ('C7', r'^setupclient|^gameloop|^checksimulation|^main$|^mainCRTStartup|^reset$|^reactivategame|^client_simulation|'
           r'^dedicatedserverloop|^initplayticket|^updateplayticket|^ticketreturned|^steam_|^debug_|^displaydebug|'
           r'^humansimulationclient|^itemsimulationclient|^clientsend|^clientrecv|^starttimer|^stoptimer|^resettimer'),
]


def table():
    names = {}
    for r in csv.DictReader(open(ROOT / 'analysis/client_win/functions.csv', encoding='utf-8')):
        names[int(r['rva'], 16)] = (r['name'], int(r['size']))
    return names


def calls():
    count = Counter()
    for f in sorted((ROOT / 'build/recordings').glob('client-*/sv_args.txt')):
        for line in open(f, encoding='utf-8', errors='replace'):
            if line.startswith('R '):
                count[int(line.split()[1], 16)] += 1
    return count


def main():
    names = table()
    recorded = calls()
    in_table = set(int(m, 16) for m in re.findall(r'\{\s*"[^"]*",\s*(0x[0-9a-f]+)',
                                                  (ROOT / 'src/lifted/client/lifted_table.c').read_text(encoding='utf-8')))
    adopted = set()
    for line in (ROOT / 'analysis/client_win/adopted.txt').read_text(encoding='utf-8').splitlines():
        if line.strip() and not line.startswith('#'):
            adopted.add(line.split()[0])
    server = set(l.split()[0] for l in (ROOT / 'analysis/client_win/adopted_pending.txt').read_text(
        encoding='utf-8').splitlines() if l.strip() and not l.startswith('#'))
    server |= adopted
    shared = set()
    for r in csv.DictReader(open(ROOT / 'analysis/client_win/shared_code.csv', encoding='utf-8')):
        shared.add(r['name'])
    rows = []
    for rva in sorted(in_table):
        name, size = names.get(rva, ('', 0))
        if name in server:
            group = 'C0'
        elif not name:
            group = 'C11'
        else:
            group = 'C10'
            for g, pat in RULES:
                if re.search(pat, name):
                    group = g
                    break
            if name in shared and group == 'C10':
                group = 'C0x'
        rows.append((hex(rva), name, size, group, recorded.get(rva, 0)))
    with open(ROOT / 'analysis/client_win/groups.csv', 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh)
        w.writerow(['rva', 'name', 'size', 'group', 'recorded_calls'])
        w.writerows(rows)
    stats = defaultdict(lambda: [0, 0, 0])
    for _, name, size, g, c in rows:
        s = stats[g]
        s[0] += 1
        s[1] += size
        s[2] += 1 if c else 0
    for g in sorted(stats, key=lambda x: (len(x), x)):
        print(f'{g:5} {stats[g][0]:5} functions {stats[g][1]:7} bytes {stats[g][2]:4} with recorded calls')


if __name__ == '__main__':
    main()
