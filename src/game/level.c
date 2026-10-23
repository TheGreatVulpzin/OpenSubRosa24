/* Level, landscape and track construction. */
#include "level.h"

#include <stdint.h>

#include "sr_math.h"
#include "sr_runtime.h"
#include "sr_types.h"
#include "vectors.h"

#define g_track SR_AT(TrackPiece, 0x2597efc0)
#define g_numoftracks (*SR_AT(int, 0x90dc520))
#define g_land_origin (*SR_AT(vec3, 0x9c1d8c0))
#define g_land_height SR_AT(float, 0x9c1d8cc)
#define g_game_world (*SR_AT(GameWorld, 0x2561d740))

static float track_span(const vec3 *a, const vec3 *b, int z_first)
{
    float x = (float)(a->x - b->x);
    float z = (float)(a->z - b->z);
    float square;
    if (z_first)
        square = (float)(z * z + (x * x + 0.0));
    else
        square = (float)(x * x + 0.0 + z * z);
    return (float)x87_sqrt(square);
}

static void track_triangle(TrackPiece *track, int a, int b, int c)
{
    int n = track->num_tris;
    track->tris[n][0] = a;
    track->tris[n][1] = b;
    track->tris[n][2] = c;
    track->num_tris++;
}

void createtrack(int type, const float *p, float drop_end, float rise_start)
{
    const vec3 *points = (const vec3 *)p;
    TrackPiece *track = &g_track[g_numoftracks];
    vec3 s, a1, a2, b1, b2, v;
    float c, t;
    int i, j, axis, side;

    track->type = type;
    track->num_verts = 0;
    track->num_tris = 0;
    for (axis = 0; axis < 3; axis++) {
        float *pos = (float *)&track->pos;
        pos[axis] = (float)(p[3 + axis] + p[axis]);
    }
    for (axis = 0; axis < 3; axis++) {
        float *pos = (float *)&track->pos;
        pos[axis] = (float)(p[6 + axis] + pos[axis]);
    }
    for (axis = 0; axis < 3; axis++) {
        float *pos = (float *)&track->pos;
        pos[axis] = (float)(pos[axis] + p[9 + axis]);
    }
    for (axis = 0; axis < 3; axis++) {
        float *pos = (float *)&track->pos;
        pos[axis] = (float)(pos[axis] * 0.25);
    }
    if (track->pos.x >= 2048.0f) {
        float y = track->pos.y;
        for (i = -3; i <= 3; i++) {
            float z = (float)(track->pos.z + (float)i * 8.0);
            uint32_t iz = (uint32_t)(int64_t)(((double)z - g_land_origin.z) * 0.125);
            for (j = -3; j <= 3; j++) {
                float x = (float)((float)j * 8.0 + track->pos.x);
                uint32_t ix = (uint32_t)(int64_t)(((double)x - g_land_origin.x) * 0.125);
                if (ix < 0x800 && iz < 0x800)
                    g_land_height[iz * 0x801 + ix] = (float)(y - 0.25);
            }
        }
    }
    s.x = (float)(points[0].z - points[1].z);
    s.y = 0.0f;
    s.z = -(float)(points[0].x - points[1].x);
    normalizevector(&s, &s);
    c = (float)(track_span(&points[3], &points[0], 1) / 3.0);
    a1.x = (float)(c * s.x + points[0].x);
    a1.y = (float)(rise_start / 3.0 + (float)(c * s.y + points[0].y));
    a1.z = (float)(c * s.z + points[0].z);
    c = (float)(track_span(&points[2], &points[1], 0) / 3.0);
    b1.x = (float)(c * s.x + points[1].x);
    b1.y = (float)((float)(c * s.y + points[1].y) + rise_start / 3.0);
    b1.z = (float)(c * s.z + points[1].z);
    s.x = (float)(points[2].z - points[3].z);
    s.y = 0.0f;
    s.z = -(float)(points[2].x - points[3].x);
    normalizevector(&s, &s);
    c = (float)(track_span(&points[1], &points[2], 0) / 3.0);
    b2.x = (float)(c * s.x + points[2].x);
    b2.y = (float)((float)(c * s.y + points[2].y) - drop_end / 3.0);
    b2.z = (float)(c * s.z + points[2].z);
    c = (float)(track_span(&points[0], &points[3], 0) / 3.0);
    a2.x = (float)(c * s.x + points[3].x);
    a2.y = (float)((float)(c * s.y + points[3].y) - drop_end / 3.0);
    a2.z = (float)(c * s.z + points[3].z);

    t = 0.0f;
    for (i = 0; i < 8; i++) {
        if (i == 0) {
            v = points[0];
        } else if (i == 7) {
            v = points[3];
        } else {
            double u = (float)(1.0 - t);
            double q = t;
            for (axis = 0; axis < 3; axis++)
                ((float *)&v)[axis] = (float)(q * p[9 + axis] * q * q
                    + (((float *)&a2)[axis] * 3.0 * u * q * q
                    + (u * p[axis] * u * u
                    + q * (u * (u * (((float *)&a1)[axis] * 3.0))))));
        }
        track->verts[track->num_verts++] = v;
        t = (float)(t + 0.1428571492433548);
    }
    t = 0.0f;
    for (i = 0; i < 8; i++) {
        if (i == 0) {
            v = points[1];
        } else if (i == 7) {
            v = points[2];
        } else {
            double u = (float)(1.0 - t);
            double q = t;
            v.x = (float)(points[2].x * q * q * q + (b2.x * 3.0 * u * q * q
                + (points[1].x * u * u * u + q * (u * (u * (b1.x * 3.0))))));
            v.y = (float)(points[2].y * q * q * q + (b2.y * 3.0 * u * q * q
                + (b1.y * 3.0 * u * u * q + u * (u * (u * points[1].y)))));
            v.z = (float)(points[2].z * q * q * q + (b2.z * 3.0 * u * q * q
                + (b1.z * 3.0 * u * u * q + u * (u * (u * points[1].z)))));
        }
        track->verts[track->num_verts++] = v;
        t = (float)(t + 0.1428571492433548);
    }
    for (side = 0; side < 2; side++) {
        for (i = 0; i < 8; i++) {
            const vec3 *a = &track->verts[i + side * 8];
            const vec3 *b = &track->verts[i + (1 - side) * 8];
            if (type == 0) {
                v.x = a->x;
                v.y = (float)(a->y + 0.5);
                v.z = a->z;
            } else if (type == 1) {
                float dx = (float)(a->x - b->x), dz = (float)(a->z - b->z);
                float length = (float)x87_sqrt((float)(dx * dx + 0.0 + dz * dz));
                float dy, dz2, inv;
                if (length == 0.0f) {
                    dx = dy = dz2 = 0.0f;
                } else {
                    inv = (float)(1.0 / length);
                    dx = (float)(inv * dx);
                    dy = (float)(inv * 0.0);
                    dz2 = (float)(inv * dz);
                }
                v.x = (float)(a->x + dx * 2.0);
                v.y = (float)((float)(dy * 2.0 + a->y) - 0.5);
                v.z = (float)(dz2 * 2.0 + a->z);
            }
            /* A track of another type repeats the last corner. */
            track->verts[track->num_verts++] = v;
        }
    }
    for (i = 9; i <= 15; i++) {
        track_triangle(track, i - 9, i - 1, i);
        track_triangle(track, i - 9, i, i - 8);
    }
    for (i = 1; i <= 7; i++) {
        track_triangle(track, i + 15, i - 1, i);
        track_triangle(track, i + 15, i, i + 16);
    }
    for (i = 8; i <= 14; i++) {
        track_triangle(track, i, i + 16, i + 17);
        track_triangle(track, i, i + 17, i + 1);
    }
    for (i = 0; i < 8; i++) {
        const vec3 *a = &track->verts[i], *b = &track->verts[i + 8];
        float dx = (float)(a->x - b->x), dy = (float)(a->y - b->y);
        float dz = (float)(a->z - b->z);
        float length, inv;
        track->center[i].x = (float)((float)(a->x + b->x) * 0.5);
        track->center[i].y = (float)((float)(a->y + b->y) * 0.5);
        track->center[i].z = (float)((float)(a->z + b->z) * 0.5);
        length = (float)x87_sqrt((float)(dy * dy + dx * dx + dz * dz));
        if (length == 0.0f) {
            track->across[i].x = track->across[i].y = track->across[i].z = 0.0f;
        } else {
            inv = (float)(1.0 / length);
            track->across[i].x = (float)(inv * dx);
            track->across[i].y = (float)(inv * dy);
            track->across[i].z = (float)(inv * dz);
        }
    }
    track->box_min.x = track->box_min.y = track->box_min.z = 16777216.0f;
    track->box_max.x = track->box_max.y = track->box_max.z = -16777216.0f;
    for (i = 0; i < track->num_verts; i++) {
        const float *vertex = (const float *)&track->verts[i];
        float *min = (float *)&track->box_min, *max = (float *)&track->box_max;
        for (axis = 0; axis < 3; axis++) {
            if (vertex[axis] < min[axis]) min[axis] = vertex[axis];
            if (max[axis] < vertex[axis]) max[axis] = vertex[axis];
        }
    }
    g_numoftracks++;
}

/* The mean angle keeps double precision. */
void buildsomeshit(const float *start)
{
    vec3 p = { start[0], start[1], start[2] };
    float a0 = -1.5707963705062866f, a1 = a0;
    int u, n;
    g_numoftracks = 0;
    for (u = -21, n = 145; n != 0; u++, n--) {
        float corners[12], s, c, c0, ns0, c1, ns1;
        double mean;
        vec3 q;
        a1 = a0;
        if ((uint32_t)(u + 13) <= 10) a1 = (float)(a1 + 0.39269908169875);
        if ((uint32_t)u <= 2) a1 = (float)(a1 - 0.39269908169875);
        if ((uint32_t)(u - 54) <= 2) a1 = (float)(a1 - 0.39269908169875);
        if ((uint32_t)(u - 59) <= 10) a1 = (float)(a1 + 0.39269908169875);
        mean = a1 * 0.5 + a0 * 0.5;
        s = (float)x87_sin(mean);
        c = (float)x87_cos(mean);
        q.x = (float)(p.x + s * 32.0);
        q.y = (float)(p.y + 0.0);
        q.z = (float)(c * 32.0 + p.z);
        c0 = (float)x87_cos(a0);
        ns0 = (float)-x87_sin(a0);
        corners[0] = (float)(p.x - c0 * 1.0);
        corners[1] = (float)(p.y + -0.0);
        corners[2] = (float)(p.z - ns0 * 1.0);
        corners[3] = (float)(p.x + c0);
        corners[4] = (float)(p.y + 0.0);
        corners[5] = (float)(ns0 + p.z);
        c1 = (float)x87_cos(a1);
        ns1 = (float)-x87_sin(a1);
        corners[6] = (float)(c1 + q.x);
        corners[7] = (float)(q.y + 0.0);
        corners[8] = (float)(ns1 + q.z);
        corners[9] = (float)(q.x - c1 * 1.0);
        corners[10] = (float)(q.y + -0.0);
        corners[11] = (float)(q.z - ns1 * 1.0);
        createtrack(0, corners, 0.0f, 0.0f);
        a0 = a1;
        p = q;
    }
    g_game_world.spawn.x = (float)(g_track[74].pos.x - 8.0);
    g_game_world.spawn.y = (float)(g_track[74].pos.y + 2.0);
    g_game_world.spawn.z = (float)(g_track[74].pos.z + 4.0);
    g_game_world.spawn_size_x = 16.0f;
    g_game_world.spawn_size_y = 4.0f;
    g_game_world.spawn_size_z = 16.0f;
}
