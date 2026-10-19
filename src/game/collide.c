/* The collision tests against the level: the landscape, the 4 areas of blocks and the city
 * objects. Each test keeps the nearest hit in the global Intersect record and returns 1 when
 * there is a hit. */
#include "collide.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "intersect.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define g_intersect (*SR_AT(Intersect, 0x103484a0))
#define g_areas SR_AT(Area, SR_RVA_AREA)
#define g_cityobjects SR_AT(CityObjectRec, 0x15e24020)
#define NUM_AREAS 4
#define NUM_CITYOBJECTS 1024

/* Game functions with no clean version yet: the calls go to the lifted C. */
int __cdecl lf_1c8f0(float *pos, float *rot, float radius);
int __cdecl lf_40aa0(float *hit, float *normal, float *dist, uint32_t area, float *pos, float radius);
int __cdecl lf_2c0c0(float *hit, float *normal, float *t, uint32_t area, float *from, float *to);
int __cdecl lf_41550(float *hit, float *normal, float *dist, uint32_t area, float *pos, float *rot, float radius);
int __cdecl lf_146a0(float *hit, float *normal, float *t, int object, float *from, float *to);
#define circleintersectlandscape lf_1c8f0
#define sphereintersectarea lf_40aa0
#define lineintersectarea lf_2c0c0
#define circleintersectarea lf_41550
#define lineintersectcityobject lf_146a0

/* Copies a hit point and its normal into the Intersect record, bit for bit. */
static void keep_hit(const vec3 *hit, const vec3 *normal)
{
    memcpy(g_intersect.pos, hit, sizeof g_intersect.pos);
    memcpy(g_intersect.normal, normal, sizeof g_intersect.normal);
}

/* A sphere against the level: distance gets the distance of the nearest hit. */
int sphereintersectlevel(float *pos, float radius)
{
    vec3 hit, normal;
    float dist;
    uint32_t a;

    g_intersect.fraction = 1.0f;
    g_intersect.distance = 65536.0f;
    sphereintersectlandscape(pos, radius);
    for (a = 0; a < NUM_AREAS; a++) {
        if (g_areas[a].active != 0 && sphereintersectarea(&hit.x, &normal.x, &dist, a, pos, radius) != 0 &&
            dist < g_intersect.distance) {
            keep_hit(&hit, &normal);
            g_intersect.distance = dist;
        }
    }
    return 65536.0 <= g_intersect.distance ? 0 : 1;
}

/* The line from -> to against the level: fraction gets the part of the line before the nearest
 * hit. with_objects: also the city objects (their velocity goes into surface_vel). */
int lineintersectlevel(float *from, float *to, int with_objects)
{
    vec3 hit, normal;
    float t;
    uint32_t a;
    int o;

    g_intersect.fraction = 1.0f;
    lineintersectlandscape(from, to);
    for (a = 0; a < NUM_AREAS; a++) {
        if (g_areas[a].active != 0 && lineintersectarea(&hit.x, &normal.x, &t, a, from, to) != 0 &&
            t < g_intersect.fraction) {
            keep_hit(&hit, &normal);
            g_intersect.surface_vel.x = 0.0f;
            g_intersect.surface_vel.y = 0.0f;
            g_intersect.surface_vel.z = 0.0f;
            g_intersect.fraction = t;
        }
    }
    if (with_objects != 0) {
        for (o = 0; o < NUM_CITYOBJECTS; o++) {
            if (g_cityobjects[o].active != 0 && lineintersectcityobject(&hit.x, &normal.x, &t, o, from, to) != 0 &&
                t < g_intersect.fraction) {
                g_intersect.block[0] = -1;
                keep_hit(&hit, &normal);
                g_intersect.surface_vel = g_cityobjects[o].vel;
                g_intersect.fraction = t;
            }
        }
    }
    return 1.0 <= g_intersect.fraction ? 0 : 1;
}

/* The line from -> to against the areas and the city objects (not the landscape). index gets
 * the city object of the hit, or -1. */
int lineintersectall(float *from, float *to)
{
    vec3 hit, normal;
    float t;
    uint32_t a;
    int o;

    g_intersect.fraction = 1.0f;
    g_intersect.index = -1;
    for (a = 0; a < NUM_AREAS; a++) {
        if (g_areas[a].active != 0 && lineintersectarea(&hit.x, &normal.x, &t, a, from, to) != 0 &&
            t < g_intersect.fraction) {
            keep_hit(&hit, &normal);
            g_intersect.fraction = t;
        }
    }
    for (o = 0; o < NUM_CITYOBJECTS; o++) {
        if (g_cityobjects[o].active != 0 && lineintersectcityobject(&hit.x, &normal.x, &t, o, from, to) != 0 &&
            t < g_intersect.fraction) {
            g_intersect.block[0] = -1;
            keep_hit(&hit, &normal);
            g_intersect.fraction = t;
            g_intersect.surface_vel = g_cityobjects[o].vel;
            g_intersect.index = o;
        }
    }
    return 1.0 <= g_intersect.fraction ? 0 : 1;
}

/* A wheel (a circle at pos, turned by rot) against the level: distance gets the distance of the
 * nearest hit, and material its material. */
int circleintersectlevel(float *pos, float *rot, float radius)
{
    vec3 hit, normal;
    float dist;
    uint32_t a;

    g_intersect.fraction = 1.0f;
    g_intersect.distance = 65536.0f;
    circleintersectlandscape(pos, rot, radius);
    for (a = 0; a < NUM_AREAS; a++) {
        if (g_areas[a].active != 0 && circleintersectarea(&hit.x, &normal.x, &dist, a, pos, rot, radius) != 0 &&
            dist < g_intersect.distance) {
            g_intersect.material = g_intersect.area_material;
            keep_hit(&hit, &normal);
            g_intersect.fraction = 0.0f;
            g_intersect.distance = dist;
        }
    }
    return 65536.0 <= g_intersect.distance ? 0 : 1;
}

/* Keeps a hit on face f of object ob, with the triangle of the corners i, j, k: the face, the
 * 3 particles of the corners and their weights at the hit point. */
static void keep_face_hit(const Object *ob, int f, const vec3 *hit, const vec3 *normal, int i, int j, int k,
                          const vec3 *a, const vec3 *b, const vec3 *c)
{
    memcpy(g_intersect.pos, hit, sizeof g_intersect.pos);
    memcpy(g_intersect.normal, normal, sizeof g_intersect.normal);
    g_intersect.face = f;
    g_intersect.tri_particles[0] = ob->particles[ob->verts[i].particle];
    g_intersect.tri_particles[1] = ob->particles[ob->verts[j].particle];
    g_intersect.tri_particles[2] = ob->particles[ob->verts[k].particle];
    calculatebarycentric((vec3 *)g_intersect.tri_weights, (const vec3 *)g_intersect.pos, a, b, c);
}

/* A sphere against the faces of object obj (a quad is two triangles). distance gets the distance
 * of the nearest hit. An object of type 12 skips the faces with no health left. */
int sphereintersectobject(int obj, float *pos, float radius)
{
    const Object *ob = &sr_objects[obj];
    const vec3 *p = (const vec3 *)pos;
    const ObjectFace *face;
    vec3 a, b, c, d, normal, hit;
    float dist;
    int f, n, type;

    if (!(ob->box_min.x <= radius + p->x && ob->box_min.y <= p->y + radius && ob->box_min.z <= p->z + radius &&
          p->x - radius <= ob->box_max.x && p->y - radius <= ob->box_max.y && p->z - radius <= ob->box_max.z))
        return 0;
    n = ob->num_faces;
    g_intersect.distance = 65536.0f;
    if (n <= 0)
        return 0;
    type = ob->type;
    for (f = 0; f < n; f++) {
        face = &ob->faces[f];
        if (type == 12 && !(0 < face->health))
            continue;
        a = ob->verts[face->verts[0]].pos;
        b = ob->verts[face->verts[1]].pos;
        c = ob->verts[face->verts[2]].pos;
        d = ob->verts[face->verts[3]].pos;
        generatetrinormal(&normal, &a, &b, &c);
        if (sphereintersecttrianglefull(&hit, &normal, &dist, p, radius, &a, &b, &c) != 0 && dist < g_intersect.distance) {
            g_intersect.distance = dist;
            keep_face_hit(ob, f, &hit, &normal, face->verts[0], face->verts[1], face->verts[2], &a, &b, &c);
        }
        if (face->corners == 4) {
            generatetrinormal(&normal, &a, &c, &d);
            if (sphereintersecttrianglefull(&hit, &normal, &dist, p, radius, &a, &c, &d) != 0 &&
                dist < g_intersect.distance) {
                g_intersect.distance = dist;
                keep_face_hit(ob, f, &hit, &normal, face->verts[0], face->verts[2], face->verts[3], &a, &c, &d);
            }
        }
    }
    return g_intersect.distance < 65536.0 ? 1 : 0;
}

/* The line from -> to against the faces of object obj, from both sides (a face with no health
 * left does not count), and with_wheels: against the wheels too. fraction gets the part of the
 * line before the nearest hit, and surface_vel the velocity of the hit point of a face. */
int lineintersectobject(int obj, float *from, float *to, int with_wheels)
{
    const Object *ob = &sr_objects[obj];
    const vec3 *s = (const vec3 *)from, *e = (const vec3 *)to;
    const ObjectFace *face;
    const Particle *q0, *q1, *q2;
    vec3 a, b, c, normal, hit;
    float t;
    int f, n, w;

    if (s->x < ob->box_min.x && e->x < ob->box_min.x)
        return 0;
    if (!((ob->box_min.y <= s->y || ob->box_min.y <= e->y) && (ob->box_min.z <= s->z || ob->box_min.z <= e->z) &&
          (s->x <= ob->box_max.x || e->x <= ob->box_max.x) && (s->y <= ob->box_max.y || e->y <= ob->box_max.y) &&
          (s->z <= ob->box_max.z || e->z <= ob->box_max.z)))
        return 0;
    n = ob->num_faces;
    g_intersect.fraction = 1.0f;
    for (f = 0; f < n; f++) {
        face = &ob->faces[f];
        if (!(0 < face->health))
            continue;
        a = ob->verts[face->verts[0]].pos;
        b = ob->verts[face->verts[1]].pos;
        c = ob->verts[face->verts[2]].pos;
        generatetrinormal(&normal, &a, &b, &c);
        if (lineintersecttriangletwosided(&hit, &normal, &t, s, e, &a, &b, &c, 1) != 0 && t < g_intersect.fraction) {
            g_intersect.fraction = t;
            keep_face_hit(ob, f, &hit, &normal, face->verts[0], face->verts[1], face->verts[2], &a, &b, &c);
            q0 = &sr_particles[g_intersect.tri_particles[0]];
            q1 = &sr_particles[g_intersect.tri_particles[1]];
            q2 = &sr_particles[g_intersect.tri_particles[2]];
            g_intersect.surface_vel.x = (float)((float)(g_intersect.tri_weights[1] * q1->vel.x +
                                                        g_intersect.tri_weights[0] * q0->vel.x) +
                                                g_intersect.tri_weights[2] * q2->vel.x);
            g_intersect.surface_vel.y = (float)(q2->vel.y * g_intersect.tri_weights[2] +
                                                (float)(q1->vel.y * g_intersect.tri_weights[1] +
                                                        (float)(q0->vel.y * g_intersect.tri_weights[0])));
            g_intersect.surface_vel.z = (float)(g_intersect.tri_weights[2] * q2->vel.z +
                                                (float)(g_intersect.tri_weights[1] * q1->vel.z +
                                                        (float)(g_intersect.tri_weights[0] * q0->vel.z)));
        }
        if (face->corners == 4) {
            b = c;
            c = ob->verts[face->verts[3]].pos;
            generatetrinormal(&normal, &a, &b, &c);
            if (lineintersecttriangletwosided(&hit, &normal, &t, s, e, &a, &b, &c, 1) != 0 && t < g_intersect.fraction) {
                g_intersect.fraction = t;
                keep_face_hit(ob, f, &hit, &normal, face->verts[0], face->verts[2], face->verts[3], &a, &b, &c);
                q0 = &sr_particles[g_intersect.tri_particles[0]];
                g_intersect.surface_vel.x = g_intersect.tri_weights[0] * q0->vel.x;
                g_intersect.surface_vel.y = (float)(q0->vel.y * g_intersect.tri_weights[0]);
                g_intersect.surface_vel.z = (float)(g_intersect.tri_weights[0] * q0->vel.z);
                scaleaddvectors(&g_intersect.surface_vel, &g_intersect.surface_vel,
                                &sr_particles[g_intersect.tri_particles[1]].vel, g_intersect.tri_weights[1]);
                scaleaddvectors(&g_intersect.surface_vel, &g_intersect.surface_vel,
                                &sr_particles[g_intersect.tri_particles[2]].vel, g_intersect.tri_weights[2]);
            }
        }
    }
    if (with_wheels != 0) {
        for (w = 0; w < ob->num_wheels; w++) {
            if (lineintersectsphere(&hit, &normal, &t, s, e, &ob->wheels[w].pos, ob->wheels[w].radius) != 0 &&
                t < g_intersect.fraction) {
                g_intersect.face = -1;
                memcpy(g_intersect.pos, &hit, sizeof g_intersect.pos);
                memcpy(g_intersect.normal, &normal, sizeof g_intersect.normal);
                g_intersect.fraction = t;
                g_intersect.tri_particles[0] = w;
            }
        }
    }
    return g_intersect.fraction < 1.0 ? 1 : 0;
}

/* The landscape (the global "landscape" of the Linux 0.24c): an origin, then a grid of
 * 2049 x 2049 heights, 8 units apart. Each cell is two triangles. */
#define g_land_origin (*SR_AT(vec3, 0x9c1d8c0))
#define g_land_height SR_AT(float, 0x9c1d8cc)
#define LAND_CELLS 0x800
#define LAND_ROW 0x801

/* Clamps a cell index to 0..LAND_CELLS. */
static int land_clamp(int i)
{
    if (i < 0)
        i = 0;
    if (LAND_CELLS < i)
        i = LAND_CELLS;
    return i;
}

/* Keeps a hit on the landscape (it does not move). */
static void keep_land_hit(const vec3 *hit, const vec3 *normal, float dist)
{
    memcpy(g_intersect.pos, hit, sizeof g_intersect.pos);
    memcpy(g_intersect.normal, normal, sizeof g_intersect.normal);
    g_intersect.fraction = 0.0f;
    g_intersect.surface_vel.x = 0.0f;
    g_intersect.surface_vel.y = 0.0f;
    g_intersect.surface_vel.z = 0.0f;
    g_intersect.distance = dist;
}

/* A sphere against the cells of the landscape under it. distance gets the distance of the
 * nearest hit. */
int sphereintersectlandscape(float *pos, float radius)
{
    const vec3 *p = (const vec3 *)pos;
    vec3 a, b, c, normal, hit;
    float x8, z8, x18, z18, dist;
    int x0, x1, z0, z1, x, z, i;

    x0 = land_clamp(-(int)(long long)(((p->x - g_land_origin.x) - radius) * -0.125) - 1);
    z0 = land_clamp(-(int)(long long)(((p->z - g_land_origin.z) - radius) * -0.125) - 1);
    x1 = land_clamp(1 - (int)(long long)((radius + (p->x - g_land_origin.x)) * -0.125));
    z1 = land_clamp(1 - (int)(long long)(((p->z - g_land_origin.z) + radius) * -0.125));
    g_intersect.fraction = 1.0f;
    g_intersect.distance = 65536.0f;
    for (z = z0; z <= z1; z++) {
        for (x = x0; x <= x1; x++) {
            if ((unsigned)x >= LAND_CELLS || (unsigned)z >= LAND_CELLS)
                continue;
            i = z * LAND_ROW + x;
            x8 = (float)((float)x * 8.0);
            z8 = (float)((float)z * 8.0);
            x18 = (float)((float)(x + 1) * 8.0);
            z18 = (float)((float)(z + 1) * 8.0);
            a.x = (float)(g_land_origin.x + x8);
            a.y = (float)(g_land_origin.y + g_land_height[i]);
            a.z = (float)(g_land_origin.z + z8);
            b.x = (float)(g_land_origin.x + x18);
            b.y = (float)(g_land_origin.y + g_land_height[i + 1]);
            b.z = (float)(g_land_origin.z + z8);
            c.x = (float)(x18 + g_land_origin.x);
            c.y = (float)(g_land_origin.y + g_land_height[i + LAND_ROW + 1]);
            c.z = (float)(g_land_origin.z + z18);
            generatetrinormal(&normal, &a, &b, &c);
            if (sphereintersecttriangle(&hit, &normal, &dist, p, radius, &a, &b, &c) != 0 && dist < g_intersect.distance)
                keep_land_hit(&hit, &normal, dist);
            b.x = (float)(g_land_origin.x + x18);
            b.y = (float)(g_land_origin.y + g_land_height[i + LAND_ROW + 1]);
            b.z = (float)(g_land_origin.z + z18);
            c.x = (float)(x8 + g_land_origin.x);
            c.y = (float)(g_land_origin.y + g_land_height[i + LAND_ROW]);
            c.z = (float)(g_land_origin.z + z18);
            generatetrinormal(&normal, &a, &b, &c);
            if (sphereintersecttriangle(&hit, &normal, &dist, p, radius, &a, &b, &c) != 0 && dist < g_intersect.distance)
                keep_land_hit(&hit, &normal, dist);
        }
    }
    return g_intersect.distance < 65536.0 ? 1 : 0;
}

/* Tests the two triangles of landscape cell (x, z) against the line from -> to. */
static void line_land_cell(int x, int z, const vec3 *from, const vec3 *to)
{
    vec3 a, b, c, normal, hit;
    float x8, z8, x18, z18, t;
    int i = z * LAND_ROW + x;

    x8 = (float)((float)x * 8.0);
    z8 = (float)((float)z * 8.0);
    x18 = (float)((float)(x + 1) * 8.0);
    z18 = (float)((float)(z + 1) * 8.0);
    a.x = (float)(g_land_origin.x + x8);
    a.y = (float)(g_land_origin.y + g_land_height[i]);
    a.z = (float)(g_land_origin.z + z8);
    b.x = (float)(g_land_origin.x + x18);
    b.y = (float)(g_land_origin.y + g_land_height[i + 1]);
    b.z = (float)(g_land_origin.z + z8);
    c.x = (float)(x18 + g_land_origin.x);
    c.y = (float)(g_land_height[i + LAND_ROW + 1] + g_land_origin.y);
    c.z = (float)(g_land_origin.z + z18);
    generatetrinormal(&normal, &a, &b, &c);
    if (lineintersecttriangle(&hit, &normal, &t, from, to, &a, &b, &c) != 0 && t < g_intersect.fraction) {
        memcpy(g_intersect.pos, &hit, sizeof g_intersect.pos);
        memcpy(g_intersect.normal, &normal, sizeof g_intersect.normal);
        g_intersect.fraction = t;
        g_intersect.surface_vel.x = 0.0f;
        g_intersect.surface_vel.y = 0.0f;
        g_intersect.surface_vel.z = 0.0f;
    }
    b.x = (float)(g_land_origin.x + x18);
    b.y = (float)(g_land_origin.y + g_land_height[i + LAND_ROW + 1]);
    b.z = (float)(g_land_origin.z + z18);
    c.x = (float)(x8 + g_land_origin.x);
    c.y = (float)(g_land_origin.y + g_land_height[i + LAND_ROW]);
    c.z = (float)(g_land_origin.z + z18);
    generatetrinormal(&normal, &a, &b, &c);
    if (lineintersecttriangle(&hit, &normal, &t, from, to, &a, &b, &c) != 0 && t < g_intersect.fraction) {
        memcpy(g_intersect.pos, &hit, sizeof g_intersect.pos);
        memcpy(g_intersect.normal, &normal, sizeof g_intersect.normal);
        g_intersect.fraction = t;
        g_intersect.surface_vel.x = 0.0f;
        g_intersect.surface_vel.y = 0.0f;
        g_intersect.surface_vel.z = 0.0f;
    }
}

/* The line from -> to against the landscape: a walk over the cells that the line crosses (the
 * step of y is always 0), at most 0x4000 cells. fraction gets the part of the line before the
 * nearest hit. */
int lineintersectlandscape(float *from, float *to)
{
    const vec3 *s = (const vec3 *)from, *e = (const vec3 *)to;
    const float *origin = &g_land_origin.x;
    uint32_t cell[3], end[3];
    int step[3], k, axis, n, i, cx, cz;
    float a[3], b[3], c[3], d, frac, f2, m;
    double t, fl;
    uint32_t dz, dx;

    for (k = 0; k < 3; k++) {
        cell[k] = (uint32_t)(long long)((from[k] - origin[k]) * 0.125);
        end[k] = (uint32_t)(long long)((to[k] - origin[k]) * 0.125);
        a[k] = (float)((to[k] - from[k]) * 0.125);
        if (k == 1)
            a[1] = 0.0f;
        step[k] = a[k] <= 0.0 ? -1 : 1;
        d = (float)fabs(a[k]);
        a[k] = d;
        b[k] = d;
        c[k] = d;
    }
    for (k = 0; k < 3; k++) {
        frac = (float)(from[k] * 0.125);
        t = (double)frac;
        fl = floor((double)frac);
        frac = (float)(t - (fl + 0.5));
        if (0 < step[k])
            frac = -frac;
        f2 = (float)(frac + frac);
        m = k == 0 ? c[1] : c[0];
        m = f2 * m;
        a[k] = (float)(m - a[k]);
        m = k == 2 ? c[1] : c[2];
        m = f2 * m;
        b[k] = (float)(m - b[k]);
    }
    dz = end[2] - cell[2];
    dx = end[0] - cell[0];
    n = (int)(((dz ^ (uint32_t)((int)dz >> 31)) - (uint32_t)((int)dz >> 31)) +
              ((dx ^ (uint32_t)((int)dx >> 31)) - (uint32_t)((int)dx >> 31)));
    if (0x4000 < n)
        n = 0x4000;
    g_intersect.block[0] = -1;
    g_intersect.fraction = 1.0f;
    if (n < 0)
        return 0;
    for (i = 0; i <= n; i++) {
        cx = (int)cell[0];
        cz = (int)cell[2];
        if (cell[0] < LAND_CELLS && cell[2] < LAND_CELLS)
            line_land_cell(cx, cz, s, e);
        if ((uint32_t)cx == end[0] && (uint32_t)cz == end[2])
            break;
        axis = a[1] <= a[0];
        if (a[2] <= b[0] || axis == 1)
            axis = b[2] < b[1] ? 2 : 1;
        cell[axis] += (uint32_t)step[axis];
        if (axis == 0) {
            a[0] = (float)(a[0] + c[1]);
            a[1] = (float)(a[1] - c[1]);
            a[2] = (float)(a[2] - c[2]);
            b[0] = (float)(b[0] + c[2]);
        } else if (axis == 1) {
            a[0] = (float)(a[0] - c[0]);
            a[1] = (float)(c[0] + a[1]);
            b[1] = (float)(c[2] + b[1]);
            b[2] = (float)(b[2] - c[2]);
        } else {
            a[2] = (float)(c[0] + a[2]);
            b[0] = (float)(b[0] - c[0]);
            b[1] = (float)(b[1] - c[1]);
            b[2] = (float)(b[2] + c[1]);
        }
    }
    return g_intersect.fraction < 1.0 ? 1 : 0;
}
