"""Test the paths of the network functions that the recordings may not reach.

Reads a recording snapshot, builds one situation (a join, a kick, a chat command, a radio...)
and writes a new snapshot and an args file with the calls of one function, in one folder per
case. With --run the script runs the harness on each case and deletes the case at once (a
snapshot is about 120 MB):

    python -I tools/lift/synth_net.py --run [--no-cover] <recording dir> <work dir> [case,...]
    python -I tools/lift/synth_net.py --invalid --run [--timeout 15] <recording dir> <work dir> [case,...]

The invalid mode checks the planned input policy. It can fail before the server fixes.
Use step.sh --invalid --rec DIR --synth net FUNCTIONS for both gates.

The functions: serverrecv (33650), server_playermessage (49940), calculateplayervoice (3cfe0),
the codecs of the packets (pbitreadnego, packscalar, unpackangle, packunitvector,
packpositiondelta, unpackpositiondelta, unpackorientation) and, with the client cases,
unpackevent, unpackobjectpacket, clientrecv and packobjectpacket2. Use a recording with
players, connections and voicechat (play-20261012 or murilo-20261015-222536-subvulpescode).
A case named garbage_* reads memory that the packet does not set (a cut packet leaves a local
uninitialized), so the result is not the same in each run. Name it to run it.
The cover count is the number of basic blocks of the function that the case reaches.
"""
import copy
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'net'))
from synth_bonds import Snapshot, find, HUMANS, HUMAN_SIZE, ITEMS, ITEM_SIZE
import srpacket as sp

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / 'original/blackhouser/subrosadedicated024d.exe'

# RVAs of the globals.
PLAYERS, PLAYER_SIZE = 0xededd00, 0x2408
CONNS, CONN_SIZE, NUMCONN = 0x15e39020, 0xf170, 0x1c4afc0
SERVER, OPTION, GAME, TICKS = 0x761ea60, 0x90dc340, 0x90dc540, 0x90dc578
ACCOUNT_KEY, NUMACC, ACCOUNTS, ACCOUNT_SIZE = 0xf437b60, 0xf437b64, 0xf437b68, 0x4c
KICKED, NUMKICKED, DEBUGIT = 0x103485a0, 0x10370c00, 0x204020
SPEEX_RING, RING_SIZE = 0x12a81d00 + 0x10000, 0x9210

# Player fields.
P_ACTIVE, P_NAME, P_ACCOUNT_ID, P_IS_ADMIN, P_TRIES, P_ACCOUNT, P_BLOCK = 0x0, 0x4, 0x24, 0x2c, 0x30, 0x34, 0x38
P_MONEY, P_GODMODE, P_HUMAN, P_NUMACT, P_ACTIONS, P_IS_BOT = 0x40, 0x44, 0x78, 0xe4, 0xec, 0x1a38
# Connection fields.
C_IP, C_PORT, C_GAME_ID, C_ADMIN, C_PLAYER, C_IDLE = 0x0, 0x4, 0x8, 0xc, 0x10, 0x1c
C_VOICE, C_SPECTATE = 0x54, 0x154
# Account fields.
A_NAME, A_MONEY, A_LOOK, A_BAN = 0x8, 0x2c, 0x34, 0x48

VERSION = 24
BUF = 0x1952d680 + 0x10070 + 0x1000  # a free place in the receive buffer, for the codec cases


def image_bytes(rva, n):
    """Bytes of the 024d file at an RVA (the sections are read from the PE header)."""
    d = EXE.read_bytes()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    count, opt = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
    for i in range(count):
        o = pe + 24 + opt + i * 40
        vs, va, rs, ra = struct.unpack_from('<IIII', d, o + 8)
        if va <= rva < va + max(vs, rs):
            return d[ra + rva - va:ra + rva - va + n]
    raise ValueError(hex(rva))


def packet(obj):
    w = sp.PacketWriter()
    obj.write(w)
    return w.getvalue()


def recv_line(data, ip, port, ret=None):
    """The E line of a datagram: the data, the address of the sender and the result."""
    frm = b'\x02\x00' + struct.pack('>H', port) + bytes(ip) + bytes(8)
    n = len(data) if ret is None else ret
    return 'E recvfrom V:24c V:10000 V:0 V:10 o:%s o:%s r:%x' % (data.hex(), frm.hex(), n)


class Case:
    """One snapshot with helpers to write players, connections and accounts."""

    def __init__(self, base, ctx, share=False):
        self.s = base if share else copy.deepcopy(base)
        self.ctx = ctx

    def rd(self, rva, fmt='i'):
        return self.s.read(rva, fmt)[0]

    def wr(self, rva, v, fmt='i'):
        self.s.write(rva, fmt, v)

    def player(self, p, off, v, fmt='i'):
        self.wr(PLAYERS + p * PLAYER_SIZE + off, v, fmt)

    def conn(self, i, off, v, fmt='i'):
        self.wr(CONNS + i * CONN_SIZE + off, v, fmt)

    def text(self, p, k, s):
        """Put a chat text in the action ring of a player. Returns the RVA of the text."""
        rva = PLAYERS + p * PLAYER_SIZE + P_ACTIONS + k * 0x54 + 0x14
        for i, b in enumerate(s.ljust(64, b'\0')[:64]):
            self.s.write(rva + i, 'B', b)
        return rva

    def account(self, i, ids, ban=0, money=1000):
        a = ACCOUNTS + i * ACCOUNT_SIZE
        for k in range(0, ACCOUNT_SIZE, 4):
            self.s.write(a + k, 'i', 0)
        self.s.write(a, 'II', *ids)
        for k, b in enumerate(b'tester'):
            self.s.write(a + A_NAME + k, 'B', b)
        self.s.write(a + A_MONEY, 'i', money)
        self.s.write(a + A_LOOK, 'iii', 1, 2, 3)
        self.s.write(a + A_BAN, 'i', ban)
        if i >= self.rd(NUMACC):
            self.wr(NUMACC, i + 1)

    def addr(self):
        """The ip and the port of the connection 0."""
        c = self.ctx
        return c['ip'], c['port']


def join_packet(ids, name=b'Tester', version=VERSION):
    return packet(sp.Join(version, name, ids))


def input_packet(game_id, actions=(), voice=None, spectate=-1, flag=1, start=0, axes=None):
    voice = voice if voice is not None else [sp.VoiceFrame(i, 1, bytes([i + 1] * (i + 2))) for i in range(6)]
    axes = axes or (0.5, 0.0, -0.5, 1.0, 1.25, -0.25, 0.0, 0.0)
    return packet(sp.Input(game_id, axes, 0x1234, 1, 2, 77, (1.0, 2.0, 3.0), start, list(actions), voice, flag,
                           spectate))


def recv_cases(c):
    """serverrecv: each case is a list of args lines."""
    ctx = c.ctx
    ip, port = c.addr()
    other = (10, 1, 2, 3)
    rnd = c.rd(GAME + 0x1c)
    pa = ctx['p']
    acc = c.rd(PLAYERS + pa * PLAYER_SIZE + P_ACCOUNT)
    out = {}

    def case(name, packets, edit=None, addr=None):
        def build(cc):
            if edit:
                edit(cc)
            a = addr or (ip, port)
            return ['R 33650'] + [recv_line(p, *a) if isinstance(p, bytes) else recv_line(p[0], *p[1]) for p in packets]
        out[name] = build

    case('idle', [])
    case('query7', [packet(sp.Query(7, b'\1\2\3\4'))])
    case('query6', [packet(sp.Query(6, b'\1\2\3\4'))])
    case('join_new', [join_packet((0x1111, 0x2222))], addr=(other, 5000))
    case('join_new_debug', [join_packet((0x1112, 0x2223))], lambda cc: cc.wr(DEBUGIT, 1), addr=(other, 5000))
    case('join_known', [join_packet((0x1111, 0x2222))])
    case('join_full', [join_packet((0x1111, 0x2222))], lambda cc: cc.wr(SERVER + 0x5c, 0), addr=(other, 5000))
    case('join_oldversion', [join_packet((0x1111, 0x2222), version=23)], addr=(other, 5000))
    case('join_empty_name', [join_packet((0x1111, 0x2222), name=b'  ')], addr=(other, 5000))

    def take_over(cc):
        cc.player(pa, P_ACCOUNT, 0)
        cc.account(0, (0x3333, 0x4444))
        cc.player(pa, P_ACCOUNT_ID, 0x3333)
        cc.player(pa, P_ACCOUNT_ID + 4, 0x4444)
    case('join_takeover', [join_packet((0x3333, 0x4444))], take_over, addr=(other, 5000))

    def banned(cc):
        cc.account(cc.rd(NUMACC), (0x5555, 0x6666), ban=5)
    case('join_banned', [join_packet((0x5555, 0x6666))], banned, addr=(other, 5000))

    def adopt(gametype):
        def f(cc):
            n = cc.rd(NUMACC)
            cc.account(n, (0x7777, 0x8888), money=-5)
            h = ctx['h']
            cc.s.write(HUMANS + h * HUMAN_SIZE + 0xc, 'i', n)
            cc.s.write(HUMANS + h * HUMAN_SIZE + 0x8, 'i', -1)
            cc.wr(GAME + 8, gametype)
            cc.wr(DEBUGIT, 1)
        return f
    case('join_adopt', [join_packet((0x7777, 0x8888))], adopt(1), addr=(other, 5000))
    case('join_adopt4', [join_packet((0x7777, 0x8888))], adopt(4), addr=(other, 5000))
    case('join_short', [join_packet((0x1111, 0x2222))[:20]], addr=(other, 5000))
    case('disconnect', [packet(sp.Disconnect())])
    case('disconnect_unknown', [packet(sp.Disconnect())], addr=(other, 5000))
    case('unknown_type', [b'7DFP\x09\x01\x02\x03'])
    case('garbage_short4', [b'7DFP'])
    case('wrong_magic', [b'XXXX\x04\x00'])

    def kicked(cc):
        cc.wr(NUMKICKED, 2)
        cc.s.write(KICKED, 'II', 0x01020304, struct.unpack('>I', bytes(ip))[0])
    case('kicked', [packet(sp.Query(7))], kicked)

    def timeout(cc):
        cc.conn(ctx['conn'], C_IDLE, 0x4af)
    case('timeout', [], timeout)

    def ban_conn(cc):
        cc.account(5, (0x9999, 0xaaaa), ban=3)
        cc.player(pa, P_ACCOUNT, 5)
    case('banned_connection', [], ban_conn)
    case('no_account', [], lambda cc: cc.player(pa, P_ACCOUNT, -1))
    case('bad_account_index', [], lambda cc: cc.player(pa, P_ACCOUNT, 99999))
    case('player_minus1', [], lambda cc: cc.conn(ctx['conn'], C_PLAYER, -1))

    case('input', [input_packet(rnd)])
    case('input_spectate', [input_packet(rnd, spectate=3)])
    case('input_stale', [input_packet(rnd + 1)])
    case('input_unknown_sender', [input_packet(rnd)], addr=(other, 5000))
    acts = [sp.Action(0, 3, 12345), sp.Action(1, 0, 2, b'hello world'), sp.Action(2, 5, 7)]
    case('input_actions', [input_packet(rnd, acts)])
    case('input_actions_wrap', [input_packet(rnd, acts * 5, start=60)])
    case('input_actions_high', [input_packet(rnd, acts, start=200)])
    case('input_chat_empty', [input_packet(rnd, [sp.Action(1, 0, 0, b'')])])
    case('input_voice_big', [input_packet(rnd, voice=[sp.VoiceFrame(100 + i, 2, bytes([9] * 200)) for i in range(6)])])
    case('input_voice_empty', [input_packet(rnd, voice=[sp.VoiceFrame(i, 0, b'') for i in range(6)])])
    full = input_packet(rnd, acts)
    case('garbage_input_cut_voice', [full[:len(full) - 30]])
    case('garbage_input_cut_actions', [full[:90]])
    case('garbage_input_cut_head', [full[:20]])
    case('input_two_packets', [input_packet(rnd), packet(sp.Query(7)), packet(sp.Disconnect())])
    case('input_player_minus1', [input_packet(rnd)], lambda cc: cc.conn(ctx['conn'], C_PLAYER, -1))
    return out


def message_cases(c):
    """server_playermessage: a text from the player p."""
    ctx = c.ctx
    p = ctx['p']
    out = {}
    word = image_bytes(0x5efec, 6)

    def case(name, text, admin=1, edit=None):
        def build(cc):
            cc.player(p, P_IS_ADMIN, admin)
            if edit:
                edit(cc)
            rva = cc.text(p, 0, text)
            return ['R 49940 V:%x M:%x' % (p, rva)]
        out[name] = build

    case('plain', b'hello there', 0)
    case('upper', b'HeLLo ThErE', 0)
    case('filter', b'you ' + word + b' x', 0)
    case('filter_twice', word + b' ' + word, 0)
    case('muted', b'hello', 0, lambda cc: cc.player(p, P_BLOCK, 1))
    case('no_player', b'hello', 0, lambda cc: None)
    case('long', b'x' * 63, 0)
    password = bytes(c.s.read(OPTION + 0x88, '50s')[0]).split(b'\0')[0]
    case('admin_ok', b'/admin ' + password, 0)
    case('admin_wrong', b'/admin nope', 0)
    case('admin_tries', b'/admin ' + password, 0, lambda cc: cc.player(p, P_TRIES, 4))
    case('admin_toomany', b'/admin ' + password, 0, lambda cc: cc.player(p, P_TRIES, 9))
    case('admin_noconn', b'/admin ' + password, 0, lambda cc: cc.conn(ctx['conn'], C_PLAYER, -1))
    case('notadmin_cmd', b'/cash', 0)
    for name, text in {
        'hlist': b'/hlist', 'list1': b'/list1', 'godmode': b'/godmode', 'cash': b'/cash',
        'resetgame': b'/resetgame', 'bann': b'/bann', 'clearkick': b'/clearkick', 'arm': b'/army',
        'unknown': b'/whatever', 'message': b'/message a message for all', 'heli': b'/heli',
        'ban1': b'/ban1 2', 'ban1_bad': b'/ban1 zzz', 'ban1_range': b'/ban1 99999', 'ban1_empty': b'/ban1 ',
        'bany': b'/bany 77', 'bany_text': b'/bany zz', 'give': b'/give 1 500', 'give_bad': b'/give q',
        'kick': b'/kick Tester', 'kick_none': b'/kick Nobody',
    }.items():
        case(name, text)
    case('bany_candidate', b'/bany 77', 1, lambda cc: cc.wr(SERVER + 0x7c, 2))
    case('bany_candidate_text', b'/bany zz', 1, lambda cc: cc.wr(SERVER + 0x7c, 2))
    case('bany_short_msg', b'/bany zz', 1, lambda cc: cc.wr(SERVER + 0x7c, 2))
    case('godmode_off', b'/godmode', 1, lambda cc: cc.player(p, P_GODMODE, 1))

    def many(cc):
        n = 25
        for i in range(2, n):
            cc.conn(i, C_PLAYER, p if i % 3 else -1)
        cc.wr(NUMCONN, n)
    case('list2', b'/list2', 1, many)
    case('list3', b'/list3', 1, many)
    case('list2_few', b'/list2')
    case('list3_few', b'/list3')

    def named(cc):
        cc.player(p, P_NAME, 0)
        for i, b in enumerate(b'Tester'):
            cc.s.write(PLAYERS + p * PLAYER_SIZE + P_NAME + i, 'B', b)
        cc.conn(ctx['conn'], C_PLAYER, p)
    case('kick_named', b'/kick Tester', 1, named)
    case('phone', b'/phone')
    case('phone_nohuman', b'/phone', 1, lambda cc: cc.player(p, P_HUMAN, -1))
    return out


def voice_cases(c):
    """calculateplayervoice: the connection and the player of ctx."""
    ctx = c.ctx
    p, conn = ctx['p'], ctx['conn']
    out = {}

    def case(name, edit=None, voicechat=1, args=None):
        def build(cc):
            cc.wr(OPTION + 0x12c, voicechat)
            if edit:
                edit(cc)
            a = args or (conn, p)
            return ['R 3cfe0 V:%x V:%x' % a]
        out[name] = build

    case('off', voicechat=0)
    case('basic')

    def near(cc):
        h0 = cc.rd(PLAYERS + p * PLAYER_SIZE + P_HUMAN)
        for q in ctx['others']:
            hq = cc.rd(PLAYERS + q * PLAYER_SIZE + P_HUMAN)
            if hq != -1 and h0 != -1:
                pos = cc.s.read(HUMANS + h0 * HUMAN_SIZE + 0xc8, 'fff')
                cc.s.write(HUMANS + hq * HUMAN_SIZE + 0xc8 + 0, 'fff', pos[0] + 3, pos[1], pos[2])
                cc.s.write(HUMANS + hq * HUMAN_SIZE + 0xc8 + 0xe0 * 2, 'fff', pos[0] + 3, pos[1], pos[2])
    case('near', near)

    def slots(cc):
        near(cc)
        for k, q in enumerate(ctx['others'][:3]):
            s = CONNS + conn * CONN_SIZE + C_VOICE + k * 0x20
            cc.s.write(s, 'iiii', 1, q, cc.rd(PLAYERS + q * PLAYER_SIZE + P_HUMAN), -1)
            cc.s.write(s + 0x1c, 'f', float(10 * (k + 1)))
    case('slots', slots)

    def full_slots(cc):
        for k in range(8):
            s = CONNS + conn * CONN_SIZE + C_VOICE + k * 0x20
            q = ctx['others'][k % len(ctx['others'])]
            cc.s.write(s, 'iiii', 1, q, cc.rd(PLAYERS + q * PLAYER_SIZE + P_HUMAN), -1)
            cc.s.write(s + 0x1c, 'f', float(5 + 3 * k))
        cc.s.write(CONNS + conn * CONN_SIZE + 0x50, 'f', 100.0)
        near(cc)
    case('full', full_slots)

    def full_far(cc):
        full_slots(cc)
        cc.s.write(CONNS + conn * CONN_SIZE + 0x50, 'f', 70000.0)
    case('full_far', full_far)

    def full_nan(cc):
        full_slots(cc)
        cc.s.write(CONNS + conn * CONN_SIZE + 0x70, 'I', 0x7fc00000)
    case('full_nan', full_nan)

    def voice_off(cc):
        near(cc)
        for q in ctx['others']:
            cc.s.write(SPEEX_RING + q * RING_SIZE + 0x920c, 'i', 1)
    case('voice_off', voice_off)
    case('spectator', lambda cc: (near(cc), cc.player(p, P_HUMAN, -1), cc.conn(conn, C_SPECTATE, 1)))
    case('nobody', lambda cc: (near(cc), cc.player(p, P_HUMAN, -1), cc.conn(conn, C_SPECTATE, -1)))
    case('dead_speaker', lambda cc: (slots(cc), cc.player(ctx['others'][0], P_ACTIVE, 0)))

    def radio(cc):
        h0 = cc.rd(PLAYERS + p * PLAYER_SIZE + P_HUMAN)
        q = ctx['others'][0]
        hq = cc.rd(PLAYERS + q * PLAYER_SIZE + P_HUMAN)
        a, b = ctx['free'][:2]
        for i, (it, other, human) in enumerate(((a, b, hq), (b, a, h0))):
            base = ITEMS + it * ITEM_SIZE
            for k in range(0, ITEM_SIZE, 4):
                cc.s.write(base + k, 'i', 0)
            cc.s.write(base, 'i', 1)
            cc.s.write(base + 0xc, 'i', 0x17)
            cc.s.write(base + 0x18, 'i', human)
            cc.s.write(base + 0x1c, 'i', -1)
            cc.s.write(base + 0x20, 'i', 0)
            cc.s.write(base + 0x48, 'fff', 5.0 + 20 * i, 5.0, 5.0)
            cc.s.write(base + 0xf8, 'i', other)
            cc.s.write(base + 0x110, 'i', 3)
        return a, b
    out_radio = {}

    def radio_case(name, edit=None):
        def build(cc):
            cc.wr(OPTION + 0x12c, 1)
            a, b = radio(cc)
            if edit:
                edit(cc, a, b)
            return ['R 3cfe0 V:%x V:%x' % (conn, p)]
        out_radio[name] = build
    radio_case('radio')
    radio_case('radio_slot', lambda cc, a, b: cc.s.write(CONNS + conn * CONN_SIZE + C_VOICE, 'iiii', 1,
               cc.rd(HUMANS + cc.rd(ITEMS + b * ITEM_SIZE + 0x18) * HUMAN_SIZE + 0x8), -1, a))
    radio_case('radio_unlinked', lambda cc, a, b: cc.s.write(ITEMS + b * ITEM_SIZE + 0xf8, 'i', -1))
    radio_case('radio_far', lambda cc, a, b: cc.s.write(ITEMS + b * ITEM_SIZE + 0x48, 'fff', 900.0, 0.0, 0.0))
    radio_case('radio_state', lambda cc, a, b: cc.s.write(ITEMS + b * ITEM_SIZE + 0x110, 'i', 2))
    out.update(out_radio)
    return out


def codec_cases(c):
    """The codecs of the packets, on the buffers of the snapshot."""
    out = {}
    f = lambda x: '%08x' % struct.unpack('<I', struct.pack('<f', x))[0]
    nan = '7fc00000'

    def case(name, line):
        out[name] = lambda cc: [line]
    for i, x in enumerate((0.0, 0.3, -0.3, 1.0, -1.0, 5.0, 123.0, -0.0)):
        case('packscalar_%d' % i, 'R 16860 P:0:%s V:%s V:6' % (struct.pack('<f', x).hex(), f(1.0)))
    case('packscalar_nan', 'R 16860 P:0:%s V:%s V:8' % (struct.pack('<I', 0x7fc00000).hex(), f(1.0)))
    case('packscalar_range', 'R 16860 P:0:%s V:%s V:8' % (struct.pack('<f', 7.0).hex(), f(4.0)))
    case('packscalar_bits1', 'R 16860 P:0:%s V:%s V:1' % (struct.pack('<f', 0.5).hex(), f(1.0)))
    case('packscalar_bits13', 'R 16860 P:0:%s V:%s V:d' % (struct.pack('<f', -0.5).hex(), f(1.0)))
    def bit_widths(cc):
        load_recv(cc, bytes((i * 73 + 11) & 255 for i in range(256)))
        return ['R db20 M:%x V:%x' % (BUF, bits)
                for _ in range(8) for bits in (0, 1, 2, 4, 6, 8, 12, 16, 21, 31, 32)]
    out['pbitread_widths'] = bit_widths

    for i, bits in enumerate((8, 16, 6, 12)):
        case('pbitreadnego_%d' % i, 'R ddd0 P:0:00000000 V:%x' % bits)
    for i, bits in enumerate((8, 16, 31, 5)):
        case('unpackangle_%d' % i, 'R 16710 P:0:00000000 V:%x' % bits)
    for i, v in enumerate(((0.1, 0.2, 0.9), (-0.9, 0.2, 0.1), (0.1, -0.8, 0.3), (0.0, 0.0, -1.0), (1.0, 1.0, 1.0),
                           (0.0, 0.0, 0.0))):
        blob = struct.pack('<3f', *v).hex()
        case('packunitvector_%d' % i, 'R 5160 P:0:00000000000000000000000000000000 P:0:%s V:%x' % (blob, 10 + i))
    case('packunitvector_nan', 'R 5160 P:0:0000000000000000 P:0:%s V:8' % struct.pack('<3I', 0x7fc00000, 0, 0x3f800000).hex())
    A, B, OUT = BUF, BUF + 0x20, BUF + 0x40

    def delta(name, line, a, b):
        def build(cc):
            cc.s.write(A, '3f', *a)
            cc.s.write(B, '3f', *b)
            return [line]
        out[name] = build
    delta('packpositiondelta', 'R 2efd0 M:%x M:%x V:%s V:10' % (A, B, f(8.0)), (1.5, -2.0, 0.25), (1.0, 3.0, 0.25))
    delta('packpositiondelta_big', 'R 2efd0 M:%x M:%x V:%s V:8' % (A, B, f(1.0)), (100.0, -2.0, 0.25), (1.0, 3.0, 0.25))
    delta('packpositiondelta_nan', 'R 2efd0 M:%x M:%x V:%s V:8' % (A, B, f(1.0)), (float('nan'), 0.0, 0.0), (1.0, 3.0, 0.25))
    delta('unpackpositiondelta', 'R 2f090 M:%x M:%x V:%s V:10' % (OUT, B, f(8.0)), (0, 0, 0), (1.0, 3.0, 0.25))
    delta('unpackpositiondelta_8', 'R 2f090 M:%x M:%x V:%s V:8' % (OUT, B, f(1.0)), (0, 0, 0), (1.0, 3.0, 0.25))
    for i, bits in enumerate((31, 21, 9, 3, 12)):
        case('unpackorientation_%d' % i, 'R 16320 P:0:000000000000000000000000 V:%x' % bits)
    return out

# BsdSocket receive cursor, for the cases that read a packet from the buffer.
BSD = 0x1952d680
RECV_SIZE, RECV_POS, RECV_BIT, RECV_BUF = BSD + 0x10064, BSD + 0x10068, BSD + 0x1006c, BSD + 0x10070
EVENTS, EVENT_SIZE = 0x93dd880, 0x80
OBJECTS, OBJECT_SIZE = 0x5592600, 0x8230
CITYOBJECTS, CITYOBJECT_SIZE = 0x15e24020, 0x54


def load_recv(cc, data, pos=4):
    """Put data in the receive buffer, as recvpacket leaves it."""
    for i, b in enumerate(data):
        cc.s.write(RECV_BUF + i, 'B', b)
    cc.wr(RECV_SIZE, len(data))
    cc.wr(RECV_POS, pos)
    cc.wr(RECV_BIT, 0)


def event_bytes(ev):
    w = sp.PacketWriter()
    ev.write(w)
    return w.getvalue()


SAMPLE_EVENTS = {
    0: dict(weapon=3, shooter=77, pos=(1.0, 2.0, 3.0), vel=(0.5, -0.25, 9.0)),
    1: dict(kind=2, target=40, pos=(4.0, 5.0, 6.0), normal=(0.0, 1.0, 0.0)),
    2: dict(text=b'hello from the server', attach_type=0, attach_id=-1, range_mode=1),
    3: dict(object=55, object_type=9, variant=-1),
    4: dict(object=56, arg_a=3, arg_b=700, vec_a=(1.0, 1.5, 2.0), vec_b=(3.0, 3.5, 4.0)),
    5: dict(item_key=0x12345, parent_human=-1, parent_item=7, parent_slot=3),
    6: dict(item=100, call_state=3, info=4321, info2=1),
    7: dict(packed=0x1234, human=-1, money=-5000, look=0x2abc, name=b'Playername'),
    8: dict(player=9, money=123456, stocks=-4),
    9: dict(sound=12, pos=(7.0, 8.0, 9.0), volume=0.75, pitch=1.25),
    10: dict(door=2, open=1),
    11: dict(a=1, b=-2, c=3, d=-4),
    12: dict(a=10, b=-20, c=30, d=-40),
    13: dict(a=100, b=200, c=300),
    14: dict(a=5, b=6, c=7, d=0x12345),
    15: dict(a=11, b=12, c=13),
    16: dict(a=1, b=-2, c=3, d=-1, vec_a=(1.0, 2.0, 3.0), vec_b=(0.0, 0.0, 1.0)),
    17: dict(human=300, active=1, player=-1),
}


def event_cases(c):
    """unpackevent: one event of each type in the receive buffer, and some broken ones."""
    out = {}

    def case(name, data, pos=4, index=0x1234):
        def build(cc):
            load_recv(cc, data, pos)
            return ['R 17b60 V:%x' % index]
        out[name] = build
    head = b'7DFP'
    for t, f in SAMPLE_EVENTS.items():
        case('unpackevent_%d' % t, head + event_bytes(sp.Event(t, 5000, f)))
    case('unpackevent_filler', head + event_bytes(sp.Event(sp.EV_FILLER)))
    case('unpackevent_nego', head + event_bytes(sp.Event(3, 9, dict(object=1, object_type=2, variant=15))))
    case('unpackevent_text_empty', head + event_bytes(sp.Event(2, 9, dict(text=b''))))
    case('unpackevent_text_long', head + event_bytes(sp.Event(2, 9, dict(text=b'x' * 63))))
    for t in (2, 4, 7, 9, 16):
        data = head + event_bytes(sp.Event(t, 5000, SAMPLE_EVENTS[t]))
        case('unpackevent_cut_%d' % t, data[:len(data) - 9])
    return out


def world_blocks(ctx):
    h = ctx['h']
    pos = (10.0, 20.0, 30.0)
    oa, ob = (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)
    joints = [(0.1 * k, -0.2, 0.3) for k in range(14)]
    return dict(
        humans=[sp.HumanBlock(h, 0, -1, 2, 80, 1, pos, oa, ob, joints)],
        humans_car=[sp.HumanBlock(h, 0, 3, 1, 50, 0, pos, (0.0, 0.6, 0.8), (0.0, -0.8, 0.6), joints[:8])],
        objects=[sp.ObjectBlock(5, 0, pos, oa, ob, [0.1, -0.2, 0.3, 0.4, 0.5, -0.6],
                                [0.1, -0.2, 0.3, 0.4, 0.5, -0.6, 0.7, 0.9], (0.5, -0.25, 1.0), 1234)],
        objects1=[sp.ObjectBlock(6, 1, pos, (0.0, 0.0, 1.0), (1.0, 0.0, 0.0), [0.0] * 6, [0.0] * 8,
                                 (0.0, 0.0, 0.0), 77)],
        objects2=[sp.ObjectBlock(7, 2, pos)],
        items=[sp.ItemBlock(ctx['free'][0], pos, oa, ob), sp.ItemBlock(ctx['free'][1], pos, ob, oa, (3, 4, 0x1234567))],
        cityobjects=[sp.CityObjectBlock(9, pos, oa, ob, 3, 4, -0.5)],
    )


def object_cases(c):
    """unpackobjectpacket: the four lists in the receive buffer."""
    out = {}
    blocks = world_blocks(c.ctx)

    def case(name, humans=(), objects=(), items=(), cityobjects=()):
        def build(cc):
            w = sp.PacketWriter()
            for lst in (humans, objects, items, cityobjects):
                w.write_bits(len(lst), 8)
                for b in lst:
                    b.write(w)
            load_recv(cc, w.getvalue())
            return ['R 1dba0']
        out[name] = build
    case('unpackobjectpacket_humans', blocks['humans'])
    case('unpackobjectpacket_humans_car', blocks['humans_car'])
    case('unpackobjectpacket_objects', objects=blocks['objects'])
    case('unpackobjectpacket_objects1', objects=blocks['objects1'])
    case('unpackobjectpacket_objects2', objects=blocks['objects2'])
    case('unpackobjectpacket_items', items=blocks['items'])
    case('unpackobjectpacket_cityobjects', cityobjects=blocks['cityobjects'])
    case('unpackobjectpacket_all', blocks['humans'], blocks['objects'] + blocks['objects1'] + blocks['objects2'],
         blocks['items'], blocks['cityobjects'])
    case('unpackobjectpacket_empty')
    return out


def state_packet(ctx, rnd, ticks, blocks=None, **kw):
    blocks = blocks or {}
    st = sp.State(game_id=rnd, tick=ticks, phase=kw.get('phase', 1), phase_timer=777, world=0x12345,
                  team_counts=(3, 4, 5), player=ctx['p'], human=kw.get('human', -1), look_yaw=1.25, look_pitch=-0.5,
                  menu=kw.get('menu', 0), menu_building=0, shop=[(1, 2, 3), (4, 5, 6)], money=-777, action_ack=9,
                  health=88, unk_15f0=5, unk_15f4=6, inventory=[[1, 2], [], [3], [], [], [4, 5, 6], []],
                  event_total=0x1234, first_event=kw.get('first', 0x1234), events=kw.get('events', []))
    st.humans = blocks.get('humans', [])
    st.objects = blocks.get('objects', [])
    st.items = blocks.get('items', [])
    st.cityobjects = blocks.get('cityobjects', [])
    w = sp.PacketWriter()
    st.write(w)
    return w.getvalue()


def client_cases(c):
    """clientrecv: the packets of the server, read by the client."""
    ctx = c.ctx
    ip, port = c.addr()
    rnd = c.rd(GAME + 0x1c)
    ticks = c.rd(TICKS)
    blocks = world_blocks(ctx)
    out = {}

    def case(name, packets, edit=None):
        def build(cc):
            if edit:
                edit(cc)
            return ['R 2e310'] + [recv_line(p, ip, port) for p in packets]
        out[name] = build
    evs = [sp.Event(t, 5000, f) for t, f in SAMPLE_EVENTS.items()]
    allb = dict(humans=blocks['humans'], objects=blocks['objects'] + blocks['objects1'],
                items=blocks['items'], cityobjects=blocks['cityobjects'])
    case('clientrecv_idle', [])
    case('clientrecv_timeout', [], lambda cc: cc.wr(0x1954d700 + 0x28, 0x4af))
    case('clientrecv_newgame', [packet(sp.NewGame())])
    case('clientrecv_state', [state_packet(ctx, rnd, ticks + 1, allb, human=ctx['h'], events=evs[:10])])
    case('clientrecv_state_shop', [state_packet(ctx, rnd, ticks + 1, allb, menu=10, events=evs[10:])])
    case('clientrecv_state_phase2', [state_packet(ctx, rnd, ticks + 1, allb, phase=2)])
    case('clientrecv_state_old', [state_packet(ctx, rnd, ticks - 5, allb)])
    case('clientrecv_state_skip', [state_packet(ctx, rnd, ticks - 50, allb)])
    case('clientrecv_state_other_round', [state_packet(ctx, rnd + 1, ticks + 1, allb)])
    case('clientrecv_state_next_event', [state_packet(ctx, rnd, ticks + 1, allb, events=evs[:3])],
         lambda cc: cc.wr(0xf437318, 0x1234))

    def held(cc):
        base = ITEMS + ctx['free'][0] * ITEM_SIZE
        cc.s.write(base, 'i', 1)
        cc.s.write(base + 0x18, 'ii', ctx['h'], -1)
        cc.s.write(base + 0x20, 'i', 2)
    case('clientrecv_state_items_held', [state_packet(ctx, rnd, ticks + 1, dict(items=blocks['items']))], held)
    full = state_packet(ctx, rnd, ticks + 1, allb, human=ctx['h'], events=evs[:6])
    case('garbage_clientrecv_state_cut', [full[:len(full) - 25]])
    case('garbage_clientrecv_state_cut_early', [full[:60]])
    voice = [sp.VoiceSlot(3, -1, [sp.VoiceFrame(i, 1, bytes([i + 1] * 5)) for i in range(4)]) if i % 2 == 0 else None
             for i in range(8)]
    case('clientrecv_voice', [packet(sp.Voice(voice))])
    case('clientrecv_voice_item', [packet(sp.Voice([sp.VoiceSlot(-1, 9, [sp.VoiceFrame(60 + i, 2, b'')
                                                                          for i in range(4)])] + [None] * 7))])
    case('clientrecv_two', [packet(sp.Voice([None] * 8)), full])
    return out


def packevent_cases(c):
    """packevent: one event in the ring, written for a connection."""
    ctx = c.ctx
    p, conn = ctx['p'], ctx['conn']
    out = {}
    IDX = 0x2000

    def case(name, typ, admin=0, age=0, edit=None, **f):
        def build(cc):
            e = EVENTS + IDX * EVENT_SIZE
            for k in range(0, EVENT_SIZE, 4):
                cc.s.write(e + k, 'i', 0)
            cc.s.write(e, 'ii', typ, cc.rd(TICKS) - age)
            cc.s.write(e + 0x8, 'fff', *f.get('pos', (1.0, 2.0, 3.0)))
            cc.s.write(e + 0x14, 'fff', *f.get('vec', (0.5, 0.25, -1.0)))
            cc.s.write(e + 0x20, 'iiii', f.get('param', 5), f.get('attach_type', 1), f.get('attach_id', 2),
                       f.get('range_mode', 1))
            cc.s.write(e + 0x30, 'ff', 0.75, 1.5)
            for i, b in enumerate(f.get('text', b'chat text here').ljust(64, b'\0')[:64]):
                cc.s.write(e + 0x40 + i, 'B', b)
            cc.conn(conn, C_ADMIN, admin)
            if edit:
                edit(cc)
            return ['R 16f10 V:%x V:%x' % (IDX, conn)]
        out[name] = build
    for t in range(0, 19):
        case('packevent_%d' % t, t, 1, param=0x1235 if t != 2 else 11)
    for t in range(0, 19):
        case('packevent_user_%d' % t, t, 0, param=0x1235 if t != 2 else 11, attach_type=3)
    case('packevent_old_message', 2, 1, age=700, attach_type=1)
    case('packevent_old_message3', 2, 1, age=700, attach_type=3)
    case('packevent_old_sound', 9, 1, age=700)
    case('packevent_admin_msg_user', 2, 0, attach_type=4)
    case('packevent_admin_msg_admin', 2, 1, attach_type=4)
    other = ctx['others'][0]

    def human_pos(cc):
        hid = cc.rd(PLAYERS + other * PLAYER_SIZE + P_HUMAN)
        cc.s.write(CONNS + conn * CONN_SIZE + 0xf164, 'fff', 0.0, 0.0, 0.0)
        cc.s.write(HUMANS + hid * HUMAN_SIZE + 0xc8 + 0xe0 * 2, 'fff', 1.0, 1.0, 1.0)
    for rm in (0, 1, 2):
        case('packevent_range_human_%d' % rm, 2, 0, attach_type=1, attach_id=other, range_mode=rm, edit=human_pos)
        case('packevent_range_item_%d' % rm, 2, 0, attach_type=2, attach_id=ctx['free'][0], range_mode=rm)

    def far(cc):
        human_pos(cc)
        cc.s.write(CONNS + conn * CONN_SIZE + 0xf164, 'fff', 900.0, 0.0, 0.0)
    case('packevent_far_human', 2, 0, attach_type=1, attach_id=other, range_mode=0, edit=far)

    def own_text(cc):
        for i, b in enumerate(b'what I said'.ljust(64, b'\0')):
            cc.s.write(PLAYERS + p * PLAYER_SIZE + 0x23c8 + i, 'B', b)
        cc.conn(conn, C_PLAYER, p)
    hp = c.rd(PLAYERS + p * PLAYER_SIZE + P_HUMAN)
    case('packevent_own_text', 2, 0, attach_type=1, attach_id=hp, edit=own_text)
    case('packevent_own_text_other', 2, 0, attach_type=1, attach_id=hp + 1, edit=own_text)
    case('packevent_no_player', 2, 0, attach_type=1, attach_id=hp, edit=lambda cc: cc.conn(conn, C_PLAYER, -1))
    case('packevent_attach_none', 2, 0, attach_type=1, attach_id=-1)
    return out


def item_at(cc, i, pos, vel=(0.0, 0.0, 0.0), angvel=(0.0, 0.0, 0.0), ph=-1, pi=-1, slot=0, typ=1):
    a = ITEMS + i * ITEM_SIZE
    for k in range(0, ITEM_SIZE, 4):
        cc.s.write(a + k, 'i', 0)
    cc.s.write(a, 'i', 1)
    cc.s.write(a + 0xc, 'i', typ)
    cc.s.write(a + 0x18, 'iii', ph, pi, slot)
    cc.s.write(a + 0x48, 'fff', *pos)
    cc.s.write(a + 0x6c, 'fff', *vel)
    cc.s.write(a + 0xb4, 'fff', *angvel)
    cc.s.write(a + 0x84, 'fffffffff', 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)


def objectpacket_cases(c):
    """packobjectpacket and packobjectpacket2: the conn and the player of ctx."""
    ctx = c.ctx
    p, conn, h = ctx['p'], ctx['conn'], ctx['h']
    out = {}
    first, second = ctx['free'][:2]

    def case(rva, name, edit=None, seq=0, behind=0, args=None):
        def build(cc):
            cc.conn(conn, 0x4c, seq, 'I')
            if behind:
                cc.wr(0xf437318, 100 + behind)
                cc.conn(conn, 0x44, 100)
            if edit:
                edit(cc)
            return ['R %s V:%x V:%x' % ((rva,) + (args or (conn, p)))]
        out[name] = build

    def cars(cc):
        for k in range(20):
            o = OBJECTS + k * OBJECT_SIZE
            cc.s.write(o, 'iii', 1, 12 if k == 3 else 1, 3 if k == 7 else 1)
            cc.s.write(o + 0x24, 'fff', 5.0 * k, 3.0, 2.0)
            cc.s.write(o + 0x3c, 'fffffffff', 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)

    def city(cc):
        for k in range(0, 100, 3):
            cc.s.write(CITYOBJECTS + k * CITYOBJECT_SIZE, 'i', 1)
            cc.s.write(CITYOBJECTS + k * CITYOBJECT_SIZE + 0x8, 'fff', float(k), 1.0, 2.0)

    def many_items(cc):
        for k in range(300):
            item_at(cc, 1023 - k, (5.0, 5.0, 5.0))
    for rva, tag in (('20680', 'packobjectpacket'), ('1f4a0', 'packobjectpacket2')):
        case(rva, tag + '_basic')
        case(rva, tag + '_behind', behind=40)
        case(rva, tag + '_behind_small', behind=20)
        case(rva, tag + '_nohuman', edit=lambda cc: cc.player(p, P_HUMAN, -1))
        case(rva, tag + '_noplayer', args=(conn, 0xffffffff))
        case(rva, tag + '_dead', edit=lambda cc: cc.s.write(HUMANS + h * HUMAN_SIZE + 0x28, 'i', 0))
        for seq in (0, 1, 3, 5, 0x40):
            case(rva, tag + '_seq%d' % seq, seq=seq, edit=lambda cc: (
                item_at(cc, first, (5.0, 5.0, 5.0)), item_at(cc, second, (500.0, 5.0, 5.0), (3.0, 0.0, 0.0))))
        case(rva, tag + '_items_moving', seq=7, edit=lambda cc: (
            item_at(cc, first, (50.0, 5.0, 5.0), (0.5, 0.0, 0.0)),
            item_at(cc, second, (50.0, 5.0, 5.0), (0.0, 0.0, 0.0), (0.0, 0.5, 0.0))))
        case(rva, tag + '_items_still', seq=7, edit=lambda cc: (
            item_at(cc, first, (50.0, 5.0, 5.0)), item_at(cc, second, (50.0, 5.0, 5.0), ph=0, slot=1)))
        case(rva, tag + '_items_held', seq=7, edit=lambda cc: (
            item_at(cc, first, (5.0, 5.0, 5.0), ph=0, slot=1), item_at(cc, second, (5.0, 5.0, 5.0), ph=0, slot=3)))
        case(rva, tag + '_items_nan', seq=7, edit=lambda cc: item_at(
            cc, first, (50.0, 5.0, 5.0), (float('nan'), 0.0, 0.0), (float('nan'), 0.0, 0.0)))
        case(rva, tag + '_items_cash', edit=lambda cc: (
            item_at(cc, first, (5.0, 5.0, 5.0), typ=0x10),
            cc.s.write(ITEMS + first * ITEM_SIZE + 0x134, 'iiI', 3, 4, 0x1234567)))
        case(rva, tag + '_items_many', edit=many_items)
        case(rva, tag + '_objects', edit=cars)
        case(rva, tag + '_objects_ownvehicle', edit=lambda cc: (
            cars(cc), cc.s.write(HUMANS + h * HUMAN_SIZE + 0x14, 'i', 4)))
        case(rva, tag + '_humans_vehicle', edit=lambda cc: [
            cc.s.write(HUMANS + k * HUMAN_SIZE + 0x14, 'i', 4) for k in range(0, 40, 2)])
        case(rva, tag + '_cityobjects', edit=city)
    return out



FUNCS = {'serverrecv': recv_cases, 'server_playermessage': message_cases, 'calculateplayervoice': voice_cases,
         'codec': codec_cases, 'unpackevent': event_cases, 'unpackobjectpacket': object_cases,
         'clientrecv': client_cases, 'packevent': packevent_cases, 'packobjectpacket': objectpacket_cases}
RVA_NAME = {'55700': 'logic_playeractions', '33650': 'serverrecv', '49940': 'server_playermessage', '3cfe0': 'calculateplayervoice',
            'db20': 'pbitread', 'd9f0': 'cs_pread',
            '16860': 'packscalar', 'ddd0': 'pbitreadnego', '16710': 'unpackangle', '5160': 'packunitvector',
            '2efd0': 'packpositiondelta', '2f090': 'unpackpositiondelta', '16320': 'unpackorientation', '17b60': 'unpackevent', '1dba0': 'unpackobjectpacket',
            '2e310': 'clientrecv', '16f10': 'packevent', '20680': 'packobjectpacket', '1f4a0': 'packobjectpacket2'}


def set_recv_buffer(cc, data):
    """Put a packet in the receive buffer, for the codec cases that read one."""
    return data


def run_harness(d, fname, cover=True):
    """Run the harness on one case. Returns the result line and the number of blocks reached."""
    cmd = [str(ROOT / 'build/hook/harness.exe'), str(ROOT / 'build/hook/024d_fixed.exe'),
           str(d / 'sv_snapshot.bin'), str(d / 'sv_args.txt'), fname]
    env = dict(os.environ, HARNESS_ENV_DIR=str(d / 'env'))
    covered = dict(env, HARNESS_COVER=str(ROOT / 'build/lift/blocks.txt'))
    blocks = []
    if cover:
        out = subprocess.run(cmd, capture_output=True, text=True, errors='replace', env=covered, cwd=ROOT).stdout
        blocks = [int(ln.split()[4]) for ln in out.splitlines() if ln.startswith('cover ') and len(ln.split()) > 4]
    out = subprocess.run(cmd, capture_output=True, text=True, errors='replace', env=env, cwd=ROOT).stdout
    res = [ln for ln in out.splitlines() if 'functions tested' in ln]
    return (res[-1] if res else 'NO RESULT: ' + out[-300:]), (blocks[0] if blocks else 0)


def context(base):
    """Indexes of a player with a connection, a human, some other players and free items."""
    n = base.read(NUMCONN, 'i')[0]
    conn = next(i for i in range(n) if base.read(CONNS + i * CONN_SIZE + C_PLAYER, 'i')[0] != -1)
    p = base.read(CONNS + conn * CONN_SIZE + C_PLAYER, 'i')[0]
    ip = struct.pack('>I', base.read(CONNS + conn * CONN_SIZE, 'I')[0])
    port = base.read(CONNS + conn * CONN_SIZE + C_PORT, 'i')[0] & 0xffff
    others = [q for q in range(256) if q != p and base.read(PLAYERS + q * PLAYER_SIZE, 'i')[0]
              and base.read(PLAYERS + q * PLAYER_SIZE + P_HUMAN, 'i')[0] != -1]
    humans = [h for h in range(512) if base.read(HUMANS + h * HUMAN_SIZE, 'i')[0] == 1
              and base.read(HUMANS + h * HUMAN_SIZE + 0x8, 'i')[0] == -1]
    h = (humans or [0])[0]
    free = [i for i in range(1023, 0, -1) if base.read(ITEMS + i * ITEM_SIZE, 'i')[0] == 0]
    return dict(p=p, conn=conn, ip=tuple(ip), port=port, others=others, h=h, free=free)


def main():
    if '--invalid' in sys.argv:
        from synth_security import main as security_main
        return security_main([a for a in sys.argv[1:] if a != '--invalid'])
    run = '--run' in sys.argv
    if run:
        sys.argv.remove('--run')
    cover = '--no-cover' not in sys.argv
    if not cover:
        sys.argv.remove('--no-cover')
    rec, out = Path(sys.argv[1]), Path(sys.argv[2])
    base = Snapshot(rec / 'sv_snapshot.bin')
    ctx = context(base)
    print({k: (v if not isinstance(v, list) else v[:4]) for k, v in ctx.items()})
    want = sys.argv[3].split(',') if len(sys.argv) > 3 else None
    for group, make in FUNCS.items():
        c0 = Case(base, ctx, share=True)
        cases = make(c0)
        for name, build in cases.items():
            if want and name not in want and group not in want:
                continue
            if name.startswith('garbage_') and (not want or name not in want):
                continue
            if name in {'input_actions_high', 'input_voice_big', 'input_player_minus1', 'join_takeover', 'join_short',
                        'phone_nohuman', 'message'}:
                if want and name in want:
                    if name == 'message':
                        raise SystemExit('message: use --invalid with message_short or message_max')
                    raise SystemExit(name + ': use --invalid for the security expectation')
                continue
            cc = Case(base, ctx)
            args = build(cc)
            rva = args[0].split()[1]
            d = out / name
            d.mkdir(parents=True, exist_ok=True)
            cc.s.save(d / 'sv_snapshot.bin')
            (d / 'sv_args.txt').write_text('\n'.join(args) + '\n')
            if run:
                result, blocks = run_harness(d, RVA_NAME[rva], cover)
                print('%-24s %-22s blocks %3d  %s' % (name, RVA_NAME[rva], blocks, result), flush=True)
                shutil.rmtree(d, ignore_errors=True)
            else:
                print('case', name, args[0])


if __name__ == '__main__':
    sys.exit(main())
