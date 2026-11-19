/* The simple head model.
 *
 * Eight points give a rough head. Each triangle splits into four along smooth curves (Hermite,
 * with the normal of each point), and two quads pull in to make the eyes. The model is flat.
 * The triangle list of the model in work comes back after the model: other code still uses it.
 */
#include "textures/head_models.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"
#include "textures/model_parts.h"
#include "textures/models.h"
#include "textures/noise.h"
#include "vectors.h"

#define g_temp cl_data_modeltemp

#define HEAD_POINTS 8
#define HEAD_SCALE 0.125f
#define HEAD_TRIS 12
#define TANGENT_SCALE 1.25
#define MERGE_TOLERANCE 6.103515625E-5f /* 1 / 16384 */
#define EYE_QUADS 2

/* The 8 points of the head before the scale: the top, four around the middle and three below
 * for the jaw. */
static const vec3 g_head_points[HEAD_POINTS] = {
    {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f},    {0.0f, 0.0f, -1.0f},  {-1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f}, {0.75f, -1.25f, 0.0f}, {0.0f, -1.5f, -1.0f}, {-0.75f, -1.25f, 0.0f},
};

/* The normal of each point, for the curves of the edges. Some zeros are negative zeros. */
static const vec3 g_head_normals[HEAD_POINTS] = {
    {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f},  {0.0f, 0.0f, -1.0f},  {-1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, -0.0f}, {0.0f, -0.0f, -1.0f}, {-1.0f, 0.0f, -0.0f},
};

static const int g_head_tris[HEAD_TRIS][3] = {
    {0, 2, 1}, {0, 3, 2}, {0, 4, 3}, {0, 1, 4}, {1, 2, 6}, {3, 6, 2},
    {1, 6, 5}, {3, 7, 6}, {4, 1, 5}, {4, 5, 7}, {4, 7, 3}, {5, 6, 7},
};

/* The two eye quads: vertex indexes after the split. */
static const int g_eye_quads[EYE_QUADS][4] = {{9, 2, 16, 17}, {2, 12, 18, 16}};

/* Add a triangle. Its shading mode stays as the model in work had it: the model is flat. */
static void head_tri(int a, int b, int c)
{
    MeshTri *tri = &g_temp.tris[g_temp.num_tris++];

    tri->verts[0] = a;
    tri->verts[1] = b;
    tri->verts[2] = c;
}

/* The tangent of a curve at a point along an edge: the edge made normal to the point normal,
 * unit length, times scale. */
static void edge_tangent(vec3 *out, const vec3 *normal, const vec3 *edge, float scale)
{
    vec3 side;

    crossproduct(&side, normal, edge);
    crossproduct(out, &side, normal);
    normalizevector(out, out);
    scalevector(out, out, scale);
}

/* Split each triangle into four: a new vertex at the middle of the curve of each edge, three
 * corner triangles at the end of the list and the middle triangle in the place of the old one. */
static void split_head_tris(void)
{
    int count = g_temp.num_tris, t, k;

    for (t = 0; t < count; t++) {
        int *tri = g_temp.tris[t].verts, mid[3];

        for (k = 0; k < 3; k++) {
            int a = tri[k], b = tri[(k + 1) % 3];
            vec3 edge, tangent_a, tangent_b, point;
            float scale;

            subtractvectors(&edge, &g_temp.verts[b], &g_temp.verts[a]);
            scale = (float)(vectorlength(&edge) * TANGENT_SCALE);
            edge_tangent(&tangent_a, &g_temp.vert_normals[a], &edge, scale);
            edge_tangent(&tangent_b, &g_temp.vert_normals[b], &edge, scale);
            hermiteinterpolation(&point.x, 0.5f, &g_temp.verts[a].x, &tangent_a.x, &g_temp.verts[b].x, &tangent_b.x);
            mid[k] = modeltemp_addvertex(&point, MERGE_TOLERANCE);
        }
        head_tri(tri[0], mid[0], mid[2]);
        head_tri(tri[1], mid[1], mid[0]);
        head_tri(tri[2], mid[2], mid[1]);
        tri[0] = mid[0];
        tri[1] = mid[1];
        tri[2] = mid[2];
    }
}

/* Remove each triangle whose three corners are all in the set: the last triangle moves into
 * its place. */
static void remove_tris_within(const int set[4])
{
    int t = 0, k, j, inside;

    while (t < g_temp.num_tris) {
        inside = 0;
        for (k = 0; k < 3; k++)
            for (j = 0; j < 4; j++)
                if (g_temp.tris[t].verts[k] == set[j]) {
                    inside++;
                    break;
                }
        if (inside == 3) {
            g_temp.num_tris--;
            g_temp.tris[t] = g_temp.tris[g_temp.num_tris];
        } else {
            t++;
        }
    }
}

/* Make the model of the simple head. */
void __cdecl createheadmodel3(int model)
{
    int i, q;

    g_temp.num_tris = 0;
    g_temp.num_quads = 0;
    g_temp.num_lines = 0;
    for (i = 0; i < HEAD_POINTS; i++)
        g_temp.verts[i] = g_head_points[i];
    g_temp.num_verts = HEAD_POINTS;
    for (i = 0; i < g_temp.num_verts; i++)
        scalevector(&g_temp.verts[i], &g_temp.verts[i], HEAD_SCALE);
    for (i = 0; i < HEAD_TRIS; i++)
        head_tri(g_head_tris[i][0], g_head_tris[i][1], g_head_tris[i][2]);
    for (i = 0; i < HEAD_POINTS; i++)
        g_temp.vert_normals[i] = g_head_normals[i];
    split_head_tris();
    for (q = 0; q < EYE_QUADS; q++)
        remove_tris_within(g_eye_quads[q]);
    for (i = 0; i < g_temp.num_tris; i++)
        model_settritexcoord(i, 1.0f, 0.0f);
    for (q = 0; q < EYE_QUADS; q++) {
        int quad = g_temp.num_quads++;

        memcpy(g_temp.quads[quad].verts, g_eye_quads[q], sizeof(g_eye_quads[q]));
        model_extrudequad2(quad);
        model_extrudeeye(quad, 0);
    }
    for (q = 0; q < g_temp.num_quads; q++) {
        model_setquadtexcoord(q, 1.0f, 0.0f);
        modeltemp_quadtotris(q);
    }
    memcpy(g_temp.tri_backup, g_temp.tris, (size_t)g_temp.num_tris * sizeof(MeshTri));
    setupmodel(model, 1);
    memcpy(g_temp.tris, g_temp.tri_backup, (size_t)g_temp.num_tris * sizeof(MeshTri));
}
