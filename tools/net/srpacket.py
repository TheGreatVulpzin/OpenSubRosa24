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


# --------------------------------------------------------------------------- events

EV_BULLET, EV_BULLETHIT, EV_MESSAGE, EV_CREATEOBJECT, EV_UPDATEOBJECT, EV_UPDATEITEM, \
    EV_UPDATEITEMINFO, EV_UPDATEPLAYER, EV_UPDATEPLAYER_FINANCE, EV_SOUND, EV_UPDATEDOOR, \
    EV_UPDATESCENARIO, EV_UPDATEMISSION, EV_UPDATESTOCKS, EV_UPDATECARMARKET, \
    EV_UPDATEBLOCK_INTERIOR, EV_UPDATEBLOCK_QUAD, EV_UPDATEHUMAN = range(18)
EV_FILLER = 0x12         # "nothing here": 6 bits, no tick, no body

EVENT_NAMES = {
    0: "bullet", 1: "bullethit", 2: "message", 3: "createobject", 4: "updateobject",
    5: "updateitem", 6: "updateiteminfo", 7: "updateplayer", 8: "updateplayer_finance",
    9: "sound", 10: "updatedoor", 11: "updatescenario", 12: "updatemission", 13: "updatestocks",
    14: "updatecarmarket", 15: "updateblock_interior", 16: "updateblock_quad", 17: "updatehuman",
    0x12: "filler",
}

# Field kinds: u = unsigned bits, n = bits where all-ones means -1, i32 = raw int32,
# f32 = raw float32, v3 = raw 3 x float32. Order is the wire order (packevent/unpackevent).
EVENT_FIELDS: dict[int, list[tuple]] = {
    0: [("u", "weapon", 4), ("u", "shooter", 10), ("v3", "pos"), ("v3", "vel")],
    1: [("u", "kind", 4), ("u", "target", 6), ("v3", "pos"), ("v3", "normal")],
    # 2 (message) and 7 (updateplayer) have strings and are handled in code.
    3: [("u", "object", 10), ("u", "object_type", 8), ("n", "variant", 4)],
    4: [("u", "object", 10), ("u", "arg_a", 4), ("u", "arg_b", 10), ("v3", "vec_a"), ("v3", "vec_b")],
    5: [("u", "item_key", 20), ("n", "parent_human", 10), ("n", "parent_item", 10), ("u", "parent_slot", 4)],
    6: [("u", "item", 10), ("u", "call_state", 3), ("u", "info", 16), ("u", "info2", 2)],
    8: [("u", "player", 8), ("i32", "money"), ("i32", "stocks")],
    9: [("u", "sound", 8), ("v3", "pos"), ("f32", "volume"), ("f32", "pitch")],
    10: [("u", "door", 8), ("u", "open", 1)],
    11: [("i32", "a"), ("i32", "b"), ("i32", "c"), ("i32", "d")],
    12: [("i32", "a"), ("i32", "b"), ("i32", "c"), ("i32", "d")],
    13: [("i32", "a"), ("i32", "b"), ("i32", "c")],
    14: [("u", "a", 8), ("u", "b", 8), ("u", "c", 8), ("u", "d", 20)],
    15: [("i32", "a"), ("i32", "b"), ("i32", "c")],
    16: [("i32", "a"), ("i32", "b"), ("i32", "c"), ("n", "d", 10), ("v3", "vec_a"), ("v3", "vec_b")],
    17: [("u", "human", 10), ("u", "active", 1), ("n", "player", 8)],
}

MESSAGE_MAX = 63          # 6-bit length
PLAYER_EVENT_NAME_CHARS = 31


@dataclass
class Event:
    type: int
    tick: int = 0                       # 28 bits, server tick when created (absent for filler)
    fields: dict = field(default_factory=dict)

    @property
    def name(self) -> str:
        return EVENT_NAMES.get(self.type, f"unknown{self.type}")

    # -- wire
    def write(self, w: PacketWriter) -> None:
        w.write_bits(self.type, 6)
        if self.type == EV_FILLER:
            return
        w.write_bits(self.tick, 28)
        f = self.fields
        if self.type == EV_MESSAGE:
            text = _ascii7(f.get("text", b""))[:MESSAGE_MAX]
            w.write_bits(len(text), 6)
            w.write_bits(f.get("attach_type", 0), 4)
            w.write_bits(f.get("attach_id", -1), 10)
            w.write_bits(f.get("range_mode", 0), 4)
            for ch in text:
                w.write_bits(ch, 7)
        elif self.type == EV_UPDATEPLAYER:
            w.write_bits(f.get("packed", 0), 16)
            w.write_bits(f.get("human", -1), 10)
            w.i32(f.get("money", 0))
            w.write_bits(f.get("look", 0), 16)
            name = _ascii7(f.get("name", b"")).ljust(PLAYER_EVENT_NAME_CHARS, b"\0")
            for ch in name[:PLAYER_EVENT_NAME_CHARS]:
                w.write_bits(ch, 7)
        elif self.type in EVENT_FIELDS:
            for spec in EVENT_FIELDS[self.type]:
                _write_field(w, spec, f.get(spec[1], 0))
        elif self.type > 0x11:
            raise PacketError(f"cannot write event type {self.type}")

    @classmethod
    def read(cls, r: PacketReader) -> "Event":
        t = r.read_bits(6)
        if t == EV_FILLER:
            return cls(t)
        tick = r.read_bits(28)
        f: dict[str, Any] = {}
        if t == EV_MESSAGE:
            n = r.read_bits(6)
            f["attach_type"] = r.read_bits(4)
            f["attach_id"] = r.nego(10)
            f["range_mode"] = r.read_bits(4)
            f["text"] = bytes(r.read_bits(7) for _ in range(n))
        elif t == EV_UPDATEPLAYER:
            f["packed"] = r.read_bits(16)
            f["human"] = r.nego(10)
            f["money"] = r.i32()
            f["look"] = r.read_bits(16)
            raw = bytes(r.read_bits(7) for _ in range(PLAYER_EVENT_NAME_CHARS))
            f["name"] = raw.split(b"\0", 1)[0]
        elif t in EVENT_FIELDS:
            for spec in EVENT_FIELDS[t]:
                f[spec[1]] = _read_field(r, spec)
        return cls(t, tick, f)


def _ascii7(b) -> bytes:
    if isinstance(b, str):
        b = b.encode("latin-1", "replace")
    return bytes(c & 0x7F for c in b)


def _write_field(w: PacketWriter, spec: tuple, value) -> None:
    kind = spec[0]
    if kind in ("u", "n"):
        w.write_bits(int(value), spec[2])
    elif kind == "i32":
        w.i32(int(value))
    elif kind == "f32":
        w.f32(float(value))
    elif kind == "v3":
        w.vec3(value if value else (0.0, 0.0, 0.0))
    else:
        raise AssertionError(kind)


def _read_field(r: PacketReader, spec: tuple):
    kind = spec[0]
    if kind == "u":
        return r.read_bits(spec[2])
    if kind == "n":
        return r.nego(spec[2])
    if kind == "i32":
        return r.i32()
    if kind == "f32":
        return r.f32()
    if kind == "v3":
        return r.vec3()
    raise AssertionError(kind)


def updateplayer_packed(player: int, is_bot: bool, active: bool, team: int) -> int:
    """Event 7 `packed`: (team+1)<<10 | active<<9 | is_bot<<8 | player (createevent_updateplayer)."""
    return ((team + 1) << 10) | (int(active) << 9) | (int(is_bot) << 8) | (player & 0xFF)


def split_updateplayer_packed(v: int) -> dict:
    return {"player": v & 0xFF, "is_bot": bool(v & 0x100), "active": bool(v & 0x200),
            "team": (v >> 10) - 1}


def updateitem_key(item: int, active: bool, item_type: int) -> int:
    """Event 5 `item_key`: type<<11 | active<<10 | item (createevent_updateitem)."""
    return (item_type << 11) | (int(active) << 10) | (item & 0x3FF)


def split_updateitem_key(v: int) -> dict:
    return {"item": v & 0x3FF, "active": bool(v & 0x400), "type": v >> 11}


# --------------------------------------------------------------------------- packets

def _name32(b) -> bytes:
    if isinstance(b, str):
        b = b.encode("latin-1", "replace")
    return bytes(b[:32]).ljust(32, b"\0")


def _cstr(b: bytes) -> str:
    return b.split(b"\0", 1)[0].decode("latin-1")


@dataclass
class VoiceFrame:
    """One compressed voice frame (the game keeps them in a `speex` array; codec settings were not
    checked). `index` is a position in a 64-entry ring."""
    index: int
    mode: int = 0                 # 2 bits
    data: bytes = b""

    def write(self, w: PacketWriter, index_bits: int) -> None:
        if len(self.data) > 255:
            raise PacketError("voice frame longer than 255 bytes")
        w.write_bits(self.index, index_bits)
        w.write_bits(len(self.data), 8)
        w.write_bits(self.mode, 2)
        w.write_bytes(self.data)

    @classmethod
    def read(cls, r: PacketReader, index_bits: int) -> "VoiceFrame":
        index = r.read_bits(index_bits)
        n = r.read_bits(8)
        mode = r.read_bits(2)
        return cls(index, mode, r.read_bytes(n) if n else b"")


@dataclass
class Query:
    """Type 0, client -> server (or any server): "who are you?"."""
    TYPE = T_QUERY
    version: int = PROTOCOL_VERSION
    nonce: bytes = bytes(4)       # opaque 4 bytes, echoed by the reply (client sends SDL_GetTicks)

    def write(self, w):
        w.u8(self.TYPE)
        w.write_bits(self.version, 8)
        w.write_bytes(self.nonce[:4].ljust(4, b"\0"))

    @classmethod
    def read(cls, r):
        version = r.read_bits(8)
        return cls(version, r.read_bytes(4))


@dataclass
class QueryReply:
    """Type 1, server -> client."""
    TYPE = T_QUERY_REPLY
    version: int = PROTOCOL_VERSION
    nonce: bytes = bytes(4)
    gametype: int = 3             # 4 bits
    players: int = 0              # 8 bits, non-bot players
    max_players: int = 16         # 8 bits
    name: bytes = b""             # 32 bytes, zero padded
    account_key: Optional[int] = None   # only sent when the asking client has version > 6

    @property
    def server_name(self) -> str:
        return _cstr(self.name)

    def write(self, w):
        w.u8(self.TYPE)
        w.write_bits(self.version, 8)
        w.write_bytes(self.nonce[:4].ljust(4, b"\0"))
        w.write_bits(self.gametype, 4)
        w.write_bits(self.players, 8)
        w.write_bits(self.max_players, 8)
        w.write_bytes(_name32(self.name))
        if self.account_key is not None:
            w.u32(self.account_key)

    @classmethod
    def read(cls, r):
        version = r.read_bits(8)
        nonce = r.read_bytes(4)
        gametype = r.read_bits(4)
        players = r.read_bits(8)
        max_players = r.read_bits(8)
        name = r.read_bytes(32)
        key = r.u32() if r.remaining() >= 4 else None
        return cls(version, nonce, gametype, players, max_players, name, key)


@dataclass
class Join:
    """Type 2, client -> server. Sent every 100 ms until a type 6 packet arrives."""
    TYPE = T_JOIN
    version: int = PROTOCOL_VERSION
    name: bytes = b""             # 32 bytes (the client keeps at most 30 characters)
    account_id: tuple = (0, 0)    # two uint32

    @property
    def player_name(self) -> str:
        return _cstr(self.name)

    def write(self, w):
        w.u8(self.TYPE)
        w.write_bits(self.version, 8)
        w.write_bytes(_name32(self.name))
        w.u32(self.account_id[0])
        w.u32(self.account_id[1])

    @classmethod
    def read(cls, r):
        version = r.read_bits(8)
        name = r.read_bytes(32)
        return cls(version, name, (r.u32(), r.u32()))


@dataclass
class NewGame:
    """Type 6, server -> client: "this is game N, load it". Sent until the client echoes N."""
    TYPE = T_NEWGAME
    game_id: int = 0
    gametype: int = 3             # 4 bits
    game_c: int = 0               # 4 bits, game+0xc (0.24c layout)
    angle_a: float = 0.0          # 16-bit angles, game+0x4c / game+0x50
    angle_b: float = 0.0

    def write(self, w):
        w.u8(self.TYPE)
        w.u32(self.game_id)
        w.write_bits(self.gametype, 4)
        w.write_bits(self.game_c, 4)
        w.angle(self.angle_a, 16)
        w.angle(self.angle_b, 16)

    @classmethod
    def read(cls, r):
        game_id = r.u32()
        gametype = r.read_bits(4)
        game_c = r.read_bits(4)
        return cls(game_id, gametype, game_c, r.angle(16), r.angle(16))


@dataclass
class Disconnect:
    """Type 7, client -> server."""
    TYPE = T_DISCONNECT

    def write(self, w): w.u8(self.TYPE)

    @classmethod
    def read(cls, r): return cls()


@dataclass
class Heartbeat:
    """Type 0x20, server -> master server, every 1024 ticks (~17 s)."""
    TYPE = T_HEARTBEAT

    def write(self, w): w.u8(self.TYPE)

    @classmethod
    def read(cls, r): return cls()


@dataclass
class ListRequest:
    """Type 0x21, client -> master server."""
    TYPE = T_LIST

    def write(self, w): w.u8(self.TYPE)

    @classmethod
    def read(cls, r): return cls()


@dataclass
class ListReply:
    """Type 0x21, master server -> client. Layout inferred from the client only."""
    TYPE = T_LIST
    servers: list = field(default_factory=list)   # (ip as host-order uint32, port)

    def write(self, w):
        w.u8(self.TYPE)
        w.u32(len(self.servers))
        for ip, port in self.servers:
            w.u32(ip)
            w.write_bytes(struct.pack("<H", port))

    @classmethod
    def read(cls, r):
        n = r.u32()
        if n >= 0x2000:
            raise PacketError("server list too long")
        out = []
        for _ in range(n):
            ip = r.u32()
            out.append((ip, struct.unpack("<H", r.read_bytes(2))[0]))
        return cls(out)

    @staticmethod
    def ip_str(ip: int) -> str:
        return ".".join(str((ip >> s) & 255) for s in (24, 16, 8, 0))


ACTION_MENU, ACTION_CHAT, ACTION_ITEM = 0, 1, 2


@dataclass
class Action:
    """One entry of the client's action ring (player_addaction_*)."""
    kind: int                     # 0 menu, 1 chat, 2 item
    a: int = 0                    # menu: 8 bits id; item: 16 bits
    b: int = 0                    # menu: int32; chat: 4 bits mode; item: 16 bits
    text: bytes = b""             # chat only, up to 63 bytes

    def write(self, w):
        w.write_bits(self.kind, 4)
        if self.kind == ACTION_MENU:
            w.write_bits(self.a, 8)
            w.i32(self.b)
        elif self.kind == ACTION_CHAT:
            text = self.text[:MESSAGE_MAX]
            w.write_bits(len(text), 6)
            for c in text:                       # cs_pwrite(...,1,1) per byte: re-aligns each time
                w.write_bytes(bytes([c]))
            w.write_bits(self.b, 4)
        elif self.kind == ACTION_ITEM:
            w.write_bits(self.a, 16)
            w.write_bits(self.b, 16)

    @classmethod
    def read(cls, r):
        kind = r.read_bits(4)
        if kind == ACTION_MENU:
            return cls(kind, r.read_bits(8), r.i32())
        if kind == ACTION_CHAT:
            n = r.read_bits(6)
            text = b"".join(r.read_bytes(1) for _ in range(n))
            return cls(kind, n, r.read_bits(4), text)
        if kind == ACTION_ITEM:
            return cls(kind, r.read_bits(16), r.read_bits(16))
        return cls(kind)


@dataclass
class Input:
    """Type 4, client -> server, once per client frame."""
    TYPE = T_INPUT
    game_id: int = 0
    axes: tuple = (0.0,) * 8      # player+0x7c..0x98 (4 axes, yaw, pitch, 2 extra)
    buttons: int = 0              # player+0x9c bitmask
    input_mode: int = 0           # 8 bits
    hud_mode: int = 0             # 4 bits
    event_ack: int = 0            # 16 bits: next event index the client expects
    cam_pos: tuple = (0.0, 0.0, 0.0)
    action_start: int = 0         # 8 bits: ring index of actions[0]
    actions: list = field(default_factory=list)       # at most 15
    voice: list = field(default_factory=list)         # exactly 6 VoiceFrame (index 8 bits)
    voice_flag: int = 0           # 1 bit
    spectate: int = -1            # nego 8: human being spectated, -1 none

    def write(self, w):
        if len(self.actions) > 15:
            raise PacketError("at most 15 actions per packet")
        if len(self.voice) != 6:
            raise PacketError("an input packet carries exactly 6 voice frames")
        w.u8(self.TYPE)
        w.u32(self.game_id)
        for a in self.axes:
            w.f32(a)
        w.u32(self.buttons)
        w.write_bits(self.input_mode, 8)
        w.write_bits(self.hud_mode, 4)
        w.write_bits(self.event_ack, 16)
        w.vec3(self.cam_pos)
        w.write_bits(len(self.actions), 4)
        w.write_bits(self.action_start, 8)
        for act in self.actions:
            act.write(w)
        for fr in self.voice:
            fr.write(w, 8)
        w.write_bits(self.voice_flag, 1)
        w.write_bits(self.spectate, 8)

    @classmethod
    def read(cls, r):
        game_id = r.u32()
        axes = tuple(r.f32() for _ in range(8))
        buttons = r.u32()
        input_mode = r.read_bits(8)
        hud_mode = r.read_bits(4)
        event_ack = r.read_bits(16)
        cam = r.vec3()
        n = r.read_bits(4)
        start = r.read_bits(8)
        actions = [Action.read(r) for _ in range(n)]
        voice = [VoiceFrame.read(r, 8) for _ in range(6)]
        flag = r.read_bits(1)
        spectate = r.nego(8)
        return cls(game_id, axes, buttons, input_mode, hud_mode, event_ack, cam, start, actions,
                   voice, flag, spectate)


@dataclass
class HumanBlock:
    """A human (character) in the object packet. kind 1 has no body (never sent by 0.24)."""
    id: int
    kind: int = 0
    vehicle: int = -1             # nego 8: object the human sits in, -1 none
    seat: int = 0                 # 4 bits
    health: int = 100             # stored as health+10 in 7 bits; decoded as health
    bleeding: int = 0
    pos: tuple = (0.0, 0.0, 0.0)
    orient_a: tuple = (1.0, 0.0, 0.0)
    orient_b: tuple = (0.0, 1.0, 0.0)
    joints: list = field(default_factory=list)   # bones 1..14 (1..8 in a vehicle): 3 angles each

    @staticmethod
    def bones_sent(vehicle: int) -> int:
        return 15 if vehicle == -1 else 9

    def write(self, w):
        w.write_bits(self.id, 10)
        w.write_bits(self.kind, 1)
        if self.kind != 0:
            return
        w.write_bits(self.vehicle, 8)
        w.write_bits(self.seat, 4)
        w.write_bits(max(self.health + 10, 0), 7)
        w.write_bits(self.bleeding, 1)
        w.vec3(self.pos)
        w.orientation(self.orient_a, 31)
        w.orientation(self.orient_b, 31)
        want = self.bones_sent(self.vehicle) - 1
        if len(self.joints) != want:
            raise PacketError(f"human {self.id}: need {want} joints, got {len(self.joints)}")
        for j in self.joints:
            for ang in j:
                w.angle(ang, 16)

    @classmethod
    def read(cls, r):
        hid = r.read_bits(10)
        kind = r.read_bits(1)
        if kind != 0:
            return cls(hid, kind)
        vehicle = r.nego(8)
        seat = r.read_bits(4)
        health = r.read_bits(7) - 10
        bleeding = r.read_bits(1)
        pos = r.vec3()
        oa = r.orientation(31)
        ob = r.orientation(31)
        joints = [tuple(r.angle(16) for _ in range(3)) for _ in range(cls.bones_sent(vehicle) - 1)]
        return cls(hid, 0, vehicle, seat, health, bleeding, pos, oa, ob, joints)


@dataclass
class ObjectBlock:
    """A vehicle/object. mode 0 and 1 carry the full state (31 or 21-bit orientations),
    mode 2 only the position."""
    id: int
    mode: int = 0
    pos: tuple = (0.0, 0.0, 0.0)
    orient_a: tuple = (1.0, 0.0, 0.0)
    orient_b: tuple = (0.0, 1.0, 0.0)
    angles: list = field(default_factory=lambda: [0.0] * 6)     # 8-bit angles
    scalars: list = field(default_factory=lambda: [0.0] * 8)    # 6-bit scalars, scale 1.0
    vel: tuple = (0.0, 0.0, 0.0)                                # 8-bit scalars, scale 1.0
    flags: int = 0                                              # 13 bits (object+0x5ee0)

    def write(self, w):
        w.write_bits(self.id, 10)
        w.write_bits(self.mode, 2)
        w.vec3(self.pos)
        if self.mode in (0, 1):
            bits = 31 if self.mode == 0 else 21
            w.orientation(self.orient_a, bits)
            w.orientation(self.orient_b, bits)
            for a in self.angles:
                w.angle(a, 8)
            for s in self.scalars:
                w.scalar(1.0, s, 6)
            for s in self.vel:
                w.scalar(1.0, s, 8)
            w.write_bits(self.flags, 13)

    @classmethod
    def read(cls, r):
        oid = r.read_bits(10)
        mode = r.read_bits(2)
        pos = r.vec3()
        if mode not in (0, 1):
            return cls(oid, mode, pos)
        bits = 31 if mode == 0 else 21
        oa = r.orientation(bits)
        ob = r.orientation(bits)
        angles = [r.angle(8) for _ in range(6)]
        scalars = [r.scalar(1.0, 6) for _ in range(8)]
        vel = tuple(r.scalar(1.0, 8) for _ in range(3))
        return cls(oid, mode, pos, oa, ob, angles, scalars, vel, r.read_bits(13))


@dataclass
class ItemBlock:
    id: int
    pos: tuple = (0.0, 0.0, 0.0)
    orient_a: tuple = (1.0, 0.0, 0.0)
    orient_b: tuple = (0.0, 1.0, 0.0)
    cash: Optional[tuple] = None  # (item+0x138 4 bits, item+0x134 4 bits, item+0x13c 30 bits) for type 0x10

    def write(self, w):
        w.write_bits(self.id, 10)
        w.vec3(self.pos)
        w.orientation(self.orient_a, 31)
        w.orientation(self.orient_b, 31)
        w.write_bits(1 if self.cash is not None else 0, 1)
        if self.cash is not None:
            w.write_bits(self.cash[0], 4)
            w.write_bits(self.cash[1], 4)
            w.write_bits(self.cash[2], 30)

    @classmethod
    def read(cls, r):
        iid = r.read_bits(10)
        pos = r.vec3()
        oa = r.orientation(31)
        ob = r.orientation(31)
        cash = None
        if r.read_bits(1):
            cash = (r.read_bits(4), r.read_bits(4), r.read_bits(30))
        return cls(iid, pos, oa, ob, cash)


@dataclass
class CityObjectBlock:
    id: int
    pos: tuple = (0.0, 0.0, 0.0)
    orient_a: tuple = (1.0, 0.0, 0.0)
    orient_b: tuple = (0.0, 1.0, 0.0)
    a: int = 0                    # cityobject+0x4c, 7 bits
    b: int = 0                    # cityobject+0x48, 7 bits
    scalar: float = 0.0           # cityobject+0x50, 6-bit scalar, scale 1.0

    def write(self, w):
        w.write_bits(self.id, 10)
        w.vec3(self.pos)
        w.orientation(self.orient_a, 31)
        w.orientation(self.orient_b, 31)
        w.write_bits(self.a, 7)
        w.write_bits(self.b, 7)
        w.scalar(1.0, self.scalar, 6)

    @classmethod
    def read(cls, r):
        cid = r.read_bits(10)
        pos = r.vec3()
        oa = r.orientation(31)
        ob = r.orientation(31)
        return cls(cid, pos, oa, ob, r.read_bits(7), r.read_bits(7), r.scalar(1.0, 6))


SHOP_MENU = 10                  # player.menu value that adds the shop list to the state packet
INVENTORY_SLOTS = 7


@dataclass
class State:
    """Type 5, server -> client: the whole world view for one player."""
    TYPE = T_STATE
    game_id: int = 0
    tick: int = 0
    phase: int = 2                # 4 bits: 1 = waiting, 2 = round running, 3 = round over
    ready: Optional[list] = None  # 32 x 1 bit, present only when phase == 1
    phase_timer: int = 0          # 16 bits
    world: int = 0                # 30 bits (game_world)
    team_counts: tuple = (0, 0, 0)   # 3 x 6 bits
    player: int = 0               # 8 bits: the receiving player's slot
    human: int = -1               # nego 10: the player's human, -1 none
    look_yaw: float = 0.0         # 16-bit angles
    look_pitch: float = 0.0
    cam_pos: tuple = (0.0, 0.0, 0.0)
    menu: int = 0                 # 8 bits
    menu_building: int = 0        # 16 bits
    shop: list = field(default_factory=list)   # menu == 10 only: (8 bits, 24 bits, 4 bits)
    money: int = 0                # int32
    action_ack: int = 0           # 8 bits: number of actions the server has received (ring index)
    health: int = 0               # 8 bits
    unk_15f0: int = 0             # 6 bits, player+0x15f0 (never written by the server logic)
    unk_15f4: int = 0             # 8 bits, player+0x15f4
    inventory: list = field(default_factory=lambda: [[] for _ in range(INVENTORY_SLOTS)])
    humans: list = field(default_factory=list)
    objects: list = field(default_factory=list)
    items: list = field(default_factory=list)
    cityobjects: list = field(default_factory=list)
    event_total: int = 0          # 16 bits: server numofevents
    first_event: int = 0          # 16 bits: index of events[0]
    events: list = field(default_factory=list)

    def write(self, w):
        w.u8(self.TYPE)
        w.u32(self.game_id)
        w.u32(self.tick)
        w.write_bits(self.phase, 4)
        if self.phase == 1:
            ready = self.ready if self.ready is not None else [0] * 32
            if len(ready) != 32:
                raise PacketError("ready needs 32 entries")
            for b in ready:
                w.write_bits(b, 1)
        w.write_bits(self.phase_timer, 16)
        w.write_bits(self.world, 30)
        for c in self.team_counts:
            w.write_bits(c, 6)
        w.write_bits(self.player, 8)
        w.write_bits(self.human, 10)
        w.angle(self.look_yaw, 16)
        w.angle(self.look_pitch, 16)
        w.vec3(self.cam_pos)
        w.write_bits(self.menu, 8)
        w.write_bits(self.menu_building, 16)
        if self.menu == SHOP_MENU:
            w.write_bits(len(self.shop), 8)
            for a, b, c in self.shop:
                w.write_bits(a, 8)
                w.write_bits(b, 24)
                w.write_bits(c, 4)
        w.i32(self.money)
        w.write_bits(self.action_ack, 8)
        w.write_bits(self.health, 8)
        w.write_bits(self.unk_15f0, 6)
        w.write_bits(self.unk_15f4, 8)
        if len(self.inventory) != INVENTORY_SLOTS:
            raise PacketError("inventory needs 7 slots")
        for slot in self.inventory:
            if len(slot) > 15:
                raise PacketError("at most 15 entries per inventory slot")
            w.write_bits(len(slot), 4)
            for v in slot:
                w.write_bits(v, 16)
        for blocks in (self.humans, self.objects, self.items, self.cityobjects):
            if len(blocks) > 255:
                raise PacketError("at most 255 blocks per list")
            w.write_bits(len(blocks), 8)
            for b in blocks:
                b.write(w)
        if len(self.events) > 63:
            raise PacketError("at most 63 events per packet (the server sends at most 31)")
        w.write_bits(self.event_total, 16)
        w.write_bits(len(self.events), 6)
        w.write_bits(self.first_event, 16)
        for ev in self.events:
            ev.write(w)

    @classmethod
    def read(cls, r):
        s = cls()
        s.game_id = r.u32()
        s.tick = r.u32()
        s.phase = r.read_bits(4)
        if s.phase == 1:
            s.ready = [r.read_bits(1) for _ in range(32)]
        s.phase_timer = r.read_bits(16)
        s.world = r.read_bits(30)
        s.team_counts = tuple(r.read_bits(6) for _ in range(3))
        s.player = r.read_bits(8)
        s.human = r.nego(10)
        s.look_yaw = r.angle(16)
        s.look_pitch = r.angle(16)
        s.cam_pos = r.vec3()
        s.menu = r.read_bits(8)
        s.menu_building = r.read_bits(16)
        if s.menu == SHOP_MENU:
            n = r.read_bits(8)
            s.shop = [(r.read_bits(8), r.read_bits(24), r.read_bits(4)) for _ in range(n)]
        s.money = r.i32()
        s.action_ack = r.read_bits(8)
        s.health = r.read_bits(8)
        s.unk_15f0 = r.read_bits(6)
        s.unk_15f4 = r.read_bits(8)
        s.inventory = []
        for _ in range(INVENTORY_SLOTS):
            n = r.read_bits(4)
            s.inventory.append([r.read_bits(16) for _ in range(n)])
        for attr, cls_ in (("humans", HumanBlock), ("objects", ObjectBlock),
                           ("items", ItemBlock), ("cityobjects", CityObjectBlock)):
            n = r.read_bits(8)
            setattr(s, attr, [cls_.read(r) for _ in range(n)])
        s.event_total = r.read_bits(16)
        n = r.read_bits(6)
        s.first_event = r.read_bits(16)
        s.events = [Event.read(r) for _ in range(n)]
        return s


@dataclass
class VoiceSlot:
    human: int = -1               # nego 8
    item: int = -1                # nego 8 (radio item), -1 none
    frames: list = field(default_factory=list)   # exactly 4, index 6 bits


@dataclass
class Voice:
    """Type 8, server -> client, sent right after every type 5 packet."""
    TYPE = T_VOICE
    slots: list = field(default_factory=lambda: [None] * 8)   # 8 entries, None = unused

    def write(self, w):
        if len(self.slots) != 8:
            raise PacketError("a voice packet has 8 slots")
        w.u8(self.TYPE)
        for s in self.slots:
            w.write_bits(0 if s is None else 1, 1)
            if s is None:
                continue
            if len(s.frames) != 4:
                raise PacketError("a voice slot carries exactly 4 frames")
            w.write_bits(s.human, 8)
            w.write_bits(s.item, 8)
            for fr in s.frames:
                fr.write(w, 6)

    @classmethod
    def read(cls, r):
        slots = []
        for _ in range(8):
            if not r.read_bits(1):
                slots.append(None)
                continue
            human = r.nego(8)
            item = r.nego(8)
            slots.append(VoiceSlot(human, item, [VoiceFrame.read(r, 6) for _ in range(4)]))
        return cls(slots)


_CLASSES = {c.TYPE: c for c in (Query, QueryReply, Join, NewGame, Disconnect, Heartbeat,
                                Input, State, Voice)}


def build_packet(pkt) -> bytes:
    """Serialize a packet object to the datagram payload."""
    w = PacketWriter()
    pkt.write(w)
    return w.getvalue()


def parse_packet(data: bytes):
    """Parse a datagram. Type 0x21 is a request when it has no body, a server list otherwise."""
    r = PacketReader(data)
    t = r.u8()
    if t == T_LIST:
        return ListReply.read(r) if len(data) > 5 else ListRequest()
    cls = _CLASSES.get(t)
    if cls is None:
        raise PacketError(f"unknown packet type {t}")
    return cls.read(r)


# --------------------------------------------------------------------------- command line

def _main(argv: list[str]) -> int:
    if len(argv) >= 2 and argv[0] == "dump":
        arg = argv[1]
        try:
            with open(arg) as fh:
                text = fh.read()
        except OSError:
            text = arg
        data = bytes.fromhex("".join(text.split()))
        print(parse_packet(data))
        return 0
    if argv[:1] == ["selftest"]:
        pkt = State(game_id=1, tick=2, humans=[HumanBlock(0, joints=[(0.0, 0.0, 0.0)] * 14)],
                    events=[Event(EV_MESSAGE, 5, {"text": b"hello"})])
        raw = build_packet(pkt)
        print(len(raw), "bytes:", raw[:16].hex(), "...")
        print(parse_packet(raw))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(_main(sys.argv[1:]))
