/* The models of the guns and of the round items.
 *
 * A gun is a body box with a grip and a front pulled out of it, a trigger box and a barrel as
 * wide as its bullet. The round item is a body of 16 sides with two boxes. Both builders fill
 * the model in work and store it with setupmodel.
 */
#include "textures/item_models.h"

#include <string.h>

#include "client.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "textures/mesh_edit.h"
#include "textures/model_parts.h"
#include "textures/models.h"
#include "vectors.h"
#include "items.h"

#define g_temp cl_data_modeltemp

#define SHADE_SMOOTH (-1)
#define SHADE_CAP 2           /* the normal of corner 2: the point on the axis */
#define BARREL_GAP 0.004999999888241291 /* 0.005f as a double: the barrel is a little wider than the bullet */
#define ROUND_SIDES 16
#define ROUND_SIDE_MASK 15
#define ROUND_STEP 0.39269909262657166 /* pi / 8 rounded to a float */

/* Give the quads from first to the last quad one texture coordinate. */
static void quads_texcoord(int first, float u, float v)
{
    int i;

    for (i = first; i < g_temp.num_quads; i++)
        model_setquadtexcoord(i, u, v);
}

/* The radius of a barrel or a round body for an item type: the caliber of its bullet plus a
 * small gap. */
static float barrel_radius(int item_type)
{
    return (float)(sr_data_bullettypes[sr_data_itemtype[item_type].bullet_type].caliber + BARREL_GAP);
}

/* Make the model of a gun: a body box with a grip pulled down and the front pulled forward in
 * two steps, a trigger box and a barrel as wide as the bullet. Texture u 1 marks the dark
 * parts. Only the AK-47 keeps them: the other guns are one color. The MP5 is shorter. */
void __cdecl creategunmodel2(int model, int item_type)
{
    vec3 place = {0.0f, 0.0f, -0.09375f};
    vec3 rot[3];
    int front, grip, q;

    g_temp.num_verts = 0;
    g_temp.num_tris = 0;
    g_temp.num_quads = 0;
    g_temp.num_lines = 0;
    addboxtomodelquad(&place, 0.0625f, 0.078125f, 0.3125f);
    quads_texcoord(0, 0.0f, 0.0f);
    front = g_temp.num_quads - 3;
    grip = g_temp.num_quads - 5;
    place.x = 0.0f;
    place.y = item_type == ITEM_AK47 ? -0.0625f : -0.03125f;
    place.z = 0.25f;
    model_extrudequadscale(grip, &place, 1.0f, 1.5f, 1.0f);
    model_setquadtexcoord(grip, 1.0f, 0.0f);
    quads_texcoord(g_temp.num_quads - 4, 1.0f, 0.0f);
    place.x = 0.0f;
    place.y = 0.0f;
    place.z = item_type == ITEM_MP5 ? -0.0625f : -0.125f;
    model_extrudequadscale(front, &place, 1.0f, 0.875f, 1.0f);
    model_setquadtexcoord(front, 1.0f, 0.0f);
    quads_texcoord(g_temp.num_quads - 4, 1.0f, 0.0f);
    model_extrudequadscale(front, &place, 0.875f, 0.875f, 1.0f);
    model_setquadtexcoord(front, 0.0f, 0.0f);
    quads_texcoord(g_temp.num_quads - 4, 0.0f, 0.0f);
    place.x = 0.0f;
    place.y = -0.0625f;
    place.z = 0.0625f;
    addboxtomodelquad(&place, 0.046875f, 0.125f, 0.0625f);
    quads_texcoord(g_temp.num_quads - 6, 1.0f, 0.0f);
    if (item_type != ITEM_AK47)
        quads_texcoord(0, 0.0f, 0.0f);
    place.x = 0.0f;
    place.y = 0.0f;
    place.z = item_type == ITEM_MP5 ? -0.0625f : -0.125f;
    resetorientation(rot);
    addcylindertomodel(&place, rot, barrel_radius(item_type), item_type == ITEM_MP5 ? 0.375f : 0.5f);
    for (q = 0; q < g_temp.num_quads; q++)
        modeltemp_quadtotris(q);
    setupmodel(model, 0);
}

/* One ring of 16 vertices around the z axis at the height z. The normal of each vertex points
 * out from the axis, as long as the radius. */
static void round_ring(float radius, float z)
{
    float angle = 0.0f;
    int i;

    for (i = 0; i < ROUND_SIDES; i++) {
        int n = g_temp.num_verts++;

        g_temp.verts[n].x = (float)(x87_cos((double)angle) * radius);
        g_temp.verts[n].y = (float)(-x87_sin((double)angle) * radius);
        g_temp.verts[n].z = z;
        g_temp.vert_normals[n].x = (float)(x87_cos((double)angle) * radius);
        g_temp.vert_normals[n].y = (float)(-x87_sin((double)angle) * radius);
        g_temp.vert_normals[n].z = 0.0f;
        g_temp.vert_texcoords[n].u = 0.0f;
        g_temp.vert_texcoords[n].v = 0.0f;
        angle = (float)(angle + ROUND_STEP);
    }
}

/* Add a vertex on the axis at the height z. It gets no normal: it keeps the normal that the
 * model in work had there before. */
static void axis_point(float z)
{
    int n = g_temp.num_verts++;

    g_temp.verts[n].x = 0.0f;
    g_temp.verts[n].y = 0.0f;
    g_temp.verts[n].z = z;
    g_temp.vert_texcoords[n].u = 0.0f;
    g_temp.vert_texcoords[n].v = 0.0f;
}

/* Make the model of a round item type: a body of 16 sides with the width of its bullet, from 0
 * to the muzzle along z, a back point, and two boxes from the shape sizes of the type. */
void __cdecl createcashmodel(int model, int item_type)
{
    const ItemType *item = &sr_data_itemtype[item_type];
    float radius = barrel_radius(item_type);
    vec3 center;
    int i;

    g_temp.num_verts = 0;
    g_temp.num_tris = 0;
    g_temp.num_quads = 0;
    g_temp.num_lines = 0;
    round_ring(radius, 0.0f);
    round_ring(radius, item->muzzle.z);
    axis_point(0.0f);
    axis_point((float)(-item->shape[1] * 0.125));
    for (i = 0; i < ROUND_SIDES; i++) {
        int next = (i + 1) & ROUND_SIDE_MASK;

        modeltemp_addtri(i, ROUND_SIDES + i, next + ROUND_SIDES, SHADE_SMOOTH);
        modeltemp_addtri(i, next + ROUND_SIDES, next, SHADE_SMOOTH);
        modeltemp_addtri(i, next, ROUND_SIDES * 2, SHADE_CAP);
        modeltemp_addtri(next + ROUND_SIDES, ROUND_SIDES + i, ROUND_SIDES * 2 + 1, SHADE_CAP);
    }
    center.x = 0.0f;
    center.y = -0.015625f;
    center.z = (float)(item->shape[2] * 0.5 + -item->shape[1] * 0.5);
    addboxtomodel(&center, item->shape[0], item->shape[3], (float)((item->shape[2] + item->shape[1]) * 0.5));
    center.x = 0.0f;
    center.y = -0.125f;
    center.z = 0.0f;
    addboxtomodel(&center, 0.03125f, 0.125f, 0.03125f);
    for (i = 0; i < g_temp.num_tris; i++)
        memset(&g_temp.tri_texcoords[i], 0, sizeof(TriTexCoords));
    setupmodel(model, 0);
}
