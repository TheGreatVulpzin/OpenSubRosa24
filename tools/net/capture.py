"""Read a packet capture of the hook DLL (sv_packets.bin, subvulpes.ini capture_packets=1).

Usage:
  python -I tools/net/capture.py <sv_packets.bin> [--decode] [--limit N]
  python -I tools/net/capture.py <sv_packets.bin> --stats

Each line: time (ms), server tick, direction, peer, size, packet type. --decode also
parses the datagram with srpacket.py and prints the result (or the parse error).
--stats counts packets and bytes per direction and type, and the parse errors.
"""
import argparse
import socket
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import srpacket  # noqa: E402

RECORD = struct.Struct('<BIIIHH')


def records(path):
    data = Path(path).read_bytes()
    if data[:4] != b'SVPK':
        raise SystemExit('not a SubVulpes capture (no SVPK header)')
    pos = 12
    while pos + RECORD.size <= len(data):
        d, t, tick, ip, port, n = RECORD.unpack_from(data, pos)
        pos += RECORD.size
        yield d, t, tick, socket.inet_ntoa(struct.pack('<I', ip)), port, data[pos:pos + n]
        pos += n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('file')
    ap.add_argument('--decode', action='store_true')
    ap.add_argument('--stats', action='store_true')
    ap.add_argument('--limit', type=int, default=0)
    args = ap.parse_args()

    count, size, errors = Counter(), Counter(), Counter()
    shown = 0
    for d, t, tick, ip, port, raw in records(args.file):
        kind = raw[4] if len(raw) > 4 else -1
        key = ('in ' if d == 0 else 'out', kind)
        count[key] += 1
        size[key] += len(raw)
        parsed, err = None, None
        if args.decode or args.stats:
            try:
                parsed = srpacket.parse_packet(raw)
            except Exception as e:  # noqa: BLE001 - report any parse failure
                err = f'{type(e).__name__}: {e}'
                errors[key] += 1
        if args.stats or (args.limit and shown >= args.limit):
            continue
        shown += 1
        arrow = '<-' if d == 0 else '->'
        print(f'{t:8d} ms  tick {tick:7d}  {arrow} {ip}:{port:<5}  {len(raw):5d} bytes  type {kind}')
        if args.decode:
            print('    ' + (err or repr(parsed))[:400])

    if args.stats:
        for key in sorted(count):
            print(f'{key[0]} type {key[1]:3d}: {count[key]:6d} packets, {size[key]:9d} bytes, '
                  f'{errors[key]} parse errors')


if __name__ == '__main__':
    main()
