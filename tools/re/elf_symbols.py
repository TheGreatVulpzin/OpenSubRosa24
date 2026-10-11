"""Dump function symbols from a 64-bit little-endian ELF (no external deps).

Usage: python -I tools/re/elf_symbols.py <elf> [--sections]
Output: one line per function: <addr hex> <size> <name>, sorted by address.
"""
import struct
import sys

data = open(sys.argv[1], 'rb').read()
assert data[:4] == b'\x7fELF' and data[4] == 2, 'expected ELF64'

shoff, = struct.unpack_from('<Q', data, 0x28)
shentsize, shnum, shstrndx = struct.unpack_from('<HHH', data, 0x3A)
# sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize
sections = [struct.unpack_from('<IIQQQQIIQQ', data, shoff + i * shentsize) for i in range(shnum)]


def cstr(table_off, off):
    end = data.index(b'\0', table_off + off)
    return data[table_off + off:end].decode()


if '--sections' in sys.argv:
    shstr = sections[shstrndx][4]
    for s in sections:
        print(f'{cstr(shstr, s[0]):<20} addr={s[3]:#x} size={s[5]}')
    sys.exit()

funcs = []
for s in sections:
    if s[1] != 2:  # SHT_SYMTAB
        continue
    strtab = sections[s[6]][4]
    for i in range(s[5] // 24):
        st_name, info, _, shndx, value, size = struct.unpack_from('<IBBHQQ', data, s[4] + i * 24)
        if info & 0xF == 2 and shndx != 0:  # STT_FUNC, defined
            funcs.append((value, size, cstr(strtab, st_name)))

for value, size, name in sorted(funcs):
    print(f'{value:x} {size} {name}')
