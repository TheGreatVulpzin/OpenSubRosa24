/* Import wrappers for the record of the side-effect functions (docs/side-effects.md).
 *
 * subvulpes.ini: record_imports=1. At startup each wrapper replaces one import of the 024d:
 * - MSVCR100 and KERNEL32: the slot in the import table (IAT) of the 024d. getpaths copies
 *   the fread and fwrite slots into two globals later, so these copies are wrappers too.
 * - WinSock: the pointer that the proxy stub jumps through (instrumentation/hook/proxy.c).
 * A wrapper calls the real function. While a recorded call is open (sv_rec_active), it
 * also writes one event line with the arguments, the bytes in and out, and the return.
 * Outside a recorded call the wrapper only adds the check of sv_rec_active.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hook.h"
#include "record.h"
#include "sr_runtime.h"

FARPROC *sv_wsock_slot(const char *name);

/* The IAT slot of dll!name in the 024d, or NULL. */
static void **iat_slot(const char *dll, const char *name)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)sr_base;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(sr_base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    const IMAGE_IMPORT_DESCRIPTOR *imp = (const IMAGE_IMPORT_DESCRIPTOR *)(sr_base + dir->VirtualAddress);
    for (; imp->Name; imp++) {
        const IMAGE_THUNK_DATA32 *names;
        IMAGE_THUNK_DATA32 *slots;
        if (_stricmp((const char *)(sr_base + imp->Name), dll) != 0)
            continue;
        names = (const IMAGE_THUNK_DATA32 *)(sr_base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk
                                                                                  : imp->FirstThunk));
        slots = (IMAGE_THUNK_DATA32 *)(sr_base + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))
                continue;
            if (strcmp((const char *)((const IMAGE_IMPORT_BY_NAME *)(sr_base + names->u1.AddressOfData))->Name,
                       name) == 0)
                return (void **)&slots->u1.Function;
        }
    }
    return NULL;
}

/* Put wrapper in the IAT slot of dll!name. *real gets the old address. */
static int patch(const char *dll, const char *name, void *wrapper, void **real)
{
    void **slot = iat_slot(dll, name);
    DWORD old;
    if (!slot) {
        sv_log("record: import %s!%s not found", dll, name);
        return 0;
    }
    if (!VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old))
        return 0;
    *real = *slot;
    *slot = wrapper;
    VirtualProtect(slot, sizeof *slot, old, &old);
    return 1;
}

static int patch_wsock(const char *name, void *wrapper, void **real)
{
    FARPROC *slot = sv_wsock_slot(name);
    if (!slot || !*slot)
        return 0;
    *real = (void *)*slot;
    *slot = (FARPROC)(uintptr_t)wrapper;
    return 1;
}

#define ON sv_rec_active()

/* ------------------------------------------------------------ MSVCR100 */

typedef struct { int dummy; } SvFile;  /* the FILE of MSVCR100.dll: an opaque handle here */

static void *(__cdecl *r_malloc)(size_t);
static void *__cdecl w_malloc(size_t n)
{
    void *p = r_malloc(n);
    if (ON) {
        sv_ev_begin("malloc"); sv_ev_v((uint32_t)n); sv_ev_r(p != NULL); sv_ev_end();
    }
    return p;
}

static void(__cdecl *r_free)(void *);
static void __cdecl w_free(void *p)
{
    if (ON) {
        sv_ev_begin("free"); sv_ev_v(p != NULL); sv_ev_end();
    }
    r_free(p);
}

/* Files opened for reading only. A replay reads them again from the server folder (the map
 * and the buildings are the same files). So fread and fgetc on such a file write no event:
 * createlevel alone makes 3.2 million 4-byte reads. The fclose event has the number of read
 * calls, the number of bytes and a hash of all of them, to check that the file is the same. */
#define RO_MAX 64
typedef struct { SvFile *f; uint32_t calls, bytes, hash; } RoFile;
static RoFile g_ro[RO_MAX];

static RoFile *read_only(SvFile *f)
{
    int i;
    for (i = 0; i < RO_MAX; i++)
        if (f && g_ro[i].f == f)
            return &g_ro[i];
    return NULL;
}

static void ro_add(RoFile *ro, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    size_t i;
    ro->calls++;
    ro->bytes += (uint32_t)n;
    for (i = 0; i < n; i++)
        ro->hash = (ro->hash ^ b[i]) * 16777619u;
}

static SvFile *(__cdecl *r_fopen)(const char *, const char *);
static SvFile *__cdecl w_fopen(const char *path, const char *mode)
{
    SvFile *f = r_fopen(path, mode);
    if (f && mode && strchr(mode, 'r') && !strchr(mode, '+') && !strchr(mode, 'w') && !strchr(mode, 'a')) {
        int i;
        for (i = 0; i < RO_MAX && g_ro[i].f; i++)
            ;
        if (i < RO_MAX) {
            g_ro[i].f = f;
            g_ro[i].calls = g_ro[i].bytes = 0;
            g_ro[i].hash = 2166136261u;
        }
    }
    if (ON) {
        sv_ev_begin("fopen"); sv_ev_s(path); sv_ev_s(mode); sv_ev_r((uint32_t)(uintptr_t)f); sv_ev_end();
    }
    return f;
}

static int(__cdecl *r_fclose)(SvFile *);
static int __cdecl w_fclose(SvFile *f)
{
    RoFile *ro = read_only(f), copy = { 0 };
    int r;
    if (ro) {
        copy = *ro;
        ro->f = NULL;
    }
    r = r_fclose(f);
    if (ON) {
        sv_ev_begin("fclose"); sv_ev_v((uint32_t)(uintptr_t)f);
        if (copy.f) {
            /* read-only file: reads, bytes and hash of everything read */
            sv_ev_v(copy.calls); sv_ev_v(copy.bytes); sv_ev_v(copy.hash);
        }
        sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(__cdecl *r_fgetc)(SvFile *);
static int __cdecl w_fgetc(SvFile *f)
{
    int r = r_fgetc(f);
    RoFile *ro = read_only(f);
    if (ro) {
        unsigned char c = (unsigned char)r;
        ro_add(ro, &c, r == -1 ? 0 : 1);
    } else if (ON) {
        sv_ev_begin("fgetc"); sv_ev_v((uint32_t)(uintptr_t)f); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(__cdecl *r_fseek)(SvFile *, long, int);
static int __cdecl w_fseek(SvFile *f, long off, int origin)
{
    int r = r_fseek(f, off, origin);
    if (ON) {
        sv_ev_begin("fseek"); sv_ev_v((uint32_t)(uintptr_t)f); sv_ev_v((uint32_t)off); sv_ev_v((uint32_t)origin);
        sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static size_t(__cdecl *r_fread)(void *, size_t, size_t, SvFile *);
static size_t __cdecl w_fread(void *buf, size_t size, size_t count, SvFile *f)
{
    size_t r = r_fread(buf, size, count, f);
    RoFile *ro = read_only(f);
    if (ro) {
        ro_add(ro, buf, r * size);
    } else if (ON) {
        sv_ev_begin("fread"); sv_ev_v((uint32_t)size); sv_ev_v((uint32_t)count); sv_ev_v((uint32_t)(uintptr_t)f);
        sv_ev_bytes('o', buf, r * size); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static size_t(__cdecl *r_fwrite)(const void *, size_t, size_t, SvFile *);
static size_t __cdecl w_fwrite(const void *buf, size_t size, size_t count, SvFile *f)
{
    size_t r = r_fwrite(buf, size, count, f);
    if (ON) {
        sv_ev_begin("fwrite"); sv_ev_v((uint32_t)size); sv_ev_v((uint32_t)count); sv_ev_v((uint32_t)(uintptr_t)f);
        sv_ev_bytes('b', buf, size * count); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

/* fprintf and printf: the wrapper formats the text with the vsnprintf of MSVCR100 (the same
 * formatting as the game) and writes it with the real fputs/vprintf path of MSVCR100. */
static int(__cdecl *r_fprintf)(SvFile *, const char *, ...);
static int(__cdecl *m_vfprintf)(SvFile *, const char *, va_list);
static int(__cdecl *m_vsnprintf)(char *, size_t, const char *, va_list);
static int __cdecl w_fprintf(SvFile *f, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = m_vfprintf(f, fmt, ap);
    va_end(ap);
    if (ON) {
        char text[1024];
        va_start(ap, fmt);
        m_vsnprintf(text, sizeof text - 1, fmt, ap);
        va_end(ap);
        text[sizeof text - 1] = 0;
        sv_ev_begin("fprintf"); sv_ev_v((uint32_t)(uintptr_t)f); sv_ev_bytes('b', text, strlen(text));
        sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(__cdecl *r_printf)(const char *, ...);
static int(__cdecl *m_vprintf)(const char *, va_list);
static int __cdecl w_printf(const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = m_vprintf(fmt, ap);
    va_end(ap);
    if (ON) {
        char text[1024];
        va_start(ap, fmt);
        m_vsnprintf(text, sizeof text - 1, fmt, ap);
        va_end(ap);
        text[sizeof text - 1] = 0;
        sv_ev_begin("printf"); sv_ev_bytes('b', text, strlen(text)); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(__cdecl *r_chdir)(const char *);
static int __cdecl w_chdir(const char *path)
{
    int r = r_chdir(path);
    if (ON) {
        sv_ev_begin("_chdir"); sv_ev_s(path); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static char *(__cdecl *r_getcwd)(char *, int);
static char *__cdecl w_getcwd(char *buf, int size)
{
    char *r = r_getcwd(buf, size);
    if (ON) {
        sv_ev_begin("_getcwd"); sv_ev_v((uint32_t)size); sv_ev_s(r); sv_ev_r(r != NULL); sv_ev_end();
    }
    return r;
}

#define FINDDATA_SIZE 296  /* _finddata64i32_t of MSVC 2010 */

static intptr_t(__cdecl *r_findfirst)(const char *, void *);
static intptr_t __cdecl w_findfirst(const char *mask, void *data)
{
    intptr_t r = r_findfirst(mask, data);
    if (ON) {
        sv_ev_begin("_findfirst64i32"); sv_ev_s(mask); sv_ev_bytes('o', data, r == -1 ? 0 : FINDDATA_SIZE);
        sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(__cdecl *r_findnext)(intptr_t, void *);
static int __cdecl w_findnext(intptr_t h, void *data)
{
    int r = r_findnext(h, data);
    if (ON) {
        sv_ev_begin("_findnext64i32"); sv_ev_v((uint32_t)h); sv_ev_bytes('o', data, r == 0 ? FINDDATA_SIZE : 0);
        sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(__cdecl *r_findclose)(intptr_t);
static int __cdecl w_findclose(intptr_t h)
{
    int r = r_findclose(h);
    if (ON) {
        sv_ev_begin("_findclose"); sv_ev_v((uint32_t)h); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static __int64(__cdecl *r_time64)(__int64 *);
static __int64 __cdecl w_time64(__int64 *t)
{
    __int64 r = r_time64(t);
    if (ON) {
        sv_ev_begin("_time64"); sv_ev_v(t != NULL); sv_ev_bytes('o', &r, 8); sv_ev_end();
    }
    return r;
}

static void(__cdecl *r_sleep)(unsigned long);
static void __cdecl w_sleep(unsigned long ms)
{
    if (ON) {
        sv_ev_begin("_sleep"); sv_ev_v((uint32_t)ms); sv_ev_end();
    }
    r_sleep(ms);
}

static void(__cdecl *r_tzset)(void);
static void __cdecl w_tzset(void)
{
    r_tzset();
    if (ON) {
        sv_ev_begin("_tzset"); sv_ev_end();
    }
}

static long *(__cdecl *r_timezone)(void);
static long *__cdecl w_timezone(void)
{
    long *r = r_timezone();
    if (ON) {
        sv_ev_begin("__timezone"); sv_ev_bytes('o', r, 4); sv_ev_end();
    }
    return r;
}

static int *(__cdecl *r_daylight)(void);
static int *__cdecl w_daylight(void)
{
    int *r = r_daylight();
    if (ON) {
        sv_ev_begin("__daylight"); sv_ev_bytes('o', r, 4); sv_ev_end();
    }
    return r;
}

/* ------------------------------------------------------------ KERNEL32 */

static BOOL(WINAPI *r_qpc)(LARGE_INTEGER *);
static BOOL WINAPI w_qpc(LARGE_INTEGER *c)
{
    BOOL r = r_qpc(c);
    if (ON) {
        sv_ev_begin("QueryPerformanceCounter"); sv_ev_bytes('o', c, 8); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static BOOL(WINAPI *r_qpf)(LARGE_INTEGER *);
static BOOL WINAPI w_qpf(LARGE_INTEGER *c)
{
    BOOL r = r_qpf(c);
    if (ON) {
        sv_ev_begin("QueryPerformanceFrequency"); sv_ev_bytes('o', c, 8); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static void(WINAPI *r_gstaft)(FILETIME *);
static void WINAPI w_gstaft(FILETIME *t)
{
    r_gstaft(t);
    if (ON) {
        sv_ev_begin("GetSystemTimeAsFileTime"); sv_ev_bytes('o', t, 8); sv_ev_end();
    }
}

static HANDLE(WINAPI *r_gcp)(void);
static HANDLE WINAPI w_gcp(void)
{
    HANDLE r = r_gcp();
    if (ON) {
        sv_ev_begin("GetCurrentProcess"); sv_ev_r((uint32_t)(uintptr_t)r); sv_ev_end();
    }
    return r;
}

static BOOL(WINAPI *r_gpam)(HANDLE, DWORD_PTR *, DWORD_PTR *);
static BOOL WINAPI w_gpam(HANDLE h, DWORD_PTR *proc, DWORD_PTR *sys)
{
    BOOL r = r_gpam(h, proc, sys);
    if (ON) {
        sv_ev_begin("GetProcessAffinityMask"); sv_ev_v((uint32_t)(uintptr_t)h); sv_ev_bytes('o', proc, 4);
        sv_ev_bytes('o', sys, 4); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static BOOL(WINAPI *r_spam)(HANDLE, DWORD_PTR);
static BOOL WINAPI w_spam(HANDLE h, DWORD_PTR mask)
{
    BOOL r = r_spam(h, mask);
    if (ON) {
        sv_ev_begin("SetProcessAffinityMask"); sv_ev_v((uint32_t)(uintptr_t)h); sv_ev_v((uint32_t)mask);
        sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

/* ------------------------------------------------------------ WinSock */

typedef struct { unsigned int count; uintptr_t fd[64]; } SvFdSet;  /* fd_set of WinSock 1.1 */

static int(WINAPI *r_sendto)(uintptr_t, const char *, int, int, const void *, int);
static int WINAPI w_sendto(uintptr_t s, const char *buf, int len, int flags, const void *to, int tolen)
{
    int r = r_sendto(s, buf, len, flags, to, tolen);
    if (ON) {
        sv_ev_begin("sendto"); sv_ev_v((uint32_t)s); sv_ev_v((uint32_t)flags); sv_ev_bytes('b', buf, (size_t)len);
        sv_ev_bytes('b', to, tolen > 0 ? (size_t)tolen : 0); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(WINAPI *r_recvfrom)(uintptr_t, char *, int, int, void *, int *);
static int WINAPI w_recvfrom(uintptr_t s, char *buf, int len, int flags, void *from, int *fromlen)
{
    int in_len = fromlen ? *fromlen : 0;
    int r = r_recvfrom(s, buf, len, flags, from, fromlen);
    if (ON) {
        sv_ev_begin("recvfrom"); sv_ev_v((uint32_t)s); sv_ev_v((uint32_t)len); sv_ev_v((uint32_t)flags);
        sv_ev_v((uint32_t)in_len); sv_ev_bytes('o', buf, r > 0 ? (size_t)r : 0);
        sv_ev_bytes('o', from, r >= 0 && fromlen ? (size_t)*fromlen : 0); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(WINAPI *r_send)(uintptr_t, const char *, int, int);
static int WINAPI w_send(uintptr_t s, const char *buf, int len, int flags)
{
    int r = r_send(s, buf, len, flags);
    if (ON) {
        sv_ev_begin("send"); sv_ev_v((uint32_t)s); sv_ev_v((uint32_t)flags); sv_ev_bytes('b', buf, (size_t)len);
        sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(WINAPI *r_recv)(uintptr_t, char *, int, int);
static int WINAPI w_recv(uintptr_t s, char *buf, int len, int flags)
{
    int r = r_recv(s, buf, len, flags);
    if (ON) {
        sv_ev_begin("recv"); sv_ev_v((uint32_t)s); sv_ev_v((uint32_t)len); sv_ev_v((uint32_t)flags);
        sv_ev_bytes('o', buf, r > 0 ? (size_t)r : 0); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static uintptr_t(WINAPI *r_socket)(int, int, int);
static uintptr_t WINAPI w_socket(int af, int type, int proto)
{
    uintptr_t r = r_socket(af, type, proto);
    if (ON) {
        sv_ev_begin("socket"); sv_ev_v((uint32_t)af); sv_ev_v((uint32_t)type); sv_ev_v((uint32_t)proto);
        sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(WINAPI *r_bind)(uintptr_t, const void *, int);
static int WINAPI w_bind(uintptr_t s, const void *addr, int len)
{
    int r = r_bind(s, addr, len);
    if (ON) {
        sv_ev_begin("bind"); sv_ev_v((uint32_t)s); sv_ev_bytes('b', addr, len > 0 ? (size_t)len : 0);
        sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(WINAPI *r_connect)(uintptr_t, const void *, int);
static int WINAPI w_connect(uintptr_t s, const void *addr, int len)
{
    int r = r_connect(s, addr, len);
    if (ON) {
        sv_ev_begin("connect"); sv_ev_v((uint32_t)s); sv_ev_bytes('b', addr, len > 0 ? (size_t)len : 0);
        sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(WINAPI *r_closesocket)(uintptr_t);
static int WINAPI w_closesocket(uintptr_t s)
{
    int r = r_closesocket(s);
    if (ON) {
        sv_ev_begin("closesocket"); sv_ev_v((uint32_t)s); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(WINAPI *r_ioctlsocket)(uintptr_t, long, unsigned long *);
static int WINAPI w_ioctlsocket(uintptr_t s, long cmd, unsigned long *arg)
{
    unsigned long in = arg ? *arg : 0;
    int r = r_ioctlsocket(s, cmd, arg);
    if (ON) {
        sv_ev_begin("ioctlsocket"); sv_ev_v((uint32_t)s); sv_ev_v((uint32_t)cmd); sv_ev_v((uint32_t)in);
        sv_ev_bytes('o', arg, 4); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(WINAPI *r_getsockopt)(uintptr_t, int, int, char *, int *);
static int WINAPI w_getsockopt(uintptr_t s, int level, int opt, char *val, int *len)
{
    int r = r_getsockopt(s, level, opt, val, len);
    if (ON) {
        sv_ev_begin("getsockopt"); sv_ev_v((uint32_t)s); sv_ev_v((uint32_t)level); sv_ev_v((uint32_t)opt);
        sv_ev_bytes('o', val, len && r == 0 ? (size_t)*len : 0); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(WINAPI *r_select)(int, SvFdSet *, SvFdSet *, SvFdSet *, const void *);
static int WINAPI w_select(int n, SvFdSet *rd, SvFdSet *wr, SvFdSet *ex, const void *timeout)
{
    int r;
    if (ON) {
        sv_ev_begin("select-in"); sv_ev_v((uint32_t)n); sv_ev_bytes('b', rd, rd ? sizeof *rd : 0);
        sv_ev_bytes('b', wr, wr ? sizeof *wr : 0); sv_ev_bytes('b', ex, ex ? sizeof *ex : 0);
        sv_ev_bytes('b', timeout, timeout ? 8 : 0); sv_ev_end();
    }
    r = r_select(n, rd, wr, ex, timeout);
    if (ON) {
        sv_ev_begin("select"); sv_ev_bytes('o', rd, rd ? sizeof *rd : 0); sv_ev_bytes('o', wr, wr ? sizeof *wr : 0);
        sv_ev_bytes('o', ex, ex ? sizeof *ex : 0); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

static int(WINAPI *r_fdisset)(uintptr_t, SvFdSet *);
static int WINAPI w_fdisset(uintptr_t s, SvFdSet *set)
{
    int r = r_fdisset(s, set);
    if (ON) {
        sv_ev_begin("__WSAFDIsSet"); sv_ev_v((uint32_t)s); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

typedef struct { char *name; char **aliases; short type, length; char **list; } SvHostent;

static SvHostent *(WINAPI *r_gethostbyname)(const char *);
static SvHostent *WINAPI w_gethostbyname(const char *name)
{
    SvHostent *h = r_gethostbyname(name);
    if (ON) {
        sv_ev_begin("gethostbyname"); sv_ev_s(name);
        sv_ev_bytes('o', h && h->list && h->list[0] ? h->list[0] : NULL, h ? (size_t)h->length : 0);
        sv_ev_r(h != NULL); sv_ev_end();
    }
    return h;
}

typedef struct { char *name; char **aliases; short port; char *proto; } SvServent;

static SvServent *(WINAPI *r_getservbyname)(const char *, const char *);
static SvServent *WINAPI w_getservbyname(const char *name, const char *proto)
{
    SvServent *s = r_getservbyname(name, proto);
    if (ON) {
        sv_ev_begin("getservbyname"); sv_ev_s(name); sv_ev_s(proto); sv_ev_v(s ? (uint32_t)(unsigned short)s->port : 0);
        sv_ev_r(s != NULL); sv_ev_end();
    }
    return s;
}

static int(WINAPI *r_wsastartup)(WORD, void *);
static int WINAPI w_wsastartup(WORD version, void *data)
{
    int r = r_wsastartup(version, data);
    if (ON) {
        sv_ev_begin("WSAStartup"); sv_ev_v(version); sv_ev_r((uint32_t)r); sv_ev_end();
    }
    return r;
}

/* ------------------------------------------------------------ install */

#define CRT "MSVCR100.dll"
#define K32 "KERNEL32.dll"
#define WS "WSOCK32.dll"

typedef struct {
    const char *dll, *name;
    void *wrapper;
    void **real;
} Wrap;

#define WR(dll, name, w, r) { dll, name, (void *)(uintptr_t)w, (void **)&r }
static const Wrap g_wraps[] = {
    WR(CRT, "malloc", w_malloc, r_malloc),
    WR(CRT, "free", w_free, r_free),
    WR(CRT, "fopen", w_fopen, r_fopen),
    WR(CRT, "fclose", w_fclose, r_fclose),
    WR(CRT, "fgetc", w_fgetc, r_fgetc),
    WR(CRT, "fseek", w_fseek, r_fseek),
    WR(CRT, "fread", w_fread, r_fread),
    WR(CRT, "fwrite", w_fwrite, r_fwrite),
    WR(CRT, "fprintf", w_fprintf, r_fprintf),
    WR(CRT, "printf", w_printf, r_printf),
    WR(CRT, "_chdir", w_chdir, r_chdir),
    WR(CRT, "_getcwd", w_getcwd, r_getcwd),
    WR(CRT, "_findfirst64i32", w_findfirst, r_findfirst),
    WR(CRT, "_findnext64i32", w_findnext, r_findnext),
    WR(CRT, "_findclose", w_findclose, r_findclose),
    WR(CRT, "_time64", w_time64, r_time64),
    WR(CRT, "_sleep", w_sleep, r_sleep),
    WR(CRT, "_tzset", w_tzset, r_tzset),
    WR(CRT, "__timezone", w_timezone, r_timezone),
    WR(CRT, "__daylight", w_daylight, r_daylight),
    WR(K32, "QueryPerformanceCounter", w_qpc, r_qpc),
    WR(K32, "QueryPerformanceFrequency", w_qpf, r_qpf),
    WR(K32, "GetSystemTimeAsFileTime", w_gstaft, r_gstaft),
    WR(K32, "GetCurrentProcess", w_gcp, r_gcp),
    WR(K32, "GetProcessAffinityMask", w_gpam, r_gpam),
    WR(K32, "SetProcessAffinityMask", w_spam, r_spam),
    WR(WS, "sendto", w_sendto, r_sendto),
    WR(WS, "recvfrom", w_recvfrom, r_recvfrom),
    WR(WS, "send", w_send, r_send),
    WR(WS, "recv", w_recv, r_recv),
    WR(WS, "socket", w_socket, r_socket),
    WR(WS, "bind", w_bind, r_bind),
    WR(WS, "connect", w_connect, r_connect),
    WR(WS, "closesocket", w_closesocket, r_closesocket),
    WR(WS, "ioctlsocket", w_ioctlsocket, r_ioctlsocket),
    WR(WS, "getsockopt", w_getsockopt, r_getsockopt),
    WR(WS, "select", w_select, r_select),
    WR(WS, "__WSAFDIsSet", w_fdisset, r_fdisset),
    WR(WS, "gethostbyname", w_gethostbyname, r_gethostbyname),
    WR(WS, "getservbyname", w_getservbyname, r_getservbyname),
    WR(WS, "WSAStartup", w_wsastartup, r_wsastartup),
};
#undef WR

/* The hook build: each wrapper goes into the IAT of the 024d (WinSock: the proxy stub). */
int sv_record_imports_install(void)
{
    HMODULE crt = GetModuleHandleA(CRT);
    size_t i;
    int n = 0;
    if (!crt) {
        sv_log("record: %s is not loaded", CRT);
        return 0;
    }
    *(FARPROC *)&m_vfprintf = GetProcAddress(crt, "vfprintf");
    *(FARPROC *)&m_vprintf = GetProcAddress(crt, "vprintf");
    *(FARPROC *)&m_vsnprintf = GetProcAddress(crt, "_vsnprintf");
    if (!m_vfprintf || !m_vprintf || !m_vsnprintf) {
        sv_log("record: vfprintf, vprintf or _vsnprintf not found in %s", CRT);
        return 0;
    }
    for (i = 0; i < sizeof g_wraps / sizeof *g_wraps; i++)
        n += !strcmp(g_wraps[i].dll, WS) ? patch_wsock(g_wraps[i].name, g_wraps[i].wrapper, g_wraps[i].real)
                                  : patch(g_wraps[i].dll, g_wraps[i].name, g_wraps[i].wrapper, g_wraps[i].real);
    sv_log("record: %d import wrappers installed", n);
    return n;
}

/* The standalone recording build: the CRT functions that the printf wrappers use. */
void sv_record_imports_printf(void *vfprintf_fn, void *vprintf_fn, void *vsnprintf_fn)
{
    *(void **)&m_vfprintf = vfprintf_fn;
    *(void **)&m_vprintf = vprintf_fn;
    *(void **)&m_vsnprintf = vsnprintf_fn;
}

/* The standalone recording build: sr_import asks for each import here. The wrapper keeps
 * real, the function of src/platform/plat_imports.c. An import without a wrapper (sprintf,
 * memcpy...) has no side effect: it stays real. */
void *sv_record_wrap(const char *dll, const char *name, void *real)
{
    size_t i;
    for (i = 0; i < sizeof g_wraps / sizeof *g_wraps; i++)
        if (!_stricmp(g_wraps[i].dll, dll) && !strcmp(g_wraps[i].name, name)) {
            *g_wraps[i].real = real;
            return g_wraps[i].wrapper;
        }
    return real;
}
