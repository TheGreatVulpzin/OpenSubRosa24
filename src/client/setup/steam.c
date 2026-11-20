/* Steam availability and the cached play ticket.
 * Steam entry points return without calling the service APIs.
 * Player names and identifiers stay in local game state.
 */
#include "client.h"
#include "setup/steam.h"
#include "sr_runtime.h"

/* Clear the account identifier and return its storage. */
void *__fastcall CSteamID(uint64_t *id)
{
    *id = 0;
    return id;
}

/* Return all bits of the account identifier. */
uint64_t __fastcall ConvertToUint64(const uint64_t *id)
{
    return *id;
}

/* Return the cached play ticket identifier. */
uint64_t getplayticket(void)
{
    return (uint64_t)(uint32_t)cl_data_play_ticket[2] | ((uint64_t)(uint32_t)cl_data_play_ticket[3] << 32);
}

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
