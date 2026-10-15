/* Offline differential test of the lifted functions.
 *
 * Usage: harness <subrosadedicated024d.exe> <sv_snapshot.bin> <sv_args.txt> [filter]
 *   filter: a part of a function name, or exact names separated by commas ("a,b,c"),
 *   or @file with one exact name per line (tools/lift/gate.py --changed).
 *   HARNESS_ENV_DIR: the sandbox folder of the test environment (default build/harness_env),
 *   so that several harnesses can run at the same time.
 *   HARNESS_RESET=1: put the snapshot back before each function (results do not depend on
 *   the order of the functions).
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
 *
 * Both versions run in the test environment of src/platform/plat_test.c: a fixed clock, a
 * heap pool, file writes in a sandbox and no network. The harness also compares what each
 * version does with the environment (packets sent, files written, heap blocks).
 * HARNESS_GAME_DIR is the server folder for file reads (default original/blackhouser).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness_ui.h"
#include "hook.h"
#include "lifted_table.h"
#include "plat_test.h"
#include "platform.h"
#include "sr_runtime.h"
#include "sr_server.h"
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
    int npackets;                 /* packets that the call received (E recvfrom lines) */
    PlatPacket *packets;
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
    uint8_t *shadow;  /* the content before the current call (both runs start from it) */
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
        /* malloc leaves the field with old heap bytes. apply_regions then skipped its malloc
         * and copied the region to that address: a crash or a silent overwrite, depending on
         * the heap content (gate.py --changed, 2026-10-15). */
        r->shadow = NULL;
        r->mapped = VirtualAlloc((void *)(uintptr_t)r->base, r->size, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH,
                                 PAGE_READWRITE) != NULL;
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
        if (fread((void *)(uintptr_t)g_regions[i].base, g_regions[i].size, 1, f) != 1) {
            g_regions[i].mapped = 0;
            continue;
        }
        if (!g_regions[i].shadow)
            g_regions[i].shadow = malloc(g_regions[i].size);
        if (g_regions[i].shadow)
            memcpy(g_regions[i].shadow, (void *)(uintptr_t)g_regions[i].base, g_regions[i].size);
        else
            g_regions[i].mapped = 0;
    }
    fclose(f);
}

/* Heap writes. The verifier undoes the .data writes of the lifted run, but the heap regions
 * of the snapshot are outside .data. Write watch finds the pages that each run writes:
 * after the lifted run they go back to the shadow copy, so the original starts from the
 * same heap, and the harness compares the pages of both runs. */
typedef struct HeapPage {
    uint8_t *addr;
    uint8_t data[4096];  /* the page after the lifted run */
} HeapPage;

static HeapPage *g_heap_pages;
static int g_nheap, g_heap_cap;
static void **g_watch_buf;
static ULONG_PTR g_watch_cap;

static void heap_reset_watch(void)
{
    int i;
    for (i = 0; i < g_nregions; i++)
        if (g_regions[i].mapped)
            ResetWriteWatch((void *)(uintptr_t)g_regions[i].base, g_regions[i].size);
}

/* Pages written in region i since the last reset. Returns the count (g_watch_buf). */
static ULONG_PTR heap_written(int i)
{
    ULONG_PTR count, pages = g_regions[i].size / 4096 + 1;
    ULONG gran;
    if (pages > g_watch_cap) {
        g_watch_buf = realloc(g_watch_buf, pages * sizeof(void *));
        g_watch_cap = pages;
    }
    count = g_watch_cap;
    if (GetWriteWatch(0, (void *)(uintptr_t)g_regions[i].base, g_regions[i].size, g_watch_buf, &count, &gran) != 0)
        return 0;
    return count;
}

/* After the lifted run: keep its pages and put the shadow content back. */
static void heap_after_lifted(void)
{
    int i;
    g_nheap = 0;
    for (i = 0; i < g_nregions; i++) {
        ULONG_PTR k, n;
        if (!g_regions[i].mapped)
            continue;
        n = heap_written(i);
        for (k = 0; k < n; k++) {
            uint8_t *page = g_watch_buf[k];
            if (g_nheap == g_heap_cap) {
                g_heap_cap = g_heap_cap ? g_heap_cap * 2 : 256;
                g_heap_pages = realloc(g_heap_pages, g_heap_cap * sizeof(HeapPage));
            }
            g_heap_pages[g_nheap].addr = page;
            memcpy(g_heap_pages[g_nheap].data, page, 4096);
            memcpy(page, g_regions[i].shadow + (page - (uint8_t *)(uintptr_t)g_regions[i].base), 4096);
            g_nheap++;
        }
    }
    heap_reset_watch();
}

static int same_page(const uint8_t *a, const uint8_t *b, int *at)
{
    int i;
    for (i = 0; i < 4096; i += 4) {
        float x, y;
        if (memcmp(a + i, b + i, 4) == 0)
            continue;
        memcpy(&x, a + i, 4);
        memcpy(&y, b + i, 4);
        if (!sv_same_float(x, y)) {
            *at = i;
            return 0;
        }
    }
    return 1;
}

static int cmp_heap_page(const void *a, const void *b)
{
    uintptr_t x = (uintptr_t)((const HeapPage *)a)->addr, y = (uintptr_t)((const HeapPage *)b)->addr;
    return x < y ? -1 : x > y;
}

/* After the original run: compare the pages that either run wrote, then make the state
 * after the original the new shadow. Returns 1 when the heap writes are the same. */
static int heap_after_original(char *why, size_t n)
{
    int i, k, same = 1, at;
    /* Sorted by address: the lookup of each page that the original wrote is a binary search
     * (a linear search made the area functions take minutes). */
    qsort(g_heap_pages, (size_t)g_nheap, sizeof(HeapPage), cmp_heap_page);
    for (k = 0; k < g_nheap && same; k++)
        if (!same_page(g_heap_pages[k].data, g_heap_pages[k].addr, &at)) {
            snprintf(why, n, "heap: memory at %08x differs", (unsigned)(uintptr_t)(g_heap_pages[k].addr + at));
            same = 0;
        }
    for (i = 0; i < g_nregions; i++) {
        ULONG_PTR j, cnt;
        uint8_t *base = (uint8_t *)(uintptr_t)g_regions[i].base;
        if (!g_regions[i].mapped)
            continue;
        cnt = heap_written(i);
        for (j = 0; j < cnt; j++) {
            uint8_t *page = g_watch_buf[j];
            uint8_t *old = g_regions[i].shadow + (page - base);
            /* A page that only the original wrote: the lifted run left the shadow content. */
            if (same) {
                int lifted_wrote = bsearch(&page, g_heap_pages, (size_t)g_nheap, sizeof(HeapPage), cmp_heap_page) != NULL;
                if (!lifted_wrote && !same_page(old, page, &at)) {
                    snprintf(why, n, "heap: lifted did not write %08x", (unsigned)(uintptr_t)(page + at));
                    same = 0;
                }
            }
            memcpy(old, page, 4096);
        }
    }
    for (k = 0; k < g_nheap; k++) {
        for (i = 0; i < g_nregions; i++) {
            uint8_t *base = (uint8_t *)(uintptr_t)g_regions[i].base;
            if (g_regions[i].mapped && g_heap_pages[k].addr >= base && g_heap_pages[k].addr < base + g_regions[i].size)
                memcpy(g_regions[i].shadow + (g_heap_pages[k].addr - base), g_heap_pages[k].addr, 4096);
        }
    }
    heap_reset_watch();
    return same;
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
    plat_test_patch_globals();
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
            void *stub;
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal)) {
                fn = GetProcAddress(m, (LPCSTR)(uintptr_t)IMAGE_ORDINAL32(names->u1.Ordinal));
                stub = plat_lookup(dll, NULL, IMAGE_ORDINAL32(names->u1.Ordinal));
            } else {
                const char *name = (const char *)((IMAGE_IMPORT_BY_NAME *)(sr_base + names->u1.AddressOfData))->Name;
                fn = GetProcAddress(m, name);
                stub = plat_lookup(dll, name, 0);
            }
            if (stub)
                fn = (FARPROC)stub;
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

/* Hex bytes up to the next space. Returns the count. */
static int hex_bytes(const char *hex, unsigned char *out, int max)
{
    int k;
    for (k = 0; k < max && isxdigit((unsigned char)hex[2 * k]) && isxdigit((unsigned char)hex[2 * k + 1]); k++)
        out[k] = (unsigned char)(hexval(hex[2 * k]) * 16 + hexval(hex[2 * k + 1]));
    return k;
}

/* "E recvfrom V:s V:len V:flags V:fromlen o:<data> o:<from> r:<ret>" (hook/record_imports.c),
 * or "E recv V:s V:len V:flags o:<data> r:<ret>": add the packet to the call. */
static void add_packet(Call *c, char *line)
{
    PlatPacket *p;
    char *tok, *data = NULL, *from = NULL;
    int outs = 0;
    c->packets = realloc(c->packets, (c->npackets + 1) * sizeof(PlatPacket));
    p = &c->packets[c->npackets];
    memset(p, 0, sizeof *p);
    p->ret = -1;
    for (tok = strtok(line, " \r\n"); tok; tok = strtok(NULL, " \r\n")) {
        if (!strncmp(tok, "o:", 2) && outs++ == 0)
            data = tok + 2;
        else if (!strncmp(tok, "o:", 2))
            from = tok + 2;
        else if (!strncmp(tok, "r:", 2))
            p->ret = (int)strtoul(tok + 2, NULL, 16);
    }
    if (data && p->ret > 0) {
        p->data = malloc((size_t)p->ret);
        p->len = hex_bytes(data, p->data, p->ret);
    }
    if (from)
        p->fromlen = hex_bytes(from, p->from, (int)sizeof p->from);
    c->npackets++;
}

/* Hand-written functions (src/server/) that the lifter does not convert. The recordings have
 * no call of a function without a record hook. A function without arguments then runs once
 * from the snapshot. */
static const LfEntry hand_table[] = {
    { "loadaccountsserver", 0xe2e0, (void *)(uintptr_t)sr_loadaccountsserver, 0, LF_RET_VOID, 4, "" },
};
#define HAND_COUNT ((int)(sizeof hand_table / sizeof hand_table[0]))

static const LfEntry *table_entry(int i)
{
    return i < lf_table_count ? &lf_table[i] : &hand_table[i - lf_table_count];
}

static void add_hand_calls(void)
{
    int i, j;
    for (i = 0; i < HAND_COUNT; i++) {
        const LfEntry *e = &hand_table[i];
        for (j = 0; j < g_ncalls && g_calls[j].rva != e->rva; j++)
            ;
        if (j < g_ncalls || e->dwords != 0)
            continue;
        g_calls = realloc(g_calls, (g_ncalls + 1) * sizeof(Call));
        memset(&g_calls[g_ncalls], 0, sizeof(Call));
        g_calls[g_ncalls++].rva = e->rva;
    }
}

static int load_calls(const char *path)
{
    FILE *f = fopen(path, "r");
    /* An E line holds up to 64 KB of bytes as hex (hook/record.c). */
    static char line[(64u << 10) * 2 + 4096];
    int cap = 1024;
    int last = -1;            /* the call that the E lines belong to */
    if (!f)
        return 0;
    g_calls = calloc(cap, sizeof(Call));
    while (fgets(line, sizeof line, f)) {
        Call *c;
        char *p = line, *tok;
        if (last >= 0 && (!strncmp(line, "E recvfrom ", 11) || !strncmp(line, "E recv ", 7))) {
            add_packet(&g_calls[last], line);
            continue;
        }
        if (line[0] == 'Z')
            last = -1;
        if (line[0] != 'R')
            continue;
        last = -1;
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
        last = g_ncalls++;
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

/* 1 when a value argument is an address inside the stack of this thread. A pointer to a
 * local of the server that Ghidra typed as an int (rotateorientation gets "(int)&local_38")
 * has no content here, and both versions would write over the stack of the harness. */
/* The name filter: a part of a name, or a list of exact names (commas or @file). */
static char *g_names;  /* ",a,b,c," for an exact list, NULL for a part of a name */
static const char *g_part;

static void set_filter(const char *filter)
{
    size_t n;
    if (!filter)
        return;
    if (filter[0] == '@') {
        FILE *f = fopen(filter + 1, "r");
        char line[256];
        g_names = calloc(1, 2);
        g_names[0] = ',';
        n = 1;
        while (f && fgets(line, sizeof line, f)) {
            size_t k = strcspn(line, " \r\n");
            if (!k)
                continue;
            g_names = realloc(g_names, n + k + 2);
            memcpy(g_names + n, line, k);
            n += k;
            g_names[n++] = ',';
            g_names[n] = '\0';
        }
        if (f)
            fclose(f);
        return;
    }
    if (!strchr(filter, ',')) {
        g_part = filter;
        return;
    }
    n = strlen(filter);
    g_names = malloc(n + 3);
    snprintf(g_names, n + 3, ",%s,", filter);
}

static int filter_match(const char *name)
{
    char key[96];
    if (g_part)
        return strstr(name, g_part) != NULL;
    if (!g_names)
        return 1;
    snprintf(key, sizeof key, ",%s,", name);
    return strstr(g_names, key) != NULL;
}

static int points_into_stack(const Call *c, const char *kinds)
{
    ULONG_PTR low, high;
    int i;
    GetCurrentThreadStackLimits(&low, &high);
    for (i = 0; i < c->n; i++)
        if ((c->kind[i] == 'V' || (c->kind[i] == 'P' && kinds[i] != 'p')) && c->value[i] >= low &&
            c->value[i] < high)
            return 1;
    return 0;
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

/* Probes: HARNESS_PROBE=rva[:espoff],... puts an INT3 on each instruction of the original
 * and prints ST0, ST1 and the float at [esp+espoff] when it runs ("rva:espoff*": the 3
 * floats that [esp+espoff] points to). Compare the intermediate
 * values of the original with printf()s put in the lifted C. */
#define MAX_PROBES 16
static struct {
    uint32_t rva, espoff;
    int has_off;
    uint8_t byte;
} g_probe[MAX_PROBES];
static int g_nprobe, g_rearm = -1;

static double x87_to_double(const BYTE *r)
{
    uint64_t m;
    int e = (r[8] | (r[9] << 8)) & 0x7fff;
    memcpy(&m, r, 8);
    if (e == 0 && m == 0)
        return (r[9] & 0x80) ? -0.0 : 0.0;
    return ((r[9] & 0x80) ? -1 : 1) * ldexp((double)m, e - 16383 - 63);
}

static LONG CALLBACK probe_handler(EXCEPTION_POINTERS *ep)
{
    CONTEXT *c = ep->ContextRecord;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    int k;
    if (code == EXCEPTION_SINGLE_STEP && g_rearm >= 0) {
        *(uint8_t *)(sr_base + g_probe[g_rearm].rva) = 0xcc;
        g_rearm = -1;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code != EXCEPTION_BREAKPOINT)
        return EXCEPTION_CONTINUE_SEARCH;
    for (k = 0; k < g_nprobe; k++) {
        if ((uintptr_t)ep->ExceptionRecord->ExceptionAddress != sr_base + g_probe[k].rva)
            continue;
        printf("  probe %05x: st0 %.17g st1 %.17g", g_probe[k].rva, x87_to_double(c->FloatSave.RegisterArea),
               x87_to_double(c->FloatSave.RegisterArea + 10));
        if (g_probe[k].has_off == 2) {
            const float *v = *(const float **)(uintptr_t)(c->Esp + g_probe[k].espoff);
            printf(" *[esp+%x] %.9g %.9g %.9g", g_probe[k].espoff, v[0], v[1], v[2]);
        } else if (g_probe[k].has_off) {
            float f;
            memcpy(&f, (void *)(uintptr_t)(c->Esp + g_probe[k].espoff), 4);
            printf(" [esp+%x] %.9g (%08x)", g_probe[k].espoff, f, *(uint32_t *)&f);
        }
        printf(" eax %x ebx %x esi %x edi %x ebp %x\n", c->Eax, c->Ebx, c->Esi, c->Edi, c->Ebp);
        *(uint8_t *)(sr_base + g_probe[k].rva) = g_probe[k].byte;
        c->Eip = (DWORD)(sr_base + g_probe[k].rva);
        c->EFlags |= 0x100;  /* single step, then put the INT3 back */
        g_rearm = k;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void set_probes(const char *spec)
{
    while (spec && *spec && g_nprobe < MAX_PROBES) {
        char *end;
        DWORD old;
        uint8_t *p;
        g_probe[g_nprobe].rva = strtoul(spec, &end, 16);
        if (*end == ':') {
            g_probe[g_nprobe].espoff = strtoul(end + 1, &end, 16);
            g_probe[g_nprobe].has_off = 1;
            if (*end == '*') {  /* rva:off*: [esp+off] is a pointer to 3 floats */
                g_probe[g_nprobe].has_off = 2;
                end++;
            }
        }
        p = (uint8_t *)(sr_base + g_probe[g_nprobe].rva);
        VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old);
        g_probe[g_nprobe].byte = *p;
        *p = 0xcc;
        g_nprobe++;
        spec = *end == ',' ? end + 1 : NULL;
    }
    if (g_nprobe)
        AddVectoredExceptionHandler(1, probe_handler);
}

/* Watchpoint: HARNESS_WATCH=rva puts a hardware data breakpoint (DR0, 4 bytes, writes) on
 * one .data address and prints each instruction that writes it, with the new value. The
 * place is an RVA in the 024d, or an address in the harness (the lifted code). */
static uint32_t g_watch;

static LONG CALLBACK watch_handler(EXCEPTION_POINTERS *ep)
{
    CONTEXT *c = ep->ContextRecord;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == 0xE0574348) {  /* raised by set_watch: load the debug registers */
        c->Dr0 = (uintptr_t)sr_base + g_watch;
        c->Dr7 = 1 | (1u << 16) | (3u << 18);  /* L0, write, 4 bytes */
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code == EXCEPTION_SINGLE_STEP && (c->Dr6 & 1)) {
        uintptr_t at = c->Eip;
        uint32_t v = *(uint32_t *)(sr_base + g_watch);
        float f;
        memcpy(&f, &v, 4);
        if (at >= sr_base && at < sr_base + g_image_size)
            printf("  write %08x (%.9g) after 024d+%x\n", v, f, (unsigned)(at - sr_base));
        else
            printf("  write %08x (%.9g) after %p (harness)\n", v, f, (void *)at);
        c->Dr6 = 0;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void set_watch(const char *spec)
{
    if (!spec)
        return;
    g_watch = strtoul(spec, NULL, 16);
    AddVectoredExceptionHandler(1, watch_handler);
    RaiseException(0xE0574348, 0, 0, NULL);
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

/* Differences that no C version can remove. They do not count as a failure. */
static const char *known_difference(const char *name)
{
    static const char *const known[][2] = {
        /* sscanf("%d") finds no number: the original returns an uninitialized stack value. */
        { "getfloat", "returns an uninitialized local when there is no number" },
        /* x87 precision: the error grows through the computation (docs/lift.md, "Float precision"). */
        { "buildsomeshit", "x87 precision" },
    };
    size_t k;
    for (k = 0; k < sizeof known / sizeof known[0]; k++)
        if (strcmp(known[k][0], name) == 0)
            return known[k][1];
    return NULL;
}

/* ------------------------------------------------------------------ main */

/* A crash outside the guarded calls: print where, with the module and its offset, so the
 * gate log shows more than an exit code 0xc0000005. */
static LONG WINAPI unhandled(EXCEPTION_POINTERS *ep)
{
    HMODULE m = NULL;
    char name[MAX_PATH] = "?";
    uintptr_t at = (uintptr_t)ep->ExceptionRecord->ExceptionAddress;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (const char *)at, &m))
        GetModuleFileNameA(m, name, sizeof name);
    fprintf(stderr, "harness crashed: exception %08lx at %s+0x%x (data address %08x)\n",
            ep->ExceptionRecord->ExceptionCode, strrchr(name, '\\') ? strrchr(name, '\\') + 1 : name,
            (unsigned)(at - (uintptr_t)m),
            ep->ExceptionRecord->NumberParameters >= 2 ? (unsigned)ep->ExceptionRecord->ExceptionInformation[1] : 0);
    fprintf(stderr, "  esi %08lx edi %08lx ecx %08lx, 024d at %08x, snapshot pages %p (%u)\n",
            ep->ContextRecord->Esi, ep->ContextRecord->Edi, ep->ContextRecord->Ecx, (unsigned)sr_base,
            (void *)g_snap.pages, g_snap.npages);
    {
        int r;
        for (r = 0; r < g_nregions; r++)
            fprintf(stderr, "  heap region %08x size %08x mapped %d shadow %p\n", g_regions[r].base, g_regions[r].size,
                    g_regions[r].mapped, (void *)g_regions[r].shadow);
    }
    /* Stack words that point into the same module: likely return addresses (build/hook/*.map). */
    if (m) {
        const uint32_t *sp = (const uint32_t *)(uintptr_t)ep->ContextRecord->Esp;
        uintptr_t lo = (uintptr_t)m, hi = lo + 0x400000;
        int k, shown = 0;
        fprintf(stderr, "  stack:");
        for (k = 0; k < 256 && shown < 12; k++) {
            if (IsBadReadPtr(sp + k, 4))
                break;
            if (sp[k] > lo && sp[k] < hi) {
                fprintf(stderr, " +0x%x", (unsigned)(sp[k] - lo));
                shown++;
            }
        }
        fprintf(stderr, "\n");
    }
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char **argv)
{
    int i, j, total_ok = 0, total_diff = 0, total_known = 0, total_skip = 0, total_crash = 0, tested = 0;
    const char *filter = argc > 4 ? argv[4] : NULL;
    const char *game_dir;
    int reset_each = getenv("HARNESS_RESET") && atoi(getenv("HARNESS_RESET")) != 0;
    SetUnhandledExceptionFilter(unhandled);
    int imports, index = 0, total_funcs = 0;
    LARGE_INTEGER t_start, t_end, freq;
    IMAGE_NT_HEADERS32 *nt;

    if (argc < 4) {
        fprintf(stderr, "usage: harness <024d.exe> <sv_snapshot.bin> <sv_args.txt> [name filter]\n");
        return 2;
    }
    setvbuf(stdout, NULL, _IONBF, 0);  /* a crash keeps all output before it */
    ui_init();
    ui_banner();
    QueryPerformanceCounter(&t_start);
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
    add_hand_calls();
    for (i = 0; i < MAX_ARGS; i++)
        g_buf[i] = VirtualAlloc(NULL, BUF, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    apply_snapshot();
    /* The game folder: HARNESS_GAME_DIR, else the folder of the recorded server (getpaths
     * stores it at RVA 0x916d4e0) when it exists on this PC, else the original server. */
    game_dir = getenv("HARNESS_GAME_DIR");
    if (!game_dir) {
        const char *recorded = (const char *)(sr_base + 0x916d4e0);
        DWORD attr = GetFileAttributesA(recorded);
        game_dir = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) ? recorded
                                                                                       : "original/blackhouser";
    }
    set_filter(filter);
    if (!plat_test_init(game_dir, getenv("HARNESS_ENV_DIR") ? getenv("HARNESS_ENV_DIR") : "build/harness_env")) {
        fprintf(stderr, "cannot set up the test environment (game folder %s, MSVCR100.dll)\n", game_dir);
        return 1;
    }
    plat_test_patch_globals();
    imports = resolve_imports();
    lf_x87_init();
    set_probes(getenv("HARNESS_PROBE"));
    set_watch(getenv("HARNESS_WATCH"));
    if (!sv_verify_init()) {
        fprintf(stderr, "verifier: no .data section\n");
        return 1;
    }
    sv_verify_ignore(0x14043e0, 0x5800);  /* profiler timers (RDTSC), see hooks_024d.c */
    ui_info("recording", "%s (%d calls)", argv[3], g_ncalls);
    ui_info("snapshot", "%s (%u pages, %d of %d heap regions)", argv[2], g_snap.npages, g_mapped, g_nregions);
    ui_info("024d", "%p (snapshot base %08x), %d imports", (void *)g_mod, g_snap.base, imports);
    ui_info("game folder", "%s", game_dir);
    ui_info("filter", "%s", filter ? filter : "(all functions)");
    ui_info("reset", "%s", reset_each ? "snapshot before each function" : "off");
    printf("\n");
    if ((uint32_t)sr_base != g_snap.base)
        ui_warning("the 024d did not load at the snapshot base. Pointers in the snapshot are guessed. "
                   "Use the same copy from tools/re/fixed_base.py for the server and the harness.");
    for (i = 0; i < lf_table_count + HAND_COUNT; i++)
        if (filter_match(table_entry(i)->name))
            total_funcs++;

    for (i = 0; i < lf_table_count + HAND_COUNT; i++) {
        const LfEntry *e = table_entry(i);
        SvVerify v;
        int ok = 0, diff = 0, skip = 0, crash = 0, ncalls = 0, stack_skip = 0;
        char first[320] = "";
        if (!filter_match(e->name))
            continue;
        /* HARNESS_RESET=1: every function starts from the snapshot. Without it, the writes of
         * the original runs stay, and a later function can see a heap that the earlier calls
         * changed (humansimulation call 1223 of 20261013-195210 crashes in the original after
         * 1222 other calls). With it, a result does not depend on the other functions, so
         * gate.py --changed gives the same counts as a full gate. */
        if (reset_each && index > 0) {
            apply_snapshot();
            resolve_imports();
        }
        index++;
        memset(&v, 0, sizeof v);
        v.name = e->name;
        v.every = 1;
        for (j = 0; j < g_ncalls; j++) {
            const Call *c = &g_calls[j];
            uint32_t a[MAX_ARGS], ne = 0, de = 0, no = 0, dorig = 0;
            char env_why[256] = "", heap_why[256] = "";
            int env_same;
            double fe = 0, fo = 0;
            int k, new_ok, orig_ok, same, fails_before;
            uint32_t mask;
            if (c->rva != e->rva || c->n != e->dwords)
                continue;
            if (!args_usable(c, e->args)) {
                skip++;
                continue;
            }
            if (points_into_stack(c, e->args)) {
                skip++;
                stack_skip++;
                continue;
            }
            mask = e->ret_size == 1 ? 0xffu : e->ret_size == 2 ? 0xffffu : 0xffffffffu;
            ncalls++;
            build_args(c, e->args, a);
            fails_before = v.fails;
            v.broken = 0;
            sv_track_begin_new(&v);
            if (g_nprobe)
                printf(" call %d: lifted\n", j);
            plat_test_set_packets(c->packets, c->npackets);
            plat_test_begin(0);
            heap_reset_watch();
            new_ok = safe_invoke(e->lifted, a, c->n, e->ret, &ne, &de, &fe);
            plat_test_end(0);
            heap_after_lifted();
            sv_track_switch_to_original();
            for (k = 0; k < c->n; k++)
                if (c->kind[k] == 'P')
                    memcpy(g_after_new[k], g_buf[k], BUF);
            build_args(c, e->args, a);
            if (g_nprobe)
                printf(" call %d: original\n", j);
            plat_test_begin(1);
            orig_ok = safe_invoke((void *)(sr_base + e->rva), a, c->n, e->ret, &no, &dorig, &fo);
            plat_test_end(1);
            env_same = plat_test_same(env_why, sizeof env_why);
            if (!heap_after_original(heap_why, sizeof heap_why) && env_same) {
                env_same = 0;
                memcpy(env_why, heap_why, sizeof env_why);
            }
            same = e->ret == LF_RET_VOID || (e->ret == LF_RET_INT && ((ne ^ no) & mask) == 0) ||
                   (e->ret == LF_RET_INT64 && ne == no && de == dorig) ||
                   (e->ret == LF_RET_FLOAT && sv_same_float(fe, fo));
            for (k = 0; k < c->n; k++)
                if (c->kind[k] == 'P' && e->args[k] == 'p' && !same_memory(g_after_new[k], g_buf[k])) {
                    int b = 0;
                    while (g_after_new[k][b] == g_buf[k][b])
                        b++;
                    same = 0;
                    if (getenv("HARNESS_DEBUG")) {
                        float fn, fo2;
                        memcpy(&fn, g_after_new[k] + (b & ~3), 4);
                        memcpy(&fo2, g_buf[k] + (b & ~3), 4);
                        printf("  call %d arg %d: memory differs at %+d: new %02x original %02x (as float %.9g / %.9g)\n",
                               j, k, b - BUF / 2, g_after_new[k][b], g_buf[k][b], fn, fo2);
                    }
                }
            if (!env_same && getenv("HARNESS_DEBUG"))
                printf("  call %d: %s\n", j, env_why);
            if (getenv("HARNESS_DEBUG") && atoi(getenv("HARNESS_DEBUG")) >= 2)
                plat_test_dump();
            sv_verify_end(&v, same && env_same && new_ok, NULL);
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
            } else if (v.fails != fails_before || !same || !env_same) {
                diff++;
                if (!first[0] && !env_same)
                    snprintf(first, sizeof first, "call %d: %s", j, env_why);
                else if (!first[0])
                    snprintf(first, sizeof first, "call %d: return %08x/%08x (float %g/%g)%s", j, ne, no, fe, fo,
                             same ? "" : ", return or pointer memory differs");
            } else {
                ok++;
            }
        }
        /* A value argument that points into the stack is often a pointer that Ghidra typed
         * as an int: analysis/024d/params.csv gives it a pointer type. Without this note, a
         * function whose calls are all skipped left no line (loadblock, 2026-10-15). */
        if (stack_skip && !first[0])
            snprintf(first, sizeof first, "%d skipped: value points into the stack (check params.csv)", stack_skip);
        if (!ncalls) {
            if (skip) {
                total_skip += skip;
                ui_function(index, total_funcs, e->name, 0, 0, 0, skip, 0,
                            first[0] ? first : "all calls skipped: old record without a value");
            }
            continue;
        }
        tested++;
        total_ok += ok;
        if (diff && known_difference(e->name)) {
            total_known += diff;
            snprintf(first, sizeof first, "known: %s", known_difference(e->name));
        } else {
            total_diff += diff;
        }
        total_skip += skip;
        total_crash += crash;
        ui_function(index, total_funcs, e->name, ok, diff, crash, skip, diff && known_difference(e->name), first);
    }
    QueryPerformanceCounter(&t_end);
    QueryPerformanceFrequency(&freq);
    ui_summary(tested, total_ok, total_diff, total_known, total_crash, total_skip,
               (double)(t_end.QuadPart - t_start.QuadPart) / (double)freq.QuadPart);
    return total_diff || total_crash ? 3 : 0;
}
