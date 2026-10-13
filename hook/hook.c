/* Hook installation, log and configuration. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hook.h"
#include "sr_runtime.h"

uintptr_t sr_base;

static char g_dir[MAX_PATH];  /* folder of the executable, with a trailing backslash */

static const char *exe_dir(void)
{
    if (!g_dir[0]) {
        char *slash;
        GetModuleFileNameA(NULL, g_dir, MAX_PATH);
        slash = strrchr(g_dir, '\\');
        if (slash)
            slash[1] = '\0';
    }
    return g_dir;
}

void sv_log(const char *fmt, ...)
{
    /* The file stays open. Each line is flushed, so a crash report is complete. */
    static FILE *f;
    SYSTEMTIME t;
    va_list ap;

    if (!f) {
        char path[MAX_PATH];
        snprintf(path, sizeof path, "%ssubvulpes.log", exe_dir());
        f = fopen(path, "a");
        if (!f)
            return;
    }
    GetLocalTime(&t);
    fprintf(f, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
}

int sv_config_int(const char *key, int def)
{
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%ssubvulpes.ini", exe_dir());
    return (int)GetPrivateProfileIntA("hooks", key, def, path);
}

/* Check the original bytes. Returns 1 if they match. */
static int check_bytes(const SvHook *h, const uint8_t *site)
{
    int i;
    for (i = 0; i < h->prologue_len; i++) {
        if (h->mask[i] && site[i] != h->expect[i])
            return 0;
    }
    return 1;
}

static void write_jmp(uint8_t *from, const void *to)
{
    from[0] = 0xE9;  /* JMP rel32 */
    *(int32_t *)(from + 1) = (int32_t)((uintptr_t)to - ((uintptr_t)from + 5));
}

static int install(SvHook *h)
{
    uint8_t *site = (uint8_t *)(sr_base + h->rva);
    uint8_t *tramp;
    DWORD old;

    if (!check_bytes(h, site)) {
        sv_log("ERROR %s: original bytes differ at +0x%x (another patch at the same place?)", h->name, h->rva);
        return 0;
    }

    /* Trampoline: original prologue + JMP back to the rest of the function. */
    tramp = (uint8_t *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!tramp) {
        sv_log("ERROR %s: VirtualAlloc failed (%lu)", h->name, GetLastError());
        return 0;
    }
    memcpy(tramp, site, h->prologue_len);
    write_jmp(tramp + h->prologue_len, site + h->prologue_len);
    VirtualProtect(tramp, 32, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), tramp, 32);
    if (h->original)
        *h->original = tramp;

    /* Site: JMP to the new function. Remaining bytes become INT3. */
    if (!VirtualProtect(site, h->prologue_len, PAGE_EXECUTE_READWRITE, &old)) {
        sv_log("ERROR %s: VirtualProtect failed (%lu)", h->name, GetLastError());
        return 0;
    }
    write_jmp(site, h->replacement);
    memset(site + 5, 0xCC, h->prologue_len - 5);
    VirtualProtect(site, h->prologue_len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, h->prologue_len);

    h->installed = 1;
    return 1;
}

int sv_install_hooks(SvHook *hooks, int count)
{
    int i, n = 0;
    for (i = 0; i < count; i++) {
        SvHook *h = &hooks[i];
        h->enabled = sv_config_int(h->name, 1);
        if (!h->enabled) {
            sv_log("hook %-28s disabled in subvulpes.ini", h->name);
            continue;
        }
        if (install(h)) {
            sv_log("hook %-28s installed at +0x%05x", h->name, h->rva);
            n++;
        }
    }
    return n;
}
