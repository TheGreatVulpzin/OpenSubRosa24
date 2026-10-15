/* Hook installation, log and configuration. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hook.h"
#include "sr_runtime.h"

uintptr_t sr_base;

static char g_dir[MAX_PATH];
static SvHook *g_installed[1024];  /* installed hooks, for sv_unhook_rva */
static int g_ninstalled, g_unhooked;  /* folder of the executable, with a trailing backslash */

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
    /* Errors also go to the server console (only after sv_console_init). */
    if (strncmp(fmt, "ERROR", 5) == 0) {
        char text[512];
        va_start(ap, fmt);
        vsnprintf(text, sizeof text, fmt, ap);
        va_end(ap);
        sv_console_event(1, "%s", text);
    }
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
    if (g_ninstalled < (int)(sizeof g_installed / sizeof g_installed[0]))
        g_installed[g_ninstalled++] = h;
    return 1;
}

/* Put the original prologue back: the function runs with no hook from now on. The
 * trampoline holds the original bytes (a relative prologue is never hooked). Call it only
 * on the game thread, from inside a hook: no other thread runs game code. */
int sv_unhook_rva(uint32_t rva)
{
    int i;
    for (i = 0; i < g_ninstalled; i++) {
        SvHook *h = g_installed[i];
        uint8_t *site;
        DWORD old;
        if (!h->installed || h->rva != rva || !h->original || !*h->original)
            continue;
        site = (uint8_t *)(sr_base + h->rva);
        if (!VirtualProtect(site, h->prologue_len, PAGE_EXECUTE_READWRITE, &old))
            return 0;
        memcpy(site, *h->original, h->prologue_len);
        VirtualProtect(site, h->prologue_len, old, &old);
        FlushInstructionCache(GetCurrentProcess(), site, h->prologue_len);
        h->installed = 0;
        g_unhooked++;
        return 1;
    }
    return 0;
}

/* Put a removed hook back (the rotation of the recorder). Game thread only. */
int sv_rehook_rva(uint32_t rva)
{
    int i;
    for (i = 0; i < g_ninstalled; i++) {
        SvHook *h = g_installed[i];
        uint8_t *site;
        DWORD old;
        if (h->installed || h->rva != rva)
            continue;
        site = (uint8_t *)(sr_base + h->rva);
        if (!VirtualProtect(site, h->prologue_len, PAGE_EXECUTE_READWRITE, &old))
            return 0;
        write_jmp(site, h->replacement);
        memset(site + 5, 0xCC, h->prologue_len - 5);
        VirtualProtect(site, h->prologue_len, old, &old);
        FlushInstructionCache(GetCurrentProcess(), site, h->prologue_len);
        h->installed = 1;
        g_unhooked--;
        return 1;
    }
    return 0;
}

int sv_unhooked_count(void)
{
    return g_unhooked;
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
