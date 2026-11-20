/* Queries of team mission slots.
 * Each team has sixteen slots. The active flag marks a slot in use.
 * A full list returns its size when a free slot is requested.
 */
#include "logic.h"

/* Return the first free mission slot, or sixteen when all are active. */
int scenario_getnextmission(int team)
{
    int slot;
    Mission *missions = team_missions(team);
    for (slot = 0; slot < 16; ++slot)
        if (!missions[slot].active)
            break;
    return slot;
}

/* Return one if an active mission has the search kind. */
int scenario_isseekmission(int team)
{
    int slot;
    Mission *missions = team_missions(team);
    for (slot = 0; slot < 16; ++slot)
        if (missions[slot].active && missions[slot].kind == 4)
            return 1;
    return 0;
}
