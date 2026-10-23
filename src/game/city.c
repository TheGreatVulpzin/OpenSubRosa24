/* City intersections and streets. */
#include "city.h"

#include <stdint.h>
#include <string.h>

#include "area.h"
#include "logic.h"
#include "vectors.h"

void city_addintersection(int x, int y, int z)
{
    CityIntersection *intersection = &g_city.intersections[g_city.num_intersections];
    int i;
    intersection->x = x;
    intersection->y = y;
    intersection->z = z;
    for (i = 0; i < 4; i++) {
        intersection->roads[i] = -1;
        intersection->extent[i] = 0;
    }
    intersection->pos.x = (float)((float)x * 4.0);
    intersection->pos.y = (float)((float)(y + 1) * 4.0);
    intersection->pos.z = (float)((float)z * 4.0);
    intersection->light = 0;
    intersection->light_max = 0;
    g_city.num_intersections++;
}

void addcitystreet(int a, int b, int width_a, int width_b, const char *name)
{
    CityIntersection *start = &g_city.intersections[a];
    CityIntersection *end = &g_city.intersections[b];
    int n = g_city.num_roads;
    CityRoad *road = &g_city.roads[n];
    if (end->z - start->z < end->x - start->x) {
        road->kind = 0;
        start->roads[0] = n;
        end->roads[2] = n;
        if (start->extent[1] < width_b) start->extent[1] = width_b;
        if (start->extent[3] < width_a) start->extent[3] = width_a;
        if (end->extent[1] < width_b) end->extent[1] = width_b;
        if (end->extent[3] < width_a) end->extent[3] = width_a;
    } else {
        road->kind = 1;
        start->roads[1] = n;
        end->roads[3] = n;
        if (start->extent[0] < width_a) start->extent[0] = width_a;
        if (start->extent[2] < width_b) start->extent[2] = width_b;
        if (end->extent[0] < width_a) end->extent[0] = width_a;
        if (end->extent[2] < width_b) end->extent[2] = width_b;
    }
    road->dir.x = (float)(end->x - start->x);
    road->dir.y = 0.0f;
    road->dir.z = (float)(end->z - start->z);
    normalizevector(&road->dir, &road->dir);
    road->side.x = -road->dir.z;
    road->side.y = 0.0f;
    road->side.z = road->dir.x;
    road->ends[0] = a;
    road->ends[1] = b;
    road->width[0] = width_a;
    road->width[1] = width_b;
    strcpy(road->name, name);
    g_city.num_roads++;
}

void buildcityinterection(int n)
{
    const CityIntersection *intersection = &g_city.intersections[n];
    uint16_t materials[8] = { 3, 3, 3, 3, 3, 3, 3, 3 };
    int x, z;
    int ix = intersection->x, iz = intersection->z, y = intersection->y + 1;
    int e0 = intersection->extent[0], e1 = intersection->extent[1];
    uint32_t object = (uint32_t)n << 12;
    for (z = iz - intersection->extent[3]; z < iz + intersection->extent[1]; z++) {
        for (x = ix - intersection->extent[2]; x < ix + intersection->extent[0]; x++)
            area_createblock(0, x, intersection->y, z, 0x40000000u, materials);
    }
    if (intersection->roads[2] != -1)
        area_interior_setobject(0, ix + e0, y, iz + e1, (int)(object | 5u));
    if (intersection->roads[0] != -1)
        area_interior_setobject(0, ix - e0 - 1, y, iz - e1 - 1, (int)(object | 0x805u));
    if (intersection->roads[3] != -1)
        area_interior_setobject(0, ix - e0 - 1, y, iz + e1, (int)(object | 0x1000405u));
    if (intersection->roads[1] != -1)
        area_interior_setobject(0, ix + e0, y, iz - e1 - 1, (int)(object | 0x1000c05u));
}

static void city_street_axis(const CityRoad *road, int axis, uint16_t materials[8])
{
    const CityIntersection *a = &g_city.intersections[road->ends[0]];
    const CityIntersection *b = &g_city.intersections[road->ends[1]];
    int start = axis == 0 ? a->x + a->extent[0] : a->z + a->extent[1];
    int base = axis == 0 ? a->z : a->x;
    int length = (axis == 0 ? b->x - b->extent[2] : b->z - b->extent[3]) - start;
    int width_a = road->width[0], width_b = road->width[1];
    int dy = b->y - a->y;
    int height = dy < 0 ? -dy : dy;
    int ramp_start = (length >> 1) - 3 * height;
    int ramp_end = (length >> 1) + 3 * height;
    int y = a->y, step = 0;
    int i, j;
    int left = axis == 0 ? g_materiallist.street_x_left : g_materiallist.street_z_left;
    int right = axis == 0 ? g_materiallist.street_x_right : g_materiallist.street_z_right;

    for (i = 0; i < length; i++) {
        int ramp = ramp_start <= i && i < ramp_end;
        if (ramp && dy > 0 && step % 6 == 0)
            y++;
        materials[0] = 3;
        for (j = -width_a; j < width_b; j++) {
            uint32_t block;
            if (j == -1)
                materials[5] = (uint16_t)(axis == 0 ? 5 : 8);
            else if (j == 0)
                materials[5] = (uint16_t)(axis == 0 ? 4 : 7);
            else if (j < -1)
                materials[5] = j <= -width_a ? 3 : (uint16_t)left;
            else
                materials[5] = j >= width_b - 1 ? 3 : (uint16_t)right;
            if (!ramp) {
                block = 0x40000000u;
            } else {
                int row = dy < 1 ? axis + 2 : axis;
                int index = dy < 1 ? 5 - step : step;
                materials[0] = materials[5];
                block = (uint32_t)g_blocklist.slope6[row * 6 + index] | 0x80000000u;
            }
            area_createblock(0, axis == 0 ? start + i : base + j, y,
                             axis == 0 ? base + j : start + i, block, materials);
        }
        if (ramp) {
            if (dy < 0 && step % 6 == 5)
                y--;
            step = (step + 1) % 6;
        }
    }
    /* Caps keep the material of the last row and overwrite the end rows. */
    for (j = -width_a; j < width_b; j++) {
        int end = axis == 0 ? b->x - b->extent[2] - 1 : b->z - b->extent[3] - 1;
        materials[5] = (uint16_t)(axis == 0 ? 9 : 6);
        area_createblock(0, axis == 0 ? start : base + j, a->y,
                         axis == 0 ? base + j : start, 0x40000000u, materials);
        materials[5] = (uint16_t)(axis == 0 ? 9 : 6);
        area_createblock(0, axis == 0 ? end : base + j, b->y,
                         axis == 0 ? base + j : end, 0x40000000u, materials);
    }
}

void buildcitystreet(int n)
{
    const CityRoad *road = &g_city.roads[n];
    /* Empty streets still give each cap a defined material. */
    uint16_t materials[8] = { 3, 3, 3, 3, 3, 3, 3, 3 };
    if (road->kind == 0)
        city_street_axis(road, 0, materials);
    if (road->kind == 1)
        city_street_axis(road, 1, materials);
}
