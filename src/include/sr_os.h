/* The operating system calls of the game: sockets, files, time and the CRT random numbers.
 *
 * Each call goes to the import of the game (sr_import). Each build gives its own import: the
 * test environment in the harness, the recorder in the recording build, the system in the
 * server. So a test or a recording sees each call.
 */
#ifndef SR_OS_H
#define SR_OS_H

#include <stdint.h>

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

#endif /* SR_OS_H */
