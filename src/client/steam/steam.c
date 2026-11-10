/* The Steam interface of the client. The client calls steam_api.dll through imports. After
 * steam_init succeeds, the other functions read the name and the id of the player and run the
 * Steam callbacks. When Steam is off they do nothing.
 */
#include "steam/steam.h"

#include <string.h>

#include "sr_runtime.h"

void *sr_import(const char *dll, const char *name);

#define g_steam_on cl_data_steam_on

/* Virtual function slots of the Steam interfaces. */
#define FRIENDS_GET_NAME 0
#define USER_GET_ID 2

typedef char(__cdecl *SteamBoolFn)(void);
typedef char(__cdecl *SteamRestartFn)(unsigned app_id);
typedef void *(__cdecl *SteamInterfaceFn)(void);
typedef const char *(__fastcall *GetNameFn)(void *self, int unused);
typedef const uint64_t *(__fastcall *GetIdFn)(void *self, int unused, uint64_t *result);

static void *steam_function(const char *name)
{
    return sr_import("STEAM_API.DLL", name);
}

/* Start the Steam interface. The result is 1 when it runs. Steam may restart the game
 * through the Steam client first, and then the result is 1 as well. */
int __cdecl steam_init(void)
{
    if (((SteamRestartFn)steam_function("SteamAPI_RestartAppIfNecessary"))(0) == 0) {
        if (((SteamBoolFn)steam_function("SteamAPI_Init"))() == 0)
            return 0;
    }
    g_steam_on = 1;
    return 1;
}

void __cdecl steam_shutdown(void)
{
    ((void(__cdecl *)(void))steam_function("SteamAPI_Shutdown"))();
    g_steam_on = 0;
}

/* Run the Steam callbacks. */
void __cdecl steam_update(void)
{
    if (g_steam_on != 0)
        ((void(__cdecl *)(void))steam_function("SteamAPI_RunCallbacks"))();
}

/* Copy the Steam name of the player into name. The result is 1 when Steam is on. */
int __cdecl steam_getname(char *name)
{
    if (g_steam_on == 0)
        return 0;
    {
        void **friends = ((SteamInterfaceFn)steam_function("SteamFriends"))();
        const char *text = ((GetNameFn *)*friends)[FRIENDS_GET_NAME](friends, 0);
        strcpy(name, text);
    }
    return 1;
}

/* Write the 64 bit Steam id of the player into id. Nothing is written when Steam is off. */
void __cdecl steam_getid(uint64_t *id)
{
    uint64_t result;

    if (g_steam_on == 0)
        return;
    {
        void **user = ((SteamInterfaceFn)steam_function("SteamUser"))();
        const uint64_t *value = ((GetIdFn *)*user)[USER_GET_ID](user, 0, &result);
        *id = *value;
    }
}
