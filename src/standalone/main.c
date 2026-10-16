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
    HMODULE m = load_dll(dll);
    void *p = m ? (void *)GetProcAddress(m, name) : NULL;
    if (!p) {
        fprintf(stderr, "import not found: %s!%s\n", dll, name);
        ExitProcess(1);
    }
    return p;
}

void *sr_import_ordinal(const char *dll, unsigned ordinal)
{
    HMODULE m = load_dll(dll);
    void *p = m ? (void *)GetProcAddress(m, MAKEINTRESOURCEA(ordinal)) : NULL;
    if (!p) {
        fprintf(stderr, "import not found: %s!#%u\n", dll, ordinal);
        ExitProcess(1);
    }
    return p;
}

/* ------------------------------------------------------------ log */

void sv_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("[subvulpes] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* The hook build prints the per-hook profile here. The standalone server has no hooks. */
void sv_hooks_lifted_profile(void)
{
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

int main(void)
{
    unsigned cw;
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
