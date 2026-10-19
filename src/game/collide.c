/* The collision tests against the level: the landscape, the 4 areas of blocks and the city
 * objects. Each test keeps the nearest hit in the global Intersect record and returns 1 when
 * there is a hit. */
#include "collide.h"

#include <stdint.h>
#include <string.h>

#include "sr_runtime.h"

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
