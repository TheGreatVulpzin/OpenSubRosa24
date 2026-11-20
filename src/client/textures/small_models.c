/* Small models of boxes: the foot, the rotor, the sunglasses, the key and the cell phone. */
#include "textures/small_models.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"
#include "textures/model_parts.h"
#include "textures/models.h"
#include "vectors.h"

#define g_temp cl_data_modeltemp

/* Game functions of other files. */
#define addboxtomodelquad CL_FN(0x4b2ad0, void(__cdecl *)(const vec3 *center, float x, float y, float z))

#define FOOT_DROP 0.03125f
#define LENS_CENTER_X 0.0421875f
#define LENS_CENTER_Z (-0.125f)
#define LENS_HALF_WIDTH 0.03750000149011612f
#define LENS_HALF_HEIGHT 0.03125f
#define LENS_HALF_DEPTH 0.0010000000474974513f
#define KEY_BLADE_Z (-0.046875f)
#define KEY_BLADE_HALF_X 0.015625f
#define KEY_BLADE_HALF_Y 0.03125f
#define KEY_BLADE_HALF_Z 0.09375f
#define KEY_BLADE_U 0.5f
#define KEY_RING_Z 0.015625f
#define KEY_RING_RADIUS 0.03125f
#define KEY_RING_LENGTH 0.03125f
#define PHONE_BOTTOM 0.75   /* the bottom of the phone is 0.75 of its half height below the middle */
#define PHONE_TOP_RISE 0.25 /* the top pulls up by a quarter of the half height */
#define ANTENNA_Y 0.125f
#define ANTENNA_Z (-0.03125f)
#define ANTENNA_HALF_X 0.015625f
#define ANTENNA_HALF_Y 0.25f
#define ANTENNA_HALF_Z 0.015625f
#define BOX_TRIS 12
#define BOX_CORNERS 8
#define BOX_FACES 6
#define TOP_QUADS 5      /* model_extrudequad adds 4 side quads and moves the top quad */

/* Clear the model in work. */
static void clear_model(void)
{
    g_temp.num_verts = 0;
    g_temp.num_tris = 0;
    g_temp.num_quads = 0;
    g_temp.num_lines = 0;
}

/* Set texture coordinate 0, 0 on the three corners of every triangle. */
static void zero_texcoords(void)
{
    int t, k;

    for (t = 0; t < g_temp.num_tris; t++)
        for (k = 0; k < 3; k++) {
            g_temp.tri_texcoords[t].corners[k].u = 0.0f;
            g_temp.tri_texcoords[t].corners[k].v = 0.0f;
        }
}

/* The foot: a box of half sizes x, y and z, with its back at z 0 and a little below 0 in y. */
void __cdecl createfootmodel(int model, float x, float y, float z)
{
    vec3 center;

    clear_model();
    center.x = 0.0f;
    center.y = -FOOT_DROP;
    center.z = (float)(-z * 0.5);
    addboxtomodel(&center, x, y, z);
    zero_texcoords();
    setupmodel(model, 0);
}

/* The rotor: a box of half sizes x, y and z, with its center at z -z. */
void __cdecl createrotormodel(int model, float x, float y, float z)
{
    vec3 center;

    clear_model();
    center.x = 0.0f;
    center.y = 0.0f;
    center.z = -z;
    addboxtomodel(&center, x, y, z);
    zero_texcoords();
    setupmodel(model, 0);
}

#define HALF_PI_F 1.5707963705062866f
#define LENS_SHIFT 0.00937500037252903f
#define LENS_HALF_SHIFT 0.004687500186264515f

/* The sunglasses: two thin boxes, one for each eye. The outer corners of the front face move
 * out and the inner corners move in, so each lens is narrower at the nose. */
void __cdecl createsunglassmodel(int model)
{
    vec3 center;
    vec3 *v = g_temp.verts;
    int t;

    clear_model();
    center.x = -LENS_CENTER_X;
    center.y = 0.0f;
    center.z = LENS_CENTER_Z;
    addboxtomodel(&center, LENS_HALF_WIDTH, LENS_HALF_HEIGHT, LENS_HALF_DEPTH);
    v[1].x = (float)(v[1].x - LENS_SHIFT);
    v[2].x = (float)(v[2].x - LENS_SHIFT);
    v[0].x = (float)(v[0].x + LENS_HALF_SHIFT);
    v[3].x = (float)(v[3].x + LENS_HALF_SHIFT);
    center.x = LENS_CENTER_X;
    center.y = 0.0f;
    center.z = LENS_CENTER_Z;
    addboxtomodel(&center, LENS_HALF_WIDTH, LENS_HALF_HEIGHT, LENS_HALF_DEPTH);
    v[8].x = (float)(v[8].x + LENS_SHIFT);
    v[11].x = (float)(v[11].x + LENS_SHIFT);
    v[9].x = (float)(v[9].x - LENS_HALF_SHIFT);
    v[10].x = (float)(v[10].x - LENS_HALF_SHIFT);
    for (t = 0; t < g_temp.num_tris; t++)
        model_settritexcoord(t, 0.0f, 0.0f);
    setupmodel(model, 0);
}

/* Split each quad into two flat triangles with the texture coordinates of the quad. */
static void quads_to_tris(void)
{
    int q;

    for (q = 0; q < g_temp.num_quads; q++)
        modeltemp_quadtotris(q);
}

/* The key: a flat box with texture coordinate 0.5, 0 on its quads and a ring across its
 * head. */
void __cdecl createkeymodel(int model)
{
    vec3 center, rot[3];
    int i;

    clear_model();
    center.x = 0.0f;
    center.y = 0.0f;
    center.z = KEY_BLADE_Z;
    addboxtomodelquad(&center, KEY_BLADE_HALF_X, KEY_BLADE_HALF_Y, KEY_BLADE_HALF_Z);
    for (i = 0; i < g_temp.num_quads; i++)
        model_setquadtexcoord(i, KEY_BLADE_U, 0.0f);
    center.x = 0.0f;
    center.y = 0.0f;
    center.z = KEY_RING_Z;
    resetorientation(rot);
    rotateorientation(rot, &rot[1], HALF_PI_F);
    addcylindertomodel(&center, rot, KEY_RING_RADIUS, KEY_RING_LENGTH);
    for (i = 0; i < g_temp.num_tris; i++)
        model_settritexcoord(i, 0.0f, 0.0f);
    quads_to_tris();
    setupmodel(model, 0);
}

/* The cell phone: a box of half sizes x, y and z, narrower at the bottom (0.75 of y), with
 * the top pulled up by a quarter of y and narrowed at the back, and an antenna box. */
void __cdecl createcellphonemodel(int model, float x, float y, float z)
{
    vec3 offset, center;
    int i, n;

    clear_model();
    for (i = 0; i < BOX_CORNERS; i++) {
        vec3 *v = &g_temp.verts[g_temp.num_verts++];

        v->x = (i & 3) == 0 || (i & 3) == 3 ? -x : x;
        v->y = i < 4 ? (float)(-y * PHONE_BOTTOM) : y;
        v->z = (i & 3) == 0 || (i & 3) == 1 ? -z : z;
    }
    for (i = 0; i < BOX_FACES; i++) {
        int q = g_temp.num_quads;

        memcpy(g_temp.quads[q].verts, &sr_data_cubequad[i * 4], sizeof(g_temp.quads[q].verts));
        model_setquadtexcoord(q, 0.0f, 0.0f);
        g_temp.num_quads++;
    }
    offset.x = 0.0f;
    offset.y = (float)(-y * PHONE_TOP_RISE);
    offset.z = 0.0f;
    model_extrudequad(0, &offset);
    n = g_temp.num_verts;
    g_temp.verts[n - 1].x = (float)(g_temp.verts[n - 1].x - x * 0.5);
    g_temp.verts[n - 4].x = (float)(g_temp.verts[n - 4].x - x * 0.5);
    for (i = TOP_QUADS; i > 0; i--)
        model_setquadtexcoord(g_temp.num_quads - i, 0.0f, 0.0f);
    center.x = 0.0f;
    center.y = ANTENNA_Y;
    center.z = ANTENNA_Z;
    addboxtomodel(&center, ANTENNA_HALF_X, ANTENNA_HALF_Y, ANTENNA_HALF_Z);
    for (i = BOX_TRIS; i > 0; i--)
        model_settritexcoord(g_temp.num_tris - i, 1.0f, 0.0f);
    quads_to_tris();
    setupmodel(model, 0);
}
