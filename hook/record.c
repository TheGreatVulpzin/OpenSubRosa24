/* Record real inputs for the offline differential test (src/tests/harness.c).
 *
 * subvulpes.ini:
 *   record_args=1          the thunks of the lifted hooks call sv_record_args
 *   snapshot_after=60      seconds after start: write the .data section to sv_snapshot.bin
 *   record_imports=1       record the side-effect functions with their import calls (below)
 *
 * sv_args.txt, one line per recorded call (up to RECORD_MAX different calls per function):
 *   R <rva> <arg> <arg> ...
 * Each argument (one dword) has a kind:
 *   V:xxxxxxxx   a plain value
 *   M:xxxxxxxx   an address inside the 024d image, as an RVA (the harness relocates it)
 *   P:xxxxxxxx:hex...  a readable address: the value, then the 256 bytes it points to
 *                (old recordings: P:hex..., without the value)
 *
 * sv_snapshot.bin: header (magic, image base, .data RVA, .data size, x87 control word),
 * then each .data page that is not all zero: page index (uint32) + 4096 bytes.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"
#include "record.h"
#include "sr_runtime.h"
#include "verify.h"

#define RECORD_MAX 24
#define RECORD_FUNCS 1024
#define BLOB 256

static int g_record = -1;
static FILE *g_args;
static uint32_t g_seen[RECORD_FUNCS][RECORD_MAX];
static unsigned char g_count[RECORD_FUNCS];
static uint32_t g_image_size;

static int open_args(void);

static uint32_t hash_args(const uint32_t *a, int n)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < n; i++)
        h = (h ^ a[i]) * 16777619u;
    return h ? h : 1;
}

static int readable(const void *p, size_t n)
{
    MEMORY_BASIC_INFORMATION mi;
    /* The image of the 024d is always mapped. VirtualQuery on its 636 MB .data takes about
     * 1 ms (Windows measures the whole region), and building_load reads thousands of 4-byte
     * values: recording took 15 seconds per building. */
    if (g_image_size && n <= g_image_size && (uintptr_t)p >= sr_base &&
        (uintptr_t)p - sr_base <= g_image_size - n)  /* no overflow for p near 0xffffffff */
        return 1;
    /* The image of the 024d is always mapped. VirtualQuery on its 636 MB .data takes about
     * 1 ms (Windows measures the whole region), and building_load reads thousands of 4-byte
     * values: recording took 15 seconds per building. */
    if (g_image_size && n <= g_image_size && (uintptr_t)p >= sr_base &&
        (uintptr_t)p - sr_base <= g_image_size - n)  /* no overflow for p near 0xffffffff */
        return 1;
    if (!VirtualQuery(p, &mi, sizeof mi) || mi.State != MEM_COMMIT)
        return 0;
    if (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
        return 0;
    return (const char *)p + n <= (const char *)mi.BaseAddress + mi.RegionSize;
}

static void exe_path(char *out, size_t n, const char *file)
{
    char *slash;
    GetModuleFileNameA(NULL, out, (DWORD)n);
    slash = strrchr(out, '\\');
    if (slash)
        slash[1] = 0;
    strncat(out, file, n - strlen(out) - 1);
}

void sv_record_args(uint32_t rva, int index, const uint32_t *args, int ndw)
{
    uint32_t h;
    int i, k;
    if (g_record < 0)
        g_record = sv_config_int("record_args", 0) && open_args();
    if (!g_record || index < 0 || index >= RECORD_FUNCS || g_count[index] >= RECORD_MAX || sv_verify_busy())
        return;
    h = hash_args(args, ndw);
    for (k = 0; k < g_count[index]; k++)
        if (g_seen[index][k] == h)
            return;
    g_seen[index][g_count[index]++] = h;

    fprintf(g_args, "R %x", rva);
    for (i = 0; i < ndw; i++) {
        uint32_t v = args[i];
        if (v >= sr_base && v < sr_base + g_image_size) {
            fprintf(g_args, " M:%x", v - (uint32_t)sr_base);
        } else if (v >= 0x10000 && readable((void *)v, BLOB)) {
            const unsigned char *p = (const unsigned char *)v;
            /* The value too: a float such as 1.0 (0x3f800000) can look like a readable address. */
            fprintf(g_args, " P:%x:", v);
            for (k = 0; k < BLOB; k++)
                fprintf(g_args, "%02x", p[k]);
        } else {
            fprintf(g_args, " V:%x", v);
        }
    }
    fputc('\n', g_args);
    fflush(g_args);
}

/* ------------------------------------------------------------ record and replay
 *
 * subvulpes.ini: record_imports=1. The record-only thunks of the side-effect functions
 * (hook/hooks_record.c) open a scope, call the original and close the scope. While a scope
 * is open, the import wrappers (hook/record_imports.c) add one "E" line per import call.
 * When the scope closes, the whole call goes to sv_args.txt at once:
 *   R <rva> <args>          the arguments, as for the lifted functions
 *   E <import> <fields>     each import call, in order
 *   Z r:<eax>               the return value of the function
 * A scope that is open inside another one gives its events to both. */

#define SCOPE_MAX 8     /* recorded nesting depth */
#define RET_MAX 256     /* nesting depth of the thunks (return addresses) */
static size_t g_scope_cap = 8u << 20;  /* subvulpes.ini record_cap_mb */

typedef struct {
    int rec;            /* this call is recorded */
    int idx;            /* function slot, for the limit of RECORD_MAX calls */
    char *buf;
    size_t len, cap;
    int dropped;        /* the call grew over the cap: not written */
} Scope;

static __declspec(thread) uint32_t t_rets[RET_MAX];
static __declspec(thread) int t_depth;
static __declspec(thread) Scope t_scope[SCOPE_MAX];
static int g_rec_imports = -1;
static int g_rec_trace;  /* subvulpes.ini record_trace=1: log each begin and end */
static uint32_t g_rec_rva[RECORD_FUNCS];
static int g_rec_nfuncs;
static uint32_t g_rec_seen[RECORD_FUNCS][RECORD_MAX];
static unsigned char g_rec_count[RECORD_FUNCS];
static uint32_t g_rec_calls[RECORD_FUNCS];
#define REC_FIRST 4
static int g_rec_every = 50;  /* subvulpes.ini record_every */
static char g_line[(64u << 10) * 2 + 4096];
static size_t g_line_len;

static int open_args(void)
{
    char path[MAX_PATH];
    IMAGE_NT_HEADERS32 *nt;
    if (g_args)
        return 1;
    exe_path(path, sizeof path, "sv_args.txt");
    g_args = fopen(path, "a");
    nt = (IMAGE_NT_HEADERS32 *)(sr_base + ((IMAGE_DOS_HEADER *)sr_base)->e_lfanew);
    g_image_size = nt->OptionalHeader.SizeOfImage;
    return g_args != NULL;
}

static void scope_add(Scope *s, const char *p, size_t n)
{
    if (s->dropped)
        return;
    if (s->len + n > g_scope_cap) {
        s->dropped = 1;
        sv_log("record: a call is over %u MB and is not written", (unsigned)(g_scope_cap >> 20));
        return;
    }
    if (s->len + n + 1 > s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 4096;
        char *nb;
        while (cap < s->len + n + 1)
            cap *= 2;
        nb = (char *)realloc(s->buf, cap);
        if (!nb) {
            s->dropped = 1;
            return;
        }
        s->buf = nb;
        s->cap = cap;
    }
    memcpy(s->buf + s->len, p, n);
    s->len += n;
    s->buf[s->len] = 0;
}

static void scope_printf(Scope *s, const char *fmt, ...)
{
    char tmp[64];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0)
        scope_add(s, tmp, (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1);
}

static void scope_hex(Scope *s, const unsigned char *p, size_t n)
{
    static const char digits[] = "0123456789abcdef";
    char tmp[512];
    size_t i, k = 0;
    for (i = 0; i < n; i++) {
        tmp[k++] = digits[p[i] >> 4];
        tmp[k++] = digits[p[i] & 15];
        if (k == sizeof tmp) {
            scope_add(s, tmp, k);
            k = 0;
        }
    }
    scope_add(s, tmp, k);
}

static int func_slot(uint32_t rva)
{
    int i;
    for (i = 0; i < g_rec_nfuncs; i++)
        if (g_rec_rva[i] == rva)
            return i;
    if (g_rec_nfuncs == RECORD_FUNCS)
        return -1;
    g_rec_rva[g_rec_nfuncs] = rva;
    return g_rec_nfuncs++;
}

void sv_rec_begin(uint32_t rva, const uint32_t *args, int ndw)
{
    int d = t_depth, i;
    if (g_rec_imports < 0) {
        g_rec_imports = sv_config_int("record_imports", 0) && open_args();
        g_rec_trace = sv_config_int("record_trace", 0);
        g_rec_every = sv_config_int("record_every", 50);
        g_scope_cap = (size_t)sv_config_int("record_cap_mb", 8) << 20;
        if (g_rec_every < 1)
            g_rec_every = 1;
    }
    if (g_rec_trace)
        sv_log("rec begin %x depth %d", rva, t_depth);
    if (sv_config_int("record_trace", 0))
        sv_log("rec begin %x depth %d", rva, t_depth);
    if (d < RET_MAX)
        t_rets[d] = 0;
    t_depth++;
    if (d >= SCOPE_MAX)
        return;
    memset(&t_scope[d], 0, sizeof t_scope[d]);
    if (!g_rec_imports)
        return;
    t_scope[d].idx = func_slot(rva);
    if (t_scope[d].idx < 0 || g_rec_count[t_scope[d].idx] >= RECORD_MAX)
        return;
    /* The first REC_FIRST calls of a function are always recorded (the startup functions run
     * once). After that, one call in record_every, so the records of the tick functions
     * spread over the whole session instead of the first ticks. */
    g_rec_calls[t_scope[d].idx]++;
    if (g_rec_count[t_scope[d].idx] >= REC_FIRST && g_rec_calls[t_scope[d].idx] % g_rec_every != 0)
        return;
    t_scope[d].rec = 1;
    scope_printf(&t_scope[d], "R %x", rva);
    for (i = 0; i < ndw; i++) {
        uint32_t v = args[i];
        if (v >= sr_base && v < sr_base + g_image_size) {
            scope_printf(&t_scope[d], " M:%x", v - (uint32_t)sr_base);
        } else if (v >= 0x10000 && readable((void *)v, BLOB)) {
            scope_printf(&t_scope[d], " P:%x:", v);
            scope_hex(&t_scope[d], (const unsigned char *)v, BLOB);
        } else {
            scope_printf(&t_scope[d], " V:%x", v);
        }
    }
    scope_add(&t_scope[d], "\n", 1);
}

void sv_rec_push_ret(uint32_t ret)
{
    if (t_depth >= 1 && t_depth <= RET_MAX)
        t_rets[t_depth - 1] = ret;
    else
        sv_log("ERROR record: thunk nesting deeper than %d", RET_MAX);
}

uint32_t sv_rec_end(uint32_t eax)
{
    int d = --t_depth;
    if (g_rec_trace)
        sv_log("rec end depth %d", d);
    static int trace = -1;
    if (trace < 0)
        trace = sv_config_int("record_trace", 0);
    if (trace)
        sv_log("rec end depth %d", d);
    uint32_t ret = d < RET_MAX ? t_rets[d] : 0;
    if (d < SCOPE_MAX) {
        Scope *s = &t_scope[d];
        if (s->rec && !s->dropped) {
            uint32_t h = 2166136261u;
            size_t i;
            int k, dup = 0;
            scope_printf(s, "Z r:%x\n", eax);
            /* Keep calls that differ in their arguments or in their imports. */
            for (i = 0; i < s->len; i++)
                h = (h ^ (unsigned char)s->buf[i]) * 16777619u;
            for (k = 0; k < g_rec_count[s->idx]; k++)
                dup |= g_rec_seen[s->idx][k] == h;
            if (!dup && !s->dropped && g_rec_count[s->idx] < RECORD_MAX) {
                g_rec_seen[s->idx][g_rec_count[s->idx]++] = h;
                fwrite(s->buf, 1, s->len, g_args);
                fflush(g_args);
            }
        }
        free(s->buf);
        memset(s, 0, sizeof *s);
    }
    return ret;
}

int sv_rec_active(void)
{
    int d, n = t_depth < SCOPE_MAX ? t_depth : SCOPE_MAX;
    for (d = 0; d < n; d++)
        if (t_scope[d].rec && !t_scope[d].dropped)
            return 1;
    return 0;
}

void sv_ev_begin(const char *name)
{
    g_line_len = (size_t)snprintf(g_line, 64, "E %s", name);
}

static void line_add(const char *p, size_t n)
{
    if (g_line_len + n < sizeof g_line - 2) {
        memcpy(g_line + g_line_len, p, n);
        g_line_len += n;
    }
}

void sv_ev_v(uint32_t v)
{
    char tmp[16];
    line_add(tmp, (size_t)snprintf(tmp, sizeof tmp, " V:%x", v));
}

void sv_ev_r(uint32_t r)
{
    char tmp[16];
    line_add(tmp, (size_t)snprintf(tmp, sizeof tmp, " r:%x", r));
}

void sv_ev_bytes(char tag, const void *p, size_t n)
{
    static const char digits[] = "0123456789abcdef";
    const unsigned char *b = (const unsigned char *)p;
    char tmp[32];
    size_t i, max = (sizeof g_line - g_line_len - 64) / 2;
    if (!p || !readable(p, n ? n : 1)) {
        line_add(tmp, (size_t)snprintf(tmp, sizeof tmp, " %c:-", tag));
        return;
    }
    if (n > max) {
        /* Too large for one line: the length and a hash only. */
        uint32_t h = 2166136261u;
        for (i = 0; i < n; i++)
            h = (h ^ b[i]) * 16777619u;
        line_add(tmp, (size_t)snprintf(tmp, sizeof tmp, " %c#%x:%x", tag, (unsigned)n, h));
        return;
    }
    line_add(tmp, (size_t)snprintf(tmp, sizeof tmp, " %c:", tag));
    for (i = 0; i < n; i++) {
        g_line[g_line_len++] = digits[b[i] >> 4];
        g_line[g_line_len++] = digits[b[i] & 15];
    }
}

void sv_ev_hash(char tag, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    uint32_t h = 2166136261u;
    char tmp[32];
    size_t i;
    if (!p || !readable(p, n ? n : 1)) {
        line_add(tmp, (size_t)snprintf(tmp, sizeof tmp, " %c:-", tag));
        return;
    }
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 16777619u;
    line_add(tmp, (size_t)snprintf(tmp, sizeof tmp, " %c#%x:%x", tag, (unsigned)n, h));
}

void sv_ev_s(const char *s)
{
    sv_ev_bytes('s', s, s && readable(s, 1) ? strnlen(s, 260) : 0);
}

void sv_ev_end(void)
{
    int d, n = t_depth < SCOPE_MAX ? t_depth : SCOPE_MAX;
    g_line[g_line_len++] = '\n';
    for (d = 0; d < n; d++)
        if (t_scope[d].rec)
            scope_add(&t_scope[d], g_line, g_line_len);
}

void sv_snapshot_check(void)
{
    static int after = -1, done;
    static DWORD start;
    char path[MAX_PATH];
    const uint8_t *data;
    size_t size, page;
    uint32_t data_rva, base, magic = 0x50414e53; /* "SNAP" */
    unsigned short cw;
    FILE *f;

    if (done)
        return;
    if (after < 0) {
        after = sv_config_int("snapshot_after", 0);
        start = GetTickCount();
        if (after <= 0) {
            done = 1;
            return;
        }
    }
    if (GetTickCount() - start < (DWORD)after * 1000u || sv_verify_busy())
        return;
    done = 1;
    if (!sv_verify_data(&data, &size))
        return;
    exe_path(path, sizeof path, "sv_snapshot.bin");
    f = fopen(path, "wb");
    if (!f)
        return;
    __asm fnstcw cw
    base = (uint32_t)sr_base;
    data_rva = (uint32_t)((uintptr_t)data - sr_base);
    fwrite(&magic, 4, 1, f);
    fwrite(&base, 4, 1, f);
    fwrite(&data_rva, 4, 1, f);
    fwrite(&size, 4, 1, f);
    fwrite(&cw, 2, 1, f);
    for (page = 0; page < size / 4096; page++) {
        const uint32_t *w = (const uint32_t *)(data + page * 4096);
        int k, nonzero = 0;
        for (k = 0; k < 1024 && !nonzero; k++)
            nonzero = w[k] != 0;
        if (nonzero) {
            uint32_t idx = (uint32_t)page;
            fwrite(&idx, 4, 1, f);
            fwrite(w, 4096, 1, f);
        }
    }
    {
        /* The game also keeps pointers to heap memory in .data. Write every committed
         * private read/write region (heaps, malloc blocks), with its address: the harness
         * maps them at the same addresses. End of .data pages: index 0xffffffff. */
        uint32_t end = 0xffffffffu, total = 0;
        uintptr_t a = 0x10000;
        MEMORY_BASIC_INFORMATION mi;
        fwrite(&end, 4, 1, f);
        while (a < 0x7fff0000 && VirtualQuery((void *)a, &mi, sizeof mi)) {
            if (mi.State == MEM_COMMIT && mi.Type == MEM_PRIVATE && mi.Protect == PAGE_READWRITE) {
                uint32_t rbase = (uint32_t)(uintptr_t)mi.BaseAddress, rsize = (uint32_t)mi.RegionSize;
                fwrite(&rbase, 4, 1, f);
                fwrite(&rsize, 4, 1, f);
                fwrite(mi.BaseAddress, rsize, 1, f);
                total += rsize;
            }
            a = (uintptr_t)mi.BaseAddress + mi.RegionSize;
        }
        sv_log("snapshot: %u KB of heap regions", total / 1024);
    }
    fclose(f);
    sv_log("snapshot of .data written to sv_snapshot.bin");
}
