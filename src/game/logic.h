/* Shared records and globals for the game logic. */
#ifndef SR_LOGIC_H
#define SR_LOGIC_H

#include "sr_runtime.h"

#define g_market sr_data_market
#define g_city sr_data_city
#define g_scenario sr_data_scenario
#define g_game_world sr_data_game_world
#define g_train_near sr_data_train_near
#define g_teamname sr_data_teamname
#define g_dollarbillvalue sr_data_dollarbillvalue
#define g_world_msg sr_data_world_msg
#define g_world_digits sr_data_world_digits
#define g_manager_msg sr_data_manager_msg
#define g_farbs_line sr_data_farbs_line
#define g_farbs_digits sr_data_farbs_digits
#define g_guard_used sr_data_guard_used
#define g_numofclientaccounts (*((int *)&sr_data_clientaccounts.count))
#define g_clientaccounts ((int (*)[4])((int *)&sr_data_clientaccounts.records))
#define g_numofheadlines (*((int *)&sr_data_newspaper.num_lines))
#define g_headlines ((char (*)[0x100])((char *)&sr_data_newspaper.lines))
#define g_modellist sr_data_modellist
#define g_texturelist sr_data_texturelist
#define g_materials ((MaterialRec *)&sr_data_materials.records)

void logic_player(int player);
void logic_playeractions(int player);
void logic_suits_buy(int player, int row);
void logic_gunstore_buy(int player, int row);
void logic_cardealer_buy(int player, int row);
int logic_createcar(int type, int team, int color);
void logic_setteammanager(int team, int player);
void logic_farbslist(int rank);
void resetgame(void);
void setupgame(void);
void logicsimulation(void);
void logicsimulation_driving(void);
void logicsimulation_world(void);
void logicsimulation_round(void);
void ai_assessthreats(int player, int human);
void playerai(int player);

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
