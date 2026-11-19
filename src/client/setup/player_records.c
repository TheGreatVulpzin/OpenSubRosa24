/* Small updates of the player records and the mission lists of the client. */
#include "setup/player_records.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"

#define NUM_PLAYERS 0x100
#define NUM_MISSIONS 16
#define MISSION_SEEK 4

/* Add a waypoint to the list of a player. The three coordinates are floats given as their 32 bits and
 * stored as they are. The list has no limit check: the 65th waypoint writes past it. */
void __cdecl addplayerwaypoint(int player, uint32_t x_bits, uint32_t y_bits, uint32_t z_bits)
{
    Player *p = &sr_data_player[player];
    AiWaypoint *waypoint = &p->waypoints[p->num_path];

    memcpy(&waypoint->pos.x, &x_bits, sizeof x_bits);
    memcpy(&waypoint->pos.y, &y_bits, sizeof y_bits);
    memcpy(&waypoint->pos.z, &z_bits, sizeof z_bits);
    p->num_path++;
}

/* Give each active player of a team an equal part of an amount of cash. The part is rounded toward
 * zero. A team with no player gets nothing. */
void __cdecl logic_splitteamcash(int team, int amount)
{
    int count = 0;
    int i;

    for (i = 0; i < NUM_PLAYERS; i++)
        if (sr_data_player[i].active != 0 && sr_data_player[i].team == team)
            count++;
    if (count > 0)
        for (i = 0; i < NUM_PLAYERS; i++)
            if (sr_data_player[i].active != 0 && sr_data_player[i].team == team)
                sr_data_player[i].money += amount / count;
}

/* Tell if a team has an active mission of the seek kind. */
int __cdecl scenario_isseekmission(int team)
{
    const Mission *missions = (const Mission *)((const uint8_t *)&sr_data_game.team0_missions + team * 0x117c);
    int i;

    for (i = 0; i < NUM_MISSIONS; i++)
        if (missions[i].active != 0 && missions[i].kind == MISSION_SEEK)
            return 1;
    return 0;
}
