#!/usr/bin/env python3
"""Reader and writer for Sub Rosa 0.24 network packets (UDP, dedicated server <-> client).

Wire format documented in docs/protocol.md. Every datagram is:

    "7DFP" (4 bytes, `socketid`)  |  type (1 byte)  |  type-specific body

The body mixes two kinds of fields, exactly like the original code does:

* bit fields (`pbitwrite` / `pbitread`): LSB first, packed into the byte stream;
* byte fields (`cs_pwrite` / `cs_pread`): the stream is first rounded up to the
  next byte boundary, then raw bytes follow (little-endian int32/float32).

This module copies the behaviour of the original functions (limits included), so
what it writes is what the game writes and what it reads is what the game reads.

Usage:
    python -I tools/net/srpacket.py dump <hex-or-file>    # parse a datagram and print it
    python -I tools/net/srpacket.py selftest              # build and re-parse sample packets

Everything quantized (angles, scalars, orientations) is lossy, as in the game.
Parsed floats are the decoded values; re-encoding them may move a value by one
quantization step (the original has the same drift).
"""
from __future__ import annotations

import math
import struct
import sys
from dataclasses import dataclass, field
from typing import Any, Optional

MAGIC = b"7DFP"          # socketid, global at 0x6ac008 (ELF 0.24c); 024d: RVA 0x60d08
PROTOCOL_VERSION = 24    # `currentversion`
MAX_PACKET = 0x10000     # size of the send and receive buffers
TWO_PI = 6.28318530718   # constant used by packangle (double at 0x4a5bc8 in the ELF)
DEFAULT_PORT = 27584     # 0x6bc0, `port=` default
MASTER_FALLBACK = ("216.55.186.104", 27590)   # hard-coded in getmasterserverip

# Packet types (first byte after the magic)
T_QUERY = 0              # client -> server
T_QUERY_REPLY = 1        # server -> client
T_JOIN = 2               # client -> server
T_INPUT = 4              # client -> server
T_STATE = 5              # server -> client
T_NEWGAME = 6            # server -> client
T_DISCONNECT = 7         # client -> server
T_VOICE = 8              # server -> client
T_HEARTBEAT = 0x20       # server -> master
T_LIST = 0x21            # client -> master (request) and master -> client (reply)


class PacketError(ValueError):
    """The datagram cannot be parsed, or the packet cannot be built."""


# --------------------------------------------------------------------------- bitstream

class PacketWriter:
    """Copy of the send side: resetsendpacket, pbitwrite, cs_pwrite, sendpacket."""

    def __init__(self) -> None:
        self.buf = bytearray(MAX_PACKET + 16)
        self.pos = 0
        self.bit = 0
        self.write_bytes(MAGIC)           # resetsendpacket writes socketid with cs_pwrite

    # -- cursor (serversend rewinds to patch the event count)
    def tell(self) -> tuple[int, int]:
        return self.pos, self.bit

    def seek(self, where: tuple[int, int]) -> None:
        self.pos, self.bit = where

    def _align(self) -> None:
        if self.bit > 0:
            self.pos += 1
            self.bit = 0

    def write_bits(self, value: int, nbits: int) -> None:
        """pbitwrite: `nbits` low bits of `value`, LSB first. Also serves pbitwritenego/sign."""
        if self.pos >= MAX_PACKET:
            raise PacketError("send buffer full")
        if not 0 <= nbits <= 32:
            raise PacketError(f"bad bit count {nbits}")
        v = value & ((1 << nbits) - 1)
        n = nbits
        while n > 0:
            room = 8 - self.bit
            take = min(n, room)
            mask = ((1 << take) - 1) << self.bit
            self.buf[self.pos] = (self.buf[self.pos] & ~mask & 0xFF) | ((v << self.bit) & mask)
            v >>= take
            n -= take
            self.bit += take
            if self.bit == 8:
                self.pos += 1
                self.bit = 0

    def write_bytes(self, data: bytes) -> None:
        """cs_pwrite: align to a byte boundary, then copy raw bytes."""
        if self.pos >= MAX_PACKET:
            raise PacketError("send buffer full")
        self._align()
        if self.pos + len(data) > MAX_PACKET:
            raise PacketError("send buffer overflow")
        self.buf[self.pos:self.pos + len(data)] = data
        self.pos += len(data)

    # typed helpers
    def u8(self, v: int) -> None: self.write_bytes(struct.pack("<B", v & 0xFF))
    def i32(self, v: int) -> None: self.write_bytes(struct.pack("<i", _wrap_i32(v)))
    def u32(self, v: int) -> None: self.write_bytes(struct.pack("<I", v & 0xFFFFFFFF))
    def f32(self, v: float) -> None: self.write_bytes(struct.pack("<f", v))
    def vec3(self, v) -> None: self.write_bytes(struct.pack("<3f", *v))      # packposition

    def angle(self, x: float, nbits: int) -> None: self.write_bits(pack_angle(x, nbits), nbits)
    def scalar(self, scale: float, x: float, nbits: int) -> None:
        self.write_bits(pack_scalar(scale, x, nbits), nbits)
    def orientation(self, v, nbits: int) -> None: self.write_bits(pack_orientation(v, nbits), nbits)

    def getvalue(self) -> bytes:
        """What sendpacket sends: a partial last byte counts as a whole byte."""
        end = self.pos + (1 if self.bit > 0 else 0)
        return bytes(self.buf[:end])


class PacketReader:
    """Copy of the receive side: recvpacket, pbitread, cs_pread."""

    def __init__(self, data: bytes) -> None:
        if len(data) < 5:
            raise PacketError("datagram shorter than magic + type")
        if len(data) > MAX_PACKET:
            raise PacketError("datagram longer than 65536 bytes")
        if data[:4] != MAGIC:
            raise PacketError("bad magic (socketid)")
        self.data = bytes(data)
        self.len = len(data)
        self.pos = 4
        self.bit = 0

    def read_bits(self, nbits: int) -> int:
        """pbitread. The original only checks `len >= pos + nbits//8`, so it may read up to
        7 bits past the end (stale buffer bytes). We read zeros there."""
        if self.pos >= MAX_PACKET or self.len < (nbits >> 3) + self.pos:
            raise PacketError("read past end of packet")
        out = 0
        got = 0
        n = nbits
        while n > 0:
            byte = self.data[self.pos] if self.pos < self.len else 0
            room = 8 - self.bit
            take = min(n, room)
            out |= ((byte >> self.bit) & ((1 << take) - 1)) << got
            got += take
            n -= take
            self.bit += take
            if self.bit == 8:
                self.pos += 1
                self.bit = 0
        return out

    def read_bytes(self, n: int) -> bytes:
        """cs_pread: the length check uses the position BEFORE aligning (as the original)."""
        if self.pos >= MAX_PACKET or self.len < self.pos + n:
            raise PacketError("read past end of packet")
        if self.bit > 0:
            self.pos += 1
            self.bit = 0
        out = self.data[self.pos:self.pos + n]
        if len(out) < n:          # only possible through the 1-byte slack above
            out += bytes(n - len(out))
        self.pos += n
        return out

    def remaining(self) -> int:
        return max(0, self.len - self.pos - (1 if self.bit else 0))

    # typed helpers
    def nego(self, nbits: int) -> int:
        """pbitreadnego: all ones means -1."""
        v = self.read_bits(nbits)
        return -1 if v == (1 << nbits) - 1 else v
    def u8(self) -> int: return self.read_bytes(1)[0]
    def i32(self) -> int: return struct.unpack("<i", self.read_bytes(4))[0]
    def u32(self) -> int: return struct.unpack("<I", self.read_bytes(4))[0]
    def f32(self) -> float: return struct.unpack("<f", self.read_bytes(4))[0]
    def vec3(self) -> tuple[float, float, float]: return struct.unpack("<3f", self.read_bytes(12))
    def angle(self, nbits: int) -> float: return unpack_angle(self.read_bits(nbits), nbits)
    def scalar(self, scale: float, nbits: int) -> float:
        return unpack_scalar(scale, self.read_bits(nbits), nbits)
    def orientation(self, nbits: int): return unpack_orientation(self.read_bits(nbits), nbits)


def _wrap_i32(v: int) -> int:
    v &= 0xFFFFFFFF
    return v - (1 << 32) if v & 0x80000000 else v


# --------------------------------------------------------------------------- quantizers

def pack_angle(x: float, nbits: int) -> int:
    """packangle: sign bit + (nbits-1)-bit magnitude of |x| in [0, 2*pi), truncated, wrapping."""
    half = (1 << nbits) >> 1
    v = int((half / TWO_PI) * abs(x)) & (half - 1)
    if x < 0.0:
        v |= half
    return v


def unpack_angle(v: int, nbits: int) -> float:
    half = (1 << nbits) >> 1
    x = struct.unpack("<f", struct.pack("<f", (v & (half - 1)) / (half / TWO_PI)))[0]
    return -x if (v & half) else x


def pack_scalar(scale: float, x: float, nbits: int) -> int:
    """packscalar: sign bit + magnitude |x|/scale scaled to nbits-1 bits, clamped (not wrapped)."""
    half = (1 << nbits) >> 1
    v = int((half / scale) * abs(x))
    if v > half - 1:
        v = half - 1
    if x < 0.0:
        v |= half
    return v


def unpack_scalar(scale: float, v: int, nbits: int) -> float:
    half = (1 << nbits) >> 1
    x = (v & (half - 1)) / (half / scale)
    return -x if (v & half) else x


# Octahedron used by packorientation: `octahedron` (6 vertices) and `octahedronindex`
# (8 octants x 3 vertex indices), read from .data of the ELF 0.24c.
_OCT = [(0.0, -1.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 0.0, -1.0),
        (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0)]
_OCT_IDX = [(5, 3, 4), (5, 4, 1), (5, 2, 3), (5, 1, 2),
            (4, 3, 0), (1, 4, 0), (3, 2, 0), (2, 1, 0)]


def _add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def _cross(a, b):
    # crossproduct() of the game: (a.z*b.y - a.y*b.z, ...) = b x a, the opposite of the usual a x b.
    return (a[2] * b[1] - a[1] * b[2], a[0] * b[2] - a[2] * b[0], a[1] * b[0] - a[0] * b[1])


def _norm(a):
    n = math.sqrt(_dot(a, a))
    return a if n == 0.0 else (a[0] / n, a[1] / n, a[2] / n)


def pack_orientation(p, nbits: int) -> int:
    """packorientation: 3 bits pick one of 8 octahedron faces (bit0: x<0, bit1: z<0, bit2: y<0),
    then every 2 bits subdivide the triangle into 4 (0,1,2 = corner at a,b,c; 3 = centre)."""
    code = (1 if p[0] < 0.0 else 0) | (2 if p[2] < 0.0 else 0) | (4 if p[1] < 0.0 else 0)
    ia, ib, ic = _OCT_IDX[code]
    a, b, c = _OCT[ia], _OCT[ib], _OCT[ic]
    for i in range(3, nbits, 2):
        t0, t1, t2 = _norm(_add(a, b)), _norm(_add(b, c)), _norm(_add(c, a))
        if _dot(_cross(_sub(p, t2), _sub(t0, t2)), p) <= 0.0:
            if _dot(_cross(_sub(p, t0), _sub(t1, t0)), p) <= 0.0:
                if _dot(_cross(_sub(p, t1), _sub(t2, t1)), p) <= 0.0:
                    code |= 3 << i
                    a, b, c = t0, t1, t2
                else:
                    code |= 2 << i
                    a, b = t2, t1
            else:
                code |= 1 << i
                a, c = t0, t1
        else:
            b, c = t0, t2
    return code & ((1 << nbits) - 1)


def unpack_orientation(v: int, nbits: int):
    ia, ib, ic = _OCT_IDX[v & 7]
    a, b, c = _OCT[ia], _OCT[ib], _OCT[ic]
    for i in range(3, nbits, 2):
        t0, t1, t2 = _norm(_add(a, b)), _norm(_add(b, c)), _norm(_add(c, a))
        d = (v >> i) & 3
        if d == 0:
            b, c = t0, t2
        elif d == 1:
            a, c = t0, t1
        elif d == 2:
            a, b = t2, t1
        else:
            a, b, c = t0, t1, t2
    return _norm(_add(_add(a, b), c))
