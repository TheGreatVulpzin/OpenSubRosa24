"""Compare image decoding, byte operations and font records on memory inputs."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[2]
PAGE = 4096
BASE = 0x20000000
SCRATCH = 0x19e00000
FUNCTIONS = {
    'bitreverse16': 0x5250, 'paeth': 0x7e40, 'compute_y': 0x6690,
    'zstbi_get8': 0x53a0, 'fill_bits': 0x5330, 'zreceive': 0x52d0,
    'zbuild_huffman': 0x4f40, 'zhuffman_decode': 0x55c0, 'expand': 0x5720,
    'compute_huffman_codes': 0x57d0, 'parse_uncompressed_block': 0x5b00,
    'parse_zlib_header': 0x5c80, 'init_defaults': 0x5d10,
    'parse_huffman_block': 0x53e0, 'parse_zlib': 0x4df0,
    'get_chunk_header': 0x7030, 'check_png_header': 0x7060,
    'create_png_image_raw': 0x73c0, 'create_png_image': 0x70b0,
    'compute_transparency': 0x7ec0, 'expand_palette': 0x7fc0,
    'parse_png_file': 0x66c0, 'do_png': 0x5fc0, 'convert_format': 0x6110,
    'decode_jpeg_image': 0x21d0, 'parse_entropy_coded_data': 0x2390,
    'grow_buffer_unsafe': 0x28a0, 'decode_block': 0x2950, 'decode': 0x2aa0,
    'extend_receive': 0x2cb0, 'idct_block': 0x2d40, 'process_marker': 0x3780,
    'build_huffman': 0x3b80, 'process_scan_header': 0x3d90,
    'decode_jpeg_header': 0x3f80, 'at_eof': 0x4080,
    'process_frame_header': 0x40c0, 'YCbCr_to_RGB_row': 0x4940,
    'cleanup_jpeg': 0x4a90, 'bmp_load': 0x8500, 'psd_load': 0x9ef0,
    'hdr_load': 0xa490, 'createfont': 0x108060, 'loadfont': 0x1095a0,
    'setfontresolution': 0x109f30, 'freadswap': 0xfdfb0,
    'write8': 0xae60, 'writefv': 0xace0,
}
RESULT = re.compile(r'^\[\s*\d+/\d+\]\s+(\S+)\s+(\d+) same\s+(\d+) different\s+(\d+) crashed', re.M)


class Cases:
    def __init__(self, recording):
        self.header = (recording / 'sv_snapshot.bin').open('rb').read(18)
        _, base, self.data_rva, self.size, _ = struct.unpack('<IIIIH', self.header)
        if base != BASE:
            raise ValueError('The saved image base must be 0x20000000.')
        self.pages = {}
        self.next = SCRATCH
        self.calls = []
        self.labels = []
        image = (ROOT / 'build/hook/subrosa_fixed.exe').read_bytes()
        pe = struct.unpack_from('<I', image, 0x3c)[0]
        sections = struct.unpack_from('<H', image, pe + 6)[0]
        optional = struct.unpack_from('<H', image, pe + 20)[0]
        for index in range(sections):
            position = pe + 24 + optional + index * 40
            if image[position:position + 8].rstrip(b'\0') == b'.data':
                rva, size, offset = struct.unpack_from('<III', image, position + 12)
                self.write(rva, image[offset:offset + size])

    def write(self, rva, raw):
        while raw:
            index, offset = divmod(rva - self.data_rva, PAGE)
            if not 0 <= rva - self.data_rva < self.size:
                raise ValueError(f'Input exceeds the data region: {rva:x}')
            count = min(len(raw), PAGE - offset)
            self.pages.setdefault(index, bytearray(PAGE))[offset:offset + count] = raw[:count]
            rva += count
            raw = raw[count:]

    def alloc(self, data):
        if isinstance(data, int):
            data = bytes(data)
        rva = self.next
        self.next = (rva + len(data) + 31) & ~15
        self.write(rva, data)
        return rva

    def call(self, name, label, *args):
        self.calls.append(f'R {FUNCTIONS[name]:x}' + ''.join(f' {kind}:{value & 0xffffffff:x}' for kind, value in args) + '\nZ r:0\n')
        self.labels.append({'function': name, 'case': label})

    def reader(self, data, size=0x28, width=0, height=0, channels=0):
        source = self.alloc(data)
        fields = struct.pack('<IIIIIII', width, height, channels, 0, 0, BASE + source, BASE + source + len(data))
        return self.alloc(fields + bytes(size - len(fields)))

    def zbuffer(self, data, size=32768, tables=False):
        source, output = self.alloc(data), self.alloc(size)
        fields = struct.pack('<IIIIIIII', BASE + source, BASE + source + len(data), 0, 0,
                             BASE + output, BASE + output, BASE + output + size, 0)
        raw = fields + (zhuffman(fixed_lengths()) + zhuffman([5] * 32) if tables else bytes(0xfc8))
        return self.alloc(raw)

    def save(self, folder):
        folder.mkdir(parents=True, exist_ok=True)
        with (folder / 'sv_snapshot.bin').open('wb') as out:
            out.write(self.header)
            for index, page in sorted(self.pages.items()):
                out.write(struct.pack('<I', index))
                out.write(page)
            out.write(struct.pack('<I', 0xffffffff))
        (folder / 'sv_args.txt').write_text(''.join(self.calls), encoding='utf-8')
        (folder / 'cases.json').write_text(json.dumps(self.labels, indent=2), encoding='utf-8')


def v(value):
    return 'V', value


def m(rva):
    return 'M', rva


def fixed_lengths():
    return [8] * 144 + [9] * 112 + [7] * 24 + [8] * 8


def zhuffman(sizes):
    table = bytearray(0x7e4)
    struct.pack_into('<512H', table, 0, *([65535] * 512))
    frequency = [sizes.count(i) if i else 0 for i in range(17)]
    code, total, next_code = 0, 0, [0] * 17
    for length in range(1, 16):
        next_code[length] = code
        struct.pack_into('<H', table, 0x400 + 2 * length, code)
        struct.pack_into('<H', table, 0x464 + 2 * length, total)
        code += frequency[length]
        struct.pack_into('<I', table, 0x420 + 4 * length, code << (16 - length))
        code *= 2
        total += frequency[length]
    struct.pack_into('<I', table, 0x460, 65536)
    first = next_code[:]
    for symbol, length in enumerate(sizes):
        if not length:
            continue
        start = struct.unpack_from('<H', table, 0x464 + 2 * length)[0]
        slot = next_code[length] - first[length] + start
        table[0x484 + slot] = length
        struct.pack_into('<H', table, 0x5a4 + slot * 2, symbol)
        if length <= 9:
            reverse = int(f'{next_code[length]:0{length}b}'[::-1], 2)
            for fast in range(reverse, 512, 1 << length):
                struct.pack_into('<H', table, fast * 2, slot)
        next_code[length] += 1
    return table


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def png(color, filter_type, transparency=False, interlace=False, compression=6):
    width, height = 9, 7
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    rows = [bytes(((x * 31 + y * 53 + c * 71) % (16 if color == 3 else 256))
                  for x in range(width) for c in range(channels)) for y in range(height)]
    raw = bytearray()
    passes = [(0, 0, 1, 1)] if not interlace else [(0,0,8,8),(4,0,8,8),(0,4,4,8),(2,0,4,4),(0,2,2,4),(1,0,2,2),(0,1,1,2)]
    for xs, ys, dx, dy in passes:
        previous = None
        for y in range(ys, height, dy):
            row = b''.join(rows[y][x * channels:(x + 1) * channels] for x in range(xs, width, dx))
            if not row:
                continue
            raw.append(filter_type)
            for i, value in enumerate(row):
                left = row[i - channels] if i >= channels else 0
                above = previous[i] if previous else 0
                corner = previous[i - channels] if previous and i >= channels else 0
                estimate = left + above - corner
                neighbors = (left, above, corner)
                prediction = (0, left, above, (left + above) // 2,
                              min(neighbors, key=lambda n: abs(estimate - n)))[filter_type]
                raw.append((value - prediction) & 255)
            previous = row
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, color, 0, 0, interlace))
    if color == 3:
        data += chunk(b'PLTE', bytes((i * 13 + c * 61) & 255 for i in range(16) for c in range(3)))
    if transparency and color in (0, 2, 3):
        values = bytes(range(16)) if color == 3 else b'\0\0' if color == 0 else b'\0\0\0G\0\x8e'
        data += chunk(b'tRNS', values)
    return data + chunk(b'IDAT', zlib.compress(raw, compression)) + chunk(b'IEND', b'')


def basic_cases(cases):
    for value in (0, 1, 2, 3, 255, 256, 32767, 32768, 65535, 0x12345678, 0xffffffff):
        cases.call('bitreverse16', f'bits_{value:x}', v(value))
    for left in (0, 1, 127, 255):
        for above in (0, 1, 127, 255):
            for corner in (0, 1, 127, 255):
                cases.call('paeth', 'neighbors', v(left), v(above), v(corner))
                cases.call('compute_y', 'rgb', v(left), v(above), v(corner))
    for name in ('zstbi_get8', 'fill_bits', 'zreceive', 'parse_zlib_header'):
        for data in (b'', b'\x78\x9c\xab\x45\x67\x89', b'\x78\x20', b'\x00\x00'):
            args = [m(cases.zbuffer(data))]
            if name == 'zreceive':
                args.append(v(7))
            cases.call(name, data.hex(), *args)
    cases.call('init_defaults', 'fixed')
    for sizes in (fixed_lengths(), [5] * 32, [1, 1], [1, 1, 2], [10] * 4):
        cases.call('zbuild_huffman', f'tree_{len(sizes)}', m(cases.alloc(0x7e4)), m(cases.alloc(bytes(sizes))), v(len(sizes)))
    payload = bytes((i * i // 7) % 43 for i in range(16384))
    for level, strategy in ((0, zlib.Z_DEFAULT_STRATEGY), (6, zlib.Z_FIXED), (9, zlib.Z_DEFAULT_STRATEGY)):
        encoder = zlib.compressobj(level, zlib.DEFLATED, 15, 8, strategy)
        stream = encoder.compress(payload) + encoder.flush()
        for header in (0, 1):
            cases.call('parse_zlib', f'level_{level}_header_{header}', m(cases.zbuffer(stream if header else stream[2:-4])), v(header))
        raw = stream[2:-4]
        block_type = (raw[0] >> 1) & 3
        if block_type == 0:
            cases.call('parse_uncompressed_block', 'stored', m(cases.zbuffer(raw[1:])))
        if block_type == 1:
            context = cases.zbuffer(raw, tables=True)
            source = cases.alloc(raw[1:])
            cases.write(context, struct.pack('<I', BASE + source))
            cases.write(context + 4, struct.pack('<I', BASE + source + len(raw) - 1))
            cases.write(context + 8, struct.pack('<II', 5, raw[0] >> 3))
            cases.call('parse_huffman_block', 'fixed', m(context))
        if block_type == 2:
            context = cases.zbuffer(raw)
            source = cases.alloc(raw[1:])
            cases.write(context, struct.pack('<I', BASE + source))
            cases.write(context + 4, struct.pack('<I', BASE + source + len(raw) - 1))
            cases.write(context + 8, struct.pack('<II', 5, raw[0] >> 3))
            cases.call('compute_huffman_codes', 'dynamic', m(context))
    table = cases.alloc(zhuffman(fixed_lengths()))
    for value in (0, 7, 0xaaaaaaaa, 0xffffffff):
        context = cases.zbuffer(b'\0' * 8)
        cases.write(context + 8, struct.pack('<II', 32, value))
        cases.call('zhuffman_decode', f'code_{value:x}', m(context), m(table))
    cases.call('expand', 'fixed_limit', m(cases.zbuffer(b'')), v(1))
    context = cases.zbuffer(b'', size=32)
    cases.write(context + 28, struct.pack('<I', 1))
    cases.call('expand', 'grow', m(context), v(100))


def png_cases(cases, real_files):
    samples = []
    for color in (0, 2, 3, 4, 6):
        for filter_type in range(5):
            samples.append((f'color_{color}_filter_{filter_type}', png(color, filter_type)))
        samples.append((f'color_{color}_adam7', png(color, 4, interlace=True)))
        if color in (0, 2, 3):
            samples.append((f'color_{color}_transparent', png(color, 2, transparency=True)))
    samples.extend((f'game_{index}', file.read_bytes()) for index, file in enumerate(real_files))
    for label, data in samples:
        for requested in (0, 1, 2, 3, 4):
            context = cases.reader(data)
            outputs = cases.alloc(12)
            cases.call('do_png', f'{label}_channels_{requested}', m(context), m(outputs), m(outputs + 4), m(outputs + 8), v(requested))
        cases.call('parse_png_file', label, m(cases.reader(data)), v(0), v(0))
        cases.call('check_png_header', label, m(cases.reader(data)))
        context = cases.reader(data[8:])
        cases.call('get_chunk_header', label, m(context))
    for data in (b'', b'not a PNG', b'\x89PNX'):
        cases.call('check_png_header', 'bad_signature', m(cases.reader(data)))
    for channels in (1, 2, 3, 4):
        raw = bytes([0] + list(range(channels * 3)) + [4] + list(range(channels * 3)))
        for name in ('create_png_image_raw', 'create_png_image'):
            for output in (channels, channels + 1):
                context = cases.reader(b'', width=3, height=2, channels=channels)
                args = [m(context), m(cases.alloc(raw)), v(len(raw)), v(output)]
                args += [v(3), v(2)] if name.endswith('_raw') else [v(0)]
                cases.call(name, f'channels_{channels}_{output}', *args)
    for channels in (2, 4):
        context = cases.reader(b'', width=3, height=1, channels=channels)
        pixels = cases.alloc(bytes([1, 2, 3, 4] * 3))
        cases.write(context + 36, struct.pack('<I', BASE + pixels))
        cases.call('compute_transparency', f'channels_{channels}', m(context), m(cases.alloc(bytes([1, 2, 3]))), v(channels))
    for source in range(1, 5):
        for requested in range(1, 5):
            pixels = cases.alloc(bytes((i * 43) & 255 for i in range(6 * source)))
            cases.call('convert_format', f'channels_{source}_{requested}', m(pixels), v(source), v(requested), v(3), v(2))
    for requested in (3, 4):
        context = cases.reader(b'', width=4, height=1, channels=1)
        pixels = cases.alloc(bytes([0, 3, 2, 1]))
        cases.write(context + 36, struct.pack('<I', BASE + pixels))
        palette = cases.alloc(bytes((i * 37) & 255 for i in range(16)))
        cases.call('expand_palette', f'channels_{requested}', m(context), m(palette), v(4), v(requested))


def jhuffman(counts, values):
    table = bytearray(0x690)
    sizes = [length for length, count in enumerate(counts, 1) for _ in range(count)]
    table[0x500:0x500 + len(sizes)] = bytes(sizes)
    table[0x400:0x400 + len(values)] = values
    table[:512] = bytes([255]) * 512
    slot, code = 0, 0
    for length in range(1, 17):
        struct.pack_into('<i', table, 0x64c + length * 4, slot - code)
        while slot < len(sizes) and sizes[slot] == length:
            struct.pack_into('<H', table, 0x200 + slot * 2, code)
            if length <= 9:
                for index in range(code << (9 - length), (code + 1) << (9 - length)):
                    table[index] = slot
            slot += 1
            code += 1
        struct.pack_into('<I', table, 0x604 + length * 4, code << (16 - length))
        code *= 2
    struct.pack_into('<I', table, 0x648, 0xffffffff)
    return table


ZIGZAG = [0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,
          14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,
          46,53,60,61,54,47,55,62,63]


def jpeg_segments(data):
    position = 2
    while position < len(data):
        if data[position] != 255:
            raise ValueError('Invalid JPEG marker position.')
        marker = data[position + 1]
        length = int.from_bytes(data[position + 2:position + 4], 'big')
        yield marker, data[position + 2:position + 2 + length], position + 2 + length
        position += 2 + length
        if marker == 0xda:
            break


def jpeg_state(cases, data):
    context = bytearray(0x36c0)
    parts = []
    for marker, segment, after in jpeg_segments(data):
        if marker == 0xdb:
            for start in range(2, len(segment), 65):
                table = segment[start] & 15
                for index, value in enumerate(segment[start + 1:start + 65]):
                    context[0x349c + table * 64 + ZIGZAG[index]] = value
        elif marker == 0xc4:
            start = 2
            while start < len(segment):
                selector = segment[start]
                counts = segment[start + 1:start + 17]
                count = sum(counts)
                values = segment[start + 17:start + 17 + count]
                offset = (0x1a5c if selector >> 4 else 0x1c) + (selector & 15) * 0x690
                context[offset:offset + 0x690] = jhuffman(counts, values)
                start += 17 + count
        elif marker == 0xc0:
            height, width, channels = struct.unpack_from('>HHB', segment, 3)
            parts = [list(struct.unpack_from('BBB', segment, 8 + index * 3)) for index in range(channels)]
            hmax = max(part[1] >> 4 for part in parts)
            vmax = max(part[1] & 15 for part in parts)
            columns, rows = (width + hmax * 8 - 1) // (hmax * 8), (height + vmax * 8 - 1) // (vmax * 8)
            struct.pack_into('<III', context, 0, width, height, channels)
            struct.pack_into('<6I', context, 0x359c, hmax, vmax, columns, rows, hmax * 8, vmax * 8)
            for index, (identity, sampling, quantization) in enumerate(parts):
                h, vertical = sampling >> 4, sampling & 15
                stride, buffer_height = columns * h * 8, rows * vertical * 8
                pixels = cases.alloc(stride * buffer_height)
                fields = [identity, h, vertical, quantization, 0, 0, 0,
                          (width * h + hmax - 1) // hmax, (height * vertical + vmax - 1) // vmax,
                          stride, buffer_height, BASE + pixels, BASE + pixels, 0]
                struct.pack_into('<14I', context, 0x35b4 + index * 0x38, *fields)
        elif marker == 0xdd:
            struct.pack_into('<I', context, 0x36b8, int.from_bytes(segment[2:4], 'big'))
        elif marker == 0xda:
            count = segment[2]
            struct.pack_into('<I', context, 0x36a4, count)
            for index in range(count):
                identity, selector = segment[3 + index * 2:5 + index * 2]
                part = next(i for i, fields in enumerate(parts) if fields[0] == identity)
                struct.pack_into('<II', context, 0x35c4 + part * 0x38, selector >> 4, selector & 15)
                struct.pack_into('<I', context, 0x36a8 + index * 4, part)
            source = cases.alloc(data[after:])
            struct.pack_into('<III', context, 16, 0, BASE + source, BASE + source + len(data) - after)
    context[0x369c] = 255
    return cases.alloc(context)


def jpeg_cases(cases, folder):
    for file in sorted(folder.glob('*.jpg')):
        data = file.read_bytes()
        cases.call('decode_jpeg_image', file.stem, m(cases.reader(data, size=0x36c0)))
        for scan in (0, 1, 2):
            cases.call('decode_jpeg_header', f'{file.stem}_scan_{scan}', m(cases.reader(data, size=0x36c0)), v(scan))
        for marker, segment, _ in jpeg_segments(data):
            if marker in (0xdb, 0xc4, 0xe0):
                cases.call('process_marker', f'{file.stem}_{marker:x}', m(cases.reader(segment, size=0x36c0)), v(marker))
            elif marker == 0xc0:
                for scan in (0, 2):
                    cases.call('process_frame_header', f'{file.stem}_scan_{scan}', m(cases.reader(segment, size=0x36c0)), v(scan))
            elif marker == 0xda:
                context = jpeg_state(cases, data)
                source = cases.alloc(segment)
                cases.write(context + 20, struct.pack('<II', BASE + source, BASE + source + len(segment)))
                cases.call('process_scan_header', file.stem, m(context))
        cases.call('parse_entropy_coded_data', file.stem, m(jpeg_state(cases, data)))
        context = jpeg_state(cases, data)
        block = cases.alloc(128)
        cases.call('decode_block', file.stem, m(context), m(block), m(context + 0x1c), m(context + 0x1a5c), v(0))
        context = jpeg_state(cases, data)
        cases.call('decode', file.stem, m(context), m(context + 0x1c))
        cases.call('cleanup_jpeg', file.stem, m(jpeg_state(cases, data)))
    for payload in (b'', b'\0\x7f\xff\0\x25', b'\xff\xd9'):
        cases.call('grow_buffer_unsafe', payload.hex(), m(cases.reader(payload, size=0x36c0)))
    for count in range(1, 16):
        cases.call('extend_receive', f'bits_{count}', m(cases.reader(b'\xab\xcd\xef\0', size=0x36c0)), v(count))
    for seed in range(10):
        coefficients = [0] * 64 if seed == 0 else [((i * 19 + seed * 7) % 127) - 63 for i in range(64)]
        if seed == 1:
            coefficients = [137] + [0] * 63
        quantization = bytes((i * 11 + seed * 17) % 255 + 1 for i in range(64))
        cases.call('idct_block', f'block_{seed}', m(cases.alloc(160)), v(17),
                   m(cases.alloc(struct.pack('<64h', *coefficients))), m(cases.alloc(quantization)))
    for counts in ([1] + [0] * 15, [0, 2, 3] + [0] * 13, [0] * 15 + [3]):
        cases.call('build_huffman', 'lengths', m(cases.alloc(0x690)), m(cases.alloc(struct.pack('<16i', *counts))))
    for stride in (3, 4):
        cases.call('YCbCr_to_RGB_row', f'stride_{stride}', m(cases.alloc(40)),
                   m(cases.alloc(bytes([0, 1, 127, 128, 254, 255]))),
                   m(cases.alloc(bytes([0, 255, 127, 128, 254, 1]))),
                   m(cases.alloc(bytes([255, 0, 127, 128, 1, 254]))), v(6), v(stride))
    for data in (b'', b'123'):
        cases.call('at_eof', 'end', m(cases.reader(data)))


def other_cases(cases):
    for width, height in ((1, 2), (3, -2), (9, 3)):
        rows = b''.join(bytes((column * 37 + row * 11) & 255 for column in range(width * 3)) + bytes(-width * 3 & 3)
                        for row in range(abs(height)))
        header = struct.pack('<2sIHHI', b'BM', 54 + len(rows), 0, 0, 54)
        header += struct.pack('<IiiHH6I', 40, width, height, 1, 24, 0, len(rows), 0, 0, 0, 0)
        for requested in range(5):
            outputs = cases.alloc(12)
            cases.call('bmp_load', f'{width}_{height}_{requested}', m(cases.reader(header + rows)),
                       m(outputs), m(outputs + 4), m(outputs + 8), v(requested))
    for compression in (0, 1):
        for channels in (1, 3, 4):
            width, height = 3, 2
            header = b'8BPS' + struct.pack('>H6sHIIHHIIIH', 1, bytes(6), channels, height, width, 8, 3, 0, 0, 0, compression)
            data = b''
            if compression:
                data = bytes(height * channels * 2)
                data += b''.join(bytes([width * height - 1]) + bytes((channel * 31 + i * 17) & 255 for i in range(width * height))
                                 for channel in range(channels))
            else:
                data = bytes((channel * 31 + i * 17) & 255 for channel in range(channels) for i in range(width * height))
            for requested in range(5):
                outputs = cases.alloc(12)
                cases.call('psd_load', f'{compression}_{channels}_{requested}', m(cases.reader(header + data)),
                           m(outputs), m(outputs + 4), m(outputs + 8), v(requested))
    for width, packed in ((3, False), (9, True), (9, False)):
        data = b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n' + f'-Y 2 +X {width}\n'.encode()
        for row in range(2):
            if packed:
                data += bytes([2, 2, 0, width])
                data += b''.join(bytes([128 + width, value]) for value in (33, 75, 143, 130))
            else:
                data += bytes([33, 75, 143, 130]) * width
        for requested in range(5):
            outputs = cases.alloc(12)
            cases.call('hdr_load', f'{width}_{packed}_{requested}', m(cases.reader(data)),
                       m(outputs), m(outputs + 4), m(outputs + 8), v(requested))
    cases.call('setfontresolution', 'screen', v(1920), v(1080))
    cases.call('createfont', 'built_in')
    for file in sorted((ROOT / 'build/client_test/texture').glob('*font*.png')):
        for size in (16.0, 32.0, 48.0):
            path = cases.alloc(str(file.resolve()).encode('utf-8') + b'\0')
            cases.call('loadfont', f'{file.stem}_{size}', v(1005), m(path), v(struct.unpack('<I', struct.pack('<f', size))[0]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('recording', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('--run', action='store_true')
    parser.add_argument('--funcs', help='Comma-separated function names.')
    parser.add_argument('--no-game-images', action='store_true')
    parser.add_argument('--file-io', action='store_true')
    parser.add_argument('--exact', action='store_true', help='Require equal float bits in memory.')
    args = parser.parse_args()
    cases = Cases(args.recording)
    basic_cases(cases)
    png_cases(cases, [] if args.no_game_images else sorted((ROOT / 'original/client/data').rglob('*.png')))
    jpeg_cases(cases, ROOT / 'build/client_c1/jpeg')
    other_cases(cases)
    cases.save(args.work)
    print(f'{len(cases.calls)} calls, {len(cases.pages)} pages', flush=True)
    if not args.run:
        return
    env = dict(os.environ, HARNESS_GAME_DIR=str(ROOT / 'build/client_test'), HARNESS_RESET='1',
               HARNESS_DEBUG='1', HARNESS_RETRY='0', SV_UI_WIDTH='220',
               HARNESS_ENV_DIR=str(args.work / 'files'), SV_LOG_FILE=str(args.work.resolve() / 'compare.log'))
    env['PATH'] = str(ROOT / 'build/client_deps') + os.pathsep + env['PATH']
    selected = args.funcs or ','.join(sorted({item['function'] for item in cases.labels}))
    program = ROOT / 'build/hook/harness_client.exe'
    if args.exact:
        exact = args.work.resolve() / 'exact'
        exact.mkdir(exist_ok=True)
        shutil.copy2(program, exact / program.name)
        program = exact / program.name
        (exact / 'subvulpes.ini').write_text('[hooks]\nverify_float_ulps=0\n', encoding='ascii')
    if args.file_io:
        input_file = args.work.resolve() / 'input.bin'
        input_file.write_bytes(bytes(range(13)))
        env['HARNESS_IMAGE_IO'] = str(input_file)
    with (args.work / 'results.txt').open('w', encoding='utf-8') as output:
        result = subprocess.run([str(program), str(ROOT / 'build/hook/subrosa_fixed.exe'),
                                 str(args.work / 'sv_snapshot.bin'), str(args.work / 'sv_args.txt'), selected],
                                cwd=ROOT, env=env, stdout=output, stderr=subprocess.STDOUT, timeout=1800)
    text = (args.work / 'results.txt').read_text(encoding='utf-8', errors='replace')
    rows = {name: dict(same=int(same), different=int(different), crashed=int(crashed))
            for name, same, different, crashed in RESULT.findall(text)}
    expected = {'freadswap', 'write8', 'writefv'} if args.file_io else set(selected.split(','))
    if set(rows) != expected:
        raise SystemExit(f'Missing results: {sorted(expected - set(rows))}')
    (args.work / 'results.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
    for name, row in rows.items():
        print(name, row, flush=True)
    if result.returncode or not rows or any(row['different'] or row['crashed'] or not row['same'] for row in rows.values()):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
