/* Round timers, world time and team membership. */
#include "logic.h"

#include <stdio.h>
#include <string.h>

#include "collide.h"
#include "economy.h"
#include "events.h"
#include "items.h"
#include "humans.h"
#include "objects.h"
#include "player.h"
#include "scenario.h"
#include "server.h"
#include "sr_os.h"
#include "sr_server.h"
#include "system.h"
#include "util.h"
#include "vectors.h"

static void reset_purchases(void)
{
    int i;
    for (i = 0; i < 256; ++i) {
        sr_players[i].num_purchases = 0;
        if (sr_players[i].num_cars > 0)
            --sr_players[i].num_cars;
        sr_players[i].small_bills = 0;
    }
}

static void expire_bot(Player *p)
{
    int human = p->human_id;
    if (human != -1) {
        Human *h = &sr_humans[human];
        int slot;
        if (h->vehicle_id != -1)
            sr_objects[h->vehicle_id].time_to_live = 0;
        for (slot = 0; slot < 8; ++slot) {
            int count = h->inventory[slot].count, j;
            for (j = 0; j < count; ++j) {
                int item = h->inventory[slot].items[j];
                if (sr_items[item].num_children > 0) {
                    int child = sr_items[item].children[0];
                    sr_items[child].time_to_live = 0;
                    item_removeitemlink(child, item);
                }
                sr_items[item].time_to_live = 0;
            }
        }
        h->despawn_timer = 0;
        h->player_id = -1;
    }
    p->active = 0;
}

static void world_test_items(void)
{
    static const int types[9] = {23, 23, 23, 2, 3, 3, 16, 26, 12};
    vec3 pos, rot[3];
    int j;
    pos.x = (float)((double)g_game_world.spawn.x + 4.0);
    pos.y = g_game_world.spawn.y;
    pos.z = (float)((double)g_game_world.spawn.z + 16.0);
    resetorientation(rot);
    for (j = 0; j < 9; ++j) {
        int item = createitem(types[j], &pos, NULL, rot);
        if (j < 3)
            sr_items[item].phone_id = (j + 1) * 0x457;
        if (j == 6) {
            sr_items[item].bills_mask = 0x01001001u;
            sr_items[item].last_bill = 9;
        }
        pos.z = (float)((double)pos.z + 1.0);
    }
    createobject(11, &pos, NULL, rot, 0);
}

void logicsimulation_world(void)
{
    int i, team, slot, stale;
    uint32_t phase;
    for (i = 0; i < 256; ++i) {
        Player *p = &sr_players[i];
        if (p->active && p->is_bot) {
            if (p->human_id == -1)
                p->active = 0;
            if (p->despawn_time != 0 && p->despawn_time < g_game_world.clock)
                expire_bot(p);
        }
    }
    if (sr_game.state == 1) {
        sr_game.state = 2;
        sr_game.timer = sr_game.round_time;
        g_game_world.clock = 0x1a5e00;
        scenario_createtraffic2(32);
        if (sr_debugit)
            world_test_items();
    }
    if (sr_game.state != 2)
        return;
    g_game_world.clock += 10;
    if (g_game_world.clock > 0x3b537f) {
        for (i = 0; i < 256; ++i)
            if (sr_players[i].active)
                player_calcassets(i);
        saveaccountsserver();
        resetgame();
        return;
    }
    stale = 16;
    for (team = 0; team < 5; ++team) {
        g_market.stocks[team].price_int = (int)g_market.stocks[team].price;
        for (slot = 0; slot < 16; ++slot) {
            Mission *m = &team_missions(team)[slot];
            if (m->active && m->expire < g_game_world.clock)
                scenario_setmissioninactive(m->scenario);
        }
    }
    if (g_game_world.clock % 36000 == 0)
        for (i = 0; i < sr_numofaccounts; ++i)
            if (sr_accounts[i].ban_time > 0)
                --sr_accounts[i].ban_time;
    if (g_game_world.clock % 18000 == 0) {
        economy_carmarket();
        economy_gunmarket();
        saveaccountsserver();
        for (i = 0; i < 256; ++i)
            if (sr_players[i].active && sr_players[i].account != -1)
                sr_accounts[sr_players[i].account].playtime += 5;
        for (i = 0; i < sr_numofaccounts; ++i)
            if (sr_accounts[i].name_locked > 0)
                --sr_accounts[i].name_locked;
        server_writelogfile();
        createevent_updatestocks();
    }
    phase = (uint32_t)(g_game_world.clock % 0x1a5e0 - 54000);
    if (phase < 0x4b1 && (int)phase % 120 == 0)
        logic_farbslist((int)phase / 120 - 1);
    phase = (uint32_t)(g_game_world.clock % 0x1a5e0);
    if (phase < 600 && phase % 120 == 0) {
        int manager;
        team = (int)(phase / 120);
        if (team > 4)
            team = 4;
        manager = *(int *)TEAM_FIELD(team0_manager, team);
        if (manager == -1)
            sprintf(g_world_msg, "%s open", g_teamname[team]);
        else
            sprintf(g_world_msg, "%s manager: %s", g_teamname[team], sr_players[manager].name);
        sr_createevent_message(0, g_world_msg, -1, 0);
        if (g_market.stocks[team].shares > 0) {
            sprintf(g_world_msg, "%s market cap: $", g_teamname[team]);
            stale = (int)((double)g_market.stocks[team].price * g_market.stocks[team].shares);
            sprintf_comma(g_world_digits, stale);
            strcat(g_world_msg, g_world_digits);
            sr_createevent_message(0, g_world_msg, -1, 0);
        }
    }
    if (g_game_world.clock % 54000 == 0) {
        reset_purchases();
        createscenario_world();
    }
    if (sr_game.num_newspapers < 8) {
        vec3 pos, rot[3];
        pos.x = (float)((double)g_game_world.spawn.x - 4.0);
        pos.y = g_game_world.spawn.y;
        pos.z = (float)((double)g_game_world.spawn.z + 1.0);
        resetorientation(rot);
        while (sr_game.num_newspapers < 8) {
            createitem(26, &pos, NULL, rot);
            ++sr_game.num_newspapers;
        }
    }
    for (i = 0; i < 256; ++i)
        if (sr_players[i].active && !sr_players[i].is_bot && sr_players[i].human_id == -1)
            sr_players[i].menu = 1;
    for (i = 0; i < 256; ++i) {
        Player *p = &sr_players[i];
        Human *h;
        if (!p->active || p->human_id == -1)
            continue;
        h = &sr_humans[p->human_id];
        team = p->team;
        if (team >= 0 && team < 5) {
            if (!(p->input_buttons & 0x40000)) {
                p->leave_ticks = 0;
            } else if (!pointinsidebase(&h->pos.x, team, 0) && ++p->leave_ticks > 0xb3) {
                int *manager = (int *)TEAM_FIELD(team0_manager, team);
                playersellstock(i, p->stocks);
                if (*manager == i)
                    *manager = -1;
                p->team = 7;
                /* The body receives the value left by the earlier loops. */
                h->team = stale;
                createevent_updateplayer(i);
                createevent_updateplayer_finance(i);
            }
            team = p->team;
            if (*(int *)TEAM_FIELD(team0_manager, team) == -1 &&
                pointinsidebasesafe(&h->pos.x, team, 0))
                logic_setteammanager(team, i);
        }
        if (p->team == 7) {
            for (team = 0; team < 5; ++team)
                if (pointinsidebasesafe(&h->pos.x, team, 0)) {
                    int manager = *(int *)TEAM_FIELD(team0_manager, team);
                    h->protect_timer = 0;
                    if (manager == -1 || (sr_players[manager].human_id != -1 &&
                        pointinsidebasesafe(&sr_humans[sr_players[manager].human_id].pos.x, team, 0))) {
                        h->team = team;
                        p->team = team;
                        createevent_updateplayer(i);
                    }
                }
            stale = 5;
        }
    }
    for (team = 0; team < 5; ++team)
        for (slot = 0; slot < 16; ++slot) {
            Mission *m = &team_missions(team)[slot];
            if (m->active && sr_items[m->item].time_to_live > 0)
                sr_items[m->item].time_to_live = 0xffff;
        }
}

static void round_phones(int team, int id)
{
    vec3 pos, rot[3];
    int item;
    float angle;
    safe_center(team, &pos);
    resetorientation(rot);
    item = createitem(23, &pos, NULL, rot);
    if (item != -1) {
        sr_items[item].phone_line = 0;
        sr_items[item].phone_id = id - 1;
        createevent_updateiteminfo(item);
    }
    angle = (float)((double)*(float *)TEAM_FIELD(team0_angle, team) - 1.570796326795);
    rotateorientation(rot, &rot[1], angle);
    pos.x = (float)((double)pos.x + (double)rot[0].x * 0.5);
    pos.y = (float)((double)pos.y + (double)rot[0].y * 0.5);
    pos.z = (float)((double)pos.z + (double)rot[0].z * 0.5);
    item = createitem(23, &pos, NULL, rot);
    if (item != -1) {
        sr_items[item].phone_line = 1;
        sr_items[item].phone_id = id;
        createevent_updateiteminfo(item);
    }
}

void logicsimulation_round(void)
{
    int i;
    for (i = 0; i < 256; ++i)
        if (sr_players[i].active && sr_players[i].countdown > 0)
            --sr_players[i].countdown;
    if (sr_game.state == 1) {
        int all_ready = 1, count = 0, t = sr_game.timer, team;
        for (i = 0; i < 256; ++i) {
            Player *p = &sr_players[i];
            if (!p->active || p->is_bot)
                continue;
            team = p->team;
            if (team >= 0 && team < 3)
                ++count;
            if (!p->ready)
                all_ready = 0;
            if (t < 0x744 && p->human_id == -1 && team != -1 && team < 3) {
                vec3 pos, rot[3];
                float angle;
                int human;
                safe_center(team, &pos);
                pos.x = (float)((double)((os_rand() & 255) - 127) * 4.0 * 0.0078125 + pos.x);
                pos.z = (float)((double)((os_rand() & 255) - 127) * 4.0 * 0.0078125 + pos.z);
                angle = (float)((double)*(float *)TEAM_FIELD(team0_angle, team) - 1.570796326795);
                resetorientation(rot);
                rotateorientation(rot, &rot[1], angle);
                human = createhuman(&pos.x, &rot[0].x, i);
                p->human_id = human;
                sr_humans[human].axis = angle;
                p->menu = 0;
                player_loadinventory(i);
                t = sr_game.timer;
            }
        }
        if (!count)
            all_ready = 0;
        if (t == sr_game.wait_time)
            for (team = 0; team < 3; ++team)
                sr_createevent_updatedoor(team, 0);
        sr_game.timer = t - 1;
        if (count == 0 && (uint32_t)(t - 0xbba) < 299)
            sr_game.timer = 0xce4;
        if (all_ready && sr_game.timer > 0x744) {
            sr_game.timer = 0x744;
            return;
        }
        if (sr_game.timer == 0x708) {
            createscenario_world();
            if (sr_game.timer == 0x708) {
                scenario_createtraffic2(24);
                for (team = 0; team < 3; ++team) {
                    int slot;
                    logic_createcar(0, team, 0);
                    if (*(int *)TEAM_FIELD(team0_members, team) > 4)
                        logic_createcar(8, team, 0);
                    for (slot = 0; slot < 16; ++slot) {
                        Mission *m = &team_missions(team)[slot];
                        if (m->active && m->kind == 4) {
                            TeamCarSpot *spot = (TeamCarSpot *)TEAM_FIELD(team0_car_spots, team);
                            vec3 pos;
                            pos.x = (float)((double)spot->pos.x - (double)spot->rot[2].x * 8.0);
                            pos.y = (float)((double)(float)((double)spot->pos.y + 20.0) - (double)spot->rot[2].y * 8.0);
                            pos.z = (float)((double)spot->pos.z - (double)spot->rot[2].z * 8.0);
                            createobject(11, &pos, NULL, spot->rot, 0);
                            break;
                        }
                    }
                    round_phones(team, 0x458 + team * 0x457);
                }
            }
        }
        if (sr_game.timer < 1) {
            for (team = 0; team < 3; ++team)
                sr_createevent_updatedoor(team, 1);
            sr_game.state = 2;
            sr_game.timer = sr_game.round_time;
            return;
        }
    } else if (sr_game.state == 2) {
        int t = --sr_game.timer, no_money = 1, all_out = 1, team, slot;
        if (t % 0xe10 == 0)
            reset_purchases();
        for (i = 0; i < 1024; ++i)
            if (sr_items[i].active && sr_items[i].type == 16)
                no_money = 0;
        for (team = 0; team < 3; ++team)
            if (*(int *)TEAM_FIELD(team0_members, team) > 0)
                for (slot = 0; slot < 16; ++slot) {
                    Mission *m = &team_missions(team)[slot];
                    if (m->active) {
                        no_money = 0;
                        if (sr_items[m->item].time_to_live > 0)
                            sr_items[m->item].time_to_live = 0xffff;
                    }
                }
        if (no_money && t > 60) {
            t = 60;
            sr_game.timer = 60;
        }
        for (i = 0; i < 256; ++i)
            if (sr_players[i].active && !sr_players[i].is_bot && sr_players[i].human_id != -1)
                all_out = 0;
        if (all_out && t > 60) {
            sr_game.timer = 60;
            return;
        }
        if (t < 1) {
            sr_game.state = 3;
            sr_game.timer = sr_game.end_time;
            return;
        }
    } else if (sr_game.state == 3 && --sr_game.timer < 1) {
        for (i = 0; i < 256; ++i)
            if (sr_players[i].active && !sr_players[i].is_bot)
                player_saveinventory(i);
        resetgame();
    }
}
