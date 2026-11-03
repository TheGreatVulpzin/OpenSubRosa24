/* Call counter of the client proxy.
 *
 * It counts calls of three groups and writes the counts once per second:
 * game functions (the hook tables of the lifter), GLee stubs and OPENGL32.dll imports.
 * It records no arguments. The numbers show how big a recording of the client would be.
 *
 * Files next to the executable: subvulpes_counts.log (one line per second) and
 * subvulpes_counts.csv (totals per function, rewritten every 5 seconds).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"

#define CL_COUNT_MAX 4096

extern volatile LONG cl_count[CL_COUNT_MAX];
extern SvHook cl_count_hooks[];
extern const int cl_count_nhooks;
extern const struct { int hook, offset; } cl_count_relocs[];
extern const struct { uint32_t rva; const char *name; } cl_count_stubs[];
extern const int cl_count_nstubs;

extern uintptr_t sr_base;

void sv_console_event(int error, const char *fmt, ...)
{
    (void)error;
    (void)fmt;
}

static const char *g_name[CL_COUNT_MAX];
static int g_first_stub, g_first_iat, g_total;

static uint8_t *g_code;
static size_t g_code_used;

static uint8_t *code_alloc(size_t n)
{
    uint8_t *p;
    if (!g_code) {
        g_code = (uint8_t *)VirtualAlloc(NULL, 1 << 20, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!g_code)
            return NULL;
    }
    if (g_code_used + n > (1u << 20))
        return NULL;
    p = g_code + g_code_used;
    g_code_used += n;
    return p;
}

/* pushfd, lock inc [counter], popfd, then the code that follows. */
static uint8_t *emit_count(uint8_t *p, volatile LONG *counter)
{
    *p++ = 0x9c;
    *p++ = 0xf0;
    *p++ = 0xff;
    *p++ = 0x05;
    *(uint32_t *)p = (uint32_t)(uintptr_t)counter;
    p += 4;
    *p++ = 0x9d;
    return p;
}

static uint8_t *emit_jmp_abs(uint8_t *p, const void *target)
{
    *p++ = 0xe9;
    *(int32_t *)p = (int32_t)((uintptr_t)target - ((uintptr_t)p + 4));
    return p + 4;
}

/* A GLee stub starts with "push ebp; mov ebp, esp; call <check>". The counter thunk runs
 * these 8 bytes itself (with the call fixed) and jumps to the rest of the stub. */
static int hook_stub(uint32_t rva, int idx)
{
    uint8_t *site = (uint8_t *)(sr_base + rva);
    static const uint8_t head[4] = { 0x55, 0x8b, 0xec, 0xe8 };
    uint8_t *thunk, *p;
    DWORD old;
    int32_t rel;

    if (memcmp(site, head, 4) != 0)
        return 0;
    thunk = code_alloc(40);
    if (!thunk)
        return 0;
    p = emit_count(thunk, &cl_count[idx]);
    *p++ = 0x55;
    *p++ = 0x8b;
    *p++ = 0xec;
    *p++ = 0xe8;
    memcpy(&rel, site + 4, 4);
    *(int32_t *)p = rel + (int32_t)((uintptr_t)site + 8 - ((uintptr_t)p + 4));
    p += 4;
    emit_jmp_abs(p, site + 8);
    if (!VirtualProtect(site, 8, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    emit_jmp_abs(site, thunk);
    memset(site + 5, 0xcc, 3);
    VirtualProtect(site, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 8);
    return 1;
}

/* Patch the import address table entries of OPENGL32.dll. */
static int hook_gl_imports(int idx0)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)sr_base;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(sr_base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)(sr_base + dir->VirtualAddress);
    int n = 0;

    for (; d->Name; d++) {
        const IMAGE_THUNK_DATA32 *names;
        IMAGE_THUNK_DATA32 *slots;
        if (lstrcmpiA((const char *)(sr_base + d->Name), "OPENGL32.dll") != 0)
            continue;
        names = (const IMAGE_THUNK_DATA32 *)(sr_base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        slots = (IMAGE_THUNK_DATA32 *)(sr_base + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, slots++) {
            uint8_t *thunk, *p;
            DWORD old;
            int idx = idx0 + n;
            if (idx >= CL_COUNT_MAX || (names->u1.Ordinal & 0x80000000u))
                continue;
            thunk = code_alloc(16);
            if (!thunk)
                continue;
            p = emit_count(thunk, &cl_count[idx]);
            *p++ = 0xff;
            *p++ = 0x25;
            /* The jump reads a private copy of the real address, not the patched slot. */
            {
                uint32_t *copy = (uint32_t *)code_alloc(4);
                if (!copy)
                    continue;
                *copy = slots->u1.Function;
                *(uint32_t *)p = (uint32_t)(uintptr_t)copy;
            }
            g_name[idx] = (const char *)(sr_base + names->u1.AddressOfData + 2);
            if (!VirtualProtect(slots, 4, PAGE_READWRITE, &old))
                continue;
            slots->u1.Function = (DWORD)(uintptr_t)thunk;
            VirtualProtect(slots, 4, old, &old);
            n++;
        }
    }
    return n;
}

static DWORD WINAPI sampler(LPVOID unused)
{
    static LONG last[CL_COUNT_MAX];
    FILE *log;
    char dir[MAX_PATH], path[MAX_PATH];
    char *slash;
    int sec = 0, i;

    (void)unused;
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    slash = strrchr(dir, '\\');
    if (slash)
        slash[1] = '\0';
    snprintf(path, sizeof path, "%ssubvulpes_counts.log", dir);
    log = fopen(path, "w");
    if (!log)
        return 0;
    fprintf(log, "sec game gl_stub gl_import\n");
    for (;;) {
        unsigned game = 0, stub = 0, iat = 0;
        Sleep(1000);
        sec++;
        for (i = 0; i < g_total; i++) {
            LONG now = cl_count[i], delta = now - last[i];
            last[i] = now;
            if (i < g_first_stub)
                game += (unsigned)delta;
            else if (i < g_first_iat)
                stub += (unsigned)delta;
            else
                iat += (unsigned)delta;
        }
        fprintf(log, "%d %u %u %u\n", sec, game, stub, iat);
        fflush(log);
        if (sec % 5 == 0) {
            FILE *csv;
            snprintf(path, sizeof path, "%ssubvulpes_counts.csv", dir);
            csv = fopen(path, "w");
            if (csv) {
                fprintf(csv, "group,name,calls\n");
                for (i = 0; i < g_total; i++)
                    if (cl_count[i] && g_name[i])
                        fprintf(csv, "%s,%s,%ld\n", i < g_first_stub ? "game" : i < g_first_iat ? "gl_stub" : "gl_import",
                                g_name[i], (long)cl_count[i]);
                fclose(csv);
            }
        }
    }
}

/* Log the first exceptions with the address as an offset of the executable. */
static LONG CALLBACK crash_log(EXCEPTION_POINTERS *ep)
{
    static volatile LONG n;
    const EXCEPTION_RECORD *r = ep->ExceptionRecord;
    const CONTEXT *c = ep->ContextRecord;
    uintptr_t eip = (uintptr_t)r->ExceptionAddress;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION && r->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION &&
        r->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION && r->ExceptionCode != EXCEPTION_BREAKPOINT)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&n) <= 10)
        sv_log("exception 0x%08lx at 0x%08x (exe+0x%x) eax=%08x ecx=%08x edx=%08x esp=%08x ebp=%08x",
               (unsigned long)r->ExceptionCode, (unsigned)eip, (unsigned)(eip - sr_base), (unsigned)c->Eax,
               (unsigned)c->Ecx, (unsigned)c->Edx, (unsigned)c->Esp, (unsigned)c->Ebp);
    return EXCEPTION_CONTINUE_SEARCH;
}

int cl_count_start(void)
{
    AddVectoredExceptionHandler(1, crash_log);
    int i, installed, stubs = 0, imports;
    const uint32_t delta = (uint32_t)(sr_base - 0x400000u);

    for (i = 0; cl_count_relocs[i].hook >= 0; i++) {
        uint32_t v;
        uint8_t *b = (uint8_t *)cl_count_hooks[cl_count_relocs[i].hook].expect + cl_count_relocs[i].offset;
        memcpy(&v, b, 4);
        v += delta;
        memcpy(b, &v, 4);
    }
    /* subvulpes.ini count_game, count_stub and count_gl turn one group off. */
    {
        int from = sv_config_int("game_from", 0), to = sv_config_int("game_to", cl_count_nhooks);
        if (to > cl_count_nhooks)
            to = cl_count_nhooks;
        installed = sv_config_int("count_game", 1) && from < to ? sv_install_hooks(cl_count_hooks + from, to - from) : 0;
    }
    for (i = 0; i < cl_count_nhooks; i++)
        g_name[i] = cl_count_hooks[i].name;
    g_first_stub = cl_count_nhooks;
    for (i = 0; i < cl_count_nstubs; i++) {
        g_name[g_first_stub + i] = cl_count_stubs[i].name;
        if (sv_config_int("count_stub", 1))
            stubs += hook_stub(cl_count_stubs[i].rva, g_first_stub + i);
    }
    g_first_iat = g_first_stub + cl_count_nstubs;
    imports = sv_config_int("count_gl", 1) ? hook_gl_imports(g_first_iat) : 0;
    g_total = g_first_iat + imports;
    sv_log("counter: %d game hooks, %d GLee stubs, %d OpenGL imports", installed, stubs, imports);
    CreateThread(NULL, 0, sampler, NULL, 0, NULL);
    return installed;
}
