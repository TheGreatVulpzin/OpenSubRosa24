/* Parts that the model builders add to the model in work: a cylinder, the cabin of a vehicle
 * and an eye.
 *
 * The cylinder has 16 sides and a point at the middle of each end. The cabin is an inner box
 * that follows the faces of 8 vertices of the body, with walls to close the gaps. The eye
 * replaces a quad of the head with a ring of 8 vertices around its middle.
 */
#include "textures/model_parts.h"

#include <string.h>

#include "client.h"
#include "math/small_vectors.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "textures/mesh_edit.h"
#include "textures/models.h"
#include "vectors.h"

#define g_temp cl_data_modeltemp
#define g_cubequad sr_data_cubequad /* the 4 corners of each of the 6 faces of a cube */

#define SHADE_FLAT (-2)
#define SHADE_SMOOTH (-1)
#define SHADE_CAP 2 /* the normal of corner 2: the middle of an end */
#define ROUND_SIDES 16
#define ROUND_SIDE_MASK 15
#define ROUND_STEP 0.39269909262657166 /* pi / 8 rounded to a float */
#define CABIN_FACES 6
#define CABIN_INSET 0.0625f
#define CABIN_FLOOR_DROP -0.375f /* face 0 is the floor */
#define CABIN_ROOF_DROP -0.0625f /* face 5 is the roof */
#define CABIN_ROOF 5
#define EYE_POINTS 8
#define EYE_STEP 0.7853981633975 /* pi / 4 */
#define EYE_DEPTH 0.015625f
#define EYE_U 0.625
#define EYE_V 0.875

/* Add one vertex: the place relative to center turned by rot, and the normal turned by rot. */
static void cylinder_vertex(const vec3 *center, const vec3 *rot, const vec3 *place, float normal_x, float normal_y,
                            float normal_z)
{
    int n = g_temp.num_verts;
    vec3 point;

    addvectors(&point, place, center);
    projectvector(&g_temp.verts[n], &point, rot);
    g_temp.vert_normals[n].x = normal_x;
    g_temp.vert_normals[n].y = normal_y;
    g_temp.vert_normals[n].z = normal_z;
    projectvector(&g_temp.vert_normals[n], &g_temp.vert_normals[n], rot);
    g_temp.vert_texcoords[n].u = 0.0f;
    g_temp.vert_texcoords[n].v = 0.0f;
    g_temp.num_verts++;
}

/* One ring of 16 vertices at the height z. */
static void cylinder_ring(const vec3 *center, const vec3 *rot, float radius, float z)
{
    float angle = 0.0f;
    int i;

    for (i = 0; i < ROUND_SIDES; i++) {
        vec3 place;

        place.x = (float)(x87_cos((double)angle) * radius);
        place.y = (float)(-x87_sin((double)angle) * radius);
        place.z = z;
        cylinder_vertex(center, rot, &place, (float)x87_cos((double)angle), (float)-x87_sin((double)angle), 0.0f);
        angle = (float)(angle + ROUND_STEP);
    }
}

/* Add a cylinder of 16 sides along -z from center, turned by rot. The sides are smooth and
 * each end has a point in the middle. */
void __cdecl addcylindertomodel(const vec3 *center, const vec3 *rot, float radius, float length)
{
    int first = g_temp.num_verts, i;
    vec3 place = {0.0f, 0.0f, 0.0f};

    cylinder_ring(center, rot, radius, 0.0f);
    cylinder_ring(center, rot, radius, -length);
    cylinder_vertex(center, rot, &place, 0.0f, 0.0f, 1.0f);
    place.z = -length;
    cylinder_vertex(center, rot, &place, 0.0f, 0.0f, -1.0f);
    for (i = 0; i < ROUND_SIDES; i++) {
        int next = (i + 1) & ROUND_SIDE_MASK;

        modeltemp_addtri(first + i, first + ROUND_SIDES + i, first + ROUND_SIDES + next, SHADE_SMOOTH);
        modeltemp_addtri(first + i, first + ROUND_SIDES + next, first + next, SHADE_SMOOTH);
        modeltemp_addtri(first + i, first + next, first + ROUND_SIDES * 2, SHADE_CAP);
        modeltemp_addtri(first + ROUND_SIDES + next, first + ROUND_SIDES + i, first + ROUND_SIDES * 2 + 1, SHADE_CAP);
    }
}

/* The body vertex at corner k of cabin face f. */
static int cabin_body(const int *corners, int f, int k)
{
    return corners[g_cubequad[f * 4 + (k & 3)]];
}

/* Add the cabin of a vehicle inside the box of 8 body vertices: for each corner of each of the
 * 6 box faces, an inner vertex moved in along the two edges of the face (the floor and the roof
 * also move down). Each face edge gets two triangles to its inner vertices, and walls close the
 * gaps between the floor, the roof and the side faces. */
void __cdecl addcockpittomodel(const int *corners)
{
    int first = g_temp.num_verts, f, k, side;

    for (f = 0; f < CABIN_FACES; f++)
        for (k = 0; k < 4; k++) {
            const vec3 *corner = &g_temp.verts[cabin_body(corners, f, k)];
            vec3 along, across, point;

            subtractvectors(&along, &g_temp.verts[cabin_body(corners, f, k + 1)], corner);
            subtractvectors(&across, &g_temp.verts[cabin_body(corners, f, k + 3)], corner);
            normalizevector(&along, &along);
            normalizevector(&across, &across);
            scaleaddvectors(&point, corner, &along, CABIN_INSET);
            scaleaddvectors(&point, &point, &across, CABIN_INSET);
            if (f == 0)
                scaleaddvectors(&point, &point, &sr_data_yaxis, CABIN_FLOOR_DROP);
            if (f == CABIN_ROOF)
                scaleaddvectors(&point, &point, &sr_data_yaxis, CABIN_ROOF_DROP);
            g_temp.verts[g_temp.num_verts++] = point;
        }
    for (f = 0; f < CABIN_FACES; f++)
        for (k = 0; k < 4; k++) {
            int next = (k + 1) & 3;

            modeltemp_addtri(cabin_body(corners, f, k), cabin_body(corners, f, next), first + f * 4 + next,
                             SHADE_SMOOTH);
            modeltemp_addtri(cabin_body(corners, f, k), first + f * 4 + next, first + f * 4 + k, SHADE_SMOOTH);
        }
    for (k = 0; k < 4; k++) {
        int next = (k + 1) & 3;

        for (side = 1; side < 5; side++)
            if (g_cubequad[k] == g_cubequad[side * 4 + 3] && g_cubequad[next] == g_cubequad[side * 4 + 2]) {
                modeltemp_addtri(first + k, first + next, first + 2 + side * 4, SHADE_SMOOTH);
                modeltemp_addtri(first + k, first + 2 + side * 4, first + 3 + side * 4, SHADE_SMOOTH);
                modeltemp_addtri(first + k, first + 3 + side * 4, first + 6 + (side & 3) * 4, SHADE_SMOOTH);
            }
        modeltemp_addtri(first + 4 + k * 4, first + 5 + next * 4, first + 6 + next * 4, SHADE_SMOOTH);
        modeltemp_addtri(first + 4 + k * 4, first + 6 + next * 4, first + 7 + k * 4, SHADE_SMOOTH);
        for (side = 1; side < 5; side++)
            if (g_cubequad[CABIN_ROOF * 4 + k] == g_cubequad[side * 4 + 1] &&
                g_cubequad[CABIN_ROOF * 4 + next] == g_cubequad[side * 4]) {
                modeltemp_addtri(first + 20 + k, first + 20 + next, first + side * 4, SHADE_SMOOTH);
                modeltemp_addtri(first + 20 + k, first + side * 4, first + 1 + side * 4, SHADE_SMOOTH);
                modeltemp_addtri(first + 20 + next, first + 5 + (side & 3) * 4, first + side * 4, SHADE_SMOOTH);
            }
    }
    modeltemp_addtri(first, first + 2, first + 1, SHADE_SMOOTH);
    modeltemp_addtri(first, first + 3, first + 2, SHADE_SMOOTH);
}

/* Add one eye vertex at offset from center, with the offset as its texture coordinate. The
 * vertex follows the face bone of the head. The result is its index. */
static int eye_vertex(const vec3 *center, const vec3 *offset, int bone)
{
    int n = g_temp.num_verts;

    g_temp.vert_texcoords[n].u = offset->x;
    g_temp.vert_texcoords[n].v = offset->y;
    addvectors(&g_temp.verts[n], center, offset);
    sethumanmodelvertface(n, bone, offset->x, offset->y, offset->z);
    g_temp.num_verts++;
    return n;
}

/* The texture coordinate of an eye triangle corner: 4 times the vertex coordinate, moved to the
 * place of the eye in the texture. */
static void eye_texcoord(int tri, int corner)
{
    const TexCoord *at = &g_temp.vert_texcoords[g_temp.tris[tri].verts[corner]];

    g_temp.tri_texcoords[tri].corners[corner].u = (float)(at->u * 4.0 + EYE_U);
    g_temp.tri_texcoords[tri].corners[corner].v = (float)(at->v * 4.0 + EYE_V);
}

/* Replace a quad of the head with an eye: a ring of 8 vertices around the middle of the quad,
 * a little out of the face, and a middle vertex. Flat triangles join the quad corners to the
 * ring, and the ring to the middle. Each odd ring vertex is a little nearer to the middle in y. */
void __cdecl model_extrudeeye(int quad, int bone)
{
    int corners[4], ring[EYE_POINTS], middle, i, k, t;
    float angle = 0.0f;
    vec3 center, offset;

    for (k = 0; k < 4; k++)
        corners[k] = g_temp.quads[quad].verts[k];
    addvectors(&center, &g_temp.verts[corners[0]], &g_temp.verts[corners[1]]);
    addvectors(&center, &center, &g_temp.verts[corners[2]]);
    addvectors(&center, &center, &g_temp.verts[corners[3]]);
    scalevector(&center, &center, 0.25f);
    for (i = 0; i < EYE_POINTS; i++) {
        offset.x = (float)(-x87_sin((double)angle) * 0.125 * 0.25 * 0.75);
        offset.y = (float)(x87_cos((double)angle) * 0.125 * 0.25 * 0.3125);
        offset.z = EYE_DEPTH;
        if (i & 1)
            offset.y = (float)(offset.y * 0.875);
        ring[i] = eye_vertex(&center, &offset, bone);
        angle = (float)(angle + EYE_STEP);
    }
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = EYE_DEPTH;
    middle = eye_vertex(&center, &offset, bone);
    for (k = 0; k < 4; k++) {
        int before = ((k + 3) & 3) * 2;

        model_settritexcoord(modeltemp_addtri(corners[k], corners[(k + 1) & 3], ring[k * 2], SHADE_FLAT), 1.0f, 0.0f);
        model_settritexcoord(modeltemp_addtri(corners[k], ring[k * 2], ring[before + 1], SHADE_FLAT), 1.0f, 0.0f);
        model_settritexcoord(modeltemp_addtri(corners[k], ring[before + 1], ring[before], SHADE_FLAT), 1.0f, 0.0f);
    }
    for (i = 0; i < EYE_POINTS; i++) {
        t = modeltemp_addtri(ring[i], ring[(i + 1) & 7], middle, SHADE_FLAT);
        for (k = 0; k < 3; k++)
            eye_texcoord(t, k);
    }
    model_deletequad(quad);
}

/* Replace a quad of the head with the larger eye of the Bezier head: a ring of 8 vertices, each
 * 1.5 times as far from the middle as in model_extrudeeye, and a middle vertex. The texture
 * coordinate of a ring vertex is 0.875 times its offset. Only the middle vertex follows the face
 * bone. The triangles are the same as in model_extrudeeye. */
void __cdecl model_extrudeeye2(int quad, int bone)
{
    int corners[4], ring[EYE_POINTS], middle, i, k, t, n;
    float angle = 0.0f;
    vec3 center, offset;
    double side;

    for (k = 0; k < 4; k++)
        corners[k] = g_temp.quads[quad].verts[k];
    addvectors(&center, &g_temp.verts[corners[0]], &g_temp.verts[corners[1]]);
    addvectors(&center, &center, &g_temp.verts[corners[2]]);
    addvectors(&center, &center, &g_temp.verts[corners[3]]);
    scalevector(&center, &center, 0.25f);
    for (i = 0; i < EYE_POINTS; i++) {
        side = -x87_sin((double)angle) * 0.125 * 0.75;
        offset.x = (float)((side + side) * 0.125 * 1.5);
        side = x87_cos((double)angle) * 0.125 * 0.3125;
        offset.y = (float)((side + side) * 0.125 * 1.5);
        offset.z = EYE_DEPTH;
        if (i & 1)
            offset.y = (float)(offset.y * 0.875);
        n = g_temp.num_verts;
        g_temp.vert_texcoords[n].u = (float)(offset.x * 0.875);
        g_temp.vert_texcoords[n].v = (float)(offset.y * 0.875);
        addvectors(&g_temp.verts[n], &center, &offset);
        ring[i] = g_temp.num_verts++;
        angle = (float)(angle + EYE_STEP);
    }
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = EYE_DEPTH;
    n = g_temp.num_verts;
    g_temp.vert_texcoords[n].u = 0.0f;
    g_temp.vert_texcoords[n].v = 0.0f;
    addvectors(&g_temp.verts[n], &center, &offset);
    middle = n;
    sethumanmodelvertface(n, bone, offset.x, offset.y, offset.z);
    g_temp.num_verts++;
    for (k = 0; k < 4; k++) {
        int before = ((k + 3) & 3) * 2;

        model_settritexcoord(modeltemp_addtri(corners[k], corners[(k + 1) & 3], ring[k * 2], SHADE_FLAT), 1.0f, 0.0f);
        model_settritexcoord(modeltemp_addtri(corners[k], ring[k * 2], ring[before + 1], SHADE_FLAT), 1.0f, 0.0f);
        model_settritexcoord(modeltemp_addtri(corners[k], ring[before + 1], ring[before], SHADE_FLAT), 1.0f, 0.0f);
    }
    for (i = 0; i < EYE_POINTS; i++) {
        t = modeltemp_addtri(ring[i], ring[(i + 1) & 7], middle, SHADE_FLAT);
        for (k = 0; k < 3; k++)
            eye_texcoord(t, k);
    }
    model_deletequad(quad);
}

#define C0 0
#define C1 1
#define C2 2
#define C3 3
#define P(row, k) (4 + (row) * 4 + (k)) /* vertex k of face row 0 (left eye), 1 (right eye), 2 (mouth) */
#define MOUTH_Z (-0.15f)
#define MOUTH_INNER_Z (-0.18125f)

/* Add a quad with one texture coordinate. The result is its index. */
static int add_quad(int a, int b, int c, int d, float u)
{
    int q = g_temp.num_quads;

    g_temp.quads[q].verts[0] = a;
    g_temp.quads[q].verts[1] = b;
    g_temp.quads[q].verts[2] = c;
    g_temp.quads[q].verts[3] = d;
    model_setquadtexcoord(q, u, 0.0f);
    g_temp.num_quads++;
    return q;
}

/* Make a face on a quad of the head. Three rows of 4 vertices go around the middle of the quad:
 * the left eye (row 0, face bone 0), the right eye (row 1, face bone 1) and the mouth (row 2,
 * bone 2). Flat triangles and quads fill the face between the rows and the quad corners. The
 * mouth goes 1/32 into the head, and each eye gets an eye from model_extrudeeye. The quads of
 * the eyes and the mouth go into head_face_quads. */
void __cdecl model_extrudequadface(int quad)
{
    static const int tris[12][3] = {
        {C0, P(0, 1), P(0, 0)}, {C0, P(0, 0), P(0, 3)}, {C0, P(0, 3), C3},
        {C1, C2, P(1, 2)},      {C1, P(1, 2), P(1, 1)}, {C1, P(1, 1), P(1, 0)},
        {C2, P(2, 2), P(2, 1)}, {C2, P(2, 1), P(1, 3)}, {C2, P(1, 3), P(1, 2)},
        {C3, P(0, 3), P(0, 2)}, {C3, P(0, 2), P(2, 0)}, {C3, P(2, 0), P(2, 3)},
    };
    static const int fill[3][4] = {
        {P(2, 3), P(2, 2), C2, C3}, {P(0, 2), P(1, 3), P(2, 1), P(2, 0)}, {P(0, 1), P(1, 0), P(1, 3), P(0, 2)},
    };
    int v[16], row, k, i, q;
    vec3 center, place, offset, depth;

    for (k = 0; k < 4; k++)
        v[k] = g_temp.quads[quad].verts[k];
    addvectors(&center, &g_temp.verts[v[0]], &g_temp.verts[v[1]]);
    addvectors(&center, &center, &g_temp.verts[v[2]]);
    addvectors(&center, &center, &g_temp.verts[v[3]]);
    scalevector(&center, &center, 0.25f);
    for (row = 0; row < 3; row++)
        for (k = 0; k < 4; k++) {
            float half_width = row < 2 ? 0.03125f : 0.015625f;
            int n = g_temp.num_verts;

            memset(&place, 0, sizeof place);
            memset(&offset, 0, sizeof offset);
            if (row == 0)
                place.x = 0.0625f;
            else if (row == 1)
                place.x = -0.0625f;
            place.y = row < 2 ? 0.03125f : -0.03125f;
            offset.x = k == 0 || k == 3 ? half_width : -half_width;
            offset.y = k < 2 ? 0.015625f : -0.015625f;
            if (row == 2 && k < 2)
                offset.y = (float)(offset.y + 0.015625);
            addvectors(&place, &place, &offset);
            addvectors(&g_temp.verts[n], &center, &place);
            v[P(row, k)] = n;
            if (row < 2)
                sethumanmodelvertface(n, row, offset.x, offset.y, offset.z);
            else
                sethumanmodelvert(n, 2, place.x, place.y, MOUTH_Z);
            g_temp.num_verts++;
        }
    for (i = 0; i < 12; i++)
        model_settritexcoord(modeltemp_addtri(v[tris[i][0]], v[tris[i][1]], v[tris[i][2]], SHADE_FLAT), 1.0f, 0.0f);
    for (i = 0; i < 3; i++)
        add_quad(v[fill[i][0]], v[fill[i][1]], v[fill[i][2]], v[fill[i][3]], 1.0f);
    for (row = 0; row < 3; row++) {
        q = add_quad(v[P(row, 0)], v[P(row, 1)], v[P(row, 2)], v[P(row, 3)], row == 2 ? 1.0f : 0.0f);
        cl_data_head_face_quads[row] = q;
    }
    g_temp.quads[quad].verts[0] = v[C0];
    g_temp.quads[quad].verts[1] = v[C1];
    g_temp.quads[quad].verts[2] = v[P(1, 0)];
    g_temp.quads[quad].verts[3] = v[P(0, 1)];
    depth.x = 0.0f;
    depth.y = 0.0f;
    depth.z = -0.03125f;
    model_extrudequad(cl_data_head_face_quads[2], &depth);
    /* The 4 new vertices of the mouth: the upper two are narrower. */
    for (k = 0; k < 4; k++) {
        int n = g_temp.num_verts - 4 + k;

        offset.x = k == 0 || k == 3 ? 0.015625f : -0.015625f;
        if (k < 2) {
            offset.y = (float)((float)(0.0f - 0.03125) + 0.015625);
            offset.x = (float)(offset.x * 0.75);
        } else {
            offset.y = (float)((0.0f - 0.03125) - 0.015625);
        }
        sethumanmodelvert(n, 2, offset.x, offset.y, MOUTH_INNER_Z);
    }
    for (q = g_temp.num_quads - 4; q < g_temp.num_quads; q++)
        model_setquadtexcoord(q, 1.0f, 0.0f);
    model_extrudeeye(cl_data_head_face_quads[0], 0);
    model_extrudeeye(cl_data_head_face_quads[1], 1);
}
