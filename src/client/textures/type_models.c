/* The models of the object types and of the bullets.
 *
 * The model of an object type (a car, a helicopter, a train part) takes its vertices from the
 * particles and the quad points of the type, and its triangles from the faces. A window face
 * becomes a window record of the model, not a triangle. Each vertex and each corner gets a
 * smooth normal, and each vertex a tangent. The model of a bullet is a short round body with a
 * point at each end.
 */
#include "textures/type_models.h"

#include <math.h>
#include <string.h>

#include "client.h"
#include "math/small_vectors.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "textures/models.h"
#include "vectors.h"

#define g_temp cl_data_modeltemp
#define g_models cl_data_model

#define FACE_TRI 3
#define FACE_QUAD 4
#define SHADE_SMOOTH (-1)
#define SHADE_TIP 2         /* the normal of corner 2: the point at the end of a bullet */
#define TYPE_LAST_CAR 6     /* types 0 to 6 are the cars */
#define TYPE_MINIVAN 8      /* 7 is the van, 8 the minivan */
#define TYPE_TRUCK 9
#define TYPE_HELICOPTER 11
#define TYPE_TRAIN 12
#define TANGENT_LIMIT 0.707f
#define FLOOR_LIMIT 0.9f
#define FLOOR_U 0.75f
#define WINDOW_INSET 0.975f
#define BULLET_SIDES 8
#define BULLET_SIDE_MASK 7
#define FULL_TURN 6.28318530718

/* Game functions of other files. */
#define addcockpittomodel CL_FN(0x4b0af0, void(__cdecl *)(int *corners))

/* The eight vertices of the cabin of each kind of type. The helicopter uses the car cabin. */
static const int g_cabin_car[8] = {10, 11, 13, 12, 16, 17, 19, 18};
static const int g_cabin_van[8] = {8, 9, 11, 10, 12, 13, 15, 14};
static const int g_cabin_truck[8] = {12, 13, 15, 14, 16, 17, 19, 18};

/* The vertices: each particle, then each quad point (a weighted sum of 4 particles), relative
 * to the center. The quad points start at the index num_particles of the quads. */
static void type_verts(const ObjectType *type, const vec3 *center)
{
    int i, k;

    for (i = 0; i < type->num_particles; i++)
        subtractvectors(&g_temp.verts[g_temp.num_verts++], &type->particles[i].pos, center);
    for (i = type->num_particles; i < type->num_quads; i++) {
        const TypeQuad *quad = &type->quads[i];
        vec3 point;

        scalevector(&point, &type->particles[quad->particles[0]].pos, quad->weights[0]);
        for (k = 1; k < 4; k++)
            scaleaddvectors(&point, &point, &type->particles[quad->particles[k]].pos, quad->weights[k]);
        subtractvectors(&g_temp.verts[g_temp.num_verts++], &point, center);
    }
}

/* The triangles: first the triangle faces, then two triangles for each quad face that is not
 * a window. The cars, the vans and the helicopter also close the last face from the back. */
static void type_tris(const ObjectType *type, int type_index)
{
    int i;

    for (i = 0; i < type->num_faces; i++) {
        const TypeFace *face = &type->faces[i];

        if (face->corners == FACE_TRI)
            modeltemp_addtri(face->verts[0], face->verts[1], face->verts[2], SHADE_SMOOTH);
    }
    for (i = 0; i < type->num_faces; i++) {
        const TypeFace *face = &type->faces[i];

        if (face->corners != FACE_QUAD || face->is_window == 1)
            continue;
        modeltemp_addtri(face->verts[0], face->verts[1], face->verts[2], SHADE_SMOOTH);
        modeltemp_addtri(face->verts[0], face->verts[2], face->verts[3], SHADE_SMOOTH);
        if (((type_index >= 0 && type_index <= TYPE_MINIVAN) || type_index == TYPE_HELICOPTER) &&
            i == type->num_faces - 1) {
            modeltemp_addtri(face->verts[0], face->verts[2], face->verts[1], SHADE_SMOOTH);
            modeltemp_addtri(face->verts[0], face->verts[3], face->verts[2], SHADE_SMOOTH);
        }
    }
}

/* Add the cabin from a copy of the corner list. */
static void type_cabin(const int *vertices)
{
    int corners[8];

    memcpy(corners, vertices, sizeof(corners));
    addcockpittomodel(corners);
}

/* The cabin of a car, a van, the truck or the helicopter. */
static void type_cockpit(int type_index)
{
    if (type_index >= 0 && type_index <= TYPE_LAST_CAR)
        type_cabin(g_cabin_car);
    if (type_index > TYPE_LAST_CAR && type_index <= TYPE_MINIVAN)
        type_cabin(g_cabin_van);
    if (type_index == TYPE_TRUCK)
        type_cabin(g_cabin_truck);
    if (type_index == TYPE_HELICOPTER)
        type_cabin(g_cabin_car);
}

/* The normal of each corner: the sum of the normals of the triangles that share its edge
 * (each shared edge adds both triangles to both corners), made unit length. */
static void type_corner_normals(void)
{
    int a, b, e, f;

    for (a = 0; a < g_temp.num_tris * 3; a++)
        zerovector((unsigned int *)&g_temp.corner_normals[a]);
    for (a = 0; a < g_temp.num_tris; a++)
        for (b = a + 1; b < g_temp.num_tris; b++)
            for (e = 0; e < 3; e++)
                for (f = 0; f < 3; f++) {
                    vec3 *corner_a = &g_temp.corner_normals[a * 3 + e];
                    vec3 *corner_b = &g_temp.corner_normals[b * 3 + f];

                    if (!modeltempvertsequal(g_temp.tris[a].verts[e], g_temp.tris[b].verts[(f + 1) % 3]) ||
                        !modeltempvertsequal(g_temp.tris[a].verts[(e + 1) % 3], g_temp.tris[b].verts[f]))
                        continue;
                    addvectors(corner_a, corner_a, &g_temp.tri_normals[a]);
                    addvectors(corner_b, corner_b, &g_temp.tri_normals[a]);
                    addvectors(corner_a, corner_a, &g_temp.tri_normals[b]);
                    addvectors(corner_b, corner_b, &g_temp.tri_normals[b]);
                }
    for (a = 0; a < g_temp.num_tris * 3; a++)
        normalizevector(&g_temp.corner_normals[a], &g_temp.corner_normals[a]);
}

/* The tangent of each vertex: the unit cross of its normal and the up axis, or the east axis
 * when the normal points mostly up or down. */
static void type_tangents(void)
{
    int i;

    for (i = 0; i < g_temp.num_verts; i++) {
        vec3 normal = g_temp.vert_normals[i], tangent;

        crossproduct(&tangent, &normal, fabs((double)normal.y) <= (double)TANGENT_LIMIT ? &sr_data_yaxis : &sr_data_xaxis);
        normalizevector(&g_temp.vert_tangents[i], &tangent);
    }
}

/* Texture coordinate 0 for each vertex and corner. On a train part, a triangle that faces up
 * takes the u of the floor. */
static void type_texcoords(int type_index)
{
    int i, k;

    for (i = 0; i < g_temp.num_tris; i++) {
        float up = g_temp.tri_normals[i].y;

        for (k = 0; k < 3; k++) {
            g_temp.vert_texcoords[g_temp.tris[i].verts[k]].u = 0.0f;
            g_temp.vert_texcoords[g_temp.tris[i].verts[k]].v = 0.0f;
        }
        for (k = 0; k < 3; k++) {
            g_temp.tri_texcoords[i].corners[k].u = 0.0f;
            g_temp.tri_texcoords[i].corners[k].v = 0.0f;
        }
        if (type_index == TYPE_TRAIN && up > FLOOR_LIMIT)
            for (k = 0; k < 3; k++)
                g_temp.tri_texcoords[i].corners[k].u = FLOOR_U;
    }
}

/* A window record for each window face: the four corners, each moved along its link by the
 * link weight, and pulled in a little to the center (not on a train part). */
static void type_windows(Model *model, const ObjectType *type, int type_index)
{
    int i, k;

    model->num_windows = 0;
    for (i = 0; i < type->num_faces; i++) {
        const TypeFace *face = &type->faces[i];
        ModelWindow *window;

        if (face->corners != FACE_QUAD || face->is_window != 1)
            continue;
        window = &model->windows[model->num_windows];
        window->face = i;
        for (k = 0; k < 4; k++) {
            vec3 corner = g_temp.verts[face->verts[k]];

            if (face->links[k] != -1) {
                subtractvectors(&corner, &g_temp.verts[face->links[k]], &corner);
                scaleaddvectors(&corner, &g_temp.verts[face->verts[k]], &corner, face->link_weights[k]);
            }
            if (type_index != TYPE_TRAIN)
                scalevector(&corner, &corner, WINDOW_INSET);
            window->corners[k] = corner;
        }
        model->num_windows++;
    }
}

/* Make the model of an object type. A type with no particles makes nothing. */
void __cdecl createtypemodel(int model, int type_index)
{
    const ObjectType *type = &sr_data_objecttype[type_index];
    Model *m = &g_models[model];
    vec3 center;
    int i;

    if (type->num_particles == 0)
        return;
    zerovector((unsigned int *)&center);
    for (i = 0; i < type->num_particles; i++)
        addvectors(&center, &center, &type->particles[i].pos);
    scalevector(&center, &center, (float)(1.0 / (float)type->num_particles));
    m->center = center;
    g_temp.num_verts = 0;
    g_temp.num_tris = 0;
    g_temp.num_lines = 0;
    type_verts(type, &center);
    type_tris(type, type_index);
    type_cockpit(type_index);
    g_temp.num_lines = 0;
    for (i = 0; i < type->num_lines; i++)
        g_temp.lines[g_temp.num_lines++] = type->lines[i];
    modeltemp_smoothnormals();
    type_corner_normals();
    type_tangents();
    type_texcoords(type_index);
    type_windows(m, type, type_index);
    setupmodel(model, 1);
}

/* One ring of the bullet at the height z: 8 vertices around the axis with the caliber as the
 * radius. */
static void bullet_ring(float radius, float z)
{
    float angle = 0.0f;
    int i;

    for (i = 0; i < BULLET_SIDES; i++) {
        int n = g_temp.num_verts++;

        g_temp.verts[n].x = (float)(x87_cos((double)angle) * radius);
        g_temp.verts[n].y = (float)(-x87_sin((double)angle) * radius);
        g_temp.verts[n].z = z;
        g_temp.vert_texcoords[n].u = 0.0f;
        g_temp.vert_texcoords[n].v = 0.0f;
        angle = (float)(FULL_TURN / (float)BULLET_SIDES + angle);
    }
}

/* Add a point on the axis at the height z. */
static void bullet_point(float z)
{
    int n = g_temp.num_verts++;

    g_temp.verts[n].x = 0.0f;
    g_temp.verts[n].y = 0.0f;
    g_temp.verts[n].z = z;
    g_temp.vert_texcoords[n].u = 0.0f;
    g_temp.vert_texcoords[n].v = 0.0f;
}

/* Make the model of a bullet type: two rings of 8 vertices along the z axis, a tip at the
 * front and a point at the back, with smooth normals. flat_back puts the back point a quarter
 * of the length behind the middle, else three quarters. */
void __cdecl createbulletmodel(int model, int type, int flat_back)
{
    const BulletType *bullet = &sr_data_bullettypes[type];
    float radius = bullet->caliber, length = bullet->length;
    int i;

    g_temp.num_verts = 0;
    g_temp.num_tris = 0;
    g_temp.num_quads = 0;
    g_temp.num_lines = 0;
    bullet_ring(radius, (float)(length * 0.5));
    bullet_ring(radius, (float)(-length * 0.5));
    bullet_point((float)(length * 0.5));
    bullet_point(flat_back == 0 ? (float)(-length * 0.5 * 1.5) : (float)(-length * 0.25));
    for (i = 0; i < BULLET_SIDES; i++) {
        int next = (i + 1) & BULLET_SIDE_MASK;

        modeltemp_addtri(i, BULLET_SIDES + i, next + BULLET_SIDES, SHADE_SMOOTH);
        modeltemp_addtri(i, next + BULLET_SIDES, next, SHADE_SMOOTH);
        modeltemp_addtri(i, next, BULLET_SIDES * 2, SHADE_TIP);
        modeltemp_addtri(next + BULLET_SIDES, BULLET_SIDES + i, BULLET_SIDES * 2 + 1, SHADE_SMOOTH);
    }
    for (i = 0; i < g_temp.num_tris; i++)
        memset(&g_temp.tri_texcoords[i], 0, sizeof(TriTexCoords));
    modeltemp_smoothnormals();
    setupmodel(model, 0);
}
