/* The items: create, link and delete them, and the bills of a cash item.
 *
 * An item can be in the inventory of a human (parent_human and parent_slot) or inside
 * another item (parent_item, the place in children[] is parent_slot). Each change of place
 * makes an item event (createevent_updateitem).
 *
 * A cash item holds up to 10 bills: 3 bits each in bills_mask, bill 0 in the low bits.
 * last_bill is the index of the last bill (-1: no bill).
 */
#include "items.h"

#include <string.h>

#include "events.h"
#include "sr_runtime.h"

#define MAX_ITEMS 0x400
#define NO_LIMIT 0xffff /* time_to_live of an item that stays */

/* Game globals. */
#define g_bill_values SR_AT(int, 0x60458) /* the value of each bill type: 1 5 10 20 50 100 1000 0 */
#define g_xaxis (*SR_AT(vec3, 0x60cd4))   /* (1 0 0) */
#define g_yaxis (*SR_AT(vec3, 0x60ce0))   /* (0 1 0) */

/* The shift of the bill at index i (3 bits each): (i * 3) & 31. */
#define BILL_SHIFT(i) ((uint32_t)((i) * 3) & 31)

/* The value of all the bills of a cash item. */
int item_billvalue(int item)
{
    const Item *it = &sr_items[item];
    int i, total = 0;
    for (i = 0; i <= it->last_bill; i++)
        total += g_bill_values[(it->bills_mask >> BILL_SHIFT(i)) & 7];
    return total;
}

/* Put a bill of a type at index pos. The bills from pos up move one place up. Returns 0 when
 * the item holds 10 bills. */
int item_addbill(int item, int pos, int bill)
{
    Item *it = &sr_items[item];
    uint32_t mask;
    int n, i;
    if (it->last_bill > 8)
        return 0;
    n = ++it->last_bill;
    if (pos < n) {
        mask = it->bills_mask;
        for (i = n; i > pos; i--)
            mask = (mask & ~(7u << BILL_SHIFT(i))) | ((mask >> BILL_SHIFT(i - 1)) & 7) << BILL_SHIFT(i);
        it->bills_mask = mask;
        n = pos;
    }
    it->num_bills++;
    it->bills_mask = (~(7u << BILL_SHIFT(n)) & it->bills_mask) | (uint32_t)bill << BILL_SHIFT(n);
    return 1;
}

/* Take the bill at index pos out. The bills above it move one place down. When the last bill
 * goes, the item goes too (time_to_live 0). */
void item_removebill(int item, int pos)
{
    Item *it = &sr_items[item];
    uint32_t mask;
    int n = it->last_bill;
    if (n == 0) {
        it->time_to_live = 0;
        return;
    }
    if (n < 0)
        return;
    it->last_bill = n - 1;
    if (pos > n - 1)
        return;
    mask = it->bills_mask;
    for (; pos <= it->last_bill; pos++) {
        mask &= ~(7u << BILL_SHIFT(pos));
        mask |= ((mask >> BILL_SHIFT(pos + 1)) & 7) << BILL_SHIFT(pos);
        it->bills_mask = mask;
    }
}

/* Put child into parent. The item type of the child must fit in the type of the parent. Up
 * to 5 Cash items can go in (into an Open Briefcase), the other types only alone. */
int linkitemtoitem(int parent, int child)
{
    Item *p = &sr_items[parent], *c = &sr_items[child];
    int n = p->num_children;
    if (n > 7)
        return 0;
    if (sr_itemtypes[c->type].fits_in[p->type] == 0)
        return 0;
    if (c->type == ITEM_CASH || c->type == ITEM_CASH_2) {
        if (n >= 5)
            return 0;
    } else if (n > 0) {
        return 0;
    }
    p->children[n] = child;
    p->num_children++;
    c->parent_item = parent;
    c->parent_slot = n;
    return 1;
}

/* Add an item to an inventory slot of a human. Returns 0 when the slot holds 8 items. */
int item_addhumanlink(int item, int human, int slot)
{
    InventorySlot *s = &sr_humans[human].inventory[slot];
    if (s->count >= 8)
        return 0;
    s->items[s->count++] = item;
    return 1;
}

/* Take an item out of an inventory slot of a human. The last item of the slot takes its
 * place. */
void item_removehumanlink(uint32_t item, int human, int slot)
{
    InventorySlot *s = &sr_humans[human].inventory[slot];
    int i = 0;
    while (i < s->count) {
        if ((uint32_t)s->items[i] != item) {
            i++;
            continue;
        }
        s->items[i] = s->items[--s->count];
        sr_items[item].parent_human = -1;
        createevent_updateitem(item);
    }
}

/* Take child out of parent. The last child of the parent takes its place. */
void item_removeitemlink(uint32_t child, uint32_t parent)
{
    Item *p = &sr_items[parent];
    int i = 0;
    while (i < p->num_children) {
        if ((uint32_t)p->children[i] != child) {
            i++;
            continue;
        }
        p->children[i] = p->children[--p->num_children];
        sr_items[child].parent_item = -1;
        createevent_updateitem(child);
        createevent_updateitem(parent);
    }
}

/* A new item of a type, at pos, with vel (NULL: no velocity) and the orientation rot.
 * Returns its index, or -1 when all 1024 items are in use. */
uint32_t createitem(int type, const vec3 *pos, const vec3 *vel, const vec3 rot[3])
{
    const ItemType *t = &sr_itemtypes[type];
    Item *it;
    uint32_t i;
    for (i = 0; sr_items[i].active != 0; i++)
        if (i + 1 == MAX_ITEMS)
            return 0xffffffff;
    it = &sr_items[i];
    memset(it, 0, sizeof *it);
    it->active = 1;
    it->type = type;
    it->time_to_live = NO_LIMIT;
    it->parent_human = -1;
    it->mass = t->mass;
    it->parent_item = -1;
    it->pos = *pos;
    it->pos_prev = *pos;
    if (vel)
        memcpy(&it->vel, vel, sizeof it->vel);
    memcpy(it->rot, rot, sizeof it->rot);
    it->inv_inertia = t->inv_inertia;
    it->unk_e4 = t->client_param;
    it->ammo = t->ammo;
    it->linked_item = -1;
    it->vehicle_id = -1;
    it->inertia.x = (float)(1.0 / it->inv_inertia.x);
    it->inertia.y = (float)(1.0 / it->inv_inertia.y);
    it->inertia.z = (float)(1.0 / it->inv_inertia.z);
    if (type == ITEM_DOOR) {
        /* The two ends of the door: half a unit back on the x axis, one unit down and up on
         * the y axis. */
        float bx = (float)(g_xaxis.x * -0.5), by = (float)(g_xaxis.y * -0.5), bz = (float)(g_xaxis.z * -0.5), up;
        it->ends[0].x = -0.5f;
        it->ends[0].y = -1.0f;
        it->ends[1].x = -0.5f;
        it->ends[1].y = 1.0f;
        it->end_pos[0].x = (float)(pos->x + bx);
        it->end_pos[0].y = (float)(pos->y + by);
        it->end_pos[0].z = (float)(pos->z + bz);
        up = it->ends[0].y;
        it->end_pos[0].x = (float)(it->end_pos[0].x + up * g_yaxis.x);
        it->end_pos[0].y = (float)(g_yaxis.y * up + it->end_pos[0].y);
        it->end_pos[0].z = (float)(it->end_pos[0].z + g_yaxis.z * up);
        it->end_pos[1].x = (float)(pos->x + bx);
        it->end_pos[1].y = (float)(pos->y + by);
        it->end_pos[1].z = (float)(bz + pos->z);
        up = it->ends[1].y;
        it->end_pos[1].x = (float)(it->end_pos[1].x + up * g_yaxis.x);
        it->end_pos[1].y = (float)(g_yaxis.y * up + it->end_pos[1].y);
        it->end_pos[1].z = (float)(up * g_yaxis.z + it->end_pos[1].z);
    }
    createevent_updateitem(i);
    return i;
}

/* Move an item:
 *   child != -1           child goes into item;
 *   human != -1           item goes into the inventory slot of the human;
 *   child and human -1    item leaves its parent item or its human.
 * Returns 1 when the item moves. */
int linkitem(uint32_t item, uint32_t child, int human, int slot)
{
    Item *it = &sr_items[item];
    int count;
    if (item >= MAX_ITEMS)
        return 0;
    if (child != 0xffffffff) {
        int old_slot = sr_items[child].parent_slot;
        if (!linkitemtoitem((int)item, (int)child))
            return 0;
        if (sr_items[child].parent_human != -1)
            item_removehumanlink(child, sr_items[child].parent_human, old_slot);
        createevent_updateitem(item);
        createevent_updateitem(child);
        return 1;
    }
    if (human == -1) {
        if (it->parent_item != -1) {
            item_removeitemlink(item, (uint32_t)it->parent_item);
            createevent_updateitem(item);
            return 1;
        }
        if (it->parent_human != -1) {
            item_removehumanlink(item, it->parent_human, it->parent_slot);
            createevent_updateitem(item);
            return 1;
        }
        return 0;
    }
    /* Slots 0 and 1 hold one item. Slot 2 holds two guns. The other slots hold two items
     * that are not guns. */
    count = sr_humans[human].inventory[slot].count;
    if (count >= 2 || (slot <= 1 && count >= 1))
        return 0;
    if (slot == 2 ? !sr_itemtypes[it->type].is_gun : slot >= 3 && sr_itemtypes[it->type].is_gun)
        return 0;
    if (it->parent_item != -1) {
        item_removeitemlink(item, (uint32_t)it->parent_item);
        it->parent_item = -1;
    }
    if (it->parent_human != -1)
        item_removehumanlink(item, it->parent_human, it->parent_slot);
    if (item_addhumanlink((int)item, human, slot)) {
        it->parent_human = human;
        it->parent_slot = slot;
    }
    createevent_updateitem(item);
    return 1;
}

/* Count down the life of each item, and delete the items at the end of their life. A
 * deleted item leaves its human and its parent, and its children leave it. */
void itemtimetolive(void)
{
    Item *it = sr_items;
    uint32_t i;
    for (i = 0; i < MAX_ITEMS; i++, it++) {
        if (it->active == 0)
            continue;
        if (it->time_to_live < NO_LIMIT)
            it->time_to_live--;
        if (it->time_to_live >= 1)
            continue;
        if (it->type == ITEM_NEWSPAPER)
            sr_game.num_newspapers--;
        if (it->parent_human != -1)
            item_removehumanlink(i, it->parent_human, it->parent_slot);
        while (it->num_children > 0) {
            int c = it->children[--it->num_children];
            sr_items[c].parent_item = -1;
            createevent_updateitem((uint32_t)c);
        }
        if (it->parent_item != -1)
            item_removeitemlink(i, (uint32_t)it->parent_item);
        if (it->type == ITEM_KEY && (uint32_t)it->vehicle_id < MAX_ITEMS)
            sr_objects[it->vehicle_id].time_to_live = 0; /* the vehicle goes with its key */
        it->active = 0;
        createevent_updateitem(i);
    }
}
