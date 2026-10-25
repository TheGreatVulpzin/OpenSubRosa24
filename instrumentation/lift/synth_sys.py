"""Test the paths of the system functions that the recordings may not reach.

Reads a recording snapshot, puts the game into one situation, and writes a new snapshot and
an args file with calls of one function, in one folder per case. With --run the script runs
the harness on each case and deletes the case at once (a snapshot is about 120 MB):

    python -I instrumentation/lift/synth_sys.py --run <recording dir> <work dir> [case,...]

The functions: the dead code (systemtimer, testtimer, gettimeofday, setcpuaffinity,
init_by_array), the parser (checkstring, findstring, getint, getstring, cs_getline,
loadtextfile), loadsaveoptions, the file lists, csradixsort, sprintf_comma, cs_pread,
debug_resetlogfile and server_writelogfile. Use the recording 20261013-1016-smoke.
The result of the harness must be "same" for every case. The calls use a scratch area inside
the parser buffer of the snapshot, so the cases that read the parser set their own text.
"""
import copy
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_bonds import Snapshot

ROOT = Path(__file__).resolve().parents[2]

# RVAs of the globals.
PARSER = 0x13d22d60
P_POS, P_SIZE, P_TEXT = 0x0, 0x4, 0x8
FILELIST, SLOT = 0xe219200, 0x10004
RADIX, R_KEYS = 0xff47c80, 0x200004
BSDSOCKET = 0x1952d680
B_RECV_SIZE, B_RECV_POS, B_RECV_BIT, B_RECV_BUF = 0x10064, 0x10068, 0x1006c, 0x10070
DEBUGIT = 0x204020
NUMACC, ACCOUNTS, ACCOUNT_SIZE, A_PLAYTIME = 0xf437b64, 0xf437b68, 0x4c, 0x30
STATS = 0x1b49788

# The scratch area: unused bytes in the middle of the parser text. A pointer argument
# refers to it with M:<rva>.
SCRATCH = PARSER + P_TEXT + 0x80000
OUT = SCRATCH + 0x1000


def put(s, rva, data):
    for k, b in enumerate(data):
        s.write(rva + k, 'B', b)


def text(s, data, pos=0):
    """Put a text in the parser buffer with a 0 after it."""
    put(s, PARSER + P_TEXT, data + b'\0')
    s.write(PARSER + P_POS, 'i', pos)
    s.write(PARSER + P_SIZE, 'i', len(data))


def key(s, data):
    put(s, SCRATCH, data + b'\0')
    return 'M:%x' % SCRATCH


# The average of the stats file has no value with no account: the game prints a leftover
# register, the new code prints 0.
EXPECTED_DIFFERENT = {'server_writelogfile_none'}


def cases():
    """Each case: the name, the function, a function that edits the snapshot and returns the
    args lines."""
    out = {}

    def case(name, func, build):
        out[name] = (func, build)

    # Dead code in the game: no recording has a call.
    case('systemtimer', 'systemtimer', lambda s: ['R 5350'])
    case('testtimer', 'testtimer', lambda s: ['R 18570'])
    case('gettimeofday_tv', 'gettimeofday', lambda s: ['R b230 M:%x V:0' % OUT])
    case('gettimeofday_tz', 'gettimeofday', lambda s: ['R b230 V:0 M:%x' % OUT])
    case('gettimeofday_both', 'gettimeofday', lambda s: ['R b230 M:%x M:%x' % (OUT, OUT + 0x100)])
    case('setcpuaffinity', 'setcpuaffinity', lambda s: ['R b300'])

    def init_array(n):
        def build(s):
            for i in range(n):
                s.write(SCRATCH + i * 4, 'I', (i * 2654435761 + 12345) & 0xffffffff)
            return ['R ad70 M:%x V:%x' % (SCRATCH, n)]
        return build
    case('init_by_array_4', 'init_by_array', init_array(4))
    case('init_by_array_624', 'init_by_array', init_array(624))
    case('init_by_array_700', 'init_by_array', init_array(700))

    # cs_pread: a read inside the packet, a read that needs the move to the next byte, a read
    # past the end, and a position past the limit.
    def pread(size, pos, bit, count=(2, 3)):
        def build(s):
            s.write(BSDSOCKET + B_RECV_SIZE, 'i', size)
            s.write(BSDSOCKET + B_RECV_POS, 'i', pos)
            s.write(BSDSOCKET + B_RECV_BIT, 'i', bit)
            put(s, BSDSOCKET + B_RECV_BUF, bytes(range(1, 64)))
            return ['R d9f0 M:%x V:%x V:%x' % (OUT, count[0], count[1])]
        return build
    case('cs_pread_ok', 'cs_pread', pread(40, 4, 0))
    case('cs_pread_align', 'cs_pread', pread(40, 4, 3))
    case('cs_pread_short', 'cs_pread', pread(8, 4, 0))
    case('cs_pread_limit', 'cs_pread', pread(40, 0x10000, 0))

    # Logs: the debug flag on, and no account.
    def debug_on(s):
        s.write(DEBUGIT, 'i', 1)
        return ['R 5280']
    case('debug_resetlogfile_on', 'debug_resetlogfile', debug_on)

    def no_accounts(s):
        s.write(NUMACC, 'i', 0)
        return ['R 1420']
    case('server_writelogfile_none', 'server_writelogfile', no_accounts)

    def some_accounts(s):
        s.write(NUMACC, 'i', 3)
        for i, t in enumerate((0, 25, 4000000000 & 0xffffffff)):
            a = ACCOUNTS + i * ACCOUNT_SIZE
            put(s, a + 8, b'tester%d\0' % i)
            s.write(a + A_PLAYTIME, 'I', t)
        return ['R 1420']
    case('server_writelogfile_three', 'server_writelogfile', some_accounts)

    # The parser.
    def number(data, pos=0):
        def build(s):
            text(s, data, pos)
            return ['R d110']
        return build
    case('getint_number', 'getint', number(b'port=27200\r\nname=x\r\n', 5))
    case('getint_negative', 'getint', number(b'x=-45 y', 0))
    case('getint_long', 'getint', number(b'1' * 300, 0))
    case('getint_overflow', 'getint', number(b'99999999999', 0))
    case('getint_end', 'getint', number(b'abc', 3))

    def line(func, rva, data, pos, mx):
        def build(s):
            text(s, data, pos)
            put(s, OUT, b'\xaa' * 80)
            return ['R %s M:%x V:%x' % (rva, OUT, mx)]
        return build
    case('getstring_line', 'getstring', line('getstring', 'd240', b'name=bob\r\nport=1\r\n', 5, 0x1e))
    case('getstring_short', 'getstring', line('getstring', 'd240', b'name=bobsmith\r\n', 5, 4))
    case('getstring_zero', 'getstring', line('getstring', 'd240', b'name=bob\r\n', 5, 0))
    case('getstring_end', 'getstring', line('getstring', 'd240', b'abc', 3, 10))
    case('cs_getline_line', 'cs_getline', line('cs_getline', 'd2a0', b'one\r\n\r\ntwo\r\n', 0, 40))
    case('cs_getline_end', 'cs_getline', line('cs_getline', 'd2a0', b'abc', 3, 10))
    case('cs_getline_last', 'cs_getline', line('cs_getline', 'd2a0', b'abc\r\n', 0, 40))

    def find(rva, data, pos, name):
        def build(s):
            text(s, data, pos)
            return ['R %s %s' % (rva, key(s, name))]
        return build
    case('checkstring_end', 'checkstring', find('d0c0', b'xxport=', 2, b'port=9'))
    case('checkstring_match', 'checkstring', find('d0c0', b'xxport=1', 2, b'port='))
    case('findstring_end', 'findstring', find('29650', b'name=x\r\nport=', 0, b'port='))
    case('findstring_missing', 'findstring', find('29650', b'name=x\r\n', 0, b'port='))
    case('findstring_empty', 'findstring', find('29650', b'', 0, b'port='))

    def load_missing(s):
        text(s, b'old text', 3)
        return ['R d020 %s' % key(s, b'no-such-file.txt')]
    case('loadtextfile_missing', 'loadtextfile', load_missing)

    def options_save(s):
        return ['R 4e7a0 V:1']
    case('loadsaveoptions_save', 'loadsaveoptions', options_save)

    # The file lists and the sort.
    def noext(names):
        def build(s):
            base = FILELIST + 7 * SLOT
            s.write(base, 'i', len(names))
            for i, n in enumerate(names):
                put(s, base + 4 + i * 64, n)
            return ['R d680 V:7']
        return build
    case('removeflieextentions_plain', 'removeflieextentions', noext([b'abc\0', b'x.png\0', b'.hidden\0']))
    case('removeflieextentions_long', 'removeflieextentions', noext([b'a' * 64]))

    def list_dirs(dirs):
        def build(s):
            return ['R d3a0 %s V:6 V:%d' % (key(s, b'*'), dirs)]
        return build
    case('listfiles_files', 'listfiles', list_dirs(0))
    case('listfiles_dirs', 'listfiles', list_dirs(1))

    def sort_keys(vals):
        def build(s):
            s.write(RADIX, 'i', len(vals))
            for i, v in enumerate(vals):
                s.write(RADIX + R_KEYS + i * 4, 'I', v)
            return ['R ac00']
        return build
    case('csradixsort_none', 'csradixsort', sort_keys([]))
    case('csradixsort_one', 'csradixsort', sort_keys([7]))
    case('csradixsort_mixed', 'csradixsort',
         sort_keys([5, 0xffffffff, 0, 0x80000000, 0x7fffffff, 256, 255, 5, 0x01000000]))
    case('csradixsort_many', 'csradixsort', sort_keys([(i * 2654435761) & 0xffffffff for i in range(3000)]))

    def comma(value):
        def build(s):
            put(s, OUT, b'\xaa' * 40)
            return ['R 8880 M:%x V:%x' % (OUT, value & 0xffffffff)]
        return build
    # The harness skips a value that looks like an address of its own process (about 65536 to
    # 4194303), so the cases for 1000 to 999999 are the recorded calls.
    for v in (0, 999, 1000, 12345678, 100000000, 123456789, 999999999, 1000000000, 2147483647, -1, -1234567):
        case('sprintf_comma_%d' % v, 'sprintf_comma', comma(v))
    return out


def run_harness(d, func):
    cmd = [str(ROOT / 'build/hook/harness.exe'), str(ROOT / 'build/hook/024d_fixed.exe'),
           str(d / 'sv_snapshot.bin'), str(d / 'sv_args.txt'), func]
    env = dict(os.environ, HARNESS_ENV_DIR=str(d / 'env'))
    out = subprocess.run(cmd, capture_output=True, text=True, errors='replace', env=env, cwd=ROOT).stdout
    res = [ln for ln in out.splitlines() if 'functions tested' in ln]
    return res[-1] if res else 'NO RESULT: ' + out[-300:]


def main():
    args = [a for a in sys.argv[1:] if a != '--run']
    run = '--run' in sys.argv
    rec, out = Path(args[0]), Path(args[1])
    want = args[2].split(',') if len(args) > 2 else None
    base = Snapshot(rec / 'sv_snapshot.bin')
    bad = 0
    for name, (func, build) in cases().items():
        if want and name not in want:
            continue
        s = copy.deepcopy(base)
        lines = build(s)
        d = out / name
        d.mkdir(parents=True, exist_ok=True)
        s.save(d / 'sv_snapshot.bin')
        (d / 'sv_args.txt').write_text('\n'.join(lines) + '\n')
        if run:
            result = run_harness(d, func)
            ok = ' 0 different' in result and ' 0 lifted crashed' in result and ' 0 skipped' in result
            if name in EXPECTED_DIFFERENT:
                ok = ' 1 different' in result
            bad += not ok
            print('%-30s %-22s %s%s' % (name, func, result, '' if ok else '   <-- CHECK'), flush=True)
            shutil.rmtree(d, ignore_errors=True)
        else:
            print('case', name, lines[0])
    if run:
        print('%d case(s) to check' % bad)


if __name__ == '__main__':
    main()
