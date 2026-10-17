/* The bullets: the bullet types, a new bullet and the end of its life.
 *
 * The bullets are a packed list of up to 16384 (sr_bullets, sr_numofbullets): a bullet at
 * the end of its life takes the last bullet into its place.
 *
 * The float math runs on the x87 (this file compiles with /arch:IA32). Keep the order of the
 * terms and each (float): a change gives a different last bit.
 */
#include "bullets.h"

#include <math.h>
#include <string.h>

#include "sr_runtime.h"
#include "vectors.h"

#define MAX_BULLETS 0x4000

/* Game globals. */
#define g_bullettypes SR_AT(float, 0xf437320) /* 4 bullet types, 6 floats each */
#define g_xaxis (*SR_AT(vec3, 0x60cd4))       /* (1 0 0) */
#define g_yaxis (*SR_AT(vec3, 0x60ce0))       /* (0 1 0) */

/* The 4 bullet types, 6 floats each, as raw bits (the meaning of most values is not known
 * yet). The first value goes to Bullet.unk_000c. */
static const uint32_t bullettypes[4][6] = {
    { 0x3c23d70a, 0x3b956f22, 0x3bf9b131, 0x3d1fbe77, 0, 0x3fc00000 },
    { 0x3bcb295f, 0x3b80522c, 0x3bb630a9, 0x3d3851ec, 0, 0x3fc00000 },
    { 0x3c03126f, 0x3c0249c2, 0x3c1374bc, 0x3c9ba5e3, 0, 0x3fc00000 },
    { 0x3c83126f, 0x3bbf1af7, 0x3c3295ea, 0x3d03126f, 0, 0x3fc00000 },
};

void setupbullettypes(void)
{
    memcpy(g_bullettypes, bullettypes, sizeof bullettypes);
}

/* A new bullet of a type at pos with the velocity vel. side, up and back are a frame: back is
 * the opposite of the velocity. Returns the index of the bullet, or -1 when the list is full. */
int createbullet(int type, const vec3 *pos, const vec3 *vel, int owner)
{
    int n = sr_numofbullets;
    Bullet *b;
    if (n >= MAX_BULLETS)
        return -1;
    b = &sr_bullets[n];
    memset(b, 0, sizeof *b);
    b->type = type;
    b->owner = owner;
    b->ttl = 600;
    memcpy(&b->pos, pos, sizeof b->pos);
    memcpy(&b->pos_prev, pos, sizeof b->pos_prev);
    b->back.x = -vel->x;
    b->back.y = -vel->y;
    b->back.z = -vel->z;
    normalizevector(&b->back, &b->back);
    /* side = back x an axis: the x axis when back is near the y axis, else the y axis. */
    if (0.7070000171661377 <= fabs(b->back.y)) {
        b->side.x = (float)(g_xaxis.y * b->back.z - g_xaxis.z * b->back.y);
        b->side.y = (float)(b->back.x * g_xaxis.z - g_xaxis.x * b->back.z);
        b->side.z = (float)(b->back.y * g_xaxis.x - g_xaxis.y * b->back.x);
    } else {
        b->side.x = (float)(g_yaxis.y * b->back.z - g_yaxis.z * b->back.y);
        b->side.y = (float)(g_yaxis.z * b->back.x - g_yaxis.x * b->back.z);
        b->side.z = (float)(g_yaxis.x * b->back.y - b->back.x * g_yaxis.y);
    }
    normalizevector(&b->side, &b->side);
    b->up.x = (float)(b->side.z * b->back.y - b->side.y * b->back.z);
    b->up.y = (float)(b->side.x * b->back.z - b->side.z * b->back.x);
    b->up.z = (float)(b->back.x * b->side.y - b->back.y * b->side.x);
    normalizevector(&b->up, &b->up);
    b->vel = *vel;
    sr_stats.numofbullets++;
    b->unk_000c = g_bullettypes[type * 6];
    b->gravity = 0.0027222223579883575f;
    sr_numofbullets = n + 1;
    return n;
}

/* Count down the life of each bullet. A bullet at the end of its life takes the last bullet
 * into its place. The loop then looks at the moved bullet again, so it loses one more tick,
 * as in the 024d. */
void bullettimetolive(void)
{
    Bullet *bullets = sr_bullets;
    Bullet *b = bullets;
    int n = sr_numofbullets; /* a local count stays in a register */
    int i = 0;
    while (i < n) {
        if (b->ttl < 0x10000)
            b->ttl--;
        if (b->ttl >= 1) {
            i++;
            b++;
            continue;
        }
        n--;
        if (i == n)
            break;
        *b = bullets[n];
    }
    sr_numofbullets = n;
}
