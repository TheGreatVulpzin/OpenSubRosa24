/* Level, landscape and track construction. */
#include "level.h"

#include <stdint.h>
#include <string.h>

#include "area.h"
#include "city.h"
#include "logic.h"
#include "random.h"
#include "sr_math.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "sr_types.h"
#include "vectors.h"

#define g_track SR_AT(TrackPiece, 0x2597efc0)
#define g_numoftracks (*SR_AT(int, 0x90dc520))
#define g_land_origin (*SR_AT(vec3, 0x9c1d8c0))
#define g_land_height SR_AT(float, 0x9c1d8cc)
#define g_level_editor (*SR_AT(LevelEditor, SR_RVA_LEVEL_EDITOR))
#define g_building (*SR_AT(Building, SR_RVA_BUILDING))
#define g_level_marks SR_AT(uint8_t, 0x12a80c44)

/* Game functions of other files. */
void __cdecl lf_0f2b0(uint32_t area, const vec3 *origin, int size_x, int size_y, int size_z, float scale);
void __cdecl lf_3da50(uint32_t area, uint32_t x, uint32_t y, uint32_t z,
                     uint32_t block, const uint16_t *materials);
void __cdecl lf_296f0(uint32_t area, uint32_t x, uint32_t y, uint32_t z, int block);
void __cdecl lf_13d80(const char *name, int flags);
void __cdecl lf_03d70(void);
void __cdecl lf_041c0(void);
void __cdecl lf_04450(void);
int *__cdecl lf_46c30(uint32_t area, int x, int y, int z,
                     int size_x, int size_z, int floors, int kind);
#define createarea lf_0f2b0
#define area_createblock lf_3da50
#define area_setblock lf_296f0
#define building_load lf_13d80
#define building_setupbuild lf_03d70
#define building_setupfloorplannewer lf_041c0
#define building_rotatefloorplan lf_04450
#define building_buildfloorplan lf_46c30

static float track_span(const vec3 *a, const vec3 *b, int z_first)
{
    float x = (float)(a->x - b->x);
    float z = (float)(a->z - b->z);
    float square;
    if (z_first)
        square = (float)(z * z + (x * x + 0.0));
    else
        square = (float)(x * x + 0.0 + z * z);
    return (float)x87_sqrt(square);
}

static void track_triangle(TrackPiece *track, int a, int b, int c)
{
    int n = track->num_tris;
    track->tris[n][0] = a;
    track->tris[n][1] = b;
    track->tris[n][2] = c;
    track->num_tris++;
}

void createtrack(int type, const float *p, float drop_end, float rise_start)
{
    const vec3 *points = (const vec3 *)p;
    TrackPiece *track = &g_track[g_numoftracks];
    vec3 s, a1, a2, b1, b2, v;
    float c, t;
    int i, j, axis, side;

    track->type = type;
    track->num_verts = 0;
    track->num_tris = 0;
    for (axis = 0; axis < 3; axis++) {
        float *pos = (float *)&track->pos;
        pos[axis] = (float)(p[3 + axis] + p[axis]);
    }
    for (axis = 0; axis < 3; axis++) {
        float *pos = (float *)&track->pos;
        pos[axis] = (float)(p[6 + axis] + pos[axis]);
    }
    for (axis = 0; axis < 3; axis++) {
        float *pos = (float *)&track->pos;
        pos[axis] = (float)(pos[axis] + p[9 + axis]);
    }
    for (axis = 0; axis < 3; axis++) {
        float *pos = (float *)&track->pos;
        pos[axis] = (float)(pos[axis] * 0.25);
    }
    if (track->pos.x >= 2048.0f) {
        float y = track->pos.y;
        for (i = -3; i <= 3; i++) {
            float z = (float)(track->pos.z + (float)i * 8.0);
            uint32_t iz = (uint32_t)(int64_t)(((double)z - g_land_origin.z) * 0.125);
            for (j = -3; j <= 3; j++) {
                float x = (float)((float)j * 8.0 + track->pos.x);
                uint32_t ix = (uint32_t)(int64_t)(((double)x - g_land_origin.x) * 0.125);
                if (ix < 0x800 && iz < 0x800)
                    g_land_height[iz * 0x801 + ix] = (float)(y - 0.25);
            }
        }
    }
    s.x = (float)(points[0].z - points[1].z);
    s.y = 0.0f;
    s.z = -(float)(points[0].x - points[1].x);
    normalizevector(&s, &s);
    c = (float)(track_span(&points[3], &points[0], 1) / 3.0);
    a1.x = (float)(c * s.x + points[0].x);
    a1.y = (float)(rise_start / 3.0 + (float)(c * s.y + points[0].y));
    a1.z = (float)(c * s.z + points[0].z);
    c = (float)(track_span(&points[2], &points[1], 0) / 3.0);
    b1.x = (float)(c * s.x + points[1].x);
    b1.y = (float)((float)(c * s.y + points[1].y) + rise_start / 3.0);
    b1.z = (float)(c * s.z + points[1].z);
    s.x = (float)(points[2].z - points[3].z);
    s.y = 0.0f;
    s.z = -(float)(points[2].x - points[3].x);
    normalizevector(&s, &s);
    c = (float)(track_span(&points[1], &points[2], 0) / 3.0);
    b2.x = (float)(c * s.x + points[2].x);
    b2.y = (float)((float)(c * s.y + points[2].y) - drop_end / 3.0);
    b2.z = (float)(c * s.z + points[2].z);
    c = (float)(track_span(&points[0], &points[3], 0) / 3.0);
    a2.x = (float)(c * s.x + points[3].x);
    a2.y = (float)((float)(c * s.y + points[3].y) - drop_end / 3.0);
    a2.z = (float)(c * s.z + points[3].z);

    t = 0.0f;
    for (i = 0; i < 8; i++) {
        if (i == 0) {
            v = points[0];
        } else if (i == 7) {
            v = points[3];
        } else {
            double u = (float)(1.0 - t);
            double q = t;
            for (axis = 0; axis < 3; axis++)
                ((float *)&v)[axis] = (float)(q * p[9 + axis] * q * q
                    + (((float *)&a2)[axis] * 3.0 * u * q * q
                    + (u * p[axis] * u * u
                    + q * (u * (u * (((float *)&a1)[axis] * 3.0))))));
        }
        track->verts[track->num_verts++] = v;
        t = (float)(t + 0.1428571492433548);
    }
    t = 0.0f;
    for (i = 0; i < 8; i++) {
        if (i == 0) {
            v = points[1];
        } else if (i == 7) {
            v = points[2];
        } else {
            double u = (float)(1.0 - t);
            double q = t;
            v.x = (float)(points[2].x * q * q * q + (b2.x * 3.0 * u * q * q
                + (points[1].x * u * u * u + q * (u * (u * (b1.x * 3.0))))));
            v.y = (float)(points[2].y * q * q * q + (b2.y * 3.0 * u * q * q
                + (b1.y * 3.0 * u * u * q + u * (u * (u * points[1].y)))));
            v.z = (float)(points[2].z * q * q * q + (b2.z * 3.0 * u * q * q
                + (b1.z * 3.0 * u * u * q + u * (u * (u * points[1].z)))));
        }
        track->verts[track->num_verts++] = v;
        t = (float)(t + 0.1428571492433548);
    }
    for (side = 0; side < 2; side++) {
        for (i = 0; i < 8; i++) {
            const vec3 *a = &track->verts[i + side * 8];
            const vec3 *b = &track->verts[i + (1 - side) * 8];
            if (type == 0) {
                v.x = a->x;
                v.y = (float)(a->y + 0.5);
                v.z = a->z;
            } else if (type == 1) {
                float dx = (float)(a->x - b->x), dz = (float)(a->z - b->z);
                float length = (float)x87_sqrt((float)(dx * dx + 0.0 + dz * dz));
                float dy, dz2, inv;
                if (length == 0.0f) {
                    dx = dy = dz2 = 0.0f;
                } else {
                    inv = (float)(1.0 / length);
                    dx = (float)(inv * dx);
                    dy = (float)(inv * 0.0);
                    dz2 = (float)(inv * dz);
                }
                v.x = (float)(a->x + dx * 2.0);
                v.y = (float)((float)(dy * 2.0 + a->y) - 0.5);
                v.z = (float)(dz2 * 2.0 + a->z);
            }
            /* A track of another type repeats the last corner. */
            track->verts[track->num_verts++] = v;
        }
    }
    for (i = 9; i <= 15; i++) {
        track_triangle(track, i - 9, i - 1, i);
        track_triangle(track, i - 9, i, i - 8);
    }
    for (i = 1; i <= 7; i++) {
        track_triangle(track, i + 15, i - 1, i);
        track_triangle(track, i + 15, i, i + 16);
    }
    for (i = 8; i <= 14; i++) {
        track_triangle(track, i, i + 16, i + 17);
        track_triangle(track, i, i + 17, i + 1);
    }
    for (i = 0; i < 8; i++) {
        const vec3 *a = &track->verts[i], *b = &track->verts[i + 8];
        float dx = (float)(a->x - b->x), dy = (float)(a->y - b->y);
        float dz = (float)(a->z - b->z);
        float length, inv;
        track->center[i].x = (float)((float)(a->x + b->x) * 0.5);
        track->center[i].y = (float)((float)(a->y + b->y) * 0.5);
        track->center[i].z = (float)((float)(a->z + b->z) * 0.5);
        length = (float)x87_sqrt((float)(dy * dy + dx * dx + dz * dz));
        if (length == 0.0f) {
            track->across[i].x = track->across[i].y = track->across[i].z = 0.0f;
        } else {
            inv = (float)(1.0 / length);
            track->across[i].x = (float)(inv * dx);
            track->across[i].y = (float)(inv * dy);
            track->across[i].z = (float)(inv * dz);
        }
    }
    track->box_min.x = track->box_min.y = track->box_min.z = 16777216.0f;
    track->box_max.x = track->box_max.y = track->box_max.z = -16777216.0f;
    for (i = 0; i < track->num_verts; i++) {
        const float *vertex = (const float *)&track->verts[i];
        float *min = (float *)&track->box_min, *max = (float *)&track->box_max;
        for (axis = 0; axis < 3; axis++) {
            if (vertex[axis] < min[axis]) min[axis] = vertex[axis];
            if (max[axis] < vertex[axis]) max[axis] = vertex[axis];
        }
    }
    g_numoftracks++;
}

/* The mean angle keeps double precision. */
void buildsomeshit(const float *start)
{
    vec3 p = { start[0], start[1], start[2] };
    float a0 = -1.5707963705062866f, a1 = a0;
    int u, n;
    g_numoftracks = 0;
    for (u = -21, n = 145; n != 0; u++, n--) {
        float corners[12], s, c, c0, ns0, c1, ns1;
        double mean;
        vec3 q;
        a1 = a0;
        if ((uint32_t)(u + 13) <= 10) a1 = (float)(a1 + 0.39269908169875);
        if ((uint32_t)u <= 2) a1 = (float)(a1 - 0.39269908169875);
        if ((uint32_t)(u - 54) <= 2) a1 = (float)(a1 - 0.39269908169875);
        if ((uint32_t)(u - 59) <= 10) a1 = (float)(a1 + 0.39269908169875);
        mean = a1 * 0.5 + a0 * 0.5;
        s = (float)x87_sin(mean);
        c = (float)x87_cos(mean);
        q.x = (float)(p.x + s * 32.0);
        q.y = (float)(p.y + 0.0);
        q.z = (float)(c * 32.0 + p.z);
        c0 = (float)x87_cos(a0);
        ns0 = (float)-x87_sin(a0);
        corners[0] = (float)(p.x - c0 * 1.0);
        corners[1] = (float)(p.y + -0.0);
        corners[2] = (float)(p.z - ns0 * 1.0);
        corners[3] = (float)(p.x + c0);
        corners[4] = (float)(p.y + 0.0);
        corners[5] = (float)(ns0 + p.z);
        c1 = (float)x87_cos(a1);
        ns1 = (float)-x87_sin(a1);
        corners[6] = (float)(c1 + q.x);
        corners[7] = (float)(q.y + 0.0);
        corners[8] = (float)(ns1 + q.z);
        corners[9] = (float)(q.x - c1 * 1.0);
        corners[10] = (float)(q.y + -0.0);
        corners[11] = (float)(q.z - ns1 * 1.0);
        createtrack(0, corners, 0.0f, 0.0f);
        a0 = a1;
        p = q;
    }
    g_game_world.spawn.x = (float)(g_track[74].pos.x - 8.0);
    g_game_world.spawn.y = (float)(g_track[74].pos.y + 2.0);
    g_game_world.spawn.z = (float)(g_track[74].pos.z + 4.0);
    g_game_world.spawn_size_x = 16.0f;
    g_game_world.spawn_size_y = 4.0f;
    g_game_world.spawn_size_z = 16.0f;
}

static float level_blend_weight(float value)
{
    if (0.0 <= value) {
        if (1.0 < value)
            value = 1.0f;
    } else {
        value = 0.0f;
    }
    return value;
}

void createlandscape(void)
{
    int x, z;
    g_land_origin.x = -4096.0f;
    g_land_origin.z = -4096.0f;
    for (z = 0; z <= 0x800; z++) {
        for (x = 0; x <= 0x800; x++) {
            float dx = (float)(x - 640), dz = (float)(z - 640);
            float distance = (float)x87_sqrt((float)(dx * dx + dz * dz));
            float fall = (float)((float)(distance * 0.0078125) - 1.5);
            float fx = (float)((float)x * 0.125), fz = (float)((float)z * 0.125);
            float n1, n2, n3, n4, n5, f2;
            float *height = &g_land_height[z * 0x801 + x];
            fall = (float)(fall * 0.5);
            if (0.0 <= fall) {
                if (3.0 < fall)
                    fall = 3.0f;
            } else {
                fall = 0.0f;
            }
            n1 = serpnoise2d((float)(fx * 0.5), (float)(fz * 0.5), 0xffffffffu);
            n2 = serpnoise2d(fx, fz, 0xffffffffu);
            n3 = serpnoise2d((float)(fx * 0.25), (float)(fz * 0.25), 0xffffffffu);
            n4 = serpnoise2d((float)(fx * 0.125), (float)(fz * 0.125), 0xffffffffu);
            n5 = serpnoise2d((float)(fx * 0.125 * 0.5), (float)(fz * 0.125 * 0.5), 0xffffffffu);
            f2 = (float)(fall * fall);
            *height = (float)((n5 + f2 * 0.25) * 32.0 * f2
                + (float)((n4 + f2) * 16.0
                + (float)(n3 * 8.0 + (float)((float)(n2 + n2 + 23.75) + (float)(n1 * 4.0)))));
            if ((uint32_t)(z - 0x1f1) < 0x11f && (uint32_t)(x - 0x1f1) < 0x11f) {
                float t = -1.0f, f;
                if (z <= 0x200) {
                    f = level_blend_weight((float)((float)(0x200 - z) * 0.0625));
                    if (-1.0 < f) t = f;
                }
                if (z >= 0x300) {
                    f = level_blend_weight((float)((float)(z - 0x300) * 0.0625));
                    if (t < f) t = f;
                }
                if (x <= 0x200) {
                    f = level_blend_weight((float)((float)(0x200 - x) * 0.0625));
                    if (t < f) t = f;
                }
                if (x >= 0x300) {
                    f = level_blend_weight((float)((float)(x - 0x300) * 0.0625));
                    if (t < f) t = f;
                }
                if (-1.0 < t)
                    *height = (float)((1.0 - t) * 23.75 + (double)*height * t);
            }
            if (x >= 0x201 && x <= 0x2ff && z >= 0x201 && z <= 0x2ff) {
                *height = 16.0f;
                if (z == 0x201 || x == 0x201 || z == 0x2ff || x == 0x2ff)
                    *height = 20.0f;
            }
        }
    }
}

static void level_materials(uint16_t materials[8], uint16_t value)
{
    int i;
    for (i = 0; i < 8; i++)
        materials[i] = value;
}

static void level_fix_ground(int x, int z)
{
    uint32_t word = area_getblock(0, x, 5, z);
    if ((word & 0xe0000000u) == 0x40000000u)
        area_setblock(0, x, 5, z, (int)(word & 0xffffffdfu));
}

void createlevel(void)
{
    static const char *const base_names[5] = { "base1", "base2", "base3", "base4", "base5" };
    static const struct { const char *name; int kind; } shops[] = {
        { "dealership2", 11 }, { "gunshop", 14 }, { "hospital0", 15 },
        { "aoffice1", 10 }, { "conveniencestore1", 13 }
    };
    const vec3 start = { 448.0f, 24.0f, 615.0f };
    const vec3 origin = { 0.0f, 0.0f, 0.0f };
    uint16_t materials[8];
    int x, y, z, i, j, k;

    os_srand(0);
    memset(g_level_marks, 0, 0x1000);
    createlandscape();
    buildsomeshit((const float *)&start);
    createarea(0, &origin, 64, 8, 64, 4.0f);
    g_city.num_intersections = 0;
    g_city.num_roads = 0;
    g_city.num_blocks = 0;
    g_city.unused_29810 = 0;
    g_city.unused_2f814 = 0;
    for (k = 0; k < 8; k++)
        *(int *)TEAM_FIELD(team0_num_car_spots, k) = 0;
    g_city.num_buildings = 0;
    if (sr_game.gametype != 1) {
        CityBlock *block;
        for (i = 0; i < g_level_editor.num_intersections; i++) {
            const LevelIntersection *entry = &g_level_editor.intersections[i];
            city_addintersection(entry->x, 5, entry->z);
        }
        for (i = 0; i < g_level_editor.num_streets; i++) {
            const LevelStreet *entry = &g_level_editor.streets[i];
            addcitystreet(entry->a, entry->b, entry->lanes_a, entry->lanes_b, entry->name);
        }
        g_city.unused_04 = 0;
        block = &g_city.blocks[g_city.num_blocks];
        block->x = block->z = 0;
        block->size_x = 32;
        block->height = 6;
        block->size_z = 32;
        block->inner_x = block->inner_z = 24;
        g_city.num_blocks++;
    }
    level_materials(materials, 3);
    for (y = 0; y < 6; y++) {
        for (z = 0; z < 512; z++) {
            for (x = 0; x < 512; x++) {
                if (z < 0x88 || z > 0x97)
                    area_createblock(0, x, y, z, 0x40000000u, materials);
            }
        }
    }
    if (sr_game.gametype == 1) {
        building_setupbuild();
        building_setupfloorplannewer();
        building_buildfloorplan(0, 0x24, 6, 0x24, g_building.build_size_x,
                               g_building.build_size_z, g_building.build_floors, 0);
        if (sr_game.gametype == 1)
            return;
    }
    for (i = 0; i < g_city.num_roads; i++)
        buildcitystreet(i);
    for (i = 0; i < g_city.num_intersections; i++)
        buildcityinterection(i);
    level_materials(materials, 0);
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            int mask = (i == 0) | ((j == 7) << 1) | ((i == 7) << 2) | ((j == 0) << 3);
            x = 0x1ea + j;
            z = 0x9b + i;
            area_createblock(0, x, 6, z, (uint32_t)g_blocklist.edge[mask] | 0x80000000u, materials);
            level_fix_ground(x, z);
        }
    }
    for (z = 1; z <= 510; z++) {
        for (x = 1; x <= 510; x++) {
            int tile = g_level_editor.tiles[z * 1024 + x];
            int kind, low;
            uint32_t word;
            if (tile == 0)
                continue;
            level_materials(materials, 3);
            area_createblock(0, x, 5, z, 0x40000000u, materials);
            low = tile & 15;
            level_materials(materials, low == 0 ? 0 : low == 1 ? 3 : (uint16_t)g_materiallist.tile_material);
            kind = tile >> 4;
            if (kind == 1) {
                int mask = ((g_level_editor.tiles[(z - 1) * 1024 + x] & 0xf0) != 0x10)
                    | (((g_level_editor.tiles[z * 1024 + x + 1] & 0xf0) != 0x10) << 1)
                    | (((g_level_editor.tiles[(z + 1) * 1024 + x] & 0xf0) != 0x10) << 2)
                    | (((g_level_editor.tiles[z * 1024 + x - 1] & 0xf0) != 0x10) << 3);
                word = (uint32_t)g_blocklist.edge[mask] | 0x80000000u;
            } else if (kind >= 2 && kind <= 5) {
                word = (uint32_t)g_blocklist.slab[kind - 2] | 0x80000000u;
            } else if (kind == 6) {
                word = 0x40000000u;
            } else if (kind >= 8 && kind <= 15) {
                word = (uint32_t)((const int *)g_blocklist.fence)[kind - 8] | 0x80000000u;
            } else {
                continue;
            }
            area_createblock(0, x, 6, z, word, materials);
            level_fix_ground(x, z);
        }
    }
    for (i = 0; i < g_level_editor.num_buildings; i++) {
        const LevelBuilding *entry = &g_level_editor.buildings[i];
        int kind = 9;
        building_load(entry->name, 0);
        building_setupbuild();
        for (k = entry->turns; k > 0; k--)
            building_rotatefloorplan();
        building_setupfloorplannewer();
        for (j = 0; j < 5; j++) {
            if (strcmp(entry->name, base_names[j]) == 0) {
                kind = j;
                break;
            }
        }
        if ((uint32_t)kind < 5)
            *(float *)TEAM_FIELD(team0_angle, kind) = (float)((float)entry->turns * 3.14159265359 * 0.5);
        for (j = 0; j < 5; j++) {
            if (strcmp(entry->name, shops[j].name) == 0) {
                kind = shops[j].kind;
                break;
            }
        }
        building_buildfloorplan(0, entry->x, 6, entry->z, g_building.build_size_x,
                               g_building.build_size_z, g_building.build_floors, kind);
    }
}
