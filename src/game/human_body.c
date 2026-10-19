/* The body of a human: the joint bonds and the muscle bonds that hold its 15 bones together,
 * and the end of a human. */
#include "humans.h"

#include <stdint.h>
#include <string.h>

#include "items.h"
#include "sr_runtime.h"

/* Game globals. */
#define g_body SR_AT(Body, 0xf270280) /* the bone tree: the parent and the offset of each bone */

#define NUM_HUMANS 0x200
#define NUM_SLOTS 8
#define EVENT_DELETEHUMAN 0x11

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
        ev->type = EVENT_DELETEHUMAN;
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
