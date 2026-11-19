/* The list of particles: add one, remove one and count down the life of every particle. */
#include "render/sprite_pool.h"

#include <math.h>
#include <string.h>

#include <stdint.h>

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

/* The corners of the face of a block, in the world: the face has 4 corners that index the corner
 * list of the block type. Each corner is scaled by the block size and moved to the block. */
static void block_face_corners(int area, int x, int y, int z, const AreaBlock *block, int face, vec3 *corners)
{
    const vec3 *verts = (const vec3 *)(uintptr_t)block->verts;
    const int *faces = (const int *)(uintptr_t)block->faces;
    float size = sr_data_area[area].block_size;
    int c;

    for (c = 0; c < 4; c++) {
        corners[c] = verts[faces[c + face * 4]];
        scalevector(&corners[c], &corners[c], size);
        corners[c].x = (float)((float)x * size + corners[c].x);
        corners[c].y = (float)((float)y * size + corners[c].y);
        corners[c].z = (float)((float)z * size + corners[c].z);
    }
}

/* Break a window face of a block into 4 triangle pieces, one for each side of the face. A piece
 * has two corners of the face and the point apex, starts at rest, falls with the speed of impulse
 * and lives 60000 ticks. A block word that is not a special block, or a block with no mesh, gives
 * nothing. */
void __cdecl createwindowsprites_block(int area, int x, int y, int z, uint32_t word, int face, const vec3 *apex,
                                       const float *impulse)
{
    const AreaBlock *block;
    vec3 corners[4];
    int k;

    if ((word & 0xe0000000u) != 0x80000000u)
        return;
    block = &sr_data_area_block[word & 0xffff];
    if (block->num_verts == 0 || block->num_faces == 0)
        return;
    block_face_corners(area, x, y, z, block, face, corners);
    for (k = 0; k < 4; k++) {
        ClientSprite *piece;

        if (cl_data_sprite_count >= SPRITE_LIMIT)
            continue;
        piece = &cl_data_sprites[cl_data_sprite_count];
        piece->kind = SPRITE_TRIANGLE;
        piece->rot[0] = corners[k];
        piece->rot[1] = corners[(k + 1) & 3];
        piece->rot[2] = *apex;
        if (k == 2)
            piece->rot[2].y = (float)(piece->rot[2].y - 0.0625);
        piece->corner_vel[0].x = piece->corner_vel[0].y = piece->corner_vel[0].z = 0.0f;
        piece->corner_vel[1].x = piece->corner_vel[1].y = piece->corner_vel[1].z = 0.0f;
        scalevector(&piece->corner_vel[2], (const vec3 *)impulse, 0.015625f);
        piece->life = 60000;
        piece->size = 0.25f;
        piece->gravity = 0.0027222223579883575f;
        piece->drag = 0.9975000023841858f;
        piece->spin.x = (float)vectordistance(&piece->rot[1], &piece->rot[0]);
        piece->spin.y = (float)vectordistance(&piece->rot[2], &piece->rot[1]);
        piece->spin.z = (float)vectordistance(&piece->rot[0], &piece->rot[2]);
        cl_data_sprite_count++;
    }
}
