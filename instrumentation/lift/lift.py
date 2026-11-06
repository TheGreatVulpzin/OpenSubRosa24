"""Lift Ghidra pseudo-C of the 024d to C that compiles with MSVC, and generate hooks.

Input:  build/lift/ded024d.jsonl (instrumentation/ghidra/LiftExport.java)
Output: src/lifted/lifted_024d.c   lifted functions (lf_<rva>)
        instrumentation/hook/hooks_lifted.c        hook table + verification wrappers
        build/lift/report.csv      status of every function

A function is NOT lifted when:
  - it already has a hand-written hook (instrumentation/hook/hooks_024d.c);
  - the pseudo-C has decompiler artifacts (register inputs, SEH, x87 intrinsics without
    arguments, computed jumps...);
  - the prologue has a relative jump (no safe trampoline);
  - it reaches a side effect outside memory (network, files, time, heap), directly or
    through its callees. Verification runs a function twice, so these cannot be verified;
  - it does not compile (found with --compile; the loop removes it and tries again).

Usage:
  python -I instrumentation/lift/lift.py [--compile] [--only 0x6660,0xa580] [--max N]
"""
import argparse
import csv
import json
import os
import re
import shutil
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x400000
VCVARS = os.environ.get('VCVARS', r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat')


def vcvars_line():
    """The .bat line that sets up MSVC x86, or nothing when cl is already in PATH (CI)."""
    return '' if shutil.which('cl') else f'@call "{VCVARS}" >nul 2>nul' + chr(13) + chr(10)

# Decompiler artifacts that the lifter does not handle (yet).
BLOCKERS = {
    'in_': 'register input', 'unaff_': 'register input', 'extraout_': 'register output',
    'halt_baddata': 'bad data', 'ExceptionList': 'SEH frame', 'stack0x': 'stack address',
    'switchD': 'computed jump', '(code *)': 'function pointer call', 'in_ST0': 'x87 input',
    'POPCOUNT': 'popcount', 'LOCK()': 'lock prefix',
    'segment(': 'segment', 'CONCAT3': 'odd concat',
}

# Imports with no effect outside memory (safe to run twice in a verification).
PURE_IMPORTS = {
    'memset', 'memcpy', 'memmove', 'memcmp', 'strncmp', 'strcmp', 'strlen', 'strcpy', 'strncpy',
    'strcat', 'strchr', 'strstr', 'sprintf', 'sscanf', 'rand', 'srand', 'floor', 'ceil', 'fabs',
    'abs', 'atoi', 'atof', 'toupper', 'tolower', 'qsort', '_stricmp', '_strnicmp',
    'htons', 'htonl', 'ntohs', 'ntohl',
    '_CIsqrt', '_CIsin', '_CIcos', '_CItan', '_CIacos', '_CIatan2', '_CIpow',
}

# x87 intrinsics: arguments on the x87 stack. Call the thunks in src/platform/x87_math.c.
X87_INTRINSICS = {'_CIsqrt', '_CIsin', '_CIcos', '_CItan', '_CIacos', '_CIatan2', '_CIpow'}

# Byte swaps: inline (WinSock imports of the 024d are by ordinal).
INLINE_IMPORTS = {
    'htons': '#define htons(x) ((ushort)((((ushort)(x)) >> 8) | (((ushort)(x)) << 8)))',
    'ntohs': '#define ntohs(x) ((ushort)((((ushort)(x)) >> 8) | (((ushort)(x)) << 8)))',
    'htonl': '#define htonl(x) ((uint)(((uint)(x) >> 24) | (((uint)(x) >> 8) & 0xff00u) | (((uint)(x) << 8) & 0xff0000u) | ((uint)(x) << 24)))',
    'ntohl': '#define ntohl(x) ((uint)(((uint)(x) >> 24) | (((uint)(x) >> 8) & 0xff00u) | (((uint)(x) << 8) & 0xff0000u) | ((uint)(x) << 24)))',
}

# The 024d imports WinSock by ordinal: the lifted code finds the import by its ordinal.
WSOCK_ORDINALS = {
    'bind': 2, 'closesocket': 3, 'connect': 4, 'getsockopt': 7, 'inet_addr': 10, 'ioctlsocket': 12,
    'recv': 16, 'recvfrom': 17, 'select': 18, 'send': 19, 'sendto': 20, 'socket': 23,
    'gethostbyname': 52, 'getservbyname': 55, 'gethostname': 57, 'WSAStartup': 115, '__WSAFDIsSet': 151,
}

# Game functions that lifted code does not call. __security_check_cookie (MSVC /GS) only
# stops the process when the stack is corrupt. The lifted copy keeps its own stack frame,
# so the check has no meaning there. Removing it also removes a false side effect
# (the failure path calls GetCurrentProcess).
SKIPPED_CALLS = {0x5ce18: '__security_check_cookie'}

# Game functions that lifted code replaces with plain C. _ftol2 (MSVC CRT) converts the
# x87 value in ST0 to a 64-bit int with truncation, the same as a C cast.
INLINE_CALLS = {0x5ce90: '#define {name}(x) ((ulonglong)(longlong)(x))'}

SIZE_TYPE = {1: 'uchar', 2: 'ushort', 4: 'uint', 8: 'ulonglong'}

HEADER_RE = re.compile(r'^(?P<ret>.+?)\s*\b(?:(?P<cc>__cdecl(?:_keep[CDF]+)?|__stdcall|__fastcall|__thiscall)\s+)?'
                       r'(?P<name>[A-Za-z_]\w*)\s*\((?P<params>.*)\)\s*$', re.S)


def strip_comments(s):
    return re.sub(r'/\*.*?\*/', '', s, flags=re.S)


def parse_header(c):
    """'void __fastcall FUN_x(int a, char *b)' -> (ret, cc, name, [(type, name)])."""
    head = strip_comments(c.split('{', 1)[0]).strip()
    m = HEADER_RE.match(' '.join(head.split()))
    if not m:
        return None
    params = []
    p = m.group('params').strip()
    if p and p != 'void':
        for part in p.split(','):
            part = part.strip()
            if part == '...':
                params.append(('...', ''))
                continue
            pm = re.match(r'(.+?)\s*\b([A-Za-z_]\w*)$', part)
            if not pm:
                return None
            params.append((pm.group(1).strip(), pm.group(2)))
    # __cdecl_keepC/D/F (instrumentation/ghidra/FixRegisterPreservation.java) only tell the decompiler
    # which registers a callee keeps. For C they are __cdecl.
    cc = re.sub(r'^__cdecl_keep[CDF]+$', '__cdecl', m.group('cc') or '__cdecl')
    return m.group('ret').strip(), cc, m.group('name'), params


def proto_cast(ret, cc, params):
    """Function pointer type for a cast: RET (CC *)(T1, T2)."""
    if cc == '__thiscall':
        cc = '__fastcall'  # thiscall = fastcall with an unused EDX in C
        # Only a callee with stack arguments needs the EDX slot: it keeps them off ECX and EDX.
        if len(params) > 1:
            params = [params[0], ('int', 'edx_unused')] + list(params[1:])
    plist = ', '.join(t if t == '...' else t for t, _ in params) or 'void'
    return f'{ret} ({cc} *)({plist})'


# The .text of the image (addresses of the target, see TARGETS['text']). An address there is
# code: SR_FN, not SR_VA.


def global_macro(g):
    name, va, typ = g['name'], int(g['va'], 16), g['type'].strip()
    if typ in ('undefined', ''):
        typ = 'undefined'
    m = re.match(r'(.+?)\s*\[(\d+)\]$', typ)
    # A label in the code (the qsort comparator LAB_0040d320): its address is a function.
    sr = 'SR_FN' if TARGET['text'][0] <= va < TARGET['text'][1] else 'SR_VA'
    # A function used as a value (a callback stored in a table, the qsort comparator): the C value is
    # the address of the function, not the first byte of its code.
    if sr == 'SR_FN' and typ == 'undefined' and name.startswith(('FUN_', 'LAB_')) and TARGET.get('lift_loops'):
        return f'#define {name} ((void *){sr}(0x{va:x}))'
    if typ in ('string', 'unicode'):
        # A string literal that Ghidra names as a global: the C value is the address of the text.
        return f'#define {name} ((char *){sr}(0x{va:x}))'
    if m:
        return f'#define {name} (*({m.group(1)} (*)[{m.group(2)}]){sr}(0x{va:x}))'
    return f'#define {name} (*({typ} *){sr}(0x{va:x}))'


def partial_access(c):
    """Ghidra 'x._4_2_' (2 bytes at offset 4 of x) -> C pointer access. x can be a member
    path ('local_148.sa_data._2_4_')."""
    def rep(m):
        t = SIZE_TYPE.get(int(m.group(3)), 'uchar')
        return f'(*({t} *)((uchar *)&{m.group(1)} + {m.group(2)}))'
    return re.sub(r'(?<![\w.>])([A-Za-z_]\w*(?:(?:\.|->)(?!_\d+_\d+_)[A-Za-z_]\w*)*)\._(\d+)_(\d+)_', rep, c)


def load_names():
    names = {}
    with open(ROOT / TARGET['names'], encoding='utf-8') as fh:
        for r in csv.DictReader(fh):
            if r['name']:
                names[r['rva']] = r['name']
    return names


# The C symbol of each function in the generated table and hooks: lf_<rva>, or the real name
# of a function that is rewritten by hand in REWRITTEN_DIRS (rewritten_rvas).
SYMBOLS = {}
UNHOOKABLE = set()

# Function types of the OpenGL slots (analysis/client_win/gl_slots.csv).
def gl_slot_types():
    path = ROOT / 'analysis/client_win/gl_slots.csv'
    if not path.exists():
        return set()
    return {line.split(',', 2)[1] for line in path.read_text(encoding='utf-8').splitlines()[1:] if line.strip()}


GL_SLOT_TYPES = gl_slot_types()

# Imports that a function stores without a call (checkbigendian): the library of each one.
EXREF_IMPORTS = {'fread': 'MSVCR100.DLL', 'fwrite': 'MSVCR100.DLL'}


def is_loop(rva):
    """A frame loop (main, the game loop, a menu, an editor) that the standalone build lifts."""
    return bool(TARGET.get('lift_loops')) and rva in TARGET['record_skip']


REWRITTEN_DIRS = ('src/common', 'src/net', 'src/game')

# Inputs and outputs of each binary. The client writes under build/lift/client_win, so a
# client run never changes a file of the server.
TARGETS = {
    'ded024d': dict(
        export='build/lift/ded024d.jsonl', names='analysis/024d/functions.csv',
        lifted='src/lifted/lifted_024d.c', hooks='instrumentation/hook/hooks_lifted.c',
        record='instrumentation/hook/hooks_record.c', table='src/lifted/lifted_table.c',
        report='build/lift/report.csv', hand='instrumentation/hook/hooks_024d.c',
        rewritten=REWRITTEN_DIRS, extra_includes=(), text=(0x401000, 0x45e000),
        skipped={0x5ce18: '__security_check_cookie'}, pure=frozenset(),
        inline={0x5ce90: '#define {name}(x) ((ulonglong)(longlong)(x))'},
        crt_start=0x5ce18, record_skip=frozenset({0x5c8d0, 0x5cd10}), skip_callers=False),
    'client_win': dict(
        export='build/lift/client_win.jsonl', names='analysis/client_win/functions.csv',
        lifted='build/lift/client_win/lifted_client.c', hooks='build/lift/client_win/hooks_lifted.c',
        record='build/lift/client_win/hooks_record.c', table='build/lift/client_win/lifted_table.c',
        report='build/lift/client_win/report.csv', hand=None, rewritten=(),
        extra_includes=('lift_os_client.h', 'lift_gl_client.h'), text=(0x401000, 0x511000),
        skipped={0x10c6fb: '__security_check_cookie'},
        # __alloca_probe only touches the stack pages for a local array: the C compiler does that itself.
        inline={0x10c650: '#define {name}(x) ((ulonglong)(longlong)(x))', 0x10c710: '#define {name}() ((void)0)'},
        # The client calls the CRT math functions of MSVCR100 as plain imports.
        pure=frozenset({'sqrt', 'sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'atan2', 'pow', 'exp', 'log',
                        'log10', 'fmod'}),
        crt_start=0x10c6fb, skip_callers=True,
        # C++ glue of the Steam callbacks (Set, CCallResult, CCallbackBase and the unregister pair).
        # The standalone client has no Steam: a call to one of them does nothing and returns 0.
        ignored=frozenset({0x10bfa0, 0x10c040, 0x10c090, 0x10c190, 0x10c1b0}),
        # The standalone client needs the loops in C. They get no hook and no harness call.
        lift_loops=True,
        # checkmenuitems and FUN_00401300 call the callback of a menu item (cdecl, integer arguments).
        code_call_ok=frozenset({0x101be0, 0x1300}),
        # Functions that call OpenGL through the pointer slots. The harness has no OpenGL driver:
        # the table flag 2 makes it skip them. The standalone client calls them.
        standalone_only=frozenset({0x7d060, 0x742f0, 0xd5690}),
        # Functions that run a frame loop until the player leaves (main, the game loop, the menus,
        # the editors): the offline harness cannot run them, and neither can their callers.
        record_skip=frozenset({0x58d20, 0x5d150, 0x5dbd0, 0x5e020, 0x7b100, 0x9c5c0, 0xa8e10, 0xa9310, 0xaa5a0,
                               0xaa920, 0xabc00, 0xc14a0, 0xd2290, 0xd38c0, 0xfa9f0, 0x10ae00})),
}
TARGET = TARGETS['ded024d']


def symbol(rva):
    return SYMBOLS.get(rva, f'lf_{rva:05x}')


def rewritten_rvas(names):
    """Functions rewritten by hand in REWRITTEN_DIRS: no lifted body, but the same table entry, live
    hook and standalone call. A definition with the real name of the function
    (analysis/024d/functions.csv) or with lf_<rva>. Fills SYMBOLS."""
    by_name = {n: int(r, 16) for r, n in names.items()}
    definition = re.compile(r'^\w[^;{}#\n]*?\b(\w+)\([^;{}]*\)\s*\{', re.M)
    out = set()
    for f in sorted(f for d in TARGET['rewritten'] for f in (ROOT / d).glob('*.c')):
        for name in definition.findall(f.read_text(encoding='utf-8')):
            m = re.fullmatch(r'lf_([0-9a-f]{5})', name)
            rva = int(m.group(1), 16) if m else by_name.get(name)
            if rva is not None:
                out.add(rva)
                SYMBOLS[rva] = name
    return out


def handwritten_rvas():
    if TARGET['hand'] is None:
        return set()
    src = (ROOT / TARGET['hand']).read_text(encoding='utf-8')
    return {int(x, 16) for x in re.findall(r'\{\s*"\w+",\s*(0x[0-9a-fA-F]+),', src)}


def side_effects(funcs):
    """Function rva -> reason, for functions that reach an impure import."""
    direct = {}
    graph = defaultdict(set)
    for rva, f in funcs.items():
        for c in f['calls']:
            if c['external']:
                if c['target'] not in PURE_IMPORTS | TARGET['pure'] and rva not in direct:
                    direct[rva] = c['target']
            elif 'rva' in c and int(c['rva'], 16) not in SKIPPED_CALLS.keys() | INLINE_CALLS.keys():
                graph[rva].add(int(c['rva'], 16))
    side = dict(direct)
    changed = True
    while changed:
        changed = False
        for rva, callees in graph.items():
            if rva in side:
                continue
            for callee in callees:
                if callee in side:
                    side[rva] = f'calls 0x{callee:x} ({side[callee]})'
                    changed = True
                    break
    return side


DENORMAL_RE = re.compile(r'(?<![0-9.\w])(\d\.\d+e-(?:3[89]|4[0-5]))(?![0-9.\w])')
FLOAT_TINY = 2.0 ** -149  # smallest float denormal = int 1 read as float


def exact_denormals(c):
    """Ghidra typed an int as float: 'x == 1.4013e-45' means 'the int bits of x == 1'.

    Ghidra prints 6 significant digits. In C the literal is a double, so the compare runs in
    double and fails (1.4013e-45 is not 2^-149). Each literal becomes the exact float value
    (9 digits, 'f' suffix), so the compare and the store use the same bits as the original.
    A literal that does not map to exactly one int stays blocked."""
    def rep(m):
        text = m.group(1)
        n = round(float(text) / FLOAT_TINY)
        same = [k for k in (n - 1, n, n + 1) if k > 0 and float('%.6g' % (k * FLOAT_TINY)) == float(text)]
        if same != [n]:
            raise ValueError(text)
        return '%.9ef' % (n * FLOAT_TINY)
    try:
        return DENORMAL_RE.sub(rep, c), None
    except ValueError as e:
        return None, f'int typed as float (ambiguous literal {e})'


def pointer_names(body, params, globals_):
    """Names of the variables with a pointer type: parameters, locals and globals."""
    names = {n for t, n in params if t.endswith('*')}
    names |= set(re.findall(r'^\s*[A-Za-z_][\w ]*\*+\s*([A-Za-z_]\w*);', body, flags=re.M))
    names |= {g['name'] for g in globals_ if g['type'].strip().endswith('*')}
    return names


def pointer_float_casts(body, params, globals_):
    """'(float)p' with a pointer p is a bit copy in Ghidra (one stack slot holds a pointer
    at one time and a float at another time). In C it does not compile: use lf_p2f()."""
    ptrs = pointer_names(body, params, globals_)
    if not ptrs:
        return body

    def rep(m):
        return f'lf_p2f({m.group(1)})' if m.group(1) in ptrs else m.group(0)
    return re.sub(r'\(float\)\s*([A-Za-z_]\w*)(?![\w\[.(]|\s*->)', rep, body)


FLOATISH_RE = re.compile(r'\bfVar\d+|\bdVar\d+|\(float\)|\(double\)|\(float10\)|\d\.\d')


def split_args(s):
    """Top-level comma split of a call argument list (without the outer parentheses)."""
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def call_spans(s, name):
    """(start of the argument list, end after ')') of each call 'name(...)' in s."""
    for m in re.finditer(r'\b' + re.escape(name) + r'\s*\(', s):
        end = balanced_end(s, m.end() - 1)
        if end > 0:
            yield m.end(), end


def call_args(s, name):
    return [split_args(s[a:e - 1]) for a, e in call_spans(s, name)]


def local_types(body, params):
    """name -> base type of the parameters and local declarations."""
    types = {n: t for t, n in params}
    for m in re.finditer(r'^\s*([A-Za-z_][\w ]*?)\s*(\**)\s*([A-Za-z_]\w*)\s*(\[\d+\])?\s*;', body, flags=re.M):
        types.setdefault(m.group(3), m.group(1).strip() + (' ' + m.group(2) if m.group(2) else ''))
    return types


def expr_type(e, types):
    """Small type inference for one argument. Returns a type name, 'literal' for an int
    literal, '*'-ended for a pointer, or '' when unknown."""
    e = e.strip()
    if re.fullmatch(r'-?(0x[0-9a-fA-F]+|\d+)', e):
        return 'literal'
    if re.fullmatch(r'-?\d+\.\d*(e-?\d+)?f?', e):
        return 'float'
    if re.fullmatch(r'&\s*\(?[A-Za-z_]\w*\)?(\s*\[[^\]]*\])?', e):
        return 'void *'
    m = re.fullmatch(r'\(([A-Za-z_][\w ]*\**)\)\s*([A-Za-z_]\w*|\(.*\))', e, flags=re.S)
    if m:
        return m.group(1).strip()
    m = re.fullmatch(r'([A-Za-z_]\w*)', e)
    if m:
        if re.fullmatch(r'fVar\d+', e):
            return 'float'
        return types.get(e, '')
    m = re.fullmatch(r'\*\s*([A-Za-z_]\w*)', e)
    if m:
        t = types.get(m.group(1), '')
        return t[:-1].strip() if t.endswith('*') else ''
    m = re.fullmatch(r'\*\s*\(([A-Za-z_][\w ]*?)\s*\*\)\s*(\(.*\)|[A-Za-z_]\w*)', e, flags=re.S)
    if m:
        return m.group(1).strip()  # *(T *)(p): a T
    m = re.fullmatch(r'\(&([A-Za-z_]\w*)\)\s*\[[^\[\]]*\]', e)
    if m:
        return types.get(m.group(1), '')  # (&DAT_x)[i]: element of the type of DAT_x
    m = re.fullmatch(r'([A-Za-z_]\w*)\s*\[[^\[\]]*\]', e)
    if m:
        t = types.get(m.group(1), '')
        if t.endswith('*'):
            return t[:-1].strip()
        return t  # array declared "int local_54 [8]": the element type
    return ''


def undefined_names(body, params, globals_):
    """Variables whose Ghidra type is raw bytes: name -> size (undefined4: 4, undefined8: 8)."""
    size = {'undefined4': 4, 'undefined8': 8}
    names = {g['name']: size[g['type'].strip()] for g in globals_ if g['type'].strip() in size}
    names.update({n: size[t] for t, n in params if t in size})
    for m in re.finditer(r'^\s*(undefined[48])\s+([A-Za-z_]\w*);', body, flags=re.M):
        names[m.group(2)] = size[m.group(1)]
    return names


def lvalue_size(s, j, undef):
    """Size (4 or 8) of the raw-byte lvalue at s[j]."""
    m = re.match(r'\(?&?([A-Za-z_]\w*)', s[j:])
    if m and m.group(1) in undef:
        return undef[m.group(1)]
    return 4


def lvalue_end(s, j, undef):
    """End of an lvalue of raw-byte type at s[j], or -1: NAME, (&NAME)[...] or
    *(undefined4 *)(...)."""
    m = re.match(r'([A-Za-z_]\w*)(?![\w(\[.]|\s*->)', s[j:])
    if m and m.group(1) in undef:
        return j + m.end()
    m = re.match(r'\(&([A-Za-z_]\w*)\)\[', s[j:])
    if m and m.group(1) in undef:
        k = j + m.end() - 1
        depth = 0
        for t in range(k, len(s)):
            depth += s[t] == '['
            depth -= s[t] == ']'
            if depth == 0:
                return t + 1
        return -1
    m = re.match(r'\*\(undefined4 \*\)', s[j:])
    if m:
        return operand_end(s, j + m.end())
    return -1


def raw_float_uses(body, undef, types=None):
    """Ghidra: a cast from raw bytes (undefined4) to float is a bit copy. In C it converts
    the number. Rewrite (float)X -> (*(float *)&(X)) for X of raw-byte type (variable,
    (&DAT_x)[i] or *(undefined4 *)p). For 8 raw bytes (undefined8) the value is a double.
    A float stored into raw bytes is marked in LiftExport.java (floatStores), from the p-code."""
    out, i = [], 0
    for m in re.finditer(r'\(float\)\s*', body):
        if m.start() < i:
            continue
        end = lvalue_end(body, m.end(), undef)
        if end < 0 and types:
            # An element of a raw-byte pointer: (float)p[1] or (float)*p with p undefined4 *.
            em = re.match(r'\*?\s*[A-Za-z_]\w*(\s*\[[^\[\]]*\])?', body[m.end():])
            if em and (em.group(1) or em.group(0).startswith('*')) and \
                    expr_type(em.group(0), types) == 'undefined4':
                end = m.end() + em.end()
        if end < 0:
            continue
        out.append(body[i:m.start()])
        if lvalue_size(body, m.end(), undef) == 8:
            # 8 raw bytes read as float: Ghidra means the double in them, converted.
            out.append(f'((float)*(double *)&({body[m.end():end]}))')
        else:
            out.append(f'(*(float *)&({body[m.end():end]}))')
        i = end
    out.append(body[i:])
    return ''.join(out)


# A declaration, not a statement: "return local_4;" is not a variable of type "return".
STACK_VAR_RE = re.compile(r'^(\s*)(?!(?:return|goto|else|case|do)\b)([A-Za-z_][\w ]*?)\s*(\**)\s*((?:local|[a-z]+Stack)_([0-9a-f]+))\s*(\[\d+\])?\s*;[ \t]*$',
                          flags=re.M)


def stack_frame(body):
    """Put the stack variables at their original offsets in one frame.

    Ghidra names a stack variable after its offset (local_74 is 0x74 bytes under the return
    address). The original code can take the address of one variable and reach its
    neighbors (&local_74 passed as a vector, the callee reads local_70 too). C gives no
    order for separate locals, so they all go into lf_frame, at the original offsets.
    Returns (body, macro lines, undef lines)."""
    found = list(STACK_VAR_RE.finditer(body))
    if not found:
        return body, [], []
    size = max(int(m.group(5), 16) for m in found) + 16
    size = (size + 15) & ~15
    pre, post = [], []
    for m in found:
        typ = (m.group(2).strip() + ' ' + m.group(3)).strip()
        name, off, arr = m.group(4), int(m.group(5), 16), m.group(6)
        if arr:
            pre.append(f'#define {name} (*({typ} (*){arr})(lf_frame + 0x{size - off:x}))')
        else:
            pre.append(f'#define {name} (*({typ} *)(lf_frame + 0x{size - off:x}))')
        post.append(f'#undef {name}')
    body = STACK_VAR_RE.sub('', body)
    brace = body.index('{')
    body = body[:brace + 1] + f'\n  __declspec(align(16)) uchar lf_frame[0x{size:x}];' + body[brace + 1:]
    return body, pre, post


def balanced_end(s, i):
    """Index after the parenthesized group that starts at s[i] == '('."""
    depth = 0
    for j in range(i, len(s)):
        if s[j] == '(':
            depth += 1
        elif s[j] == ')':
            depth -= 1
            if depth == 0:
                return j + 1
    return -1


def fix_float_to_pointer(line, ptype):
    """Compiler error "cannot convert from 'float' to 'T *'": the last '(T *)' cast on the
    line is a bit copy of a float. Rewrite '(T *)X' as '((T *)lf_f2p(X))'."""
    ms = list(re.finditer(r'\(\s*' + re.escape(ptype.replace(' ', '')).replace(r'\*', r'\s*\*') + r'\s*\)\s*', line))
    if not ms:
        return None
    m = ms[-1]
    k = m.end()
    end = operand_end(line, k)
    if end < 0:
        return None
    return line[:m.start()] + f'(({ptype})lf_f2p({line[k:end]}))' + line[end:]


CAST_TYPES = {'float', 'double', 'float10', 'int', 'uint', 'char', 'uchar', 'short', 'ushort', 'byte', 'bool',
              'longlong', 'ulonglong', 'undefined', 'undefined1', 'undefined2', 'undefined4', 'undefined8',
              'dword', 'word', 'sbyte', 'unsigned int', 'long', 'ulong'}


def operand_end(s, k):
    """End index of the cast operand that starts at s[k]: skips more casts ('(float)x'),
    then takes a parenthesized group or a name with [..], ., -> after it."""
    j = k
    while True:
        while j < len(s) and s[j] in ' \n':
            j += 1
        if j < len(s) and s[j] == '(':
            end = balanced_end(s, j)
            if end < 0:
                return -1
            inner = s[j + 1:end - 1].strip()
            if inner in CAST_TYPES or re.fullmatch(r'[A-Za-z_][\w ]*\*+', inner):
                j = end
                continue
            return end
        em = re.match(r'[A-Za-z_]\w*', s[j:])
        if not em:
            return -1
        j += em.end()
        while j < len(s):
            if s[j] == '[':
                depth = 0
                for t in range(j, len(s)):
                    depth += s[t] == '['
                    depth -= s[t] == ']'
                    if depth == 0:
                        break
                j = t + 1
            elif s.startswith('->', j) or s[j] == '.':
                pm = re.match(r'(->|\.)\s*[A-Za-z_]\w*', s[j:])
                if not pm:
                    break
                j += pm.end()
            else:
                break
        return j


def fix_text_line(text, rel, ptype):
    """Apply fix_float_to_pointer to line rel of a function text (or to the statement that
    ends there: MSVC reports the last line of a statement on several lines)."""
    nl = chr(10)
    lines = text.split(nl)
    if not 0 <= rel < len(lines):
        return None
    for first in (rel, max(0, rel - 1), max(0, rel - 3), max(0, rel - 6)):
        window = nl.join(lines[first:rel + 1])
        new = fix_float_to_pointer(window, ptype)
        if new is not None:
            return nl.join(lines[:first] + new.split(nl) + lines[rel + 1:])
    return None


def register_artifacts(c):
    """Decompiler artifacts that have a plain C meaning:
    - in_FPUControlWord: the x87 control word (fnstcw), lf_fpucw() (packobjectpacket).
    - in_EAX (ECX, EDX, EBX) declared as a local and written before any read: Ghidra keeps the
      register name, but the value comes from the function itself (createevent_updateitem).
    - &stack0xffffffcc: a stack slot under the return address that has no local name, the
      start of a temporary array (multiply3x3matrices). It becomes &local_34, so stack_frame
      puts it at the same offset as the original."""
    if 'in_FPUControlWord' in c:
        c = re.sub(r'^\s*\w+\s+in_FPUControlWord;\s*\n', '', c, flags=re.M)
        c = re.sub(r'\bin_FPUControlWord\b', 'lf_fpucw()', c)
    for reg in ('EAX', 'ECX', 'EDX', 'EBX'):
        name = f'in_{reg}'
        decl = re.search(rf'^\s*\w+\s+{name};\s*$', c, flags=re.M)
        if not decl:
            continue
        first = re.search(rf'\b{name}\b(\s*=(?!=))?', c[decl.end():])
        if first and first.group(1):
            c = re.sub(rf'\b{name}\b', f'lf_{reg.lower()}', c)
    offs = sorted(set(int(x, 16) for x in re.findall(r'&stack0x(ffff[0-9a-f]{4})\b', c)))
    if offs and not re.search(r'(?<!&)stack0x', c):
        brace = c.index('{')
        decls = ''
        for v in offs:
            n = 0x100000000 - v
            if not re.search(rf'\blocal_{n:x}\b', c):
                decls += f'\n  undefined4 local_{n:x};'
            c = c.replace(f'&stack0x{v:08x}', f'((undefined *)&local_{n:x})')
        c = c[:brace + 1] + decls + c[brace + 1:]
    return c


# Fixes for single decompiler bugs: (old text, new text) per function. Each old text must
# exist, or the lift fails. The harness checks the result against the original.
LIFT_PATCHES = {
    # servercycle: Ghidra drops "mov byte ptr [esp+0Fh],20h", a store into local_1 after
    # its address goes to the call. The master server packet then has 0x00, not 0x20.
    0x33570: [('cs_pwrite(&local_1,1,1);', 'local_1 = 0x20;\n      cs_pwrite(&local_1,1,1);')],
    # Client, gettimeofday: __allshl takes edx:eax and cl in registers. The call shifts the high
    # word of the file time left by 32 bits.
    0xfae20: [('uVar3 = __allshl(local_c.dwHighDateTime | uVar4,uVar5);',
               'uVar3 = (ulonglong)(local_c.dwHighDateTime | uVar4) << 32;')],
    # Client, mainmenu: the debug line "STEAM %u %u" calls __aullshr with its inputs in registers.
    # The line only runs when the debug flag is set, so the value is zero here.
    0xaa920: [('local_7c = __aullshr();', 'local_7c = 0;')],
    # Client, drawtext and FUN_0040ac50: "&stack0x.." is the first argument of the variable list,
    # the word after the last named parameter.
    0x106b30: [('local_34 = (uint *)&stack0x00000028;', 'local_34 = (uint *)(&a + 1);')],
    # Client, createmenubutton(type, ...): "stack0x8" is the first argument of the list, the word after
    # "type". The decompiler also shows that first argument as an input of the function.
    0x102b70: [('undefined4 in_stack_00000008;', ''),
               ('= in_stack_00000008;', '= *(undefined4 *)(&type + 1);'),
               ('local_10 = (double *)&stack0x00000008;', 'local_10 = (double *)(&type + 1);'),
               ('local_10 = (double *)&stack0x0000000c;', 'local_10 = (double *)(&type + 2);')],
    # Client, tesselatebezierderivative: the result lives in ECX, which the decompiler shows as an input.
    # Every caller passes param_1 in 1..3, so each case sets it.
    0x3dac0: [('float unaff_ECX;', 'float bezier = 0;'),
              ('unaff_ECX = (float)(-param_3', 'bezier = (float)(-param_3'),
              ('unaff_ECX = (float)(param_4 * 2.0', 'bezier = (float)(param_4 * 2.0'),
              ('unaff_ECX = (float)(param_4 * 3.0', 'bezier = (float)(param_4 * 3.0'),
              ('return (float10)unaff_ECX;', 'return (float10)bezier;')],
    0xac50: [('writefv(_File,param_10,(int *)&stack0x0000002c);', 'writefv(_File,param_10,(int *)(&param_10 + 1));')],
}


def lift_function(f, rva, headers, imports_used):
    """Return (C text, error). error is None on success."""
    c = register_artifacts(f['c'])
    for old, new in LIFT_PATCHES.get(rva, []):
        if old not in c:
            return None, f'lift patch text not found: {old}'
        c = c.replace(old, new, 1)
    for tok, why in BLOCKERS.items():
        # A call through a table of game callbacks. The table holds the addresses that lifted code
        # stored, so the call reaches the C function (or the original, in the hook build).
        if tok == '(code *)' and rva in TARGET.get('code_call_ok', ()):
            continue
        # At the start of a word: "builtin_strncpy" is not a register input "in_".
        if re.search(r'(?<![A-Za-z0-9_])' + re.escape(tok), c):
            return None, why
    c, err = exact_denormals(c)
    if err:
        return None, err
    hdr = headers.get(rva)
    if not hdr:
        return None, 'header not parsed'
    ret, cc, name, params = hdr
    if f['prologue']['relative'] or f['prologue']['len'] < 5:
        # The standalone build needs no hook, so it lifts the function. The hook table skips it.
        if not TARGET.get('lift_loops'):
            return None, 'prologue not hookable'
        UNHOOKABLE.add(rva)
    if cc != '__cdecl' and not (cc == '__thiscall' and TARGET.get('lift_loops')):
        return None, f'calling convention {cc}'
    if any(t == '...' for t, _ in params):
        # The standalone build lifts a variable list when a patch gives the address of its first
        # argument. The hook table skips the function: a hook cannot forward a list.
        if not (TARGET.get('lift_loops') and rva in LIFT_PATCHES):
            return None, 'variadic'
        UNHOOKABLE.add(rva)

    # A LARGE_INTEGER read as one 64-bit value: "(longlong)local_8".
    for v in re.findall(r'\bLARGE_INTEGER\s+(\w+);', c):
        c = re.sub(r'\(longlong\)' + v + r'\b(?![.\[])', v + '.QuadPart', c)

    pre, post = [], []
    for g in f['globals']:
        # The slot of an import: the _exref macro below gives the address instead.
        if g['name'].endswith('_exref') and g['name'][:-6] in EXREF_IMPORTS and TARGET.get('lift_loops'):
            continue
        # The name of a string literal can hold characters that a macro name cannot.
        if not re.fullmatch(r'[A-Za-z_]\w*', g['name']):
            safe = re.sub(r'\W', '_', g['name'])
            c = c.replace(g['name'], safe)
            g = dict(g, name=safe)
        # A global that holds a function address: the fread and fwrite pointers of getpaths.
        # A slot with a function type from gl_slots.csv (lift_gl_client.h) keeps that type.
        if (g['type'].strip().endswith('*') and not g['type'].strip()[:-1].strip() in GL_SLOT_TYPES
                and re.search(r'\(\*' + re.escape(g['name']) + r'\)\(', c)):
            g = dict(g, type='code *')
        pre.append(global_macro(g))
        post.append(f'#undef {g["name"]}')
    # "pcVar9 = rand_exref;": the original loads the import address from the IAT once and
    # calls it through a register. The import macro gives the same address.
    external = {c['name'] for c in f['calls'] if c['external']}
    for x in sorted(set(re.findall(r'\b([A-Za-z_]\w*)_exref\b', c))):
        if x not in external:
            # An import that the function stores and never calls: the macro gets its address.
            if x not in EXREF_IMPORTS or not TARGET.get('lift_loops'):
                return None, f'import reference without a call: {x}_exref'
            idx = imports_used.setdefault((EXREF_IMPORTS[x], x), len(imports_used))
            pre.append(f'#define {x}_exref ((code *)LF_IMPORT({idx}, "{EXREF_IMPORTS[x]}", "{x}"))')
            post.append(f'#undef {x}_exref')
            continue
        pre.append(f'#define {x}_exref ((code *){x})')
        post.append(f'#undef {x}_exref')
    for call in f['calls']:
        cname = call['name']
        if call['external']:
            tgt = call['target']
            if tgt in INLINE_IMPORTS:
                pre.append(INLINE_IMPORTS[tgt])
                post.append(f'#undef {cname}')
                continue
            if tgt in X87_INTRINSICS:
                pre.append(f'#define {cname} x87_{tgt[3:]}')
                post.append(f'#undef {cname}')
                continue
            if tgt.startswith('_CI'):
                return None, f'x87 intrinsic {tgt} not supported'
            ph = parse_header(call['proto'] + '{')
            if not ph:
                return None, f'import prototype: {call["proto"]}'
            iret, _icc, _iname, iparams = ph
            icc = call['cc'] if call['cc'] in ('__cdecl', '__stdcall', '__fastcall') else '__cdecl'
            key = (call.get('library', ''), tgt)
            idx = imports_used.setdefault(key, len(imports_used))
            if key[0].upper() == 'WSOCK32.DLL' and tgt in WSOCK_ORDINALS:
                ref = f'LF_IMPORT_ORD({idx}, "{key[0]}", {WSOCK_ORDINALS[tgt]})'
            else:
                ref = f'LF_IMPORT({idx}, "{key[0]}", "{tgt}")'
            pre.append(f'#define {cname} (({proto_cast(iret, icc, iparams)}){ref})')
            post.append(f'#undef {cname}')
        else:
            callee = int(call['rva'], 16)
            if callee in INLINE_CALLS:
                pre.append(INLINE_CALLS[callee].format(name=cname))
                post.append(f'#undef {cname}')
                continue
            if callee in SKIPPED_CALLS:
                pre.append(f'#define {cname}(x) ((void)0)')
                post.append(f'#undef {cname}')
                continue
            ch = headers.get(callee)
            if not ch:
                return None, f'callee header unknown: {cname}'
            cret, ccc, _cn, cparams = ch
            # A call can push more arguments than the callee prototype shows (Ghidra kept
            # only the ones that the callee reads). Push them all, as the original does.
            most = max((len(a) for a in call_args(c, cname)), default=0)
            # A callee with a variable list ("...") takes the extra arguments in the list.
            if most > len(cparams) and ccc == '__cdecl' and not any(t == '...' for t, _ in cparams):
                extra = call_args(c, cname)
                extra = next(a for a in extra if len(a) == most)[len(cparams):]
                cparams = list(cparams) + [('float' if FLOATISH_RE.search(a) else 'undefined4', f'extra_{i}')
                                           for i, a in enumerate(extra)]
            if ccc == '__thiscall' and len(cparams) > 1:
                # The decompiled call has the arguments "this, a, b". The cast has an EDX slot after this.
                pre.append(f'#define {cname}(this_, ...) (({proto_cast(cret, ccc, cparams)})'
                           f'SR_FN(0x{callee + BASE:x}))((this_), 0, __VA_ARGS__)')
            else:
                pre.append(f'#define {cname} (({proto_cast(cret, ccc, cparams)})SR_FN(0x{callee + BASE:x}))')
            post.append(f'#undef {cname}')

    body = partial_access(strip_comments(c)).strip()
    # SSE values (bondsimulation2): C cannot assign an array, so use the lf_m128 struct.
    body = re.sub(r'\bundefined1\s+(auVar\d+)\s*\[16\];', r'lf_m128 \1;', body)
    body = body.replace('*(undefined1 (*) [16])', '*(lf_m128 *)')
    body = re.sub(r'\bdpps\(', 'lf_dpps(', body)
    body = re.sub(r'\b__cdecl_keep[CDF]+\b', '__cdecl', body)
    body = re.sub(r'\bNAN\b(?!\s*\()', 'LF_NAN', body)
    body = raw_float_uses(body, undefined_names(body, params, f['globals']),
                          dict({g['name']: g['type'].strip() for g in f['globals']}, **local_types(body, params)))
    body = pointer_float_casts(body, params, f['globals'])
    # Call arguments with the bits of another type: LiftExport.java (callArgBits).
    # Short strings stored as an immediate ("Uzi" = 0x00697a55): Ghidra shows them as an
    # address (&DAT_00697a55). The loader does not relocate them, so neither do we.
    relocs = set(f.get('relocvals', []))

    def immediate(m):
        v = int(m.group(2), 16)
        text = v.to_bytes(4, 'little')
        if v in relocs or not all(b == 0 or 32 <= b < 127 for b in text):
            return m.group(0)
        return f'((undefined *)0x{v:x})'
    # "&DAT_00460020 + i" is an address in pointer arithmetic, not a string, even when its
    # bytes are printable: the code used [eax*4+460018h] and Ghidra added the +8.
    body = re.sub(r'&(_*DAT_([0-9a-f]{8}))(?![\w\[])(?!\s*[+\-])', immediate, body)
    # Relocated addresses (SR_VA, LF_RELOC) come from LiftExport.java (relocatedConstants).
    body, fpre, fpost = stack_frame(body)
    pre += fpre
    post += fpost
    # Rename the definition: FUN_x(...) -> lf_<rva>(...), with an explicit convention.
    head_end = body.index('{')
    head = body[:head_end]
    lf = f'lf_{rva:05x}'
    head = re.sub(r'\b(__cdecl|__stdcall|__fastcall|__thiscall)\s+', '', head)
    if cc == '__thiscall':
        # The standalone build lifts a method as a fastcall with an unused EDX slot, as the callers
        # of a method call it. The hook table skips it: a hook cannot keep ECX.
        if not TARGET.get('lift_loops'):
            return None, 'thiscall definition'
        UNHOOKABLE.add(rva)
        cc = '__fastcall'
        head = re.sub(r'\b' + re.escape(name) + r'\s*\(', f'{cc} {lf}(', head, count=1)
        if len(params) > 1:
            head = re.sub(re.escape(lf) + r'\(([^,]+),', lf + r'(\1, int edx_unused,', head, count=1)
        return '\n'.join(pre) + '\n' + head + body[head_end:] + '\n' + '\n'.join(post) + '\n', None
    head = re.sub(r'\b' + re.escape(name) + r'\s*\(', f'{cc} {lf}(', head, count=1)
    text = '\n'.join(pre) + '\n' + head + body[head_end:] + '\n' + '\n'.join(post) + '\n'
    return text, None


WIDE_TYPES = {'float10', 'double', 'longlong', 'ulonglong', 'undefined8', 'qword', 'undefined5', 'undefined6',
              'undefined7'}
FLOAT_RETURNS = {'float10', 'double', 'float'}
BYTE_TYPES = {'bool', 'char', 'uchar', 'byte', 'undefined1', 'sbyte'}
WORD_TYPES = {'short', 'ushort', 'word', 'undefined2', 'wchar16'}


def fpu_thunk(rva, ret, params, index=-1):
    """Naked x86 thunk around hk_<rva>.

    The 024d was built with whole-program optimization, so callers break the normal ABI:
    - they keep live values on the x87 stack across calls;
    - they keep values in ECX/EDX (normally scratch registers) across calls, when the
      original callee does not touch them.
    The thunk saves the caller x87 state (FXSAVE), gives the C code an empty x87 stack
    with the same control word, and restores everything on return: the x87 state
    (FXRSTOR, with a float result pushed back on top) and EAX/ECX/EDX, except the
    registers that carry the return value. The saved registers go to the free bytes
    464..511 of the FXSAVE area."""
    dwords = sum(2 if t in WIDE_TYPES else 1 for t, _ in params)
    if ret == 'void' or ret in FLOAT_RETURNS:
        keep = ['eax', 'ecx', 'edx']
    elif ret in WIDE_TYPES:
        keep = ['ecx']
    else:
        keep = ['ecx', 'edx']
    slot = {'eax': 464, 'ecx': 468, 'edx': 472}
    lines = [f'__declspec(naked) static void th_{rva:05x}(void)', '{', '    __asm {',
             '        push ebp', '        mov ebp, esp', '        and esp, -16', '        sub esp, 512',
             '        fxsave [esp]',
             '        mov [esp + 464], eax', '        mov [esp + 468], ecx', '        mov [esp + 472], edx',
             '        fninit', '        fldcw word ptr [esp]',
             # Inputs for the offline test (instrumentation/hook/record.c, subvulpes.ini record_args=1).
             '        lea eax, [ebp + 8]', f'        push {dwords}', '        push eax', f'        push {index}',
             f'        push 0x{rva:x}', '        call sv_record_args', '        add esp, 16']
    for i in reversed(range(dwords)):
        lines.append(f'        push dword ptr [ebp + {8 + 4 * i}]')
    lines.append(f'        call hk_{rva:05x}')
    # A 1- or 2-byte return: C leaves the upper bits of EAX undefined. The original code
    # usually returns a clean EAX (xor eax, eax / setcc al), and an optimized caller can
    # read all of EAX. Zero-extend it.
    if ret in BYTE_TYPES:
        lines.append('        movzx eax, al')
    elif ret in WORD_TYPES:
        lines.append('        movzx eax, ax')
    if dwords:
        lines.append(f'        add esp, {4 * dwords}')
    if ret in FLOAT_RETURNS:
        lines += ['        sub esp, 16', '        fstp qword ptr [esp]', '        fxrstor [esp + 16]']
        lines += [f'        mov {r}, [esp + {16 + slot[r]}]' for r in keep]
        lines += ['        fld qword ptr [esp]', '        add esp, 16']
    else:
        lines.append('        fxrstor [esp]')
        lines += [f'        mov {r}, [esp + {slot[r]}]' for r in keep]
    lines += ['        mov esp, ebp', '        pop ebp', '        ret', '    }', '}']
    return chr(10).join(lines)


def gen_hooks(lifted, headers, funcs, names):
    """instrumentation/hook/hooks_lifted.c: wrappers and SvHook table for the lifted functions."""
    out = ['/* Generated by instrumentation/lift/lift.py. Do not edit. */',
           '#define WIN32_LEAN_AND_MEAN', '#include <windows.h>', '', '#include <string.h>', '',
           '#include "hook.h"', '#include "hook_wrap.h"', '#include "lift_support.h"', '#include "record.h"',
           '#include "verify.h"', '']
    table, stats, inits = [], [], []
    used_names = Counter()
    for rva in lifted:
        ret, cc, _name, params = headers[rva]
        f = funcs[rva]
        nice = names.get(f'0x{rva:x}', f'lf_{rva:05x}')
        used_names[nice] += 1
        if used_names[nice] > 1:
            nice = f'{nice}_{rva:05x}'
        plist = ', '.join(f'{t} {n}' for t, n in params) or 'void'
        types = ', '.join(t for t, _ in params) or 'void'
        args = ', '.join(n for _, n in params)
        lf = symbol(rva)
        out.append(f'{ret} {cc} {lf}({plist});')
        out.append(f'static {ret} ({cc} *orig_{rva:05x})({types});')
        out.append(f'static SvVerify v_{rva:05x} = {{ "{nice}", -1 }};')
        if ret == 'void':
            body = f'SV_HOOK_VOID(v_{rva:05x}, {lf}({args}), orig_{rva:05x}({args}), NULL);'
        else:
            mac = 'SV_HOOK_RET_FLOAT' if ret in FLOAT_RETURNS else 'SV_HOOK_RET'
            body = f'{mac}({ret}, v_{rva:05x}, {lf}({args}), orig_{rva:05x}({args}), NULL);'
        out.append(f'static {ret} {cc} hk_{rva:05x}({plist})\n{{\n    {body}\n}}')
        out.append(fpu_thunk(rva, ret, params, len(table)))
        pro = f['prologue']
        bs = bytes.fromhex(pro['bytes'])
        out.append(f'static uint8_t expect_{rva:05x}[{len(bs)}] = {{ {", ".join(f"0x{b:02x}" for b in bs)} }};')
        for off in pro['relocs']:
            inits.append(f'    relocate(expect_{rva:05x}, {off});')
        table.append(f'    {{ "{nice}", 0x{rva:x}, (void *)th_{rva:05x}, {len(bs)}, expect_{rva:05x}, mask_all, '
                     f'(void **)&orig_{rva:05x}, 0, 0 }},')
        stats.append(f'    &v_{rva:05x},')
        out.append('')
    out += ['static const uint8_t mask_all[32] = {',
            '    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,',
            '    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,',
            '};', '',
            'static SvHook g_lifted[] = {'] + (table or ['    { 0 },']) + ['};', '',
            'static SvVerify *g_lifted_stats[] = {'] + (stats or ['    0,']) + ['};', '',
            '/* An absolute address in the prologue moves with ASLR: add the relocation delta. */',
            'static void relocate(uint8_t *expect, int off)',
            '{',
            '    uint32_t v;',
            '    memcpy(&v, expect + off, 4);',
            '    v += (uint32_t)(sr_base - 0x400000u);',
            '    memcpy(expect + off, &v, 4);',
            '}', '',
            'int sv_hooks_lifted_install(void)',
            '{',
            '    x87_init();',
            '    /* subvulpes.ini lifted_from / lifted_to: install only a range (to find a bad hook). */',
            '    int from = sv_config_int("lifted_from", 0);',
            f'    int to = sv_config_int("lifted_to", {len(lifted)});'] + inits + [
            f'    if (to > {len(lifted)})',
            f'        to = {len(lifted)};',
            '    if (from < 0)',
            '        from = 0;',
            '    if (from >= to)',
            '        return 0;',
            f'    sv_log("lifted hooks %d..%d of {len(lifted)}", from, to);',
            '    return sv_install_hooks(g_lifted + from, to - from);',
            '}', '',
            'void sv_hooks_lifted_profile(void)',
            '{',
            f'    sv_verify_profile(g_lifted_stats, {len(lifted)}, 25);',
            '}', '',
            'void sv_hooks_lifted_summary(void)',
            '{',
            '    int i;',
            f'    for (i = 0; i < {len(lifted)}; i++)',
            '        if (g_lifted_stats[i]->calls)',
            '            sv_verify_summary(g_lifted_stats[i]);',
            '}', '']
    return '\n'.join(out)


# Side-effect functions that get no record-only hook: the loops that run until the player
# leaves (TARGET['record_skip']) and the CRT (TARGET['crt_start'] and up).
LOOP_CALLERS = set()


def loop_callers(funcs):
    """Functions that reach a loop of record_skip through calls (client only: the harness would
    run the loop forever)."""
    if not TARGET['skip_callers']:
        return set()
    callers = defaultdict(set)
    for rva, f in funcs.items():
        for c in f['calls']:
            if not c['external'] and 'rva' in c:
                callers[int(c['rva'], 16)].add(rva)
    out, todo = set(), list(TARGET['record_skip'])
    while todo:
        for caller in callers[todo.pop()]:
            if caller not in out:
                out.add(caller)
                todo.append(caller)
    return out


def harness_only(rva, side):
    """A side-effect function of the game that the lifter lifts anyway. The live server keeps
    the original (a second run would send a second packet). The offline harness tests it in
    the environment of instrumentation/platform/plat_test.c."""
    return (rva in side and rva not in TARGET['record_skip'] and rva not in LOOP_CALLERS
            and rva < TARGET['crt_start'])


def gen_record_hooks(side, headers, funcs, names):
    """instrumentation/hook/hooks_record.c: a record-only hook for each side-effect function of the game
    (docs/side-effects.md). The thunk keeps every register, the flags and the x87 state. It
    replaces the return address, calls the original with the same stack, and records the
    arguments, the import calls (instrumentation/hook/record_imports.c) and the return value."""
    out = ['/* Generated by instrumentation/lift/lift.py. Do not edit. */',
           '#define WIN32_LEAN_AND_MEAN', '#include <windows.h>', '', '#include <string.h>', '',
           '#include "hook.h"', '#include "record.h"', '#include "sr_runtime.h"', '']
    table, inits = [], []
    for rva in sorted(side):
        if rva in TARGET['record_skip'] or rva >= TARGET['crt_start'] or rva not in headers:
            continue
        f = funcs[rva]
        pro = f['prologue']
        bs = bytes.fromhex(pro['bytes'])
        # A relative call or jump in the prologue goes to a wrong place from the trampoline
        # (loadaccountsserver starts with a call).
        if len(bs) < 5 or pro['relative']:
            continue
        params = headers[rva][3]
        ndw = sum(2 if t in WIDE_TYPES else 1 for t, _ in params)
        nice = names.get(f'0x{rva:x}', f'lf_{rva:05x}')
        out.append(f'static void *orig_r{rva:05x};')
        out.append(f'__declspec(naked) static void rt_{rva:05x}(void)\n{{\n    __asm {{')
        save = ['pushad', 'pushfd', 'mov ebp, esp', 'and esp, -16', 'sub esp, 512', 'fxsave [esp]', 'fninit',
                'fldcw word ptr [esp]']
        load = ['fxrstor [esp]', 'mov esp, ebp', 'popfd', 'popad']
        asm = (save + ['lea eax, [ebp + 40]', f'push {ndw}', 'push eax', f'push 0x{rva:x}', 'call sv_rec_begin',
                       'add esp, 12', 'push dword ptr [ebp + 36]', 'call sv_rec_push_ret', 'add esp, 4'] + load +
               ['add esp, 4', f'call dword ptr [orig_r{rva:05x}]', 'sub esp, 4'] + save +
               ['push dword ptr [ebp + 32]', 'call sv_rec_end', 'add esp, 4', 'mov [ebp + 36], eax'] + load + ['ret'])
        out += [f'        {a}' for a in asm] + ['    }', '}']
        out.append(f'static uint8_t expect_r{rva:05x}[{len(bs)}] = {{ {", ".join(f"0x{b:02x}" for b in bs)} }};')
        for off in pro['relocs']:
            inits.append(f'    relocate(expect_r{rva:05x}, {off});')
        table.append(f'    {{ "record_{nice}", 0x{rva:x}, (void *)rt_{rva:05x}, {len(bs)}, expect_r{rva:05x}, mask_all, '
                     f'&orig_r{rva:05x}, 0, 0 }},')
        out.append('')
    out += ['static const uint8_t mask_all[32] = {',
            '    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,',
            '    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,',
            '};', '',
            'static SvHook g_record[] = {'] + (table or ['    { 0 },']) + ['};', '',
            'static void relocate(uint8_t *expect, int off)',
            '{',
            '    uint32_t v;',
            '    memcpy(&v, expect + off, 4);',
            '    v += (uint32_t)(sr_base - 0x400000u);',
            '    memcpy(expect + off, &v, 4);',
            '}', '',
            '/* subvulpes.ini record_imports=1: import wrappers and record-only hooks. */',
            'int sv_hooks_record_install(void)',
            '{',
            '    if (!sv_config_int("record_imports", 0))',
            '        return 0;'] + inits + [
            '    sv_record_imports_install();',
            f'    return sv_install_hooks(g_record, {len(table)});',
            '}', '']
    return chr(10).join(out)


def gen_table(lifted, headers, names, side):
    """src/lifted/lifted_table.c: the lifted functions for the offline test (instrumentation/tests/harness.c).
    Per function: name, RVA, the lifted C function, argument size in dwords, return kind, and
    1 for a side-effect function (the standalone recording build records it with its imports)."""
    out = ['/* Generated by instrumentation/lift/lift.py. Do not edit. */', '#include "lifted_table.h"', '']
    rows = []
    used = Counter()
    for rva in lifted:
        ret, _cc, _n, params = headers[rva]
        dwords = sum(2 if t in WIDE_TYPES else 1 for t, _ in params)
        kind = ('LF_RET_VOID' if ret == 'void' else 'LF_RET_FLOAT' if ret in FLOAT_RETURNS
                else 'LF_RET_INT64' if ret in WIDE_TYPES else 'LF_RET_INT')
        nice = names.get(f'0x{rva:x}', f'lf_{rva:05x}')
        used[nice] += 1
        if used[nice] > 1:
            nice = f'{nice}_{rva:05x}'
        if _cc == '__thiscall' and rva in UNHOOKABLE:
            # Lifted as a fastcall with an unused EDX slot: the symbol name holds the argument size.
            n = len(params) + (1 if len(params) > 1 else 0)
            out.append(f'void __fastcall {symbol(rva)}({", ".join(["int"] * n)});')
        else:
            out.append(f'void {symbol(rva)}(void);')
        size = 1 if ret in BYTE_TYPES else 2 if ret in WORD_TYPES else 4
        # One letter per argument dword: p = pointer, v = value.
        kinds = ''.join(('vv' if t in WIDE_TYPES else 'p' if t.strip().endswith('*') else 'v') for t, _ in params)
        rows.append(f'    {{ "{nice}", 0x{rva:x}, (void *){symbol(rva)}, {dwords}, {kind}, {size}, "{kinds}", '
                    f'{2 if rva in TARGET.get("standalone_only", ()) else int(rva in side)} }},')
    out += ['', 'const LfEntry lf_table[] = {'] + (rows or ['    { 0 },']) + ['};',
            f'const int lf_table_count = {len(lifted)};', '']
    return chr(10).join(out)


def gen_lifted(texts, imports_used):
    out = ['/* Generated by instrumentation/lift/lift.py from Ghidra pseudo-C of subrosadedicated024d.exe.',
           ' * Do not edit: improve the lifter or move the function to src/server/ by hand. */',
           '#include "lift_support.h"', '#include "lift_os.h"',
           *(f'#include "{h}"' for h in TARGET['extra_includes']), '',
           f'static void *lf_imports[{max(1, len(imports_used))}];',
           '#define LF_IMPORT(i, dll, name) (lf_imports[i] ? lf_imports[i] : (lf_imports[i] = sr_import(dll, name)))',
           '#define LF_IMPORT_ORD(i, dll, o) (lf_imports[i] ? lf_imports[i] : (lf_imports[i] = sr_import_ordinal(dll, o)))',
           '']
    lines = {}
    for rva, text in texts:
        start = sum(t.count('\n') for t in out) + len(out) + 1
        out.append(f'/* ---------------------------------------------------------------- 0x{rva:x} */')
        out.append(text)
        end = sum(t.count('\n') for t in out) + len(out)
        lines[rva] = (start, end)
    return '\n'.join(out) + '\n', lines


def compile_errors(path):
    """Compile one file with MSVC x86 (/arch:IA32). Returns {line: message}."""
    bat = ROOT / 'build/lift/compile.bat'
    crlf = chr(13) + chr(10)
    bat.write_text(vcvars_line() +
                   f'cl /nologo /c /TC /std:c11 /W0 /we4013 /arch:IA32 /utf-8 /errorReport:none /I "{ROOT / "src/lifted"}" /I "{ROOT / "src/include"}" '
                   f'/Fo"{ROOT / "build/lift/lifted.obj"}" "{path}"{crlf}', encoding='mbcs')
    r = subprocess.run(['cmd', '/c', str(bat)], capture_output=True, text=True, encoding='mbcs', errors='replace')
    errs = {}
    for m in re.finditer(r'\((\d+)\)\s*:\s*(?:fatal )?error (C\d+):\s*(.*)', r.stdout + r.stderr):
        errs.setdefault(int(m.group(1)), f'{m.group(2)} {m.group(3).strip()}')
    return errs


def compile_loop(texts, imports_used, status, out_c, compile_=True, verbose=True, failed=None):
    """Write out_c, compile it, fix what the lifter can fix and remove the functions that
    still fail (status[rva] = compile_error). Returns the remaining texts."""
    for attempt in range(60):
        code, lines = gen_lifted(texts, imports_used)
        out_c.write_text(code, encoding='utf-8', newline='\n')
        if not compile_:
            break
        errs = compile_errors(out_c)
        if not errs:
            break
        # Errors that the lifter can fix in place: float stored in a pointer variable.
        tmap = dict(texts)
        fixed = set()
        # The fixes keep the line count, so several fixes per function work in one round.
        for line, msg in sorted(errs.items(), reverse=True):
            q = re.findall(r"'([^']*)'", msg)
            if not (msg.startswith('C2440') and len(q) >= 2 and q[-2] in ('float', 'double', 'float10')
                    and q[-1].rstrip().endswith('*')):
                continue
            for rva, (a, b) in lines.items():
                if a <= line <= b:
                    new = fix_text_line(tmap[rva], line - (a + 1), q[-1].strip())
                    if new is not None and new != tmap[rva]:
                        tmap[rva] = new
                        fixed.add(rva)
        if fixed:
            texts = [(r, tmap[r]) for r, _ in texts]
            verbose and print(f'attempt {attempt + 1}: fixed float/pointer casts in {len(fixed)} function(s)')
            continue
        bad = {}
        for line, msg in errs.items():
            for rva, (a, b) in lines.items():
                if a <= line <= b:
                    bad.setdefault(rva, msg)
        if not bad:
            verbose and print('compile errors outside functions:', list(errs.items())[:5])
            break
        for rva, msg in bad.items():
            status[rva] = ('compile_error', msg)
        if failed is not None:
            failed.update({r: t for r, t in texts if r in bad})
        texts = [(r, t) for r, t in texts if r not in bad]
        verbose and print(f'attempt {attempt + 1}: removed {len(bad)} function(s) with compile errors')

    return texts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--compile', action='store_true')
    ap.add_argument('--only')
    ap.add_argument('--max', type=int)
    ap.add_argument('--target', choices=sorted(TARGETS), default='ded024d')
    args = ap.parse_args()
    global TARGET
    TARGET = TARGETS[args.target]
    global SKIPPED_CALLS, INLINE_CALLS
    SKIPPED_CALLS, INLINE_CALLS = TARGET['skipped'], dict(TARGET['inline'])
    for rva in TARGET.get('ignored', ()):
        INLINE_CALLS[rva] = '#define {name}(...) 0'
    for key in ('lifted', 'hooks', 'record', 'table', 'report'):
        (ROOT / TARGET[key]).parent.mkdir(parents=True, exist_ok=True)

    funcs = {}
    with open(ROOT / TARGET['export'], encoding='utf-8') as fh:
        for line in fh:
            f = json.loads(line)
            funcs[int(f['rva'], 16)] = f
    headers = {rva: h for rva, f in funcs.items() if (h := parse_header(f['c']))}
    # The stored return type is a value (instrumentation/ghidra/FixReturnTypes.java: callers read EAX),
    # but the decompiled body returns nothing: the original leaves an intermediate value in
    # EAX and optimized callers use it. Callers see an int return. The function itself
    # stays original until its C version computes the same EAX.
    eax_only = set()
    for rva, f in funcs.items():
        h = headers.get(rva)
        if h and h[0] == 'void' and f.get('ret', 'void') not in ('void', 'undefined'):
            headers[rva] = ('undefined4',) + tuple(h[1:])
            eax_only.add(rva)
    names = load_names()
    hand = handwritten_rvas()
    rewritten = rewritten_rvas(names)
    side = side_effects(funcs)
    LOOP_CALLERS.update(loop_callers(funcs))
    only = {int(x, 16) for x in args.only.split(',')} if args.only else None

    status = {}
    texts = []
    imports_used = {}
    for rva in sorted(funcs):
        if only is not None and rva not in only:
            continue
        if rva in TARGET.get('ignored', ()):
            status[rva] = ('ignored', 'Steam C++ glue')
            continue
        if rva in hand:
            status[rva] = ('handwritten', '')
            continue
        if rva in rewritten:
            continue
        if rva in side and not harness_only(rva, side) and not is_loop(rva):
            status[rva] = ('side_effect', side[rva])
            continue
        if rva in eax_only:
            status[rva] = ('blocked', 'callers read EAX, the C body returns nothing')
            continue
        text, err = lift_function(funcs[rva], rva, headers, imports_used)
        if err:
            status[rva] = ('blocked', err)
            continue
        texts.append((rva, text))
        if args.max and len(texts) >= args.max:
            break

    texts = compile_loop(texts, imports_used, status, ROOT / TARGET['lifted'], args.compile)

    lifted = sorted([r for r, _ in texts] + [r for r in rewritten if only is None or r in only])
    for r in lifted:
        status[r] = (('loop' if is_loop(r) else 'harness_only'), side[r]) if r in side else ('lifted', '')
    for r in rewritten:
        status[r] = ('rewritten', 'by hand')
    for r in lifted:
        if r in UNHOOKABLE and r not in side:
            status[r] = ('unhookable', '')
    live = [r for r in lifted if r not in side and r not in UNHOOKABLE]
    (ROOT / TARGET['hooks']).write_text(gen_hooks(live, headers, funcs, names), encoding='utf-8', newline='\n')
    (ROOT / TARGET['record']).write_text(gen_record_hooks(side, headers, funcs, names), encoding='utf-8',
                                         newline='\n')
    (ROOT / TARGET['table']).write_text(gen_table(lifted, headers, names, side), encoding='utf-8', newline='\n')

    with open(ROOT / TARGET['report'], 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh, lineterminator='\n')
        w.writerow(['rva', 'name', 'size', 'status', 'reason'])
        for rva in sorted(status):
            w.writerow([f'0x{rva:x}', names.get(f'0x{rva:x}', ''), funcs[rva]['prologue']['len'] and
                        len(funcs[rva]['c']), *status[rva]])
    count = Counter(s for s, _ in status.values())
    print('result:', dict(count))
    reasons = Counter(r.split(' (')[0] for s, r in status.values() if s in ('blocked', 'side_effect', 'compile_error'))
    print('top reasons:', reasons.most_common(12))


if __name__ == '__main__':
    main()
