/* The network events of the server.
 *
 * The events are a ring of 65536 records (sr_events). Each function fills the next record and
 * moves the counter (sr_numofevents). The server sends each client the events that the client
 * did not confirm yet (Connection.event_ack). Only the server makes events.
 *
 * The vectors are copied as raw bytes, so no float load changes their bits.
 */
#include "events.h"

#include <string.h>

#include "area.h"
#include "sr_runtime.h"

#define MAX_ITEMS 0x400
#define sr_market (*SR_AT(Market, SR_RVA_MARKET))
/* The first event that eventsimulation did not handle yet (it lives in BlockTemp). */
#define lastevent (*SR_AT(uint32_t, 0x53f254c))

/* The next free event, with its type and the tick of now. */
static Event *new_event(int type)
{
    Event *ev = &sr_events[sr_numofevents];
    ev->type = type;
    ev->tick = sr_game.ticks;
    return ev;
}

static void next_event(void)
{
    sr_numofevents = (sr_numofevents + 1) & 0xffff;
}

/* A vector, or zero when there is no vector. */
static void copy_vec(vec3 *dst, const vec3 *src)
{
    if (src)
        memcpy(dst, src, sizeof *dst);
    else
        memset(dst, 0, sizeof *dst);
}

/* Event 3: an object was made. The clients get its type and its color. */
void createevent_createobject(int object)
{
    const Object *o = &sr_objects[object];
    Event *ev;
    if (sr_server == 0)
        return;
    ev = new_event(3);
    ev->param = object;
    ev->attach_type = o->type;
    ev->attach_id = o->color;
    next_event();
}

/* Event 5: the place of an item (on a human, in an item, in a slot), its type and state. */
void createevent_updateitem(uint32_t item)
{
    const Game *game = SR_AT(Game, SR_RVA_GAME);
    uint32_t *count = SR_AT(uint32_t, SR_RVA_NUMOFEVENTS);
    Event *events = sr_events;
    const Item *it = &sr_items[item];
    Event *ev;
    uint32_t index;
    int param;
    if (sr_server == 0 || item >= MAX_ITEMS)
        return;
    param = (it->active + it->type * 2) * 0x400 + (int)item;
    index = *count;
    ev = &events[index];
    ev->type = 5;
    ev->tick = game->ticks;
    ev->param = param;
    ev->attach_type = it->parent_human;
    ev->attach_id = it->parent_item;
    ev->range_mode = it->parent_slot;
    *count = (index + 1) & 0xffff;
}

/* Event 6: the call state and the info of an item. */
void createevent_updateiteminfo(uint32_t item)
{
    const Item *it = &sr_items[item];
    Event *ev;
    if (sr_server == 0 || item >= MAX_ITEMS)
        return;
    ev = new_event(6);
    ev->param = (int)item;
    ev->attach_type = it->call_state;
    ev->attach_id = it->info;
    ev->range_mode = it->phone_line;
    next_event();
}

/* Event 7: a player: the human, the team and the flags, the money, the looks and the name. */
void createevent_updateplayer(int player)
{
    const Player *p = &sr_players[player];
    Event *ev;
    if (sr_server == 0)
        return;
    ev = new_event(7);
    ev->param = (p->is_bot + (p->active + 2 + p->team * 2) * 2) * 0x100 + player;
    ev->attach_type = p->human_id;
    ev->attach_id = p->money;
    ev->range_mode = p->look_a + (p->look_b + (p->look_c + (p->look_d + p->look_e * 2) * 4) * 8) * 4;
    memcpy(ev->message, p->name, 31);
    ev->message[31] = '\0';
    next_event();
}

/* Event 8: the money and the stocks of a player. */
void createevent_updateplayer_finance(int player)
{
    uintptr_t image_base = sr_base;
    const Player *p = (const Player *)(image_base + SR_RVA_PLAYER) + player;
    const Game *game = (const Game *)(image_base + SR_RVA_GAME);
    uint32_t *count = (uint32_t *)(image_base + SR_RVA_NUMOFEVENTS);
    Event *events = (Event *)(image_base + SR_RVA_EVENT);
    Event *ev;
    uint32_t index;
    if (*(const int *)(image_base + SR_RVA_SERVER) == 0)
        return;
    index = *count;
    ev = &events[index];
    ev->type = 8;
    ev->tick = game->ticks;
    ev->param = player;
    {
        int money = p->money, stocks = p->stocks;
        *count = (index + 1) & 0xffff;
        ev->attach_type = money;
        ev->attach_id = stocks;
    }
}

/* Event 0: a bullet leaves a weapon. */
void createevent_bullet(int bullettype, const vec3 *pos, const vec3 *vel, int item)
{
    Event *ev;
    if (sr_server == 0)
        return;
    ev = new_event(0);
    memcpy(&ev->pos, pos, sizeof ev->pos);
    memcpy(&ev->vec, vel, sizeof ev->vec);
    ev->param = bullettype;
    ev->attach_type = item;
    next_event();
}

/* Event 1: a bullet hits something. kind: 0, 1 or 2 (what the bullet hits). */
void createevent_bullethit(int param, int kind, const vec3 *pos, const vec3 *vec)
{
    Event *ev;
    if (sr_server == 0)
        return;
    ev = new_event(1);
    memcpy(&ev->pos, pos, sizeof ev->pos);
    copy_vec(&ev->vec, vec);
    ev->param = param;
    ev->attach_type = kind;
    next_event();
}

/* Event 4: the state of an object (a vehicle), with its position and velocity. */
void createevent_updateobject(int object, int state, int value, const vec3 *pos, const vec3 *vel)
{
    Event *ev;
    if (sr_server == 0)
        return;
    ev = new_event(4);
    ev->param = object;
    ev->attach_type = state;
    ev->attach_id = value;
    copy_vec(&ev->pos, pos);
    copy_vec(&ev->vec, vel);
    next_event();
}

/* Event 9: a sound at a position. */
void createevent_sound(int sound, const vec3 *pos, float sound_a, float sound_b)
{
    Event *ev;
    if (sr_server == 0)
        return;
    ev = new_event(9);
    ev->param = sound;
    memcpy(&ev->pos, pos, sizeof ev->pos);
    ev->sound_a = sound_a;
    ev->sound_b = sound_b;
    next_event();
}

/* Event 16: a face of a block breaks (area_breakblock). Each pair of numbers goes in one int:
 * x and the area, z and y. */
void createevent_updateblock_quad(int area, int x, int y, int z, int block, int face, const vec3 *pos,
                                  const vec3 *normal)
{
    Event *ev;
    if (sr_server == 0)
        return;
    ev = new_event(16);
    ev->param = x * 0x10000 + area;
    ev->attach_type = z * 0x10000 + y;
    ev->attach_id = block;
    ev->range_mode = face;
    copy_vec(&ev->pos, pos);
    copy_vec(&ev->vec, normal);
    next_event();
}

/* Event 17: a human of a player (the player takes it over at a join). */
void createevent_updatehuman(int human, int player)
{
    const Human *h = &sr_humans[human];
    Event *ev;
    if (sr_server == 0)
        return;
    ev = new_event(17);
    ev->param = human;
    ev->attach_type = h->active;
    ev->attach_id = player;
    next_event();
}

/* Event 13: the prices of the 5 stocks as integers: two in param, two in attach_type. */
void createevent_updatestocks(void)
{
    Stock *s = sr_market.stocks;
    Event *ev;
    int k;
    if (sr_server == 0)
        return;
    for (k = 0; k < 5; k++)
        s[k].price_int = (int)(long long)s[k].price;
    ev = new_event(13);
    ev->param = s[1].price_int * 0x10000 + s[0].price_int;
    ev->attach_type = s[3].price_int * 0x10000 + s[2].price_int;
    ev->attach_id = s[4].price_int;
    next_event();
}

/* Event 14: a row of the car market (3 numbers for each row). */
void createevent_updatecarmarket(int row)
{
    const Market *m = &sr_market;
    Event *ev;
    if (sr_server == 0)
        return;
    ev = new_event(14);
    ev->param = row * 0x10 + m->car_base;
    ev->attach_type = m->car_rows[row * 3];
    ev->attach_id = m->car_rows[row * 3 + 1];
    ev->range_mode = m->car_rows[row * 3 + 2];
    next_event();
}

/* Handle the events that the server made since the last tick. Event 10 opens or closes the 2
 * doors of a team: the door block gets the door object, or none. */
void eventsimulation(void)
{
    while (lastevent != sr_numofevents) {
        const Event *ev = &sr_events[lastevent];
        if (ev->type == 10) {
            const DoorBlock *door = (const DoorBlock *)((const uint8_t *)sr_game.team0_doors + ev->param * 0x117c);
            int open = ev->attach_type;
            int d;
            for (d = 0; d < 2; d++, door++) {
                uint32_t object = open == 0 ? ((uint32_t)door->object << 10 | 9) : 0;
                area_interior_setobject(0, (uint32_t)door->x, (uint32_t)door->y, (uint32_t)door->z, (int)object);
                area_marksector(0, door->x, door->y, door->z);
            }
        }
        lastevent = (lastevent + 1) & 0xffff;
    }
}
