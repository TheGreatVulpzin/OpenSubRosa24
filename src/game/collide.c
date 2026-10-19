/* The collision tests against the level: the landscape, the 4 areas of blocks and the city
 * objects. Each test keeps the nearest hit in the global Intersect record and returns 1 when
 * there is a hit. */
#include "collide.h"

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
int __cdecl lf_1cdf0(float *pos, float radius);
int __cdecl lf_1c1d0(float *from, float *to);
int __cdecl lf_1c8f0(float *pos, float *rot, float radius);
int __cdecl lf_40aa0(float *hit, float *normal, float *dist, uint32_t area, float *pos, float radius);
int __cdecl lf_2c0c0(float *hit, float *normal, float *t, uint32_t area, float *from, float *to);
int __cdecl lf_41550(float *hit, float *normal, float *dist, uint32_t area, float *pos, float *rot, float radius);
int __cdecl lf_146a0(float *hit, float *normal, float *t, int object, float *from, float *to);
#define sphereintersectlandscape lf_1cdf0
#define lineintersectlandscape lf_1c1d0
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
