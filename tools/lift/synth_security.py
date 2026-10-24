"""Security expectations for synth_net.py --invalid --run REC WORK [case,...].

Packet cases compare raw input in C with sanitized input in the reference image.
S lines hold the sanitized datagrams. A discarded datagram becomes an unknown type
with the same length, so transport logs still match. The receive buffer, cursor,
and ingress counter are excluded from the state comparison. The counter can
advance before or after a discard, depending on where the guard runs.

State cases run C only, in recorded order, with byte checks and a process timeout.
Failures are expected until the server checks these inputs. Valid cases stay in
the normal gate. input_actions_wrap also runs here as a valid boundary control.
"""
import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import synth_net as net

PACKET_CASES = ('input_actions_high', 'input_voice_big', 'input_player_minus1',
                'input_actions_wrap', 'join_takeover', 'join_same_account_256',
                'garbage_input_cut_voice', 'garbage_input_cut_actions', 'garbage_input_cut_head')
STATE_CASES = ('input_actions_zero_64', 'input_actions_zero_255', 'menu6_zero', 'menu6_minus1',
               'menu6_minus100000', 'bank_items_full', 'phone_nohuman', 'kick_last', 'kick_full')
MARKET = 0x17179600
KICK_CAPACITY = (net.NUMKICKED - net.KICKED) // 4


class StrictReader(net.sp.PacketReader):
    def read_bits(self, n):
        if self.pos * 8 + self.bit + n > self.len * 8:
            raise net.sp.PacketError('short bit field')
        return super().read_bits(n)

    def read_bytes(self, n):
        if self.pos + bool(self.bit) + n > self.len:
            raise net.sp.PacketError('short byte field')
        return super().read_bytes(n)


def sanitize_input(raw):
    """Apply the voice guard and the planned six-bit action index rule, in place."""
    if len(raw) < 5 or raw[:5] != b'7DFP\x04':
        return raw
    data = bytearray(raw)

    def index(r):
        pos = r.pos * 8 + r.bit
        value = r.read_bits(8) & 63
        for k in range(8):
            byte, bit = divmod(pos + k, 8)
            data[byte] = (data[byte] & ~(1 << bit)) | (((value >> k) & 1) << bit)

    try:
        r = StrictReader(raw)
        r.read_bytes(1 + 4 + 8 * 4 + 4)
        r.read_bits(8)
        r.read_bits(4)
        r.read_bits(16)
        r.read_bytes(12)
        count = r.read_bits(4)
        index(r)
        for _ in range(count):
            kind = r.read_bits(4)
            if kind == 0:
                r.read_bits(8)
                r.read_bytes(4)
            elif kind == 1:
                size = r.read_bits(6)
                for _ in range(size):
                    r.read_bytes(1)
                r.read_bits(4)
            elif kind == 2:
                r.read_bits(16)
                r.read_bits(16)
        for _ in range(6):
            index(r)
            size = r.read_bits(8)
            r.read_bits(2)
            r.read_bytes(size)
    except net.sp.PacketError:
        return None
    return bytes(data)


def reference_lines(args, discard=False):
    result = []
    for line in args:
        if not line.startswith('E recvfrom '):
            continue
        parts = line.split()
        at = next(i for i, token in enumerate(parts) if token.startswith('o:'))
        raw = bytes.fromhex(parts[at][2:])
        safe = None if discard else sanitize_input(raw)
        if safe is None:
            safe = raw[:4] + b'\xff' + raw[5:]
        parts[0] = 'S'
        parts[at] = 'o:' + safe.hex()
        result.append(' '.join(parts))
    return result


def packet_case(cc, name):
    if name in {'join_same_account_256', 'join_takeover', 'input_player_minus1'}:
        p = cc.ctx['p']
        account = cc.rd(net.NUMACC)
        cc.account(account, (0x3333, 0x4444))
        cc.player(p, net.P_ACCOUNT, account)
        cc.player(p, net.P_ACCOUNT_ID, 0x3333)
        cc.player(p, net.P_ACCOUNT_ID + 4, 0x4444)
        cc.wr(net.SERVER + 0x5c, 256)
        count = 256 if name == 'join_same_account_256' else 1
        args = ['R 33650']
        reference = []
        slots = cc.rd(net.NUMCONN)
        for i in range(count):
            line = net.recv_line(net.join_packet((0x3333, 0x4444)), (10, 1, 2, 3), 10000 + i)
            args.append(line)
            reference += reference_lines([line], discard=slots >= 256)
            slots += slots < 256
        if name != 'join_same_account_256':
            # The first join leaves the old connection at player_id -1 until the next tick.
            line = net.recv_line(net.input_packet(cc.rd(net.GAME + 0x1c)), *cc.addr())
            args.append(line)
            reference += reference_lines([line], discard=True)
        return args + reference, []
    else:
        args = net.recv_cases(cc)[name](cc)
    return args + reference_lines(args), []


def state_case(cc, name):
    p, h = cc.ctx['p'], cc.ctx['h']
    player = net.PLAYERS + p * net.PLAYER_SIZE
    checks = []

    def unchanged(rva, size):
        for start in range(0, size, 4096):
            n = min(4096, size - start)
            data = cc.s.read(rva + start, '%ds' % n)[0]
            checks.append('%x %x %s' % (rva + start, n, data.hex()))

    if name.startswith('input_actions_zero_'):
        start = int(name.rsplit('_', 1)[1])
        cc.player(p, net.P_NUMACT, 0)
        cc.player(p, 0xe8, start & 63)
        for k in range(64):
            cc.player(p, net.P_ACTIONS + k * 0x54, 3)
        raw = net.input_packet(cc.rd(net.GAME + 0x1c), start=start)
        args = ['R 33650', net.recv_line(raw, *cc.addr()), 'R 55700 V:%x' % p]
        # Both cursors must match the masked wire index after the empty action list.
        checks += ['%x 4 %s' % (player + off, struct.pack('<i', start & 63).hex()) for off in (0xe4, 0xe8)]
        unchanged(player + net.P_ACTIONS, 64 * 0x54)
        return args, checks
    if name.startswith('menu6_') or name == 'bank_items_full':
        menu = 12 if name == 'bank_items_full' else 6
        value = {'menu6_zero': 0, 'menu6_minus1': -1, 'menu6_minus100000': -100000,
                 'bank_items_full': 3}[name]
        cc.player(p, net.P_HUMAN, h)
        cc.player(p, 0x64, 0)  # team
        cc.player(p, 0xd4, menu)
        cc.player(p, net.P_MONEY, 10000)
        cc.player(p, 0x54, 0)
        cc.player(p, net.P_NUMACT, 1)
        cc.player(p, 0xe8, 0)
        cc.s.write(player + net.P_ACTIONS, 'iii', 0, menu, value)
        if name == 'bank_items_full':
            cc.s.write(net.HUMANS + h * net.HUMAN_SIZE + 0x291c, 'i', 0)
            for i in range(1024):
                cc.wr(net.ITEMS + i * net.ITEM_SIZE, 1)
            # The final 32 bytes before the pool include patched import pointers.
            unchanged(net.ITEMS - net.ITEM_SIZE, net.ITEM_SIZE - 0x20)
            unchanged(net.ITEMS, net.ITEM_SIZE * 1024)
            unchanged(net.HUMANS + h * net.HUMAN_SIZE + 0x291c, 8 * 0x24)
        else:
            # A readable row before the table must not produce a purchase.
            row = MARKET + 0x144 + (value - 1) * 12
            cc.s.write(row, 'iii', 0, 0, 1)
            cc.wr(MARKET + 0x140, 16)
            unchanged(row, 12)
            unchanged(MARKET, 0x204)
        unchanged(player + net.P_MONEY, 4)
        unchanged(player + 0xd4, 4)
        return ['R 55700 V:%x' % p], checks
    if name == 'phone_nohuman':
        args = net.message_cases(cc)[name](cc)
        unchanged(net.ITEMS, 1024 * net.ITEM_SIZE)
        unchanged(0xf437318, 4)  # event count
        return args, checks
    # A named connection makes /kick reach the list write at both boundaries.
    args = net.message_cases(cc)['kick_named'](cc)
    cc.s.write(player + net.P_NAME, '32s', b'Tester')
    count = KICK_CAPACITY - 1 if name == 'kick_last' else KICK_CAPACITY
    cc.wr(net.NUMKICKED, count)
    for i in range(count):
        cc.wr(net.KICKED + i * 4, 0x01020304, 'I')
    if name == 'kick_last':
        checks.append('%x 4 %s' % (net.NUMKICKED, struct.pack('<i', KICK_CAPACITY).hex()))
        checks.append('%x 4 %s' % (net.KICKED + count * 4,
                                  struct.pack('<I', cc.rd(net.CONNS + cc.ctx['conn'] * net.CONN_SIZE, 'I')).hex()))
    else:
        unchanged(net.NUMKICKED, 4)
    unchanged(net.NUMKICKED + 4, 16)
    return args, checks


def run_case(d, checks, timeout):
    env = dict(os.environ, HARNESS_ENV_DIR=str(d / 'env'), HARNESS_RETRY='0')
    for key in ('HARNESS_TIME', 'HARNESS_COVER', 'HARNESS_CLEAN_CHECKS', 'HARNESS_INVALID',
                'HARNESS_FORBID_READ', 'HARNESS_WATCH', 'HARNESS_PROBE'):
        env.pop(key, None)
    if checks:
        env['HARNESS_CLEAN_CHECKS'] = str(d / 'checks.txt')
        if d.name.startswith('menu6_'):
            value = {'menu6_zero': 0, 'menu6_minus1': -1, 'menu6_minus100000': -100000}[d.name]
            env['HARNESS_FORBID_READ'] = '%x' % (MARKET + 0x144 + (value - 1) * 12)
    else:
        env['HARNESS_INVALID'] = '1'
    cmd = [str(net.ROOT / 'build/hook/harness.exe'), str(net.ROOT / 'build/hook/024d_fixed.exe'),
           str(d / 'sv_snapshot.bin'), str(d / 'sv_args.txt')]
    # All named calls must run. A filter could hide a missing call.
    calls = [line for line in (d / 'sv_args.txt').read_text().splitlines() if line.startswith('R ')]
    funcs = sorted({net.RVA_NAME[line.split()[1]] for line in calls})
    cmd.append(','.join(funcs) + ',')
    try:
        result = subprocess.run(cmd, cwd=net.ROOT, env=env, capture_output=True, text=True,
                                errors='replace', timeout=timeout)
    except subprocess.TimeoutExpired:
        summary = 'TIMEOUT after %g s' % timeout
        (d / 'result.txt').write_text(summary + '\n')
        return False, summary
    output = result.stdout + result.stderr
    (d / 'result.txt').write_text(output, encoding='utf-8')
    lines = [line for line in output.splitlines() if 'functions tested' in line or line.startswith('security')]
    summary = lines[-1] if lines else 'NO RESULT'
    if checks:
        passed = result.returncode == 0 and summary == 'security: %d calls, %d checks, PASS' % (len(calls), len(checks))
    else:
        passed = result.returncode == 0 and bool(re.fullmatch(
            r'1 functions tested: 1 calls same, 0 different, 0 known differences, 0 lifted crashed, 0 skipped',
            summary))
    return passed, summary


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run', action='store_true')
    ap.add_argument('--no-cover', action='store_true', help='accepted for step.sh')
    ap.add_argument('--timeout', type=float, default=15)
    ap.add_argument('recording', type=Path)
    ap.add_argument('work', type=Path)
    ap.add_argument('cases', nargs='?')
    a = ap.parse_args(argv)
    if a.timeout <= 0:
        ap.error('--timeout must be positive')
    if not a.run and not a.cases:
        ap.error('use --run or select cases to limit snapshot storage')
    want = a.cases.split(',') if a.cases else list(PACKET_CASES + STATE_CASES)
    unknown = set(want) - set(PACKET_CASES + STATE_CASES)
    if unknown:
        ap.error('unknown cases: ' + ','.join(sorted(unknown)))
    base = net.Snapshot(a.recording / 'sv_snapshot.bin')
    ctx = net.context(base)
    failures = 0
    for name in want:
        cc = net.Case(base, ctx)
        args, checks = packet_case(cc, name) if name in PACKET_CASES else state_case(cc, name)
        work = a.work.resolve()
        d = (work / name).resolve()
        if not d.is_relative_to(work):
            ap.error('case directory is outside the work directory')
        d.mkdir(parents=True, exist_ok=True)
        cc.s.save(d / 'sv_snapshot.bin')
        (d / 'sv_args.txt').write_text('\n'.join(args) + '\n')
        (d / 'checks.txt').write_text('\n'.join(checks) + '\n')
        if a.run:
            passed, summary = run_case(d, checks, a.timeout)
            failures += not passed
            print('%s: %s %s' % (name, 'PASS' if passed else 'FAIL', summary), flush=True)
            # Keep the small failure log. Remove each large snapshot before the next case.
            (d / 'sv_snapshot.bin').unlink()
            env_dir = (d / 'env').resolve()
            if not env_dir.is_relative_to(d):
                ap.error('test environment is outside the case directory')
            shutil.rmtree(env_dir, ignore_errors=True)
        else:
            print('case', name, 'checks' if checks else 'sanitized reference')
    if a.run:
        print('security: %d cases, %d failed' % (len(want), failures))
    return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
