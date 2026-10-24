"""Check the input policy, case setup, and failure reporting without game execution."""
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import synth_security as security

net = security.net


class SparseSnapshot:
    def __init__(self):
        self.data = {}

    def read(self, rva, fmt):
        return struct.unpack('<' + fmt, bytes(self.data.get(rva + i, 0) for i in range(struct.calcsize(fmt))))

    def write(self, rva, fmt, *values):
        for i, byte in enumerate(struct.pack('<' + fmt, *values)):
            self.data[rva + i] = byte


class SecurityTests(unittest.TestCase):
    def case(self):
        ctx = dict(p=0, h=0, conn=0, ip=(10, 20, 30, 40), port=5000)
        cc = net.Case(SparseSnapshot(), ctx)
        cc.wr(net.NUMCONN, 1)
        cc.conn(0, net.C_IP, 0x0a141e28, 'I')
        return cc

    def test_action_and_voice_masks_preserve_wire_bytes(self):
        for start in (0, 63, 64, 127, 200, 255):
            for actions in ([], [net.sp.Action(0, 6, -100000)],
                            [net.sp.Action(1, 0, 0, b'hello'), net.sp.Action(2, 5, 7)]):
                voice = [net.sp.VoiceFrame(index, 2, b'abc') for index in (0, 63, 64, 127, 200, 255)]
                raw = net.input_packet(17, actions, voice=voice, start=start)
                expected = net.input_packet(17, actions, voice=[net.sp.VoiceFrame(v.index & 63, v.mode, v.data)
                                                               for v in voice], start=start & 63)
                self.assertEqual(security.sanitize_input(raw), expected)

    def test_short_packets_drop(self):
        raw = net.input_packet(1, [net.sp.Action(1, 0, 0, b'hello')])
        for size in range(5, len(raw)):
            self.assertIsNone(security.sanitize_input(raw[:size]), size)
        raw = net.join_packet((1, 2))
        for size in range(5, len(raw)):
            self.assertIsNone(security.sanitize_input(raw[:size]), size)
        self.assertEqual(security.sanitize_input(raw), raw)

    def test_valid_wrap_unchanged(self):
        raw = net.input_packet(1, [net.sp.Action(0, 3, 7)] * 15, start=60)
        self.assertEqual(security.sanitize_input(raw), raw)

    def test_reference_discards_payload_without_changing_transport(self):
        raw = net.input_packet(1)
        line = net.recv_line(raw, (1, 2, 3, 4), 5000)
        safe = security.reference_lines([line], discard=True)[0]
        self.assertTrue(safe.startswith('S recvfrom '))
        self.assertIn('o:' + (raw[:4] + b'\xff' + raw[5:]).hex(), safe)
        self.assertEqual(line.split()[-2:], safe.split()[-2:])

    def test_join_capacity_and_stale_sender(self):
        cc = self.case()
        args, checks = security.state_case(cc, 'join_same_account_256')
        raw = [s for s in args if s.startswith('E ')]
        self.assertTrue(checks)
        self.assertEqual(len(raw), 256)
        self.assertEqual(len({s.split()[-2] for s in raw}), 256)
        self.assertIn('%x 4 00010000' % net.NUMCONN, checks)
        self.assertIn('%x 4 0f280000' % (net.CONNS + 255 * net.CONN_SIZE + net.C_PORT), checks)
        args, _ = security.packet_case(self.case(), 'input_player_minus1')
        self.assertEqual(sum(s.startswith('E ') for s in args), 2)
        self.assertIn('o:37444650ff', args[-1])

    def test_state_preconditions(self):
        with patch.object(net, 'image_bytes', return_value=b'filter'):
            for name in security.STATE_CASES:
                cc = self.case()
                args, checks = security.state_case(cc, name)
                self.assertTrue(checks, name)
                for line in args:
                    if line.startswith('R '):
                        self.assertIn(line.split()[1], net.RVA_NAME, name)
                if name.startswith('menu6_'):
                    self.assertEqual(cc.rd(net.PLAYERS + 0xd4), 6)
                    self.assertEqual(cc.rd(net.PLAYERS + 0x64), 0)
                    self.assertEqual(cc.rd(net.PLAYERS + net.P_ACTIONS + 8), security.MENU6_VALUES[name])
                if name == 'bank_items_full':
                    self.assertTrue(all(cc.rd(net.ITEMS + i * net.ITEM_SIZE) == 1 for i in range(1024)))
                    self.assertEqual(cc.rd(net.HUMANS + 0x291c), 0)
                if name == 'wallet_items_full':
                    self.assertEqual(cc.rd(net.ITEMS + 0xc), 16)
                    self.assertEqual(cc.rd(net.HUMANS + 0x291c), 1)
                    self.assertEqual(cc.rd(net.HUMANS + 0x2920), 0)
                if name in {'phone_items_full', 'dealer_items_full', 'market_items_full'}:
                    self.assertTrue(all(cc.rd(net.ITEMS + i * net.ITEM_SIZE + 0xc) == 0 for i in range(1024)))
                if name.startswith('input_actions_zero_'):
                    self.assertEqual([s.split()[1] for s in args if s.startswith('R ')], ['33650', '55700'])
                if name.startswith('kick_'):
                    self.assertEqual(security.KICK_CAPACITY, 41368)

    def test_timeout_and_skip_cannot_pass(self):
        with tempfile.TemporaryDirectory(dir=net.ROOT / 'build') as tmp:
            d = Path(tmp)
            (d / 'sv_args.txt').write_text('R 33650\n')
            with patch.object(security.subprocess, 'run', side_effect=subprocess.TimeoutExpired('harness', 1)):
                self.assertFalse(security.run_case(d, ['check'], 1)[0])
            output = '1 functions tested: 0 calls same, 0 different, 0 known differences, 0 lifted crashed, 1 skipped'
            with patch.object(security.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, output, '')):
                self.assertFalse(security.run_case(d, [], 1)[0])

    def test_c_policy_matches_python(self):
        exe = net.ROOT / 'build/hook/test_netguard.exe'
        if not exe.exists():
            self.skipTest('build test_netguard first')
        for start in (0, 63, 64, 200, 255):
            raw = net.input_packet(1, [net.sp.Action(0, 6, -100000)], start=start,
                                   voice=[net.sp.VoiceFrame(200 + i, 2, b'\x1a\r\n') for i in range(6)])
            for data in (raw, raw[:20], raw[:-20]):
                safe = security.sanitize_input(data)
                expected = 'DROP' if safe is None else safe.hex()
                result = subprocess.run([str(exe), '--sanitize'], input=data, capture_output=True, check=True)
                self.assertEqual(result.stdout.decode().strip(), expected)


if __name__ == '__main__':
    unittest.main()
