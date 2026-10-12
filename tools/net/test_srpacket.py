#!/usr/bin/env python3
"""Tests for srpacket.py. Run: python -I tools/net/test_srpacket.py

All packets here are synthetic: built by the writer, parsed by the reader, and checked
field by field. A few byte strings are assembled by hand from the rules in docs/protocol.md
so the writer is not only compared with the reader (which could share a mistake).
"""
import math
import os
import random
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import srpacket as sr  # noqa: E402


def close(a, b, tol):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


class BitstreamTests(unittest.TestCase):
    def test_header_is_magic_only(self):
        w = sr.PacketWriter()
        self.assertEqual(w.getvalue(), b"7DFP")
        self.assertEqual(w.tell(), (4, 0))

    def test_lsb_first_bit_packing(self):
        w = sr.PacketWriter()
        w.write_bits(0b101, 3)      # bits 0..2
        w.write_bits(0b11, 2)       # bits 3..4
        w.write_bits(0x2AB, 10)     # crosses a byte boundary
        # bits (LSB first): 101 11 | 0x2AB = 10 1010 1011
        stream = 0b101 | (0b11 << 3) | (0x2AB << 5)
        self.assertEqual(w.getvalue()[4:], stream.to_bytes(2, "little"))
        self.assertEqual(w.tell(), (5, 7))   # 15 bits -> body byte 1 (stream byte 5), bit 7

    def test_partial_byte_counts_when_sent(self):
        w = sr.PacketWriter()
        w.write_bits(1, 1)
        self.assertEqual(len(w.getvalue()), 5)

    def test_bytes_align_after_bits(self):
        w = sr.PacketWriter()
        w.write_bits(0x5, 3)
        w.u32(0x11223344)
        self.assertEqual(w.getvalue()[4:], b"\x05" + bytes.fromhex("44332211"))

    def test_value_is_masked_to_width(self):
        w = sr.PacketWriter()
        w.write_bits(0x1FF, 4)
        self.assertEqual(w.getvalue()[4], 0x0F)

    def test_overwrite_in_place(self):
        # serversend writes the event count first and patches it after the events.
        w = sr.PacketWriter()
        where = w.tell()
        w.write_bits(0, 6)
        w.write_bits(0x3FF, 10)
        end = w.tell()
        w.seek(where)
        w.write_bits(31, 6)
        w.seek(end)
        r = sr.PacketReader(w.getvalue())
        self.assertEqual(r.read_bits(6), 31)
        self.assertEqual(r.read_bits(10), 0x3FF)

    def test_random_roundtrip(self):
        rng = random.Random(1)
        for _ in range(200):
            fields = []
            w = sr.PacketWriter()
            for _ in range(rng.randint(1, 40)):
                kind = rng.choice("bbbbr")
                if kind == "b":
                    n = rng.randint(1, 32)
                    v = rng.getrandbits(n)
                    w.write_bits(v, n)
                    fields.append(("b", n, v))
                else:
                    data = bytes(rng.getrandbits(8) for _ in range(rng.randint(1, 9)))
                    w.write_bytes(data)
                    fields.append(("r", len(data), data))
            r = sr.PacketReader(w.getvalue())
            for kind, n, v in fields:
                got = r.read_bits(n) if kind == "b" else r.read_bytes(n)
                self.assertEqual(got, v)

    def test_nego(self):
        w = sr.PacketWriter()
        w.write_bits(-1, 10)
        w.write_bits(5, 10)
        r = sr.PacketReader(w.getvalue())
        self.assertEqual(r.nego(10), -1)
        self.assertEqual(r.nego(10), 5)

    def test_reader_rejects_bad_magic_and_short(self):
        with self.assertRaises(sr.PacketError):
            sr.PacketReader(b"XXXX\x00")
        with self.assertRaises(sr.PacketError):
            sr.PacketReader(b"7DFP")

    def test_reader_check_has_one_byte_slack(self):
        # pbitread only requires len >= pos + nbits//8, so 12 bits fit in 1 byte of body.
        r = sr.PacketReader(b"7DFP\x00\xAB")
        r.read_bytes(1)
        self.assertEqual(r.read_bits(12), 0xAB)   # missing high bits read as zero
        with self.assertRaises(sr.PacketError):
            r.read_bits(8)

    def test_buffer_limit(self):
        w = sr.PacketWriter()
        with self.assertRaises(sr.PacketError):
            w.write_bytes(bytes(0x10000))


class QuantizerTests(unittest.TestCase):
    def test_angle_known_values(self):
        # 16 bits: half = 0x8000, step = 2*pi/32768
        self.assertEqual(sr.pack_angle(0.0, 16), 0)
        self.assertEqual(sr.pack_angle(math.pi + 1e-4, 16), 0x4000)   # truncates, so nudge up
        self.assertEqual(sr.pack_angle(-math.pi - 1e-4, 16), 0x8000 | 0x4000)
        self.assertEqual(sr.pack_angle(2 * math.pi + 0.001, 16), 5)   # wraps: 0.001 rad past a full turn
        self.assertAlmostEqual(sr.unpack_angle(0x4000, 16), math.pi, places=5)
        self.assertAlmostEqual(sr.unpack_angle(0x8000 | 0x2000, 16), -math.pi / 2, places=5)

    def test_angle_roundtrip(self):
        rng = random.Random(2)
        for bits in (8, 16):
            step = 2 * math.pi / (1 << (bits - 1))
            for _ in range(300):
                x = rng.uniform(-6.2, 6.2)
                y = sr.unpack_angle(sr.pack_angle(x, bits), bits)
                self.assertLessEqual(abs(x - y), step + 1e-6)

    def test_scalar(self):
        self.assertEqual(sr.pack_scalar(1.0, 0.0, 8), 0)
        self.assertEqual(sr.pack_scalar(1.0, 0.5, 8), 64)
        self.assertEqual(sr.pack_scalar(1.0, -0.5, 8), 128 | 64)
        self.assertEqual(sr.pack_scalar(1.0, 9.0, 6), 31)          # clamps
        self.assertEqual(sr.pack_scalar(1.0, -9.0, 6), 32 | 31)
        self.assertAlmostEqual(sr.unpack_scalar(1.0, 64, 8), 0.5)

    def test_orientation_roundtrip(self):
        rng = random.Random(3)
        cases = [(1, 0, 0), (0, 1, 0), (0, 0, 1), (-1, 0, 0), (0, -1, 0), (0, 0, -1),
                 (0.577, 0.577, 0.577), (-0.577, 0.577, -0.577)]
        for _ in range(300):
            v = [rng.gauss(0, 1) for _ in range(3)]
            cases.append(tuple(v))
        for bits, tol in ((31, 2e-3), (21, 0.02)):
            for v in cases:
                n = math.sqrt(sum(c * c for c in v))
                u = tuple(c / n for c in v)
                code = sr.pack_orientation(u, bits)
                self.assertLess(code, 1 << bits)
                out = sr.unpack_orientation(code, bits)
                self.assertAlmostEqual(math.sqrt(sum(c * c for c in out)), 1.0, places=6)
                self.assertTrue(close(out, u, tol), (bits, u, out))

    def test_orientation_octant_bits(self):
        # first 3 bits: x<0, z<0, y<0
        self.assertEqual(sr.pack_orientation((0.6, 0.6, 0.529), 3) & 7, 0)
        self.assertEqual(sr.pack_orientation((-0.6, 0.6, 0.5), 3) & 7, 1)
        self.assertEqual(sr.pack_orientation((0.6, 0.6, -0.5), 3) & 7, 2)
        self.assertEqual(sr.pack_orientation((0.6, -0.6, 0.5), 3) & 7, 4)
        self.assertEqual(sr.pack_orientation((-0.6, -0.6, -0.5), 3) & 7, 7)


if __name__ == "__main__":
    unittest.main()
