"""Survey the executables in a folder: format, architecture, symbols, build date and SHA-256.

Usage: python -I tools/re/survey_bins.py <folder> [--manifest original/MANIFEST.sha256] [--tsv]
Output: one line per executable (PE, ELF or Mach-O). No external dependencies.

Symbols:
  ELF    -> number of functions in .symtab (like tools/re/elf_symbols.py).
  Mach-O -> number of entries in LC_SYMTAB and whether _gameloop exists.
  PE     -> COFF symbol table or PDB path in the debug directory.
"""
import datetime
import hashlib
import os
import struct
import sys


def elf_info(d):
    is64 = d[4] == 2
    machine, = struct.unpack_from('<H', d, 0x12)
    arch = {0x3E: 'x86-64', 0x03: 'i386', 0xB7: 'arm64'}.get(machine, hex(machine))
    if is64:
        shoff, = struct.unpack_from('<Q', d, 0x28)
        shentsize, shnum, _ = struct.unpack_from('<HHH', d, 0x3A)
        shfmt, symsize = '<IIQQQQIIQQ', 24
    else:
        shoff, = struct.unpack_from('<I', d, 0x20)
        shentsize, shnum, _ = struct.unpack_from('<HHH', d, 0x2E)
        shfmt, symsize = '<IIIIIIIIII', 16
    nfunc = 0
    for i in range(shnum):
        s = struct.unpack_from(shfmt, d, shoff + i * shentsize)
        if s[1] != 2:  # SHT_SYMTAB
            continue
        for j in range(s[5] // symsize):
            if is64:
                _, info, _, shndx, _, _ = struct.unpack_from('<IBBHQQ', d, s[4] + j * symsize)
            else:
                _, _, _, info, _, shndx = struct.unpack_from('<IIIBBH', d, s[4] + j * symsize)
            if info & 0xF == 2 and shndx != 0:
                nfunc += 1
    return 'ELF64' if is64 else 'ELF32', arch, f'{nfunc} functions' if nfunc else 'no (stripped)'


MACHO_CPU = {7: 'i386', 0x1000007: 'x86-64', 18: 'ppc', 0x1000012: 'ppc64'}


def macho_one(d, off):
    magic, = struct.unpack_from('<I', d, off)
    be = magic in (0xCEFAEDFE, 0xCFFAEDFE)
    e = '>' if be else '<'
    magic, cpu, _, _, ncmds, _, _ = struct.unpack_from(e + 'IiiIIII', d, off)
    is64 = magic in (0xFEEDFACF, 0xCFFAEDFE)
    p = off + (32 if is64 else 28)
    nsyms, gameloop = 0, False
    for _ in range(ncmds):
        cmd, size = struct.unpack_from(e + 'II', d, p)
        if cmd == 2:  # LC_SYMTAB
            symoff, nsyms, stroff, strsize = struct.unpack_from(e + 'IIII', d, p + 8)
            gameloop = b'\0_gameloop\0' in d[off + stroff:off + stroff + strsize]
        p += size
    return MACHO_CPU.get(cpu, hex(cpu)), nsyms, gameloop


def macho_info(d):
    magic, = struct.unpack_from('>I', d, 0)
    offs = [0]
    if magic == 0xCAFEBABE:  # fat
        n, = struct.unpack_from('>I', d, 4)
        offs = [struct.unpack_from('>iiIII', d, 8 + i * 20)[2] for i in range(n)]
    archs, syms = [], []
    for o in offs:
        arch, nsyms, gl = macho_one(d, o)
        archs.append(arch)
        syms.append(f'{arch}: {nsyms} symbols' + (', _gameloop' if gl else '') if nsyms else f'{arch}: no')
    return 'Mach-O' + (' fat' if len(offs) > 1 else ''), '+'.join(archs), '; '.join(syms)


def pe_info(d):
    pe, = struct.unpack_from('<I', d, 0x3C)
    machine, nsec, stamp, symptr, nsym, optsize, _ = struct.unpack_from('<HHIIIHH', d, pe + 4)
    arch = {0x14C: 'i386', 0x8664: 'x86-64'}.get(machine, hex(machine))
    opt = pe + 24
    is64 = struct.unpack_from('<H', d, opt)[0] == 0x20B
    linker = f'{d[opt + 2]}.{d[opt + 3]}'
    ddir = opt + (112 if is64 else 96)
    dbg_rva, dbg_size = struct.unpack_from('<II', d, ddir + 6 * 8)
    secs = [struct.unpack_from('<8sIIII', d, opt + optsize + i * 40) for i in range(nsec)]

    def rva2off(rva):
        for _, vsize, va, rsize, raw in secs:
            if va <= rva < va + max(vsize, rsize):
                return raw + rva - va
        return None

    pdb = None
    o = rva2off(dbg_rva) if dbg_rva else None
    if o is not None:
        for i in range(dbg_size // 28):
            typ, size, _, raw = struct.unpack_from('<IIII', d, o + i * 28 + 12)
            if typ == 2 and d[raw:raw + 4] == b'RSDS':  # CodeView
                pdb = d[raw + 24:d.index(b'\0', raw + 24)].decode(errors='replace')
    date = datetime.datetime.fromtimestamp(stamp, datetime.timezone.utc).strftime('%Y-%m-%d')
    if symptr and nsym:
        sym = f'COFF ({nsym} symbols)'
    elif pdb:
        sym = f'no (PDB {pdb})'
    else:
        sym = 'no'
    return f'PE32{"+" if is64 else ""} (linker {linker})', arch, sym, date


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    root = args[0]
    manifest = {}
    if '--manifest' in sys.argv:
        mpath = sys.argv[sys.argv.index('--manifest') + 1]
        args.remove(mpath)
        for line in open(mpath, encoding='utf-8'):
            h, _, name = line.rstrip('\n').partition('  ')
            manifest.setdefault(h, []).append(name)
    tsv = '--tsv' in sys.argv
    rows = []
    for dp, _, files in os.walk(root):
        for fn in files:
            path = os.path.join(dp, fn)
            if os.path.islink(path):
                continue
            with open(path, 'rb') as f:
                head = f.read(4)
            if not (head == b'\x7fELF' or head[:2] == b'MZ' or
                    head in (b'\xce\xfa\xed\xfe', b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xfe\xed\xfa\xce')):
                continue
            d = open(path, 'rb').read()
            date = '-'
            try:
                if head == b'\x7fELF':
                    fmt, arch, sym = elf_info(d)
                elif head[:2] == b'MZ':
                    if d[struct.unpack_from('<I', d, 0x3C)[0]:][:4] != b'PE\0\0':
                        continue
                    fmt, arch, sym, date = pe_info(d)
                else:
                    fmt, arch, sym = macho_info(d)
            except (struct.error, ValueError, IndexError) as ex:
                fmt, arch, sym = '?', '?', f'error: {ex}'
            h = hashlib.sha256(d).hexdigest()
            known = ', '.join(manifest.get(h, [])) or '-'
            rows.append((os.path.relpath(path, root), fmt, arch, sym, date, len(d), h, known))
    rows.sort()
    if tsv:
        print('file\tformat\tarch\tsymbols\tbuild\tbytes\tsha256\tin_manifest')
        for r in rows:
            print('\t'.join(map(str, r)))
    else:
        for r in rows:
            print(f'{r[0]}\n    {r[1]} {r[2]} | symbols: {r[3]} | build {r[4]} | {r[5]} B\n'
                  f'    sha256 {r[6]} | manifest: {r[7]}')


if __name__ == '__main__':
    main()
