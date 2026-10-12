/* Record real inputs for the offline differential test (src/tests/harness.c).
 *
 * subvulpes.ini:
 *   record_args=1          the thunks of the lifted hooks call sv_record_args
 *   snapshot_after=60      seconds after start: write the .data section to sv_snapshot.bin
 *
 * sv_args.txt, one line per recorded call (up to RECORD_MAX different calls per function):
 *   R <rva> <arg> <arg> ...
 * Each argument (one dword) has a kind:
 *   V:xxxxxxxx   a plain value
 *   M:xxxxxxxx   an address inside the 024d image, as an RVA (the harness relocates it)
 *   P:hex...     a pointer to the stack or the heap: the 256 bytes it points to
 *
 * sv_snapshot.bin: header (magic, image base, .data RVA, .data size, x87 control word),
 * then each .data page that is not all zero: page index (uint32) + 4096 bytes.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "hook.h"
#include "record.h"
#include "sr_runtime.h"
#include "verify.h"

#define RECORD_MAX 24
#define RECORD_FUNCS 1024
#define BLOB 256

static int g_record = -1;
static FILE *g_args;
static uint32_t g_seen[RECORD_FUNCS][RECORD_MAX];
static unsigned char g_count[RECORD_FUNCS];
static uint32_t g_image_size;

static uint32_t hash_args(const uint32_t *a, int n)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < n; i++)
        h = (h ^ a[i]) * 16777619u;
    return h ? h : 1;
}

static int readable(const void *p, size_t n)
{
    MEMORY_BASIC_INFORMATION mi;
    if (!VirtualQuery(p, &mi, sizeof mi) || mi.State != MEM_COMMIT)
        return 0;
    if (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
        return 0;
    return (const char *)p + n <= (const char *)mi.BaseAddress + mi.RegionSize;
}

static void exe_path(char *out, size_t n, const char *file)
{
    char *slash;
    GetModuleFileNameA(NULL, out, (DWORD)n);
    slash = strrchr(out, '\\');
    if (slash)
        slash[1] = 0;
    strncat(out, file, n - strlen(out) - 1);
}

void sv_record_args(uint32_t rva, int index, const uint32_t *args, int ndw)
{
    uint32_t h;
    int i, k;
    if (g_record < 0) {
        char path[MAX_PATH];
        IMAGE_NT_HEADERS32 *nt;
        g_record = sv_config_int("record_args", 0);
        if (g_record) {
            exe_path(path, sizeof path, "sv_args.txt");
            g_args = fopen(path, "a");
            if (!g_args)
                g_record = 0;
        }
        nt = (IMAGE_NT_HEADERS32 *)(sr_base + ((IMAGE_DOS_HEADER *)sr_base)->e_lfanew);
        g_image_size = nt->OptionalHeader.SizeOfImage;
    }
    if (!g_record || index < 0 || index >= RECORD_FUNCS || g_count[index] >= RECORD_MAX || sv_verify_busy())
        return;
    h = hash_args(args, ndw);
    for (k = 0; k < g_count[index]; k++)
        if (g_seen[index][k] == h)
            return;
    g_seen[index][g_count[index]++] = h;

    fprintf(g_args, "R %x", rva);
    for (i = 0; i < ndw; i++) {
        uint32_t v = args[i];
        if (v >= sr_base && v < sr_base + g_image_size) {
            fprintf(g_args, " M:%x", v - (uint32_t)sr_base);
        } else if (v >= 0x10000 && readable((void *)v, BLOB)) {
            const unsigned char *p = (const unsigned char *)v;
            fputs(" P:", g_args);
            for (k = 0; k < BLOB; k++)
                fprintf(g_args, "%02x", p[k]);
        } else {
            fprintf(g_args, " V:%x", v);
        }
    }
    fputc('\n', g_args);
    fflush(g_args);
}

void sv_snapshot_check(void)
{
    static int after = -1, done;
    static DWORD start;
    char path[MAX_PATH];
    const uint8_t *data;
    size_t size, page;
    uint32_t data_rva, base, magic = 0x50414e53; /* "SNAP" */
    unsigned short cw;
    FILE *f;

    if (done)
        return;
    if (after < 0) {
        after = sv_config_int("snapshot_after", 0);
        start = GetTickCount();
        if (after <= 0) {
            done = 1;
            return;
        }
    }
    if (GetTickCount() - start < (DWORD)after * 1000u || sv_verify_busy())
        return;
    done = 1;
    if (!sv_verify_data(&data, &size))
        return;
    exe_path(path, sizeof path, "sv_snapshot.bin");
    f = fopen(path, "wb");
    if (!f)
        return;
    __asm fnstcw cw
    base = (uint32_t)sr_base;
    data_rva = (uint32_t)((uintptr_t)data - sr_base);
    fwrite(&magic, 4, 1, f);
    fwrite(&base, 4, 1, f);
    fwrite(&data_rva, 4, 1, f);
    fwrite(&size, 4, 1, f);
    fwrite(&cw, 2, 1, f);
    for (page = 0; page < size / 4096; page++) {
        const uint32_t *w = (const uint32_t *)(data + page * 4096);
        int k, nonzero = 0;
        for (k = 0; k < 1024 && !nonzero; k++)
            nonzero = w[k] != 0;
        if (nonzero) {
            uint32_t idx = (uint32_t)page;
            fwrite(&idx, 4, 1, f);
            fwrite(w, 4096, 1, f);
        }
    }
    {
        /* The game also keeps pointers to heap memory in .data. Write every committed
         * private read/write region (heaps, malloc blocks), with its address: the harness
         * maps them at the same addresses. End of .data pages: index 0xffffffff. */
        uint32_t end = 0xffffffffu, total = 0;
        uintptr_t a = 0x10000;
        MEMORY_BASIC_INFORMATION mi;
        fwrite(&end, 4, 1, f);
        while (a < 0x7fff0000 && VirtualQuery((void *)a, &mi, sizeof mi)) {
            if (mi.State == MEM_COMMIT && mi.Type == MEM_PRIVATE && mi.Protect == PAGE_READWRITE) {
                uint32_t rbase = (uint32_t)(uintptr_t)mi.BaseAddress, rsize = (uint32_t)mi.RegionSize;
                fwrite(&rbase, 4, 1, f);
                fwrite(&rsize, 4, 1, f);
                fwrite(mi.BaseAddress, rsize, 1, f);
                total += rsize;
            }
            a = (uintptr_t)mi.BaseAddress + mi.RegionSize;
        }
        sv_log("snapshot: %u KB of heap regions", total / 1024);
    }
    fclose(f);
    sv_log("snapshot of .data written to sv_snapshot.bin");
}
