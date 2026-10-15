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

#define RECORD_MAX 24      /* calls of a tested function (and of a record-only hook) */
#define RECORD_FUNCS 1024
#define BLOB 256

static int g_record = -1;
static FILE *g_args;
static volatile long g_written_calls;  /* calls in sv_args.txt (console title) */
static uint32_t *g_seen[RECORD_FUNCS];  /* hashes of the recorded calls of each function */
static int g_count[RECORD_FUNCS], g_cap[RECORD_FUNCS];
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

static int g_snap_done;  /* sv_snapshot_check finished (or snapshot_after=0) */

/* ------------------------------------------------------------ what to record
 *
 * Each hook costs time on every call: the thunk (FXSAVE, the recorder) runs before the
 * original. With 277 hooks the server fell from 60 to 15-25 ticks per second. With
 * original_only=1 (a recording session) only some functions keep their hook:
 *
 *   untested   record_focus.txt, "untested": no recording has a call yet. Always hooked,
 *              every call with new arguments is recorded, no limit.
 *   diff       record_focus.txt, "diff": the harness finds a difference. Always hooked
 *              until record_diff_max calls (default 32), then rotation.
 *   tested     all other functions: they have calls in older recordings. No hook, except
 *              in the rotation: every record_rotate_sec seconds (default 180), the next
 *              record_rotate_count functions (default 15) get their hook for
 *              record_rotate_window seconds (default 3) and can record record_rotate_extra
 *              (default 8) new calls in the whole session.
 *
 * sv_args.txt stops growing at record_total_mb (default 200).
 * Hooks are removed and put back only on the game thread (inside a hook): the rotation
 * runs from sv_rec_begin, which the record-only hook of servercycle calls every tick.
 */

enum { CLS_TESTED, CLS_UNTESTED, CLS_DIFF };

static int rotation_runs(void);  /* record_imports=1: sv_rec_begin runs every tick */

typedef struct Focus {
    uint32_t rva;
    int cls;
} Focus;

static Focus *g_focus;
static int g_nfocus, g_focus_loaded;
static int g_diff_max = 32, g_rot_sec = 180, g_rot_count = 15, g_rot_window = 3, g_rot_extra = 8;
static long long g_total_max = 200ll << 20;
static int g_extra[RECORD_FUNCS];       /* rotation calls recorded per function */
static uint32_t g_tested[RECORD_FUNCS];  /* tested functions seen (rotation list) */
static int g_ntested, g_rot_next, g_rot_active[64], g_nrot_active;
static DWORD g_rot_last, g_rot_end;

static int cmp_focus(const void *a, const void *b)
{
    uint32_t x = ((const Focus *)a)->rva, y = ((const Focus *)b)->rva;
    return x < y ? -1 : x > y;
}

/* record_focus.txt next to the server: one function per line, "0x1d4d0 untested name". */
static void load_focus(void)
{
    char path[MAX_PATH], line[256];
    FILE *f;
    int cap = 0;
    g_focus_loaded = 1;
    g_diff_max = sv_config_int("record_diff_max", 32);
    g_rot_sec = sv_config_int("record_rotate_sec", 180);
    g_rot_count = sv_config_int("record_rotate_count", 15);
    g_rot_window = sv_config_int("record_rotate_window", 3);
    g_rot_extra = sv_config_int("record_rotate_extra", 8);
    g_total_max = (long long)sv_config_int("record_total_mb", 200) << 20;
    if (g_rot_count > 64)
        g_rot_count = 64;
    exe_path(path, sizeof path, "record_focus.txt");
    f = fopen(path, "r");
    if (!f) {
        sv_log("record: no record_focus.txt, all functions are in the rotation");
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char cls[32];
        unsigned rva;
        if (line[0] == '#' || sscanf(line, "%x %31s", &rva, cls) != 2)
            continue;
        if (g_nfocus == cap) {
            cap = cap ? cap * 2 : 64;
            g_focus = realloc(g_focus, cap * sizeof(Focus));
        }
        g_focus[g_nfocus].rva = rva;
        g_focus[g_nfocus].cls = strcmp(cls, "diff") == 0 ? CLS_DIFF : CLS_UNTESTED;
        g_nfocus++;
    }
    fclose(f);
    qsort(g_focus, (size_t)g_nfocus, sizeof(Focus), cmp_focus);
    sv_log("record: %d functions in record_focus.txt", g_nfocus);
}

static int focus_class(uint32_t rva)
{
    Focus key, *found;
    key.rva = rva;
    found = g_nfocus ? bsearch(&key, g_focus, (size_t)g_nfocus, sizeof(Focus), cmp_focus) : NULL;
    return found ? found->cls : CLS_TESTED;
}

static int in_rotation(uint32_t rva)
{
    int i;
    for (i = 0; i < g_nrot_active; i++)
        if (g_tested[g_rot_active[i]] == rva)
            return 1;
    return 0;
}

static int args_full(void)
{
    return g_total_max > 0 && g_args && _ftelli64(g_args) >= g_total_max;
}

/* Remove a hook in a recording session. */
static void drop_hook(uint32_t rva)
{
    if (sv_original_only() && sv_unhook_rva(rva) && sv_unhooked_count() % 50 == 1)
        sv_log("record: %d hooks removed", sv_unhooked_count());
}

/* The rotation of the tested functions. Game thread (sv_rec_begin). */
static void rotate(void)
{
    DWORD now = GetTickCount();
    int i;
    if (!sv_original_only() || !g_ntested)
        return;
    if (g_nrot_active && (int)(now - g_rot_end) >= 0) {
        for (i = 0; i < g_nrot_active; i++)
            sv_unhook_rva(g_tested[g_rot_active[i]]);
        g_nrot_active = 0;
    }
    if (g_nrot_active || now - g_rot_last < (DWORD)g_rot_sec * 1000u)
        return;
    g_rot_last = now;
    for (i = 0; i < g_ntested && g_nrot_active < g_rot_count; i++) {
        int k = (g_rot_next + i) % g_ntested;
        if (sv_rehook_rva(g_tested[k]))
            g_rot_active[g_nrot_active++] = k;
    }
    g_rot_next = (g_rot_next + i) % g_ntested;
    g_rot_end = now + (DWORD)g_rot_window * 1000u;
}

void sv_record_args(uint32_t rva, int index, const uint32_t *args, int ndw)
{
    uint32_t h;
    int i, k, cls, limit;
    if (g_record < 0)
        g_record = sv_config_int("record_args", 0) && open_args();
    if (!g_record || index < 0 || index >= RECORD_FUNCS || sv_verify_busy())
        return;
    if (!g_focus_loaded)
        load_focus();
    cls = focus_class(rva);
    /* Verification sessions (original_only=0) keep the old rule: RECORD_MAX calls, no hook
     * removal (the verifier needs the hooks). */
    if (!sv_original_only())
        limit = RECORD_MAX;
    else if (cls == CLS_UNTESTED)
        limit = 1 << 30;
    else if (cls == CLS_DIFF && g_count[index] < g_diff_max)
        limit = g_diff_max;
    else if (in_rotation(rva) && g_extra[index] < g_rot_extra)
        limit = g_count[index] + 1;
    else
        limit = 0;
    if (sv_original_only() && cls != CLS_UNTESTED && limit == 0) {
        /* A tested function (or a diff function with its calls): no hook outside the
         * rotation. Remember it for the rotation. */
        for (k = 0; k < g_ntested && g_tested[k] != rva; k++)
            ;
        if (k == g_ntested && g_ntested < RECORD_FUNCS)
            g_tested[g_ntested++] = rva;
        /* The hooks take the snapshot (sv_note_call) unless the rotation runs: without
         * record_imports, keep them until the snapshot is written. */
        if (g_snap_done || rotation_runs())
            drop_hook(rva);
        return;
    }
    if (g_count[index] >= limit || args_full())
        return;
    h = hash_args(args, ndw);
    for (k = 0; k < g_count[index]; k++)
        if (g_seen[index][k] == h)
            return;
    if (g_count[index] == g_cap[index]) {
        int cap = g_cap[index] ? g_cap[index] * 2 : 32;
        uint32_t *seen = realloc(g_seen[index], (size_t)cap * sizeof(uint32_t));
        if (!seen)
            return;
        g_seen[index] = seen;
        g_cap[index] = cap;
    }
    g_seen[index][g_count[index]++] = h;
    if (cls != CLS_UNTESTED && in_rotation(rva))
        g_extra[index]++;

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
    g_written_calls++;
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

static int rotation_runs(void)
{
    return g_rec_imports > 0;
}

/* Calls of a record-only (side-effect) function: more for a diff or untested function. */
static int rec_limit(uint32_t rva)
{
    int cls = focus_class(rva);
    return cls == CLS_DIFF ? (g_diff_max < 250 ? g_diff_max : 250) : cls == CLS_UNTESTED ? 250 : RECORD_MAX;
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
    if (d < RET_MAX)
        t_rets[d] = 0;
    t_depth++;
    if (d >= SCOPE_MAX)
        return;
    memset(&t_scope[d], 0, sizeof t_scope[d]);
    if (!g_rec_imports)
        return;
    if (d == 0) {
        /* Once per outer call on the game thread: the rotation of the hooks and the
         * snapshot (the lifted hooks are mostly removed, so sv_note_call is rare). */
        if (!g_focus_loaded)
            load_focus();
        rotate();
        sv_snapshot_check();
    }
    t_scope[d].idx = func_slot(rva);
    if (t_scope[d].idx < 0 || g_rec_count[t_scope[d].idx] >= rec_limit(rva))
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
    uint32_t ret;
    if (g_rec_trace)
        sv_log("rec end depth %d", d);
    ret = d < RET_MAX ? t_rets[d] : 0;
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
            if (!dup && !s->dropped && g_rec_count[s->idx] < rec_limit(g_rec_rva[s->idx]) && !args_full()) {
                g_rec_seen[s->idx][g_rec_count[s->idx]++] = h;
                fwrite(s->buf, 1, s->len, g_args);
                fflush(g_args);
                g_written_calls++;
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

/* The snapshot is a frozen moment of the game: the copy into memory runs on the game thread
 * (about 0.1 s for 230 MB), the slow write to the disk runs on its own thread. Writing on
 * the game thread froze the server for seconds, and then it ran extra ticks to catch up. */
typedef struct SnapBuf {
    uint8_t *data;
    size_t len;
} SnapBuf;

static void snap_put(SnapBuf *b, const void *p, size_t n)
{
    memcpy(b->data + b->len, p, n);
    b->len += n;
}

static DWORD WINAPI snapshot_writer(LPVOID arg)
{
    SnapBuf *b = arg;
    char path[MAX_PATH];
    HANDLE f;
    size_t done = 0;
    exe_path(path, sizeof path, "sv_snapshot.bin");
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    /* WriteFile in 16 MB parts: one call with the whole buffer can fail. */
    while (f != INVALID_HANDLE_VALUE && done < b->len) {
        DWORD part = (DWORD)(b->len - done < (16u << 20) ? b->len - done : (16u << 20)), wrote = 0;
        if (!WriteFile(f, b->data + done, part, &wrote, NULL) || wrote == 0)
            break;
        done += wrote;
    }
    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);
    if (done == b->len) {
        sv_log("snapshot of .data written to sv_snapshot.bin (%u MB)", (unsigned)(b->len >> 20));
        sv_console_event(0, "snapshot saved: sv_snapshot.bin");
    } else {
        sv_log("ERROR snapshot: wrote %u of %u MB of sv_snapshot.bin (error %lu)", (unsigned)(done >> 20),
               (unsigned)(b->len >> 20), GetLastError());
    }
    VirtualFree(b->data, 0, MEM_RELEASE);
    free(b);
    return 0;
}

void sv_snapshot_check(void)
{
    static int after = -1, done;
    static DWORD start;
    const uint8_t *data;
    size_t size, page, npages, total;
    uint32_t data_rva, base, magic = 0x50414e53; /* "SNAP" */
    uint32_t end = 0xffffffffu, heap = 0;
    unsigned short cw;
    uint8_t *used;
    MEMORY_BASIC_INFORMATION *regions = NULL;
    int nregions = 0, cap = 0, r;
    uintptr_t a = 0x10000;
    MEMORY_BASIC_INFORMATION mi;
    SnapBuf *b;
    HANDLE th;
    DWORD t0;

    if (done)
        return;
    if (after < 0) {
        after = sv_config_int("snapshot_after", 0);
        start = GetTickCount();
        if (after <= 0) {
            done = g_snap_done = 1;
            return;
        }
    }
    if (GetTickCount() - start < (DWORD)after * 1000u || sv_verify_busy())
        return;
    done = g_snap_done = 1;
    if (!sv_verify_data(&data, &size))
        return;
    t0 = GetTickCount();
    __asm fnstcw cw
    base = (uint32_t)sr_base;
    data_rva = (uint32_t)((uintptr_t)data - sr_base);

    /* Pass 1: the .data pages that are not all zero, and the heap regions. The game also
     * keeps pointers to heap memory in .data: every committed private read/write region
     * goes into the file with its address (the harness maps it at the same address). */
    npages = size / 4096;
    used = calloc(npages, 1);
    if (!used)
        return;
    total = 18;  /* header */
    for (page = 0; page < npages; page++) {
        const uint32_t *w = (const uint32_t *)(data + page * 4096);
        int k;
        for (k = 0; k < 1024 && !used[page]; k++)
            used[page] = w[k] != 0;
        if (used[page])
            total += 4 + 4096;
    }
    total += 4;  /* end of the .data pages */
    while (a < 0x7fff0000 && VirtualQuery((void *)a, &mi, sizeof mi)) {
        if (mi.State == MEM_COMMIT && mi.Type == MEM_PRIVATE && mi.Protect == PAGE_READWRITE &&
            (const uint8_t *)mi.BaseAddress != used) {
            if (nregions == cap) {
                cap = cap ? cap * 2 : 64;
                regions = realloc(regions, (size_t)cap * sizeof mi);
            }
            regions[nregions++] = mi;
            total += 8 + mi.RegionSize;
        }
        a = (uintptr_t)mi.BaseAddress + mi.RegionSize;
    }

    /* Pass 2: copy. The buffer is allocated after the region list, so it is not in it. */
    b = malloc(sizeof *b);
    if (b)
        b->data = VirtualAlloc(NULL, total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!b || !b->data) {
        sv_log("ERROR snapshot: no memory for a %u MB copy", (unsigned)(total >> 20));
        free(b);
        free(used);
        free(regions);
        return;
    }
    b->len = 0;
    snap_put(b, &magic, 4);
    snap_put(b, &base, 4);
    snap_put(b, &data_rva, 4);
    snap_put(b, &size, 4);
    snap_put(b, &cw, 2);
    for (page = 0; page < npages; page++) {
        if (used[page]) {
            uint32_t idx = (uint32_t)page;
            snap_put(b, &idx, 4);
            snap_put(b, data + page * 4096, 4096);
        }
    }
    snap_put(b, &end, 4);
    for (r = 0; r < nregions; r++) {
        uint32_t rbase = (uint32_t)(uintptr_t)regions[r].BaseAddress, rsize = (uint32_t)regions[r].RegionSize;
        snap_put(b, &rbase, 4);
        snap_put(b, &rsize, 4);
        snap_put(b, regions[r].BaseAddress, rsize);
        heap += rsize;
    }
    free(used);
    free(regions);
    sv_log("snapshot: copied in %lu ms (%u KB of heap regions), writing on a thread", GetTickCount() - t0,
           heap / 1024);
    th = CreateThread(NULL, 0, snapshot_writer, b, 0, NULL);
    if (th)
        CloseHandle(th);
    else
        snapshot_writer(b);
}

void sv_record_stats(int *calls, long long *bytes)
{
    *calls = (int)g_written_calls;
    *bytes = g_args ? _ftelli64(g_args) : 0;
}
