"""Lift Ghidra pseudo-C of the 024d to C that compiles with MSVC, and generate hooks.

Input:  build/lift/ded024d.jsonl (tools/ghidra/LiftExport.java)
Output: src/lifted/lifted_024d.c   lifted functions (lf_<rva>)
        hook/hooks_lifted.c        hook table + verification wrappers
        build/lift/report.csv      status of every function

A function is NOT lifted when:
  - it already has a hand-written hook (hook/hooks_024d.c);
  - the pseudo-C has decompiler artifacts (register inputs, SEH, x87 intrinsics without
    arguments, computed jumps...);
  - the prologue has a relative jump (no safe trampoline);
  - it reaches a side effect outside memory (network, files, time, heap), directly or
    through its callees. Verification runs a function twice, so these cannot be verified;
  - it does not compile (found with --compile; the loop removes it and tries again).

Usage:
  python -I tools/lift/lift.py [--compile] [--only 0x6660,0xa580] [--max N]
"""
import argparse
import csv
import json
import re
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x400000
VCVARS = r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat'

# Decompiler artifacts that the lifter does not handle (yet).
BLOCKERS = {
    'in_': 'register input', 'unaff_': 'register input', 'extraout_': 'register output',
    'halt_baddata': 'bad data', 'ExceptionList': 'SEH frame', 'stack0x': 'stack address',
    'switchD': 'computed jump', '(code *)': 'function pointer call', 'in_ST0': 'x87 input',
    'POPCOUNT': 'popcount', 'LOCK()': 'lock prefix',
    'segment(': 'segment', 'CONCAT3': 'odd concat',
    'rdtsc': 'time stamp counter',
}

# Imports with no effect outside memory (safe to run twice in a verification).
PURE_IMPORTS = {
    'memset', 'memcpy', 'memmove', 'memcmp', 'strncmp', 'strcmp', 'strlen', 'strcpy', 'strncpy',
    'strcat', 'strchr', 'strstr', 'sprintf', 'sscanf', 'rand', 'srand', 'floor', 'ceil', 'fabs',
    'abs', 'atoi', 'atof', 'toupper', 'tolower', 'qsort', '_stricmp', '_strnicmp',
    'htons', 'htonl', 'ntohs', 'ntohl',
    '_CIsqrt', '_CIsin', '_CIcos', '_CItan', '_CIacos', '_CIatan2', '_CIpow',
}

# x87 intrinsics: arguments on the x87 stack. Lifted code calls the thunks in x87_intrinsics.c.
X87_INTRINSICS = {'_CIsqrt', '_CIsin', '_CIcos', '_CItan', '_CIacos', '_CIatan2', '_CIpow'}

# Byte swaps: inline (WinSock imports of the 024d are by ordinal).
INLINE_IMPORTS = {
    'htons': '#define htons(x) ((ushort)((((ushort)(x)) >> 8) | (((ushort)(x)) << 8)))',
    'ntohs': '#define ntohs(x) ((ushort)((((ushort)(x)) >> 8) | (((ushort)(x)) << 8)))',
    'htonl': '#define htonl(x) ((uint)(((uint)(x) >> 24) | (((uint)(x) >> 8) & 0xff00u) | (((uint)(x) << 8) & 0xff0000u) | ((uint)(x) << 24)))',
    'ntohl': '#define ntohl(x) ((uint)(((uint)(x) >> 24) | (((uint)(x) >> 8) & 0xff00u) | (((uint)(x) << 8) & 0xff0000u) | ((uint)(x) << 24)))',
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
    # __cdecl_keepC/D/F (tools/ghidra/FixRegisterPreservation.java) only tell the decompiler
    # which registers a callee keeps. For C they are __cdecl.
    cc = re.sub(r'^__cdecl_keep[CDF]+$', '__cdecl', m.group('cc') or '__cdecl')
    return m.group('ret').strip(), cc, m.group('name'), params


def proto_cast(ret, cc, params):
    """Function pointer type for a cast: RET (CC *)(T1, T2)."""
    if cc == '__thiscall':
        cc = '__fastcall'  # thiscall = fastcall with an unused EDX in C
        params = [params[0], ('int', 'edx_unused')] + list(params[1:]) if params else params
    plist = ', '.join(t if t == '...' else t for t, _ in params) or 'void'
    return f'{ret} ({cc} *)({plist})'


def global_macro(g):
    name, va, typ = g['name'], int(g['va'], 16), g['type'].strip()
    if typ in ('undefined', ''):
        typ = 'undefined'
    m = re.match(r'(.+?)\s*\[(\d+)\]$', typ)
    if m:
        return f'#define {name} (*({m.group(1)} (*)[{m.group(2)}])SR_VA(0x{va:x}))'
    return f'#define {name} (*({typ} *)SR_VA(0x{va:x}))'


def partial_access(c):
    """Ghidra 'x._4_2_' (2 bytes at offset 4 of x) -> C pointer access."""
    def rep(m):
        t = SIZE_TYPE.get(int(m.group(3)), 'uchar')
        return f'(*({t} *)((uchar *)&{m.group(1)} + {m.group(2)}))'
    return re.sub(r'\b([A-Za-z_]\w*)\._(\d+)_(\d+)_', rep, c)


def load_names():
    names = {}
    with open(ROOT / 'analysis/024d/functions.csv', encoding='utf-8') as fh:
        for r in csv.DictReader(fh):
            if r['name']:
                names[r['rva']] = r['name']
    return names


def handwritten_rvas():
    src = (ROOT / 'hook/hooks_024d.c').read_text(encoding='utf-8')
    return {int(x, 16) for x in re.findall(r'\{\s*"\w+",\s*(0x[0-9a-fA-F]+),', src)}


def side_effects(funcs):
    """Function rva -> reason, for functions that reach an impure import."""
    direct = {}
    graph = defaultdict(set)
    for rva, f in funcs.items():
        for c in f['calls']:
            if c['external']:
                if c['target'] not in PURE_IMPORTS and rva not in direct:
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


def lift_function(f, rva, headers, imports_used):
    """Return (C text, error). error is None on success."""
    c = f['c']
    for tok, why in BLOCKERS.items():
        if tok in c:
            return None, why
    # A tiny float literal (1.4013e-45 = int 1 read as float) means Ghidra typed an int as float.
    if re.search(r'(?<![0-9.])\d\.\d+e-(3[89]|4[0-5])(?![0-9])', c):
        return None, 'int typed as float (denormal literal)'
    hdr = headers.get(rva)
    if not hdr:
        return None, 'header not parsed'
    ret, cc, name, params = hdr
    if f['prologue']['relative'] or f['prologue']['len'] < 5:
        return None, 'prologue not hookable'
    if cc != '__cdecl':
        return None, f'calling convention {cc}'
    if any(t == '...' for t, _ in params):
        return None, 'variadic'

    pre, post = [], []
    for g in f['globals']:
        pre.append(global_macro(g))
        post.append(f'#undef {g["name"]}')
    for call in f['calls']:
        cname = call['name']
        if call['external']:
            tgt = call['target']
            if tgt in INLINE_IMPORTS:
                pre.append(INLINE_IMPORTS[tgt])
                post.append(f'#undef {cname}')
                continue
            if tgt in X87_INTRINSICS:
                pre.append(f'#define {cname} lf{tgt}')
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
            pre.append(f'#define {cname} (({proto_cast(iret, icc, iparams)})LF_IMPORT({idx}, "{key[0]}", "{tgt}"))')
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
            pre.append(f'#define {cname} (({proto_cast(cret, ccc, cparams)})SR_VA(0x{callee + BASE:x}))')
            post.append(f'#undef {cname}')

    body = partial_access(strip_comments(c)).strip()
    body = re.sub(r'\b__cdecl_keep[CDF]+\b', '__cdecl', body)
    # Absolute addresses that the loader relocates (ASLR) but the pseudo-C shows as numbers.
    for v in f.get('relocvals', []):
        body = re.sub(r'(?<![\w])0x0*%x(?![0-9a-fA-FxX])' % v, f'((int)SR_VA(0x{v:x}))', body)
    # Rename the definition: FUN_x(...) -> lf_<rva>(...), with an explicit convention.
    head_end = body.index('{')
    head = body[:head_end]
    lf = f'lf_{rva:05x}'
    head = re.sub(r'\b(__cdecl|__stdcall|__fastcall|__thiscall)\s+', '', head)
    if cc == '__thiscall':
        return None, 'thiscall definition'
    head = re.sub(r'\b' + re.escape(name) + r'\s*\(', f'{cc} {lf}(', head, count=1)
    text = '\n'.join(pre) + '\n' + head + body[head_end:] + '\n' + '\n'.join(post) + '\n'
    return text, None


WIDE_TYPES = {'float10', 'double', 'longlong', 'ulonglong', 'undefined8', 'qword', 'undefined5', 'undefined6',
              'undefined7'}
FLOAT_RETURNS = {'float10', 'double', 'float'}


def fpu_thunk(rva, ret, params):
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
             '        fninit', '        fldcw word ptr [esp]']
    for i in reversed(range(dwords)):
        lines.append(f'        push dword ptr [ebp + {8 + 4 * i}]')
    lines.append(f'        call hk_{rva:05x}')
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
    """hook/hooks_lifted.c: wrappers and SvHook table for the lifted functions."""
    out = ['/* Generated by tools/lift/lift.py. Do not edit. */',
           '#define WIN32_LEAN_AND_MEAN', '#include <windows.h>', '', '#include <string.h>', '',
           '#include "hook.h"', '#include "hook_wrap.h"', '#include "lift_support.h"', '#include "verify.h"', '']
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
        lf = f'lf_{rva:05x}'
        out.append(f'{ret} {cc} {lf}({plist});')
        out.append(f'static {ret} ({cc} *orig_{rva:05x})({types});')
        out.append(f'static SvVerify v_{rva:05x} = {{ "{nice}", -1 }};')
        if ret == 'void':
            body = f'SV_HOOK_VOID(v_{rva:05x}, {lf}({args}), orig_{rva:05x}({args}), NULL);'
        else:
            mac = 'SV_HOOK_RET_FLOAT' if ret in FLOAT_RETURNS else 'SV_HOOK_RET'
            body = f'{mac}({ret}, v_{rva:05x}, {lf}({args}), orig_{rva:05x}({args}), NULL);'
        out.append(f'static {ret} {cc} hk_{rva:05x}({plist})\n{{\n    {body}\n}}')
        out.append(fpu_thunk(rva, ret, params))
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
            '    lf_x87_init();',
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
            'void sv_hooks_lifted_summary(void)',
            '{',
            '    int i;',
            f'    for (i = 0; i < {len(lifted)}; i++)',
            '        if (g_lifted_stats[i]->calls)',
            '            sv_verify_summary(g_lifted_stats[i]);',
            '}', '']
    return '\n'.join(out)


def gen_lifted(texts, imports_used):
    out = ['/* Generated by tools/lift/lift.py from Ghidra pseudo-C of subrosadedicated024d.exe.',
           ' * Do not edit: improve the lifter or move the function to src/server/ by hand. */',
           '#include "lift_support.h"', '',
           f'static void *lf_imports[{max(1, len(imports_used))}];',
           '#define LF_IMPORT(i, dll, name) (lf_imports[i] ? lf_imports[i] : (lf_imports[i] = sr_import(dll, name)))',
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
    bat.write_text(f'@call "{VCVARS}" >nul 2>nul{crlf}'
                   f'cl /nologo /c /TC /W0 /we4013 /arch:IA32 /utf-8 /errorReport:none /I "{ROOT / "src/lifted"}" '
                   f'/Fo"{ROOT / "build/lift/lifted.obj"}" "{path}"{crlf}', encoding='mbcs')
    r = subprocess.run(['cmd', '/c', str(bat)], capture_output=True, text=True, encoding='mbcs', errors='replace')
    errs = {}
    for m in re.finditer(r'\((\d+)\)\s*:\s*(?:fatal )?error (C\d+):\s*(.*)', r.stdout + r.stderr):
        errs.setdefault(int(m.group(1)), f'{m.group(2)} {m.group(3).strip()}')
    return errs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--compile', action='store_true')
    ap.add_argument('--only')
    ap.add_argument('--max', type=int)
    args = ap.parse_args()

    funcs = {}
    with open(ROOT / 'build/lift/ded024d.jsonl', encoding='utf-8') as fh:
        for line in fh:
            f = json.loads(line)
            funcs[int(f['rva'], 16)] = f
    headers = {rva: h for rva, f in funcs.items() if (h := parse_header(f['c']))}
    names = load_names()
    hand = handwritten_rvas()
    side = side_effects(funcs)
    only = {int(x, 16) for x in args.only.split(',')} if args.only else None

    status = {}
    texts = []
    imports_used = {}
    for rva in sorted(funcs):
        if only is not None and rva not in only:
            continue
        if rva in hand:
            status[rva] = ('handwritten', '')
            continue
        if rva in side:
            status[rva] = ('side_effect', side[rva])
            continue
        text, err = lift_function(funcs[rva], rva, headers, imports_used)
        if err:
            status[rva] = ('blocked', err)
            continue
        texts.append((rva, text))
        if args.max and len(texts) >= args.max:
            break

    out_c = ROOT / 'src/lifted/lifted_024d.c'
    for attempt in range(12):
        code, lines = gen_lifted(texts, imports_used)
        out_c.write_text(code, encoding='utf-8', newline='\n')
        if not args.compile:
            break
        errs = compile_errors(out_c)
        if not errs:
            break
        bad = {}
        for line, msg in errs.items():
            for rva, (a, b) in lines.items():
                if a <= line <= b:
                    bad.setdefault(rva, msg)
        if not bad:
            print('compile errors outside functions:', list(errs.items())[:5])
            break
        for rva, msg in bad.items():
            status[rva] = ('compile_error', msg)
        texts = [(r, t) for r, t in texts if r not in bad]
        print(f'attempt {attempt + 1}: removed {len(bad)} function(s) with compile errors')

    lifted = [r for r, _ in texts]
    for r in lifted:
        status[r] = ('lifted', '')
    (ROOT / 'hook/hooks_lifted.c').write_text(gen_hooks(lifted, headers, funcs, names), encoding='utf-8', newline='\n')

    with open(ROOT / 'build/lift/report.csv', 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh, lineterminator='\n')
        w.writerow(['rva', 'name', 'size', 'status', 'reason'])
        for rva in sorted(status):
            w.writerow([f'0x{rva:x}', names.get(f'0x{rva:x}', ''), funcs[rva]['prologue']['len'] and
                        len(funcs[rva]['c']), *status[rva]])
    count = Counter(s for s, _ in status.values())
    print('result:', dict(count))
    reasons = Counter(r.split(' (')[0] for s, r in status.values() if s in ('blocked', 'side_effect'))
    print('top reasons:', reasons.most_common(12))


if __name__ == '__main__':
    main()
