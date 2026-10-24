/* human_itemsimulation: what a human does with the items in the hands and the pockets.
 *
 * A human has 8 inventory slots. Slot 0 is the left hand, slot 1 the right hand, slots 2 to 6
 * the pockets. Button 0x10 chooses the hand that the keys use (S, the other hand is O). The
 * buttons: 1 use, 0x20 drop, 0x40 to 0x400 the pocket keys 1 to 5, 0x1000 load or unload a
 * magazine, 0x2000 swap the hands. An edge is a bit that is set now and was not set in the last
 * tick.
 *
 * With an empty hand, the use key reaches 2 units to the front (a city object or an elevator
 * or a base door), the pocket keys take an item out of a pocket, and the release of the use key
 * takes the nearest loose item on the line of sight. Every item in the inventory goes to the
 * hand or pocket position at the end.
 */
#include "humans.h"

#include "area.h"
#include "buildings.h"
#include "collide.h"
#include "events.h"
#include "intersect.h"
#include "items.h"
#include "sr_runtime.h"
#include "sr_server.h"
#include "vectors.h"

#define MAX_ITEMS 0x400
#define NUM_SLOTS 7 /* the slots that hold items on the body: two hands and five pockets */
#define NUM_POCKETS 5
#define FIRST_POCKET 2
#define RELOAD_TICKS 60

/* The buttons. */
#define BTN_USE 0x01
#define BTN_DROP 0x20
#define BTN_POCKET 0x40 /* the first pocket key: 0x40 << k for pocket k */
#define BTN_LOAD 0x1000
#define BTN_SWAP 0x2000
#define BTN_HAND 0x10

/* Game globals. */
#define g_yaxis (*SR_AT(vec3, 0x60ce0)) /* (0 1 0) */
#define g_intersect (*SR_AT(Intersect, SR_RVA_INTERSECT))
#define g_blocklist (*SR_AT(BlockList, 0xb7a8ca0))
#define g_soundlist (*SR_AT(SoundList, 0x916d700))
#define g_cityobjects SR_AT(CityObjectRec, 0x15e24020)
/* The y of the building b of the city (City + 0x2f9f0, 0x9dc bytes for each building). */
#define g_building_y(b) (*SR_AT(float, 0xe2c8c10u + (uint32_t)(b) * 0x9dcu))

/* The state of the buttons for one human. */
typedef struct Keys {
    uint32_t now, before;
} Keys;

static int key_edge(const Keys *k, uint32_t mask)
{
    return (k->now & mask) == mask && (k->before & mask) != mask;
}

/* Takes an item out of the place it is in (a hand, a pocket or another item). */
static void drop_item(uint32_t item)
{
    Item *it = &sr_items[item];

    if (it->parent_item == -1) {
        if (it->parent_human != -1) {
            item_removehumanlink(item, it->parent_human, it->parent_slot);
            createevent_updateitem(item);
        }
    } else {
        item_removeitemlink(item, it->parent_item);
        createevent_updateitem(item);
    }
}

/* The use key with an empty hand: a line of 2 units from the head. */
static void reach(Human *hu, const Keys *k)
{
    const Bone *head = &hu->bones[2];
    vec3 to;
    uint32_t link;
    int player, team, e;

    to.x = (float)(head->pos.x - head->rot[2].x * 2.0);
    to.y = (float)(head->pos.y - head->rot[2].y * 2.0);
    to.z = (float)(head->pos.z - head->rot[2].z * 2.0);
    if (!key_edge(k, BTN_USE) || !lineintersectall((float *)&head->pos, &to.x))
        return;
    if (g_intersect.block_area == -1) {
        /* A city object with a grid of floors (an elevator panel): the hit cell picks the floor. */
        if (g_intersect.index != -1 && g_intersect.face_u != -1) {
            int floor = g_intersect.face_u + (3 - g_intersect.face_v) * 8;

            if (floor < g_cityobjects[g_intersect.index].num_floors)
                g_cityobjects[g_intersect.index].floor = floor;
        }
        return;
    }
    link = area_interior_getblocklink(g_intersect.block_area, g_intersect.block[0], g_intersect.block[1],
                                      g_intersect.block[2]);
    if (link == ((uint32_t)g_blocklist.elevator_block | 0x80000000u) &&
        (e = city_createelevator(&head->pos)) != -1) {
        /* The game reads the building of the record 0 here, not of the record e. */
        g_cityobjects[e].floor = (int)((head->pos.y - g_building_y(g_cityobjects[0].building)) * 0.25);
    }
    player = hu->player_id == -1 ? 0 : hu->player_id;
    team = sr_players[player].team;
    if (sr_game.state == 2 && team >= 0 && team < 5 && pointinsidebase(&hu->pos.x, team, 4.0f)) {
        if (((uint32_t)g_blocklist.base_door_first | 0x80000000u) <= link &&
            link <= ((uint32_t)g_blocklist.base_door_last | 0x80000000u))
            sr_createevent_updatedoor(team, 0);
        if (g_intersect.block_word == 0 && g_intersect.index == 9)
            sr_createevent_updatedoor(team, 1);
    }
}

/* The hand of a human holds a door at a point. */
static void grip_loose_door(Human *hu, int hand, const Item *door)
{
    HandGrip *g = &hu->hand_grips[hand];

    g->pos.x = (float)(door->pos.x + door->rot[0].x * 0.5);
    g->pos.y = (float)(door->rot[0].y * 0.5 + door->pos.y);
    g->pos.z = (float)(door->rot[0].z * 0.5 + door->pos.z);
    g->pos.x = (float)(g->pos.x + door->rot[1].x * 0.25);
    g->pos.y = (float)(door->rot[1].y * 0.25 + g->pos.y);
    g->pos.z = (float)(door->rot[1].z * 0.25 + g->pos.z);
    g->axis = hu->input_axes[0];
}

/* The release of the use key with an empty hand: takes the nearest loose item within 0.5 units
 * of the line of sight (1.5 units long). */
static void pick_up(Human *hu, int human, int hand)
{
    const Bone *head = &hu->bones[2];
    vec3 to;
    float best = 0.5f;
    int found = -1;
    int i;

    to.x = (float)(head->pos.x - head->rot[2].x * 1.5);
    to.y = (float)(head->pos.y - head->rot[2].y * 1.5);
    to.z = (float)(head->pos.z - head->rot[2].z * 1.5);
    for (i = 0; i < MAX_ITEMS; i++) {
        const Item *it = &sr_items[i];

        if (it->active != 0 && it->parent_human == -1 && it->parent_item == -1 &&
            pointdistanceline(&head->pos, &to, &it->pos) < best) {
            found = i;
            best = pointdistanceline(&head->pos, &to, &it->pos);
        }
    }
    if (found == -1)
        return;
    linkitem(found, 0xffffffff, human, hand);
    if (sr_items[found].type == ITEM_DOOR)
        grip_loose_door(hu, hand, &sr_items[found]);
}

/* A magazine goes from the gun in the other hand to the empty hand. */
static void unload(Human *hu, int human, int hand)
{
    InventorySlot *s = &hu->inventory[hand];
    const InventorySlot *o = &hu->inventory[hand ^ 1];
    const Item *gun;

    if (!(o->count > 0))
        return;
    gun = &sr_items[o->items[0]];
    if (gun->num_children > 0 && sr_itemtypes[sr_items[gun->children[0]].type].fits_in[gun->type] != 0 &&
        linkitem(gun->children[gun->num_children - 1], 0xffffffff, human, hand) != 0) {
        hu->reload_timer = RELOAD_TICKS;
        if (sr_itemtypes[sr_items[o->items[0]].type].is_gun != 0)
            createevent_sound(g_soundlist.magazine, &sr_items[s->items[0]].pos, 1.0f, 0.875f);
    }
}

/* The hand is empty. */
static void empty_hand(Human *hu, int human, int hand, const Keys *k)
{
    InventorySlot *s = &hu->inventory[hand];
    int p;

    if (key_edge(k, BTN_LOAD) && s->count == 0)
        unload(hu, human, hand);
    for (p = 0; p < NUM_POCKETS; p++) {
        const InventorySlot *pocket = &hu->inventory[FIRST_POCKET + p];

        if (s->count == 0 && pocket->count > 0 && key_edge(k, BTN_POCKET << p))
            linkitem(pocket->items[0], 0xffffffff, human, hand);
    }
    if (s->count == 0 && !(k->now & BTN_USE) && (k->before & BTN_USE))
        pick_up(hu, human, hand);
}

/* The hand holds a door: the hand target is a point in front of the end of the door. */
static void hold_door(Human *hu, int hand, const Item *door)
{
    HandGrip *g = &hu->hand_grips[hand];
    vec3 m[3];

    m[0].x = 1.0f; m[0].y = 0.0f; m[0].z = 0.0f;
    m[1].x = 0.0f; m[1].y = 1.0f; m[1].z = 0.0f;
    m[2].x = 0.0f; m[2].y = 0.0f; m[2].z = 1.0f;
    rotateorientation(m, &g_yaxis, hu->input_axes[0]);
    g->pos.x = (float)(door->end_pos[0].x + m[0].x);
    g->pos.y = (float)(door->end_pos[0].y + m[0].y);
    g->pos.z = (float)(door->end_pos[0].z + m[0].z);
    g->pos.x = (float)(g->pos.x + m[1].x * 1.25);
    g->pos.y = (float)(m[1].y * 1.25 + g->pos.y);
    g->pos.z = (float)(m[1].z * 1.25 + g->pos.z);
    g->pos.x = (float)(g->pos.x - m[2].x * 0.125);
    g->pos.y = (float)(g->pos.y - m[2].y * 0.125);
    g->pos.z = (float)(g->pos.z - m[2].z * 0.125);
    g->axis = hu->input_axes[0];
}

/* The hand holds an item: the buttons go to the item, the drop key and the load key work, and
 * a pocket key moves the item to the pocket. */
static void full_hand(Human *hu, int human, int hand, const Keys *k)
{
    InventorySlot *s = &hu->inventory[hand];
    int other = hand ^ 1;
    int p;
    Item *it = &sr_items[s->items[0]];

    if (it->type == ITEM_DOOR)
        hold_door(hu, hand, it);
    if (k->now & BTN_USE)
        it->flags |= 1;
    if (k->now & BTN_LOAD)
        it->flags |= 2;
    if ((k->now & BTN_DROP) && (uint32_t)s->items[0] < MAX_ITEMS)
        drop_item(s->items[0]);
    if (key_edge(k, BTN_LOAD) && s->count > 0 && hu->inventory[other].count > 0) {
        uint32_t held = s->items[0];
        uint32_t target = hu->inventory[other].items[0];

        /* A magazine in the hand goes into the gun in the other hand. */
        if (sr_itemtypes[sr_items[held].type].fits_in[sr_items[target].type] != 0 &&
            linkitem(target, held, -1, 0) != 0) {
            hu->reload_timer = RELOAD_TICKS;
            if (sr_itemtypes[sr_items[hu->inventory[other].items[0]].type].is_gun != 0)
                createevent_sound(g_soundlist.magazine, &sr_items[s->items[0]].pos, 1.0f, 1.0f);
        }
    }
    for (p = 0; p < NUM_POCKETS; p++) {
        if (s->count > 0) {
            uint32_t item = s->items[0];

            if (sr_items[item].type != ITEM_BRIEFCASE && sr_items[item].type != ITEM_OPEN_BRIEFCASE &&
                key_edge(k, BTN_POCKET << p))
                linkitem(item, 0xffffffff, human, FIRST_POCKET + p);
        }
    }
}

/* The swap key: the item in the left hand goes to the right hand and the other way. */
static void swap_hands(Human *hu, int human)
{
    int count = hu->inventory[0].count;

    if (count < 1) {
        if (hu->inventory[1].count > 0 && count == 0)
            linkitem(hu->inventory[1].items[0], 0xffffffff, human, 0);
    } else {
        uint32_t left = hu->inventory[0].items[0];

        if (hu->inventory[1].count == 0) {
            linkitem(left, 0xffffffff, human, 1);
        } else {
            uint32_t right = hu->inventory[1].items[0];

            if (left < MAX_ITEMS)
                drop_item(left);
            linkitem(right, 0xffffffff, human, 0);
            linkitem(left, 0xffffffff, human, 1);
        }
    }
}

void human_itemsimulation(int human)
{
    Human *hu = &sr_humans[human];
    Keys keys;
    int hand, slot, j;

    if (hu->reload_timer > 0)
        hu->reload_timer--;
    keys.now = hu->input_buttons;
    keys.before = hu->input_buttons_prev;
    hand = (keys.now & BTN_HAND) != 0;
    if (hu->inventory[hand].count == 0)
        reach(hu, &keys);
    if (hu->inventory[hand].count < 1)
        empty_hand(hu, human, hand, &keys);
    else
        full_hand(hu, human, hand, &keys);
    if (key_edge(&keys, BTN_SWAP))
        swap_hands(hu, human);
    for (slot = 0; slot < NUM_SLOTS; slot++) {
        const InventorySlot *s = &hu->inventory[slot];

        for (j = 0; j < s->count; j++) {
            int item = (&s->items[0])[j];

            sr_items[item].at_rest = 0;
            if (sr_itemtypes[sr_items[item].type].is_gun != 0)
                hu->protect_timer = 0;
            setitempositionhuman(item, human);
        }
    }
}
