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



# --------------------------------------------------------------------------- helpers

def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]


def rnd_unit(rng):
    v = [rng.gauss(0, 1) for _ in range(3)]
    n = math.sqrt(sum(c * c for c in v))
    return tuple(c / n for c in v)


def rnd_pos(rng):
    return tuple(f32(rng.uniform(-500, 500)) for _ in range(3))


def assert_same(tc, a, b, path="", tol=0.06):
    """Equal ints/strings/lists; floats within `tol` (8-bit angles are the coarsest field)."""
    if isinstance(a, float) or isinstance(b, float):
        tc.assertLessEqual(abs(a - b), tol, path)
    elif isinstance(a, (list, tuple)):
        tc.assertEqual(len(a), len(b), path)
        for i, (x, y) in enumerate(zip(a, b)):
            assert_same(tc, x, y, f"{path}[{i}]", tol)
    elif hasattr(a, "__dataclass_fields__"):
        tc.assertIs(type(a), type(b), path)
        for name in a.__dataclass_fields__:
            assert_same(tc, getattr(a, name), getattr(b, name), f"{path}.{name}", tol)
    elif isinstance(a, dict):
        tc.assertEqual(a.keys(), b.keys(), path)
        for k in a:
            assert_same(tc, a[k], b[k], f"{path}.{k}", tol)
    else:
        tc.assertEqual(a, b, path)


def roundtrip(tc, pkt, tol=0.06):
    raw = sr.build_packet(pkt)
    tc.assertEqual(raw[:4], b"7DFP")
    back = sr.parse_packet(raw)
    assert_same(tc, pkt, back, type(pkt).__name__, tol)
    return raw, back


# --------------------------------------------------------------------------- handshake packets

class HandshakeTests(unittest.TestCase):
    def test_query_bytes(self):
        raw = sr.build_packet(sr.Query(24, b"\x01\x02\x03\x04"))
        self.assertEqual(raw, b"7DFP" + b"\x00" + b"\x18" + b"\x01\x02\x03\x04")
        self.assertEqual(sr.parse_packet(raw), sr.Query(24, b"\x01\x02\x03\x04"))

    def test_query_reply_bytes(self):
        pkt = sr.QueryReply(24, b"\xAA\xBB\xCC\xDD", gametype=3, players=5, max_players=16,
                            name=b"My server", account_key=0x01020304)
        raw = sr.build_packet(pkt)
        stream = 3 | (5 << 4) | (16 << 12)            # 4 + 8 + 8 bits -> 3 bytes
        want = (b"7DFP" + b"\x01" + b"\x18" + b"\xAA\xBB\xCC\xDD" + stream.to_bytes(3, "little")
                + b"My server".ljust(32, b"\0") + bytes.fromhex("04030201"))
        self.assertEqual(raw, want)
        back = sr.parse_packet(raw)
        self.assertEqual(back.server_name, "My server")
        self.assertEqual(back.account_key, 0x01020304)
        self.assertEqual((back.gametype, back.players, back.max_players), (3, 5, 16))

    def test_query_reply_without_account_key(self):
        pkt = sr.QueryReply(5, bytes(4), 3, 0, 8, b"old client")
        back = sr.parse_packet(sr.build_packet(pkt))
        self.assertIsNone(back.account_key)

    def test_join_bytes(self):
        pkt = sr.Join(24, b"Agent 7", (0xDEADBEEF, 5))
        raw = sr.build_packet(pkt)
        self.assertEqual(raw, b"7DFP\x02\x18" + b"Agent 7".ljust(32, b"\0")
                         + bytes.fromhex("efbeadde") + bytes.fromhex("05000000"))
        self.assertEqual(sr.parse_packet(raw).player_name, "Agent 7")

    def test_newgame(self):
        raw, back = roundtrip(self, sr.NewGame(game_id=77, gametype=3, game_c=2,
                                               angle_a=1.0, angle_b=-2.0), tol=2e-4)
        self.assertEqual(raw[:5], b"7DFP\x06")
        self.assertEqual(raw[5:9], (77).to_bytes(4, "little"))
        self.assertEqual(raw[9], 3 | (2 << 4))
        self.assertEqual(len(raw), 14)

    def test_small_packets(self):
        self.assertEqual(sr.build_packet(sr.Disconnect()), b"7DFP\x07")
        self.assertEqual(sr.build_packet(sr.Heartbeat()), b"7DFP\x20")
        self.assertEqual(sr.build_packet(sr.ListRequest()), b"7DFP\x21")
        self.assertIsInstance(sr.parse_packet(b"7DFP\x07"), sr.Disconnect)
        self.assertIsInstance(sr.parse_packet(b"7DFP\x21"), sr.ListRequest)

    def test_list_reply(self):
        servers = [(0xC0A80001, 27584), (0x7F000001, 27590)]
        raw, back = roundtrip(self, sr.ListReply(servers))
        self.assertEqual(raw, b"7DFP\x21" + (2).to_bytes(4, "little")
                         + bytes.fromhex("0100a8c0") + (27584).to_bytes(2, "little")
                         + bytes.fromhex("0100007f") + (27590).to_bytes(2, "little"))
        self.assertEqual(sr.ListReply.ip_str(servers[0][0]), "192.168.0.1")

    def test_unknown_type(self):
        with self.assertRaises(sr.PacketError):
            sr.parse_packet(b"7DFP\x63")


# --------------------------------------------------------------------------- input packet

def make_voice(rng, n=6, bits=8):
    out = []
    for i in range(n):
        out.append(sr.VoiceFrame(i if bits == 6 else 10 + i, rng.randrange(4),
                                 bytes(rng.getrandbits(8) for _ in range(rng.randrange(0, 40)))))
    return out


class InputTests(unittest.TestCase):
    def test_layout_offsets(self):
        voice = [sr.VoiceFrame(i) for i in range(6)]
        pkt = sr.Input(game_id=0x11223344, axes=tuple(float(i) for i in range(8)),
                       buttons=0xA5, input_mode=2, hud_mode=1, event_ack=0xBEEF,
                       cam_pos=(1.0, 2.0, 3.0), voice=voice)
        raw = sr.build_packet(pkt)
        self.assertEqual(raw[4], 4)
        self.assertEqual(raw[5:9], bytes.fromhex("44332211"))
        self.assertEqual(struct.unpack_from("<8f", raw, 9), tuple(float(i) for i in range(8)))
        self.assertEqual(struct.unpack_from("<I", raw, 41)[0], 0xA5)
        self.assertEqual(raw[45], 2)                          # input_mode: 8 bits
        self.assertEqual(raw[46] & 0x0F, 1)                   # hud_mode: 4 bits
        self.assertEqual(((raw[46] >> 4) | (raw[47] << 4) | (raw[48] << 12)) & 0xFFFF, 0xBEEF)
        self.assertEqual(struct.unpack_from("<3f", raw, 49), (1.0, 2.0, 3.0))
        self.assertEqual(raw[61] & 0x0F, 0)                   # no actions
        sr.parse_packet(raw)

    def test_roundtrip_with_actions_and_voice(self):
        rng = random.Random(7)
        pkt = sr.Input(
            game_id=9, axes=tuple(f32(rng.uniform(-1, 1)) for _ in range(8)), buttons=0x40001,
            input_mode=1, hud_mode=3, event_ack=1234, cam_pos=rnd_pos(rng), action_start=250,
            actions=[sr.Action(sr.ACTION_MENU, 5, -7),
                     sr.Action(sr.ACTION_CHAT, 5, 2, b"hello"),
                     sr.Action(sr.ACTION_ITEM, 400, 0xFFFF),
                     sr.Action(sr.ACTION_CHAT, 0, 0, b"")],
            voice=make_voice(rng), voice_flag=1, spectate=-1)
        raw = sr.build_packet(pkt)
        back = sr.parse_packet(raw)
        self.assertEqual(back.actions[1].text, b"hello")
        self.assertEqual(back.actions[0].b, -7)
        assert_same(self, pkt.actions[0], back.actions[0])
        self.assertEqual([a.kind for a in back.actions], [0, 1, 2, 1])
        self.assertEqual(back.voice, pkt.voice)
        self.assertEqual(back.spectate, -1)
        self.assertEqual((back.action_start, back.event_ack), (250, 1234))

    def test_limits(self):
        with self.assertRaises(sr.PacketError):
            sr.build_packet(sr.Input(voice=[]))
        with self.assertRaises(sr.PacketError):
            sr.build_packet(sr.Input(voice=[sr.VoiceFrame(0)] * 6,
                                     actions=[sr.Action(0)] * 16))

    def test_truncated(self):
        raw = sr.build_packet(sr.Input(voice=[sr.VoiceFrame(0)] * 6))
        with self.assertRaises(sr.PacketError):
            sr.parse_packet(raw[:30])


# --------------------------------------------------------------------------- events

def sample_event(rng, t):
    f = {}
    if t == sr.EV_MESSAGE:
        f = {"attach_type": 1, "attach_id": 17, "range_mode": 2, "text": b"Hello, world"}
    elif t == sr.EV_UPDATEPLAYER:
        f = {"packed": sr.updateplayer_packed(9, False, True, 1), "human": -1, "money": -50,
             "look": 0xABCD, "name": b"Player Name"}
    elif t in sr.EVENT_FIELDS:
        for spec in sr.EVENT_FIELDS[t]:
            k = spec[0]
            if k == "u":
                f[spec[1]] = rng.getrandbits(spec[2])
            elif k == "n":
                f[spec[1]] = rng.choice([-1, rng.getrandbits(spec[2] - 1)])
            elif k == "i32":
                f[spec[1]] = rng.randint(-2**31, 2**31 - 1)
            elif k == "f32":
                f[spec[1]] = f32(rng.uniform(0, 4))
            elif k == "v3":
                f[spec[1]] = rnd_pos(rng)
    return sr.Event(t, rng.getrandbits(28), f)


class EventTests(unittest.TestCase):
    def test_every_type_roundtrips(self):
        rng = random.Random(11)
        for t in range(18):
            ev = sample_event(rng, t)
            w = sr.PacketWriter()
            ev.write(w)
            w.write_bits(0x15, 5)                       # trailer proves the reader stops in the right place
            r = sr.PacketReader(w.getvalue())
            back = sr.Event.read(r)
            self.assertEqual(back, ev, sr.EVENT_NAMES[t])
            self.assertEqual(r.read_bits(5), 0x15, sr.EVENT_NAMES[t])

    def test_filler_is_six_bits(self):
        w = sr.PacketWriter()
        sr.Event(sr.EV_FILLER).write(w)
        self.assertEqual(w.tell(), (4, 6))
        self.assertEqual(w.getvalue()[4], 0x12)
        self.assertEqual(sr.Event.read(sr.PacketReader(w.getvalue())), sr.Event(0x12))

    def test_message_bits(self):
        # type(6) tick(28) len(6) attach_type(4) attach_id(10) range(4) then 7 bits per char
        w = sr.PacketWriter()
        sr.Event(sr.EV_MESSAGE, 0x123, {"attach_type": 4, "attach_id": -1, "range_mode": 0,
                                        "text": b"Hi"}).write(w)
        bits = 2 | (0x123 << 6) | (2 << 34) | (4 << 40) | (0x3FF << 44) | (0 << 54) \
            | (ord("H") << 58) | (ord("i") << 65)
        self.assertEqual(w.getvalue()[4:], bits.to_bytes(9, "little"))

    def test_updateplayer_bits_and_helpers(self):
        packed = sr.updateplayer_packed(200, True, True, 2)
        self.assertEqual(packed, (3 << 10) | 0x200 | 0x100 | 200)
        self.assertEqual(sr.split_updateplayer_packed(packed),
                         {"player": 200, "is_bot": True, "active": True, "team": 2})
        key = sr.updateitem_key(1000, True, 33)
        self.assertEqual(sr.split_updateitem_key(key), {"item": 1000, "active": True, "type": 33})
        self.assertLess(key, 1 << 20)

    def test_long_message_is_cut(self):
        w = sr.PacketWriter()
        sr.Event(sr.EV_MESSAGE, 0, {"text": b"x" * 100}).write(w)
        back = sr.Event.read(sr.PacketReader(w.getvalue()))
        self.assertEqual(len(back.fields["text"]), 63)


# --------------------------------------------------------------------------- state packet

def rnd_human(rng, i, vehicle=-1):
    n = sr.HumanBlock.bones_sent(vehicle) - 1
    return sr.HumanBlock(i, 0, vehicle, rng.randrange(16), rng.randrange(-10, 100),
                         rng.randrange(2), rnd_pos(rng), rnd_unit(rng), rnd_unit(rng),
                         [tuple(rng.uniform(-3, 3) for _ in range(3)) for _ in range(n)])


def rnd_object(rng, i, mode):
    return sr.ObjectBlock(i, mode, rnd_pos(rng), rnd_unit(rng), rnd_unit(rng),
                          [rng.uniform(-3, 3) for _ in range(6)],
                          [rng.uniform(-0.9, 0.9) for _ in range(8)],
                          tuple(rng.uniform(-0.9, 0.9) for _ in range(3)), rng.getrandbits(13))


class StateTests(unittest.TestCase):
    def test_header_bytes(self):
        raw = sr.build_packet(sr.State(game_id=0x01020304, tick=0x0A0B0C0D, phase=2))
        self.assertEqual(raw[:5], b"7DFP\x05")
        self.assertEqual(raw[5:9], bytes.fromhex("04030201"))
        self.assertEqual(raw[9:13], bytes.fromhex("0d0c0b0a"))
        self.assertEqual(raw[13] & 0x0F, 2)

    def test_empty_state_roundtrip(self):
        raw, back = roundtrip(self, sr.State(game_id=3, tick=1000, phase=2, world=12345,
                                             team_counts=(1, 2, 3), money=-100))
        self.assertEqual(back.team_counts, (1, 2, 3))
        self.assertEqual(back.money, -100)

    def test_phase_one_carries_ready_flags(self):
        ready = [i % 2 for i in range(32)]
        raw, back = roundtrip(self, sr.State(phase=1, ready=ready, phase_timer=3600))
        self.assertEqual(back.ready, ready)
        none_raw = sr.build_packet(sr.State(phase=2))
        self.assertEqual(len(raw) - len(none_raw), 4)       # 32 bits

    def test_shop_only_for_menu_10(self):
        shop = [(1, 12345, 3), (2, 0xFFFFFF, 15)]
        with_shop = sr.State(menu=10, menu_building=7, shop=shop)
        raw, back = roundtrip(self, with_shop)
        self.assertEqual(back.shop, shop)
        other = sr.State(menu=9, menu_building=7, shop=shop)
        back = sr.parse_packet(sr.build_packet(other))
        self.assertEqual(back.shop, [])

    def test_full_random_state(self):
        rng = random.Random(21)
        events = [sample_event(rng, t) for t in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17)]
        events.insert(3, sr.Event(sr.EV_FILLER))
        state = sr.State(
            game_id=42, tick=123456, phase=1, ready=[rng.getrandbits(1) for _ in range(32)],
            phase_timer=3000, world=(1 << 30) - 1, team_counts=(5, 6, 7), player=200,
            human=300, look_yaw=1.5, look_pitch=-0.5, cam_pos=rnd_pos(rng),
            menu=10, menu_building=33, shop=[(1, 2, 3)], money=2 ** 31 - 1, action_ack=17,
            health=100, unk_15f0=3, unk_15f4=4,
            inventory=[[0x1234, 5], [], [1], [], [7, 8, 9], [], [0xFFFF]],
            humans=[rnd_human(rng, 1), rnd_human(rng, 2, vehicle=5), sr.HumanBlock(3, kind=1)],
            objects=[rnd_object(rng, 10, 0), rnd_object(rng, 11, 1),
                     sr.ObjectBlock(12, 2, rnd_pos(rng))],
            items=[sr.ItemBlock(5, rnd_pos(rng), rnd_unit(rng), rnd_unit(rng), (3, 4, 123456)),
                   sr.ItemBlock(6, rnd_pos(rng), rnd_unit(rng), rnd_unit(rng))],
            cityobjects=[sr.CityObjectBlock(9, rnd_pos(rng), rnd_unit(rng), rnd_unit(rng), 100, 27, -0.4)],
            event_total=777, first_event=770, events=events)
        raw, back = roundtrip(self, state, tol=0.1)
        self.assertEqual(back.humans[1].vehicle, 5)
        self.assertEqual(len(back.humans[1].joints), 8)      # only 9 bones inside a vehicle
        self.assertEqual(len(back.humans[0].joints), 14)
        self.assertEqual(back.events[3], sr.Event(sr.EV_FILLER))
        self.assertEqual(back.events[2].fields["text"], events[2].fields["text"])

    def test_human_health_offset(self):
        h = sr.HumanBlock(1, health=-4, joints=[(0, 0, 0)] * 14)
        back = sr.parse_packet(sr.build_packet(sr.State(humans=[h]))).humans[0]
        self.assertEqual(back.health, -4)

    def test_wrong_joint_count_is_rejected(self):
        with self.assertRaises(sr.PacketError):
            sr.build_packet(sr.State(humans=[sr.HumanBlock(1, joints=[(0, 0, 0)])]))

    def test_truncated_state(self):
        raw = sr.build_packet(sr.State(humans=[rnd_human(random.Random(1), 1)]))
        with self.assertRaises(sr.PacketError):
            sr.parse_packet(raw[:60])

    def test_player_never_seen_as_negative(self):
        # human = -1 is the all-ones 10-bit pattern
        back = sr.parse_packet(sr.build_packet(sr.State(human=-1)))
        self.assertEqual(back.human, -1)


class VoiceTests(unittest.TestCase):
    def test_roundtrip(self):
        rng = random.Random(5)
        slots = [None] * 8
        slots[0] = sr.VoiceSlot(3, -1, make_voice(rng, 4, 6))
        slots[5] = sr.VoiceSlot(-1, 40, make_voice(rng, 4, 6))
        raw, back = roundtrip(self, sr.Voice(slots))
        self.assertEqual(raw[:5], b"7DFP\x08")
        self.assertEqual(back.slots[0].frames, slots[0].frames)
        self.assertIsNone(back.slots[1])

    def test_empty_voice_is_one_byte_of_flags(self):
        raw = sr.build_packet(sr.Voice())
        self.assertEqual(raw, b"7DFP\x08\x00")


if __name__ == "__main__":
    unittest.main()
