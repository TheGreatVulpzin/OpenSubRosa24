/* The list of particles: add one, remove one and count down the life of every particle. */
#include "render/sprite_pool.h"

#include <math.h>
#include <string.h>

#include "client.h"
#include "render/sprites.h"
#include "sr_runtime.h"
#include "vectors.h"

enum { SPRITE_LIMIT = 0x4000, LIFE_FOREVER = 0xffff, SPARK_LIFE = 0x3c, TRACE_LIFE = 3 };

/* The edge of a mark is 1/64 above its surface. */
#define MARK_LIFT 0.015625
/* A mark turns its first axis from the x axis, unless its normal is mostly vertical. */
#define MARK_VERTICAL 0.7070000171661377
#define SPRITE_GRAVITY 0.0027222223579883575f

/* Add a particle and return its index, or -1 when the list is full. A mark (kind 0) needs a
 * normal in vel: the particle gets a frame around it and moves 1/64 off the surface. */
int __cdecl createsprite(int kind, const vec3 *pos, const vec3 *vel, float size)
{
    static const vec3 unit_x = {1.0f, 0.0f, 0.0f};
    static const vec3 unit_y = {0.0f, 1.0f, 0.0f};
    ClientSprite *sprite;
    int index;

    if (cl_data_sprite_count >= SPRITE_LIMIT)
        return -1;
    sprite = &cl_data_sprites[cl_data_sprite_count];
    sprite->kind = kind;
    sprite->pos = *pos;
    if (vel == NULL)
        sprite->vel.x = sprite->vel.y = sprite->vel.z = 0.0f;
    else
        sprite->vel = *vel;
    if (kind == SPRITE_MARK) {
        sprite->life = LIFE_FOREVER;
        sprite->size = size;
        sprite->size2 = size;
        sprite->gravity = 0.0f;
        sprite->drag = 0.0f;
        normalizevector(&sprite->rot[2], vel);
        if (MARK_VERTICAL <= fabs((double)sprite->rot[2].y))
            crossproduct(&sprite->rot[0], &sprite->rot[2], &unit_x);
        else
            crossproduct(&sprite->rot[0], &sprite->rot[2], &unit_y);
        normalizevector(&sprite->rot[0], &sprite->rot[0]);
        crossproduct(&sprite->rot[1], &sprite->rot[0], &sprite->rot[2]);
        normalizevector(&sprite->rot[1], &sprite->rot[1]);
        scaleaddvectors(&sprite->pos, &sprite->pos, &sprite->rot[2], MARK_LIFT);
    }
    if (kind == SPRITE_STAIN) {
        sprite->life = LIFE_FOREVER;
        sprite->size = (float)(size * 0.25);
        sprite->size2 = (float)(size * 0.5);
        sprite->gravity = SPRITE_GRAVITY;
        sprite->drag = 0.9975000023841858f;
    }
    if (kind == SPRITE_SPARK) {
        sprite->life = SPARK_LIFE;
        sprite->size = size;
        sprite->size2 = size;
        sprite->gravity = SPRITE_GRAVITY;
        sprite->drag = 0.9750000238418579f;
    }
    if (kind == SPRITE_TRACE) {
        sprite->life = TRACE_LIFE;
        sprite->size = size;
        sprite->size2 = size;
        sprite->gravity = 0.0f;
        sprite->drag = 0.875f;
    }
    index = cl_data_sprite_count;
    cl_data_sprite_count++;
    return index;
}

/* Remove a particle. The last particle takes its place. An index outside the list changes nothing. */
void __cdecl deletesprite(int index)
{
    if (index > -1 && index < cl_data_sprite_count) {
        cl_data_sprite_count--;
        if (index != cl_data_sprite_count)
            memcpy(&cl_data_sprites[index], &cl_data_sprites[cl_data_sprite_count], sizeof(ClientSprite));
    }
}

/* Count the life of every particle down by one tick. A life of 0xffff does not change. A particle
 * with no life left is removed. */
void __cdecl spritetimetolive(void)
{
    int index = 0;

    while (index < cl_data_sprite_count) {
        if (cl_data_sprites[index].life < LIFE_FOREVER)
            cl_data_sprites[index].life--;
        if (cl_data_sprites[index].life < 1)
            deletesprite(index);
        else
            index++;
    }
}
