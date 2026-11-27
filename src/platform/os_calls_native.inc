/* The operating system calls of the native client: each one is a direct call of the C runtime,
 * winsock or KERNEL32. The file is part of os_calls.c. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
    return recvfrom(socket, buf, len, flags, (struct sockaddr *)from, fromlen);
}

int os_sendto(uintptr_t socket, const char *buf, int len, int flags, const void *to, int tolen)
{
    return sendto(socket, buf, len, flags, (const struct sockaddr *)to, tolen);
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
    return getsockopt(socket, level, name, value, len);
}

int os_send(uintptr_t socket, const char *buf, int len, int flags)
{
    return send(socket, buf, len, flags);
}

int os_recv(uintptr_t socket, char *buf, int len, int flags)
{
    return recv(socket, buf, len, flags);
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

void os_sleep(unsigned long ms)
{
    Sleep(ms);
}

/* The random numbers of the client. This is the generator of the Microsoft C runtime: the same
 * sequence for the same seed, without the per thread lookup that its rand does on each call. The
 * start seed is 1, as in the runtime. */
static unsigned rand_state = 1;

int os_rand(void)
{
    rand_state = rand_state * 214013u + 2531011u;
    return (int)((rand_state >> 16) & 0x7fff);
}

void os_srand(unsigned seed)
{
    rand_state = seed;
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

/* fprintf, sprintf and sscanf take a list of arguments that a wrapper cannot pass on. */
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
    return (os_printf_fn)printf;
}

char *os_getcwd(char *buffer, int size)
{
    return _getcwd(buffer, size);
}

int os_wsastartup(unsigned short version, void *data)
{
    return WSAStartup(version, (LPWSADATA)data);
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
