/* The arms of a human: the angles that the muscles of the chest, the head and the arms hold, the
 * bonds that hold an item in each hand, and the turn that moves each arm toward its target. */
#include "humans.h"

#include <stdint.h>

#include "items.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define g_xaxis (*SR_AT(vec3, 0x60cd4)) /* (1 0 0) */
#define g_yaxis (*SR_AT(vec3, 0x60ce0)) /* (0 1 0) */
#define g_zaxis (*SR_AT(vec3, 0x60cec)) /* (0 0 1) */
#define g_body SR_AT(Body, 0xf270280)   /* the bone tree: the parent and the offset of each bone */
#define g_bondopt SR_AT(BondOpt, SR_RVA_BONDOPT)

#define PI_2 1.5707963705062866f /* pi / 2 */
#define PI_4 0.7853981852531433f /* pi / 4 */
#define PI_8 0.39269909262657166f /* pi / 8 */
#define PI_16 0.19634954631328583f /* pi / 16 */
#define BOND_HOLD 10 /* holds a point of an item at a point of a bone */
#define BOND_TURN 11 /* turns an item toward the turn of a bone */
#define HOLD_UNREAD 0.266666681f /* 4 / 15 */

/* The dot product with the grouping that these functions use: z + (x + y). */
static float dot3(const vec3 *a, const vec3 *b)
{
    return (float)(a->z * b->z + (a->x * b->x + a->y * b->y));
}

/* r = the turn of item it in the frame of bone b. */
static void item_in_bone(vec3 r[3], const Bone *b, const Item *it)
{
    int i, j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            (&r[i].x)[j] = dot3(&b->rot[i], &it->rot[j]);
}

/* q.w in -1..1. A NaN becomes -1. */
static void clamp_w(quat *q)
{
    if (!(-1.0 <= q->w))
        q->w = -1.0f;
    else if (1.0 < q->w)
        q->w = 1.0f;
}

/* The angle of the turn of a unit quaternion. */
static float quat_angle(const quat *q)
{
    double a = x87_acos(q->w);

    return (float)(a + a);
}

/* Sets the fields of a bond of type `type` between bone `bone` of human `human` and item `item`.
 * The other fields keep their old values. */
static Bond *item_bond(int i, int type, int human, int bone, int item, float stiffness, float damping)
{
    Bond *b = &sr_bonds[i];

    b->type = type;
    b->owner = human;
    b->bone_a = bone;
    b->target = item;
    b->mode = 0;
    b->stiffness = stiffness;
    b->damping = damping;
    b->unread_84 = HOLD_UNREAD;
    return b;
}

/* The point of a hold bond: local_a is 0 and local_b is the grip on the item. */
static void hold_points(Bond *b, const vec3 *grip)
{
    b->local_a.x = 0.0f;
    b->local_a.y = 0.0f;
    b->local_a.z = 0.0f;
    b->local_b.x = (float)(grip->x + 0.0);
    b->local_b.y = (float)(grip->y + 0.0);
    b->local_b.z = (float)(grip->z + 0.0);
}

/* dir *= the stiffness of bond i. Then the mass shares of bone b and item it, and the smallest
 * inertia of the item. */
static void turn_finish(BondOpt *o, int i, const Bone *b, const Item *it)
{
    float k = sr_bonds[i].stiffness;
    float inv, first;

    o->dir.x = k * o->dir.x;
    o->dir.y = (float)(o->dir.y * k);
    o->dir.z = (float)(k * o->dir.z);
    inv = (float)(1.0 / (b->mass + it->mass));
    o->share_a = (float)(inv * it->mass);
    o->share_b = (float)(inv * b->mass);
    first = it->inertia.x;
    o->arm_a.x = first;
    if (it->inertia.y < first)
        o->arm_a.x = it->inertia.y;
    if (it->inertia.z < o->arm_a.x)
        o->arm_a.x = it->inertia.z;
}

/* The bonds that hold the item of slot 0 in the right hand (bone 8). It returns 0 for a door: the
 * caller then stops. */
static int hold_right(int human, const Human *hu)
{
    const Bone *hand = &hu->bones[8];
    int item = hu->inventory[0].items[0];
    const Item *it = &sr_items[item];
    int type = it->type, b;
    vec3 grip = sr_itemtypes[type].grip, r[3], tmp;
    BondOpt *o;
    float ang;
    quat q;

    if (type == ITEM_DOOR) {
        grip.x = 0.5f;
        grip.y = 0.25f;
        grip.z = -0.125f;
    }
    if (type == ITEM_CELL_PHONE)
        grip.z = 0.0f;
    if (type == ITEM_CASH_2) {
        grip.y = 0.0f;
        grip.z = 0.125f;
    }
    if (it->type == ITEM_NEWSPAPER) {
        grip.x = 0.1625f;
        grip.y = 0.0f;
        grip.z = 0.1875f;
    }
    b = sr_numofbonds;
    hold_points(item_bond(b, BOND_HOLD, human, 8, item, 0.25f, 0.25f), &grip);
    sr_numofbonds = b + 1;
    if (it->type == ITEM_DOOR)
        return 0;

    b = b + 1;
    item_bond(b, BOND_TURN, human, 8, item, 0.0625f, 0.25f);
    g_bondopt[b].damping = sr_bonds[b].damping;
    item_in_bone(r, hand, it);
    rotateorientation(r, &g_xaxis, PI_2);
    if (it->type == ITEM_CASH_2)
        rotateorientation(r, &g_xaxis, -1.1780972480773926f);
    if (it->type == ITEM_NEWSPAPER)
        rotateorientation(r, &g_xaxis, -1.3744467496871948f);
    ang = -0.09817477315664291f;
    if (it->type == ITEM_9MM)
        ang = -0.04908738521234375f;
    if (it->type == ITEM_UZI)
        ang = -0.1227184608578682f;
    rotateorientation(r, &g_xaxis, ang);
    orientationtoquaternion(&q, r);
    q.x = -q.x;
    q.y = -q.y;
    q.z = -q.z;
    normalizevector((vec3 *)&q, (const vec3 *)&q);
    clamp_w(&q);
    ang = quat_angle(&q);
    o = &g_bondopt[sr_numofbonds];
    o->dir.x = ang * q.x;
    o->dir.y = (float)(q.y * ang);
    o->dir.z = (float)(ang * q.z);
    tmp = o->dir;
    projectvector(&o->dir, &tmp, hand->rot);
    turn_finish(o, sr_numofbonds, hand, it);
    sr_numofbonds++;
    return 1;
}

/* The bonds that hold an item in the left hand (bone 5): the item of slot 1, or a two-handed gun
 * of slot 0. It returns 0 for a door: the caller then stops. */
static int hold_left(int human, const Human *hu)
{
    const Bone *hand = &hu->bones[5];
    int in_slot1 = hu->inventory[1].count;
    int b = sr_numofbonds, item;
    const Item *it;
    vec3 grip, r[3], tmp;
    BondOpt *o;
    float ax, ay, az, s, c;

    grip.y = -0.0625f;
    grip.z = -0.1875f;
    if (in_slot1 == 0) {
        item = hu->inventory[0].items[0];
    } else {
        item = hu->inventory[1].items[0];
        grip.y = 0.0f;
        grip.z = 0.0625f;
        if (sr_items[item].type == ITEM_BRIEFCASE)
            grip.z = 0.1875f;
    }
    it = &sr_items[item];
    grip.x = 0.0f;
    if (in_slot1 == 0 && sr_itemtypes[it->type].one_handed != 0)
        return 1;
    if (it->type == ITEM_DOOR) {
        grip.y = 0.125f;
        grip.x = 0.5f;
    }
    hold_points(item_bond(b, BOND_HOLD, human, 5, item, 0.0625f, 0.09375f), &grip);
    if (0 < in_slot1 || sr_itemtypes[it->type].is_gun != 0) {
        b = b + 1;
        sr_numofbonds = b;
    }
    if (it->type == ITEM_DOOR)
        return 0;
    if (!(0 < in_slot1))
        return 1;

    item_bond(b, BOND_TURN, human, 5, item, 0.03125f, 0.25f);
    g_bondopt[b].damping = sr_bonds[b].damping;
    item_in_bone(r, hand, it);
    rotateorientation(r, &g_xaxis, PI_2);
    if (it->type == ITEM_BRIEFCASE)
        rotateorientation(r, &g_yaxis, PI_2);
    if (it->type == ITEM_CASH_2)
        rotateorientation(r, &g_xaxis, -1.1780972480773926f);
    ax = (float)x87_atan2(r[1].z, r[2].z);
    ay = (float)x87_atan2(-r[0].z, (float)x87_sqrt(r[0].x * r[0].x + r[0].y * r[0].y));
    s = (float)x87_sin(ax);
    c = (float)x87_cos(ax);
    az = (float)x87_atan2(s * r[2].x - c * r[1].x, c * r[1].y - r[2].y * s);
    o = &g_bondopt[sr_numofbonds];
    o->dir.x = ax;
    o->dir.y = ay;
    o->dir.z = az;
    tmp = o->dir;
    o->dir.x = tmp.x * hand->rot[0].x;
    o->dir.y = (float)(hand->rot[0].y * tmp.x);
    o->dir.z = (float)(tmp.x * hand->rot[0].z);
    scaleaddvectors(&o->dir, &o->dir, &hand->rot[1], tmp.y);
    scaleaddvectors(&o->dir, &o->dir, &hand->rot[2], tmp.z);
    turn_finish(o, sr_numofbonds, hand, it);
    sr_numofbonds++;
    return 1;
}

/* The angles that the muscles of the chest, the head and the arms hold, and the bonds of the items
 * in the hands. A dead human keeps the angles of its arm bones. Then human_calcarmtorque, except
 * when a hand holds a door. */
void human_calculatearmangles(int human)
{
    Human *hu = &sr_humans[human];
    int count0 = hu->inventory[0].count;
    int has_gun = 0, k;
    vec3 *chest, *head, *axis;
    float swing, va, vp, last, ax, az, s, c;

    if (0 < count0 && sr_itemtypes[sr_items[hu->inventory[0].items[0]].type].is_gun != 0)
        has_gun = 1;
    if (hu->health < 1) {
        for (k = 1; k <= 8; k++) {
            const Bone *p = &hu->bones[g_body[k].parent];
            Bone *b = &hu->bones[k];
            float a1 = dot3(&b->rot[0], &p->rot[1]), b1 = dot3(&b->rot[1], &p->rot[1]);
            float c1 = dot3(&b->rot[2], &p->rot[1]), a2 = dot3(&b->rot[0], &p->rot[2]);
            float b2 = dot3(&b->rot[1], &p->rot[2]), c2 = dot3(&b->rot[2], &p->rot[2]);

            ax = (float)x87_atan2(c1, c2);
            s = (float)x87_sin(ax);
            c = (float)x87_cos(ax);
            az = (float)x87_atan2(s * a2 - c * a1, c * b1 - b2 * s);
            if (ax < -PI_4)
                ax = -PI_4;
            if (PI_4 < ax)
                ax = PI_4;
            if ((k == 4 || k == 7) && 0.0 < ax)
                ax = 0.0f;
            resetorientation(b->target_rot);
            rotateorientation(b->target_rot, &g_xaxis, ax);
            if (k == 3) {
                if (az < 0.0)
                    az = 0.0f;
                rotateorientation(b->target_rot, &b->target_rot[2], az);
            } else if (k == 6) {
                if (0.0 < az)
                    az = 0.0f;
                rotateorientation(b->target_rot, &b->target_rot[2], az);
            }
        }
        return;
    }

    /* The chest turns with the aim, and swings a gun to the side. */
    swing = PI_8;
    if (hu->move_mode == 2)
        swing = PI_4;
    if ((hu->input_buttons & 2) != 0)
        swing = 0.0f;
    if (!has_gun)
        swing = 0.0f;
    if (0 < count0 && sr_itemtypes[sr_items[hu->inventory[0].items[0]].type].one_handed != 0)
        swing = -swing;
    va = 0.0f;
    vp = 0.0f;
    if (hu->vehicle_id != -1) {
        va = (float)(hu->view_axis + 0.0);
        vp = (float)(hu->view_pitch + 0.0);
    }
    chest = hu->bones[1].target_rot;
    head = hu->bones[2].target_rot;
    resetorientation(chest);
    if (hu->vehicle_id == -1) {
        rotateorientation(chest, &chest[1], (float)(hu->arm_axis * 0.5 + va));
        rotateorientation(chest, &chest[0], (float)(hu->arm_pitch * 0.75 + hu->pitch * 0.5));
        rotateorientation(chest, &chest[1], swing);
        resetorientation(head);
        rotateorientation(head, &g_yaxis, (float)((hu->arm_axis * 0.5 - swing) + 0.04908738521234375));
        if (hu->move_mode == 2 && has_gun)
            rotateorientation(head, &head[2], PI_16);
        last = (float)(hu->arm_pitch * 0.25 + hu->pitch * 0.5);
        axis = &head[0];
    } else {
        rotateorientation(chest, &chest[1], (float)(va + swing));
        rotateorientation(chest, &chest[0], vp);
        resetorientation(head);
        last = -swing;
        axis = &head[1];
    }
    rotateorientation(head, axis, last);

    if (0 < hu->inventory[0].count) {
        if (hold_right(human, hu) == 0)
            return;
    } else if (hu->inventory[1].count < 1) {
        human_calcarmtorque(human);
        return;
    }
    if (hold_left(human, hu) == 0)
        return;
    human_calcarmtorque(human);
}

/* out = the turn from rel (the bone in the frame of its parent) to qa: the axis times the
 * angle, in the world frame of the bone rows rot. */
static void turn_toward(vec3 *out, const vec3 rot[3], quat *qa, quat *rel)
{
    vec3 v;
    float ang;

    rel->x = -rel->x;
    rel->y = -rel->y;
    rel->z = -rel->z;
    projectorientation(qa, rel, qa);
    normalizevector((vec3 *)qa, (const vec3 *)qa);
    clamp_w(qa);
    ang = quat_angle(qa);
    v.x = (float)(ang * qa->x);
    v.y = (float)(ang * qa->y);
    v.z = (float)(ang * qa->z);
    out->x = (float)(v.x * rot[0].x + v.y * rot[1].x + v.z * rot[2].x);
    out->y = (float)(rot[2].y * v.z + (rot[0].y * v.x + rot[1].y * v.y));
    out->z = (float)(v.x * rot[0].z + rot[1].z * v.y + rot[2].z * v.z);
}

/* The item type in the hand of side `side` (0 left, 1 right), or -1. The left hand holds the
 * item of slot 1, else the item of slot 0. */
static int hand_item_type(const Human *hu, int side)
{
    if (side == 0 && 0 < hu->inventory[1].count)
        return sr_items[hu->inventory[1].items[0]].type;
    if (0 < hu->inventory[0].count)
        return sr_items[hu->inventory[0].items[0]].type;
    return -1;
}

/* The target of the hand of side `side` in the frame of the chest, for the item type it holds. */
static void hand_target(vec3 *t, const Human *hu, int side, int type, float swing)
{
    int count0, mm;

    t->x = 0.0f;
    t->y = 0.0f;
    t->z = -0.5f;
    if (side == 0) {
        t->x = -0.5f;
    } else if (side == 1 && type == ITEM_CELL_PHONE) {
        if (sr_items[hu->inventory[0].items[0]].call_state == 2)
            t->y = 0.125f;
        if (sr_items[hu->inventory[0].items[0]].call_state == 3) {
            t->y = (float)(t->y + 0.1875);
            t->z = -0.0625f;
        }
    }
    /* For an empty hand (type -1) the game reads the record before the first item type. */
    if (sr_itemtypes[type].is_gun != 0) {
        mm = hu->move_mode;
        t->x = -0.0625f;
        t->y = 0.0625f;
        t->z = -0.725000024f;
        if (mm == 0) {
            t->x = 0.0f;
            t->y = -0.25f;
            t->z = -0.3125f;
        }
        if (mm == 1) {
            t->x = 0.0f;
            t->y = 0.0f;
            t->z = -0.625f;
        }
        t->z = (float)(t->z * 0.375);
        if (mm == 2) {
            t->x = -0.09375f;
            t->y = 0.09375f;
            t->z = -0.5f;
        }
        if (side == 1) {
            if (0 < hu->inventory[0].count &&
                sr_itemtypes[sr_items[hu->inventory[0].items[0]].type].one_handed != 0) {
                t->y = 0.0625f;
                t->z = -0.725000024f;
                if (mm == 0) {
                    t->y = -0.125f;
                    t->z = -0.375f;
                }
                t->x = 0.0f;
                if (mm == 1) {
                    t->x = 0.0f;
                    t->y = -0.0625f;
                    t->z = -0.375f;
                }
            }
        } else if (side == 0) {
            t->x = (float)(t->x - g_zaxis.x * 0.3125);
            t->y = (float)(t->y - g_zaxis.y * 0.3125);
            t->z = (float)(t->z - g_zaxis.z * 0.3125);
        }
    }
    if (hu->vehicle_id == -1)
        rotatevector(t, &g_xaxis, (float)(hu->pitch * 0.5));
    rotatevector(t, &g_yaxis, swing);
    if (side == 0) {
        t->x = (float)(t->x + g_xaxis.x * 0.5);
        t->y = (float)(g_xaxis.y * 0.5 + t->y);
        t->z = (float)(g_xaxis.z * 0.5 + t->z);
    }
    count0 = hu->inventory[0].count;
    if (count0 == 0) {
        t->x = 0.0f;
        t->y = -0.75f;
        t->z = -0.0f;
    }
    if (side == 0) {
        if (hu->inventory[1].count == 0) {
            if (0 < count0 && sr_itemtypes[sr_items[hu->inventory[0].items[0]].type].one_handed != 0) {
                t->x = 0.0f;
                t->y = -0.625f;
                t->z = -0.125f;
            }
            if (0 < count0 && sr_itemtypes[sr_items[hu->inventory[0].items[0]].type].is_gun == 0) {
                t->x = 0.0f;
                t->y = -0.625f;
                t->z = -0.125f;
            }
        }
        if (type == ITEM_BRIEFCASE || type == ITEM_OPEN_BRIEFCASE) {
            t->x = 0.0f;
            t->y = -0.75f;
            t->z = -0.0f;
        }
    }
}

/* Moves the hand of each arm a step toward its target, and sets the muscles (Bone.bond) of the
 * shoulder, the elbow and the wrist to turn the arm there. The shoulders are bones 3 and 6, the
 * elbows 4 and 7, the hands 5 and 8. */
void human_calcarmtorque(int human)
{
    const Human *hu = &sr_humans[human];
    const Bone *chest = &hu->bones[1];
    float swing, step, dist, s, dy, reach, bend;
    double half, a;
    quat lean, fix, rel, qa, qs, qe;
    vec3 shoulder, hand, hand_vel, sv, rv, d, r, t, e, m, f[3];
    int side, type;

    swing = hu->move_mode == 2 ? -PI_4 : -PI_8;
    if (0 < hu->inventory[0].count && sr_itemtypes[sr_items[hu->inventory[0].items[0]].type].one_handed != 0)
        swing = -swing;
    if (hu->inventory[0].count == 0)
        swing = 0.0f;
    else if (sr_itemtypes[sr_items[hu->inventory[0].items[0]].type].is_gun == 0)
        swing = 0.0f;
    half = swing * 0.5;
    lean.x = 0.0f;
    lean.y = -(float)x87_sin(swing * 0.5);
    lean.z = 0.0f;
    lean.w = (float)x87_cos(half);

    for (side = 0; side < 2; side++) {
        const Bone *sh = &hu->bones[side * 3 + 3];
        const Bone *el = &hu->bones[side * 3 + 4];
        const Bone *hd = &hu->bones[side * 3 + 5];
        const vec3 *j = &g_body[side * 3 + 3].joint;

        type = hand_item_type(hu, side);

        /* The shoulder, and the hand and its velocity, in the frame of the chest. */
        shoulder.x = (float)(j->z * chest->rot[2].x +
                             (float)(j->y * chest->rot[1].x + (float)(chest->pos.x + j->x * chest->rot[0].x)));
        shoulder.y = (float)(j->z * chest->rot[2].y +
                             (float)(chest->rot[1].y * j->y + (float)(chest->rot[0].y * j->x + chest->pos.y)));
        shoulder.z = (float)(j->z * chest->rot[2].z +
                             (float)(j->y * chest->rot[1].z + (float)(j->x * chest->rot[0].z + chest->pos.z)));
        d.x = (float)(hd->pos.x - shoulder.x);
        d.y = (float)(hd->pos.y - shoulder.y);
        d.z = (float)(hd->pos.z - shoulder.z);
        hand.x = (float)(d.z * chest->rot[0].z + (d.y * chest->rot[0].y + d.x * chest->rot[0].x));
        hand.y = (float)(d.z * chest->rot[1].z + (d.x * chest->rot[1].x + chest->rot[1].y * d.y));
        hand.z = (float)(chest->rot[2].x * d.x + chest->rot[2].y * d.y + chest->rot[2].z * d.z);
        r.x = (float)(shoulder.x - chest->pos.x);
        r.y = (float)(shoulder.y - chest->pos.y);
        r.z = (float)(shoulder.z - chest->pos.z);
        sv.x = (float)((float)(r.z * chest->w.y - r.y * chest->w.z) + chest->vel.x);
        sv.y = (float)((float)(r.x * chest->w.z - chest->w.x * r.z) + chest->vel.y);
        sv.z = (float)((float)(chest->w.x * r.y - r.x * chest->w.y) + chest->vel.z);
        rv.x = (float)(hd->vel.x - sv.x);
        rv.y = (float)(hd->vel.y - sv.y);
        rv.z = (float)(hd->vel.z - sv.z);
        hand_vel.x = (float)(rv.z * chest->rot[0].z + (rv.y * chest->rot[0].y + rv.x * chest->rot[0].x));
        hand_vel.y = (float)(rv.z * chest->rot[1].z + (rv.x * chest->rot[1].x + chest->rot[1].y * rv.y));
        hand_vel.z = (float)(chest->rot[2].x * rv.x + chest->rot[2].y * rv.y + chest->rot[2].z * rv.z);

        hand_target(&t, hu, side, type, swing);

        /* A step toward the target: at most 0.125 across, 0.0625 up or down. */
        step = 0.125f;
        e.x = (float)(t.x - hand.x);
        e.z = (float)(t.z - hand.z);
        e.y = 0.0f;
        dist = (float)(e.x * e.x + 0.0f + e.z * e.z);
        dist = (float)x87_sqrt(dist);
        s = (float)((float)x87_sqrt((dist + dist) * 32.0) * 0.25 * 0.03125);
        if (s < 0.125)
            step = s;
        if (step < dist) {
            normalizevector(&e, &e);
            t.x = (float)(hand.x + step * e.x);
            t.z = (float)(step * e.z + hand.z);
        }
        dy = (float)(t.y - hand.y);
        if (dy < -0.0625)
            dy = -0.0625f;
        if (0.0625 < dy)
            dy = 0.0625f;
        t.y = (float)(dy + hand.y);

        /* The move differs from the velocity of the hand by at most 0.03125 across. Then the
         * move gets a quarter of that change again. */
        m.x = (float)(t.x - hand.x);
        m.y = (float)(t.y - hand.y);
        m.z = (float)(t.z - hand.z);
        e.x = (float)(m.x - hand_vel.x);
        e.z = (float)(m.z - hand_vel.z);
        e.y = 0.0f;
        s = (float)(e.x * e.x + 0.0f + e.z * e.z);
        s = (float)x87_sqrt(s);
        if (0.03125 < s) {
            normalizevector(&e, &e);
            m.x = (float)(hand_vel.x + e.x * 0.03125);
            m.z = (float)(e.z * 0.03125 + hand_vel.z);
        }
        e.x = (float)(m.x - hand_vel.x);
        e.y = (float)(m.y - hand_vel.y);
        e.z = (float)(m.z - hand_vel.z);
        m.x = (float)(e.x * 0.25 + m.x);
        m.y = (float)(e.y * 0.25 + m.y);
        m.z = (float)(m.z + e.z * 0.25);
        t.x = (float)(m.x + hand.x);
        t.y = (float)(m.y + hand.y);
        t.z = (float)(m.z + hand.z);

        /* The elbow bends so that the arm (2 x 0.375) reaches the target. */
        reach = (float)(t.z * t.z + (t.x * t.x + t.y * t.y));
        reach = (float)x87_sqrt(reach);
        if (0.75 < reach)
            reach = 0.75f;
        a = x87_acos(reach / 0.75);
        bend = (float)(a + a);

        /* The shoulder points the upper arm at the target. The columns of f are x, y and z: y
         * points from the target to the shoulder, z = y x (the x axis), x = y x z. */
        multiplyquaternion(f, sh->rot, chest->rot);
        orientationtoquaternion(&rel, f);
        e.x = -t.x;
        e.y = -t.y;
        e.z = -t.z;
        if (e.z < 0.0625)
            e.z = 0.0625f;
        {
            vec3 y, z, x;

            normalizevector(&y, &e);
            z.x = (float)(y.z * g_xaxis.y - y.y * g_xaxis.z);
            z.y = (float)(y.x * g_xaxis.z - g_xaxis.x * y.z);
            z.z = (float)(g_xaxis.x * y.y - y.x * g_xaxis.y);
            normalizevector(&z, &z);
            x.x = (float)(y.y * z.z - y.z * z.y);
            x.y = (float)(z.x * y.z - y.x * z.z);
            x.z = (float)(z.y * y.x - z.x * y.y);
            f[0].x = x.x;
            f[0].y = y.x;
            f[0].z = z.x;
            f[1].x = x.y;
            f[1].y = y.y;
            f[1].z = z.y;
            f[2].x = x.z;
            f[2].y = y.z;
            f[2].z = z.z;
        }
        rotateorientation(f, &g_xaxis, (float)(-bend * 0.5));
        orientationtoquaternion(&qa, f);
        qs = qa;
        turn_toward(&g_bondopt[sh->bond].dir, sh->rot, &qa, &rel);

        /* The elbow bends by bend. */
        multiplyquaternion(f, el->rot, sh->rot);
        orientationtoquaternion(&rel, f);
        resetorientation(f);
        rotateorientation(f, &g_xaxis, bend);
        orientationtoquaternion(&qa, f);
        qe = qa;
        turn_toward(&g_bondopt[el->bond].dir, el->rot, &qa, &rel);

        /* The wrist turns the hand with the pitch and the swing, at most pi / 16. */
        multiplyquaternion(f, hd->rot, el->rot);
        orientationtoquaternion(&rel, f);
        resetorientation(f);
        rotateorientation(f, &g_xaxis, (float)(-hu->pitch * 0.5));
        orientationtoquaternion(&qa, f);
        qe.x = -qe.x;
        qe.y = -qe.y;
        qe.z = -qe.z;
        projectorientation(&qa, &qa, &qe);
        qs.x = -qs.x;
        qs.y = -qs.y;
        qs.z = -qs.z;
        projectorientation(&qa, &qa, &qs);
        projectorientation(&qa, &qa, &lean);
        if (side == 1 || (type != ITEM_BRIEFCASE && type != ITEM_OPEN_BRIEFCASE)) {
            fix.x = -(float)x87_sin(-0.7853981633975);
            fix.y = 0.0f;
            fix.z = 0.0f;
            fix.w = (float)x87_cos(-0.7853981633975);
            projectorientation(&qa, &qa, &fix);
        }
        rel.x = -rel.x;
        rel.y = -rel.y;
        rel.z = -rel.z;
        projectorientation(&qa, &rel, &qa);
        normalizevector((vec3 *)&qa, (const vec3 *)&qa);
        clamp_w(&qa);
        bend = quat_angle(&qa);
        if (bend < -PI_16)
            bend = -PI_16;
        if (PI_16 < bend)
            bend = PI_16;
        {
            float vx = (float)(qa.x * bend), vy = (float)(bend * qa.y), vz = (float)(bend * qa.z);
            vec3 *dir = &g_bondopt[hd->bond].dir;

            dir->x = (float)(vx * hd->rot[0].x + vy * hd->rot[1].x + vz * hd->rot[2].x);
            dir->y = (float)(hd->rot[2].y * vz + (hd->rot[0].y * vx + hd->rot[1].y * vy));
            dir->z = (float)(vx * hd->rot[0].z + hd->rot[1].z * vy + hd->rot[2].z * vz);
        }
    }
}
