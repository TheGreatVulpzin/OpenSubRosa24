/* Functions of the server only (src/server) that the clean game code calls.
 * The client does not run the code that calls them: tools/re/check_adopted.py keeps those
 * functions out of the client. These empty versions only satisfy the linker. */
#include "sr_server.h"

void __cdecl sr_createevent_message(int kind, const char *text, int target, int extra)
{
    (void)kind;
    (void)text;
    (void)target;
    (void)extra;
}

void __cdecl sr_createevent_updatedoor(int a, int b)
{
    (void)a;
    (void)b;
}

void __cdecl sr_createevent_updatemission(int team, int mission)
{
    (void)team;
    (void)mission;
}

void __cdecl sr_fixplayername(int id)
{
    (void)id;
}
