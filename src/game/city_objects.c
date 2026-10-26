/* City object allocation and collision meshes. */
#include "buildings.h"

#include <string.h>

#include "logic.h"
#include "vectors.h"

#define g_cityobjects sr_data_cityobject
#define g_cityobjecttypes sr_data_cityobjecttype
#define g_debug sr_data_debug
#define g_yaxis (&sr_data_yaxis)
#define g_cubevertex ((float *)&sr_data_cubevertex[0])
#define g_cubequad sr_data_cubequad

int createcityobject(int type, const vec3 *pos, float angle)
{
    int i;
    for (i = 0; i < 1024; i++) {
        CityObjectRec *object = &g_cityobjects[i];
        if (object->active == 0) {
            object->type = type;
            object->active = 1;
            object->pos = *pos;
            object->rot[0] = (vec3){1.0f, 0.0f, 0.0f};
            object->rot[1] = (vec3){0.0f, 1.0f, 0.0f};
            object->rot[2] = (vec3){0.0f, 0.0f, 1.0f};
            rotateorientation(object->rot, g_yaxis, angle);
            return i;
        }
    }
    return -1;
}

int city_createelevator(const vec3 *pos)
{
    int i, building = -1, object;
    int *block;
    vec3 center;
    g_debug.affinity_before = 0x457;
    for (i = 0; i < g_city.num_buildings; i++) {
        const vec3 *min = BLD_PTR(vec3, building_min, i);
        const vec3 *max = BLD_PTR(vec3, building_max, i);
        if (min->x <= pos->x && min->y <= pos->y && min->z <= pos->z &&
            pos->x < max->x && pos->y < max->y && pos->z < max->z)
            building = i;
    }
    if (building == -1)
        return -1;
    object = BLD_INT(elevator, building);
    if (object != -1)
        return object;
    block = BLD_PTR(int, elevator_block, building);
    center.x = (float)(block[0] * 4.0 + 2.0);
    center.y = (float)(block[1] * 4.0 + 2.0);
    center.z = (float)(block[2] * 4.0 + 2.0);
    object = createcityobject(2, &center, 0.0f);
    if (object == -1)
        return -1;
    BLD_INT(elevator, building) = object;
    g_cityobjects[object].building = building;
    g_cityobjects[object].num_floors = BLD_INT(num_floors, building);
    g_debug.affinity_after = 0x457;
    return object;
}

void cityobject_addbox(int type, float x, float y, float z, float sx, float sy, float sz)
{
    CityObjectTypeRec *t = &g_cityobjecttypes[type];
    int base = t->num_verts;
    int k, q, j;
    for (k = 0; k < 8; k++) {
        vec3 *v = &t->verts[t->num_verts];
        v->x = (float)(x + sx * ((double)g_cubevertex[3 * k] - 0.5));
        v->y = (float)(y + sy * ((double)g_cubevertex[3 * k + 1] - 0.5));
        v->z = (float)(z + sz * ((double)g_cubevertex[3 * k + 2] - 0.5));
        t->num_verts++;
    }
    for (q = 0; q < 6; q++) {
        CityFace *face = &t->faces[t->num_faces];
        face->corners = 4;
        for (j = 0; j < 4; j++)
            face->verts[j] = g_cubequad[4 * q + j] + base;
        t->num_faces++;
    }
}

void setupcityobjecttypes(void)
{
    static const struct { int type; float x, y, z, sx, sy, sz; } boxes[] = {
        {9, 0.0f, 2.0f, -1.625f, 4.0f, 4.0f, 0.25f},
        {1, 0.0f, 1.0f, 0.0f, 1.875f, 0.25f, 1.0f},
        {1, 0.6875f, 0.46875f, 0.0f, 0.25f, 1.0f, 1.0f},
        {1, -0.6875f, 0.46875f, 0.0f, 0.25f, 1.0f, 1.0f},
        {1, 0.0f, 0.46875f, 0.3125f, 1.640625f, 1.0f, 0.125f},
        {4, 0.0f, 0.625f, 0.125f, 0.3125f, 0.25f, 0.3125f},
        {4, 0.0f, 0.0f, 0.0f, 0.75f, 1.0f, 0.625f},
        {8, 0.0f, 0.0f, 0.0f, 1.0f, 1.25f, 1.0f},
        {5, 0.0f, 0.0f, 0.0f, 0.375f, 6.0f, 0.375f},
        {5, 0.0f, 3.0f, -5.0f, 0.25f, 0.25f, 10.0f},
        {5, 0.0f, 3.0f, -6.0f, 0.3125f, 1.0f, 0.5f},
        {5, 0.0f, 3.0f, -10.0f, 0.3125f, 1.0f, 0.5f},
    };
    static const vec3 elevator_verts[8] = {
        {-2.0f, -1.875f, -2.0f}, {2.0f, -1.875f, -2.0f},
        {2.0f, -1.875f, 2.0f}, {-2.0f, -1.875f, 2.0f},
        {0.875f, 0.125f, -1.4375f}, {1.625f, 0.125f, -1.4375f},
        {1.625f, -0.625f, -1.4375f}, {0.875f, -0.625f, -1.4375f},
    };
    CityObjectTypeRec *t = &g_cityobjecttypes[2];
    int i, j;
    for (i = 0; i < (int)(sizeof boxes / sizeof boxes[0]); i++)
        cityobject_addbox(boxes[i].type, boxes[i].x, boxes[i].y, boxes[i].z,
                          boxes[i].sx, boxes[i].sy, boxes[i].sz);
    memcpy(&t->verts[t->num_verts], elevator_verts, sizeof elevator_verts);
    t->num_verts += 8;
    for (i = 0; i < 2; i++) {
        CityFace *face = &t->faces[t->num_faces];
        face->corners = 4;
        for (j = 0; j < 4; j++)
            face->verts[j] = i * 4 + j;
        if (i == 1)
            face->grid = 1;
        t->num_faces++;
    }
    t->radius = 5.0f;
    g_cityobjecttypes[9].radius = 3.5f;
    g_cityobjecttypes[1].radius = 3.0f;
    g_cityobjecttypes[1].block_offset = (vec3){0.0f, 0.1875f, 0.0f};
    g_cityobjecttypes[4].radius = 3.0f;
    g_cityobjecttypes[4].block_offset = (vec3){0.0f, 0.75f, -1.0f};
    t = &g_cityobjecttypes[8];
    for (i = 0; i < 4; i++) {
        t->verts[i].x = (float)(t->verts[i].x * 0.75);
        t->verts[i].y = (float)(t->verts[i].y * 0.75);
        t->verts[i].z = (float)(t->verts[i].z * 0.75);
    }
    t->radius = 3.0f;
    t->block_offset = (vec3){0.0f, 0.625f, -1.0f};
    g_cityobjecttypes[5].radius = 3.0f;
    g_cityobjecttypes[5].block_offset = (vec3){0.0f, 3.25f, -1.0f};
}
