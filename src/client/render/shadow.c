/* The shadow volume of a model.
 *
 * The light comes along a direction. Every triangle that faces away from the light pushes its
 * three corners 512 units along the light. The pushed shape closes with a side quad on each
 * edge that borders a triangle facing the light (or has no neighbor), a cap at the front and a cap at
 * the back. The triangles go into one list that the stencil pass draws.
 */
#include "render/shadow.h"

#include "client.h"
#include "setup/timers.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"

#define g_models cl_data_model
#define g_timerlist cl_data_timerlist
#define g_light_position cl_data_light_position
#define g_corners cl_data_shadow_corners
#define g_count cl_data_shadow_count
#define g_list cl_data_shadow_list

/* Game functions of other files. */
#define setupobjectrender CL_FN(0x5066b0, void(__cdecl *)(const void *placement, const void *rotation))

GL_IMPORT(glPushMatrix, (void), ())
GL_IMPORT(glPopMatrix, (void), ())
GL_IMPORT(glDrawArrays, (uint32_t mode, int first, int count), (mode, first, count))

enum { SHADOW_DEPTH_SIGN = -512 };

/* Add one point to the list of the shadow. */
static void add_point(const vec3 *point)
{
    g_list[g_count++] = *point;
}

/* Add the two triangles of the side quad over one edge of a triangle. The edge goes from
 * corner a to corner b, and ra and rb are the same corners pushed along the light. */
static void add_side(const vec3 *a, const vec3 *b, const vec3 *ra, const vec3 *rb)
{
    add_point(a);
    add_point(ra);
    add_point(rb);
    add_point(a);
    add_point(rb);
    add_point(b);
}

/* Tell if the edge needs a side quad: the neighbor is missing, or it faces away from the
 * light (its normal points along the light). */
static int edge_is_open(const Model *model, int neighbor, const vec3 *light)
{
    if (neighbor == -1)
        return 1;
    return !(dotproduct(&model->tri_normals[neighbor], light) <= 0.0f);
}

/* Build the shadow volume of a model for the light, and draw it. The light direction is turned
 * into the object's own axes with the three rows of the rotation. */
void __cdecl rendermodelshadow(int model, const void *placement, const vec3 rotation[3])
{
    const Model *record = &g_models[model];
    float push = (float)SHADOW_DEPTH_SIGN;
    vec3 light;
    int triangle;

    starttimer(g_timerlist.rendermodelshadow);
    light.x = dotproduct((const vec3 *)g_light_position, &rotation[0]);
    light.y = dotproduct((const vec3 *)g_light_position, &rotation[1]);
    light.z = dotproduct((const vec3 *)g_light_position, &rotation[2]);
    g_count = 0;
    for (triangle = 0; triangle < record->num_tris; ++triangle) {
        const MeshTri *indexes = &record->tris[triangle];
        const MeshTri *neighbors = &record->tri_neighbors[triangle];
        const char *verts = (const char *)record->render_verts;
        int stride = record->vertex_floats * 4;
        const vec3 *corner[3];
        int k;

        if (!(dotproduct(&light, &record->tri_normals[triangle]) <= 0.0f))
            continue;
        for (k = 0; k < 3; ++k) {
            corner[k] = (const vec3 *)(verts + indexes->verts[k] * stride);
            scaleaddvectors(&g_corners[k], corner[k], &light, push);
            g_corners[3 + k] = *corner[k];
            scaleaddvectors(&g_corners[3 + k], &g_corners[3 + k], &light, 0.0f);
        }
        if (edge_is_open(record, neighbors->verts[0], &light))
            add_side(&g_corners[3], &g_corners[4], &g_corners[0], &g_corners[1]);
        if (edge_is_open(record, neighbors->verts[1], &light))
            add_side(&g_corners[4], &g_corners[5], &g_corners[1], &g_corners[2]);
        if (edge_is_open(record, neighbors->verts[2], &light))
            add_side(&g_corners[5], &g_corners[3], &g_corners[2], &g_corners[0]);
        add_point(&g_corners[3]);
        add_point(&g_corners[4]);
        add_point(&g_corners[5]);
        add_point(&g_corners[0]);
        add_point(&g_corners[2]);
        add_point(&g_corners[1]);
    }
    glPushMatrix();
    setupobjectrender(placement, rotation);
    glVertexPointer(3, GL_FLOAT, sizeof(vec3), g_list);
    glDrawArrays(GL_TRIANGLES, 0, g_count);
    glPopMatrix();
    stoptimer(g_timerlist.rendermodelshadow);
}
