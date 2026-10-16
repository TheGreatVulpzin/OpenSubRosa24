/* subvulpes-server: the dedicated server without subrosadedicated024d.exe.
 *
 * Phase 5.1 (docs/plano.md). The game code is the lifted C and the hand-written C. Its
 * memory is a region with the layout of the 024d image: SR_VA() and the g_ globals work as
 * with the hooks. The game imports (MSVCR100, WSOCK32, KERNEL32) come from the system DLLs.
 * Run it in the folder of a server (config.txt, data/, user/).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <float.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "image_024d.h"

uintptr_t sr_base;  /* the image region (map_image) */
void sr_fn_init(void);
void lf_x87_init(void);
void __cdecl sr_getpaths(void);

/* ------------------------------------------------------------ imports */

void *sr_import(const char *dll, const char *name)
{
    HMODULE m = LoadLibraryA(dll);
    void *p = m ? (void *)GetProcAddress(m, name) : NULL;
    if (!p) {
        fprintf(stderr, "import not found: %s!%s\n", dll, name);
        ExitProcess(1);
    }
    return p;
}

void *sr_import_ordinal(const char *dll, unsigned ordinal)
{
    HMODULE m = LoadLibraryA(dll);
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
    sr_getpaths();  /* never returns: it runs the main loop */
    return 0;
}
