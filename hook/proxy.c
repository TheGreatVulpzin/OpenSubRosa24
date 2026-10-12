/* wsock32.dll proxy.
 *
 * subrosadedicated024d.exe imports WSOCK32.dll. Windows loads the copy in the
 * folder of the executable first: this DLL. It loads the system wsock32.dll,
 * forwards the 47 WinSock 1.1 functions and installs the SubVulpes hooks.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#include "hook.h"
#include "sr_runtime.h"

int sv_hooks_024d_install(void);
void sv_hooks_024d_summary(void);

/* Identity of the 024d (PE header). SHA-256 of the file:
 * 11e006786d945d5731cb664fba718a094d8f02648b13fcc20c26c8614ea603fd */
#define SR_024D_TIMESTAMP 0x55ea3325u
#define SR_024D_SIZEOFIMAGE 0x25f3b000u

/* ------------------------------------------------------------ forwarding */

/* Each exported function is a JMP to the real function. The stack stays intact,
 * so the calling convention (stdcall) of the original still applies. */
#define SV_FORWARDS(X)                                                              \
    X(1, accept) X(2, bind) X(3, closesocket) X(4, connect) X(5, getpeername)      \
    X(6, getsockname) X(7, getsockopt) X(8, htonl) X(9, htons) X(10, inet_addr)    \
    X(11, inet_ntoa) X(12, ioctlsocket) X(13, listen) X(14, ntohl) X(15, ntohs)    \
    X(16, recv) X(17, recvfrom) X(18, select) X(19, send) X(20, sendto)            \
    X(21, setsockopt) X(22, shutdown) X(23, socket) X(51, gethostbyaddr)           \
    X(52, gethostbyname) X(53, getprotobyname) X(54, getprotobynumber)             \
    X(55, getservbyname) X(56, getservbyport) X(57, gethostname)                   \
    X(101, WSAAsyncSelect) X(102, WSAAsyncGetHostByAddr)                           \
    X(103, WSAAsyncGetHostByName) X(104, WSAAsyncGetProtoByNumber)                 \
    X(105, WSAAsyncGetProtoByName) X(106, WSAAsyncGetServByPort)                   \
    X(107, WSAAsyncGetServByName) X(108, WSACancelAsyncRequest)                    \
    X(109, WSASetBlockingHook) X(110, WSAUnhookBlockingHook)                       \
    X(111, WSAGetLastError) X(112, WSASetLastError) X(113, WSACancelBlockingCall)  \
    X(114, WSAIsBlocking) X(115, WSAStartup) X(116, WSACleanup) X(151, __WSAFDIsSet)

#define SV_DECLARE(ord, name) static FARPROC p_##name;
SV_FORWARDS(SV_DECLARE)

#define SV_STUB(ord, name) \
    __declspec(naked) void sv_##name(void) { __asm { jmp p_##name } }
SV_FORWARDS(SV_STUB)

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
        sv_log("warning: the system wsock32 has no ordinal %d (%s)", ord, #name);
    SV_FORWARDS(SV_RESOLVE)
    return 1;
}

/* ------------------------------------------------------------ startup */

static int is_024d(void)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)sr_base;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(sr_base + dos->e_lfanew);
    return nt->FileHeader.TimeDateStamp == SR_024D_TIMESTAMP &&
           nt->OptionalHeader.SizeOfImage == SR_024D_SIZEOFIMAGE;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        sr_base = (uintptr_t)GetModuleHandleA(NULL);
        sv_log("SubVulpes loaded. Base of the executable: 0x%08x", (unsigned)sr_base);
        if (!load_real_wsock32()) {
            sv_log("ERROR: could not load the system wsock32.dll");
            return FALSE;
        }
        if (!sv_config_int("enabled", 1)) {
            sv_log("hooks disabled (enabled=0). Only forwarding the network.");
        } else if (!is_024d()) {
            sv_log("executable is not the expected 024d. Only forwarding the network.");
        } else {
            int n = sv_hooks_024d_install();
            sv_log("%d hook(s) installed", n);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        sv_hooks_024d_summary();
        sv_log("SubVulpes unloaded.");
        if (g_real && reserved == NULL)
            FreeLibrary(g_real);
    }
    return TRUE;
}
