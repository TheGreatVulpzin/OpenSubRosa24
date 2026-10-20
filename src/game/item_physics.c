/* itemsimulation and item_physicssimulation: the motion of the items.
 *
 * A loose item falls and turns by itself (the free physics). It hits the level with a sweep of
 * its center and with its 8 corners. After more than 3 corner hits and a very low speed, it
 * goes to rest (at_rest) and the free physics skips it. A door (item type 25) hangs at two
 * end points instead and pushes the humans that touch it (item_physicssimulation). An item
 * inside another item follows the parent.
 *
 * The rounding follows the game: a (float) cast marks a store to memory, and a plain
 * expression stays in a double register.
 */
#include "items.h"

#include <math.h>

#include "collide.h"
#include "solver.h"
#include "sr_runtime.h"
#include "vectors.h"

#define MAX_ITEMS 0x400
#define MAX_HUMANS 0x200
#define NUM_BONES 15
#define BONE_LEFT_HAND 5
#define BONE_RIGHT_HAND 8
#define BOND_DOOR 14 /* the contact of a door and a bone */
#define MAX_PARENT_SLOT 2

#define GRAVITY 0.0027222223579883575
#define TURN_MIN 1.52587890625E-5

/* Game globals. */
#define g_intersect (*SR_AT(Intersect, SR_RVA_INTERSECT))

/* angvel = the angular momentum along each axis of the item, times the inverse inertia. */
static void item_angvel(Item *it, const vec3 *inv)
{
    int k;

    it->angvel.x = 0.0f;
    it->angvel.y = 0.0f;
    it->angvel.z = 0.0f;
    for (k = 0; k < 3; k++) {
        const vec3 *r = &it->rot[k];
        float f = (float)((float)((r->x * it->ang_momentum.x + r->y * it->ang_momentum.y) +
                                  r->z * it->ang_momentum.z) *
                          (&inv->x)[k]);

        it->angvel.x = (float)(it->angvel.x + f * r->x);
        it->angvel.y = (float)(it->angvel.y + f * r->y);
        it->angvel.z = (float)(it->angvel.z + f * r->z);
    }
}

/* Turns the axes of the item by angvel. A turn below TURN_MIN does nothing. */
static void item_turn(Item *it)
{
    float s = (float)sqrt((float)((it->angvel.x * it->angvel.x + it->angvel.y * it->angvel.y) +
                                  it->angvel.z * it->angvel.z));

    if (TURN_MIN < s) {
        vec3 axis;
        float inv = (float)(1.0 / s);

        axis.x = (float)(inv * it->angvel.x);
        axis.y = (float)(inv * it->angvel.y);
        axis.z = (float)(inv * it->angvel.z);
        rotateorientation(it->rot, &axis, s);
    }
}

/* The length of a vector. */
static float length3(float x, float y, float z)
{
    return (float)sqrt((float)((x * x + y * y) + z * z));
}

/* One end of a door: a spring and a damper pull the world point of the end toward end_pos.
 * The change goes to dv and dw. */
static void door_end(Item *it, int e)
{
    const vec3 *local = &it->ends[e];
    vec3 a, p, d, c, t;

    a.x = (float)(local->x * it->rot[0].x);
    a.y = (float)(local->x * it->rot[0].y);
    a.z = (float)(local->x * it->rot[0].z);
    a.x = (float)(local->y * it->rot[1].x + a.x);
    a.y = (float)(local->y * it->rot[1].y + a.y);
    a.z = (float)(local->y * it->rot[1].z + a.z);
    a.x = (float)(local->z * it->rot[2].x + a.x);
    a.y = (float)(local->z * it->rot[2].y + a.y);
    a.z = (float)(local->z * it->rot[2].z + a.z);

    p.x = (float)(a.x + it->pos.x);
    p.y = (float)(a.y + it->pos.y);
    p.z = (float)(a.z + it->pos.z);
    d.x = (float)(it->end_pos[e].x - p.x);
    d.y = (float)(it->end_pos[e].y - p.y);
    d.z = (float)(it->end_pos[e].z - p.z);
    d.x = (float)(d.x * 0.125);
    d.y = (float)(d.y * 0.125);
    d.z = (float)(d.z * 0.125);

    /* c = angvel x a, plus the velocity. */
    c.x = (float)(it->angvel.z * a.y - it->angvel.y * a.z);
    c.y = (float)(a.z * it->angvel.x - it->angvel.z * a.x);
    c.z = (float)(it->angvel.y * a.x - a.y * it->angvel.x);
    c.x = (float)(c.x + it->vel.x);
    c.y = (float)(c.y + it->vel.y);
    c.z = (float)(c.z + it->vel.z);
    d.x = (float)(d.x - c.x * 0.125);
    d.y = (float)(d.y - c.y * 0.125);
    d.z = (float)(d.z - c.z * 0.125);

    it->dv.x = (float)(d.x + it->dv.x);
    it->dv.y = (float)(d.y + it->dv.y);
    it->dv.z = (float)(d.z + it->dv.z);
    t.x = (float)(d.y * a.z - a.y * d.z);
    t.y = (float)(d.z * a.x - d.x * a.z);
    t.z = (float)(d.x * a.y - d.y * a.x);
    it->dw.x = (float)(t.x + it->dw.x);
    it->dw.y = (float)(t.y + it->dw.y);
    it->dw.z = (float)(t.z + it->dw.z);
}

/* A door: it slows down, turns, falls and hangs at its two ends. A bone of a human that
 * touches it gets a contact bond. The list of bonds has no limit test here. */
void item_physicssimulation(int item)
{
    Item *it = &sr_items[item];
    int h, b, e;

    it->vel.x = (float)(it->vel.x * 0.9950000047683716);
    it->vel.y = (float)(it->vel.y * 0.9950000047683716);
    it->vel.z = (float)(it->vel.z * 0.9950000047683716);
    it->ang_momentum.x = (float)(it->ang_momentum.x * 0.8999999761581421);
    it->ang_momentum.y = (float)(it->ang_momentum.y * 0.8999999761581421);
    it->ang_momentum.z = (float)(it->ang_momentum.z * 0.8999999761581421);
    it->pos.x = (float)(it->pos.x + it->vel.x);
    it->pos.y = (float)(it->pos.y + it->vel.y);
    it->pos.z = (float)(it->pos.z + it->vel.z);

    item_angvel(it, &it->inv_inertia);
    item_turn(it);

    it->vel.y = (float)(it->vel.y - GRAVITY);
    it->dv.x = it->dv.y = it->dv.z = 0.0f;
    it->dw.x = it->dw.y = it->dw.z = 0.0f;
    for (e = 0; e < 2; e++)
        door_end(it, e);

    it->vel.x = (float)(it->dv.x + it->vel.x);
    it->vel.y = (float)(it->dv.y + it->vel.y);
    it->vel.z = (float)(it->dv.z + it->vel.z);
    it->ang_momentum.x = (float)(it->ang_momentum.x + it->dw.x);
    it->ang_momentum.y = (float)(it->ang_momentum.y + it->dw.y);
    it->ang_momentum.z = (float)(it->ang_momentum.z + it->dw.z);

    for (h = 0; h < MAX_HUMANS; h++) {
        Human *hu = &sr_humans[h];

        if (hu->active == 0)
            continue;
        for (b = 0; b < NUM_BONES; b++) {
            Bone *bone = &hu->bones[b];
            Bond *bond;

            if (b == BONE_LEFT_HAND || b == BONE_RIGHT_HAND || !item_sphereintersect(item, &bone->pos.x, 0.125f))
                continue;
            bond = &sr_bonds[sr_numofbonds++];
            bond->type = BOND_DOOR;
            bond->owner = h;
            bond->bone_a = b;
            bond->target = item;
            bond->rest_length = (float)(0.125 - g_intersect.distance);
            bond->stiffness = 0.03125f;
            bond->damping = 0.125f;
            bond->mode = 0;
            bond->normal.x = g_intersect.normal[0];
            bond->normal.y = g_intersect.normal[1];
            bond->normal.z = g_intersect.normal[2];
            bond->local_a.x = (float)(g_intersect.pos[0] - bone->pos.x);
            bond->local_a.y = (float)(g_intersect.pos[1] - bone->pos.y);
            bond->local_a.z = (float)(g_intersect.pos[2] - bone->pos.z);
            bond->local_b.x = (float)(g_intersect.pos[0] - it->pos.x);
            bond->local_b.y = (float)(g_intersect.pos[1] - it->pos.y);
            bond->local_b.z = (float)(g_intersect.pos[2] - it->pos.z);
            bond->friction = 0.4000000059604645f;
        }
    }
}

/* The sweep of the center of an item along its move: on a hit, the item stops at the surface and
 * keeps the part of the velocity along the surface, with a bounce and a loss. */
static void item_sweep(Item *it)
{
    vec3 v0 = it->vel;
    vec3 v = v0;
    vec3 t;
    float rest = 1.0f;
    float dn, m, l, k, b;
    const float *n = g_intersect.normal;

    if (!lineintersectlevel(&it->pos_prev.x, &it->pos.x, 1))
        return;
    rest = (float)((1.0 - g_intersect.fraction) * rest);
    it->pos.x = (float)(g_intersect.pos[0] + n[0] * 0.001953125);
    it->pos.y = (float)(n[1] * 0.001953125 + g_intersect.pos[1]);
    it->pos.z = (float)(g_intersect.pos[2] + n[2] * 0.001953125);
    dn = (float)(v.z * n[2] + (n[1] * v.y + v.x * n[0]));
    m = -dn;
    t.x = (float)(v.x + m * n[0]);
    t.y = (float)(n[1] * m + v.y);
    t.z = (float)(n[2] * m + v.z);
    l = length3(t.x, t.y, t.z);
    /* The limit stays in a double register. */
    if (fabs(dn) * 0.699999988079071 < l) {
        k = (float)((fabs(dn) * 0.699999988079071) / l);
        t.x = (float)(k * t.x);
        t.y = (float)(t.y * k);
        t.z = (float)(k * t.z);
    }
    b = (float)(m * 1.125);
    v.x = (float)(b * n[0] + v.x);
    v.y = (float)(b * n[1] + v.y);
    v.z = (float)(b * n[2] + v.z);
    v.x = (float)(v.x - t.x);
    v.y = (float)(v.y - t.y);
    v.z = (float)(v.z - t.z);
    v.x = (float)(v.x * 0.9375);
    v.y = (float)(v.y * 0.9375);
    v.z = (float)(v.z * 0.9375);
    it->vel = v;
    it->pos_prev = it->pos;
    it->pos.x = (float)(it->pos.x + rest * v.x);
    it->pos.y = (float)(rest * v.y + it->pos.y);
    it->pos.z = (float)(rest * v.z + it->pos.z);
}

/* The 8 corners of the box of the item: a corner that is under the level pushes the item out
 * and turns it. Returns the number of corners that touch. */
static int item_corners(Item *it, const ItemType *type)
{
    int i, contacts = 0;
    const float *n = g_intersect.normal;

    for (i = 0; i < 8; i++) {
        int u = i & 3;
        float sx = (u == 0 || u == 3) ? -type->size.x : type->size.x;
        float sy = i < 4 ? -type->size.y : type->size.y;
        float sz = u < 2 ? -type->size.z : type->size.z;
        vec3 c, r, dd, j, x, q;
        float pen, f, s;

        c.x = (float)(sx * it->rot[0].x + it->pos.x);
        c.y = (float)(sx * it->rot[0].y + it->pos.y);
        c.z = (float)(sx * it->rot[0].z + it->pos.z);
        c.x = (float)(sy * it->rot[1].x + c.x);
        c.y = (float)(it->rot[1].y * sy + c.y);
        c.z = (float)(sy * it->rot[1].z + c.z);
        c.x = (float)(sz * it->rot[2].x + c.x);
        c.y = (float)(sz * it->rot[2].y + c.y);
        c.z = (float)(sz * it->rot[2].z + c.z);
        if (!lineintersectlevel(&it->pos.x, &c.x, 1))
            continue;
        r.x = (float)(g_intersect.pos[0] - it->pos.x);
        r.y = (float)(g_intersect.pos[1] - it->pos.y);
        r.z = (float)(g_intersect.pos[2] - it->pos.z);
        dd.x = (float)(g_intersect.pos[0] - c.x);
        dd.y = (float)(g_intersect.pos[1] - c.y);
        dd.z = (float)(g_intersect.pos[2] - c.z);
        pen = (float)((n[1] * dd.y + n[0] * dd.x) + n[2] * dd.z);
        f = (float)(pen * 0.125);
        /* x = r x contact_angvel. */
        x.x = (float)(r.y * it->contact_angvel.z - r.z * it->contact_angvel.y);
        x.y = (float)(r.z * it->contact_angvel.x - r.x * it->contact_angvel.z);
        x.z = (float)(r.x * it->contact_angvel.y - it->contact_angvel.x * r.y);
        x.x = (float)(x.x * -0.125);
        x.y = (float)(x.y * -0.125);
        x.z = (float)(x.z * -0.125);
        j.x = (float)(x.x - it->contact_vel.x * 0.125);
        j.y = (float)(x.y - it->contact_vel.y * 0.125);
        j.z = (float)(x.z - it->contact_vel.z * 0.125);
        j.x = (float)(f * n[0] + j.x);
        j.y = (float)(f * n[1] + j.y);
        j.z = (float)(f * n[2] + j.z);
        s = (float)((j.y * n[1] + j.x * n[0]) + n[2] * j.z);
        if (!(0.0 < s))
            continue;
        calculatefriction(&j, (const vec3 *)n, 0.4000000059604645f);
        j.x = (float)(j.x * 0.25);
        j.y = (float)(j.y * 0.25);
        j.z = (float)(j.z * 0.25);
        it->vel.x = (float)(it->vel.x + j.x);
        it->vel.y = (float)(it->vel.y + j.y);
        it->vel.z = (float)(it->vel.z + j.z);
        q.x = (float)(r.z * j.y - r.y * j.z);
        q.y = (float)(r.x * j.z - j.x * r.z);
        q.z = (float)(j.x * r.y - r.x * j.y);
        it->ang_momentum.x = (float)(q.x + it->ang_momentum.x);
        it->ang_momentum.y = (float)(q.y + it->ang_momentum.y);
        it->ang_momentum.z = (float)(q.z + it->ang_momentum.z);
        contacts++;
        item_angvel(it, &type->inv_inertia);
    }
    return contacts;
}

/* The free physics of one item. */
static void item_free(Item *it)
{
    const ItemType *type = &sr_itemtypes[it->type];
    vec3 v0;
    int contacts;

    it->has_physics = 1;
    it->pos.x = (float)(it->pos.x + it->vel.x);
    it->pos.y = (float)(it->pos.y + it->vel.y);
    it->pos.z = (float)(it->pos.z + it->vel.z);
    item_angvel(it, &type->inv_inertia);
    item_turn(it);
    it->vel.y = (float)(it->vel.y - GRAVITY);
    v0 = it->vel;
    item_sweep(it);
    it->contact_vel = it->vel;
    it->contact_angvel = it->angvel;
    contacts = item_corners(it, type);
    if (contacts > 0) {
        it->ang_momentum.x = (float)(it->ang_momentum.x * 0.8999999761581421);
        it->ang_momentum.y = (float)(it->ang_momentum.y * 0.8999999761581421);
        it->ang_momentum.z = (float)(it->ang_momentum.z * 0.8999999761581421);
    }
    /* More than 3 corners down and almost no motion: the item rests. */
    if (contacts > 3) {
        float dx = (float)(it->vel.x - v0.x);
        float dy = (float)(it->vel.y - v0.y);
        float dz = (float)(it->vel.z - v0.z);

        if (length3(dx, dy, dz) < 0.004166666883975267 &&
            (float)sqrt((float)((it->vel.y * it->vel.y + it->vel.x * it->vel.x) + it->vel.z * it->vel.z)) <
                0.0020833334419876337 &&
            (float)sqrt((float)((it->angvel.y * it->angvel.y + it->angvel.x * it->angvel.x) +
                                it->angvel.z * it->angvel.z)) < 0.0020833334419876337)
            it->at_rest = 1;
    }
}

/* An item inside another item follows its parent. The parent type 13 or 14 is a briefcase:
 * cash and notes lie side by side in it. */
static void item_follow(Item *it)
{
    const Item *parent = &sr_items[it->parent_item];

    it->pos = parent->pos;
    it->rot[0] = parent->rot[0];
    it->rot[1] = parent->rot[1];
    it->rot[2] = parent->rot[2];
    if (parent->type == ITEM_BRIEFCASE || parent->type == ITEM_OPEN_BRIEFCASE) {
        if (it->type == ITEM_CASH) {
            float f = (float)((double)(it->parent_slot - 2) * 0.1875 + 0.09375);

            it->pos.x = (float)(f * it->rot[0].x + it->pos.x);
            it->pos.y = (float)(f * it->rot[0].y + it->pos.y);
            it->pos.z = (float)(f * it->rot[0].z + it->pos.z);
        }
        if (it->type == ITEM_CASH_2) {
            float f = (float)((double)(it->parent_slot - 2) * 0.1875 * 0.75);

            it->pos.x = (float)(f * it->rot[0].x + it->pos.x);
            it->pos.y = (float)(f * it->rot[0].y + it->pos.y);
            it->pos.z = (float)(f * it->rot[0].z + it->pos.z);
        }
    } else {
        it->pos.x = (float)(it->pos.x - it->rot[1].x * 0.0625);
        it->pos.y = (float)(it->pos.y - it->rot[1].y * 0.0625);
        it->pos.z = (float)(it->pos.z - it->rot[1].z * 0.0625);
    }
    it->pos_prev = it->pos;
}

/* The items, once for each tick. The free physics runs for a loose item, and for an item in
 * the hand of a human (inventory slot 0 or 1) that is not at rest. */
void itemsimulation(void)
{
    int i;

    for (i = 0; i < MAX_ITEMS; i++) {
        Item *it = &sr_items[i];

        if (it->active == 0)
            continue;
        if (it->pos.y < -32.0f)
            it->time_to_live = 0;
        if (it->countdown > 0)
            it->countdown--;
        it->has_physics = 0;
        if (it->type == ITEM_DOOR) {
            it->has_physics = 1;
            item_physicssimulation(i);
        } else if (it->parent_item == -1 && (it->parent_human == -1 || it->parent_slot < MAX_PARENT_SLOT)) {
            if (it->at_rest == 0)
                item_free(it);
        } else {
            it->at_rest = 0;
        }
        it->pos_prev = it->pos;
    }
    for (i = 0; i < MAX_ITEMS; i++) {
        if (sr_items[i].active != 0 && sr_items[i].parent_item != -1)
            item_follow(&sr_items[i]);
    }
}
