"""Copy a PE file, move it to a new image base and turn off ASLR (DYNAMICBASE) in the copy.

The test server and the offline harness (instrumentation/tests/harness.c) both load the copy at the
same fixed base. A snapshot of .data from the server then fits the harness without any
relocation. The default base 0x20000000 is free in both processes (the harness heap can
take the area near 0x400000).

Usage: python -I tools/re/fixed_base.py <input.exe> <output.exe> [--base 0x20000000]
The input is only read (original/ stays read-only).
"""
import argparse
import struct

DYNAMICBASE = 0x40
IMAGE_REL_BASED_HIGHLOW = 3


def sections(data, pe):
    nsec = struct.unpack_from('<H', data, pe + 6)[0]
    opt_size = struct.unpack_from('<H', data, pe + 20)[0]
    first = pe + 24 + opt_size
    out = []
    for i in range(nsec):
        o = first + 40 * i
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, o + 8)
        out.append((va, max(vsize, rsize), raw, rsize))
    return out


def rva_to_off(secs, rva):
    for va, size, raw, rsize in secs:
        if va <= rva < va + size:
            if rva - va >= rsize:
                return None  # uninitialized data: nothing in the file
            return raw + rva - va
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('dst')
    ap.add_argument('--base', default='0x20000000')
    args = ap.parse_args()
    new_base = int(args.base, 16)

    data = bytearray(open(args.src, 'rb').read())
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise SystemExit('not a PE file')
    opt = pe + 24
    old_base = struct.unpack_from('<I', data, opt + 28)[0]
    delta = (new_base - old_base) & 0xffffffff
    secs = sections(data, pe)

    # Base relocations: data directory 5.
    reloc_rva, reloc_size = struct.unpack_from('<II', data, opt + 96 + 5 * 8)
    pos = rva_to_off(secs, reloc_rva)
    end = pos + reloc_size
    count = 0
    while pos < end:
        page, block = struct.unpack_from('<II', data, pos)
        if block < 8:
            break
        for k in range((block - 8) // 2):
            entry = struct.unpack_from('<H', data, pos + 8 + 2 * k)[0]
            if entry >> 12 == IMAGE_REL_BASED_HIGHLOW:
                off = rva_to_off(secs, page + (entry & 0xfff))
                if off is not None:
                    v = struct.unpack_from('<I', data, off)[0]
                    struct.pack_into('<I', data, off, (v + delta) & 0xffffffff)
                    count += 1
        pos += block

    struct.pack_into('<I', data, opt + 28, new_base)
    flags = struct.unpack_from('<H', data, opt + 70)[0]
    struct.pack_into('<H', data, opt + 70, flags & ~DYNAMICBASE)
    open(args.dst, 'wb').write(data)
    print(f'{args.dst}: base {old_base:#x} -> {new_base:#x}, {count} relocations, ASLR off')


if __name__ == '__main__':
    main()
