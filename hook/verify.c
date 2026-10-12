/* Verificação genérica por proteção de páginas. Ver verify.h. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "hook.h"
#include "sr_global_table.h"
#include "sr_runtime.h"
#include "verify.h"

#define PAGE 4096u
#define MAX_PAGES 4096   /* páginas alteradas por execução e por versão (16 MB) */

static uint8_t *g_data;        /* início da seção .data do jogo */
static size_t g_data_size;
static int g_enabled;          /* subvulpes.ini: verify */
static int g_log_calls;        /* subvulpes.ini: log_calls */
static PVOID g_veh;
static const char *g_section = ".data";  /* os testes trocam por uma seção própria */

void sv_verify_set_section(const char *name)
{
    g_section = name;
}

/* rand() do MSVCR100: o estado fica em _getptd()->_holdrand (+0x14). */
static int *g_holdrand;

typedef struct PageSet {
    uint32_t count;
    uint32_t index[MAX_PAGES];       /* número da página dentro de .data */
    uint8_t *before[MAX_PAGES];      /* conteúdo antes da primeira escrita */
} PageSet;

static PageSet g_new, g_orig;
static PageSet *g_active;           /* conjunto que recebe as falhas agora */
static uint8_t *g_new_result[MAX_PAGES];  /* páginas da versão nova, depois de rodar */
static int g_overflow;
static int g_holdrand_before, g_holdrand_new;

/* Pool simples de páginas de cópia (reaproveitado entre chamadas). */
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
        sv_log("ERRO verificador: VirtualProtect da .data falhou (%lu)", GetLastError());
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
    sv_log("modo de verificação: %s", g_enabled ? "ligado" : "desligado");
    if (!g_enabled)
        return 1;

    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (strncmp((const char *)sec->Name, g_section, IMAGE_SIZEOF_SHORT_NAME) == 0) {
            g_data = (uint8_t *)(sr_base + sec->VirtualAddress);
            g_data_size = (sec->Misc.VirtualSize + PAGE - 1) & ~(size_t)(PAGE - 1);
        }
    }
    if (!g_data) {
        sv_log("ERRO verificador: seção %s não encontrada", g_section);
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
        sv_log("aviso verificador: estado do rand() não encontrado; funções com rand() vão divergir");

    g_pool_cap = MAX_PAGES * 3;  /* antes (nova), antes (original), resultado da nova */
    g_pool = (uint8_t *)VirtualAlloc(NULL, (size_t)g_pool_cap * PAGE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_pool) {
        sv_log("ERRO verificador: sem memória para %u páginas", g_pool_cap);
        g_enabled = 0;
        return 0;
    }
    g_veh = AddVectoredExceptionHandler(1, on_fault);
    sv_log("verificador: .data em +0x%x, %u KB", (unsigned)((uintptr_t)g_data - sr_base),
           (unsigned)(g_data_size / 1024));
    return 1;
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

void sv_track_begin_new(void)
{
    g_pool_used = 0;
    g_overflow = 0;
    g_new.count = 0;
    g_orig.count = 0;
    if (g_holdrand)
        g_holdrand_before = *g_holdrand;
    g_active = &g_new;
    protect_all(PAGE_READONLY);
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
    protect_all(PAGE_READWRITE);

    /* Guarda o resultado da versão nova e devolve as páginas ao estado anterior. */
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
    protect_all(PAGE_READONLY);
}

/* Descreve um endereço de .data como "global[i]+0xNN". */
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
    protect_all(PAGE_READWRITE);

    /* Páginas que a nova alterou: compara o resultado dela com o estado atual (da original). */
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
                    sv_log("  %s: nova=%02x original=%02x", where, mine[b], now[b]);
                }
            }
        }
    }
    /* Páginas que só a original alterou: a nova deixou o conteúdo anterior. */
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
                    sv_log("  %s: nova não escreveu, original=%02x", where, now[b]);
                }
            }
        }
    }

    {
        int rand_ok = !g_holdrand || g_holdrand_new == *g_holdrand;
        int ok = diffs == 0 && same_return && rand_ok && !g_overflow;
        v->checks++;
        if (g_log_calls)
            sv_log("chamada %s #%ld: %s \"%s\"", v->name, v->calls, ok ? "igual" : "DIFERENTE",
                   detail ? detail : "");
        if (!ok) {
            v->fails++;
            if (v->fails <= 20)
                sv_log("VERIFICA %s: DIFERENTE (chamada %ld): %ld bytes, retorno %s, rand %s%s", v->name,
                       v->calls, diffs, same_return ? "igual" : "DIFERENTE", rand_ok ? "igual" : "DIFERENTE",
                       g_overflow ? ", páginas demais (aumente MAX_PAGES)" : "");
        }
        if (v->checks == 1 || v->checks % 1000 == 0)
            sv_log("VERIFICA %s: %ld verificadas, %ld diferenças", v->name, v->checks, v->fails);
    }
}

void sv_verify_summary(const SvVerify *v)
{
    sv_log("RESUMO %s: %ld chamadas, %ld verificadas, %ld diferenças", v->name, v->calls, v->checks, v->fails);
}
