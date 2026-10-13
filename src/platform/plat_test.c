/* Platform layer for the offline harness (see platform.h and plat_test.h).
 *
 * The harness puts these functions into the import table of the 024d. The lifted code
 * reads its imports from the same table (sr_import), so both versions use them.
 *
 * - Clock: QueryPerformanceCounter starts at the same value in each run and advances by a
 *   fixed step at each call. _time64 and GetSystemTimeAsFileTime follow it. _sleep advances
 *   it and returns at once.
 * - Heap: malloc gives blocks from a pool at a fixed address. Each run starts with an empty,
 *   zeroed pool, so both runs get the same addresses. free does nothing.
 * - Files: reads come from the game folder. Each write goes to a copy in the sandbox folder
 *   of the run. fclose of a written file logs its size and hash.
 * - Network: no socket is real. send and sendto log the bytes, recv and recvfrom get no data,
 *   connect fails and name lookups find nothing.
 *
 * fread copies through a buffer of its own: the verifier protects the .data pages, and a
 * large fread goes straight to ReadFile, which fails on a protected page with no exception.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plat_test.h"
#include "platform.h"
#include "sr_runtime.h"

/* ------------------------------------------------------------------ MSVCR100 */

/* FILE of MSVCR100: not the FILE of the harness CRT. */
typedef void CFILE;

static CFILE *(__cdecl *r_fopen)(const char *, const char *);
static int(__cdecl *r_fclose)(CFILE *);
static size_t(__cdecl *r_fread)(void *, size_t, size_t, CFILE *);
static void *r_fwrite;
static int(__cdecl *r_chdir)(const char *);
static char *(__cdecl *r_getcwd)(char *, int);

/* ------------------------------------------------------------------ run state */

#define LOG_MAX (8u << 20)
#define POOL_SIZE (256u << 20)
#define MAX_FILES 64

typedef struct Log {
    char *text;
    size_t len, cap;
    int events, full;
} Log;

typedef struct TestFile {
    CFILE *f;
    int write;
    char rel[MAX_PATH];
    char path[MAX_PATH];
} TestFile;

static int g_run = -1;
static Log g_log[2];
static char g_root[MAX_PATH];      /* game folder, with a trailing backslash */
static char g_box[2][MAX_PATH];    /* sandbox folder of each run, with a trailing backslash */
static char g_saved_cwd[MAX_PATH];
static TestFile g_files[MAX_FILES];
static LONGLONG g_qpc;
static unsigned g_nsock;
static unsigned long long g_read_bytes;

static uint8_t *g_pool;
static size_t g_pool_used, g_pool_dirty;
static uint8_t *g_pool_copy;      /* the pool after run 0 */
static size_t g_pool_copy_len, g_pool_len[2];

static void ev(const char *fmt, ...)
{
    char line[1024];
    Log *l;
    va_list ap;
    int n;
    if (g_run < 0)
        return;
    l = &g_log[g_run];
    l->events++;
    if (l->full)
        return;
    va_start(ap, fmt);
    n = vsnprintf(line, sizeof line - 1, fmt, ap);
    va_end(ap);
    if (n < 0 || n > (int)sizeof line - 2)
        n = (int)sizeof line - 2;
    line[n++] = '\n';
    if (l->len + n > LOG_MAX) {
        l->full = 1;
        return;
    }
    if (l->len + n > l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 65536;
        while (cap < l->len + n)
            cap *= 2;
        l->text = realloc(l->text, cap);
        l->cap = cap;
    }
    memcpy(l->text + l->len, line, n);
    l->len += n;
}

static uint32_t fnv(const void *p, size_t n)
{
    const uint8_t *b = p;
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}

/* Up to 32 bytes as hex, for the log. Two buffers: two calls can share one printf. */
static const char *hex(const void *p, size_t n)
{
    static char buf[2][80];
    static int k;
    char *o = buf[k ^= 1];
    size_t i;
    if (!p)
        return "null";
    if (n > 32)
        n = 32;
    for (i = 0; i < n; i++)
        sprintf(o + 2 * i, "%02x", ((const uint8_t *)p)[i]);
    o[2 * n] = '\0';
    return o;
}

/* ------------------------------------------------------------------ clock */

#define QPC_START 1000000000LL    /* 100 s at 10 MHz */
#define QPC_STEP 1000             /* 0.1 ms per call */
#define UNIX_START 1767225600LL   /* 2026-01-01 */

static BOOL WINAPI st_QueryPerformanceCounter(LARGE_INTEGER *p)
{
    g_qpc += QPC_STEP;
    p->QuadPart = g_qpc;
    return TRUE;
}

static BOOL WINAPI st_QueryPerformanceFrequency(LARGE_INTEGER *p)
{
    p->QuadPart = 10000000;
    return TRUE;
}

/* FILETIME counts 100 ns from 1601. The counter also counts 100 ns. */
static void WINAPI st_GetSystemTimeAsFileTime(FILETIME *ft)
{
    ULONGLONG t = (ULONGLONG)(UNIX_START + 11644473600LL) * 10000000ULL + (ULONGLONG)g_qpc;
    ft->dwLowDateTime = (DWORD)t;
    ft->dwHighDateTime = (DWORD)(t >> 32);
}

static long long __cdecl st_time64(long long *p)
{
    long long t = UNIX_START + g_qpc / 10000000;
    if (p)
        *p = t;
    return t;
}

static void __cdecl st_sleep(unsigned long ms)
{
    ev("sleep %lu", ms);
    g_qpc += (LONGLONG)ms * 10000;
}

static BOOL WINAPI st_GetProcessAffinityMask(HANDLE h, DWORD_PTR *process, DWORD_PTR *system)
{
    (void)h;
    *process = 0xf;
    *system = 0xf;
    return TRUE;
}

static BOOL WINAPI st_SetProcessAffinityMask(HANDLE h, DWORD_PTR mask)
{
    (void)h;
    ev("SetProcessAffinityMask %lx", (unsigned long)mask);
    return TRUE;
}

/* ------------------------------------------------------------------ heap */

static void *__cdecl st_malloc(size_t n)
{
    size_t a = n ? (n + 15) & ~(size_t)15 : 16;
    void *p;
    if (a > POOL_SIZE - g_pool_used) {
        ev("malloc %u -> null", (unsigned)n);
        return NULL;
    }
    p = g_pool + g_pool_used;
    g_pool_used += a;
    if (g_pool_used > g_pool_dirty)
        g_pool_dirty = g_pool_used;
    ev("malloc %u -> +%x", (unsigned)n, (unsigned)((uint8_t *)p - g_pool));
    return p;
}

static void __cdecl st_free(void *p)
{
    if (!p)
        ev("free null");
    else if ((uint8_t *)p >= g_pool && (uint8_t *)p < g_pool + POOL_SIZE)
        ev("free +%x", (unsigned)((uint8_t *)p - g_pool));
    else
        ev("free %p (not from the pool)", p);
}

/* ------------------------------------------------------------------ files */

static int is_file(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* The path inside the game folder, or NULL for a path outside it. */
static const char *under_root(const char *full)
{
    size_t n = strlen(g_root);
    return _strnicmp(full, g_root, n) == 0 ? full + n : NULL;
}

static void make_dirs(const char *file)
{
    char d[MAX_PATH];
    char *p;
    strncpy(d, file, MAX_PATH - 1);
    d[MAX_PATH - 1] = '\0';
    for (p = d + 3; *p; p++) {
        if (*p == '\\') {
            *p = '\0';
            CreateDirectoryA(d, NULL);
            *p = '\\';
        }
    }
}

static uint32_t file_hash(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t buf[65536];
    uint32_t h = 2166136261u;
    size_t n, i;
    *size = 0;
    if (!f)
        return 0;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        for (i = 0; i < n; i++)
            h = (h ^ buf[i]) * 16777619u;
        *size += (long)n;
    }
    fclose(f);
    return h;
}

static CFILE *__cdecl st_fopen(const char *path, const char *mode)
{
    char full[MAX_PATH], box[MAX_PATH];
    const char *rel;
    int write = strpbrk(mode, "wa+") != NULL;
    CFILE *f;
    int i;

    if (!path || !GetFullPathNameA(path, MAX_PATH, full, NULL)) {
        ev("fopen %s %s -> bad path", path ? path : "null", mode);
        return NULL;
    }
    rel = under_root(full);
    if (write && (!rel || g_run < 0)) {
        /* Never write outside the sandbox. */
        ev("fopen %s %s -> refused", full, mode);
        return NULL;
    }
    if (rel)
        snprintf(box, MAX_PATH, "%s%s", g_box[g_run < 0 ? 0 : g_run], rel);
    else
        box[0] = '\0';
    if (!rel) {
        /* A read outside the game folder: the log shows the full path. */
        rel = full;
        f = r_fopen(full, mode);
    } else if (write) {
        make_dirs(box);
        if ((strchr(mode, 'a') || strchr(mode, '+')) && !is_file(box) && is_file(full))
            CopyFileA(full, box, TRUE);
        f = r_fopen(box, mode);
    } else {
        f = r_fopen(g_run >= 0 && is_file(box) ? box : full, mode);
    }
    ev("fopen %s %s -> %s", rel, mode, f ? "ok" : "null");
    if (f) {
        for (i = 0; i < MAX_FILES && g_files[i].f; i++)
            ;
        if (i < MAX_FILES) {
            g_files[i].f = f;
            g_files[i].write = write;
            strncpy(g_files[i].rel, rel, MAX_PATH - 1);
            strncpy(g_files[i].path, box, MAX_PATH - 1);
        }
    }
    return f;
}

/* Close one file of the table and log what it wrote. */
static int close_file(TestFile *o, const char *how)
{
    int r = r_fclose(o->f);
    if (o->write) {
        long size;
        uint32_t h = file_hash(o->path, &size);
        ev("%s %s: %ld bytes, hash %08x", how, o->rel, size, h);
    } else {
        ev("%s %s", how, o->rel);
    }
    o->f = NULL;
    return r;
}

static int __cdecl st_fclose(CFILE *f)
{
    int i;
    for (i = 0; i < MAX_FILES; i++)
        if (g_files[i].f && g_files[i].f == f)
            return close_file(&g_files[i], "fclose");
    ev("fclose of a file from outside the run");
    return -1;
}

static size_t __cdecl st_fread(void *buf, size_t size, size_t count, CFILE *f)
{
    static uint8_t tmp[65536];
    size_t total = size * count, done = 0;
    if (!size || !count)
        return 0;
    while (done < total) {
        size_t want = total - done < sizeof tmp ? total - done : sizeof tmp;
        size_t got = r_fread(tmp, 1, want, f);
        memcpy((uint8_t *)buf + done, tmp, got);
        done += got;
        if (got < want)
            break;
    }
    g_read_bytes += done;
    return done / size;
}

static int __cdecl st_chdir(const char *path)
{
    int r = r_chdir(path);
    ev("chdir %s -> %d", path, r);
    return r;
}

/* GetCurrentDirectory also writes past the kernel boundary: copy through a buffer. */
static char *__cdecl st_getcwd(char *buf, int size)
{
    char tmp[MAX_PATH * 2];
    if (!buf)
        return r_getcwd(NULL, size);
    if (!r_getcwd(tmp, size < (int)sizeof tmp ? size : (int)sizeof tmp))
        return NULL;
    memcpy(buf, tmp, strlen(tmp) + 1);
    return buf;
}

static int __cdecl st_printf(const char *fmt, ...)
{
    char text[1024];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    ev("printf %s", text);
    return n;
}

/* ------------------------------------------------------------------ network */

typedef UINT_PTR SOCK;

static SOCK WINAPI st_socket(int af, int type, int protocol)
{
    SOCK s = 0x100 + g_nsock++;
    ev("socket %d %d %d -> %x", af, type, protocol, (unsigned)s);
    return s;
}

static int WINAPI st_bind(SOCK s, const void *addr, int len)
{
    ev("bind %x %s", (unsigned)s, hex(addr, (size_t)len));
    return 0;
}

static int WINAPI st_ioctlsocket(SOCK s, long cmd, unsigned long *argp)
{
    ev("ioctlsocket %x %lx %lx", (unsigned)s, (unsigned long)cmd, argp ? *argp : 0);
    return 0;
}

static int WINAPI st_closesocket(SOCK s)
{
    ev("closesocket %x", (unsigned)s);
    return 0;
}

static int WINAPI st_connect(SOCK s, const void *addr, int len)
{
    ev("connect %x %s", (unsigned)s, hex(addr, (size_t)len));
    return -1;
}

static int WINAPI st_getsockopt(SOCK s, int level, int name, char *val, int *len)
{
    (void)val;
    (void)len;
    ev("getsockopt %x %d %d", (unsigned)s, level, name);
    return -1;
}

static void *WINAPI st_gethostbyname(const char *name)
{
    ev("gethostbyname %s", name ? name : "null");
    return NULL;
}

static void *WINAPI st_getservbyname(const char *name, const char *proto)
{
    ev("getservbyname %s %s", name ? name : "null", proto ? proto : "null");
    return NULL;
}

/* Nothing is ready: empty sets (fd_set starts with its count). */
static int WINAPI st_select(int nfds, unsigned *r, unsigned *w, unsigned *e, const long *timeout)
{
    ev("select %d timeout %ld.%06ld", nfds, timeout ? timeout[0] : -1L, timeout ? timeout[1] : 0L);
    if (r)
        *r = 0;
    if (w)
        *w = 0;
    if (e)
        *e = 0;
    return 0;
}

static int WINAPI st_WSAFDIsSet(SOCK s, void *set)
{
    (void)s;
    (void)set;
    return 0;
}

static int WINAPI st_send(SOCK s, const char *buf, int len, int flags)
{
    ev("send %x %d bytes flags %x hash %08x data %s", (unsigned)s, len, flags, fnv(buf, (size_t)len),
       hex(buf, (size_t)len));
    return len;
}

static int WINAPI st_sendto(SOCK s, const char *buf, int len, int flags, const void *to, int tolen)
{
    ev("sendto %x %d bytes flags %x to %s hash %08x data %s", (unsigned)s, len, flags, hex(to, (size_t)tolen),
       fnv(buf, (size_t)len), hex(buf, (size_t)len));
    return len;
}

static int WINAPI st_recv(SOCK s, char *buf, int len, int flags)
{
    (void)buf;
    ev("recv %x size %d flags %x -> no data", (unsigned)s, len, flags);
    return -1;
}

static int WINAPI st_recvfrom(SOCK s, char *buf, int len, int flags, void *from, int *fromlen)
{
    (void)buf;
    (void)from;
    (void)fromlen;
    ev("recvfrom %x size %d flags %x -> no data", (unsigned)s, len, flags);
    return -1;
}

/* ------------------------------------------------------------------ lookup */

typedef struct Stub {
    const char *dll;
    const char *name;
    unsigned ordinal;
    void *fn;
} Stub;

static const Stub g_stubs[] = {
    { "KERNEL32.dll", "QueryPerformanceCounter", 0, (void *)st_QueryPerformanceCounter },
    { "KERNEL32.dll", "QueryPerformanceFrequency", 0, (void *)st_QueryPerformanceFrequency },
    { "KERNEL32.dll", "GetSystemTimeAsFileTime", 0, (void *)st_GetSystemTimeAsFileTime },
    { "KERNEL32.dll", "GetProcessAffinityMask", 0, (void *)st_GetProcessAffinityMask },
    { "KERNEL32.dll", "SetProcessAffinityMask", 0, (void *)st_SetProcessAffinityMask },
    { "MSVCR100.dll", "_time64", 0, (void *)st_time64 },
    { "MSVCR100.dll", "_sleep", 0, (void *)st_sleep },
    { "MSVCR100.dll", "malloc", 0, (void *)st_malloc },
    { "MSVCR100.dll", "free", 0, (void *)st_free },
    { "MSVCR100.dll", "fopen", 0, (void *)st_fopen },
    { "MSVCR100.dll", "fclose", 0, (void *)st_fclose },
    { "MSVCR100.dll", "fread", 0, (void *)st_fread },
    { "MSVCR100.dll", "_chdir", 0, (void *)st_chdir },
    { "MSVCR100.dll", "_getcwd", 0, (void *)st_getcwd },
    { "MSVCR100.dll", "printf", 0, (void *)st_printf },
    { "WSOCK32.dll", NULL, 2, (void *)st_bind },
    { "WSOCK32.dll", NULL, 3, (void *)st_closesocket },
    { "WSOCK32.dll", NULL, 4, (void *)st_connect },
    { "WSOCK32.dll", NULL, 7, (void *)st_getsockopt },
    { "WSOCK32.dll", NULL, 12, (void *)st_ioctlsocket },
    { "WSOCK32.dll", NULL, 16, (void *)st_recv },
    { "WSOCK32.dll", NULL, 17, (void *)st_recvfrom },
    { "WSOCK32.dll", NULL, 18, (void *)st_select },
    { "WSOCK32.dll", NULL, 19, (void *)st_send },
    { "WSOCK32.dll", NULL, 20, (void *)st_sendto },
    { "WSOCK32.dll", NULL, 23, (void *)st_socket },
    { "WSOCK32.dll", NULL, 52, (void *)st_gethostbyname },
    { "WSOCK32.dll", NULL, 55, (void *)st_getservbyname },
    { "WSOCK32.dll", NULL, 151, (void *)st_WSAFDIsSet },
};

void *plat_lookup(const char *dll, const char *name, unsigned ordinal)
{
    size_t i;
    for (i = 0; i < sizeof g_stubs / sizeof g_stubs[0]; i++) {
        const Stub *s = &g_stubs[i];
        if (_stricmp(s->dll, dll) != 0)
            continue;
        if (name ? s->name && strcmp(s->name, name) == 0 : !s->name && s->ordinal == ordinal)
            return s->fn;
    }
    return NULL;
}

/* ------------------------------------------------------------------ control */

/* Delete a sandbox folder and all its content. Only a folder inside the sandbox root. */
static void remove_tree(const char *dir)
{
    char pat[MAX_PATH], sub[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    if (_strnicmp(dir, g_box[0], strlen(g_box[0]) - strlen("lifted\\")) != 0)
        return;
    snprintf(pat, MAX_PATH, "%s*", dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
            continue;
        snprintf(sub, MAX_PATH, "%s%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            strncat(sub, "\\", MAX_PATH - strlen(sub) - 1);
            remove_tree(sub);
            RemoveDirectoryA(sub);
        } else {
            DeleteFileA(sub);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static void full_dir(const char *in, char *out)
{
    size_t n;
    GetFullPathNameA(in, MAX_PATH - 1, out, NULL);
    n = strlen(out);
    if (n && out[n - 1] != '\\') {
        out[n] = '\\';
        out[n + 1] = '\0';
    }
}

int plat_test_init(const char *game_dir, const char *sandbox_dir)
{
    HMODULE crt = LoadLibraryA("MSVCR100.dll");
    char box[MAX_PATH];
    if (!crt)
        return 0;
    *(FARPROC *)&r_fopen = GetProcAddress(crt, "fopen");
    *(FARPROC *)&r_fclose = GetProcAddress(crt, "fclose");
    *(FARPROC *)&r_fread = GetProcAddress(crt, "fread");
    r_fwrite = (void *)GetProcAddress(crt, "fwrite");
    *(FARPROC *)&r_chdir = GetProcAddress(crt, "_chdir");
    *(FARPROC *)&r_getcwd = GetProcAddress(crt, "_getcwd");
    full_dir(game_dir, g_root);
    full_dir(sandbox_dir, box);
    CreateDirectoryA(box, NULL);
    snprintf(g_box[0], MAX_PATH, "%slifted\\", box);
    snprintf(g_box[1], MAX_PATH, "%soriginal\\", box);
    g_pool = VirtualAlloc(NULL, POOL_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    return r_fopen && r_fclose && r_fread && r_fwrite && r_chdir && r_getcwd && g_pool &&
           GetFileAttributesA(g_root) != INVALID_FILE_ATTRIBUTES;
}

void plat_test_patch_globals(void)
{
    if (!r_fwrite)
        return;  /* before plat_test_init */
    *(void **)(sr_base + 0x91022a4) = (void *)st_fread;
    *(void **)(sr_base + 0x13bf2d00) = r_fwrite;
}

void plat_test_begin(int run)
{
    Log *l = &g_log[run];
    l->len = 0;
    l->events = 0;
    l->full = 0;
    remove_tree(g_box[run]);
    memset(g_pool, 0, g_pool_dirty);
    g_pool_used = g_pool_dirty = 0;
    g_qpc = QPC_START;
    g_nsock = 0;
    g_read_bytes = 0;
    GetCurrentDirectoryA(MAX_PATH, g_saved_cwd);
    SetCurrentDirectoryA(g_root);
    g_run = run;
}

void plat_test_end(int run)
{
    int i;
    for (i = 0; i < MAX_FILES; i++)
        if (g_files[i].f)
            close_file(&g_files[i], "not closed:");
    if (g_read_bytes)
        ev("read %llu bytes", g_read_bytes);
    g_pool_len[run] = g_pool_used;
    if (run == 0) {
        if (g_pool_copy_len < g_pool_used) {
            free(g_pool_copy);
            g_pool_copy = malloc(g_pool_used);
            g_pool_copy_len = g_pool_copy ? g_pool_used : 0;
        }
        if (g_pool_copy)
            memcpy(g_pool_copy, g_pool, g_pool_used);
    }
    SetCurrentDirectoryA(g_saved_cwd);
    g_run = -1;
}

void plat_test_dump(void)
{
    int run;
    for (run = 0; run < 2; run++)
        printf("  environment log of the %s run (%d events):\n%.*s", run ? "original" : "lifted", g_log[run].events,
               (int)g_log[run].len, g_log[run].text ? g_log[run].text : "");
}

int plat_test_events(void)
{
    return g_log[0].events > g_log[1].events ? g_log[0].events : g_log[1].events;
}

/* One line of a log, without the newline, cut to fit. */
static void line_at(const Log *l, size_t at, char *out, size_t n)
{
    size_t k = 0;
    while (at < l->len && l->text[at] != '\n' && k + 1 < n)
        out[k++] = l->text[at++];
    out[k] = '\0';
}

int plat_test_same(char *why, size_t n)
{
    const Log *a = &g_log[0], *b = &g_log[1];
    size_t i, start = 0, k;
    char la[96], lb[96];
    for (i = 0; i < a->len && i < b->len && a->text[i] == b->text[i]; i++)
        if (a->text[i] == '\n')
            start = i + 1;
    if (i < a->len || i < b->len || a->events != b->events) {
        line_at(a, start, la, sizeof la);
        line_at(b, start, lb, sizeof lb);
        snprintf(why, n, "environment: lifted \"%s\", original \"%s\"", la[0] ? la : "(end)", lb[0] ? lb : "(end)");
        return 0;
    }
    if (g_pool_len[0] != g_pool_len[1]) {
        snprintf(why, n, "heap: lifted used %u bytes, original %u", (unsigned)g_pool_len[0], (unsigned)g_pool_len[1]);
        return 0;
    }
    for (k = 0; k < g_pool_len[1]; k++) {
        if (g_pool_copy[k] != g_pool[k]) {
            snprintf(why, n, "heap: block memory differs at +%x", (unsigned)k);
            return 0;
        }
    }
    return 1;
}
