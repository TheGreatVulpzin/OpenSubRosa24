"""Make build/lift/client_win/plat_client_stubs.inc: test stubs for the imports of the client.

instrumentation/platform/plat_test.c includes the file in the harness of the client. Each stub has
the exact signature of analysis/client_win/imports.csv. It writes what the call did into the
environment log, so the harness can compare the lifted run with the original run:

- OpenGL calls and the vector calls of OpenAL go into one hash per run (hundreds of thousands of calls).
- Every other call writes one line with its scalar arguments.
- A pointer argument is not compared, because the stack addresses of the two runs differ.
  A few functions give the size of the data behind a pointer (PTR_DWORDS).

Return values are fixed: the same in both runs and in every test.

Usage: python -I instrumentation/client/gen_plat_stubs.py
"""
import re
import sys

OUT = 'build/lift/client_win/plat_client_stubs.inc'

# Data behind a pointer argument that the stub hashes: function -> {parameter: dwords}.
PTR_DWORDS = {
    'glColor3fv': {'v': 3}, 'glNormal3fv': {'v': 3}, 'glTexCoord2fv': {'v': 2}, 'glVertex2fv': {'v': 2},
    'glVertex3fv': {'v': 3}, 'glLightfv': {'params': 4}, 'glMultMatrixf': {'m': 16},
    'alListenerfv': {'values': 3}, 'alSourcefv': {'values': 3},
}

# Calls whose result or output memory is fixed. The body can use the parameter names of the CSV.
CUSTOM = {
    'glGetString': 'return (unsigned char *)"SubVulpes";',
    'glGenTextures': 'int i; for (i = 0; i < n; i++) textures[i] = ++g_ids;',
    'alGenBuffers': 'int i; for (i = 0; i < n; i++) buffers[i] = ++g_ids;',
    'alGenSources': 'int i; for (i = 0; i < n; i++) sources[i] = ++g_ids;',
    'alGetString': 'return "";',
    'alcGetString': 'return "";',
    'alGetSourcei': '*value = 0;',
    'alcGetIntegerv': 'if (size > 0) data[0] = 0;',
    'alcOpenDevice': 'return (void *)0x1000;',
    'alcCreateContext': 'return (void *)0x2000;',
    'alcGetCurrentContext': 'return (void *)0x2000;',
    'alcGetContextsDevice': 'return (void *)0x1000;',
    'alcCaptureOpenDevice': 'return (void *)0x3000;',
    'alcMakeContextCurrent': 'return 1;',
    'alcCloseDevice': 'return 1;',
    'alcCaptureCloseDevice': 'return 1;',
    'SDL_GetTicks': 'return (unsigned int)(g_qpc / 10000);',
    'SDL_Delay': 'g_qpc += (LONGLONG)ms * 10000;',
    'SDL_malloc': 'return st_malloc(size);',
    'SDL_iconv_string': 'char *p = (char *)st_malloc(inbytesleft + 4); if (p) { memcpy(p, inbuf, inbytesleft); '
                        'memset(p + inbytesleft, 0, 4); } return p;',
    'SDL_wcslen': 'unsigned int n = 0; while (wstr[n]) n++; return n;',
    'SDL_isspace': 'return x == 32 || (x >= 9 && x <= 13);',
    'SDL_GetNumDisplayModes': 'return 1;',
    'SDL_GetDisplayMode': 'unsigned int *m = (unsigned int *)mode; m[0] = 0x16161804u; m[1] = 1600; m[2] = 900; '
                          'm[3] = 60; m[4] = 0; return 0;',
    'SDL_GetKeyboardState': 'static unsigned char keys[512]; const char *input = getenv("SV_INPUT_KEYS"); '
                            'memset(keys, 0, sizeof(keys)); '
                            'while (input && *input) { char *end; long key = strtol(input, &end, 10); '
                            'if (end == input) break; if (key >= 0 && key < 512) keys[key] = 1; '
                            'input = *end == 44 ? end + 1 : end; } '
                            'if (numkeys) *numkeys = 512; return keys;',
    'SDL_GetMouseState': 'const char *input = getenv("SV_INPUT_MOUSE"); '
                         'int mx = 0, my = 0, dx = 0, dy = 0; unsigned buttons = 0; '
                         'if (input) sscanf(input, "%d,%d,%d,%d,%u", &mx, &my, &dx, &dy, &buttons); '
                         'if (x) *x = mx; if (y) *y = my; return buttons;',
    'SDL_GetRelativeMouseState': 'const char *input = getenv("SV_INPUT_MOUSE"); '
                                 'int mx = 0, my = 0, dx = 0, dy = 0; unsigned buttons = 0; '
                                 'if (input) sscanf(input, "%d,%d,%d,%d,%u", &mx, &my, &dx, &dy, &buttons); '
                                 'if (x) *x = dx; if (y) *y = dy; return buttons;',
    'speex_decoder_init': 'return (void *)0x4000;',
    'speex_encoder_init': 'return (void *)0x4000;',
    'speex_preprocess_state_init': 'return (void *)0x4000;',
    'speex_lib_get_mode': 'return (void *)0x5000;',
}

# Imports that the CSV does not list because Ghidra knows their prototype.
EXTRA = [
    ('OPENGL32.DLL', 'wglGetProcAddress', '__stdcall', 'void *wglGetProcAddress(char *name)'),
    ('OPENGL32.DLL', 'wglGetCurrentDC', '__stdcall', 'void *wglGetCurrentDC(void)'),
    ('SHELL32.DLL', 'ShellExecuteA', '__stdcall',
     'void *ShellExecuteA(void *hwnd, char *op, char *file, char *params, char *dir, int show)'),
    ('SHELL32.DLL', 'SHGetSpecialFolderPathA', '__stdcall',
     'int SHGetSpecialFolderPathA(void *hwnd, char *path, int csidl, int create)'),
]
CUSTOM['ShellExecuteA'] = 'return (void *)33;'
# Winsock is not started in the harness: the real call fails, so the stub fails the same way.
EXTRA.append(('WSOCK32.DLL', 'gethostname', '__stdcall', 'int gethostname(char *name, int namelen)'))
CUSTOM['gethostname'] = 'return -1;'
# The client imports these by ordinal. The stub table needs the ordinal, not the name.
BY_ORDINAL = {'gethostname': 57}

BULK_RE = re.compile(r'^(gl(?!GetString|GenTextures)|alSource[fi]|alSource3f|alListener)')


def parse(proto):
    m = re.match(r'^(.*?)(\w+)\((.*)\)$', proto.strip())
    ret, name, params = m.group(1).strip(), m.group(2), m.group(3).strip()
    plist = [] if params in ('', 'void') else [p.strip() for p in params.split(',')]
    out = []
    for p in plist:
        pname = re.search(r'(\w+)$', p).group(1)
        out.append((p, pname, '*' in p))
    return ret, name, out


def stub(dll, name, cc, proto):
    ret, _, params = parse(proto)
    sig = ', '.join(p for p, _, _ in params) or 'void'
    body = ['    unsigned char rec_buf[256]; size_t rec_len = 0;', '    (void)rec_buf; (void)rec_len;']
    for p, pname, is_ptr in params:
        if not is_ptr:
            body.append('    PUT(%s);' % pname)
        elif pname in PTR_DWORDS.get(name, {}):
            body.append('    PUTBYTES(%s, %d);' % (pname, PTR_DWORDS[name][pname] * 4))
    kind = 'bulk' if BULK_RE.match(name) else 'ev'
    body.append('    REC_%s("%s", rec_buf, rec_len);' % (kind.upper(), name))
    custom = CUSTOM.get(name)
    if custom:
        for p, pname, _ in params:
            body.append('    (void)%s;' % pname)
        body.append('    ' + custom)
    elif ret != 'void':
        for p, pname, _ in params:
            body.append('    (void)%s;' % pname)
        body.append('    return (%s)0;' % ret)
    else:
        for p, pname, _ in params:
            body.append('    (void)%s;' % pname)
    return 'static %s %s cs_%s(%s)\n{\n%s\n}\n' % (ret, cc, name, sig, '\n'.join(body))


def main():
    rows = []
    # The prototype is the last field and keeps its commas.
    with open('analysis/client_win/imports.csv') as f:
        next(f)
        for line in f:
            if line.strip():
                dll, name, cc, proto = line.strip().split(',', 3)
                rows.append((dll, name, cc, proto))
    rows += EXTRA
    o = ['/* Generated by instrumentation/client/gen_plat_stubs.py. Do not edit. */', '']
    for r in rows:
        o.append(stub(*r))
    o.append('#define CL_STUB_ENTRIES \\')
    for dll, name, _, _ in rows:
        if name in BY_ORDINAL:
            o.append('    { "%s", NULL, %d, (void *)cs_%s }, \\' % (dll, BY_ORDINAL[name], name))
        else:
            o.append('    { "%s", "%s", 0, (void *)cs_%s }, \\' % (dll, name, name))
    o.append('')
    open(OUT, 'w', newline='\n').write('\n'.join(o) + '\n')
    print('%d stubs -> %s' % (len(rows), OUT))


if __name__ == '__main__':
    sys.exit(main())
