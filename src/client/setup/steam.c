/* Steam of the client. The client runs without Steam: every function does nothing, and
 * steam_init tells the caller that Steam is not there. Callers still call these functions.
 * When the callers do not call them, this file goes away. */
#include "client.h"

/* Steam is not supported: the client always runs without it. */
int __cdecl steam_init(void)
{
    return 0;
}

void __cdecl steam_shutdown(void)
{
}

void __cdecl steam_update(void)
{
}

/* There is no Steam name: the player name comes from the options. */
int __cdecl steam_getname(char *name)
{
    (void)name;
    return 0;
}

/* There is no Steam id: the id does not change. */
void __cdecl steam_getid(uint64_t *id)
{
    (void)id;
}

void __cdecl steam_requestticket(void *callback)
{
    (void)callback;
}

void __cdecl steam_requestauthticket(void)
{
}
