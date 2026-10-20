/* itemlogic and itemweaponsimulation: what the items do each tick.
 *
 * itemlogic runs the life counter of the loose items, the guns, the keys, the phones, the
 * briefcases and the bandages. flags holds the use bits of the tick: the bit is set by the
 * human that holds the item (human_itemsimulation). itemlogic copies flags to flags_prev and
 * clears flags at the end, so (flags & 1) and not (flags_prev & 1) is the press of the use key.
 */
#include "items.h"

#include "bullets.h"
#include "events.h"
#include "random.h"
#include "sr_runtime.h"
#include "vectors.h"

#define MAX_ITEMS 0x400
#define MAX_HUMANS 0x200
#define NO_LIMIT 0xffff       /* time_to_live of an item that stays */
#define LOOSE_LIFE 18000      /* the longest life of a loose item, in ticks */
#define PHONE_RING_LIFE 0x2ff /* the ticks that a phone rings before the call ends */
#define PHONE_REDIAL 180      /* the wait after a number is dialed */
#define GAMETYPE_FFA 3

/* The phone states. */
enum {
    PHONE_IDLE = 0,
    PHONE_DIAL = 1,    /* the number is typed */
    PHONE_RINGING = 2, /* the call goes out */
    PHONE_TALK = 3
};

/* Game globals. */
#define g_object SR_AT(Object, SR_RVA_OBJECT)
#define g_soundlist (*SR_AT(SoundList, 0x916d700))

/* The press of the use key. */
static int use_pressed(const Item *it)
{
    return (it->flags & 1) && !(it->flags_prev & 1);
}

/* Moves one round from the magazine in the gun to the chamber. */
static void chamber(Item *it)
{
    if (it->ammo == 0 && it->num_children > 0) {
        int mag = it->children[0];

        if (sr_items[mag].ammo > 0) {
            it->ammo = 1;
            sr_items[mag].ammo--;
        }
    }
}

/* The shot: the bullet leaves the muzzle at the speed of the type plus a random spread, the gun
 * moves back and the human gets a random kick. */
static void gun_shot(Item *it, int item, const ItemType *type, Human *hu)
{
    vec3 v, rnd, muzzle;
    float rnd2[2];
    float speed = -type->bullet_speed;
    float bm = g_bullettypes[type->bullet_type].mass;
    float k;
    int owner = hu->player_id;

    v.x = (float)(speed * it->rot[2].x + it->vel.x);
    v.y = (float)(it->rot[2].y * speed + it->vel.y);
    v.z = (float)(speed * it->rot[2].z + it->vel.z);
    getrandomvector3d(&rnd.x, type->spread, 0.0f);
    v.x = (float)(v.x + rnd.x);
    v.y = (float)(v.y + rnd.y);
    v.z = (float)(rnd.z + v.z);

    /* The gun gets the opposite momentum of the bullet. */
    k = (float)((bm * type->bullet_speed) / (it->mass + bm));
    it->vel.x = (float)(k * it->rot[2].x + it->vel.x);
    it->vel.y = (float)(it->rot[2].y * k + it->vel.y);
    it->vel.z = (float)(k * it->rot[2].z + it->vel.z);
    /* The muzzle offset is 0. The sums stay, as they decide the sign of a zero. */
    muzzle.x = (float)(it->pos.x + it->rot[2].x * 0.0);
    muzzle.y = (float)(it->rot[2].y * 0.0 + it->pos.y);
    muzzle.z = (float)(it->rot[2].z * 0.0 + it->pos.z);
    createbullet(type->bullet_type, &muzzle, &v, owner);
    createevent_bullet(type->bullet_type, &it->pos, &v, item);

    getrandomvector2d(rnd2, type->recoil, 0.0f);
    rnd2[0] = (float)(type->recoil + rnd2[0]);
    hu->recoil[0] = (float)(hu->recoil[0] + rnd2[0] * 0.25);
    hu->recoil[1] = (float)(rnd2[1] * 0.25 + hu->recoil[1]);
}

/* A gun held by a human. The trigger needs 2 ticks (the Magnum 6), and the Magnum and the 9mm
 * fire once for each press. Outside game state 2 only the top player of a team can shoot. */
void itemweaponsimulation(int item)
{
    Item *it = &sr_items[item];
    int type = it->type;
    int human = it->parent_human;
    int need = 2;
    int use, player;

    if (it->cooldown > 0)
        it->cooldown--;
    if (human == -1)
        return;
    player = sr_humans[human].player_id;
    if (!(sr_game.state == 2 || (player != -1 && sr_players[player].team != -1 && sr_players[player].top_of_team != 0)))
        return;

    use = it->flags & 1;
    if (use == 0)
        it->trigger_ticks = 0;
    if (type == ITEM_9MM || type == ITEM_MAGNUM) {
        if (use != 1)
            it->cooldown = 0;
        if (type == ITEM_MAGNUM)
            need = 6;
    }
    if (use == 1 && (it->trigger_ticks < need || type == ITEM_9MM || type == ITEM_MAGNUM))
        it->trigger_ticks++;
    chamber(it);
    if (it->cooldown < 1 && it->trigger_ticks == need && it->ammo > 0) {
        gun_shot(it, item, &sr_itemtypes[type], &sr_humans[human]);
        it->ammo--;
        it->cooldown = sr_itemtypes[type].fire_delay;
        if (it->ammo == 0)
            chamber(it);
    }
}

/* The life of an item. A loose item (on the ground, not a phone and not cash) and an item
 * inside a loose gun lose one tick each tick, and stay for ever in game type 3. Any other item
 * with a life goes back to "no limit". */
static void item_life(Item *it)
{
    int loose = it->parent_human == -1 && it->parent_item == -1 && it->type != ITEM_CELL_PHONE &&
                it->type != ITEM_CASH && it->type != ITEM_CASH_2;
    int parent = it->parent_item;

    if ((parent != -1 && sr_itemtypes[sr_items[parent].type].is_gun != 0 && sr_items[parent].parent_human == -1) ||
        loose) {
        if (it->time_to_live > LOOSE_LIFE)
            it->time_to_live = LOOSE_LIFE;
        it->time_to_live--;
        if (it->num_children > 0)
            it->time_to_live = LOOSE_LIFE;
        if (sr_game.gametype == GAMETYPE_FFA)
            it->time_to_live = NO_LIMIT;
    } else if (it->time_to_live > 0) {
        it->time_to_live = NO_LIMIT;
    }
}

/* The key of a vehicle: key_away stays 1 unless a human that sits in the vehicle holds it. */
static void key_logic(const Item *it)
{
    uint32_t vehicle = (uint32_t)it->vehicle_id;
    int human;

    if (vehicle >= MAX_ITEMS)
        return;
    human = it->parent_human;
    g_object[vehicle].key_away = 1;
    if (human != -1 && sr_humans[human].vehicle_id == (int)vehicle)
        g_object[vehicle].key_away = 0;
}

/* A phone dialed a number of 4 digits: the first idle phone with this id gets the call. */
static void phone_dial(Item *it, int item)
{
    int number = it->phone_number;
    int j;

    it->phone_number = 0;
    it->cooldown = PHONE_REDIAL;
    for (j = 0; j < MAX_ITEMS; j++) {
        Item *other = &sr_items[j];

        if (other->active != 0 && j != item && other->type == ITEM_CELL_PHONE &&
            (other->call_state == PHONE_IDLE || other->call_state == PHONE_DIAL) && other->linked_item == -1 &&
            other->phone_id == number) {
            it->call_state = PHONE_RINGING;
            other->call_state = PHONE_RINGING;
            it->linked_item = j;
            it->info = 0;
            other->info = it->phone_id;
            createevent_updateiteminfo(item);
            createevent_updateiteminfo(j);
            return;
        }
    }
}

/* A phone that rings: the other phone rings too, for PHONE_RING_LIFE ticks. */
static void phone_ring(Item *it, int item)
{
    int other = it->linked_item;

    if (other == -1 || sr_items[other].linked_item != -1) {
        it->ring_ticks = 0;
        return;
    }
    if ((sr_game.ticks & 0x7f) == 0) {
        createevent_sound(g_soundlist.phone_ring, &sr_items[other].pos, 1.0f, 1.0f);
        createevent_sound(g_soundlist.phone_ring, &it->pos, 0.5f, 1.0f);
    }
    it->ring_ticks++;
    if (it->ring_ticks > PHONE_RING_LIFE) {
        it->call_state = PHONE_IDLE;
        sr_items[other].call_state = PHONE_IDLE;
        sr_items[it->linked_item].info = 0;
        createevent_updateiteminfo(it->linked_item);
        it->linked_item = -1;
        it->info = 0;
        createevent_updateiteminfo(item);
    }
}

/* The use key on a phone that rings: every phone that calls this one gets the answer. With a
 * call open, the same key ends it. */
static void phone_use(Item *it, int item)
{
    int other = it->linked_item;
    int j;

    if (other == -1) {
        for (j = 0; j < MAX_ITEMS; j++) {
            Item *caller = &sr_items[j];

            if (caller->active != 0 && j != item && caller->type == ITEM_CELL_PHONE && caller->linked_item == item) {
                it->call_state = PHONE_TALK;
                caller->call_state = PHONE_TALK;
                it->linked_item = j;
                it->info = caller->phone_id;
                createevent_updateiteminfo(item);
                caller->info = it->phone_id;
                createevent_updateiteminfo(j);
            }
        }
    } else {
        it->call_state = PHONE_IDLE;
        sr_items[other].call_state = PHONE_IDLE;
        sr_items[it->linked_item].linked_item = -1;
        sr_items[it->linked_item].info = 0;
        createevent_updateiteminfo(it->linked_item);
        it->linked_item = -1;
        it->info = 0;
        createevent_updateiteminfo(item);
    }
}

/* A phone: the state machine of the number, the call and the ring. */
static void phone_logic(Item *it, int item)
{
    if (it->cooldown > 0)
        it->cooldown--;
    if (it->call_state == PHONE_IDLE) {
        it->ring_ticks = 0;
        if (use_pressed(it)) {
            it->call_state = PHONE_DIAL;
            it->phone_number = 0;
            it->info = 0;
            createevent_updateiteminfo(item);
        }
    } else if (it->call_state == PHONE_DIAL) {
        it->ring_ticks = 0;
        if (use_pressed(it)) {
            it->call_state = PHONE_IDLE;
            it->phone_number = 0;
            it->info = 0;
            createevent_updateiteminfo(item);
        }
        if (it->phone_number > 999)
            phone_dial(it, item);
    } else {
        if (it->call_state == PHONE_TALK && (it->linked_item == -1 || sr_items[it->linked_item].active == 0)) {
            it->call_state = PHONE_IDLE;
            it->linked_item = -1;
            it->info = 0;
            createevent_updateiteminfo(item);
        }
        if (it->call_state == PHONE_RINGING)
            phone_ring(it, item);
        if (use_pressed(it))
            phone_use(it, item);
    }
}

/* A bandage that the human lets go (the use key is released) heals the nearest human that
 * bleeds or has less than 10 health, within 2 units. */
static void bandage_logic(Item *it)
{
    float best = 2.0f;
    int target = -1;
    int h;

    for (h = 0; h < MAX_HUMANS; h++) {
        const Human *hu = &sr_humans[h];

        if (hu->active != 0 && hu->health > -5 && (hu->bleeding != 0 || hu->health < 10)) {
            float d = vectordistance(&it->pos, &hu->pos);

            if (d < best) {
                target = h;
                best = d;
            }
        }
    }
    if (target != -1) {
        sr_humans[target].bleeding = 0;
        if (sr_humans[target].health < 10)
            sr_humans[target].health = 10;
        it->ammo--;
        if (it->ammo < 1)
            it->time_to_live = 0;
    }
}

void itemlogic(void)
{
    int i;

    for (i = 0; i < MAX_ITEMS; i++) {
        Item *it = &sr_items[i];

        if (it->active == 0)
            continue;
        item_life(it);
        if (sr_itemtypes[it->type].is_gun != 0)
            itemweaponsimulation(i);
        if (it->type == ITEM_KEY)
            key_logic(it);
        if (it->type == ITEM_CELL_PHONE)
            phone_logic(it, i);
        if (use_pressed(it)) {
            if (it->type == ITEM_BRIEFCASE) {
                it->type = ITEM_OPEN_BRIEFCASE;
                createevent_updateitem(i);
            } else if (it->type == ITEM_OPEN_BRIEFCASE) {
                it->type = ITEM_BRIEFCASE;
                createevent_updateitem(i);
            }
        }
        if (!(it->flags & 1) && (it->flags_prev & 1) && it->type == ITEM_BANDAGE)
            bandage_logic(it);
        it->flags_prev = it->flags;
        it->flags = 0;
    }
}
