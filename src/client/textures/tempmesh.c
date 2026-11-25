/* Joins of the quads of the temporary mesh.
 *
 * tempmesh_optimize joins two quads of the outer mesh of a sector into one when they share an
 * edge, lie in one plane, have the same material and their sides continue in one line. The
 * texture coordinates of the joined quad keep the scale of the first quad.
 *
 * tempmesh_optimize2 does the same for the interior mesh. A quad with a window material, or a
 * quad that found no partner, is done and starts no more joins.
 */
#include "textures/tempmesh.h"

#include <math.h>

#include "client.h"
#include "setup/timers.h"
#include "sr_runtime.h"
#include "vectors.h"
#include "textures/mesh_edit.h"

#define g_tempmesh sr_data_tempmesh

/* Game functions of other files. */
#define tempmesh_deletequad CL_FN_NAMED(tempmesh_deletequad, 0x44d920, void(__cdecl *)(int quad))
#define tempmesh_vertnormalequal CL_FN_NAMED(tempmesh_vertnormalequal, 0x50b290, int(__cdecl *)(int a, int b))

#define g_materials cl_data_material_records
#define JOIN_TIMER 3
#define SAME_LINE 0.999750018119812f /* the cosine limit of two sides in one line */

#define NORMAL_LIMIT 9.999999747378752E-5f

/* 1 when the normals of quads a and b differ by less than 0.0001 on each axis. */
int __cdecl tempmesh_quadnormalequal(int a, int b)
{
    const vec3 *na = &g_tempmesh.quad_normals[a], *nb = &g_tempmesh.quad_normals[b];

    return fabsf((float)(na->x - nb->x)) < NORMAL_LIMIT && fabsf((float)(na->y - nb->y)) < NORMAL_LIMIT &&
           fabsf((float)(na->z - nb->z)) < NORMAL_LIMIT;
}

/* 1 when the direction from the first corner of quad i to corner a of quad i and the direction
 * to corner b of quad j are in one line. */
static int in_line(int i, int a, int j, int b)
{
    const TempMesh *tm = &g_tempmesh;
    const int *qi = tm->quads[i].verts, *qj = tm->quads[j].verts;
    vec3 d1, d2;

    subtractvectors(&d1, &tm->verts[qi[a]], &tm->verts[qi[0]]);
    subtractvectors(&d2, &tm->verts[qj[b]], &tm->verts[qi[0]]);
    normalizevector(&d1, &d1);
    normalizevector(&d2, &d2);
    return SAME_LINE <= dotproduct(&d1, &d2);
}

/* The lengths of the sides from corner 0 to corner 1 and to corner 3 of quad i. */
static void side_lengths(int i, float *side1, float *side3)
{
    const TempMesh *tm = &g_tempmesh;
    const int *q = tm->quads[i].verts;
    vec3 d1, d3;

    subtractvectors(&d1, &tm->verts[q[1]], &tm->verts[q[0]]);
    subtractvectors(&d3, &tm->verts[q[3]], &tm->verts[q[0]]);
    *side1 = vectorlength(&d1);
    *side3 = vectorlength(&d3);
}

/* Quad j lies past the side 1 to 2 of quad i: quad i takes corners 1 and 2 of quad j. */
static void join_along_1(int i, int j)
{
    TempMesh *tm = &g_tempmesh;
    int *q = tm->quads[i].verts;
    const int *qj = tm->quads[j].verts;
    float len1, len3, du, dv, eu, ev, new1, new3;
    vec3 *uv0, *uv1, *uv2;

    side_lengths(i, &len1, &len3);
    du = (float)((float)(tm->uvs[q[1]].x - tm->uvs[q[0]].x) / len1);
    dv = (float)((float)(tm->uvs[q[1]].y - tm->uvs[q[0]].y) / len1);
    eu = (float)((float)(tm->uvs[q[3]].x - tm->uvs[q[0]].x) / len3);
    ev = (float)((float)(tm->uvs[q[3]].y - tm->uvs[q[0]].y) / len3);
    q[1] = qj[1];
    q[2] = qj[2];
    side_lengths(i, &new1, &new3);
    uv0 = &tm->uvs[q[0]];
    uv1 = &tm->uvs[q[1]];
    uv2 = &tm->uvs[q[2]];
    uv1->x = (float)(du * new1 + uv0->x);
    uv1->y = (float)(dv * new1 + uv0->y);
    uv2->x = (float)(eu * new3 + (du * new1 + uv0->x));
    uv2->y = (float)(ev * new3 + (dv * new1 + uv0->y));
}

/* Quad j lies past the side 2 to 3 of quad i: quad i takes corners 2 and 3 of quad j. */
static void join_along_3(int i, int j)
{
    TempMesh *tm = &g_tempmesh;
    int *q = tm->quads[i].verts;
    const int *qj = tm->quads[j].verts;
    float len1, len3, du, dv, eu, ev, new1, new3;
    vec3 *uv0, *uv2, *uv3;

    side_lengths(i, &len1, &len3);
    du = (float)((float)(tm->uvs[q[1]].x - tm->uvs[q[0]].x) / len1);
    dv = (float)((float)(tm->uvs[q[1]].y - tm->uvs[q[0]].y) / len1);
    eu = (float)((float)(tm->uvs[q[3]].x - tm->uvs[q[0]].x) / len3);
    ev = (float)((float)(tm->uvs[q[3]].y - tm->uvs[q[0]].y) / len3);
    q[2] = qj[2];
    q[3] = qj[3];
    side_lengths(i, &new1, &new3);
    uv0 = &tm->uvs[q[0]];
    uv2 = &tm->uvs[q[2]];
    uv3 = &tm->uvs[q[3]];
    uv2->x = (float)(eu * new3 + (du * new1 + uv0->x));
    uv2->y = (float)(ev * new3 + (dv * new1 + uv0->y));
    uv3->x = (float)(eu * new3 + uv0->x);
    uv3->y = (float)(ev * new3 + uv0->y);
}

/* 1 when the side of quad i and the side of quad j have the same corners: side 1 is corners 1
 * and 2 of i on corners 0 and 3 of j, side 3 is corners 2 and 3 of i on corners 1 and 0 of j. */
static int shared_side(int i, int j, int side3)
{
    const TempMesh *tm = &g_tempmesh;
    const int *qi = tm->quads[i].verts, *qj = tm->quads[j].verts;
    int a = side3 ? 2 : 1, b = side3 ? 3 : 2, c = side3 ? 1 : 0, d = side3 ? 0 : 3;

    return tm->keys[qi[a]] == tm->keys[qj[c]] && tm->keys[qi[b]] == tm->keys[qj[d]] &&
           tm->quad_slots[i] == tm->quad_slots[j] && tempmesh_vertnormalequal(qi[a], qj[c]) &&
           tempmesh_vertnormalequal(qi[b], qj[d]) && tempmesh_quadnormalequal(i, j);
}

/* One scan for quads that join along side 1 or side 3. The result is 1 after a join. A join can
 * move the last quad into the place of quad j: a scan with quad i past the end stops at once.
 * Else the scan goes on with the next j. */
static int join_scan(int side3)
{
    TempMesh *tm = &g_tempmesh;
    int i, j, joined = 0;

    for (i = 0; i < tm->num_quads; i++)
        for (j = 0; j < tm->num_quads; j++) {
            if (i == j || !shared_side(i, j, side3) || tm->quad_slots[i] == sr_data_materiallist.sign)
                continue;
            if (!side3 ? !in_line(i, 1, j, 1) : !in_line(i, 3, j, 3))
                continue;
            if (!side3)
                join_along_1(i, j);
            else
                join_along_3(i, j);
            tempmesh_deletequad(j);
            joined = 1;
            if (i == tm->num_quads)
                return 1;
        }
    return joined;
}

/* Join the quads of the temporary mesh: first along side 1, then along side 3, each until a
 * scan finds no join. Quads with the sign material stay as they are. */
void __cdecl tempmesh_optimize(void)
{
    while (join_scan(0))
        ;
    while (join_scan(1))
        ;
}

/* Mark the quads with a window material as done. */
static void mark_windows(void)
{
    TempMesh *tm = &g_tempmesh;
    int i;

    for (i = 0; i < tm->num_quads; i++)
        tm->quad_done[i] = g_materials[tm->quad_slots[i]].flags != 0;
}

/* One scan of tempmesh_optimize2. The result is 1 when the scan must run again: a join with a
 * quad j before quad i, or quad i past the end after a join. A quad i that is not done gets done
 * at the end of its j loop unless a join with an earlier quad came before in the scan. A quad
 * can join itself: the scan does not compare i and j. */
static int join_scan2(int side3)
{
    TempMesh *tm = &g_tempmesh;
    int i, j, again = 0;

    for (i = 0; i < tm->num_quads; i++) {
        if (tm->quad_done[i] != 0)
            continue;
        for (j = 0; j < tm->num_quads; j++) {
            if (!shared_side(i, j, side3))
                continue;
            if (!side3 ? !in_line(i, 1, j, 1) : !in_line(i, 3, j, 3))
                continue;
            starttimer(JOIN_TIMER);
            if (!side3)
                join_along_1(i, j);
            else
                join_along_3(i, j);
            tempmesh_deletequad(j);
            tm->quad_done[j] = tm->quad_done[tm->num_quads];
            if (j < i)
                again = 1;
            stoptimer(JOIN_TIMER);
            if (i == tm->num_quads)
                return 1;
        }
        if (!again)
            tm->quad_done[i] = 1;
    }
    return again;
}

/* Join the quads of the interior mesh: first along side 1, then along side 3. */
void __cdecl tempmesh_optimize2(void)
{
    resettimer(JOIN_TIMER, "optimize2");
    mark_windows();
    while (join_scan2(0))
        ;
    mark_windows();
    while (join_scan2(1))
        ;
}
