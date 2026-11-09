/* Standalone client, level 1: the lifted C of subrosa.exe runs in its own image region.
 *
 * The region has the size of the image of the game. Start-up copies .rdata and the initial
 * .data into it and adds the relocation delta. The other part of .data is zero memory.
 * Calls between game functions go through sr_fn, which finds the C function of a game
 * address. Imports come from the DLLs that the game uses. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>

#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_client.h"
#include "lifted_table.h"
#include "sr_math.h"

uintptr_t sr_base;

/* The OpenGL slots of the game (build/client_image/client_gl.c). */
void client_gl_init(void);

/* The static C runtime (client_crt.c). */
void *client_crt_lookup(const char *name);

#define IMAGE_BASE 0x400000u

/* ------------------------------------------------------------ imports */

/* Frame timing, printed once a second by the sampler when SR_SAMPLE is set. The wrappers add the
 * microseconds spent in the swap, in glFinish and in the delay of the frame pacing. */
static volatile LONG g_frames, g_us_swap, g_us_finish, g_us_delay, g_delays;
static void (__cdecl *real_swap)(void *);
static void (__cdecl *real_delay)(unsigned);
static void (__stdcall *real_finish)(void);

static LONG elapsed_us(LARGE_INTEGER t0)
{
    LARGE_INTEGER t1, f;
    QueryPerformanceCounter(&t1);
    QueryPerformanceFrequency(&f);
    return (LONG)((t1.QuadPart - t0.QuadPart) * 1000000 / f.QuadPart);
}

static void __cdecl timed_swap(void *window)
{
    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    real_swap(window);
    InterlockedExchangeAdd(&g_us_swap, elapsed_us(t0));
    InterlockedIncrement(&g_frames);
}

static void __cdecl timed_delay(unsigned ms)
{
    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    real_delay(ms);
    InterlockedExchangeAdd(&g_us_delay, elapsed_us(t0));
    InterlockedIncrement(&g_delays);
}

static void __stdcall timed_finish(void)
{
    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    real_finish();
    InterlockedExchangeAdd(&g_us_finish, elapsed_us(t0));
}

void *sr_import(const char *dll, const char *name)
{
    HMODULE module;
    void *fn = NULL;
    if (!_stricmp(dll, "MSVCR100.DLL"))
        fn = client_crt_lookup(name);
    if (fn)
        return fn;
    if (!_stricmp(dll, "MSVCR100.DLL"))
        fprintf(stderr, "import %s!%s is not linked, using the DLL\n", dll, name);
    module = LoadLibraryA(dll);
    fn = module ? (void *)GetProcAddress(module, name) : NULL;
    if (!fn) {
        fprintf(stderr, "import %s!%s not found\n", dll, name);
        exit(1);
    }
    if (getenv("SR_SAMPLE")) {
        if (!_stricmp(dll, "SDL2.DLL") && !strcmp(name, "SDL_GL_SwapWindow")) {
            real_swap = (void (__cdecl *)(void *))fn;
            return (void *)timed_swap;
        }
        if (!_stricmp(dll, "SDL2.DLL") && !strcmp(name, "SDL_Delay")) {
            real_delay = (void (__cdecl *)(unsigned))fn;
            return (void *)timed_delay;
        }
        if (!_stricmp(dll, "OPENGL32.DLL") && !strcmp(name, "glFinish")) {
            real_finish = (void (__stdcall *)(void))fn;
            return (void *)timed_finish;
        }
    }
    return fn;
}

void *sr_import_ordinal(const char *dll, unsigned ordinal)
{
    HMODULE module = LoadLibraryA(dll);
    void *fn = module ? (void *)GetProcAddress(module, (LPCSTR)(uintptr_t)ordinal) : NULL;
    if (!fn) {
        fprintf(stderr, "import %s!#%u not found\n", dll, ordinal);
        exit(1);
    }
    return fn;
}

/* The menu buttons that open a web page (Twitter, CrypticSea, the driver sites). */
#pragma comment(lib, "shell32.lib")
void sr_open_url(const char *url)
{
    ShellExecuteA(NULL, "open", url, "", "", SW_SHOWNORMAL);
}

/* ------------------------------------------------------------ functions */

/* SR_TRACE=1 prints each call that goes through sr_fn. SR_TRACE=2 prints the first call of each function. */
static int g_trace;
static unsigned char g_seen[8192];

/* Open addressing hash of the table, filled at start-up. Each slot holds the table index plus one. */
#define FN_HASH_SIZE 8192u
static uint16_t g_fn_hash[FN_HASH_SIZE];

static unsigned fn_slot(uint32_t rva)
{
    return (rva * 2654435761u) >> 19;
}

/* One pointer per byte of the code range: most calls need one load. */
static void **g_fn_direct;
static uint32_t g_fn_direct_size;

static void fn_hash_init(void)
{
    int i;
    for (i = 0; i < lf_table_count; ++i) {
        unsigned s = fn_slot(lf_table[i].rva);
        while (g_fn_hash[s])
            s = (s + 1) & (FN_HASH_SIZE - 1);
        g_fn_hash[s] = (uint16_t)(i + 1);
        if (lf_table[i].rva >= g_fn_direct_size)
            g_fn_direct_size = lf_table[i].rva + 1;
    }
    g_fn_direct = (void **)calloc(g_fn_direct_size, sizeof(void *));
    if (!g_fn_direct)
        g_fn_direct_size = 0;
}

void *sr_fn(uintptr_t va)
{
    uint32_t rva = (uint32_t)(va - IMAGE_BASE);
    unsigned s;
    if (rva < g_fn_direct_size && g_fn_direct[rva])
        return g_fn_direct[rva];
    s = fn_slot(rva);
    while (g_fn_hash[s]) {
        int i = g_fn_hash[s] - 1;
        if (lf_table[i].rva == rva) {
            if (g_trace && (g_trace == 1 || !g_seen[i])) {
                g_seen[i] = 1;
                fprintf(stderr, "call %s (0x%x)\n", lf_table[i].name, (unsigned)rva);
            }
            if (!g_trace && rva < g_fn_direct_size)
                g_fn_direct[rva] = lf_table[i].lifted;
            return lf_table[i].lifted;
        }
        s = (s + 1) & (FN_HASH_SIZE - 1);
    }
    fprintf(stderr, "no C function for RVA 0x%x\n", (unsigned)rva);
    exit(1);
}

/* ------------------------------------------------------------ image */

static int image_init(void)
{
    unsigned i;
    uint8_t *base = VirtualAlloc((void *)0x20000000, sr_image_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!base)
        base = VirtualAlloc(NULL, sr_image_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!base) {
        fprintf(stderr, "cannot allocate the image (%lu)\n", GetLastError());
        return 0;
    }
    sr_base = (uintptr_t)base;
    memcpy(base + sr_image_rdata_rva, sr_image_rdata, sr_image_rdata_size);
    memcpy(base + sr_image_data_rva, sr_image_data, sr_image_data_size);
    for (i = 0; i < sr_image_reloc_count; ++i)
        *(uint32_t *)(base + sr_image_relocs[i]) += (uint32_t)(sr_base - IMAGE_BASE);
    return 1;
}

/* The menu items of the game: 0x284 bytes each from 0x5750d00, the count at 0x9465240. A crash in
 * the menu code prints the items with their name, kind and text pointer. */
static void menu_dump(void)
{
    const uint32_t items = 0x5750d00u - IMAGE_BASE, count_at = 0x9465240u - IMAGE_BASE;
    uint32_t count, i;
    if (IsBadReadPtr((void *)(sr_base + count_at), 4))
        return;
    count = *(uint32_t *)(sr_base + count_at);
    fprintf(stderr, "menu items: %u\n", (unsigned)count);
    for (i = 0; i < count && i < 400; ++i) {
        const uint8_t *it = (const uint8_t *)(sr_base + items + i * 0x284);
        if (IsBadReadPtr(it, 0x284))
            break;
        fprintf(stderr, "  item %u name \"%.24s\" kind %u value kind %u text pointer 0x%08x word 0x200 0x%08x word 0x204 0x%08x\n",
                (unsigned)i, (const char *)(it + 4), *(const uint32_t *)it, *(const uint32_t *)(it + 0x2c),
                *(const uint32_t *)(it + 0x7c), *(const uint32_t *)(it + 0x200), *(const uint32_t *)(it + 0x204));
    }
}

/* SR_DENORM=1: the x87 exception for a denormal operand is on. A denormal makes the x87 unit very slow
 * (a small integer held in a float). The handler counts the address of each hit, masks the exception
 * to let the instruction finish, and the sampler thread turns the exception on again. */
static int g_denorm;
static struct { uintptr_t eip; unsigned n; } g_dn[512];
static volatile LONG g_dn_total;
static uintptr_t g_dn_last;
static int g_dn_repeat;

/* Only bit 1 of the x87 control word (the denormal mask). The CRT call changes the other masks too. */
static void x87_denormal_mask(int masked)
{
    unsigned short w;
    __asm { fnstcw w }
    w = masked ? (unsigned short)(w | 2) : (unsigned short)(w & ~2);
    __asm { fldcw w }
}

static LONG CALLBACK denorm_handler(EXCEPTION_POINTERS *info)
{
    unsigned s;
    uintptr_t at;
    /* The step after the masked instruction: turn the exception on again. */
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        x87_denormal_mask(0);
        g_dn_repeat = 0;
        info->ContextRecord->EFlags &= ~0x100u;
        if (info->ContextRecord->ContextFlags & CONTEXT_FLOATING_POINT)
            info->ContextRecord->FloatSave.ControlWord &= ~2u;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    /* Windows reports some denormal operands as "invalid operation". A real invalid operation
     * repeats at the same address after the retry: it goes to the next handler. */
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_FLT_DENORMAL_OPERAND &&
        info->ExceptionRecord->ExceptionCode != EXCEPTION_FLT_INVALID_OPERATION)
        return EXCEPTION_CONTINUE_SEARCH;
    at = (uintptr_t)info->ExceptionRecord->ExceptionAddress;
    if (at == g_dn_last && ++g_dn_repeat > 3)
        return EXCEPTION_CONTINUE_SEARCH;
    if (at != g_dn_last)
        g_dn_repeat = 0;
    g_dn_last = at;
    s = (unsigned)((at >> 1) * 2654435761u) >> 23;
    while (g_dn[s].eip && g_dn[s].eip != at)
        s = (s + 1) & 511;
    g_dn[s].eip = at;
    g_dn[s].n++;
    InterlockedIncrement(&g_dn_total);
    _clearfp();
    x87_denormal_mask(1);
    if (info->ContextRecord->ContextFlags & CONTEXT_FLOATING_POINT) {
        info->ContextRecord->FloatSave.ControlWord |= 2;
        info->ContextRecord->FloatSave.StatusWord &= ~0x80ffu;
    }
    info->ContextRecord->EFlags |= 0x100u;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void denorm_report(void)
{
    unsigned i, j, shown;
    HMODULE self = GetModuleHandleA(NULL);
    fprintf(stderr, "denormal hits: %ld\n", g_dn_total);
    for (shown = 0; shown < 12; ++shown) {
        unsigned best = 0;
        for (i = 0, j = 0; i < 512; ++i)
            if (g_dn[i].n > g_dn[j].n)
                j = i;
        best = g_dn[j].n;
        if (!best)
            break;
        fprintf(stderr, "  exe+0x%x: %u\n", (unsigned)(g_dn[j].eip - (uintptr_t)self), best);
        g_dn[j].n = 0;
    }
}

/* SR_SAMPLE=<file>: a thread reads the EIP of the main thread every millisecond. An address in the
 * exe goes to the file as hex. An address in a DLL (the OpenGL driver, OpenAL) goes as "dll:<name>".
 * instrumentation/lift/samples.py counts them by function with the .map file. */
#pragma comment(lib, "winmm.lib")
static HANDLE g_main_thread;
static volatile LONG g_sampler_stop;
static FILE *g_samples;

static DWORD WINAPI sampler(void *arg)
{
    unsigned count = 0;
    HMODULE self = GetModuleHandleA(NULL);
    (void)arg;
    timeBeginPeriod(1);
    while (!InterlockedCompareExchange(&g_sampler_stop, 0, 0)) {
        CONTEXT c;
        int got;
        uintptr_t stack[512], caller = 0;
        Sleep(1);
        if (SuspendThread(g_main_thread) == (DWORD)-1)
            continue;
        c.ContextFlags = CONTEXT_CONTROL;
        got = GetThreadContext(g_main_thread, &c);
        if (got) {
            /* The first stack word that points into the exe is the caller of a DLL function. */
            __try {
                size_t i;
                memcpy(stack, (void *)c.Esp, sizeof stack);
                for (i = 0; i < 512; ++i)
                    if (stack[i] > (uintptr_t)self && stack[i] < (uintptr_t)self + 0x400000) {
                        caller = stack[i];
                        break;
                    }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
        /* Resume first. The main thread may hold the loader lock, and the calls below need it. */
        ResumeThread(g_main_thread);
        if (got) {
            HMODULE m = NULL;
            char name[MAX_PATH];
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(uintptr_t)c.Eip, &m);
            if (m == self || !m || !GetModuleFileNameA(m, name, sizeof name))
                fprintf(g_samples, "%08lx\n", c.Eip);
            else
                fprintf(g_samples, "dll:%s@%08lx\n", strrchr(name, '\\') ? strrchr(name, '\\') + 1 : name,
                        (unsigned long)caller);
        }
        if (++count % 1000 == 0) {
            static DWORD last;
            DWORD now = timeGetTime();
            fflush(g_samples);
            fprintf(stderr, "in %lu ms: frames %ld, swap %ld ms, glFinish %ld ms, delay %ld ms in %ld calls\n",
                    (unsigned long)(last ? now - last : 0), InterlockedExchange(&g_frames, 0),
                    InterlockedExchange(&g_us_swap, 0) / 1000,
                    InterlockedExchange(&g_us_finish, 0) / 1000, InterlockedExchange(&g_us_delay, 0) / 1000,
                    InterlockedExchange(&g_delays, 0));
            last = now;
            if (g_denorm && count % 10000 == 0)
                denorm_report();
        }
    }
    return 0;
}

static void start_sampler(void)
{
    const char *path = getenv("SR_SAMPLE");
    if (!path || !(g_samples = fopen(path, "w")))
        return;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    fprintf(g_samples, "base %08lx\n", (unsigned long)(uintptr_t)GetModuleHandleA(NULL));
    CreateThread(NULL, 0, sampler, NULL, 0, NULL);
}

/* A crash prints where it happened. The map file of the exe turns the offset into a function. */
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *info)
{
    EXCEPTION_RECORD *r = info->ExceptionRecord;
    uintptr_t at = (uintptr_t)r->ExceptionAddress;
    HMODULE self = GetModuleHandleA(NULL);
    fprintf(stderr, "exception 0x%08lx at %p (exe+0x%x)", r->ExceptionCode, r->ExceptionAddress,
            (unsigned)(at - (uintptr_t)self));
    if (r->NumberParameters >= 2)
        fprintf(stderr, ", %s %p", r->ExceptionInformation[0] ? "write of" : "read of",
                (void *)r->ExceptionInformation[1]);
    if (at >= sr_base && at < sr_base + sr_image_size)
        fprintf(stderr, ", code in the image region, RVA 0x%x", (unsigned)(at - sr_base));
    fprintf(stderr, "\n");
    /* The stack words that point into the exe are return addresses. Map them with the .map file. */
    {
        uintptr_t *sp = (uintptr_t *)info->ContextRecord->Esp;
        int shown = 0, i;
        fprintf(stderr, "eip 0x%08lx ebp 0x%08lx, return addresses (exe+offset):", info->ContextRecord->Eip,
                info->ContextRecord->Ebp);
        for (i = 0; i < 400 && shown < 24; ++i) {
            uintptr_t w;
            if (IsBadReadPtr(sp + i, sizeof w))
                break;
            w = sp[i];
            if (w > (uintptr_t)self && w < (uintptr_t)self + 0x400000) {
                fprintf(stderr, " %x", (unsigned)(w - (uintptr_t)self));
                ++shown;
            }
        }
        fprintf(stderr, "\n");
    }
    menu_dump();
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

/* CL_FN of the clean client code (src/client/client.h). */
extern void *(*cl_fn_hook)(unsigned va);
static void *cl_fn_lookup(unsigned va)
{
    return sr_fn((uintptr_t)va);
}

int main(int argc, char **argv)
{
    unsigned cw;
    int run = argc > 1 && !strcmp(argv[1], "--run");

    setvbuf(stdout, NULL, _IONBF, 0);
    g_trace = getenv("SR_TRACE") ? atoi(getenv("SR_TRACE")) : 0;
    SetUnhandledExceptionFilter(crash_filter);

    /* The game uses 53 bit x87 math. */
    _controlfp_s(&cw, _PC_53, _MCW_PC);
    if (!image_init())
        return 1;
    fn_hash_init();
    x87_init();
    cl_fn_hook = cl_fn_lookup;
    client_gl_init();
    printf("image at %p, %d C functions\n", (void *)sr_base, lf_table_count);
    if (run) {
        if (getenv("SR_DENORM") && getenv("SR_SAMPLE")) {
            g_denorm = 1;
            AddVectoredExceptionHandler(1, denorm_handler);
            x87_denormal_mask(0);
            atexit(denorm_report);
        }
        start_sampler();
        /* The entry of the game (RVA 0x10ae00) takes no arguments here. */
        ((void (__cdecl *)(void))sr_fn(IMAGE_BASE + 0x10ae00))();
    }
    return 0;
}
