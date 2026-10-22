/* Player state, input and saved equipment. */
#include "player.h"

#include <string.h>

#include "economy.h"
#include "events.h"
#include "items.h"
#include "logic.h"
#include "server.h"
#include "sr_os.h"

/* Game functions of other files. */
int __cdecl lf_4e060(int type, int team, int color);
#define logic_createcar lf_4e060

int createplayer(void)
{
    int i, j;
    Player *p;
    for (i = 0; i < 256; ++i)
        if (!sr_players[i].active)
            break;
    if (i == 256)
        return -1;
    p = &sr_players[i];
    memset(p, 0, sizeof(*p));
    p->active = 1;
    p->team = 7;
    p->money = 50000;
    p->account = -1;
    p->human_id = -1;
    for (j = 0; j < 8; ++j)
        p->saved_items[j].count = 0;
    p->menu = sr_game.state == 1 && sr_game.gametype == 3 ? 2 : 0;
    p->body_param = 1.0f;
    os_rand();
    p->colors[0].x = 0.046875f;
    p->colors[0].y = 0.046875f;
    p->colors[0].z = -0.15000000596046448f;
    p->colors[2].x = -0.046875f;
    p->colors[2].y = 0.046875f;
    p->colors[2].z = -0.15000000596046448f;
    p->colors[1].x = p->colors[1].y = p->colors[1].z = 1.0f;
    p->colors[3].x = p->colors[3].y = p->colors[3].z = 1.0f;
    p->look_a = os_rand() % 3;
    p->look_b = os_rand() & 7;
    p->look_c = os_rand() & 3;
    p->wait_isec = -1;
    return i;
}

void deleteplayer(int player)
{
    Player *p = &sr_players[player];
    int team;
    playersellstock(player, p->stocks);
    if ((uint32_t)player < 256)
        playersellstock(player, p->stocks);
    if ((sr_game.gametype == 3 || sr_game.gametype == 4) && p->account != -1)
        sr_accounts[p->account].money = p->money;
    for (team = 0; team < 5; ++team) {
        int *manager = (int *)TEAM_FIELD(team0_manager, team);
        if (*manager == player)
            *manager = -1;
    }
    if (p->human_id != -1 && sr_humans[p->human_id].player_id == player) {
        sr_humans[p->human_id].account = p->account;
        sr_humans[p->human_id].player_id = -1;
    }
    p->active = 0;
    createevent_updateplayer(player);
}

void playersimulation(void)
{
    int i, j, k;
    for (i = 0; i < 256; ++i) {
        Player *p = &sr_players[i];
        if (!p->active)
            continue;
        if (p->human_id != -1) {
            Human *h = &sr_humans[p->human_id];
            if ((h->vehicle_id == -1 && p->input_mode == 1) ||
                (h->vehicle_id != -1 && (p->input_mode == 2 || p->input_mode == 3))) {
                for (j = 0; j < 4; ++j)
                    h->input_axes[j] = p->input_axes[j];
                h->view_axis = p->yaw;
                h->view_pitch = p->input_pitch;
                h->arm_axis = p->input_extra[0];
                h->arm_pitch = p->input_extra[1];
            } else if (h->vehicle_id == -1) {
                h->view_axis = h->axis;
                h->view_pitch = h->pitch;
            }
            h->input_buttons = p->input_buttons;
            h->move_mode = p->move_mode;
            p->hud_health = h->health < 0 ? 0 : h->health;
            for (j = 0; j < 7; ++j) {
                InventorySlot *slot = &h->inventory[j];
                p->hud_inventory[j].count = slot->count;
                for (k = 0; k < slot->count; ++k) {
                    Item *item = &sr_items[slot->items[k]];
                    ItemType *type = &sr_itemtypes[item->type];
                    int value = item->type;
                    if (j < 2 && type->is_gun && item->num_children > 0) {
                        value |= 0x100;
                        if (sr_items[item->children[0]].ammo == 0)
                            value = item->type | 0x300;
                    }
                    if (j < 2 && type->ammo > 0)
                        value |= item->ammo << 8;
                    p->hud_inventory[j].items[k] = value;
                }
            }
        }
        if (p->account != -1) {
            if (sr_game.gametype == 4) {
                sr_accounts[p->account].money = p->team < 0 || p->team > 4 ? p->money :
                    (int)((double)g_market.stocks[p->team].price *
                          0.949999988079071 * p->stocks + p->money);
            } else if (sr_game.gametype == 3) {
                sr_accounts[p->account].money = p->team == -1 ? p->money :
                    (int)((double)g_market.stocks[p->team].price *
                          0.9900000095367432 * p->stocks + p->money);
            }
        }
        p->input_buttons_prev = p->input_buttons;
    }
}

void player_saveinventory(int player)
{
    Player *p = &sr_players[player];
    int j, k;
    for (j = 0; j < 7; ++j)
        p->saved_items[j].count = 0;
    if (p->human_id == -1)
        return;
    for (j = 0; j < 7; ++j) {
        InventorySlot *slot = &sr_humans[p->human_id].inventory[j];
        SavedSlot *saved = &p->saved_items[j];
        for (k = 0; k < slot->count; ++k) {
            Item *item = &sr_items[slot->items[k]];
            SavedItem *entry = &saved->items[saved->count];
            if (item->type < 13) {
                entry->type = item->type;
                entry->a = entry->b = 0;
                if (item->num_children < 1) {
                    entry->b = item->ammo;
                } else {
                    entry->a = 1;
                    entry->b = sr_items[item->children[0]].ammo;
                    if (item->ammo > 0)
                        ++entry->b;
                }
                ++saved->count;
            } else if (item->type == 24 && item->vehicle_id != -1 &&
                       sr_objects[item->vehicle_id].health > 0) {
                entry->type = 24;
                entry->a = sr_objects[item->vehicle_id].type;
                entry->b = sr_objects[item->vehicle_id].color;
                ++saved->count;
            }
        }
    }
}

void player_loadinventory(int player)
{
    Player *p = &sr_players[player];
    uint32_t team = (uint32_t)p->team;
    vec3 pos, rot[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    int j, k;
    if (p->human_id == -1)
        return;
    pos = sr_humans[p->human_id].bones[0].pos;
    for (j = 0; j < 7; ++j) {
        SavedSlot *saved = &p->saved_items[j];
        for (k = 0; k < saved->count; ++k) {
            SavedItem *entry = &saved->items[k];
            if (entry->type < 13) {
                int item = createitem(entry->type, &pos, NULL, rot);
                if (!sr_itemtypes[entry->type].is_gun) {
                    sr_items[item].ammo = entry->b;
                } else if (entry->a > 0) {
                    int mag = createitem(entry->type + 1, &pos, NULL, rot);
                    sr_items[mag].ammo = entry->b;
                    linkitem(item, mag, -1, 0);
                }
                linkitem(item, UINT32_MAX, p->human_id, j);
            } else if (entry->type == 24 && team < 3) {
                int car = logic_createcar(entry->a, team, entry->b);
                if (car != -1) {
                    int key = createitem(24, &pos, NULL, rot);
                    sr_items[key].vehicle_id = car;
                    linkitem(key, UINT32_MAX, p->human_id, j);
                }
            }
        }
    }
}
