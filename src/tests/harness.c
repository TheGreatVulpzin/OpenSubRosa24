/* Offline differential test of the lifted functions.
 *
 * Usage: harness <subrosadedicated024d.exe> <sv_snapshot.bin> <sv_args.txt> [filter]
 *
 * 1. Load the 024d as a library (its code runs, its entry point does not).
 * 2. Fill its .data section from a snapshot of a running server (hook/record.c).
 * 3. For each recorded call (sv_args.txt): run the lifted version, undo its writes, run the
 *    original, and compare the .data writes (hook/verify.c), the return value and the
 *    memory that pointer arguments point to.
 *
 * No server, no players, no hooks: the lifted code calls the original game functions.
 * A recorded call where the original itself crashes (for example a pointer into the heap
 * of the server process) is skipped.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"
#include "lifted_table.h"
#include "sr_runtime.h"
#include "verify.h"

void lf_x87_init(void);

#define MAX_ARGS 64
#define BLOB 256
#define BUF 4096

typedef struct Snapshot {
    uint32_t base, data_rva, size;
    unsigned short cw;
    uint32_t npages;
    uint32_t *index;
    uint8_t *pages;
} Snapshot;

typedef struct Call {
    uint32_t rva;
    int n;
    char kind[MAX_ARGS];          /* 'V', 'M' or 'P' */
    uint32_t value[MAX_ARGS];
    uint8_t *blob[MAX_ARGS];      /* 'P': BLOB bytes */
    char has_value[MAX_ARGS];     /* 'P': the value is known (recordings after the fix) */
} Call;

static HMODULE g_mod;
static Snapshot g_snap;
static uint32_t g_image_size;
static uint8_t *g_buf[MAX_ARGS];      /* memory for the 'P' arguments */
static uint8_t g_after_new[MAX_ARGS][BUF];

/* ------------------------------------------------------------------ snapshot */

static int load_snapshot(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint32_t magic, idx;
    uint32_t cap = 1024;
    if (!f)
        return 0;
    if (fread(&magic, 4, 1, f) != 1 || magic != 0x50414e53 || fread(&g_snap.base, 4, 1, f) != 1 ||
        fread(&g_snap.data_rva, 4, 1, f) != 1 || fread(&g_snap.size, 4, 1, f) != 1 ||
        fread(&g_snap.cw, 2, 1, f) != 1) {
        fclose(f);
        return 0;
    }
    g_snap.index = malloc(cap * 4);
    g_snap.pages = malloc((size_t)cap * 4096);
    while (fread(&idx, 4, 1, f) == 1 && idx != 0xffffffffu) {
        if (g_snap.npages == cap) {
            cap *= 2;
            g_snap.index = realloc(g_snap.index, cap * 4);
            g_snap.pages = realloc(g_snap.pages, (size_t)cap * 4096);
            if (!g_snap.index || !g_snap.pages)
                return 0;
        }
        g_snap.index[g_snap.npages] = idx;
        if (fread(g_snap.pages + (size_t)g_snap.npages * 4096, 4096, 1, f) != 1)
            break;
        g_snap.npages++;
    }
    fclose(f);
    return 1;
}

/* Heap regions of the server (after the .data pages in the snapshot). They must go to the
 * same addresses, before the harness itself uses the memory: map_regions runs first. */
typedef struct Region {
    uint32_t base, size;
    long offset;  /* content in the snapshot file */
    int mapped;
} Region;

static Region *g_regions;
static int g_nregions, g_mapped;
static char g_snap_path[MAX_PATH];

static void map_regions(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint32_t idx, hdr[2];
    int cap = 256;
    if (!f)
        return;
    strncpy(g_snap_path, path, MAX_PATH - 1);
    fseek(f, 18, SEEK_SET);
    while (fread(&idx, 4, 1, f) == 1 && idx != 0xffffffffu)
        fseek(f, 4096, SEEK_CUR);
    g_regions = malloc(cap * sizeof(Region));
    while (fread(hdr, 4, 2, f) == 2) {
        Region *r;
        if (g_nregions == cap) {
            cap *= 2;
            g_regions = realloc(g_regions, cap * sizeof(Region));
        }
        r = &g_regions[g_nregions++];
        r->base = hdr[0];
        r->size = hdr[1];
        r->offset = ftell(f);
        r->mapped = VirtualAlloc((void *)(uintptr_t)r->base, r->size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) != NULL;
        g_mapped += r->mapped;
        fseek(f, (long)r->size, SEEK_CUR);
    }
    fclose(f);
}

static void apply_regions(void)
{
    FILE *f = fopen(g_snap_path, "rb");
    int i;
    if (!f)
        return;
    for (i = 0; i < g_nregions; i++) {
        if (!g_regions[i].mapped)
            continue;
        fseek(f, g_regions[i].offset, SEEK_SET);
        if (fread((void *)(uintptr_t)g_regions[i].base, g_regions[i].size, 1, f) != 1)
            g_regions[i].mapped = 0;
    }
    fclose(f);
}

/* Put the snapshot into .data. Pointers into the image move with the load address: each
 * dword in the old image range gets the relocation delta (a small risk for an int that
 * looks like an address). */
static void apply_snapshot(void)
{
    uint8_t *data = (uint8_t *)(sr_base + g_snap.data_rva);
    uint32_t delta = (uint32_t)sr_base - g_snap.base;
    uint32_t i;
    DWORD old;
    VirtualProtect(data, g_snap.size, PAGE_READWRITE, &old);
    memset(data, 0, g_snap.size);
    for (i = 0; i < g_snap.npages; i++) {
        uint32_t *w = (uint32_t *)(data + (size_t)g_snap.index[i] * 4096);
        int k;
        memcpy(w, g_snap.pages + (size_t)i * 4096, 4096);
        if (delta)
            for (k = 0; k < 1024; k++)
                if (w[k] >= g_snap.base && w[k] < g_snap.base + g_image_size)
                    w[k] += delta;
    }
    apply_regions();
}

/* The loader does not resolve the imports of an .exe that LoadLibrary loads. Fill the
 * import address table here: by name, or by ordinal (the WinSock imports of the 024d). */
static int resolve_imports(void)
{
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(sr_base + ((IMAGE_DOS_HEADER *)sr_base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(sr_base + dir.VirtualAddress);
    int count = 0;
    for (; d->Name; d++) {
        const char *dll = (const char *)(sr_base + d->Name);
        HMODULE m = LoadLibraryA(dll);
        IMAGE_THUNK_DATA32 *names = (IMAGE_THUNK_DATA32 *)(sr_base + (d->OriginalFirstThunk ? d->OriginalFirstThunk
                                                                                            : d->FirstThunk));
        IMAGE_THUNK_DATA32 *iat = (IMAGE_THUNK_DATA32 *)(sr_base + d->FirstThunk);
        DWORD old;
        if (!m) {
            fprintf(stderr, "cannot load %s\n", dll);
            return 0;
        }
        for (; names->u1.AddressOfData; names++, iat++) {
            FARPROC fn;
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))
                fn = GetProcAddress(m, (LPCSTR)(uintptr_t)IMAGE_ORDINAL32(names->u1.Ordinal));
            else
                fn = GetProcAddress(m, ((IMAGE_IMPORT_BY_NAME *)(sr_base + names->u1.AddressOfData))->Name);
            VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old);
            iat->u1.Function = (DWORD)(uintptr_t)fn;
            VirtualProtect(&iat->u1.Function, 4, old, &old);
            count++;
        }
    }
    return count;
}

/* ------------------------------------------------------------------ recorded calls */

static Call *g_calls;
static int g_ncalls;

static int hexval(char c)
{
    return c <= '9' ? c - '0' : (c | 32) - 'a' + 10;
}

static int load_calls(const char *path)
{
    FILE *f = fopen(path, "r");
    static char line[MAX_ARGS * (BLOB * 2 + 4) + 64];
    int cap = 1024;
    if (!f)
        return 0;
    g_calls = calloc(cap, sizeof(Call));
    while (fgets(line, sizeof line, f)) {
        Call *c;
        char *p = line, *tok;
        if (line[0] != 'R')
            continue;
        if (g_ncalls == cap) {
            cap *= 2;
            g_calls = realloc(g_calls, cap * sizeof(Call));
            memset(g_calls + g_ncalls, 0, (cap - g_ncalls) * sizeof(Call));
        }
        c = &g_calls[g_ncalls];
        strtok(p, " \r\n");
        tok = strtok(NULL, " \r\n");
        if (!tok)
            continue;
        c->rva = strtoul(tok, NULL, 16);
        while ((tok = strtok(NULL, " \r\n")) && c->n < MAX_ARGS) {
            c->kind[c->n] = tok[0];
            if (tok[0] == 'P') {
                int k;
                const char *hex = tok + 2, *colon = strchr(tok + 2, ':');
                c->has_value[c->n] = colon != NULL;  /* new format: P:value:blob */
                if (colon) {
                    c->value[c->n] = strtoul(tok + 2, NULL, 16);
                    hex = colon + 1;
                }
                c->blob[c->n] = malloc(BLOB);
                for (k = 0; k < BLOB && hex[2 * k] && hex[2 * k + 1]; k++)
                    c->blob[c->n][k] = (uint8_t)(hexval(hex[2 * k]) * 16 + hexval(hex[2 * k + 1]));
            } else {
                c->value[c->n] = strtoul(tok + 2, NULL, 16);
            }
            c->n++;
        }
        g_ncalls++;
    }
    fclose(f);
    return 1;
}

/* 0 when a value argument was recorded as a pointer without its value (old recordings). */
static int args_usable(const Call *c, const char *kinds)
{
    int i;
    for (i = 0; i < c->n; i++)
        if (c->kind[i] == 'P' && kinds[i] != 'p' && !c->has_value[i])
            return 0;
    return 1;
}

/* Arguments of a call: values, relocated image addresses, and fresh copies of the blobs
 * for pointer parameters. A value parameter that looked like an address gets its value. */
static void build_args(const Call *c, const char *kinds, uint32_t *a)
{
    int i;
    for (i = 0; i < c->n; i++) {
        if (c->kind[i] == 'M') {
            a[i] = (uint32_t)sr_base + c->value[i];
        } else if (c->kind[i] == 'P' && kinds[i] != 'p') {
            a[i] = c->value[i];
        } else if (c->kind[i] == 'P') {
            memset(g_buf[i], 0, BUF);
            memcpy(g_buf[i] + BUF / 2, c->blob[i], BLOB);
            a[i] = (uint32_t)(uintptr_t)(g_buf[i] + BUF / 2);
        } else {
            a[i] = c->value[i];
        }
    }
}

/* ------------------------------------------------------------------ calls */

/* cdecl call with n dwords of arguments. Returns EAX, EDX and ST0 (for LF_RET_FLOAT). */
static void invoke(void *fn, const uint32_t *args, int n, int kind, uint32_t *r_eax, uint32_t *r_edx, double *r_f)
{
    uint32_t ra = 0, rd = 0;
    double rf = 0;
    int n4 = n * 4;
    unsigned short cw = g_snap.cw;
    __asm {
        mov  ebx, esp
        sub  esp, n4
        and  esp, -16
        mov  esi, args
        xor  edx, edx
    copy_loop:
        cmp  edx, n4
        jae  copy_done
        mov  eax, [esi + edx]
        mov  [esp + edx], eax
        add  edx, 4
        jmp  copy_loop
    copy_done:
        fninit
        fldcw cw
        call fn
        mov  ra, eax
        mov  rd, edx
        mov  esp, ebx
        cmp  kind, 3        ; LF_RET_FLOAT
        jne  no_float
        fstp rf
    no_float:
        fninit
    }
    *r_eax = ra;
    *r_edx = rd;
    *r_f = rf;
}

static char g_crash[128];  /* the last exception: code, place and data address */

static int crash_filter(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    uintptr_t at = (uintptr_t)er->ExceptionAddress;
    uintptr_t addr = er->NumberParameters >= 2 ? er->ExceptionInformation[1] : 0;
    if (at >= sr_base && at < sr_base + g_image_size)
        snprintf(g_crash, sizeof g_crash, "exception %08lx at 024d+%x, address %08x", er->ExceptionCode,
                 (unsigned)(at - sr_base), (unsigned)addr);
    else
        snprintf(g_crash, sizeof g_crash, "exception %08lx at %p, address %08x", er->ExceptionCode, (void *)at,
                 (unsigned)addr);
    return EXCEPTION_EXECUTE_HANDLER;
}

static int safe_invoke(void *fn, const uint32_t *args, int n, int kind, uint32_t *r_eax, uint32_t *r_edx,
                       double *r_f)
{
    __try {
        invoke(fn, args, n, kind, r_eax, r_edx, r_f);
    } __except (crash_filter(GetExceptionInformation())) {
        return 0;
    }
    return 1;
}

/* Memory behind a pointer argument: the same, word by word, with the float tolerance of the
 * verifier (a float output can differ in the last bits, see docs/lift.md "Float precision"). */
static int same_memory(const uint8_t *a, const uint8_t *b)
{
    int i;
    for (i = 0; i < BUF; i += 4) {
        float x, y;
        if (memcmp(a + i, b + i, 4) == 0)
            continue;
        memcpy(&x, a + i, 4);
        memcpy(&y, b + i, 4);
        if (!sv_same_float(x, y))
            return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ main */

int main(int argc, char **argv)
{
    int i, j, total_ok = 0, total_diff = 0, total_skip = 0, total_crash = 0, tested = 0;
    const char *filter = argc > 4 ? argv[4] : NULL;
    IMAGE_NT_HEADERS32 *nt;

    if (argc < 4) {
        fprintf(stderr, "usage: harness <024d.exe> <sv_snapshot.bin> <sv_args.txt> [name filter]\n");
        return 2;
    }
    map_regions(argv[2]);
    g_mod = LoadLibraryExA(argv[1], NULL, 0);
    if (!g_mod) {
        fprintf(stderr, "cannot load %s (error %lu)\n", argv[1], GetLastError());
        return 1;
    }
    sr_base = (uintptr_t)g_mod;
    nt = (IMAGE_NT_HEADERS32 *)(sr_base + ((IMAGE_DOS_HEADER *)sr_base)->e_lfanew);
    g_image_size = nt->OptionalHeader.SizeOfImage;
    if (!load_snapshot(argv[2]) || !load_calls(argv[3])) {
        fprintf(stderr, "cannot read the snapshot or the recorded calls\n");
        return 1;
    }
    for (i = 0; i < MAX_ARGS; i++)
        g_buf[i] = VirtualAlloc(NULL, BUF, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    apply_snapshot();
    printf("imports resolved: %d\n", resolve_imports());
    lf_x87_init();
    if (!sv_verify_init()) {
        fprintf(stderr, "verifier: no .data section\n");
        return 1;
    }
    sv_verify_ignore(0x14043e0, 0x5800);  /* profiler timers (RDTSC), see hooks_024d.c */
    printf("024d at %p, snapshot base %08x, %u pages, %d recorded calls\n", (void *)g_mod, g_snap.base,
           g_snap.npages, g_ncalls);
    printf("heap regions: %d of %d mapped at their server address\n", g_mapped, g_nregions);
    if ((uint32_t)sr_base != g_snap.base)
        printf("WARNING: the 024d did not load at the snapshot base. Pointers in the snapshot are guessed.\n"
               "Use the same copy from tools/re/fixed_base.py for the server and the harness.\n");

    for (i = 0; i < lf_table_count; i++) {
        const LfEntry *e = &lf_table[i];
        SvVerify v;
        int ok = 0, diff = 0, skip = 0, crash = 0, ncalls = 0;
        char first[160] = "";
        if (filter && !strstr(e->name, filter))
            continue;
        memset(&v, 0, sizeof v);
        v.name = e->name;
        v.every = 1;
        for (j = 0; j < g_ncalls; j++) {
            const Call *c = &g_calls[j];
            uint32_t a[MAX_ARGS], ne = 0, de = 0, no = 0, dorig = 0;
            double fe = 0, fo = 0;
            int k, new_ok, orig_ok, same, fails_before;
            uint32_t mask;
            if (c->rva != e->rva || c->n != e->dwords)
                continue;
            if (!args_usable(c, e->args)) {
                skip++;
                continue;
            }
            mask = e->ret_size == 1 ? 0xffu : e->ret_size == 2 ? 0xffffu : 0xffffffffu;
            ncalls++;
            build_args(c, e->args, a);
            fails_before = v.fails;
            v.broken = 0;
            sv_track_begin_new(&v);
            new_ok = safe_invoke(e->lifted, a, c->n, e->ret, &ne, &de, &fe);
            sv_track_switch_to_original();
            for (k = 0; k < c->n; k++)
                if (c->kind[k] == 'P')
                    memcpy(g_after_new[k], g_buf[k], BUF);
            build_args(c, e->args, a);
            orig_ok = safe_invoke((void *)(sr_base + e->rva), a, c->n, e->ret, &no, &dorig, &fo);
            same = e->ret == LF_RET_VOID || (e->ret == LF_RET_INT && ((ne ^ no) & mask) == 0) ||
                   (e->ret == LF_RET_INT64 && ne == no && de == dorig) ||
                   (e->ret == LF_RET_FLOAT && sv_same_float(fe, fo));
            for (k = 0; k < c->n; k++)
                if (c->kind[k] == 'P' && e->args[k] == 'p' && !same_memory(g_after_new[k], g_buf[k])) {
                    int b = 0;
                    while (g_after_new[k][b] == g_buf[k][b])
                        b++;
                    same = 0;
                    if (getenv("HARNESS_DEBUG"))
                        printf("  call %d arg %d: memory differs at %+d: new %02x original %02x\n", j, k,
                               b - BUF / 2, g_after_new[k][b], g_buf[k][b]);
                }
            sv_verify_end(&v, same && new_ok, NULL);
            if (!orig_ok) {
                skip++;
                if (!first[0])
                    snprintf(first, sizeof first, "call %d: original crashed: %s", j, g_crash);
                apply_snapshot();  /* the original crashed: its partial writes stay */
                resolve_imports();
            } else if (!new_ok) {
                crash++;
                if (!first[0])
                    snprintf(first, sizeof first, "call %d: the lifted version crashed: %s", j, g_crash);
            } else if (v.fails != fails_before || !same) {
                diff++;
                if (!first[0])
                    snprintf(first, sizeof first, "call %d: return %08x/%08x (float %g/%g)%s", j, ne, no, fe, fo,
                             same ? "" : ", return or pointer memory differs");
            } else {
                ok++;
            }
        }
        if (!ncalls)
            continue;
        tested++;
        total_ok += ok;
        total_diff += diff;
        total_skip += skip;
        total_crash += crash;
        printf("%-34s %3d same %3d different %3d crashed %3d skipped  %s\n", e->name, ok, diff, crash, skip,
               first);
    }
    printf("\n%d functions tested: %d calls same, %d different, %d lifted crashed, %d skipped\n", tested,
           total_ok, total_diff, total_crash, total_skip);
    return total_diff || total_crash ? 3 : 0;
}
