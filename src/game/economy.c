/* Stock prices, shops, player taxes and mission rewards. */
#include "economy.h"

#include "events.h"
#include "logic.h"
#include "sr_os.h"
#include "sr_server.h"

static void split_stock(int team, int shares)
{
    int i;
    Stock *stock = &g_market.stocks[team];

    stock->shares = shares * 5;
    stock->price = (float)((double)stock->price * 0.20000000298023224);
    for (i = 0; i < 256; ++i) {
        Player *player = &sr_players[i];
        if (player->active && player->team == team) {
            player->stocks *= 5;
            createevent_updateplayer_finance(i);
        }
    }
}

void economysimulation(void)
{
    int team, i, slot, j;

    for (team = 0; team < 3; ++team) {
        Stock *stock = &g_market.stocks[team];
        int cash = *(int *)TEAM_FIELD(team0_cash, team);
        float growth = (float)((double)cash / 100.0 - 0.5);
        for (i = 0; i < 256; ++i) {
            Player *player = &sr_players[i];
            if (player->active && player->ready > 0 && player->team == team) {
                int half = cash / 2;
                player->money += 150;
                if (growth >= 1.0 && player->top_of_team)
                    half = (int)((double)growth / 1000.0 *
                                 ((double)player->stocks * stock->price) + half);
                player->money += half;
            }
        }
        stock->price = (float)(((double)growth / 200.0 + 1.0) * stock->price);
        if (stock->price < 10.0f)
            stock->price = 10.0f;
        if (stock->price >= 500.0) {
            stock->price = (float)((double)stock->price * 0.20000000298023224);
            for (i = 0; i < 256; ++i)
                if (sr_players[i].active && sr_players[i].team == team)
                    sr_players[i].stocks *= 5;
        }
    }
    for (i = 0; i < 256; ++i)
        if (sr_players[i].active)
            sr_players[i].top_of_team = 0;
    for (team = 0; team < 3; ++team) {
        int best = 1000;
        int who = -1;
        for (i = 0; i < 256; ++i) {
            Player *player = &sr_players[i];
            if (player->active && player->ready > 0 && player->team == team) {
                double value = (double)g_market.stocks[player->team].price *
                               player->stocks + player->money;
                if ((double)best < value) {
                    best = (int)value;
                    who = i;
                }
            }
        }
        if (who != -1)
            sr_players[who].top_of_team = 1;
    }
    for (i = 0; i < 256; ++i) {
        Player *player = &sr_players[i];
        if (!player->active || player->human_id == -1)
            continue;
        for (slot = 0; slot < 7; ++slot) {
            InventorySlot *inventory = &sr_humans[player->human_id].inventory[slot];
            SavedSlot *saved = &player->saved_items[slot];
            saved->count = 0;
            for (j = 0; j < inventory->count; ++j) {
                Item *item = &sr_items[inventory->items[j]];
                if (item->type < 13) {
                    /* The other two saved values keep their previous contents. */
                    saved->items[saved->count++].type = item->type;
                } else if (item->type == 24 && item->vehicle_id != -1 &&
                           sr_objects[item->vehicle_id].health > 0) {
                    Object *object = &sr_objects[item->vehicle_id];
                    SavedItem *key = &saved->items[saved->count++];
                    key->type = 24;
                    key->a = object->type;
                    key->b = object->color;
                }
            }
        }
    }
}

void economy_carmarket(void)
{
    int b;
    for (b = 0; b < g_city.num_buildings; ++b) {
        int *kind = (int *)((uint8_t *)&g_city.building_type + b * 0x9dc);
        int *count = (int *)((uint8_t *)&g_city.shop_count + b * 0x9dc);
        int (*rows)[3] = (int (*)[3])((uint8_t *)g_city.shop_rows + b * 0x9dc);
        int i = 0;
        int has_turbo = 0;
        if (*kind != 2)
            continue;
        while (i < *count) {
            if ((os_rand() & 7) == 0) {
                --*count;
                rows[i][0] = rows[*count][0];
                rows[i][1] = rows[*count][1];
                rows[i][2] = rows[*count][2];
            } else {
                if (rows[i][0] == 11)
                    has_turbo = 1;
                ++i;
            }
        }
        while (*count < 8) {
            int r = os_rand() & 15;
            int type;
            if (r < 2)
                type = 7;
            else if (r < 10)
                type = 0;
            else if (r < 13)
                type = 8;
            else if (r < 15) {
                type = 6;
                if ((os_rand() & 1) == 0 && !has_turbo)
                    type = 11;
            } else {
                type = 4;
                if ((os_rand() & 15) == 0)
                    type = 5;
            }
            rows[*count][0] = type;
            rows[*count][1] = sr_data_objecttype[type].price;
            rows[*count][2] = os_rand() % 6;
            /* New turbo rows do not change the flag during this refill. */
            ++*count;
        }
    }
}

void economy_gunmarket(void)
{
    static const int kinds[] = {6, 7, 0, 1, 2, 3, 8, 9, 4, 5};
    int b, i;
    for (b = 0; b < g_city.num_buildings; ++b) {
        int *kind = (int *)((uint8_t *)&g_city.building_type + b * 0x9dc);
        int *count = (int *)((uint8_t *)&g_city.shop_count + b * 0x9dc);
        int (*rows)[3] = (int (*)[3])((uint8_t *)g_city.shop_rows + b * 0x9dc);
        if (*kind != 7)
            continue;
        *count = 0;
        for (i = 0; i < 10; ++i) {
            rows[*count][0] = kinds[i];
            rows[*count][1] = sr_itemtypes[kinds[i]].price;
            rows[*count][2] = 0;
            ++*count;
        }
    }
}

void playerbuystock(int i, int n)
{
    Player *player = &sr_players[i];
    Stock *stock;
    int shares;
    double cost;
    if (player->team == -1)
        return;
    stock = &g_market.stocks[player->team];
    shares = stock->shares;
    if (shares < 100000) {
        if (shares + n > 99999)
            n = 100000 - shares;
    } else {
        n = 0;
    }
    cost = (double)stock->price * n;
    if ((double)player->money > cost) {
        player->stocks += n;
        stock->shares = shares + n;
        player->money = (int)((double)player->money - cost);
    }
}

void playersellstock(int i, int n)
{
    Player *player = &sr_players[i];
    Stock *stock;
    float price, fee;
    int j;
    if (player->team == -1 || n > player->stocks)
        return;
    stock = &g_market.stocks[player->team];
    price = stock->price;
    for (j = 0; j < 16; ++j) {
        Mission *mission = &team_missions(player->team)[j];
        if (mission->active && mission->stake > 0) {
            int difference = mission->stake - stock->stake;
            if (difference < 0)
                difference = 0;
            if (stock->shares > 0)
                price = (float)((double)price - (double)difference / stock->shares);
        }
    }
    fee = sr_game.gametype == 3 ? 0.98f : 0.95f;
    player->money = (int)((double)fee * price * n + player->money);
    stock->shares -= n;
    player->stocks -= n;
}

void player_calcassets(int i)
{
    Player *player = &sr_players[i];
    int slot, j;
    if (player->human_id == -1)
        return;
    playersellstock(i, player->stocks);
    for (slot = 0; slot < 7; ++slot) {
        InventorySlot *inventory = &sr_humans[player->human_id].inventory[slot];
        for (j = 0; j < inventory->count; ++j) {
            Item *item = &sr_items[inventory->items[j]];
            if (item->type == 24 && (uint32_t)item->vehicle_id < 1024) {
                Object *object = &sr_objects[item->vehicle_id];
                if (object->active && object->health > 0)
                    player->money += sr_data_objecttype[object->type].price;
            }
        }
    }
}

void playerdeathtax(uint32_t i)
{
    Player *player;
    int over;
    if (i >= 256)
        return;
    player = &sr_players[i];
    over = player->money - 10000;
    if (over > 0)
        player->money -= (int)((double)over * 0.05000000074505806);
    if (player->team != -1 && player->team < 5) {
        Stock *stock = &g_market.stocks[player->team];
        int n = (int)(((double)player->stocks * stock->price - 0.0) / stock->price);
        if (n > 0) {
            n /= 10;
            stock->shares -= n;
            player->stocks -= n;
        }
    }
    createevent_updateplayer_finance((int)i);
}

void playerteamkilltax(int i, int damage)
{
    Player *player;
    float t, fraction;
    int take;
    if ((uint32_t)i >= 256 || damage <= 0)
        return;
    player = &sr_players[i];
    sr_game.teamkill_rate = 0.05f;
    if (sr_game.gametype == 4 && player->human_id != -1) {
        Human *human = &sr_humans[player->human_id];
        human->health -= damage;
        human->bleeding = 1;
    }
    t = (float)damage;
    fraction = (float)((double)t * 0.05000000074505806 / 100.0);
    player->money = player->money - damage * 100 - 1;
    if (fraction < 0.0f)
        fraction = 0.0f;
    else if (fraction > 1.0f)
        fraction = 1.0f;
    take = (int)((double)player->money * fraction);
    if (take < 0)
        take = 0;
    player->money -= take;
    if (player->team != -1) {
        Stock *stock = &g_market.stocks[player->team];
        float x = (float)((double)t / 100.0);
        int n;
        fraction = (float)((double)x * 0.05000000074505806);
        if (fraction < 0.0f)
            fraction = 0.0f;
        else if (fraction > 1.0f)
            fraction = 1.0f;
        n = (int)((double)player->stocks * fraction);
        if (n == 0 && player->stocks > 0)
            n = 1;
        stock->shares -= n;
        player->stocks -= n;
    }
    createevent_updateplayer(i);
    createevent_updateplayer_finance(i);
}

void logic_addteamcash(int team, int amount)
{
    Stock *stock = &g_market.stocks[team];
    int shares = stock->shares;
    int members = *(int *)TEAM_FIELD(team0_members, team);
    int per = amount;
    int i;
    if (shares > 0)
        stock->price = (float)((double)amount / shares + stock->price);
    if (members > 0)
        per = amount / members;
    for (i = 0; i < 256; ++i) {
        Player *player = &sr_players[i];
        if (player->active && player->team == team) {
            float fraction = (float)(1.0 - (double)player->stocks / shares);
            if (fraction < 0.0f)
                fraction = 0.0f;
            else if (fraction > 1.0f)
                fraction = 1.0f;
            player->money += (int)((double)per * fraction);
            createevent_updateplayer_finance(i);
        }
    }
    if (stock->price > 500.0)
        split_stock(team, shares);
    createevent_updatestocks();
}

void logic_teammissioncomplete(int team, int m)
{
    int id = team_missions(team)[m].scenario;
    int t, j, i;
    for (t = 0; t < 5; ++t) {
        Stock *stock = &g_market.stocks[t];
        for (j = 0; j < 16; ++j) {
            Mission *mission = &team_missions(t)[j];
            if (!mission->active || mission->scenario != id)
                continue;
            if ((uint32_t)mission->item < 1024)
                sr_items[mission->item].time_to_live = 0;
            if (t == team) {
                stock->price = (float)((double)mission->own_growth * stock->price + stock->price);
                for (i = 0; i < 256; ++i) {
                    Player *player = &sr_players[i];
                    if (player->active && player->team == t) {
                        player->money += mission->cash;
                        if (sr_game.gametype == 3)
                            createevent_updateplayer_finance(i);
                    }
                }
            } else if (team_missions(team)[j].cash > 0) {
                /* The cash test uses the finishing team's mission at the same index. */
                stock->price = (float)((double)mission->other_growth * stock->price + stock->price);
            }
            if (mission->stake > 0) {
                if (t == team && mission->stake_min < mission->stake)
                    mission->stake = mission->stake_min;
                mission->stake -= stock->stake;
                stock->stake = 0;
                if (mission->stake < 0)
                    mission->stake = 0;
                if (stock->shares > 0)
                    stock->price = (float)((double)stock->price -
                                           (double)mission->stake / stock->shares);
                mission->stake = 0;
            }
            mission->active = 0;
            sr_createevent_updatemission(t, j);
        }
    }
    for (t = 0; t < 5; ++t) {
        Stock *stock = &g_market.stocks[t];
        if (stock->price < 10.0f)
            stock->price = 10.0f;
        if (stock->price > 500.0)
            split_stock(t, stock->shares);
    }
    createevent_updatestocks();
}

void scenario_setmissioninactive(int id)
{
    int t, j;
    for (t = 0; t < 5; ++t) {
        Stock *stock = &g_market.stocks[t];
        for (j = 0; j < 16; ++j) {
            Mission *mission = &team_missions(t)[j];
            if (!mission->active || mission->scenario != id)
                continue;
            if ((uint32_t)mission->item < 1024)
                sr_items[mission->item].time_to_live = 0;
            if (mission->stake > 0) {
                int difference = stock->stake;
                stock->stake = 0;
                mission->stake -= difference;
                if (stock->shares > 0)
                    stock->price = (float)((double)stock->price -
                                           (double)mission->stake / stock->shares);
                mission->stake = 0;
            }
            mission->active = 0;
            sr_createevent_updatemission(t, j);
        }
    }
}

int scenario_getnumofmissions(int team)
{
    int j;
    int count = 0;
    for (j = 0; j < 16; ++j) {
        Mission *mission = &team_missions(team)[j];
        if (mission->active) {
            ++count;
            if (mission->kind > 1)
                ++count;
        }
    }
    return count;
}
