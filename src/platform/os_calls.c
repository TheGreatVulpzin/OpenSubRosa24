/* The operating system calls of the game, through its imports (see sr_os.h). Each function
 * finds its import at the first call. */
#include "sr_os.h"

void *sr_import(const char *dll, const char *name);
void *sr_import_ordinal(const char *dll, unsigned ordinal);

/* WSOCK32 ordinals of the game. */
#define WSOCK_CLOSESOCKET 3
#define WSOCK_BIND 2
#define WSOCK_CONNECT 4
#define WSOCK_GETSOCKOPT 7
#define WSOCK_INET_ADDR 10
#define WSOCK_IOCTLSOCKET 12
#define WSOCK_RECV 16
#define WSOCK_RECVFROM 17
#define WSOCK_SELECT 18
#define WSOCK_SEND 19
#define WSOCK_SENDTO 20
#define WSOCK_SOCKET 23
#define WSOCK_GETHOSTBYNAME 52
#define WSOCK_GETSERVBYNAME 55
#define WSOCK_FDISSET 151

int os_recvfrom(uintptr_t socket, char *buf, int len, int flags, void *from, int *fromlen)
{
    static int(__stdcall * call)(uintptr_t, char *, int, int, void *, int *);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_RECVFROM);
    return call(socket, buf, len, flags, from, fromlen);
}

int os_sendto(uintptr_t socket, const char *buf, int len, int flags, const void *to, int tolen)
{
    static int(__stdcall * call)(uintptr_t, const char *, int, int, const void *, int);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_SENDTO);
    return call(socket, buf, len, flags, to, tolen);
}

/* rand of the CRT of the game: the game and the server code share one sequence. */
int os_rand(void)
{
    static int(__cdecl * call)(void);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "rand");
    return call();
}

uintptr_t os_socket(int af, int type, int protocol)
{
    static uintptr_t(__stdcall * call)(int, int, int);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_SOCKET);
    return call(af, type, protocol);
}

int os_bind(uintptr_t socket, const void *addr, int addrlen)
{
    static int(__stdcall * call)(uintptr_t, const void *, int);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_BIND);
    return call(socket, addr, addrlen);
}

int os_ioctlsocket(uintptr_t socket, long cmd, unsigned long *arg)
{
    static int(__stdcall * call)(uintptr_t, long, unsigned long *);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_IOCTLSOCKET);
    return call(socket, cmd, arg);
}

int os_connect(uintptr_t socket, const void *addr, int addrlen)
{
    static int(__stdcall * call)(uintptr_t, const void *, int);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_CONNECT);
    return call(socket, addr, addrlen);
}

int os_select(int nfds, void *readfds, void *writefds, void *exceptfds, const void *timeout)
{
    static int(__stdcall * call)(int, void *, void *, void *, const void *);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_SELECT);
    return call(nfds, readfds, writefds, exceptfds, timeout);
}

int os_fdisset(uintptr_t socket, void *set)
{
    static int(__stdcall * call)(uintptr_t, void *);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_FDISSET);
    return call(socket, set);
}

int os_getsockopt(uintptr_t socket, int level, int name, char *value, int *len)
{
    static int(__stdcall * call)(uintptr_t, int, int, char *, int *);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_GETSOCKOPT);
    return call(socket, level, name, value, len);
}

int os_send(uintptr_t socket, const char *buf, int len, int flags)
{
    static int(__stdcall * call)(uintptr_t, const char *, int, int);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_SEND);
    return call(socket, buf, len, flags);
}

int os_recv(uintptr_t socket, char *buf, int len, int flags)
{
    static int(__stdcall * call)(uintptr_t, char *, int, int);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_RECV);
    return call(socket, buf, len, flags);
}

int os_closesocket(uintptr_t socket)
{
    static int(__stdcall * call)(uintptr_t);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_CLOSESOCKET);
    return call(socket);
}

void *os_gethostbyname(const char *name)
{
    static void *(__stdcall * call)(const char *);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_GETHOSTBYNAME);
    return call(name);
}

void *os_getservbyname(const char *name, const char *proto)
{
    static void *(__stdcall * call)(const char *, const char *);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_GETSERVBYNAME);
    return call(name, proto);
}

uint32_t os_inet_addr(const char *text)
{
    static uint32_t(__stdcall * call)(const char *);
    if (!call)
        *(void **)&call = sr_import_ordinal("WSOCK32.dll", WSOCK_INET_ADDR);
    return call(text);
}

void os_sleep(unsigned long ms)
{
    static void(__cdecl * call)(unsigned long);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "_sleep");
    call(ms);
}

/* Files of the CRT. FILE * is a void * here: the CRT of the game owns the FILE. */
void *os_fopen(const char *path, const char *mode)
{
    static void *(__cdecl * call)(const char *, const char *);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "fopen");
    return call(path, mode);
}

int os_fclose(void *file)
{
    static int(__cdecl * call)(void *);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "fclose");
    return call(file);
}

int os_fgetc(void *file)
{
    static int(__cdecl * call)(void *);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "fgetc");
    return call(file);
}

/* fprintf and sscanf take a list of arguments that a wrapper cannot pass on. These two
 * functions return the import, and sr_os.h makes a macro of each. */
os_fprintf_fn os_fprintf_import(void)
{
    static os_fprintf_fn call;
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "fprintf");
    return call;
}

os_sscanf_fn os_sscanf_import(void)
{
    static os_sscanf_fn call;
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "sscanf");
    return call;
}

int os_chdir(const char *path)
{
    static int(__cdecl * call)(const char *);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "_chdir");
    return call(path);
}

int64_t os_time64(int64_t *out)
{
    static int64_t(__cdecl * call)(int64_t *);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "_time64");
    return call(out);
}

void os_srand(unsigned seed)
{
    static void(__cdecl * call)(unsigned);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "srand");
    call(seed);
}

intptr_t os_findfirst64i32(const char *pattern, void *data)
{
    static intptr_t(__cdecl * call)(const char *, void *);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "_findfirst64i32");
    return call(pattern, data);
}

int os_findnext64i32(intptr_t handle, void *data)
{
    static int(__cdecl * call)(intptr_t, void *);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "_findnext64i32");
    return call(handle, data);
}

int os_findclose(intptr_t handle)
{
    static int(__cdecl * call)(intptr_t);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "_findclose");
    return call(handle);
}

void os_tzset(void)
{
    static void(__cdecl * call)(void);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "_tzset");
    call();
}

long *os_timezone(void)
{
    static long *(__cdecl * call)(void);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "__timezone");
    return call();
}

int *os_daylight(void)
{
    static int *(__cdecl * call)(void);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "__daylight");
    return call();
}

/* Time and process calls of KERNEL32. */
int os_qpc(int64_t *count)
{
    static int(__stdcall * call)(int64_t *);
    if (!call)
        *(void **)&call = sr_import("KERNEL32.dll", "QueryPerformanceCounter");
    return call(count);
}

int os_qpf(int64_t *frequency)
{
    static int(__stdcall * call)(int64_t *);
    if (!call)
        *(void **)&call = sr_import("KERNEL32.dll", "QueryPerformanceFrequency");
    return call(frequency);
}

void os_getsystemtimeasfiletime(uint32_t *filetime)
{
    static void(__stdcall * call)(uint32_t *);
    if (!call)
        *(void **)&call = sr_import("KERNEL32.dll", "GetSystemTimeAsFileTime");
    call(filetime);
}

void *os_getcurrentprocess(void)
{
    static void *(__stdcall * call)(void);
    if (!call)
        *(void **)&call = sr_import("KERNEL32.dll", "GetCurrentProcess");
    return call();
}

int os_getprocessaffinitymask(void *process, uintptr_t *process_mask, uintptr_t *system_mask)
{
    static int(__stdcall * call)(void *, uintptr_t *, uintptr_t *);
    if (!call)
        *(void **)&call = sr_import("KERNEL32.dll", "GetProcessAffinityMask");
    return call(process, process_mask, system_mask);
}

int os_setprocessaffinitymask(void *process, uintptr_t mask)
{
    static int(__stdcall * call)(void *, uintptr_t);
    if (!call)
        *(void **)&call = sr_import("KERNEL32.dll", "SetProcessAffinityMask");
    return call(process, mask);
}
