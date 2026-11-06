/* Standalone client, level 1: the lifted C of subrosa.exe runs in its own image region.
 *
 * The region has the size of the image of the game. Start-up copies .rdata and the initial
 * .data into it and adds the relocation delta. The other part of .data is zero memory.
 * Calls between game functions go through sr_fn, which finds the C function of a game
 * address. Imports come from the DLLs that the game uses. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_client.h"
#include "lifted_table.h"

uintptr_t sr_base;

/* The OpenGL slots of the game (build/client_image/client_gl.c). */
void client_gl_init(void);

#define IMAGE_BASE 0x400000u

/* ------------------------------------------------------------ imports */

void *sr_import(const char *dll, const char *name)
{
    HMODULE module = LoadLibraryA(dll);
    void *fn = module ? (void *)GetProcAddress(module, name) : NULL;
    if (!fn) {
        fprintf(stderr, "import %s!%s not found\n", dll, name);
        exit(1);
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

/* ------------------------------------------------------------ functions */

/* SR_TRACE=1 prints each call that goes through sr_fn. */
static int g_trace;

/* lf_table is sorted by address. */
void *sr_fn(uintptr_t va)
{
    uint32_t rva = (uint32_t)(va - IMAGE_BASE);
    int lo = 0, hi = lf_table_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (lf_table[mid].rva == rva) {
            if (g_trace)
                fprintf(stderr, "call %s (0x%x)\n", lf_table[mid].name, (unsigned)rva);
            return lf_table[mid].lifted;
        }
        if (lf_table[mid].rva < rva)
            lo = mid + 1;
        else
            hi = mid - 1;
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
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char **argv)
{
    unsigned cw;
    int run = argc > 1 && !strcmp(argv[1], "--run");

    setvbuf(stdout, NULL, _IONBF, 0);
    g_trace = getenv("SR_TRACE") != NULL;
    SetUnhandledExceptionFilter(crash_filter);

    /* The game uses 53 bit x87 math. */
    _controlfp_s(&cw, _PC_53, _MCW_PC);
    if (!image_init())
        return 1;
    client_gl_init();
    printf("image at %p, %d C functions\n", (void *)sr_base, lf_table_count);
    if (run) {
        /* The entry of the game (RVA 0x10ae00) takes no arguments here. */
        ((void (__cdecl *)(void))sr_fn(IMAGE_BASE + 0x10ae00))();
    }
    return 0;
}
