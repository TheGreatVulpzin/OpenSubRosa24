/* The body of a human: the joint bonds and the muscle bonds that hold its 15 bones together,
 * and the end of a human. */
#include "humans.h"

#include <stdint.h>
#include <string.h>

#include "items.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define g_body SR_AT(Body, 0xf270280) /* the bone tree: the parent and the offset of each bone */

#define NUM_HUMANS 0x200
#define NUM_SLOTS 8
#define NUM_BONES 15
#define EVENT_HUMAN 0x11 /* attach_type 1: a new human, 0: the end of a human */

/* Ends human `human`: the player loses it, the items leave its slots, its bonds stop, and the
 * clients get an event. */
void deletehuman(uint32_t human)
{
    Human *hu;
    Event *ev;
    int p, s, k;

    if (human >= NUM_HUMANS)
        return;
    hu = &sr_humans[human];
    p = hu->player_id;
    if (p != -1 && (uint32_t)sr_players[p].human_id == human)
        sr_players[p].human_id = -1;
    for (s = 0; s < NUM_SLOTS; s++)
        while (0 < hu->inventory[s].count)
            item_removehumanlink(hu->inventory[s].items[hu->inventory[s].count - 1], human, s);
    for (k = 0; k < hu->num_joints; k++) {
        sr_bonds[hu->joints[k].bond].type = 0;
        sr_bonds[hu->joints[k].bond].time_to_live = 0;
    }
    for (k = 0; k < hu->num_muscles; k++) {
        sr_bonds[hu->muscles[k].bond].type = 0;
        sr_bonds[hu->muscles[k].bond].time_to_live = 0;
    }
    hu->active = 0;
    if (sr_server != 0) {
        ev = &sr_events[sr_numofevents];
        ev->type = EVENT_HUMAN;
        ev->tick = sr_game.ticks;
        ev->param = human;
        ev->attach_type = 0;
        ev->attach_id = hu->player_id;
        sr_numofevents = (sr_numofevents + 1) & 0xffff;
    }
}

/* A new bond of type `type` between bone bone_a of human `human` and bone target. */
static Bond *new_body_bond(int i, int type, int human, int bone_a, int target)
{
    Bond *b = &sr_bonds[i];

    memset(b, 0, sizeof *b);
    b->type = type;
    b->time_to_live = 0x10000;
    b->mode = 0;
    b->owner = human;
    b->bone_a = bone_a;
    b->target = target;
    b->stiffness = 0.0625f;
    b->damping = 0.125f;
    return b;
}

/* Adds a joint bond (type 8) of human `human` between bone bone_a and bone target, at pos in the
 * frame of bone_a. It returns the bond. */
int createjointbond(int human, int bone_a, int target, const vec3 *pos)
{
    Human *hu = &sr_humans[human];
    HumanJoint *j = &hu->joints[hu->num_joints];
    int i = sr_numofbonds;
    Bond *b;

    j->bond = i;
    j->bone = bone_a;
    j->body = target;
    hu->num_joints++;
    b = new_body_bond(i, 8, human, bone_a, target);
    sr_numofbonds = i + 1;
    b->local_a = *pos;
    b->local_b.x = (float)(pos->x - g_body[target].pos.x);
    b->local_b.y = (float)(pos->y - g_body[target].pos.y);
    b->local_b.z = (float)(pos->z - g_body[target].pos.z);
    return i;
}

/* Adds a joint bond and a muscle bond (type 9) of human `human` between bone parent and bone
 * `bone`. The muscle turns the bone toward Bone.target_rot. */
void human_createmuscleball(int human, int parent, int bone, const vec3 *pos)
{
    Human *hu = &sr_humans[human];
    HumanMuscle *m;
    int i;

    createjointbond(human, parent, bone, pos);
    i = sr_numofbonds;
    m = &hu->muscles[hu->num_muscles];
    m->bond = i;
    m->parent = parent;
    m->bone = bone;
    m->pos = *pos;
    hu->num_muscles++;
    new_body_bond(i, 9, human, parent, bone);
    hu->bones[bone].bond = i;
    sr_numofbonds = i + 1;
}

/* Adds the muscles of human `human`: the chest and the head on the hip, then the two legs on the
 * hip and the two arms on the chest. */
void human_buildmuscles(int human)
{
    static const int limbs[4][2] = {{9, 0}, {12, 0}, {3, 1}, {6, 1}};
    int k, b;

    human_createmuscleball(human, 0, 1, &g_body[1].joint);
    human_createmuscleball(human, 1, 2, &g_body[2].joint);
    for (k = 0; k < 4; k++) {
        b = limbs[k][0];
        human_createmuscleball(human, limbs[k][1], b, &g_body[b].joint);
        human_createmuscleball(human, b, b + 1, &g_body[b + 1].joint);
        human_createmuscleball(human, b + 1, b + 2, &g_body[b + 2].joint);
    }
}

/* Places item `item` on human `human` by the chest bone (bone 1), except an item in slot 0 or
 * 1. The item takes the turn of the chest and the velocity of the hip. */
void setitempositionhuman(int item, int human)
{
    Item *it = &sr_items[item];
    const Bone *chest = &sr_humans[human].bones[1];

    if (it->parent_slot == 0 || it->parent_slot == 1)
        return;
    it->rot[0] = chest->rot[0];
    it->rot[1].x = -chest->rot[2].x;
    it->rot[1].y = -chest->rot[2].y;
    it->rot[1].z = -chest->rot[2].z;
    it->rot[2] = chest->rot[1];
    it->pos.x = (float)(chest->pos.x - it->rot[1].x * 0.25);
    it->pos.y = (float)(chest->pos.y - it->rot[1].y * 0.25);
    it->pos.z = (float)(chest->pos.z - it->rot[1].z * 0.25);
    it->pos.x = (float)(it->pos.x + it->rot[0].x * 0.125);
    it->pos.y = (float)(it->rot[0].y * 0.125 + it->pos.y);
    it->pos.z = (float)(it->rot[0].z * 0.125 + it->pos.z);
    it->pos_prev = it->pos;
    it->vel = sr_humans[human].bones[0].vel;
    it->ang_momentum.x = 0.0f;
    it->ang_momentum.y = 0.0f;
    it->ang_momentum.z = 0.0f;
}

/* The bone tree of a human, filled by setupbody: the parent of each bone, its place and joint in
 * the frame of the parent, its half size along each axis and its mass. */
static const struct {
    int parent;
    vec3 pos, joint, half_size;
    float mass;
} body_table[NUM_BONES] = {
    {0, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.25f, 0.125f, 0.09375f}, 21.0f},              /* hip */
    {0, {0.0f, 0.375f, 0.0f}, {0.0f, 0.125f, 0.0f}, {0.25f, 0.25f, 0.09375f}, 21.0f},           /* chest */
    {1, {0.0f, 0.375f, 0.0f}, {0.0f, 0.1875f, 0.0f}, {0.125f, 0.125f, 0.125f}, 8.0f},           /* head */
    {1, {-0.25f, 0.0f, 0.0f}, {-0.25f, 0.1875f, 0.0f}, {0.0625f, 0.1875f, 0.0625f}, 3.0f},      /* upper arms */
    {3, {0.0f, -0.375f, 0.0f}, {0.0f, -0.1875f, 0.0f}, {0.0625f, 0.1875f, 0.0625f}, 2.5f},
    {4, {0.0f, -0.1875f, 0.0f}, {0.0f, -0.1875f, 0.0f}, {0.0625f, 0.0625f, 0.0625f}, 1.5f},
    {1, {0.25f, 0.0f, 0.0f}, {0.25f, 0.1875f, 0.0f}, {0.0625f, 0.1875f, 0.0625f}, 3.0f},
    {6, {-0.0f, -0.375f, 0.0f}, {-0.0f, -0.1875f, 0.0f}, {0.0625f, 0.1875f, 0.0625f}, 2.5f},
    {7, {-0.0f, -0.1875f, 0.0f}, {-0.0f, -0.1875f, 0.0f}, {0.0625f, 0.0625f, 0.0625f}, 1.5f},
    {0, {-0.09375f, -0.25f, 0.0f}, {-0.09375f, -0.0625f, 0.0f}, {0.09375f, 0.25f, 0.09375f}, 12.0f}, /* legs */
    {9, {0.0f, -0.5f, 0.0f}, {0.0f, -0.25f, 0.0f}, {0.09375f, 0.25f, 0.09375f}, 7.0f},
    {10, {0.0f, -0.25f, 0.0f}, {0.0f, -0.1875f, 0.0f}, {0.09375f, 0.0625f, 0.1875f}, 3.0f},
    {0, {0.09375f, -0.25f, 0.0f}, {0.09375f, -0.0625f, 0.0f}, {0.09375f, 0.25f, 0.09375f}, 12.0f},
    {12, {-0.0f, -0.5f, 0.0f}, {-0.0f, -0.25f, 0.0f}, {0.09375f, 0.25f, 0.09375f}, 7.0f},
    {13, {-0.0f, -0.25f, 0.0f}, {-0.0f, -0.1875f, 0.0f}, {0.09375f, 0.0625f, 0.1875f}, 3.0f},
};

/* Fills the bone tree (g_body) of the humans, and the inverse inertia of each bone: a box of the
 * half size plus the radius, per unit of mass. The hip (bone 0) uses 3 in place of 6. */
void setupbody(void)
{
    Body *bd;
    float div, a, b, c, a2, b2, c2;
    int k;

    for (k = 0; k < NUM_BONES; k++) {
        bd = &g_body[k];
        bd->parent = body_table[k].parent;
        bd->pos = body_table[k].pos;
        if (k != 0)
            bd->joint = body_table[k].joint;
        bd->radius = 0.0625f;
        bd->half_size = body_table[k].half_size;
        bd->mass = body_table[k].mass;
    }
    g_body[3].unread_3c = 0.375f;
    for (k = 0; k < NUM_BONES; k++) {
        bd = &g_body[k];
        div = k == 0 ? 3.0f : 6.0f;
        a = (float)(bd->half_size.x * 2.0 + bd->radius);
        b = (float)(bd->half_size.y * 2.0 + bd->radius);
        c = (float)(bd->half_size.z * 2.0 + bd->radius);
        a2 = a * a;
        b2 = b * b;
        c2 = c * c;
        bd->inv_inertia.x = (float)(1.0 / ((b2 + c2) / div));
        bd->inv_inertia.y = (float)(1.0 / ((a2 + c2) / div));
        bd->inv_inertia.z = (float)(1.0 / ((a2 + b2) / div));
    }
}

/* Makes a human at pos, turned by rot (3 rows of 3 floats), for player `player` (-1: a human with
 * no player, with random colors). It returns the human, or -1 when all 512 are in use. */
int createhuman(float *posp, float *rot, int player)
{
    const vec3 *pos = (const vec3 *)posp;
    Human *hu;
    Event *ev;
    float lx, ly, lz, wx, wy, wz, f;
    int h, k, p, s;

    for (h = 0; h < NUM_HUMANS; h++)
        if (sr_humans[h].active == 0)
            break;
    if (h == NUM_HUMANS)
        return -1;
    hu = &sr_humans[h];
    memset(hu, 0, sizeof *hu);
    hu->active = 1;
    hu->player_id = player;
    hu->account = -1;
    hu->despawn_timer = 0xe10;
    hu->body_param = player == -1 ? 1.0f : sr_players[player].body_param;
    hu->stance = 1.0f;
    hu->health = 100;
    hu->protect_timer = (((uint32_t)os_rand() & 3) + 3) * 0xe10;
    hu->vehicle_id = -1;
    hu->input_buttons_prev = 0x3c0;
    for (s = 0; s < NUM_SLOTS; s++)
        hu->inventory[s].count = 0;
    for (k = 0; k < NUM_BONES; k++) {
        const Body *bd = &g_body[k];
        Bone *b = &hu->bones[k];

        /* The place of the bone in the frame of the human: the sum of the offsets to the hip. */
        lx = bd->pos.x;
        ly = bd->pos.y;
        lz = bd->pos.z;
        for (p = bd->parent; p != 0; p = g_body[p].parent) {
            lx = (float)(g_body[p].pos.x + lx);
            ly = (float)(g_body[p].pos.y + ly);
            lz = (float)(g_body[p].pos.z + lz);
        }
        wx = (float)(pos->x + (float)(ly * rot[3] + lx * rot[0] + lz * rot[6]));
        wy = (float)((float)(rot[7] * lz + (lx * rot[1] + rot[4] * ly)) + pos->y);
        wz = (float)((float)(lz * rot[8] + (rot[5] * ly + rot[2] * lx)) + pos->z);
        b->pos.x = wx;
        b->pos.y = wy;
        b->pos.z = wz;
        b->pos_prev = b->pos;
        memcpy(b->rot, rot, sizeof b->rot);
        b->radius = bd->radius;
        b->fixed_7c = 0.3125f;
        b->half_size = bd->half_size;
        b->mass = bd->mass;
        b->inv_inertia = bd->inv_inertia;
        b->inertia.x = (float)(1.0 / b->inv_inertia.x);
        b->inertia.y = (float)(1.0 / b->inv_inertia.y);
        b->inertia.z = (float)(1.0 / b->inv_inertia.z);
        resetorientation(b->target_rot);
    }
    human_buildmuscles(h);
    hu->spawn_pos = *pos;
    hu->spawn_pos2 = *pos;
    if (player == -1) {
        f = (float)((float)((uint32_t)os_rand() & 0xf) * 0.125 * 0.25 * 0.0625);
        hu->colors[0].x = (float)(0.0625 - f);
        hu->colors[0].y = 0.03125f;
        hu->colors[0].z = -0.15f;
        hu->colors[2].x = (float)(f - 0.0625);
        hu->colors[2].y = 0.03125f;
        hu->colors[2].z = -0.15f;
        f = (float)((float)((uint32_t)os_rand() & 0xf) * 0.125 * 0.0625 + 1.0);
        hu->look_a = 0;
        hu->look_b = 0;
        hu->look_c = 0;
        hu->colors[1].x = f;
        hu->colors[1].y = f;
        hu->colors[1].z = 1.0f;
        hu->colors[3].x = f;
        hu->colors[3].y = f;
        hu->colors[3].z = 1.0f;
    } else {
        const Player *pl = &sr_players[player];

        memcpy(hu->colors, pl->colors, sizeof hu->colors);
        hu->team = pl->team;
        hu->look_a = pl->look_a;
        hu->look_b = pl->look_b;
        hu->look_c = pl->look_c;
    }
    if (sr_server != 0) {
        ev = &sr_events[sr_numofevents];
        ev->type = EVENT_HUMAN;
        ev->tick = sr_game.ticks;
        ev->param = h;
        ev->attach_type = hu->active;
        ev->attach_id = hu->player_id;
        sr_numofevents = (sr_numofevents + 1) & 0xffff;
    }
    return h;
}
