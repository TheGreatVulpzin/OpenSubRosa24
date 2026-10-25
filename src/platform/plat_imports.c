/* The imports of the game for the standalone server, linked at build time (Phase 5.2).
 *
 * The 024d takes its CRT from MSVCR100.dll and its sockets from WSOCK32.dll. The lifted C
 * asks for each import by name (sr_import) or by ordinal (sr_import_ordinal). Here each one
 * is a function of the static CRT of this exe, of KERNEL32 or of WSOCK32, so the server
 * does not need MSVCR100.dll.
 *
 * instrumentation/tests/crt_diff.c shows that the static CRT gives the same bits as MSVCR100 for all of
 * these functions, with the inputs of the game. sprintf differs only for %e and %f, which
 * the game does not use. WSOCK32.dll sends most socket functions on to ws2_32.dll: those
 * link to ws2_32 here, the same code. recv, recvfrom and getsockopt have their own code in
 * WSOCK32.dll: those come from it, from System32 only (build/hook holds the proxy
 * wsock32.dll of the hook build).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>

#include <ctype.h>
#include <direct.h>
#include <io.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "platform.h"

#pragma comment(lib, "ws2_32.lib")
/* The tables hold the addresses of DLL functions. Their identity does not matter here. */
#pragma warning(disable : 4232)

/* The x87 intrinsics of the CRT. They take their arguments on the x87 stack, so no header
 * declares them. The static CRT has the same code as ucrtbase.dll. */
void __cdecl _CIsqrt(void);
void __cdecl _CIsin(void);
void __cdecl _CIcos(void);
void __cdecl _CItan(void);
void __cdecl _CIacos(void);
void __cdecl _CIatan2(void);
void __cdecl _CIpow(void);

/* MSVCR100 _sleep: Sleep with at least 1 ms. The new CRT does not have it. */
static void __cdecl plat_sleep(unsigned long ms)
{
    Sleep(ms ? ms : 1);
}

typedef struct {
    const char *name;
    void *fn;
} Import;

static const Import g_crt[] = {
    { "__daylight", (void *)__daylight },
    { "__timezone", (void *)__timezone },
    { "_chdir", (void *)_chdir },
    { "_findclose", (void *)_findclose },
    { "_findfirst64i32", (void *)_findfirst64i32 },
    { "_findnext64i32", (void *)_findnext64i32 },
    { "_getcwd", (void *)_getcwd },
    { "_sleep", (void *)plat_sleep },
    { "_time64", (void *)_time64 },
    { "_tzset", (void *)_tzset },
    { "fclose", (void *)fclose },
    { "fgetc", (void *)fgetc },
    { "floor", (void *)floor },
    { "fopen", (void *)fopen },
    { "fprintf", (void *)fprintf },
    { "fread", (void *)fread },
    { "free", (void *)free },
    { "fseek", (void *)fseek },
    { "fwrite", (void *)fwrite },
    { "malloc", (void *)malloc },
    { "memcpy", (void *)memcpy },
    { "memset", (void *)memset },
    { "qsort", (void *)qsort },
    { "rand", (void *)rand },
    { "sprintf", (void *)sprintf },
    { "srand", (void *)srand },
    { "sscanf", (void *)sscanf },
    { "strncmp", (void *)strncmp },
    { "tolower", (void *)tolower },
    { "_CIsqrt", (void *)_CIsqrt },
    { "_CIsin", (void *)_CIsin },
    { "_CIcos", (void *)_CIcos },
    { "_CItan", (void *)_CItan },
    { "_CIacos", (void *)_CIacos },
    { "_CIatan2", (void *)_CIatan2 },
    { "_CIpow", (void *)_CIpow },
};

static const Import g_kernel[] = {
    { "GetCurrentProcess", (void *)GetCurrentProcess },
    { "GetProcessAffinityMask", (void *)GetProcessAffinityMask },
    { "GetSystemTimeAsFileTime", (void *)GetSystemTimeAsFileTime },
    { "QueryPerformanceCounter", (void *)QueryPerformanceCounter },
    { "QueryPerformanceFrequency", (void *)QueryPerformanceFrequency },
    { "SetProcessAffinityMask", (void *)SetProcessAffinityMask },
};

/* WSOCK32 by ordinal (dumpbin /exports of SysWOW64\wsock32.dll). NULL: the function of
 * WSOCK32.dll itself. */
typedef struct {
    unsigned ordinal;
    void *fn;
} Ordinal;

static const Ordinal g_wsock[] = {
    { 2, (void *)bind },
    { 3, (void *)closesocket },
    { 4, (void *)connect },
    { 7, NULL },
    { 10, (void *)inet_addr },
    { 12, (void *)ioctlsocket },
    { 16, NULL },
    { 17, NULL },
    { 18, (void *)select },
    { 19, (void *)send },
    { 20, (void *)sendto },
    { 23, (void *)socket },
    { 52, (void *)gethostbyname },
    { 55, (void *)getservbyname },
    { 115, (void *)WSAStartup },
    { 151, (void *)__WSAFDIsSet },
};

#define COUNT(a) (sizeof(a) / sizeof *(a))

static void *find(const Import *table, size_t n, const char *name)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (!strcmp(table[i].name, name))
            return table[i].fn;
    return NULL;
}

static void *wsock32(unsigned ordinal)
{
    static HMODULE dll;
    if (!dll)
        dll = LoadLibraryExA("WSOCK32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return dll ? (void *)GetProcAddress(dll, MAKEINTRESOURCEA(ordinal)) : NULL;
}

/* NULL: the server does not know this import (main.c stops with a log line). */
void *plat_lookup(const char *dll, const char *name, unsigned ordinal)
{
    size_t i;
    if (name && !_stricmp(dll, "MSVCR100.dll"))
        return find(g_crt, COUNT(g_crt), name);
    if (name && !_stricmp(dll, "KERNEL32.dll"))
        return find(g_kernel, COUNT(g_kernel), name);
    if (!name && !_stricmp(dll, "WSOCK32.dll"))
        for (i = 0; i < COUNT(g_wsock); i++)
            if (g_wsock[i].ordinal == ordinal)
                return g_wsock[i].fn ? g_wsock[i].fn : wsock32(ordinal);
    return NULL;
}
