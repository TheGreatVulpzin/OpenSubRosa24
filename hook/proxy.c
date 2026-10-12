/* Proxy do wsock32.dll.
 *
 * O subrosadedicated024d.exe importa WSOCK32.dll. O Windows carrega primeiro
 * a cópia na pasta do executável: este DLL. Ele carrega o wsock32.dll do sistema,
 * repassa as 47 funções da WinSock 1.1 e instala os hooks do SubVulpes.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#include "hook.h"
#include "sr_runtime.h"

int sv_hooks_024d_install(void);

/* Identidade do 024d (cabeçalho PE). SHA-256 do arquivo:
 * 11e006786d945d5731cb664fba718a094d8f02648b13fcc20c26c8614ea603fd */
#define SR_024D_TIMESTAMP 0x55ea3325u
#define SR_024D_SIZEOFIMAGE 0x25f3b000u

/* ------------------------------------------------------------ repasse */

/* Cada função exportada é um JMP para a função real. A pilha fica intacta,
 * então a convenção de chamada (stdcall) da original continua valendo. */
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
        sv_log("aviso: wsock32 do sistema não tem o ordinal %d (%s)", ord, #name);
    SV_FORWARDS(SV_RESOLVE)
    return 1;
}

/* ------------------------------------------------------------ início */

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
        sv_log("SubVulpes carregado. Base do executável: 0x%08x", (unsigned)sr_base);
        if (!load_real_wsock32()) {
            sv_log("ERRO: não consegui carregar o wsock32.dll do sistema");
            return FALSE;
        }
        if (!sv_config_int("enabled", 1)) {
            sv_log("hooks desligados (enabled=0). Só repassando a rede.");
        } else if (!is_024d()) {
            sv_log("executável não é o 024d esperado. Só repassando a rede.");
        } else {
            int n = sv_hooks_024d_install();
            sv_log("%d hook(s) instalado(s)", n);
        }
    } else if (reason == DLL_PROCESS_DETACH && g_real && reserved == NULL) {
        FreeLibrary(g_real);
    }
    return TRUE;
}
