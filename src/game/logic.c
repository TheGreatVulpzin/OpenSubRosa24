/* Player menus, shops and the game logic tick. */
#include "logic.h"

#include <stdio.h>
#include <string.h>

#include "collide.h"
#include "economy.h"
#include "events.h"
#include "humans.h"
#include "items.h"
#include "objects.h"
#include "server.h"
#include "sr_math.h"
#include "sr_os.h"
#include "sr_server.h"
#include "util.h"
#include "vectors.h"

#define g_yaxis (*SR_AT(vec3, 0x60ce0))
#define g_soundlist (*SR_AT(SoundList, SR_RVA_SOUNDLIST))
#define BUILDING_FIELD(field, building) \
    ((void *)((uint8_t *)&g_city.field + (building) * 0x9dc))

static int *shop_row(int building, int row)
{
    return (int *)BUILDING_FIELD(shop_rows, building) + (row - 1) * 3;
}

static void put_item(int item, int human, int first, int last)
{
    int slot;
    for (slot = first; slot <= last; ++slot)
        if (linkitem(item, UINT32_MAX, human, slot))
            break;
}

void logic_player(int player)
{
    uintptr_t image_base = sr_base;
    Player *p = (Player *)(image_base + SR_RVA_PLAYER) + player;
    const City *city = (const City *)(image_base + SR_RVA_CITY);
    const uint8_t *types = (const uint8_t *)&city->building_type;
    int human = p->human_id;
    int count = city->num_buildings, building, bank = -1;
    Human *h;
    if ((uint32_t)human >= 512)
        return;
    h = (Human *)(image_base + SR_RVA_HUMAN) + human;
    for (building = 0; building < count; ++building) {
        int type = *(const int *)(types + building * 0x9dc);
        if ((type == 2 || type == 4 || type == 7) &&
            pointinsidebuilding(building, &h->pos.x, 0)) {
            if (!p->menu) {
                p->menu_building = building;
                p->menu = 9;
            }
            return;
        }
        /* A later shop takes priority over this bank. */
        if (type == 5 && bank == -1 && pointinsidebuilding(building, &h->pos.x, 0))
            bank = building;
    }
    if (bank != -1) {
        if (!p->menu) {
            p->menu_building = bank;
            p->menu = 12;
        }
        return;
    }
    if (p->team < 5 && pointinsidebase(&h->pos.x, p->team, 0)) {
        if (!p->menu)
            p->menu = 3;
    } else {
        p->menu = 0;
    }
}

void logic_suits_buy(int player, int row)
{
    Player *p = &sr_players[player];
    int price = shop_row(p->menu_building, row)[1];
    if (price > p->money)
        return;
    if (row == 1 && !p->look_d) {
        p->look_d = 1;
        if (p->human_id != -1)
            sr_humans[p->human_id].look_d = 1;
    } else if (row == 2 && !p->look_e) {
        p->look_e = 1;
        if (p->human_id != -1)
            sr_humans[p->human_id].look_e = 1;
    }
    p->money -= price;
    createevent_updateplayer(player);
    if (sr_game.gametype == 3)
        createevent_updateplayer_finance(player);
}

void logic_gunstore_buy(int player, int row)
{
    Player *p = &sr_players[player];
    int *entry, price, item;
    vec3 rot[3];
    if (p->num_purchases >= 10)
        return;
    entry = shop_row(p->menu_building, row);
    price = entry[1];
    if (price > p->money)
        return;
    resetorientation(rot);
    item = createitem(entry[0], &sr_humans[p->human_id].pos, NULL, rot);
    put_item(item, p->human_id, 2, 6);
    p->money -= price;
    ++p->num_purchases;
    createevent_updateplayer(player);
}

void logic_cardealer_buy(int player, int row)
{
    Player *p = &sr_players[player];
    int building = p->menu_building;
    int *entry = shop_row(building, row);
    int *next = (int *)BUILDING_FIELD(car_spawn_next, building);
    const vec3 *spawn = (const vec3 *)BUILDING_FIELD(car_spawn_pos, building);
    int k, car, key, *count;
    vec3 pos, rot[3];
    if (p->num_cars >= 3 || entry[1] > p->money)
        return;
    k = *next;
    *next = (k + 1) & 7;
    pos.x = spawn->x;
    pos.y = spawn->y;
    pos.z = (float)((double)spawn->z - (double)k * 4.0);
    resetorientation(rot);
    rotateorientation(rot, &g_yaxis, 1.5707963705062866f);
    car = createobject(entry[0], &pos, NULL, rot, entry[2]);
    p->money -= entry[1];
    key = createitem(24, &pos, NULL, rot);
    if (key != -1) {
        sr_items[key].vehicle_id = car;
        put_item(key, p->human_id, 2, 6);
    }
    ++p->num_cars;
    createevent_updateplayer(player);
    if (sr_game.gametype == 3)
        createevent_updateplayer_finance(player);
    count = (int *)BUILDING_FIELD(shop_count, building);
    if (*count > 0) {
        int *last;
        --*count;
        last = (int *)BUILDING_FIELD(shop_rows, building) + *count * 3;
        entry[0] = last[0];
        entry[1] = last[1];
        entry[2] = last[2];
    }
}

int logic_createcar(int type, int team, int color)
{
    int i, count;
    TeamCarSpot *spots;
    if (team == -1)
        return -1;
    count = *(int *)TEAM_FIELD(team0_num_car_spots, team);
    spots = (TeamCarSpot *)TEAM_FIELD(team0_car_spots, team);
    for (i = 0; i < count; ++i)
        if (!spots[i].used) {
            spots[i].used = 1;
            return createobject(type, &spots[i].pos, NULL, spots[i].rot, color);
        }
    return -1;
}

static void close_team_phone(int item)
{
    Item *phone = &sr_items[item];
    int linked = phone->linked_item;
    if (linked != -1) {
        sr_items[linked].call_state = 0;
        sr_items[linked].linked_item = -1;
        sr_items[linked].info = 0;
        createevent_updateiteminfo(linked);
    }
    phone->time_to_live = 0;
}

void logic_setteammanager(int team, int player)
{
    int *manager = (int *)TEAM_FIELD(team0_manager, team);
    uint32_t first = ((uint32_t)team + 1) * 0x457u;
    uint32_t second = (uint32_t)team * 0x457u + 0x458u;
    int item, a, b;
    float angle;
    vec3 pos, rot[3];
    if (*manager != -1)
        return;
    for (item = 0; item < 1024; ++item)
        if (sr_items[item].active && sr_items[item].type == 23) {
            if ((uint32_t)sr_items[item].phone_id == first)
                close_team_phone(item);
            if ((uint32_t)sr_items[item].phone_id == second)
                close_team_phone(item);
        }
    safe_center(team, &pos);
    resetorientation(rot);
    a = createitem(23, &pos, NULL, rot);
    if (a != -1) {
        sr_items[a].phone_line = 0;
        sr_items[a].phone_id = (int)first;
        createevent_updateiteminfo(a);
    }
    /* The completed item scan selects the angle at team index 1024. */
    angle = (float)((double)*SR_AT(float, 0x953b5c8) - 1.570796326795);
    rotateorientation(rot, &rot[1], angle);
    pos.x = (float)((double)pos.x + (double)rot[0].x * 0.5);
    pos.y = (float)((double)pos.y + (double)rot[0].y * 0.5);
    pos.z = (float)((double)pos.z + (double)rot[0].z * 0.5);
    b = createitem(23, &pos, NULL, rot);
    if (b != -1) {
        sr_items[b].phone_line = 1;
        sr_items[b].phone_id = (int)second;
        createevent_updateiteminfo(b);
    }
    *manager = player;
    sprintf(g_manager_msg, "%s is now manager of %s", sr_players[player].name, g_teamname[team]);
    sr_createevent_message(0, g_manager_msg, -1, 0);
}

void logic_farbslist(int rank)
{
    Radix *radix = SR_AT(Radix, SR_RVA_RADIX);
    int i, account;
    if (rank < 0) {
        sr_createevent_message(0, "Top 10 list", -1, 0);
        return;
    }
    if (rank >= sr_numofaccounts)
        return;
    radix->count = sr_numofaccounts;
    for (i = 0; i < sr_numofaccounts; ++i)
        radix->keys[i] = (uint32_t)(-0x70000001 - sr_accounts[i].money);
    csradixsort();
    account = radix->order[rank];
    sprintf(g_farbs_line, "%d. %s   $", rank + 1, sr_accounts[account].name);
    sprintf_comma(g_farbs_digits, sr_accounts[account].money);
    strcat(g_farbs_line, g_farbs_digits);
    sr_createevent_message(0, g_farbs_line, -1, 0);
}

static void item_action(int human, PlayerAction *action)
{
    Human *h = &sr_humans[human];
    int item = action->arg_a, value = action->arg_b, hand = -1;
    Item *it = &sr_items[item];
    if (h->inventory[0].count > 0 && h->inventory[0].items[0] == item)
        hand = 0;
    else if (h->inventory[1].count > 0 && h->inventory[1].items[0] == item)
        hand = 1;
    if (hand == -1)
        return;
    if (it->type == 16) {
        int selected = it->num_bills;
        if (value == 0) {
            ++selected;
        } else if (value == 1) {
            --selected;
        } else if (value - 2 <= it->last_bill) {
            int other = hand ^ 1;
            int bill = (it->bills_mask >> ((selected * 3) & 31)) & 7;
            if (h->inventory[other].count == 0) {
                int made = createitem(16, &it->pos, &it->vel, it->rot);
                if (made != -1) {
                    sr_items[made].bills_mask = bill;
                    sr_items[made].last_bill = 0;
                    linkitem(made, UINT32_MAX, human, other);
                    item_removebill(item, selected);
                }
            } else {
                int held = h->inventory[other].items[0];
                if (sr_items[held].type == 16 && item_addbill(held, 0, bill))
                    item_removebill(item, selected);
            }
        }
        if (selected < 0)
            selected = 0;
        if (selected > it->last_bill)
            selected = it->last_bill;
        it->num_bills = selected;
    } else if (it->type == 23 && it->call_state == 1 && it->cooldown == 0 && value >= 0 && value < 10) {
        it->phone_number = it->phone_number * 10 + value;
        createevent_sound(g_soundlist.digit[value], &it->pos, 1, 1);
        it->info = it->phone_number;
        createevent_updateiteminfo(item);
    }
}

static void bank_action(int player, int value, int own)
{
    Player *p = &sr_players[player];
    Human *h = &sr_humans[p->human_id];
    int *balance = own ? &p->money : &g_market.stocks[p->team].stake;
    if ((uint32_t)(value - 1) < 6) {
        int bill = g_dollarbillvalue[value];
        int previous = *balance;
        int success = 0;
        if (bill <= previous && (value > 2 || p->small_bills < 40)) {
            if (h->inventory[0].count < 1) {
                vec3 rot[3];
                int item;
                resetorientation(rot);
                item = createitem(16, &h->pos, NULL, rot);
                if (item != -1) {
                    sr_items[item].last_bill = 0;
                    sr_items[item].bills_mask = value;
                    linkitem(item, UINT32_MAX, p->human_id, 0);
                    *balance -= g_dollarbillvalue[value];
                    success = 1;
                }
            } else {
                int item = h->inventory[0].items[0];
                if (sr_items[item].type == 16 && item_addbill(item, 0, value)) {
                    *balance = previous - bill;
                    success = 1;
                }
            }
            if (success && value < 3)
                ++p->small_bills;
        }
    }
    if (value == 7) {
        if (h->inventory[0].count > 0) {
            int item = h->inventory[0].items[0];
            if (sr_items[item].type == 16) {
                if (own)
                    p->money += item_billvalue(item);
                else
                    logic_addteamcash(p->team, item_billvalue(item));
                sr_items[item].time_to_live = 0;
            } else if (!own) {
                int j;
                for (j = 0; j < 16; ++j) {
                    Mission *m = &team_missions(p->team)[j];
                    if (m->active && m->item == item)
                        logic_teammissioncomplete(p->team, j);
                }
            }
        }
        return;
    }
    if (value == 16)
        p->menu = 3;
}

void logic_playeractions(int player)
{
    Player *p = &sr_players[player];
    /* Shop actions share the last reopen choice during this call. */
    int reopen = 0;
    while (p->action_tail != p->num_actions) {
        PlayerAction *a = &p->actions[p->action_tail];
        int human = p->human_id;
        if (a->kind == 2 && human != -1)
            item_action(human, a);
        if (a->kind == 1 && !server_playermessage(player, a->text)) {
            if (sr_game.state == 3 || human == -1 || sr_humans[human].health < 1) {
                sr_createevent_message(0, a->text, player, a->arg_b);
            } else {
                Human *h = &sr_humans[human];
                sr_createevent_message(1, a->text, human, a->arg_b);
                if (h->inventory[0].count > 0) {
                    int phone = h->inventory[0].items[0];
                    int linked = sr_items[phone].linked_item;
                    if (sr_items[phone].type == 23 && linked != -1 && sr_items[linked].linked_item == phone)
                        sr_createevent_message(2, a->text, linked, a->arg_b);
                }
            }
        }
        if (a->kind == 0 && p->menu == a->arg_a) {
            int id = a->arg_a, value = a->arg_b;
            if (human == -1) {
                if (id == 2 && sr_game.state == 1) {
                    if (value >= 1 && value <= 3) {
                        p->team = value - 1;
                    } else if (value == 4) {
                        playersellstock(player, p->stocks);
                        p->team = 7;
                        p->top_of_team = 0;
                        createevent_updateplayer_finance(player);
                    } else if (value == 5) {
                        p->ready = 1;
                    }
                    createevent_updateplayer(player);
                } else if (id == 1 && value == 1) {
                    int r;
                    vec3 pos, rot[3];
                    p->team = 7;
                    pos.x = (float)((double)g_game_world.spawn.x + 1.0);
                    pos.y = g_game_world.spawn.y;
                    pos.z = (float)((double)g_game_world.spawn.z + 1.0);
                    r = os_rand() & 255;
                    pos.x = (float)(((double)g_game_world.spawn_size_x - 2.0) * r * 0.5 * 0.00390625 + pos.x);
                    pos.z = (float)((double)g_game_world.spawn_size_z * 0.25 + pos.z);
                    r = os_rand() & 255;
                    pos.z = (float)(((double)g_game_world.spawn_size_z - 2.0) * r * 0.5 * 0.00390625 + pos.z);
                    resetorientation(rot);
                    rotateorientation(rot, &rot[1], -1.5707963705062866f);
                    human = createhuman(&pos.x, &rot[0].x, player);
                    p->human_id = human;
                    if (human != -1) {
                        sr_humans[human].account = p->account;
                        sr_humans[human].axis = -1.5707963705062866f;
                        createevent_updateplayer(player);
                        createevent_updateplayer_finance(player);
                        p->menu = 0;
                    }
                }
            }
            if (p->human_id != -1) {
                if (id == 9 && value == 1)
                    p->menu = 10;
                if (id == 10) {
                    int building = p->menu_building;
                    int count = *(int *)BUILDING_FIELD(shop_count, building);
                    if (value > 0 && value < count + 1) {
                        if (*(int *)BUILDING_FIELD(building_type, building) == 4) {
                            logic_suits_buy(player, value);
                            reopen = 1;
                        }
                        if (*(int *)BUILDING_FIELD(building_type, building) == 2) {
                            logic_cardealer_buy(player, value);
                            reopen = 1;
                        }
                        if (*(int *)BUILDING_FIELD(building_type, building) == 7) {
                            logic_gunstore_buy(player, value);
                            reopen = 0;
                        }
                    }
                    if (reopen == 1)
                        p->menu = 9;
                }
            }
            human = p->human_id;
            if (p->team != -1 && human != -1) {
                Human *h = &sr_humans[human];
                if (id == 3) {
                    static const int menus[5] = {4, 5, 7, 11, 8};
                    if (value >= 1 && value <= 5)
                        p->menu = menus[value - 1];
                } else if (id == 4 || id == 5) {
                    static const int types[2][4] = {{10, 6, 0, 2}, {11, 7, 1, 3}};
                    if (value >= 1 && value <= 4 && p->num_purchases < 10) {
                        int type = types[id - 4][value - 1];
                        int price = sr_itemtypes[type].price;
                        if (price <= p->money || price == 0) {
                            vec3 rot[3];
                            int item;
                            resetorientation(rot);
                            item = createitem(type, &h->pos, NULL, rot);
                            put_item(item, human, 2, 6);
                            p->money -= price;
                            if (sr_game.gametype == 3)
                                createevent_updateplayer_finance(player);
                            ++p->num_purchases;
                        }
                    }
                    if (value == 16)
                        p->menu = 3;
                } else if (id == 6) {
                    if (value >= 1 && value - 1 < g_market.car_base) {
                        int *row = g_market.car_rows + (value - 1) * 3;
                        if (row[2] > 0 && row[2] <= p->money) {
                            int car = row[0], color = row[1], price = row[2];
                            int inside = pointinsidebase(&h->pos.x, p->team, 0);
                            int object;
                            if (inside && car != -1 && (object = logic_createcar(car, p->team, color)) != -1) {
                                vec3 rot[3];
                                int key;
                                p->menu = 3;
                                resetorientation(rot);
                                key = createitem(24, &h->pos, NULL, rot);
                                if (key != -1) {
                                    sr_items[key].vehicle_id = object;
                                    put_item(key, human, 2, 5);
                                }
                                p->money -= price;
                                if (sr_game.gametype == 3)
                                    createevent_updateplayer_finance(player);
                                row[2] = 0;
                                createevent_updatecarmarket(value - 1);
                            }
                        }
                    }
                    if (value == 16)
                        p->menu = 3;
                } else if (id == 7) {
                    int type = value == 1 ? 12 : value == 2 ? 13 : -1;
                    if (pointinsidebase(&h->pos.x, p->team, 0) && type != -1 && p->num_purchases < 10 &&
                        p->money >= 0 && sr_itemtypes[type].price <= p->money) {
                        vec3 pos;
                        int item, price = sr_itemtypes[type].price;
                        scaleaddvectors(&pos, &h->bones[0].pos, &h->bones[0].rot[2], -0.75f);
                        scaleaddvectors(&pos, &pos, &h->bones[0].rot[1], 0.75f);
                        item = createitem(type, &pos, NULL, h->bones[0].rot);
                        put_item(item, human, type == 12 ? 2 : 0, type == 12 ? 6 : 1);
                        p->money -= type == 12 ? sr_itemtypes[12].price : price;
                        if (type == 12 && sr_game.gametype == 3)
                            createevent_updateplayer_finance(player);
                        ++p->num_purchases;
                    }
                } else if (id == 8) {
                    static const int amount[4] = {1, 10, 100, 5000};
                    if (value >= 1 && value <= 8) {
                        if (value & 1)
                            playerbuystock(player, amount[(value - 1) / 2]);
                        else
                            playersellstock(player, amount[(value - 1) / 2]);
                    }
                    createevent_updateplayer_finance(player);
                } else if (id == 11) {
                    if (p->team >= 0 && p->team < 5)
                        bank_action(player, value, 0);
                } else if (id == 12) {
                    bank_action(player, value, 1);
                }
            }
        }
        p->action_tail = (p->action_tail + 1) & 63;
    }
}

void logicsimulation(void)
{
    int i, k, t = sr_game.ticks & 0x7ff;
    int gametype, state;
    for (k = 0; k < g_city.num_intersections; ++k) {
        CityIntersection *isec = &g_city.intersections[k];
        if (t < 0x400) {
            isec->red[0] = isec->red[2] = 2;
            isec->red[1] = isec->red[3] = t < 0x380 ? 0 : 1;
        } else {
            isec->red[1] = isec->red[3] = 2;
            isec->red[0] = isec->red[2] = t < 0x780 ? 0 : 1;
        }
        if (g_train_near && k == 4)
            for (i = 0; i < 4; ++i)
                isec->red[i] = 2;
    }
    for (k = 0; k < g_city.num_intersections; ++k) {
        CityIntersection *isec = &g_city.intersections[k];
        if (isec->light < isec->light_max && ++isec->light_timer > 0x4af) {
            ++isec->light;
            isec->light_timer = 0;
        }
    }
    if (sr_game.gametype == 0 && sr_game.state == 1) {
        if (sr_game.timer < 1) {
            sr_game.state = 2;
            sr_game.timer = sr_game.round_time;
        } else {
            --sr_game.timer;
        }
    }
    gametype = sr_game.gametype;
    state = sr_game.state;
    for (i = 0; i < 256; ++i)
        if (sr_players[i].active) {
            if (sr_players[i].human_id == -1) {
                if (gametype == 3)
                    sr_players[i].menu = state == 1 ? 2 : 0;
            } else {
                logic_player(i);
            }
        }
    for (k = 0; k < 8; ++k)
        *(int *)TEAM_FIELD(team0_members, k) = 0;
    for (i = 0; i < 256; ++i) {
        Player *p = &sr_players[i];
        if (p->active) {
            if (p->team >= 0 && p->team < 5 && p->human_id != -1)
                ++*(int *)TEAM_FIELD(team0_members, p->team);
            if (p->chat_block > 0)
                --p->chat_block;
        }
    }
    for (i = 0; i < 256; ++i)
        if (sr_players[i].active)
            logic_playeractions(i);
    if (sr_serverinfo.reset_request) {
        resetgame();
        sr_serverinfo.reset_request = 0;
    }
    if (sr_game.gametype == 3)
        logicsimulation_round();
    else if (sr_game.gametype == 4)
        logicsimulation_world();
}

void logicsimulation_driving(void)
{
    int all = 1, i, c;
    for (i = 0; i < 256; ++i) {
        Player *p = &sr_players[i];
        if (p->active && p->human_id != -1 && p->checkpoints < sr_game.num_checkpoints) {
            uint32_t mask = p->checkpoint_mask;
            for (c = 0; c < sr_game.num_checkpoints; ++c) {
                uint32_t bit = 1u << (c & 31);
                if (!(mask & bit)) {
                    const vec3 *cp = &sr_game.checkpoints[c];
                    const vec3 *pos = &sr_humans[p->human_id].pos;
                    vec3 d;
                    float sum, dist;
                    d.x = (float)((double)cp->x - pos->x);
                    d.y = (float)((double)cp->y - pos->y);
                    d.z = (float)((double)cp->z - pos->z);
                    sum = (float)((double)d.z * d.z + ((double)d.x * d.x + (double)d.y * d.y));
                    dist = (float)x87_sqrt(sum);
                    if (dist < 4.0f) {
                        mask |= bit;
                        ++p->checkpoints;
                    } else {
                        all = 0;
                    }
                }
            }
            p->checkpoint_mask = mask;
        }
    }
    if (!all)
        ++sr_game.timer;
}
