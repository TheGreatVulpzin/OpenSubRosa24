/* Mission rewards, stakes and team assignment. */
#include "scenario.h"

#include <string.h>

#include "economy.h"
#include "items.h"
#include "logic.h"
#include "sr_os.h"
#include "sr_server.h"
#include "vectors.h"

static int next_item_type(void)
{
    int type = g_scenario.item_cycle + 17;
    ++g_scenario.item_cycle;
    if (g_scenario.item_cycle + 17 > 22)
        g_scenario.item_cycle = 0;
    return type;
}

static int mission_expire(int duration)
{
    int expire = g_game_world.clock + duration;
    return expire > 0x3b0d30 ? 0x3b0d30 : expire;
}

void createmission_threeway(int unused, int n, const int *teams)
{
    int doubled = 1, expire, type, rewards[64], i;
    vec3 pos, rot[3];
    (void)unused;
    if (n <= 1)
        return;
    if ((os_rand() & 1) == 0)
        doubled = 2;
    if (n > 3 && (os_rand() & 1) == 0)
        ++doubled;
    if (doubled > n - 1)
        doubled = n - 1;
    expire = mission_expire(0x65130);
    type = next_item_type();
    rewards[0] = 0;
    for (i = 1; i < n; ++i) {
        int j, reward = 1000;
        if ((os_rand() & 1) == 0)
            reward = 1500;
        for (j = 0; j < 3; ++j)
            if ((os_rand() & 3) == 0)
                reward += 500;
        if (type == 21)
            reward *= 5;
        rewards[i] = reward;
    }
    safe_center(teams[0], &pos);
    pos.x = (float)((double)(g_scenario.next_id & 7) * 0.75 + pos.x);
    pos.z = (float)((double)pos.z - 1.0);
    resetorientation(rot);
    g_scenario.items[0] = createitem(type, &pos, NULL, rot);
    for (i = 0; i < n; ++i) {
        int team = teams[i];
        int slot = scenario_getnextmission(team);
        Mission *m = &team_missions(team)[slot];
        float cash;
        m->active = 1;
        m->scenario = g_scenario.next_id;
        m->kind = i == 0 ? 2 : (doubled < i) + 3;
        m->item = g_scenario.items[0];
        m->item_type = sr_items[m->item].type;
        m->cash = i == 0 ? 0 : rewards[i] * (i <= doubled ? 2 : 1);
        cash = (float)m->cash;
        m->own_growth = (float)((double)cash * 2.499999936844688e-05);
        m->other_growth = i == 0 ? 0 : (float)(-(double)cash * 1.249999968422344e-05);
        m->expire = expire;
        m->partner = teams[0];
        m->stake = 0;
        if (i > 0 && i <= doubled) {
            Stock *stock = &g_market.stocks[team];
            int r = os_rand() % (rewards[i] / 750 + 1) + rewards[i] / 1000;
            int value;
            if (r < 1)
                r = 1;
            value = r * 500;
            if (stock->shares < 1) {
                m->stake_min = 0;
            } else {
                int minimum = ((25 - (int)((double)stock->price * stock->shares * value *
                                           -2.499999936844688e-05)) / 50) * 50;
                m->stake_min = minimum;
                if (value < minimum)
                    value = minimum;
            }
            stock->stake += value;
            m->stake = value;
        }
        sr_createevent_updatemission(team, slot);
    }
    ++g_scenario.next_id;
}

static void transaction_mission(int team, int slot, int kind, int id, int item,
                                int reward, int expire, int partner)
{
    Mission *m = &team_missions(team)[slot];
    float cash = (float)reward;
    m->scenario = id;
    m->active = 1;
    m->kind = kind;
    m->item = item;
    m->item_type = sr_items[item].type;
    m->cash = reward;
    m->expire = expire;
    m->own_growth = (float)((double)cash * 2.499999936844688e-05);
    m->other_growth = (float)(-(double)cash * 1.249999968422344e-05);
    m->partner = partner;
    m->stake = 0;
    sr_createevent_updatemission(team, slot);
}

void createmission_transaction(int unused, int n, const int *teams)
{
    int types[3], rewards[3], ids[3], expire, k, i;
    vec3 rot[3];
    (void)unused;
    if (n <= 2)
        return;
    expire = mission_expire(0x65130);
    for (k = 0; k < 3; ++k) {
        types[k] = next_item_type();
        rewards[k] = 1000;
        if ((os_rand() & 1) == 0)
            rewards[k] = 1500;
        if ((os_rand() & 1) == 0)
            rewards[k] += 500;
        if ((os_rand() & 3) == 0)
            rewards[k] += 500;
        if ((os_rand() & 3) == 0)
            rewards[k] += 1000;
        if (types[k] == 21)
            rewards[k] *= 5;
    }
    resetorientation(rot);
    for (k = 0; k < 3; ++k) {
        vec3 pos;
        safe_center(teams[k], &pos);
        g_scenario.items[k] = createitem(types[k], &pos, NULL, rot);
        ids[k] = g_scenario.next_id++;
    }
    for (i = 0; i < n; ++i) {
        int team = teams[i];
        int j = i + 1;
        if (j > 2)
            j %= 3;
        transaction_mission(team, scenario_getnextmission(team), (i > 2) + 5,
                            ids[j], g_scenario.items[j], rewards[j], expire, teams[j]);
        if (i == 0 && (os_rand() & 7) == 0)
            transaction_mission(team, scenario_getnextmission(team), 6, ids[2],
                                g_scenario.items[2], rewards[2], expire, teams[2]);
    }
}

int createmission_acquisition(int kind, int n, const int *teams)
{
    int expire = mission_expire(0x4ab50);
    int type = next_item_type();
    int result = kind, i;
    if (kind == 1) {
        result = scenario_createchase(type, expire);
    } else if (kind == 0) {
        int building, tries = 0, quotient = 0;
        if (g_city.num_buildings == 0)
            return 0;
        building = os_rand() % g_city.num_buildings;
        while (*(int *)((uint8_t *)&g_city.building_type + building * 0x9dc) != 3 && tries < 0x400) {
            int r = os_rand();
            quotient = r / g_city.num_buildings;
            building = r % g_city.num_buildings;
            ++tries;
        }
        if (*(int *)((uint8_t *)&g_city.building_type + building * 0x9dc) != 3)
            return quotient;
        result = scenario_createguards(building, type, expire);
    }
    for (i = 0; i < n; ++i) {
        int team = teams[i];
        int slot = scenario_getnextmission(team);
        Mission *m = &team_missions(team)[slot];
        m->scenario = g_scenario.next_id;
        m->item = g_scenario.items[0];
        m->item_type = sr_items[m->item].type;
        m->active = 1;
        m->kind = kind;
        m->cash = ((os_rand() & 3) + 2) * 500;
        if (type == 21)
            m->cash *= 5;
        m->expire = expire;
        m->site = g_scenario.site;
        m->own_growth = (float)((double)m->cash * 2.499999936844688e-05);
        m->stake = 0;
        m->other_growth = 0;
        result = m->cash;
        sr_createevent_updatemission(team, slot);
        if (sr_server)
            result = (int)((uint32_t)(expire / 60) + ((uint32_t)m->site + 1) * 0x10000u);
    }
    ++g_scenario.next_id;
    return result;
}

void createscenario_world(void)
{
    int team;
    g_scenario.avg_members = 0;
    g_scenario.num_teams = 0;
    for (team = 0; team < 5; ++team) {
        int members = *(int *)TEAM_FIELD(team0_members, team);
        if (members > 0) {
            int missions = scenario_getnumofmissions(team);
            if (missions < 1 || (g_game_world.clock % 0x34bc0 == 0 && missions < 2)) {
                g_scenario.teams[g_scenario.num_teams++] = team;
                g_scenario.avg_members += members;
            }
        }
    }
    if (g_scenario.num_teams > 0)
        g_scenario.avg_members /= g_scenario.num_teams;
    if (g_scenario.avg_members < 1)
        g_scenario.avg_members = 1;
    for (team = 0; team < 5; ++team)
        if (*(int *)TEAM_FIELD(team0_members, team) == 0)
            sr_createevent_updatedoor(team, 1);
    if (g_scenario.num_teams == 0 || g_game_world.clock >= 0x38dab1)
        return;
    if (g_scenario.num_teams > 1) {
        int k;
        for (k = 0; k < 1024; ++k) {
            int a = os_rand() % g_scenario.num_teams;
            int b = os_rand() % g_scenario.num_teams;
            int tmp = g_scenario.teams[b];
            g_scenario.teams[b] = g_scenario.teams[a];
            g_scenario.teams[a] = tmp;
        }
    }
    do {
        int n = g_scenario.num_teams;
        int list[64], mode = 1, acquisition = 1, i;
        memcpy(list, g_scenario.teams, n * sizeof(int));
        if (g_game_world.clock % 0x34bc0 == 0 || sr_game.gametype == 3) {
            int limit;
            if (g_scenario.num_teams > 1 && (os_rand() & 7) < 5) {
                mode = 0;
                if ((os_rand() & 1) == 0 && n > 3)
                    n = 3;
                if ((os_rand() & 15) == 0 && n > 2)
                    n = 2;
            }
            limit = n == 3 ? 2 : 4;
            if (mode == 0) {
                createmission_threeway(0, n, list);
                acquisition = 0;
            } else if (n >= 3 && (unsigned)(os_rand() & 7) < (unsigned)limit) {
                if ((os_rand() & 3) == 0 && n > 4)
                    n = 4;
                if ((os_rand() & 7) == 0 && n > 3)
                    n = 3;
                createmission_transaction(2, n, list);
                acquisition = 0;
            }
        }
        if (acquisition) {
            int kind;
            if ((os_rand() & 3) < 2 && n > 3)
                n = 3;
            if ((os_rand() & 3) == 0 && n > 2)
                n = 2;
            if ((os_rand() & 3) == 0 && n > 1)
                n = 1;
            kind = (os_rand() & 7) > 2;
            createmission_acquisition(kind, n, list);
        }
        ++sr_stats.numofmissions;
        for (i = n; i < g_scenario.num_teams; ++i)
            g_scenario.teams[i - n] = g_scenario.teams[i];
        g_scenario.num_teams -= n;
    } while (g_scenario.num_teams > 0);
}
