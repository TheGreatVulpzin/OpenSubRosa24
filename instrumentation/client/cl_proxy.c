/* wsock32.dll proxy for subrosa.exe (client).
 *
 * subrosa.exe imports WSOCK32.dll. Windows loads the copy in the folder of the
 * executable first: this DLL. It loads the system wsock32.dll and forwards the 47
 * WinSock 1.1 functions. It writes subvulpes_client.log next to the executable.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "wsock_forwards.h"

#if defined(CL_COUNT) || defined(CL_HOOKS)
extern uintptr_t sr_base;
int sv_config_int(const char *key, int def);

/* The client has no server console: hook.c and record.c report events here. */
void sv_console_event(int error, const char *fmt, ...)
{
    (void)error;
    (void)fmt;
}
#endif
#ifdef CL_COUNT
int cl_count_start(void);
#endif
#ifdef CL_HOOKS
int cl_hooks_install(void);
void cl_hooks_summary(void);
#endif

/* Identity of the client (PE header). SHA-256 of the file is in original/MANIFEST.sha256. */
#define SR_CLIENT_TIMESTAMP 0x55bdbd69u
#define SR_CLIENT_SIZEOFIMAGE 0x2a7ff000u

static char g_log_path[MAX_PATH];

static void cl_log(const char *fmt, ...)
{
    char line[512];
    DWORD written;
    HANDLE f;
    va_list ap;
    int n;
    SYSTEMTIME t;
    GetLocalTime(&t);
    n = snprintf(line, sizeof line, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_start(ap, fmt);
    n += vsnprintf(line + n, sizeof line - 3 - (size_t)n, fmt, ap);
    va_end(ap);
    line[n++] = '\r';
    line[n++] = '\n';
    f = CreateFileA(g_log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    WriteFile(f, line, (DWORD)n, &written, NULL);
    CloseHandle(f);
}

/* Each exported function is a JMP to the real function. The stack stays intact,
 * so the stdcall convention of the original still applies. */
#define SV_DECLARE(ord, name) static FARPROC p_##name;
SV_FORWARDS(SV_DECLARE)

#define SV_STUB(ord, name) \
    __declspec(naked) void sv_##name(void) { __asm { jmp p_##name } }
SV_FORWARDS(SV_STUB)

#ifdef CL_HOOKS
/* The pointer that the stub of a WinSock function jumps through: the recorder puts a
 * wrapper there (instrumentation/hook/record_imports.c). */
FARPROC *sv_wsock_slot(const char *name)
{
#define SV_SLOT(ord, nm) if (lstrcmpA(name, #nm) == 0) return &p_##nm;
    SV_FORWARDS(SV_SLOT)
#undef SV_SLOT
    return NULL;
}
#endif

static HMODULE g_real;

static int load_real_wsock32(void)
{
    char path[MAX_PATH];
    UINT n = GetSystemDirectoryA(path, MAX_PATH);
    if (n == 0 || n > MAX_PATH - 16)
        return 0;
    lstrcatA(path, "\\wsock32.dll");
    g_real = LoadLibraryA(path);
    if (!g_real)
        return 0;
#define SV_RESOLVE(ord, name)                                   \
    p_##name = GetProcAddress(g_real, MAKEINTRESOURCEA(ord));   \
    if (!p_##name)                                              \
        cl_log("warning: the system wsock32 has no ordinal %d (%s)", ord, #name);
    SV_FORWARDS(SV_RESOLVE)
    return 1;
}

static int is_client(const unsigned char *base)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    return nt->FileHeader.TimeDateStamp == SR_CLIENT_TIMESTAMP &&
           nt->OptionalHeader.SizeOfImage == SR_CLIENT_SIZEOFIMAGE;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        char *slash;
        const unsigned char *base = (const unsigned char *)GetModuleHandleA(NULL);
        DisableThreadLibraryCalls(inst);
        GetModuleFileNameA(NULL, g_log_path, MAX_PATH);
        slash = strrchr(g_log_path, '\\');
        lstrcpyA(slash ? slash + 1 : g_log_path, "subvulpes_client.log");
        cl_log("SubVulpes client proxy loaded. Base of the executable: 0x%08x", (unsigned)(uintptr_t)base);
        if (!load_real_wsock32()) {
            cl_log("ERROR: could not load the system wsock32.dll");
            return FALSE;
        }
        if (!is_client(base))
            cl_log("executable is not the expected subrosa.exe. Only forwarding the network.");
        else {
            int handled = 0;
#if defined(CL_COUNT) || defined(CL_HOOKS)
            sr_base = (uintptr_t)base;
#endif
#ifdef CL_COUNT
            /* subvulpes.ini [hooks] count=1 counts calls (instrumentation/client/cl_count.c). */
            if (sv_config_int("count", 0)) {
                cl_log("executable is subrosa.exe. Counting calls.");
                cl_count_start();
                handled = 1;
            }
#endif
#ifdef CL_HOOKS
            /* subvulpes.ini [hooks] hooks=1 installs the lifter hooks and the recorder. */
            if (!handled && sv_config_int("hooks", 0)) {
                cl_log("executable is subrosa.exe. %d hook(s) installed.", cl_hooks_install());
                handled = 1;
            }
#endif
            if (!handled)
                cl_log("executable is subrosa.exe. Only forwarding the network.");
        }
    } else if (reason == DLL_PROCESS_DETACH) {
#ifdef CL_HOOKS
        cl_hooks_summary();
#endif
        cl_log("SubVulpes client proxy unloaded.");
        if (g_real && reserved == NULL)
            FreeLibrary(g_real);
    }
    return TRUE;
}
