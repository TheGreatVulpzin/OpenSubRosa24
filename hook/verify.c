/* Generic verification by page protection. See verify.h. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "hook.h"
#include "sr_global_table.h"
#include "sr_runtime.h"
#include "verify.h"

#define PAGE 4096u
#define MAX_PAGES 4096   /* pages changed per run and per version (16 MB) */

static uint8_t *g_data;        /* start of the game .data section */
static size_t g_data_size;
static int g_enabled;          /* subvulpes.ini: verify */
static int g_log_calls;        /* subvulpes.ini: log_calls */
static int g_full_every;       /* subvulpes.ini: verify_full_every */
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

/* Simple pool of copy pages (reused between calls). */
static uint8_t *g_pool;
static uint32_t g_pool_used, g_pool_cap;

static uint8_t *pool_page(void)
{
    if (g_pool_used == g_pool_cap)
        return NULL;
    return g_pool + (size_t)(g_pool_used++) * PAGE;
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
        VirtualProtect(g_data + (size_t)g_scope->range[i][0] * PAGE,
                       (size_t)(g_scope->range[i][1] - g_scope->range[i][0]) * PAGE, prot, &old);
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
    if (g_full_every < 1)
        g_full_every = 1;
    sv_log("verification mode: %s", g_enabled ? "on" : "off");
    if (!g_enabled)
        return 1;

    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (strncmp((const char *)sec->Name, g_section, IMAGE_SIZEOF_SHORT_NAME) == 0) {
            g_data = (uint8_t *)(sr_base + sec->VirtualAddress);
            g_data_size = (sec->Misc.VirtualSize + PAGE - 1) & ~(size_t)(PAGE - 1);
        }
    }
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
    g_pool = (uint8_t *)VirtualAlloc(NULL, (size_t)g_pool_cap * PAGE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
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
    return (v->calls - 1) % v->every == 0;
}

void sv_track_begin_new(SvVerify *v)
{
    if (!g_freq.QuadPart)
        QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_t0);
    g_scope = v;
    g_full = v->nranges == 0 || v->checks < 3 || v->checks % g_full_every == 0;
    g_pool_used = 0;
    g_overflow = 0;
    g_new.count = 0;
    g_orig.count = 0;
    if (g_holdrand)
        g_holdrand_before = *g_holdrand;
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

    g_active = &g_orig;
    protect_scope(PAGE_READONLY);
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
    long diffs = 0;
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
            if (mine[b] != now[b]) {
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
            if (before[b] != now[b]) {
                if (++diffs <= 8) {
                    describe(data_rva + g_orig.index[i] * PAGE + b, where, sizeof where);
                    sv_log("  %s: new did not write, original=%02x", where, now[b]);
                }
            }
        }
    }

    {
        int rand_ok = !g_holdrand || g_holdrand_new == *g_holdrand;
        int ok = diffs == 0 && same_return && rand_ok && !g_overflow && !v->crashed;
        LARGE_INTEGER t1;
        double ms;
        QueryPerformanceCounter(&t1);
        ms = 1000.0 * (double)(t1.QuadPart - g_t0.QuadPart) / (double)g_freq.QuadPart;
        v->checks++;
        if (g_log_calls)
            sv_log("call %s #%ld: %s \"%s\" (%.1f ms, %s)", v->name, v->calls, ok ? "same" : "DIFFERENT",
                   detail ? detail : "", ms, g_full ? "full" : "scope");
        if (!ok) {
            v->fails++;
            if (v->fails <= 20)
                sv_log("VERIFY %s: DIFFERENT (call %ld): %ld bytes, return %s, rand %s%s%s", v->name,
                       v->calls, diffs, same_return ? "same" : "DIFFERENT", rand_ok ? "same" : "DIFFERENT",
                       g_overflow ? ", too many pages (raise MAX_PAGES)" : "", v->crashed ? ", CRASHED" : "");
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
