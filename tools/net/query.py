"""Send a query (packet type 0) to a Sub Rosa 0.24 server and print the reply.

Usage: python -I tools/net/query.py [host] [port] [--version 24]

The server answers with type 1: version, game type, players, maximum players, name and
(for a client version above 6) the account key. This is the same packet that the server
list of the game sends.
"""
import argparse
import socket
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import srpacket  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('host', nargs='?', default='127.0.0.1')
    ap.add_argument('port', nargs='?', type=int, default=27600)
    ap.add_argument('--version', type=int, default=srpacket.PROTOCOL_VERSION)
    ap.add_argument('--timeout', type=float, default=2.0)
    args = ap.parse_args()

    nonce = int(time.time() * 1000).to_bytes(8, 'little')[:4]
    query = srpacket.build_packet(srpacket.Query(args.version, nonce))
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.settimeout(args.timeout)
        start = time.perf_counter()
        s.sendto(query, (args.host, args.port))
        try:
            data, peer = s.recvfrom(65536)
        except socket.timeout:
            print(f'no reply from {args.host}:{args.port} in {args.timeout} s')
            return 1
    ms = (time.perf_counter() - start) * 1000
    print(f'{len(data)} bytes from {peer[0]}:{peer[1]} in {ms:.1f} ms: {data.hex()}')
    reply = srpacket.parse_packet(data)
    print(reply)
    if getattr(reply, 'nonce', None) != nonce:
        print('WARNING: the nonce in the reply is not the one sent')
    return 0


if __name__ == '__main__':
    sys.exit(main())
