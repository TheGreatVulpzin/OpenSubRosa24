/* Shared records and globals for the game logic. */
#ifndef SR_LOGIC_H
#define SR_LOGIC_H

#include "sr_runtime.h"

#define g_market (*SR_AT(Market, SR_RVA_MARKET))
#define g_city (*SR_AT(City, SR_RVA_CITY))
#define g_scenario (*SR_AT(Scenario, SR_RVA_SCENARIO))
#define g_game_world (*SR_AT(GameWorld, SR_RVA_GAME_WORLD))
#define g_train_near (*SR_AT(int, 0x1952d668))
#define g_teamname SR_AT(const char *, 0x60438)
#define g_dollarbillvalue SR_AT(int, 0x60458)
#define g_world_msg SR_AT(char, 0x617c0)
#define g_world_digits SR_AT(char, 0x203ee0)
#define g_manager_msg SR_AT(char, 0x61780)
#define g_farbs_line SR_AT(char, 0x203fa0)
#define g_farbs_digits SR_AT(char, 0x61d48)
#define g_guard_used SR_AT(int, 0x203de0)
#define g_numofclientaccounts (*SR_AT(int, 0x1b497a0))
#define g_clientaccounts ((int (*)[4])SR_AT(int, 0x1b497a4))
#define g_numofheadlines (*SR_AT(int, 0x9bdd8a0))
#define g_headlines ((char (*)[0x100])SR_AT(char, 0x9bdd8a4))
#define g_modellist SR_AT(int, 0x17529aa0)
#define g_texturelist SR_AT(int, 0x13e22e80)
#define g_materials ((uint8_t (*)[0x58])SR_AT(uint8_t, 0x16d50024))

/* Each team occupies 0x117c bytes in the game state. */
#define TEAM_FIELD(field, team) \
    ((void *)((uint8_t *)&sr_game.field + (team) * 0x117c))

static __inline Mission *team_missions(int team)
{
    return (Mission *)TEAM_FIELD(team0_missions, team);
}

/* A full list uses slot 16, directly after the mission array. */
static __inline int scenario_getnextmission(int team)
{
    int slot;
    Mission *missions = team_missions(team);
    for (slot = 0; slot < 16; ++slot)
        if (!missions[slot].active)
            break;
    return slot;
}

static __inline void safe_center(int team, vec3 *out)
{
    const vec3 *min = (const vec3 *)TEAM_FIELD(team0_safe_min, team);
    const vec3 *max = (const vec3 *)TEAM_FIELD(team0_safe_max, team);
    out->x = (float)((double)(float)((double)min->x + max->x) * 0.5);
    out->y = (float)((double)(float)((double)min->y + max->y) * 0.5);
    out->z = (float)((double)(float)((double)min->z + max->z) * 0.5);
}

#endif
