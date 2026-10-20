/* The pose of a human: the angles that the muscles of the legs hold, and the contacts between
 * the bones of two humans. */
#include "humans.h"

#include <stdint.h>

#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define g_xaxis (*SR_AT(vec3, 0x60cd4)) /* (1 0 0) */
#define g_body SR_AT(Body, 0xf270280)   /* the bone tree: the parent and the offset of each bone */

#define NUM_HUMANS 0x200
#define NUM_BONES 15
#define MAX_KNEE 0.785398185f /* pi / 4 */

/* The muscle of bone b holds a turn of angle about the x axis of its parent. */
static void hold_angle(Bone *b, float angle)
{
    resetorientation(b->target_rot);
    rotateorientation(b->target_rot, &g_xaxis, angle);
}

/* Sets the angles that the muscles of the legs (bones 9 to 14) hold. A dead human keeps the
 * angle of each leg bone, in -pi/4 .. pi/4 (the knees 10 and 13 do not bend back). A live human
 * swings the legs with the walk phase and bends the knees with the stance. */
void human_calculatelegangles(int human)
{
    Human *hu = &sr_humans[human];
    float bend = (float)x87_acos(hu->stance);
    float angle;
    int k;

    if (hu->health < 1) {
        for (k = 9; k < NUM_BONES; k++) {
            const Bone *p = &hu->bones[g_body[k].parent];
            Bone *b = &hu->bones[k];

            angle = (float)x87_atan2(
                (float)(p->rot[1].z * b->rot[2].z + (p->rot[1].x * b->rot[2].x + p->rot[1].y * b->rot[2].y)),
                (float)(p->rot[2].z * b->rot[2].z + (p->rot[2].x * b->rot[2].x + p->rot[2].y * b->rot[2].y)));
            if (angle < -MAX_KNEE)
                angle = -MAX_KNEE;
            if (MAX_KNEE < angle)
                angle = MAX_KNEE;
            if ((k == 10 || k == 13) && angle < 0.0)
                angle = 0.0f;
            hold_angle(b, angle);
        }
        return;
    }
    hold_angle(&hu->bones[9], (float)(x87_sin(hu->walk_phase) * 3.14159265359 * 0.125 - bend));
    hold_angle(&hu->bones[10], (float)(bend + bend));
    hold_angle(&hu->bones[11], (float)(x87_sin(hu->walk_phase) * -3.14159265359 * 0.125 - bend));
    hold_angle(&hu->bones[12], (float)(x87_sin(hu->walk_phase) * -3.14159265359 * 0.125 - bend));
    hold_angle(&hu->bones[13], (float)(bend + bend));
    hold_angle(&hu->bones[14], (float)(x87_sin(hu->walk_phase) * 3.14159265359 * 0.125 - bend));
}

/* Bones 10, 11, 13 and 14 (the lower legs and the feet) do not touch other humans. */
static int touches(int bone)
{
    return bone != 11 && bone != 14 && bone != 10 && bone != 13;
}

/* Adds a contact bond (type 13) for each pair of bones of human `human` and of a later human on
 * foot that are nearer than 0.375. The bond pushes the bones apart. */
void human_collisionhuman(int human)
{
    const Human *hu = &sr_humans[human];
    const vec3 *c = &hu->bones[0].pos;
    vec3 d;
    float len, inv;
    int o, i, j;

    for (o = human + 1; o < NUM_HUMANS; o++) {
        const Human *other = &sr_humans[o];
        const vec3 *p = &other->bones[0].pos;

        if (other->active == 0 || other->vehicle_id != -1)
            continue;
        if (!(c->x - 2.25 < p->x && p->x < c->x + 2.25 && c->z - 2.25 < p->z && p->z < c->z + 2.25 &&
              c->y - 2.25 < p->y && p->y < c->y + 2.25))
            continue;
        for (i = 0; i < NUM_BONES; i++) {
            if (!touches(i))
                continue;
            for (j = 0; j < NUM_BONES; j++) {
                Bond *b;

                if (!touches(j))
                    continue;
                subtractvectors(&d, &other->bones[j].pos, &hu->bones[i].pos);
                len = vectorlength(&d);
                if (!(len < 0.375))
                    continue;
                inv = (float)(1.0 / len);
                /* Only these fields change: the rest of the record keeps its old values. */
                b = &sr_bonds[sr_numofbonds];
                b->type = 13;
                b->owner = o;
                b->bone_a = j;
                sr_numofbonds++;
                b->target = human;
                b->bone_b = i;
                b->mode = 3;
                d.x = (float)(inv * d.x);
                d.y = (float)(d.y * inv);
                d.z = (float)(inv * d.z);
                b->rest_length = (float)(0.375 - len);
                b->stiffness = 0.0625f;
                b->damping = 0.25f;
                b->friction = 0.2f;
                b->normal = d;
            }
        }
    }
}
