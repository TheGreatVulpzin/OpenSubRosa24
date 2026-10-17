/* subvulpes-server: the dedicated server without subrosadedicated024d.exe.
 *
 * Phase 5.1 (docs/plano.md). The game code is the lifted C and the hand-written C. Its
 * memory is a region with the layout of the 024d image: SR_VA() and the g_ globals work as
 * with the hooks. The game imports (MSVCR100, WSOCK32, KERNEL32) come from the system DLLs.
 * Run it in the folder of a server (config.txt, data/, user/).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>

#include <float.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "winmm.lib")

#include "image_024d.h"

uintptr_t sr_base;  /* the image region (map_image) */
void sr_fn_init(void);
void lf_x87_init(void);
void __cdecl sr_getpaths(void);

/* ------------------------------------------------------------ console and log */

/* SV_LOG=<file>: a copy of the console output. The console stays visible, as with the
 * original server. Each line goes to the disk at once, so a crash loses no line. */
static FILE *g_log;

static void sv_out(const char *s)
{
    fputs(s, stdout);
    fflush(stdout);
    if (g_log) {
        fputs(s, g_log);
        fflush(g_log);
    }
}

void sv_log(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    memcpy(line, "[subvulpes] ", 12);
    vsnprintf(line + 12, sizeof line - 13, fmt, ap);
    va_end(ap);
    strcat(line, "\n");
    sv_out(line);
}

/* The printf of the game (port, socketenabled, master server). It replaces the printf of
 * MSVCR100, which writes to its own stdout and cannot copy the text to the log. */
static int __cdecl sv_printf(const char *fmt, ...)
{
    char text[1024];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    sv_out(text);
    return n;
}

static void open_log(void)
{
    const char *path = getenv("SV_LOG");
    if (path && !(g_log = fopen(path, "w")))
        sv_log("cannot open the log file %s", path);
}

/* ------------------------------------------------------------ imports */

/* A system DLL. Never the copy next to the exe: build/hook also holds the proxy wsock32.dll of
 * the hook build, and LoadLibrary("WSOCK32.dll") would load it (its hooks write over this
 * exe at 024d addresses). MSVCR100.dll can also come from the server folder. */
static HMODULE load_dll(const char *dll)
{
    HMODULE m = LoadLibraryExA(dll, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m && _stricmp(dll, "WSOCK32.dll") != 0)
        m = LoadLibraryA(dll);
    return m;
}

void *sr_import(const char *dll, const char *name)
{
    HMODULE m;
    void *p;
    if (!_stricmp(dll, "MSVCR100.dll") && !strcmp(name, "printf"))
        return (void *)sv_printf;
    m = load_dll(dll);
    p = m ? (void *)GetProcAddress(m, name) : NULL;
    if (!p) {
        sv_log("import not found: %s!%s", dll, name);
        ExitProcess(1);
    }
    return p;
}

void *sr_import_ordinal(const char *dll, unsigned ordinal)
{
    HMODULE m = load_dll(dll);
    void *p = m ? (void *)GetProcAddress(m, MAKEINTRESOURCEA(ordinal)) : NULL;
    if (!p) {
        sv_log("import not found: %s!#%u", dll, ordinal);
        ExitProcess(1);
    }
    return p;
}

/* The main loop calls this every minute. The hook build prints its per-hook profile here.
 * The standalone server prints the profiler of the game itself when SV_TIMERS is set: each
 * timer has a name at +0 and its RDTSC cycles of the last tick at +0x48 (stoptimer, RVA
 * 0xb3f0, adds to it, and the code that starts the timer sets it to 0 each tick). The
 * table is g_timer (RVA 0x14043a0, 256 records of 0x58 bytes). */
void sv_hooks_lifted_profile(void)
{
    int i;
    if (!getenv("SV_TIMERS"))
        return;
    for (i = 0; i < 256; i++) {
        const uint8_t *t = (const uint8_t *)sr_base + 0x14043a0 + i * 0x58;
        uint64_t last;
        if (!t[0])
            continue;
        memcpy(&last, t + 0x48, sizeof last);
        sv_log("  timer %-24.40s %8.2f Mcycles in the last tick", (const char *)t, last / 1e6);
    }
}

/* ------------------------------------------------------------ sampling profiler */

/* SV_SAMPLE=<file>: a thread reads the EIP of the main thread every millisecond and writes
 * one hex address per line. tools/lift/samples.py maps them to functions with the .map file. */
static HANDLE g_main_thread;
static FILE *g_samples;

static DWORD WINAPI sampler(void *arg)
{
    (void)arg;
    timeBeginPeriod(1);
    for (;;) {
        CONTEXT c;
        Sleep(1);
        if (SuspendThread(g_main_thread) == (DWORD)-1)
            continue;
        c.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(g_main_thread, &c))
            fprintf(g_samples, "%08lx\n", c.Eip);
        ResumeThread(g_main_thread);
    }
}

static void start_sampler(void)
{
    const char *path = getenv("SV_SAMPLE");
    if (!path || !(g_samples = fopen(path, "w")))
        return;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    fprintf(g_samples, "base %08lx\n", (unsigned long)(uintptr_t)GetModuleHandleA(NULL));
    CreateThread(NULL, 0, sampler, NULL, 0, NULL);
    sv_log("sampling profiler: %s", path);
}

/* ------------------------------------------------------------ image */

/* A region of SizeOfImage bytes, filled like the Windows loader fills the 024d: .rdata and
 * the initialized .data at their RVAs, the rest zero, and each image address moved by the
 * delta between the region and the file base 0x400000. */
static int map_image(void)
{
    unsigned i;
    uint8_t *base = VirtualAlloc(NULL, sr_image_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!base) {
        fprintf(stderr, "cannot allocate the image region (%u MiB, error %lu)\n", sr_image_size >> 20,
                GetLastError());
        return 0;
    }
    sr_base = (uintptr_t)base;
    memcpy(base + sr_image_rdata_rva, sr_image_rdata, sr_image_rdata_size);
    memcpy(base + sr_image_data_rva, sr_image_data, sr_image_data_size);
    for (i = 0; i < sr_image_reloc_count; i++)
        *(uint32_t *)(base + sr_image_relocs[i]) += (uint32_t)(sr_base - 0x400000u);
    return 1;
}

/* A crash: write where it happened, so that a test log is enough to find the line. The
 * address is given as module+offset (build/hook/subvulpes-server.map and .pdb), and as
 * an image RVA when it is inside the 024d region (a game address used as code or data). */
static LONG WINAPI on_crash(EXCEPTION_POINTERS *ep)
{
    const EXCEPTION_RECORD *er = ep->ExceptionRecord;
    uintptr_t at = (uintptr_t)er->ExceptionAddress;
    HMODULE m = NULL;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (const char *)at, &m))
        GetModuleFileNameA(m, name, sizeof name);
    sv_log("CRASH: exception %08lx at %s+0x%x, data address %08x", er->ExceptionCode,
           strrchr(name, '\\') ? strrchr(name, '\\') + 1 : name, (unsigned)(at - (uintptr_t)m),
           er->NumberParameters >= 2 ? (unsigned)er->ExceptionInformation[1] : 0);
    if (er->NumberParameters >= 2 && er->ExceptionInformation[1] >= sr_base &&
        er->ExceptionInformation[1] < sr_base + sr_image_size)
        sv_log("CRASH: the data address is image RVA 0x%x", (unsigned)(er->ExceptionInformation[1] - sr_base));
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(void)
{
    unsigned cw;
    open_log();
    SetUnhandledExceptionFilter(on_crash);
    if (!map_image())
        return 1;
    sr_fn_init();
    /* The CRT of the 024d sets 53-bit x87 precision at start-up (_setdefaultprecision). */
    _controlfp_s(&cw, _PC_53, _MCW_PC);
    lf_x87_init();
    sv_log("standalone server: image at %p", (void *)sr_base);
    start_sampler();
    sr_getpaths();  /* never returns: it runs the main loop */
    return 0;
}
