/* The operating system calls of the normal server: each one is a direct call of the static C
 * runtime, winsock or KERNEL32. The file is part of os_calls.c.
 *
 * WSOCK32.dll has its own code for recv, recvfrom and getsockopt. Those three come from it, from
 * System32 only. The other socket functions are the same code in ws2_32. */
#define WIN32_LEAN_AND_MEAN
/* The game uses gethostbyname and inet_addr, so the warning about them has no use here. */
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>

#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Functions of src/standalone/main.c. */
int __cdecl sv_printf(const char *fmt, ...);
void sv_capture_packet(int outgoing, const void *data, int len, const void *addr);

/* WSOCK32 ordinals of getsockopt, recv and recvfrom. */
#define WSOCK_GETSOCKOPT 7
#define WSOCK_RECV 16
#define WSOCK_RECVFROM 17

static void *wsock32_function(unsigned ordinal)
{
    static HMODULE dll;
    void *function = NULL;
    if (!dll)
        dll = LoadLibraryExA("WSOCK32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dll)
        function = (void *)GetProcAddress(dll, MAKEINTRESOURCEA(ordinal));
    if (!function) {
        sv_printf("WSOCK32.dll has no function with ordinal %u\n", ordinal);
        ExitProcess(1);
    }
    return function;
}

void *os_malloc(size_t size)
{
    return malloc(size);
}

void os_free(void *pointer)
{
    free(pointer);
}

void *os_realloc(void *pointer, size_t size)
{
    return realloc(pointer, size);
}

int os_recvfrom(uintptr_t socket, char *buf, int len, int flags, void *from, int *fromlen)
{
    static int(__stdcall * call)(uintptr_t, char *, int, int, void *, int *);
    int count;
    if (!call)
        *(void **)&call = wsock32_function(WSOCK_RECVFROM);
    count = call(socket, buf, len, flags, from, fromlen);
    if (count > 0)
        sv_capture_packet(0, buf, count, from);
    return count;
}

int os_sendto(uintptr_t socket, const char *buf, int len, int flags, const void *to, int tolen)
{
    int count = sendto(socket, buf, len, flags, (const struct sockaddr *)to, tolen);
    if (count > 0)
        sv_capture_packet(1, buf, count, to);
    return count;
}

uintptr_t os_socket(int af, int type, int protocol)
{
    return socket(af, type, protocol);
}

int os_bind(uintptr_t socket, const void *addr, int addrlen)
{
    return bind(socket, (const struct sockaddr *)addr, addrlen);
}

int os_ioctlsocket(uintptr_t socket, long cmd, unsigned long *arg)
{
    return ioctlsocket(socket, cmd, arg);
}

int os_connect(uintptr_t socket, const void *addr, int addrlen)
{
    return connect(socket, (const struct sockaddr *)addr, addrlen);
}

int os_select(int nfds, void *readfds, void *writefds, void *exceptfds, const void *timeout)
{
    return select(nfds, (fd_set *)readfds, (fd_set *)writefds, (fd_set *)exceptfds, (const struct timeval *)timeout);
}

int os_fdisset(uintptr_t socket, void *set)
{
    return __WSAFDIsSet(socket, (fd_set *)set);
}

int os_getsockopt(uintptr_t socket, int level, int name, char *value, int *len)
{
    static int(__stdcall * call)(uintptr_t, int, int, char *, int *);
    if (!call)
        *(void **)&call = wsock32_function(WSOCK_GETSOCKOPT);
    return call(socket, level, name, value, len);
}

int os_send(uintptr_t socket, const char *buf, int len, int flags)
{
    return send(socket, buf, len, flags);
}

int os_recv(uintptr_t socket, char *buf, int len, int flags)
{
    static int(__stdcall * call)(uintptr_t, char *, int, int);
    if (!call)
        *(void **)&call = wsock32_function(WSOCK_RECV);
    return call(socket, buf, len, flags);
}

int os_closesocket(uintptr_t socket)
{
    return closesocket(socket);
}

void *os_gethostbyname(const char *name)
{
    return gethostbyname(name);
}

void *os_getservbyname(const char *name, const char *proto)
{
    return getservbyname(name, proto);
}

uint32_t os_inet_addr(const char *text)
{
    return inet_addr(text);
}

int os_wsastartup(unsigned short version, void *data)
{
    return WSAStartup(version, (LPWSADATA)data);
}

/* A sleep of 0 ms still gives up the time slice for 1 ms. */
void os_sleep(unsigned long ms)
{
    Sleep(ms ? ms : 1);
}

int os_rand(void)
{
    return rand();
}

void os_srand(unsigned seed)
{
    srand(seed);
}

int64_t os_time64(int64_t *out)
{
    return _time64(out);
}

/* FILE * is a void * in the shared code. */
void *os_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

int os_fclose(void *file)
{
    return fclose((FILE *)file);
}

int os_fgetc(void *file)
{
    return fgetc((FILE *)file);
}

size_t os_fread(void *buffer, size_t size, size_t count, void *file)
{
    return fread(buffer, size, count, (FILE *)file);
}

size_t os_fwrite(const void *buffer, size_t size, size_t count, void *file)
{
    return fwrite(buffer, size, count, (FILE *)file);
}

int os_fseek(void *file, long offset, int origin)
{
    return fseek((FILE *)file, offset, origin);
}

long os_ftell(void *file)
{
    return ftell((FILE *)file);
}

int os_feof(void *file)
{
    return feof((FILE *)file);
}

int os_fputc(int c, void *file)
{
    return fputc(c, (FILE *)file);
}

int os_chdir(const char *path)
{
    return _chdir(path);
}

char *os_getcwd(char *buffer, int size)
{
    return _getcwd(buffer, size);
}

/* fprintf and sscanf take a list of arguments that a wrapper cannot pass on. printf and sprintf
 * write to the console and the log of the server. */
os_fprintf_fn os_fprintf_import(void)
{
    return (os_fprintf_fn)fprintf;
}

os_sprintf_fn os_sprintf_import(void)
{
    return (os_sprintf_fn)sprintf;
}

os_sscanf_fn os_sscanf_import(void)
{
    return (os_sscanf_fn)sscanf;
}

os_printf_fn os_printf_import(void)
{
    return (os_printf_fn)sv_printf;
}

intptr_t os_findfirst64i32(const char *pattern, void *data)
{
    return _findfirst64i32(pattern, (struct _finddata64i32_t *)data);
}

int os_findnext64i32(intptr_t handle, void *data)
{
    return _findnext64i32(handle, (struct _finddata64i32_t *)data);
}

int os_findclose(intptr_t handle)
{
    return _findclose(handle);
}

void os_tzset(void)
{
    _tzset();
}

long *os_timezone(void)
{
    return __timezone();
}

int *os_daylight(void)
{
    return __daylight();
}

int os_qpc(int64_t *count)
{
    return QueryPerformanceCounter((LARGE_INTEGER *)count);
}

int os_qpf(int64_t *frequency)
{
    return QueryPerformanceFrequency((LARGE_INTEGER *)frequency);
}

void os_getsystemtimeasfiletime(uint32_t *filetime)
{
    GetSystemTimeAsFileTime((FILETIME *)filetime);
}

void *os_getcurrentprocess(void)
{
    return GetCurrentProcess();
}

int os_getprocessaffinitymask(void *process, uintptr_t *process_mask, uintptr_t *system_mask)
{
    return GetProcessAffinityMask((HANDLE)process, (PDWORD_PTR)process_mask, (PDWORD_PTR)system_mask);
}

int os_setprocessaffinitymask(void *process, uintptr_t mask)
{
    return SetProcessAffinityMask((HANDLE)process, mask);
}
