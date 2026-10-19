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

int os_rand(void);

#endif /* SR_OS_H */
