/* Waits of the client loop.
 */
#include "client.h"

#include "system_libs.h"

/* Wait 100 milliseconds. */
void __cdecl cs_sleep(void)
{
    SDL_Delay(100);
}
