/* The head models.
 *
 * createheadmodel2 makes a head from eight cubic Bezier triangles. Each triangle splits into 16,
 * and the corners of the small triangles go on the patch. A set of hand placed triangles then
 * replaces one area of the face.
 *
 * createheadmodel3 makes the simple head. Eight points give a rough head. Each triangle splits
 * into four along smooth curves (Hermite, with the normal of each point), and two quads pull in
 * to make the eyes. The model is flat. The triangle list of the model in work comes back after
 * the model: other code still uses it.
 */
#include "textures/head_models.h"

#include <string.h>

#include "client.h"
#include "input/keyboard.h"
#include "sr_runtime.h"
#include "textures/model_parts.h"
#include "textures/models.h"
#include "textures/noise.h"
#include "vectors.h"

#define g_temp cl_data_modeltemp
#define g_keys cl_data_audio_keys

/* Game functions of other files. */
#define model_addvertex CL_FN(0x4badd0, int(__cdecl *)(const vec3 *pos, float tolerance))
#define tesselatebeziertriangle CL_FN(0x4baed0, void(__cdecl *)(vec3 *out, float a, float b, float c, const int *patch))

/* createheadmodel2 */

/* A cubic Bezier triangle: 10 indexes into the edit vertices. Corners 0 to 2, then two points
 * on each edge, then the middle point. The patch of a triangle is in the backup triangle list,
 * 40 bytes for each triangle. */
typedef struct HeadPatch { int points[10]; } HeadPatch;

#define g_patches ((HeadPatch *)g_temp.tri_backup)

#define HEAD2_PATCHES 4       /* patches in each half of the head */
#define HEAD2_SPLITS 2
#define HEAD2_MERGE 0.001953125f /* 1 / 512 */
#define HEAD2_TOP 0.109375       /* a corner above this height gives its triangle texture row 0.625 */
#define HEAD2_EYE_DEPTH 0.03125f

/* The points of the hand placed area. M and CUT points are corners of the split patches that
 * a mark finds. The triangles at CUT points go away. N points are new points on the patches. */
enum {
    M_0, M_1, CUT_A, M_2, CUT_B, M_3, M_4, M_5, M_6, M_7, M_8, M_9,
    N_0, N_1, N_2, N_3, N_4, N_5, N_6, N_7, N_8,
    HEAD2_POINTS
};

/* A point of the split patches: the corner ids of the patch and the weights of the point. An
 * id or a weight of -1 is not tested. */
typedef struct HeadMark { int point; int corner[3]; float weight[3]; } HeadMark;

static const HeadMark g_head_marks[] = {
    {M_0,   {3, -1, -1}, {1.0f, -1.0f, -1.0f}},
    {M_1,   {5, 3, -1},  {0.25f, 0.75f, -1.0f}},
    {CUT_A, {3, 2, -1},  {0.75f, 0.25f, -1.0f}},
    {M_2,   {3, -1, 0},  {0.75f, -1.0f, 0.25f}},
    {CUT_B, {4, 3, -1},  {0.25f, 0.75f, -1.0f}},
    {M_3,   {5, 2, 3},   {0.25f, 0.25f, 0.5f}},
    {M_4,   {5, 2, 3},   {0.0f, 0.5f, 0.5f}},
    {M_5,   {3, 2, 0},   {0.5f, 0.25f, 0.25f}},
    {M_6,   {3, 2, 0},   {0.5f, 0.0f, 0.5f}},
    {M_7,   {4, 3, 0},   {0.25f, 0.5f, 0.25f}},
    {M_8,   {4, 3, 0},   {0.5f, 0.5f, 0.0f}},
    {M_9,   {5, 3, 4},   {0.25f, 0.5f, 0.25f}},
};

/* The hand placed triangles. */
static const int g_head2_tris[][3] = {
    {M_9, N_0, M_8}, {M_9, N_1, N_0}, {M_9, M_1, N_1}, {M_0, N_2, N_1}, {N_2, M_2, M_7},
    {N_3, N_2, M_7}, {M_8, N_3, M_7}, {M_8, N_0, N_3}, {M_1, M_3, N_4}, {M_3, N_5, N_4},
    {M_3, M_4, N_5}, {M_4, N_6, N_5}, {M_4, M_5, N_6}, {N_7, N_6, M_5}, {N_7, M_5, N_8},
    {M_0, N_4, N_7}, {M_1, N_4, N_1}, {N_1, N_4, M_0}, {M_0, N_7, N_2}, {M_2, M_6, M_7},
    {M_2, N_8, M_6}, {N_8, M_5, M_6},
};

/* Set the corner ids of the 8 patches: four around the top half and four around the bottom
 * half. Each patch covers its whole triangle. */
static void head2_patches(void)
{
    static const vec3 identity[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    int i, k;

    for (i = 0; i < HEAD2_PATCHES; i++) {
        int *p = g_patches[g_temp.num_tris].points, next = (i + 1) & 3;

        p[0] = next + 1;
        p[1] = (i & 3) + 1;
        p[2] = 0;
        p[3] = i * 2 + 15;
        p[4] = i * 2 + 14;
        p[5] = i * 2 + 7;
        p[6] = i * 2 + 6;
        p[7] = next * 2 + 6;
        p[8] = next * 2 + 7;
        p[9] = i + 30;
        for (k = 0; k < 3; k++)
            g_temp.patch_coords[g_temp.num_tris * 3 + k] = identity[k];
        g_temp.num_tris++;
    }
    for (i = 0; i < HEAD2_PATCHES; i++) {
        int *p = g_patches[g_temp.num_tris].points, next = (i + 1) & 3;

        p[0] = 5;
        p[1] = (i & 3) + 1;
        p[2] = next + 1;
        p[3] = i * 2 + 23;
        p[4] = i * 2 + 22;
        p[5] = i * 2 + 14;
        p[6] = i * 2 + 15;
        p[7] = next * 2 + 22;
        p[8] = next * 2 + 23;
        p[9] = i + 34;
        for (k = 0; k < 3; k++)
            g_temp.patch_coords[g_temp.num_tris * 3 + k] = identity[k];
        g_temp.num_tris++;
    }
}

/* Add a triangle with the patch of triangle t and the weights a, b and c. */
static void head2_add_part(int t, const vec3 *a, const vec3 *b, const vec3 *c)
{
    int n = g_temp.num_tris;

    g_patches[n] = g_patches[t];
    g_temp.patch_coords[n * 3] = *a;
    g_temp.patch_coords[n * 3 + 1] = *b;
    g_temp.patch_coords[n * 3 + 2] = *c;
    g_temp.num_tris++;
}

/* Split each triangle into four at the middle of its edges, in the weights of the patch. */
static void head2_split(void)
{
    int count = g_temp.num_tris, t;

    for (t = 0; t < count; t++) {
        vec3 *c = &g_temp.patch_coords[t * 3];
        vec3 c0 = c[0], c1 = c[1], c2 = c[2], edge, m01, m12, m20;

        subtractvectors(&edge, &c[1], &c[0]);
        scaleaddvectors(&m01, &c[0], &edge, 0.5f);
        subtractvectors(&edge, &c[2], &c[1]);
        scaleaddvectors(&m12, &c[1], &edge, 0.5f);
        subtractvectors(&edge, &c[0], &c[2]);
        scaleaddvectors(&m20, &c[2], &edge, 0.5f);
        head2_add_part(t, &c0, &m01, &m20);
        head2_add_part(t, &m01, &c1, &m12);
        head2_add_part(t, &m20, &m12, &c2);
        c[0] = m01;
        c[1] = m12;
        c[2] = m20;
    }
}

/* 1 when corner k of triangle t matches the mark. */
static int head2_marked(const HeadMark *m, int t, int k)
{
    const int *p = g_patches[t].points;
    const float *w = &g_temp.patch_coords[t * 3 + k].x;
    int j;

    for (j = 0; j < 3; j++) {
        if (m->corner[j] >= 0 && p[j] != m->corner[j])
            return 0;
        if (m->weight[j] >= 0.0f && w[j] != m->weight[j])
            return 0;
    }
    return 1;
}

/* Put each corner of each triangle on its patch. The marks find the M and CUT points. A mark
 * that matches no corner leaves its point unset. */
static void head2_place(int points[HEAD2_POINTS])
{
    int t, k, m;

    for (t = 0; t < g_temp.num_tris; t++) {
        model_settritexcoord(t, 1.0f, 0.0f);
        for (k = 0; k < 3; k++) {
            const vec3 *w = &g_temp.patch_coords[t * 3 + k];
            vec3 pos;

            tesselatebeziertriangle(&pos, w->x, w->y, w->z, g_patches[t].points);
            g_temp.tris[t].verts[k] = model_addvertex(&pos, HEAD2_MERGE);
            for (m = 0; m < (int)(sizeof g_head_marks / sizeof g_head_marks[0]); m++)
                if (head2_marked(&g_head_marks[m], t, k))
                    points[g_head_marks[m].point] = g_temp.tris[t].verts[k];
        }
    }
}

/* Remove each triangle that uses vertex v: the last triangle moves into its place. */
static void head2_remove_tris(int v)
{
    int t = 0;

    while (t < g_temp.num_tris) {
        const int *tri = g_temp.tris[t].verts;

        if (tri[0] == v || tri[1] == v || tri[2] == v) {
            g_temp.num_tris--;
            g_temp.tris[t] = g_temp.tris[g_temp.num_tris];
        } else {
            t++;
        }
    }
}

/* The vertex at weights a, b and c of patch p. */
static int head2_point(int p, float a, float b, float c)
{
    vec3 pos;

    tesselatebeziertriangle(&pos, a, b, c, g_patches[p].points);
    return model_addvertex(&pos, HEAD2_MERGE);
}

/* Add the N points. The shift key changes the weights of N_1 and N_4. M_2 moves to a new place
 * on patch 2. */
static void head2_new_points(int points[HEAD2_POINTS])
{
    float corner = g_keys[KEY_SHIFT] != 0 ? 0.125f : 0.0625f, sum;
    vec3 pos;

    points[N_0] = head2_point(6, 0.0714285746216774f, 0.5714285969734192f, 0.3571428656578064f);
    sum = (float)(corner + 0.9375 + 0.0625);
    points[N_1] = head2_point(6, (float)(corner / sum), (float)(0.9375 / sum), (float)(0.0625 / sum));
    points[N_2] = head2_point(2, 0.05882352963089943f, 0.8823529481887817f, 0.05882352963089943f);
    points[N_3] = head2_point(2, 0.3571428656578064f, 0.5714285969734192f, 0.0714285746216774f);
    sum = (float)(corner + 0.0625 + 0.9375);
    points[N_4] = head2_point(5, (float)(corner / sum), (float)(0.0625 / sum), (float)(0.9375 / sum));
    points[N_5] = head2_point(5, 0.0714285746216774f, 0.3571428656578064f, 0.5714285969734192f);
    points[N_6] = head2_point(1, 0.5714285969734192f, 0.3571428656578064f, 0.0714285746216774f);
    points[N_7] = head2_point(1, 0.8823529481887817f, 0.05882352963089943f, 0.05882352963089943f);
    tesselatebeziertriangle(&pos, 0.05882352963089943f, 0.7058823704719543f, 0.23529411852359772f, g_patches[2].points);
    g_temp.verts[points[M_2]] = pos;
    points[N_8] = head2_point(1, 0.7058823704719543f, 0.05882352963089943f, 0.23529411852359772f);
}

/* Add a quad of four points. */
static int head2_quad(const int points[HEAD2_POINTS], int a, int b, int c, int d)
{
    int q = g_temp.num_quads++;

    g_temp.quads[q].verts[0] = points[a];
    g_temp.quads[q].verts[1] = points[b];
    g_temp.quads[q].verts[2] = points[c];
    g_temp.quads[q].verts[3] = points[d];
    return q;
}

/* Make the model of the Bezier head. */
void __cdecl createheadmodel2(int model)
{
    int points[HEAD2_POINTS];
    int i, k, q, n;

    g_temp.num_verts = 0;
    g_temp.num_tris = 0;
    g_temp.num_quads = 0;
    g_temp.num_lines = 0;
    head2_patches();
    for (i = 0; i < HEAD2_SPLITS; i++)
        head2_split();
    head2_place(points);
    head2_remove_tris(points[CUT_B]);
    head2_remove_tris(points[CUT_A]);
    head2_remove_tris(points[M_2]);
    head2_new_points(points);
    for (i = 0; i < (int)(sizeof g_head2_tris / sizeof g_head2_tris[0]); i++) {
        int t = g_temp.num_tris;

        for (k = 0; k < 3; k++)
            g_temp.tris[t].verts[k] = points[g_head2_tris[i][k]];
        model_settritexcoord(t, 1.0f, 0.0f);
        g_temp.num_tris++;
    }
    g_temp.verts[points[M_0]].z = g_temp.verts[points[N_1]].z;
    head2_quad(points, N_0, N_1, N_2, N_3);
    model_extrudeeye2(0, 0);
    head2_quad(points, N_4, N_5, N_6, N_7);
    model_extrudeeye2(0, 0);
    q = head2_quad(points, N_2, N_7, N_8, M_2);
    model_setquadtexcoord(q, 1.0f, 0.0f);
    {
        vec3 depth = {0.0f, 0.0f, -HEAD2_EYE_DEPTH};

        model_extrudequad(0, &depth);
    }
    /* The last four vertices: the first two move down by the depth and take the z of the last
     * two. All four take half their x. */
    n = g_temp.num_verts;
    g_temp.verts[n - 4].y = (float)(g_temp.verts[n - 4].y - HEAD2_EYE_DEPTH);
    g_temp.verts[n - 3].y = (float)(g_temp.verts[n - 3].y - HEAD2_EYE_DEPTH);
    g_temp.verts[n - 4].z = g_temp.verts[n - 1].z;
    g_temp.verts[n - 3].z = g_temp.verts[n - 2].z;
    for (k = 4; k > 0; k--)
        g_temp.verts[n - k].x = (float)(g_temp.verts[n - k].x * 0.5);
    for (q = 0; q < g_temp.num_quads; q++) {
        model_setquadtexcoord(q, 1.0f, 0.0f);
        modeltemp_quadtotris(q);
    }
    for (i = 0; i < g_temp.num_tris; i++)
        for (k = 0; k < 3; k++)
            if (HEAD2_TOP < g_temp.verts[g_temp.tris[i].verts[k]].y)
                model_settritexcoord(i, 1.0f, 0.625f);
    setupmodel(model, 1);
}

/* createheadmodel3 */

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
