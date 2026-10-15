/* Generic verification by page protection. See verify.h. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"
#include "sr_global_table.h"
#include "sr_runtime.h"
#include "verify.h"
#include "record.h"

#define PAGE 4096u
/* Pages changed per run and per version (32 MB): createlandscape writes 17 MB. The offline
 * harness sets more: createarea touches about 8200 pages, createlevel more than 16384. */
#ifndef SV_MAX_PAGES
#define SV_MAX_PAGES 8192
#endif
#define MAX_PAGES SV_MAX_PAGES

static uint8_t *g_data;        /* start of the game .data section */
static size_t g_data_size;
static int g_enabled;          /* subvulpes.ini: verify */
static int g_log_calls;        /* subvulpes.ini: log_calls */
static int g_full_every;       /* subvulpes.ini: verify_full_every */
static int g_relax;            /* subvulpes.ini: verify_relax */
static SvVerify *g_scope;      /* hook of the current verification */
static int g_full;             /* 1 = this verification protects the whole .data */
static LARGE_INTEGER g_t0, g_freq;
static PVOID g_veh;
static const char *g_section = ".data";  /* tests use their own section */

void sv_verify_set_section(const char *name)
{
    g_section = name;
}

/* MSVCR100 rand(): the state lives in _getptd()->_holdrand (+0x14). */
static int *g_holdrand;

typedef struct PageSet {
    uint32_t count;
    uint32_t index[MAX_PAGES];       /* page number inside .data */
    uint8_t *before[MAX_PAGES];      /* content before the first write */
} PageSet;

static PageSet g_new, g_orig;
static int find_page(const PageSet *s, uint32_t page);
static PageSet *g_active;           /* set that receives the faults now */
static uint8_t *g_new_result[MAX_PAGES];  /* pages of the new version, after it ran */
static int g_overflow;
static int g_holdrand_before, g_holdrand_new;

/* Simple pool of copy pages (reused between calls). The pool is only reserved: pages are
 * committed in steps of POOL_STEP when a call needs them. A full commit of MAX_PAGES * 3
 * pages (576 MB) counted against the Windows commit limit even when a call used a few pages,
 * and a harness failed to start when the limit was nearly full. */
#define POOL_STEP 256
static uint8_t *g_pool;
static uint32_t g_pool_used, g_pool_cap, g_pool_committed;

static uint8_t *pool_page(void)
{
    if (g_pool_used == g_pool_cap)
        return NULL;
    if (g_pool_used == g_pool_committed) {
        uint32_t n = g_pool_cap - g_pool_committed < POOL_STEP ? g_pool_cap - g_pool_committed : POOL_STEP;
        if (!VirtualAlloc(g_pool + (size_t)g_pool_committed * PAGE, (size_t)n * PAGE, MEM_COMMIT, PAGE_READWRITE))
            return NULL;
        g_pool_committed += n;
    }
    return g_pool + (size_t)(g_pool_used++) * PAGE;
}

/* Heap of the game (MSVCR100 malloc). Page protection covers only .data, so the verifier
 * copies the committed pages of this heap before the new version runs, finds the pages each
 * version changed by comparison, and undoes the writes of the new version. Without it, a
 * malloc block that the new version writes stays changed for the original:
 * area_createblock stored the new chunk in the chunk grid (a malloc block), so the original
 * found the chunk, did not create it, and the game kept a chunk count of 0.
 *
 * Only the content of the blocks in use counts as a difference. The allocator data (heap
 * header, free lists, block headers) can differ between two runs with the same effect.
 * When the new version frees pages of the heap (a large block or a decommit), the copy
 * cannot go back: the heap stays as the new version left it, as without heap verification.
 *
 * subvulpes.ini verify_heap=0 turns it off. Only the live DLL calls sv_verify_watch_heap:
 * the offline harness uses write watch on its heap regions. */
typedef struct HeapRange {
    uint8_t *base;
    size_t size;
    size_t shadow;  /* offset of the copy in g_hshadow */
} HeapRange;

typedef struct HeapSpan {
    uintptr_t start, end;  /* user data of one block in use: [start, end) */
} HeapSpan;

static HANDLE g_heap;
static int near_float(uint32_t a, uint32_t b);
static uint8_t *g_halloc[128];  /* allocation bases of the heap (segments and large blocks) */
static int g_nhalloc;
static HeapSpan *g_hbusy;       /* blocks in use at the last scan, sorted by address */
static size_t g_nhbusy, g_hbusy_cap;
static HeapRange g_hr[512];     /* committed read/write ranges at the start of this verification */
static int g_nhr;
static uint8_t *g_hshadow, *g_hnew, *g_hchanged;  /* copy before, result of new, page changed by new */
static size_t g_hcap, g_hlogged;
static int g_hlost;             /* 1 = the new version freed heap pages: no undo, no compare */
static long g_hlost_count;

static void heap_add_alloc(uint8_t *base)
{
    int i;
    for (i = 0; i < g_nhalloc; i++)
        if (g_halloc[i] == base)
            return;
    if (g_nhalloc < (int)(sizeof g_halloc / sizeof g_halloc[0]))
        g_halloc[g_nhalloc++] = base;
}

static int cmp_span(const void *a, const void *b)
{
    uintptr_t x = ((const HeapSpan *)a)->start, y = ((const HeapSpan *)b)->start;
    return x < y ? -1 : x > y;
}

/* The allocations of the heap (its segments and its large blocks) and the blocks in use. */
static void heap_scan(void)
{
    PROCESS_HEAP_ENTRY e;
    MEMORY_BASIC_INFORMATION mbi;
    g_nhalloc = 0;
    g_nhbusy = 0;
    if (!HeapLock(g_heap))
        return;
    e.lpData = NULL;
    while (HeapWalk(g_heap, &e)) {
        if (!(e.wFlags & (PROCESS_HEAP_REGION | PROCESS_HEAP_ENTRY_BUSY)))
            continue;
        if (VirtualQuery(e.lpData, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT)
            heap_add_alloc((uint8_t *)mbi.AllocationBase);
        if (!(e.wFlags & PROCESS_HEAP_ENTRY_BUSY) || !e.cbData)
            continue;
        if (g_nhbusy == g_hbusy_cap) {
            /* The hook DLL allocates from the process heap, not from the heap it scans. */
            size_t cap = g_hbusy_cap ? g_hbusy_cap * 2 : 4096;
            HeapSpan *n = (HeapSpan *)realloc(g_hbusy, cap * sizeof(HeapSpan));
            if (!n)
                break;
            g_hbusy = n;
            g_hbusy_cap = cap;
        }
        g_hbusy[g_nhbusy].start = (uintptr_t)e.lpData;
        g_hbusy[g_nhbusy].end = (uintptr_t)e.lpData + e.cbData;
        g_nhbusy++;
    }
    HeapUnlock(g_heap);
    qsort(g_hbusy, g_nhbusy, sizeof(HeapSpan), cmp_span);
}

/* 1 = the address is in the user data of a block in use (at the last scan). */
static int heap_in_block(uintptr_t a)
{
    size_t lo = 0, hi = g_nhbusy;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (g_hbusy[mid].end <= a)
            lo = mid + 1;
        else if (g_hbusy[mid].start > a)
            hi = mid;
        else
            return 1;
    }
    return 0;
}

/* Committed read/write ranges of the allocations, and a copy of their content. */
static void heap_begin(void)
{
    int i;
    size_t total = 0;
    if (g_full || !g_nhalloc)
        heap_scan();
    g_nhr = 0;
    g_hlost = 0;
    for (i = 0; i < g_nhalloc; i++) {
        uint8_t *p = g_halloc[i];
        MEMORY_BASIC_INFORMATION mbi;
        while (VirtualQuery(p, &mbi, sizeof mbi) && mbi.AllocationBase == g_halloc[i]) {
            if (mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READWRITE &&
                g_nhr < (int)(sizeof g_hr / sizeof g_hr[0])) {
                g_hr[g_nhr].base = (uint8_t *)mbi.BaseAddress;
                g_hr[g_nhr].size = mbi.RegionSize;
                g_hr[g_nhr].shadow = total;
                total += mbi.RegionSize;
                g_nhr++;
            }
            p = (uint8_t *)mbi.BaseAddress + mbi.RegionSize;
        }
    }
    if (total > g_hlogged + g_hlogged / 4) {
        g_hlogged = total;
        sv_log("verifier: heap copy of %u KB in %d ranges, %u blocks in use", (unsigned)(total / 1024), g_nhr,
               (unsigned)g_nhbusy);
    }
    if (total > g_hcap) {
        if (g_hshadow)
            VirtualFree(g_hshadow, 0, MEM_RELEASE);
        g_hcap = total + total / 2;
        /* One block: the copy before, the result of the new version, one flag per page. */
        g_hshadow = (uint8_t *)VirtualAlloc(NULL, g_hcap * 2 + g_hcap / PAGE, MEM_COMMIT | MEM_RESERVE,
                                            PAGE_READWRITE);
        if (!g_hshadow) {
            sv_log("ERROR verifier: no memory for a heap copy of %u KB, heap not verified", (unsigned)(total / 1024));
            g_hcap = 0;
            g_nhr = 0;
            g_heap = NULL;
            return;
        }
        g_hnew = g_hshadow + g_hcap;
        g_hchanged = g_hnew + g_hcap;
    }
    for (i = 0; i < g_nhr; i++)
        memcpy(g_hshadow + g_hr[i].shadow, g_hr[i].base, g_hr[i].size);
}

/* 1 = the whole range is still committed read/write memory. */
static int heap_range_ok(const HeapRange *r)
{
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery(r->base, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READWRITE &&
           mbi.RegionSize >= r->size;
}

/* After the new version: keep the pages it changed and put the copy back. */
static void heap_after_new(const SvVerify *v)
{
    int i;
    size_t off;
    for (i = 0; i < g_nhr; i++) {
        if (!heap_range_ok(&g_hr[i])) {
            g_hlost = 1;
            if (++g_hlost_count <= 20)
                sv_log("VERIFY %s: the new version freed heap pages at 0x%08x, heap not undone (call %ld)",
                       v->name, (unsigned)(uintptr_t)g_hr[i].base, v->calls);
            return;
        }
    }
    for (i = 0; i < g_nhr; i++) {
        for (off = 0; off < g_hr[i].size; off += PAGE) {
            uint8_t *page = g_hr[i].base + off;
            size_t s = g_hr[i].shadow + off;
            g_hchanged[s / PAGE] = 0;
            if (memcmp(page, g_hshadow + s, PAGE) != 0) {
                g_hchanged[s / PAGE] = 1;
                memcpy(g_hnew + s, page, PAGE);
                memcpy(page, g_hshadow + s, PAGE);
            }
        }
    }
}

/* After the original: compare the blocks in use. Returns the count of different bytes
 * (the log gets the first ones, up to 8 with the .data differences before them). */
static long heap_compare(long logged, long *nfloat)
{
    int i;
    size_t off;
    long diffs = 0;
    if (g_hlost)
        return 0;
    for (i = 0; i < g_nhr; i++) {
        if (!heap_range_ok(&g_hr[i]))
            continue;  /* the original freed it: nothing to compare */
        for (off = 0; off < g_hr[i].size; off += PAGE) {
            const uint8_t *now = g_hr[i].base + off;
            size_t s = g_hr[i].shadow + off;
            const uint8_t *ref = g_hchanged[s / PAGE] ? g_hnew + s : g_hshadow + s;
            uint32_t b;
            if (memcmp(ref, now, PAGE) == 0)
                continue;
            for (b = 0; b < PAGE; b++) {
                if ((b & 3) == 0 && memcmp(ref + b, now + b, 4) != 0 && heap_in_block((uintptr_t)(now + b))) {
                    uint32_t x, y;
                    memcpy(&x, ref + b, 4);
                    memcpy(&y, now + b, 4);
                    if (near_float(x, y)) {
                        (*nfloat)++;
                        b += 3;
                        continue;
                    }
                }
                if (ref[b] == now[b] || !heap_in_block((uintptr_t)(now + b)))
                    continue;
                if (logged + ++diffs <= 8) {
                    if (g_hchanged[s / PAGE])
                        sv_log("  heap 0x%08x: new=%02x original=%02x", (unsigned)(uintptr_t)(now + b), ref[b], now[b]);
                    else
                        sv_log("  heap 0x%08x: new did not write, original=%02x", (unsigned)(uintptr_t)(now + b), now[b]);
                }
            }
        }
    }
    return diffs;
}

int sv_verify_watch_heap(void)
{
    HMODULE crt = GetModuleHandleA("MSVCR100.dll");
    intptr_t (__cdecl *get_heap)(void);
    if (!g_enabled || !sv_config_int("verify_heap", 1))
        return 0;
    get_heap = crt ? (intptr_t (__cdecl *)(void))(void *)GetProcAddress(crt, "_get_heap_handle") : NULL;
    if (!get_heap || !get_heap()) {
        sv_log("warning verifier: MSVCR100 heap not found; writes to malloc blocks are not verified");
        return 0;
    }
    /* The hook DLL (static CRT) uses the process heap: the log buffers must stay out of the copy. */
    if ((HANDLE)get_heap() == GetProcessHeap()) {
        sv_log("warning verifier: MSVCR100 uses the process heap; writes to malloc blocks are not verified");
        return 0;
    }
    g_heap = (HANDLE)get_heap();
    sv_log("verifier: MSVCR100 heap at 0x%08x is verified (copy and compare)", (unsigned)(uintptr_t)g_heap);
    return 1;
}

static void protect_all(DWORD prot)
{
    static int warned;
    DWORD old;
    if (!VirtualProtect(g_data, g_data_size, prot, &old) && !warned) {
        warned = 1;
        sv_log("ERROR verifier: VirtualProtect of .data failed (%lu)", GetLastError());
    }
}

/* Pages added on each side of a learned range. A list that grows (for example the
 * surfaces of addbeziersurface) writes the next page in a later call. Without the margin,
 * that write escapes the scope round: it is not undone before the original runs. */
#define SCOPE_MARGIN 16u

/* Protect only what is necessary: the whole .data (full round) or the learned scope. */
static void protect_scope(DWORD prot)
{
    unsigned i;
    DWORD old;
    if (g_full || !g_scope) {
        protect_all(prot);
        return;
    }
    for (i = 0; i < g_scope->nranges; i++) {
        size_t pages = g_data_size / PAGE;
        size_t first = g_scope->range[i][0] > SCOPE_MARGIN ? g_scope->range[i][0] - SCOPE_MARGIN : 0;
        size_t last = g_scope->range[i][1] + SCOPE_MARGIN;
        if (last > pages)
            last = pages;
        VirtualProtect(g_data + first * PAGE, (last - first) * PAGE, prot, &old);
    }
}

/* Page range of the global that contains the page (the whole array), or only the page. */
static void global_pages(uint32_t page, unsigned *first, unsigned *last)
{
    uint32_t data_rva = (uint32_t)((uintptr_t)g_data - sr_base);
    uint32_t rva = data_rva + page * PAGE;
    size_t k;
    *first = page;
    *last = page + 1;
    for (k = 0; k < sizeof sr_global_table / sizeof sr_global_table[0]; k++) {
        const SrGlobalInfo *g = &sr_global_table[k];
        uint32_t end = g->rva + g->elem_size * g->count;
        if (g->elem_size && g->count && g->rva <= rva + PAGE - 1 && rva < end && g->rva >= data_rva) {
            *first = (g->rva - data_rva) / PAGE;
            *last = (end - data_rva + PAGE - 1) / PAGE;
            return;
        }
    }
}

static void learn_page(SvVerify *v, uint32_t page)
{
    unsigned first, last, i;
    global_pages(page, &first, &last);
    for (i = 0; i < v->nranges; i++) {
        if (first < v->range[i][1] && v->range[i][0] < last) {  /* overlaps: merge */
            if (first < v->range[i][0])
                v->range[i][0] = first;
            if (last > v->range[i][1])
                v->range[i][1] = last;
            return;
        }
    }
    if (v->nranges < 32) {
        v->range[v->nranges][0] = first;
        v->range[v->nranges][1] = last;
        v->nranges++;
    } else {
        v->nranges = 0;  /* scope too large: go back to full rounds */
    }
}

static LONG CALLBACK on_fault(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    uint8_t *addr;
    uint32_t page;
    DWORD old;

    if (!g_active || er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || er->NumberParameters < 2 ||
        er->ExceptionInformation[0] != 1)
        return EXCEPTION_CONTINUE_SEARCH;
    addr = (uint8_t *)er->ExceptionInformation[1];
    if (addr < g_data || addr >= g_data + g_data_size)
        return EXCEPTION_CONTINUE_SEARCH;

    page = (uint32_t)((size_t)(addr - g_data) / PAGE);
    if (g_active->count < MAX_PAGES) {
        uint8_t *copy = pool_page();
        if (copy) {
            memcpy(copy, g_data + (size_t)page * PAGE, PAGE);
            g_active->index[g_active->count] = page;
            g_active->before[g_active->count] = copy;
            g_active->count++;
        } else {
            g_overflow = 1;
        }
    } else {
        g_overflow = 1;
    }
    VirtualProtect(g_data + (size_t)page * PAGE, PAGE, PAGE_READWRITE, &old);
    return EXCEPTION_CONTINUE_EXECUTION;
}

int sv_verify_init(void)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)sr_base;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(sr_base + dos->e_lfanew);
    const IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    HMODULE crt;
    int i;

    g_enabled = sv_config_int("verify", 1);
    g_log_calls = sv_config_int("log_calls", 1);
    g_full_every = sv_config_int("verify_full_every", 50);
    g_relax = sv_config_int("verify_relax", 200);
    if (g_full_every < 1)
        g_full_every = 1;
    sv_log("verification mode: %s", g_enabled ? "on" : "off");

    /* Find .data also when the verification is off: the memory snapshot (record.c) needs it. */
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (strncmp((const char *)sec->Name, g_section, IMAGE_SIZEOF_SHORT_NAME) == 0) {
            g_data = (uint8_t *)(sr_base + sec->VirtualAddress);
            g_data_size = (sec->Misc.VirtualSize + PAGE - 1) & ~(size_t)(PAGE - 1);
        }
    }
    if (!g_enabled)
        return 1;
    if (!g_data) {
        sv_log("ERROR verifier: section %s not found", g_section);
        g_enabled = 0;
        return 0;
    }

    crt = GetModuleHandleA("MSVCR100.dll");
    if (crt) {
        void *(__cdecl *getptd)(void) = (void *(__cdecl *)(void))(void *)GetProcAddress(crt, "_getptd");
        if (getptd)
            g_holdrand = (int *)((uint8_t *)getptd() + 0x14);
    }
    if (!g_holdrand)
        sv_log("warning verifier: rand() state not found; functions that call rand() will differ");

    g_pool_cap = MAX_PAGES * 3;  /* before (new), before (original), result of new */
    g_pool = (uint8_t *)VirtualAlloc(NULL, (size_t)g_pool_cap * PAGE, MEM_RESERVE, PAGE_READWRITE);
    if (!g_pool) {
        sv_log("ERROR verifier: no memory for %u pages", g_pool_cap);
        g_enabled = 0;
        return 0;
    }
    g_veh = AddVectoredExceptionHandler(1, on_fault);
    sv_log("verifier: .data at +0x%x, %u KB", (unsigned)((uintptr_t)g_data - sr_base),
           (unsigned)(g_data_size / 1024));
    return 1;
}

/* Ring of the last hooks called, for the crash report. */
static const char *g_recent[16];
static unsigned g_recent_pos;

void sv_note_call(const char *name)
{
    g_recent[g_recent_pos++ & 15] = name;
    sv_snapshot_check();
}

int sv_verify_data(const uint8_t **data, size_t *size)
{
    if (!g_data)
        return 0;
    *data = g_data;
    *size = g_data_size;
    return 1;
}

static LONG WINAPI on_unhandled(EXCEPTION_POINTERS *ep)
{
    static int reported;
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    uintptr_t at = (uintptr_t)er->ExceptionAddress;
    HMODULE mod = NULL;
    char modname[MAX_PATH] = "?";
    unsigned i;

    /* Vectored handler: it sees every exception first. Report only access violations
     * and similar errors, once, and let the normal handling continue. */
    if (reported || (er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
                     er->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION &&
                     er->ExceptionCode != EXCEPTION_STACK_OVERFLOW &&
                     er->ExceptionCode != EXCEPTION_INT_DIVIDE_BY_ZERO))
        return EXCEPTION_CONTINUE_SEARCH;
    reported = 1;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)at, &mod);
    if (mod)
        GetModuleFileNameA(mod, modname, MAX_PATH);
    sv_log("FATAL (first report): exception 0x%08lx at %s+0x%x.", er->ExceptionCode,
           strrchr(modname, 0x5c) ? strrchr(modname, 0x5c) + 1 : modname, (unsigned)(at - (uintptr_t)mod));
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
        sv_log("  %s access at 0x%08x", er->ExceptionInformation[0] ? "write" : "read",
               (unsigned)er->ExceptionInformation[1]);
    for (i = 0; i < 16; i++) {
        const char *n = g_recent[(g_recent_pos + i) & 15];
        if (n)
            sv_log("  recent hook: %s", n);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void sv_install_crash_report(void)
{
    /* Priority 0: after the page-guard handler, so the verifier faults never get here. */
    AddVectoredExceptionHandler(0, on_unhandled);
}

int sv_verify_busy(void)
{
    return g_active != NULL;
}

int sv_original_only(void)
{
    static int mode = -1;
    if (mode < 0)
        mode = sv_config_int("original_only", 0);
    return mode;
}

int sv_verify_should(SvVerify *v)
{
    v->calls++;
    if (!g_enabled)
        return 0;
    if (v->every <= 0) {
        char key[96];
        snprintf(key, sizeof key, "verify_every.%s", v->name);
        v->every = sv_config_int(key, v->every < 0 ? -v->every : 1);
        if (v->every < 1)
            v->every = 1;
    }
    /* A hook with many clean verifications needs fewer: after verify_relax (default 200)
     * checks with no difference, verify 1 call of every 64 (or every N, if N is larger). */
    if (v->checks >= g_relax && v->fails == 0 && v->every < 64)
        return (v->calls - 1) % 64 == 0;
    return (v->calls - 1) % v->every == 0;
}

void sv_track_begin_new(SvVerify *v)
{
    if (!g_freq.QuadPart)
        QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_t0);
    g_scope = v;
    g_full = v->nranges == 0 || v->checks < 3 || v->checks % g_full_every == 0 || v->recheck;
    g_pool_used = 0;
    g_overflow = 0;
    g_new.count = 0;
    g_orig.count = 0;
    if (g_holdrand)
        g_holdrand_before = *g_holdrand;
    if (g_heap)
        heap_begin();
    g_active = &g_new;
    protect_scope(PAGE_READONLY);
}

static int find_page(const PageSet *s, uint32_t page)
{
    uint32_t i;
    for (i = 0; i < s->count; i++)
        if (s->index[i] == page)
            return (int)i;
    return -1;
}

void sv_track_switch_to_original(void)
{
    uint32_t i;
    g_active = NULL;
    protect_scope(PAGE_READWRITE);

    /* Keep the result of the new version and put the pages back to the previous state. */
    for (i = 0; i < g_new.count; i++) {
        uint8_t *page = g_data + (size_t)g_new.index[i] * PAGE;
        g_new_result[i] = pool_page();
        if (g_new_result[i])
            memcpy(g_new_result[i], page, PAGE);
        else
            g_overflow = 1;
        memcpy(page, g_new.before[i], PAGE);
    }
    if (g_holdrand) {
        g_holdrand_new = *g_holdrand;
        *g_holdrand = g_holdrand_before;
    }
    if (g_heap)
        heap_after_new(g_scope);

    g_active = &g_orig;
    protect_scope(PAGE_READONLY);
}

static uint32_t g_ignore[16][2];  /* [rva, rva + size) */
static int g_nignore;

void sv_verify_ignore(uint32_t rva, uint32_t size)
{
    if (g_nignore < 16) {
        g_ignore[g_nignore][0] = rva;
        g_ignore[g_nignore][1] = rva + size;
        g_nignore++;
    }
}

static int ignored(uint32_t rva)
{
    int k;
    for (k = 0; k < g_nignore; k++)
        if (rva >= g_ignore[k][0] && rva < g_ignore[k][1])
            return 1;
    return 0;
}

/* Two normal floats with the same sign, at most g_ulps steps apart. The original keeps
 * x87 intermediate values with 64-bit precision in registers, the C version rounds them
 * at other places: the last bits can differ. subvulpes.ini verify_float_ulps (default 4,
 * 0 = exact). The verifier counts these words apart ("near floats"). */
static int g_ulps = -1;

static int near_float(uint32_t a, uint32_t b)
{
    uint32_t ea = (a >> 23) & 0xff, eb = (b >> 23) & 0xff;
    float x, y;
    double d, m;
    if (g_ulps < 0)
        g_ulps = sv_config_int("verify_float_ulps", 4);
    if (!g_ulps || ea == 0 || ea == 0xff || eb == 0 || eb == 0xff || (a ^ b) >> 31)
        return 0;
    if ((a > b ? a - b : b - a) <= (uint32_t)g_ulps)
        return 1;
    /* Value tolerance: an error that grows through a sum, or a tiny result of a subtraction of
     * near values (1e-9 off around 3e-7). Only for floats between 1e-12 and 1e12 in size:
     * pointers and large ints look like floats far outside this range (0x20001234 is 1e-19). */
    memcpy(&x, &a, 4);
    memcpy(&y, &b, 4);
    m = fabs(x) > fabs(y) ? fabs(x) : fabs(y);
    if (m < 1e-12 || m > 1e12 || fabs(x) < 1e-12 || fabs(y) < 1e-12)
        return 0;
    d = fabs((double)x - (double)y);
    return d <= 1e-6 || d <= 1e-5 * m;
}

int sv_same_float(double a, double b)
{
    float fa = (float)a, fb = (float)b;
    uint32_t x, y;
    if (fa == fb)
        return 1;
    memcpy(&x, &fa, 4);
    memcpy(&y, &fb, 4);
    if (fa != fa && fb != fb)
        return 1;  /* both NaN */
    return near_float(x, y);
}

/* Describe a .data address as "global[i]+0xNN". */
static void describe(uint32_t rva, char *out, size_t n)
{
    size_t k, best = (size_t)-1;
    for (k = 0; k < sizeof sr_global_table / sizeof sr_global_table[0]; k++) {
        if (sr_global_table[k].rva <= rva)
            best = k;
    }
    if (best != (size_t)-1) {
        const SrGlobalInfo *g = &sr_global_table[best];
        uint32_t off = rva - g->rva;
        if (g->elem_size && off < g->elem_size * g->count) {
            snprintf(out, n, "%s[%u]+0x%x", g->name, off / g->elem_size, off % g->elem_size);
            return;
        }
        if (off < 0x100000) {
            snprintf(out, n, "%s+0x%x", g->name, off);
            return;
        }
    }
    snprintf(out, n, "rva 0x%x", rva);
}

void sv_verify_end(SvVerify *v, int same_return, const char *detail)
{
    uint32_t i;
    long diffs = 0, nfloat = 0;
    char where[96];
    uint32_t data_rva = (uint32_t)((uintptr_t)g_data - sr_base);

    g_active = NULL;
    protect_scope(PAGE_READWRITE);

    /* Learn the written globals (for the next verifications of this hook). */
    for (i = 0; i < g_new.count; i++)
        learn_page(v, g_new.index[i]);
    for (i = 0; i < g_orig.count; i++)
        learn_page(v, g_orig.index[i]);

    /* Pages the new version changed: compare its result with the current state (from the original). */
    for (i = 0; i < g_new.count; i++) {
        const uint8_t *now = g_data + (size_t)g_new.index[i] * PAGE;
        const uint8_t *mine = g_new_result[i];
        uint32_t b;
        if (!mine)
            continue;
        for (b = 0; b < PAGE; b++) {
            if ((b & 3) == 0 && memcmp(mine + b, now + b, 4) != 0) {
                uint32_t x, y;
                memcpy(&x, mine + b, 4);
                memcpy(&y, now + b, 4);
                if (near_float(x, y)) {
                    nfloat++;
                    b += 3;
                    continue;
                }
            }
            if (mine[b] != now[b] && !ignored(data_rva + g_new.index[i] * PAGE + b)) {
                if (++diffs <= 8) {
                    describe(data_rva + g_new.index[i] * PAGE + b, where, sizeof where);
                    sv_log("  %s: new=%02x original=%02x", where, mine[b], now[b]);
                }
            }
        }
    }
    /* Pages only the original changed: the new version left the previous content. */
    for (i = 0; i < g_orig.count; i++) {
        const uint8_t *now, *before;
        uint32_t b;
        if (find_page(&g_new, g_orig.index[i]) >= 0)
            continue;
        now = g_data + (size_t)g_orig.index[i] * PAGE;
        before = g_orig.before[i];
        for (b = 0; b < PAGE; b++) {
            if (before[b] != now[b] && !ignored(data_rva + g_orig.index[i] * PAGE + b)) {
                if (++diffs <= 8) {
                    describe(data_rva + g_orig.index[i] * PAGE + b, where, sizeof where);
                    sv_log("  %s: new did not write, original=%02x", where, now[b]);
                }
            }
        }
    }

    if (g_heap)
        diffs += heap_compare(diffs, &nfloat);

    {
        int rand_ok = !g_holdrand || g_holdrand_new == *g_holdrand;
        int ok = diffs == 0 && same_return && rand_ok && !g_overflow && !v->crashed;
        LARGE_INTEGER t1;
        double ms;
        QueryPerformanceCounter(&t1);
        ms = 1000.0 * (double)(t1.QuadPart - g_t0.QuadPart) / (double)g_freq.QuadPart;
        v->checks++;
        if (g_full)
            v->recheck = 0;
        /* A memory difference in a scope round can come from a write outside the scope.
         * Do not count it. The next verification of this hook is a full round. */
        if (!ok && !g_full && diffs && same_return && rand_ok && !g_overflow && !v->crashed) {
            v->recheck = 1;
            v->unconfirmed++;
            if (v->unconfirmed <= 20)
                sv_log("VERIFY %s: difference in a scope round (call %ld, %ld bytes%s%s). The next round is full.",
                       v->name, v->calls, diffs, detail ? ", " : "", detail ? detail : "");
            ok = 1;
        }
        if (g_log_calls)
            sv_log("call %s #%ld: %s \"%s\" (%.1f ms, %s)", v->name, v->calls, ok ? "same" : "DIFFERENT",
                   detail ? detail : "", ms, g_full ? "full" : "scope");
        if (nfloat && !v->near_floats)
            sv_log("VERIFY %s: %ld floats differ in the last bits only (within verify_float_ulps)", v->name, nfloat);
        v->near_floats += nfloat;
        if (!ok) {
            v->fails++;
            /* A confirmed difference: the new version is wrong. From now on only the
             * original runs (as after a crash). verify_keep=1 in subvulpes.ini keeps the
             * new version, to collect more differences. */
            if (!sv_config_int("verify_keep", 0) && !v->broken) {
                v->broken = 1;
                sv_log("VERIFY %s: hook disabled after a difference, the original runs from now on", v->name);
            }
            if (v->fails <= 20)
                sv_log("VERIFY %s: DIFFERENT (call %ld): %ld bytes, return %s, rand %s%s%s%s%s", v->name,
                       v->calls, diffs, same_return ? "same" : "DIFFERENT", rand_ok ? "same" : "DIFFERENT",
                       g_overflow ? ", too many pages (raise MAX_PAGES)" : "", v->crashed ? ", CRASHED" : "",
                       detail ? ", " : "", detail ? detail : "");
        }
        v->crashed = 0;
        if (v->checks == 1 || v->checks % 1000 == 0)
            sv_log("VERIFY %s: %ld verified, %ld differences", v->name, v->checks, v->fails);
    }
}

int sv_crash_filter(SvVerify *v, EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    uintptr_t at = (uintptr_t)er->ExceptionAddress;
    HMODULE mod = NULL;
    char modname[MAX_PATH] = "?";

    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)at, &mod);
    if (mod)
        GetModuleFileNameA(mod, modname, MAX_PATH);
    sv_log("CRASH %s: exception 0x%08lx at %s+0x%x (call %ld). The hook now uses only the original.",
           v->name, er->ExceptionCode, strrchr(modname, 0x5c) ? strrchr(modname, 0x5c) + 1 : modname,
           (unsigned)(at - (uintptr_t)mod), v->calls);
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
        sv_log("  %s access at 0x%08x", er->ExceptionInformation[0] ? "write" : "read",
               (unsigned)er->ExceptionInformation[1]);
    v->broken = 1;
    v->crashed = 1;
    return EXCEPTION_EXECUTE_HANDLER;
}

void sv_verify_summary(const SvVerify *v)
{
    sv_log("SUMMARY %s: %ld calls, %ld verified, %ld differences%s", v->name, v->calls, v->checks, v->fails,
           v->broken ? ", BROKEN (crashed)" : "");
}
