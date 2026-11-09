/* Waits of the client loop.
 */
#include "client.h"

void *sr_import(const char *dll, const char *name);

/* Wait 100 milliseconds. */
void __cdecl cs_sleep(void)
{
    static void(__cdecl * delay)(unsigned);
    if (!delay)
        *(void **)&delay = sr_import("SDL2.DLL", "SDL_Delay");
    delay(100);
}
