/* The collision tests against the blocks of the 4 areas. A test moves the point or the line
 * into the frame of the area, where block (x, y, z) fills x..x+1 times the block size on each
 * axis. A hit goes back into the world frame. */
#include "collide.h"

#include <math.h>
#include <stdlib.h>
#include <stdint.h>

#include "area.h"
#include "intersect.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define g_intersect (*SR_AT(Intersect, 0x103484a0))
#define g_yaxis (*SR_AT(vec3, 0x60ce0)) /* (0 1 0) */
#define g_blocktypes SR_AT(AreaBlock, SR_RVA_AREA_BLOCK)
#define g_cityobjecttypes SR_AT(CityObjectTypeRec, 0xf02e500)
#define g_cubevertex SR_AT(vec3, 0x60018)                       /* the 8 corners of a cube of side 1 */
#define g_cubenormal SR_AT(vec3, 0x60078)                       /* the normal of each of the 6 faces */
#define g_cubequad ((const int (*)[4])SR_AT(int, SR_RVA_CUBEQUAD)) /* the 4 corners of each face */

/* The most cells that lineintersectarea walks. */
#define MAX_LINE_CELLS 0x4000

/* out = the frame position of corner v of a block at base. */
static void block_corner(vec3 *out, const vec3 *v, const vec3 *base, float size)
{
    out->x = (float)(v->x * size + base->x);
    out->y = (float)(v->y * size + base->y);
    out->z = (float)(v->z * size + base->z);
}

/* The corner of block cell with the lowest x, y and z, in the frame of the area. */
static void block_base(vec3 *base, const int cell[3], float size)
{
    base->x = size * (float)cell[0];
    base->y = (float)((float)cell[1] * size);
    base->z = (float)((float)cell[2] * size);
}

/* The line from -> to against the faces of the cube of block cell. A face counts when bits f and
 * f + 6 of word are set. The first triangle that the line hits stops the test. normal gets the
 * normal of each face that the test reaches. */
int lineintersectnormalblock(vec3 *hit, vec3 *normal, float *t, const vec3 *from, const vec3 *to, const int cell[3],
                             uint32_t word, float size)
{
    vec3 base, a, b, c;
    int f;

    block_base(&base, cell, size);
    for (f = 0; f < 6; f++) {
        const int *q = g_cubequad[f];

        if ((word & 1u << f) == 0 || (word & 0x40u << f) == 0)
            continue;
        block_corner(&a, &g_cubevertex[q[0]], &base, size);
        block_corner(&b, &g_cubevertex[q[1]], &base, size);
        block_corner(&c, &g_cubevertex[q[2]], &base, size);
        *normal = g_cubenormal[f];
        if (lineintersecttriangle(hit, normal, t, from, to, &a, &b, &c) != 0) {
            g_intersect.block_face = f;
            return 1;
        }
        block_corner(&b, &g_cubevertex[q[2]], &base, size);
        block_corner(&c, &g_cubevertex[q[3]], &base, size);
        if (lineintersecttriangle(hit, normal, t, from, to, &a, &b, &c) != 0) {
            g_intersect.block_face = f;
            return 1;
        }
    }
    return 0;
}

/* The line from -> to against the mesh of the block type in the low 16 bits of word. Bits 16 to
 * 25 hide the first faces. hit, normal and *t get the nearest hit. */
int lineintersectspecblock(vec3 *hit, vec3 *normal, float *t, const vec3 *from, const vec3 *to, const int cell[3],
                           uint32_t word, float size)
{
    const AreaBlock *bt = &g_blocktypes[word & 0xffff];
    const vec3 *verts = (const vec3 *)(uintptr_t)bt->verts;
    const int(*faces)[4] = (const int(*)[4])(uintptr_t)bt->faces;
    const int *slots = (const int *)(uintptr_t)bt->face_slots;
    uint32_t hidden = word >> 16 & 0x3ff;
    vec3 base, a, b, c, d, n, h;
    float tt;
    int i;

    block_base(&base, cell, size);
    *t = 1.0f;
    if (!(0 < bt->num_faces))
        return 0;
    for (i = 0; i < bt->num_faces; i++) {
        if (i < bt->num_hideable && (hidden & 1u << (i & 31)) != 0)
            continue;
        block_corner(&a, &verts[faces[i][0]], &base, size);
        block_corner(&b, &verts[faces[i][1]], &base, size);
        block_corner(&c, &verts[faces[i][2]], &base, size);
        block_corner(&d, &verts[faces[i][3]], &base, size);
        generatetrinormal(&n, &a, &b, &c);
        if (lineintersecttriangle(&h, &n, &tt, from, to, &a, &b, &c) != 0 && tt < *t) {
            *hit = h;
            *normal = n;
            *t = tt;
            g_intersect.block_face = slots[i];
            g_intersect.spec_face = i;
        }
        if (lineintersecttriangle(&h, &n, &tt, from, to, &a, &c, &d) != 0 && tt < *t) {
            *hit = h;
            *normal = n;
            *t = tt;
            g_intersect.block_face = slots[i];
            g_intersect.spec_face = i;
        }
    }
    return *t < 1.0 ? 1 : 0;
}

/* out = p in the frame of area a. */
static void to_area(vec3 *out, const Area *a, const vec3 *p)
{
    float dx = (float)(p->x - a->origin.x), dy = (float)(p->y - a->origin.y), dz = (float)(p->z - a->origin.z);

    out->x = (float)(dy * a->axes[0].y + dx * a->axes[0].x + dz * a->axes[0].z);
    out->y = (float)(a->axes[1].z * dz + (a->axes[1].y * dy + a->axes[1].x * dx));
    out->z = (float)(dz * a->axes[2].z + (a->axes[2].y * dy + a->axes[2].x * dx));
}

/* Moves a hit point and its normal from the frame of area a into the world. */
static void to_world(vec3 *hit, vec3 *normal, const Area *a)
{
    projectvector(hit, hit, a->axes);
    addvectors(hit, hit, &a->origin);
    projectvector(normal, normal, a->axes);
}

/* The place and the turn of a city object type in block cell. turn is in steps of 90 degrees
 * about the y axis. */
static void city_in_block(vec3 *pos, vec3 rot[3], int type, int turn, const int cell[3], float size)
{
    static const float angle[4] = {0.0f, 1.5707964f, 3.1415927f, 4.712389f};
    float half;

    *pos = g_cityobjecttypes[type].block_offset;
    if (turn != 0)
        rotatevector(pos, &g_yaxis, angle[turn]);
    half = size * 0.5f;
    pos->x = (float)((float)cell[0] * size + half + pos->x);
    pos->y = (float)((float)cell[1] * size + pos->y);
    pos->z = (float)((float)cell[2] * size + half + pos->z);
    resetorientation(rot);
    if (turn != 0)
        rotateorientation(rot, &g_yaxis, angle[turn]);
}

/* Writes the block of a hit into the Intersect record. */
static void set_hit_block(uint32_t area, const int cell[3], uint32_t word)
{
    g_intersect.block_area = area;
    g_intersect.block[0] = cell[0];
    g_intersect.block[1] = cell[1];
    g_intersect.block[2] = cell[2];
    g_intersect.block_word = word;
}

/* The line s -> e (frame of the area) against block word at cell. A link tests the special
 * block that it points to. interior: a link reads its word with area_interior_getblock. */
static int line_block(vec3 *hit, vec3 *normal, float *t, uint32_t area, const int cell[3], const vec3 *s,
                      const vec3 *e, uint32_t word, int interior)
{
    const Area *a = &g_areas[area];
    const int *at = cell;
    int link[3];

    switch (BLOCK_KIND(word)) {
    case BLOCK_NORMAL:
        if (lineintersectnormalblock(hit, normal, t, s, e, cell, word, a->block_size) == 0)
            return 0;
        break;
    case BLOCK_SPEC:
        if (lineintersectspecblock(hit, normal, t, s, e, cell, word, a->block_size) == 0)
            return 0;
        break;
    case BLOCK_LINK:
        link[0] = cell[0] - LINK_DX(word);
        link[1] = cell[1] - LINK_DY(word);
        link[2] = cell[2] - LINK_DZ(word);
        word = interior ? area_interior_getblock(area, link[0], link[1], link[2])
                        : area_getblock(area, link[0], link[1], link[2]);
        if (lineintersectspecblock(hit, normal, t, s, e, link, word, a->block_size) == 0)
            return 0;
        at = link;
        break;
    default:
        return 0;
    }
    to_world(hit, normal, a);
    set_hit_block(area, at, word);
    return 1;
}

/* The line s -> e (frame of the area) against cell: first the city object, then the outer
 * block, then the interior block. */
static int line_cell(vec3 *hit, vec3 *normal, float *t, uint32_t area, const int cell[3], const vec3 *s,
                     const vec3 *e)
{
    const Area *a = &g_areas[area];
    const BlockSector *sec;
    int k, idx;
    uint32_t co;

    if ((k = area_blocksector(area, cell[0], cell[1], cell[2], &idx)) == -1)
        return 0;
    sec = &g_sectors[k];
    co = sec->cityobjects[idx];
    if ((int)co > 0) {
        int type = co & 0x3ff;
        vec3 pos, rot[3];

        city_in_block(&pos, rot, type, (int)co >> 10 & 3, cell, a->block_size);
        if (lineintersectcityobjecttype(&hit->x, &normal->x, t, type, &pos.x, &rot[0].x, (float *)&s->x,
                                        (float *)&e->x) != 0) {
            to_world(hit, normal, a);
            set_hit_block(area, cell, 0);
            g_intersect.index = type;
            return 1;
        }
    }
    if (sec->blocks[idx] != 0 && line_block(hit, normal, t, area, cell, s, e, sec->blocks[idx], 0) != 0)
        return 1;
    if (sec->interior[idx] != 0 && line_block(hit, normal, t, area, cell, s, e, sec->interior[idx], 1) != 0)
        return 1;
    return 0;
}

/* The line from -> to against area `area`. It walks the cells from the start of the line to its
 * end and stops at the first hit. hit, normal and *t get the hit, and the Intersect record gets
 * its block. */
int lineintersectarea(float *hitp, float *normalp, float *t, uint32_t area, float *fromp, float *top)
{
    const Area *a = &g_areas[area];
    const vec3 *from = (const vec3 *)fromp, *to = (const vec3 *)top;
    vec3 *hit = (vec3 *)hitp, *normal = (vec3 *)normalp;
    vec3 s, e;
    const float *sv = &s.x, *ev = &e.x;
    float adv[3], err[6], delta, frac;
    int cell[3], end[3], dir[3], count, axis, i, k;

    if ((from->x < a->box_min.x && to->x < a->box_min.x) || (from->y < a->box_min.y && to->y < a->box_min.y) ||
        (from->z < a->box_min.z && to->z < a->box_min.z) || (a->box_max.x < from->x && a->box_max.x < to->x) ||
        (a->box_max.y < from->y && a->box_max.y < to->y) || (a->box_max.z < from->z && a->box_max.z < to->z))
        return 0;
    to_area(&s, a, from);
    to_area(&e, a, to);

    /* adv: the cells that the line crosses on each axis. err[]: the error terms that choose the
     * next axis. */
    for (i = 0; i < 3; i++) {
        cell[i] = (int)(long long)(a->inv_block_size * sv[i]);
        end[i] = (int)(long long)(a->inv_block_size * ev[i]);
        delta = (float)((ev[i] - sv[i]) * a->inv_block_size);
        dir[i] = delta <= 0.0 ? -1 : 1;
        adv[i] = (float)fabs(delta);
        err[i] = adv[i];
        err[i + 3] = adv[i];
    }
    if (cell[0] == end[0] && cell[1] == end[1] && cell[2] == end[2]) {
        count = 0;
    } else {
        for (i = 0; i < 3; i++) {
            double f = (float)(sv[i] * a->inv_block_size);

            frac = (float)(f - (floor(f) + 0.5));
            if (0 < dir[i])
                frac = -frac;
            err[i] = (float)((float)(frac + frac) * (i == 0 ? adv[1] : adv[0]) - err[i]);
            err[i + 3] = (float)((frac + frac) * (i == 2 ? adv[1] : adv[2]) - err[i + 3]);
        }
        count = abs(end[2] - cell[2]) + abs(end[1] - cell[1]) + abs(end[0] - cell[0]);
        if (count > MAX_LINE_CELLS)
            count = MAX_LINE_CELLS;
    }
    for (k = 0; k <= count; k++) {
        if (line_cell(hit, normal, t, area, cell, &s, &e) != 0)
            return 1;
        if (cell[0] == end[0] && cell[1] == end[1] && cell[2] == end[2])
            return 0;
        axis = err[1] <= err[0];
        if (err[2] <= err[3] || axis == 1)
            axis = err[5] < err[4] ? 2 : 1;
        cell[axis] += dir[axis];
        if (axis == 0) {
            err[0] = (float)(adv[1] + err[0]);
            err[1] = (float)(err[1] - adv[1]);
            err[2] = (float)(err[2] - adv[2]);
            err[3] = (float)(adv[2] + err[3]);
        } else if (axis == 1) {
            err[0] = (float)(err[0] - adv[0]);
            err[1] = (float)(err[1] + adv[0]);
            err[4] = (float)(adv[2] + err[4]);
            err[5] = (float)(err[5] - adv[2]);
        } else {
            err[2] = (float)(adv[0] + err[2]);
            err[3] = (float)(err[3] - adv[0]);
            err[4] = (float)(err[4] - adv[1]);
            err[5] = (float)(adv[1] + err[5]);
        }
    }
    return 0;
}
