/* The operating system calls of the game: sockets, files, time and the CRT random numbers.
 *
 * Each call goes to the import of the game (sr_import). Each build gives its own import: the
 * test environment in the harness, the recorder in the recording build, the system in the
 * server. So a test or a recording sees each call.
 */
#ifndef SR_OS_H
#define SR_OS_H

#include <stdint.h>
#include <stddef.h>

void *os_malloc(size_t size);
void os_free(void *pointer);

int os_recvfrom(uintptr_t socket, char *buf, int len, int flags, void *from, int *fromlen);
int os_sendto(uintptr_t socket, const char *buf, int len, int flags, const void *to, int tolen);

uintptr_t os_socket(int af, int type, int protocol);
int os_bind(uintptr_t socket, const void *addr, int addrlen);
int os_ioctlsocket(uintptr_t socket, long cmd, unsigned long *arg);
int os_connect(uintptr_t socket, const void *addr, int addrlen);
int os_select(int nfds, void *readfds, void *writefds, void *exceptfds, const void *timeout);
int os_fdisset(uintptr_t socket, void *set);
int os_getsockopt(uintptr_t socket, int level, int name, char *value, int *len);
int os_send(uintptr_t socket, const char *buf, int len, int flags);
int os_recv(uintptr_t socket, char *buf, int len, int flags);
int os_closesocket(uintptr_t socket);
void *os_gethostbyname(const char *name);
void *os_getservbyname(const char *name, const char *proto);
uint32_t os_inet_addr(const char *text);

/* _sleep of the CRT of the game. */
void os_sleep(unsigned long ms);

int os_rand(void);
void os_srand(unsigned seed);
int64_t os_time64(int64_t *out);

/* Files of the CRT of the game. FILE * is a void *. */
void *os_fopen(const char *path, const char *mode);
int os_fclose(void *file);
int os_fgetc(void *file);
int os_chdir(const char *path);

/* fprintf and sscanf take a list of arguments, so a wrapper cannot pass them on. The
 * function returns the import and the macro calls it: os_fprintf(file, "%d", value). */
typedef int(__cdecl *os_fprintf_fn)(void *file, const char *format, ...);
typedef int(__cdecl *os_sscanf_fn)(const char *text, const char *format, ...);
os_fprintf_fn os_fprintf_import(void);
os_sscanf_fn os_sscanf_import(void);
#define os_fprintf (os_fprintf_import())
#define os_sscanf (os_sscanf_import())

/* File search of the CRT. data is a _finddata64i32_t. */
intptr_t os_findfirst64i32(const char *pattern, void *data);
int os_findnext64i32(intptr_t handle, void *data);
int os_findclose(intptr_t handle);

/* Time zone of the CRT. */
void os_tzset(void);
long *os_timezone(void);
int *os_daylight(void);

/* KERNEL32: the performance counter, the clock and the process affinity. */
int os_qpc(int64_t *count);
int os_qpf(int64_t *frequency);
void os_getsystemtimeasfiletime(uint32_t *filetime);
void *os_getcurrentprocess(void);
int os_getprocessaffinitymask(void *process, uintptr_t *process_mask, uintptr_t *system_mask);
int os_setprocessaffinitymask(void *process, uintptr_t mask);

#endif /* SR_OS_H */
