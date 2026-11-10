/* The smoothing of the world on the client.
 *
 * The server sends the places of humans and objects a few times a second. These functions run
 * every tick and put the parts of the world where they should be now:
 *  - client_simulation moves a human or an object along the line between the last two places
 *    that the server sent, and puts the bones of a human on their joints.
 *  - humansimulationclient places a human that sits in a vehicle on its seat, and the bones
 *    of its arms and legs after its body.
 *  - itemsimulationclient places an item on the human or the item that holds it.
 */
#include "setup/interpolation.h"

#include <stdint.h>

#include "client.h"
#include "sr_runtime.h"
#include "vectors.h"

#define g_human sr_data_human
#define g_object sr_data_object
#define g_objecttype sr_data_objecttype
#define g_item sr_data_item
#define g_player sr_data_player
#define g_body sr_data_body

#define NUM_HUMANS 0x200
#define NUM_OBJECTS 0x400
#define NUM_ITEMS 0x400
#define NUM_BONES 15

/* The bones 1 to 8 are the body. The bones 9 to 14 hang from them: the arms and the legs. */
#define FIRST_BODY_BONE 1
#define FIRST_LIMB_BONE 9

/* The bones that are turned a quarter of a turn after they take the frame of their parent. */
#define BONE_TURNED_A 9
#define BONE_TURNED_B 12

/* Bit of Player.input_buttons: the player holds the button that leaves the vehicle. */
#define BUTTON_EXIT_VEHICLE 0x800
#define EXIT_TIMER_MAX 30

/* An item that is held on a bench of a human has its slot count from this number. */
#define SLOT_BASE 2

/* The extrapolation never goes more than this many ticks past the last place. */
#define MAX_EXTRAPOLATION 64.0f

/* Types of the items that hang from other items. */
#define ITEM_TYPE_HOLDER_A 0xd
#define ITEM_TYPE_HOLDER_B 0xe
#define ITEM_TYPE_HELD_A 0xf
#define ITEM_TYPE_HELD_B 0x10

static const float QUARTER_TURN = -1.5707963705062866f;

static void negatevector(vec3 *out, const vec3 *in)
{
    out->x = -in->x;
    out->y = -in->y;
    out->z = -in->z;
}

/* Move a point along the line from the last place to the new place of the server. The step
 * is the move in one tick, and the point goes as many steps from the new place as ticks have
 * passed since it came (not more than 64). */
static void extrapolate(vec3 *out, int tick, const vec3 *pos, int tick_prev, const vec3 *pos_prev)
{
    vec3 step;
    float span = (float)(tick - tick_prev);
    float elapsed;

    subtractvectors(&step, pos, pos_prev);
    if (0.0 < span)
        scalevector(&step, &step, (float)(1.0 / span));
    elapsed = (float)((int)cl_data_game_tick - tick);
    if (64.0 < elapsed)
        elapsed = MAX_EXTRAPOLATION;
    scaleaddvectors(out, pos, &step, elapsed);
}

/* Put the three rows of the frame of a bone into the bone after its parent: the same frame,
 * turned by the joint angles. The place is the place of the parent plus the offset of the
 * joint (the position of the bone minus the joint, in the frame of the bone). */
static void place_bone_on_joint(Human *human, int k)
{
    Bone *bone = &human->bones[k];
    const Bone *parent = &human->bones[g_body[k].parent];
    vec3 offset;

    scaleaddvectors(&bone->pos, &parent->pos, &parent->rot[0], g_body[k].joint.x);
    scaleaddvectors(&bone->pos, &bone->pos, &parent->rot[1], g_body[k].joint.y);
    scaleaddvectors(&bone->pos, &bone->pos, &parent->rot[2], g_body[k].joint.z);
    bone->rot[0] = parent->rot[0];
    bone->rot[1] = parent->rot[1];
    bone->rot[2] = parent->rot[2];
    rotateorientation(bone->rot, &bone->rot[0], bone->joint_angles.x);
    rotateorientation(bone->rot, &bone->rot[1], bone->joint_angles.y);
    rotateorientation(bone->rot, &bone->rot[2], bone->joint_angles.z);
    subtractvectors(&offset, &g_body[k].pos, &g_body[k].joint);
    scaleaddvectors(&bone->pos, &bone->pos, &bone->rot[0], offset.x);
    scaleaddvectors(&bone->pos, &bone->pos, &bone->rot[1], offset.y);
    scaleaddvectors(&bone->pos, &bone->pos, &bone->rot[2], offset.z);
}

/* Smooth the places that the server sent. */
void __cdecl client_simulation(void)
{
    int i, k;

    for (i = 0; i < NUM_HUMANS; i++) {
        Human *human = &g_human[i];

        if (human->active == 0 || human->net_tick >= (int)cl_data_game_tick)
            continue;
        extrapolate(&human->bones[0].pos, human->net_tick, &human->net_pos, human->net_tick_prev,
                    &human->net_pos_prev);
        for (k = FIRST_BODY_BONE; k < NUM_BONES; k++) {
            if (human->vehicle_id == -1 || k < FIRST_LIMB_BONE)
                place_bone_on_joint(human, k);
        }
    }
    for (i = 0; i < NUM_OBJECTS; i++) {
        Object *object = &g_object[i];

        if (object->active == 0 || object->net_tick >= (int)cl_data_game_tick)
            continue;
        extrapolate(&object->pos, object->net_tick, &object->net_pos, object->net_tick_prev, &object->net_pos_prev);
    }
}

/* Put a human that sits in a vehicle on its seat. */
static void seat_human(Human *human)
{
    const Object *object = &g_object[human->vehicle_id];
    const vec3 *seat = &g_objecttype[object->type].seats[human->vehicle_seat];
    int k;

    for (k = FIRST_BODY_BONE; k < FIRST_LIMB_BONE; k++)
        subtractvectors(&human->bones[k].pos, &human->bones[k].pos, &human->bones[0].pos);
    scaleaddvectors(&human->pos, &object->pos, &object->rot[0], seat->x);
    scaleaddvectors(&human->pos, &human->pos, &object->rot[1], (float)(seat->y + 0.25));
    scaleaddvectors(&human->pos, &human->pos, &object->rot[2], seat->z);
    if (0 < human->exit_timer)
        scaleaddvectors(&human->pos, &human->pos, &object->rot[0],
                        (float)(((float)human->exit_timer * seat->x) / 30.0));
    human->bones[0].pos = human->pos;
    human->bones[0].rot[0] = object->rot[0];
    human->bones[0].rot[1] = object->rot[1];
    human->bones[0].rot[2] = object->rot[2];
    for (k = FIRST_BODY_BONE; k < FIRST_LIMB_BONE; k++)
        addvectors(&human->bones[k].pos, &human->bones[k].pos, &human->bones[0].pos);
    for (k = FIRST_LIMB_BONE; k < NUM_BONES; k++) {
        Bone *bone = &human->bones[k];
        const Bone *parent = &human->bones[g_body[k].parent];

        bone->rot[0] = parent->rot[0];
        bone->rot[1] = parent->rot[1];
        bone->rot[2] = parent->rot[2];
        if (k == BONE_TURNED_A || k == BONE_TURNED_B)
            rotateorientation(bone->rot, &bone->rot[0], QUARTER_TURN);
        scaleaddvectors(&bone->pos, &parent->pos, &bone->rot[0], g_body[k].pos.x);
        scaleaddvectors(&bone->pos, &bone->pos, &bone->rot[1], g_body[k].pos.y);
        scaleaddvectors(&bone->pos, &bone->pos, &bone->rot[2], g_body[k].pos.z);
    }
}

/* Place the humans that the server put in a vehicle. A human on foot takes the place of
 * its first bone. */
void __cdecl humansimulationclient(void)
{
    int i;

    for (i = 0; i < NUM_HUMANS; i++) {
        Human *human = &g_human[i];

        if (human->active == 0)
            continue;
        if (human->vehicle_id == -1) {
            human->pos = human->bones[0].pos;
            continue;
        }
        if (human->player_id != -1) {
            uint32_t buttons = g_player[human->player_id].input_buttons;

            if ((buttons & BUTTON_EXIT_VEHICLE) == 0)
                human->exit_timer = 0;
            if ((buttons & BUTTON_EXIT_VEHICLE) != 0 && human->exit_timer < EXIT_TIMER_MAX)
                human->exit_timer++;
        }
        seat_human(human);
    }
}

/* Place an item on the item that holds it, then on the human that holds it. */
void __cdecl itemsimulationclient(void)
{
    int i;

    for (i = 0; i < NUM_ITEMS; i++) {
        Item *item = &g_item[i];

        if (item->active == 0)
            continue;
        if (0 < item->countdown)
            item->countdown--;
        if (item->parent_item != -1) {
            const Item *holder = &g_item[item->parent_item];

            item->pos = holder->pos;
            item->rot[0] = holder->rot[0];
            item->rot[1] = holder->rot[1];
            item->rot[2] = holder->rot[2];
            if (holder->type == ITEM_TYPE_HOLDER_A || holder->type == ITEM_TYPE_HOLDER_B) {
                if (item->type == ITEM_TYPE_HELD_A)
                    scaleaddvectors(&item->pos, &item->pos, &item->rot[0],
                                    (float)((float)(item->parent_slot - SLOT_BASE) * 0.1875 + 0.09375));
                if (item->type == ITEM_TYPE_HELD_B)
                    scaleaddvectors(&item->pos, &item->pos, &item->rot[0],
                                    (float)((float)(item->parent_slot - SLOT_BASE) * 0.1875 * 0.75));
            } else {
                scaleaddvectors(&item->pos, &item->pos, &item->rot[1], -0.0625f);
            }
        }
        if (item->parent_human != -1 && 1 < item->parent_slot) {
            const Bone *hand = &g_human[item->parent_human].bones[FIRST_BODY_BONE];

            item->rot[0] = hand->rot[0];
            negatevector(&item->rot[1], &hand->rot[2]);
            item->rot[2] = hand->rot[1];
            scaleaddvectors(&item->pos, &hand->pos, &item->rot[1], -0.25f);
        }
    }
}
