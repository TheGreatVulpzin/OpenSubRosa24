/* The operating system calls of the game, through its imports (see sr_os.h). Each function
 * finds its import at the first call. */
#include "sr_os.h"

void *sr_import(const char *dll, const char *name);
void *sr_import_ordinal(const char *dll, unsigned ordinal);

/* WSOCK32 ordinals of the game. */
#define WSOCK_RECVFROM 17
#define WSOCK_SENDTO 20

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

/* rand of the CRT of the game: the game and the rewritten code share one sequence. */
int os_rand(void)
{
    static int(__cdecl * call)(void);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "rand");
    return call();
}
