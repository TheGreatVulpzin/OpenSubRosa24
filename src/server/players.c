/* Players. */
#include <string.h>

#include "sr_runtime.h"
#include "sr_server.h"

/* Original: 024d RVA 0xa580 (fixplayername in the 0.24c ELF).
 * Clean the player name: replace '/' with a space, remove spaces at both ends,
 * and use "Noname" if the name is empty. The name has 32 bytes (31 + '\0'). */
void __cdecl sr_fixplayername(int id)
{
    char *name = sr_players[id].name;
    int i;

    for (i = 0; i < 31; i++) {
        if (name[i] == '/')
            name[i] = ' ';
    }
    name[31] = '\0';

    while (name[0] == ' ') {
        for (i = 0; i < 31 && name[i] != '\0'; i++)
            name[i] = name[i + 1];
    }

    i = (int)strlen(name) - 1;
    while (i >= 0 && name[i] == ' ') {
        name[i] = '\0';
        i--;
    }
    name[31] = '\0';

    if (name[0] == '\0')
        memcpy(name, "Noname", 7);
}
