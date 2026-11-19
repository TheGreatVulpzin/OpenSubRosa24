/* The mission set that a client makes for a prize item: one item chase or a guarded building,
 * then one mission for each team.
 *
 * The function has the prefix client_ in C because the server code of the same name is in the
 * same program. The client ends with the id of the next scenario as its result and calls its own
 * chase and guard builders.
 */
#include "setup/missions.h"

#include "client.h"
#include "items.h"
#include "logic.h"
#include "sr_os.h"
#include "sr_runtime.h"

#define MISSION_SECONDS 0x4ab50
#define MISSION_EXPIRE_LIMIT 0x3b0d30
#define BUILDING_TRIES 0x400
#define BUILDING_MISSION 3
#define BUILDING_RECORD 0x9dc
#define FIRST_ITEM_TYPE 0x11
#define LAST_ITEM_TYPE 0x16
#define KIND_BUILDING 0
#define KIND_CHASE 1
#define TYPE_SHARES 0x15
#define CASH_STEP 500
#define CASH_BASE 1000
#define CASH_GROWTH 2.499999936844688e-05

/* Game functions of other files. */
#define scenario_createchase_client CL_FN(0x4dd100, int(__cdecl *)(int item_type, int expire))
#define scenario_createguards_client CL_FN(0x4dca90, void(__cdecl *)(int building, int item_type, int expire))

/* The type of the building at an index: 3 is a mission building. */
static int building_kind(int building)
{
    return *(const int *)((const uint8_t *)&g_city.building_type + building * BUILDING_RECORD);
}

/* Make the missions of a prize item. kind 0 puts the prize in a mission building and kind 1 makes
 * an item chase. Each team in teams gets one mission. The result is the id of the next scenario.
 * A kind 0 set with no mission building ends early with a number that no caller reads. */
int __cdecl client_createmission_acquisition(int kind, int n, const int *teams)
{
    int expire = g_game_world.clock + MISSION_SECONDS;
    int type, building = 0, tries, quotient = 0, i;

    if (expire > MISSION_EXPIRE_LIMIT)
        expire = MISSION_EXPIRE_LIMIT;
    type = g_scenario.item_cycle + FIRST_ITEM_TYPE;
    g_scenario.item_cycle++;
    if (g_scenario.item_cycle + FIRST_ITEM_TYPE > LAST_ITEM_TYPE)
        g_scenario.item_cycle = 0;
    if (kind == KIND_CHASE)
        scenario_createchase_client(type, expire);
    if (kind == KIND_BUILDING) {
        int r;

        if (g_city.num_buildings == 0)
            return type + 1;
        r = os_rand();
        quotient = r / g_city.num_buildings;
        building = r % g_city.num_buildings;
        for (tries = 0; tries < BUILDING_TRIES && building_kind(building) != BUILDING_MISSION;
             building = r % g_city.num_buildings) {
            r = os_rand();
            quotient = r / g_city.num_buildings;
            tries++;
        }
        if (building_kind(building) != BUILDING_MISSION)
            return quotient;
        scenario_createguards_client(building, type, expire);
    }
    for (i = 0; i < n; i++) {
        int team = teams[i];
        int slot = scenario_getnextmission(team);
        Mission *m = &team_missions(team)[slot];

        m->active = 1;
        m->scenario = g_scenario.next_id;
        m->kind = kind;
        m->item = g_scenario.items[0];
        m->item_type = sr_items[g_scenario.items[0]].type;
        m->cash = ((int)((unsigned)os_rand() & 3)) * CASH_STEP + CASH_BASE;
        if (type == TYPE_SHARES)
            m->cash *= 5;
        m->own_growth = (float)((float)m->cash * CASH_GROWTH);
        m->other_growth = 0;
        m->expire = expire;
        m->site = g_scenario.site;
        m->stake = 0;
        sr_createevent_updatemission(team, slot);
    }
    g_scenario.next_id++;
    return g_scenario.next_id;
}
