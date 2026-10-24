/* Building files and working floor plans. */
#include "buildings.h"

#include <stdio.h>
#include <string.h>

#include "area.h"
#include "block_mesh.h"
#include "logic.h"
#include "sr_math.h"
#include "sr_os.h"
#include "vectors.h"

#define g_building (*SR_AT(Building, SR_RVA_BUILDING))
#define g_floorplan SR_AT(FloorplanCell, 0x2043a0)
#define g_parser (*SR_AT(Parser, SR_RVA_PARSER))
#define g_materiallist (*SR_AT(MaterialList, SR_RVA_MATERIALLIST))
#define g_materials SR_AT(MaterialRec, 0x16d50024)
#define g_area_block SR_AT(AreaBlock, SR_RVA_AREA_BLOCK)
#define g_blocklist (*SR_AT(BlockList, SR_RVA_BLOCKLIST))

typedef size_t(__cdecl *building_io_fn)(void *, size_t, size_t, void *);
#define game_fread (*SR_AT(building_io_fn, 0x91022a4))
#define game_fwrite (*SR_AT(building_io_fn, 0x13bf2d00))

static int cell_index(int floor, int row, int column)
{
    return (floor * 64 + row) * 64 + column;
}

void building_load(const char *name, int header_only)
{
    Building *b = &g_building;
    char path[64];
    int version, f, r, c, i, k;
    b->size_x = 24;
    b->size_z = 24;
    b->floors = 4;
    memset(b->cells, 0, sizeof b->cells);
    sprintf(path, "data/building/%s.sbb", name);
    g_parser.file = os_fopen(path, "rb");
    if (g_parser.file != NULL) {
        game_fread(&version, 4, 1, g_parser.file);
        if ((uint32_t)version - 6u > 4u) {
            os_fclose(g_parser.file);
            return;
        }
        game_fread(b->name, 1, 64, g_parser.file);
        game_fread(&b->size_x, 4, 1, g_parser.file);
        game_fread(&b->size_z, 4, 1, g_parser.file);
        game_fread(&b->floors, 4, 1, g_parser.file);
        if (header_only != 0) {
            os_fclose(g_parser.file);
            return;
        }
        game_fread(&b->num_materials, 4, 1, g_parser.file);
        for (i = 0; i < b->num_materials; i++)
            game_fread(b->materials + i * 64, 1, 64, g_parser.file);
        game_fread(&b->num_blocks, 4, 1, g_parser.file);
        for (i = 0; i < b->num_blocks; i++)
            game_fread(b->blocks + i * 64, 1, 64, g_parser.file);
        for (f = 0; f < b->floors; f++) {
            for (r = 0; r <= b->size_z; r++) {
                for (c = 0; c <= b->size_x; c++) {
                    BuildingCell *cell = &b->cells[cell_index(f, r, c)];
                    game_fread(&cell->outer, 4, 1, g_parser.file);
                    game_fread(&cell->inner, 4, 1, g_parser.file);
                    cell->interior_bit18 = 0;
                    if (version > 6)
                        game_fread(&cell->interior_bit18, 4, 1, g_parser.file);
                    game_fread(&cell->wall_row, 4, 1, g_parser.file);
                    game_fread(&cell->wall_col, 4, 1, g_parser.file);
                    game_fread(&cell->interior_bit16, 4, 1, g_parser.file);
                    for (k = 0; k < 8; k++)
                        game_fread(&cell->tex_outer[k], 2, 1, g_parser.file);
                    for (k = 0; k < 8; k++)
                        game_fread(&cell->tex_inner[k], 2, 1, g_parser.file);
                    game_fread(&cell->spare, 4, 1, g_parser.file);
                    game_fread(&cell->color, 4, 1, g_parser.file);
                    if (version < 8)
                        cell->color = 0;
                    if (version > 9) {
                        game_fread(&cell->guard_dir, 4, 1, g_parser.file);
                        game_fread(&cell->prize, 4, 1, g_parser.file);
                    }
                }
            }
        }
        if (version > 8) {
            for (i = 0; i < 16; i++) {
                int *area = (int *)&b->areas[i];
                for (k = 0; k < 6; k++)
                    game_fread(&area[k], 4, 1, g_parser.file);
            }
        }
        os_fclose(g_parser.file);
    }
    building_setupfloorplannewer();
    for (f = 0; f < b->floors; f++) {
        for (c = 0; c <= b->size_x; c++) {
            BuildingCell *cell = &b->cells[cell_index(f, b->size_z, c)];
            cell->outer = 0;
            cell->inner = 0;
        }
        for (r = 0; r <= b->size_z; r++) {
            BuildingCell *cell = &b->cells[cell_index(f, r, b->size_x)];
            cell->outer = 0;
            cell->inner = 0;
        }
    }
}

void building_save(const char *name)
{
    Building *b = &g_building;
    char path[64];
    int version = 10, f, r, c, i, k;
    for (f = 0; f < 64; f++) {
        for (r = 0; r < 64; r++) {
            for (c = 0; c < 64; c++) {
                const BuildingCell *cell = &b->cells[cell_index(f, r, c)];
                if (cell->outer != 0 || cell->inner != 0) {
                    if (b->floors < f + 1) b->floors = f + 1;
                    if (b->size_z < r + 1) b->size_z = r + 1;
                    if (b->size_x < c + 1) b->size_x = c + 1;
                }
            }
        }
    }
    sprintf(path, "data/building/%s.sbb", name);
    g_parser.file = os_fopen(path, "wb");
    if (g_parser.file == NULL)
        return;
    game_fwrite(&version, 4, 1, g_parser.file);
    game_fwrite(b->name, 1, 64, g_parser.file);
    game_fwrite(&b->size_x, 4, 1, g_parser.file);
    game_fwrite(&b->size_z, 4, 1, g_parser.file);
    game_fwrite(&b->floors, 4, 1, g_parser.file);
    game_fwrite(&b->num_materials, 4, 1, g_parser.file);
    for (i = 0; i < b->num_materials; i++)
        game_fwrite(b->materials + i * 64, 1, 64, g_parser.file);
    game_fwrite(&b->num_blocks, 4, 1, g_parser.file);
    for (i = 0; i < b->num_blocks; i++)
        game_fwrite(b->blocks + i * 64, 1, 64, g_parser.file);
    for (f = 0; f < b->floors; f++) {
        for (r = 0; r <= b->size_z; r++) {
            for (c = 0; c <= b->size_x; c++) {
                BuildingCell *cell = &b->cells[cell_index(f, r, c)];
                game_fwrite(&cell->outer, 4, 1, g_parser.file);
                game_fwrite(&cell->inner, 4, 1, g_parser.file);
                game_fwrite(&cell->interior_bit18, 4, 1, g_parser.file);
                game_fwrite(&cell->wall_row, 4, 1, g_parser.file);
                game_fwrite(&cell->wall_col, 4, 1, g_parser.file);
                game_fwrite(&cell->interior_bit16, 4, 1, g_parser.file);
                for (k = 0; k < 8; k++)
                    game_fwrite(&cell->tex_outer[k], 2, 1, g_parser.file);
                for (k = 0; k < 8; k++)
                    game_fwrite(&cell->tex_inner[k], 2, 1, g_parser.file);
                game_fwrite(&cell->spare, 4, 1, g_parser.file);
                game_fwrite(&cell->color, 4, 1, g_parser.file);
                game_fwrite(&cell->guard_dir, 4, 1, g_parser.file);
                game_fwrite(&cell->prize, 4, 1, g_parser.file);
            }
        }
    }
    for (i = 0; i < 16; i++) {
        int *area = (int *)&b->areas[i];
        for (k = 0; k < 6; k++)
            game_fwrite(&area[k], 4, 1, g_parser.file);
    }
    os_fclose(g_parser.file);
}

void building_setupbuild(void)
{
    Building *b = &g_building;
    int mat_map[256] = {0};
    uint32_t block_map[1024] = {0};
    int i, j, f, r, c, k;
    for (i = 0; i < b->num_materials; i++)
        for (j = 0; j < g_materiallist.num_materials; j++)
            if (strcmp(b->materials + i * 64, g_materials[j].name) == 0)
                mat_map[i] = j;
    for (i = 0; i < b->num_blocks; i++)
        for (j = 0; j < g_blocklist.num_blocks; j++)
            if (strcmp(b->blocks + i * 64, g_area_block[j].name) == 0)
                block_map[i] = (uint32_t)j;
    b->build_size_x = b->size_x;
    b->build_size_z = b->size_z;
    b->build_floors = b->floors;
    for (f = 0; f < b->build_floors; f++) {
        for (r = 0; r <= b->build_size_z; r++) {
            for (c = 0; c <= b->build_size_x; c++) {
                i = cell_index(f, r, c);
                const BuildingCell *s = &b->cells[i];
                BuildingCell *d = &b->build_cells[i];
                d->outer = s->outer;
                if ((s->outer & 0xe0000000u) == 0x80000000u)
                    d->outer = (block_map[s->outer & 0x3ff] | 0x80000000u) + ((s->outer >> 24) & 3);
                d->inner = s->inner;
                if ((s->inner & 0xe0000000u) == 0x80000000u)
                    d->inner = (block_map[s->inner & 0x3ff] | 0x80000000u) + ((s->inner >> 24) & 3);
                d->wall_row = s->wall_row;
                d->wall_col = s->wall_col;
                d->interior_bit18 = s->interior_bit18;
                d->interior_bit16 = s->interior_bit16;
                for (k = 0; k < 8; k++) {
                    d->tex_outer[k] = (int16_t)((s->tex_outer[k] & 0xff00) |
                                              (uint16_t)mat_map[s->tex_outer[k] & 0xff]);
                    d->tex_inner[k] = (int16_t)(uint16_t)mat_map[s->tex_inner[k] & 0xff];
                }
                d->color = s->color;
                d->guard_dir = s->guard_dir;
                d->prize = s->prize;
            }
        }
    }
    memcpy(b->build_areas, b->areas, sizeof b->areas);
}

static int has_wall(int wall)
{
    return (wall > 0 && wall < 9) || wall == 12;
}

void building_setupfloorplannewer(void)
{
    Building *b = &g_building;
    int f, r, c, i;
    for (f = 0; f < b->build_floors; f++) {
        for (r = 0; r <= b->build_size_z; r++) {
            for (c = 0; c <= b->build_size_x; c++) {
                i = cell_index(f, r, c);
                const BuildingCell *s = &b->build_cells[i];
                FloorplanCell *d = &g_floorplan[i];
                d->outer = s->outer;
                d->inner = s->inner;
                d->wall_row = s->wall_row;
                d->wall_col = s->wall_col;
                d->interior_bit18 = s->interior_bit18;
                d->interior_bit16 = s->interior_bit16;
                d->interior_bit17 = f >= b->build_floors - 1 ||
                    b->build_cells[i + 4096].outer == 0 || b->build_cells[i + 4096].interior_bit16 != 0;
                memcpy(d->tex_outer, s->tex_outer, sizeof d->tex_outer);
                memcpy(d->tex_inner, s->tex_inner, sizeof d->tex_inner);
                d->color = s->color;
                d->guard_dir = s->guard_dir;
                d->prize = s->prize;
            }
        }
    }
    for (f = 0; f < b->build_floors; f++) {
        for (r = 0; r <= b->build_size_z; r++) {
            for (c = 0; c <= b->build_size_x; c++) {
                i = cell_index(f, r, c);
                FloorplanCell *p = &g_floorplan[i];
                if (p->outer == 0x40000000u && (p->inner == 0xffff || p->inner == 0 ||
                    g_area_block[p->inner & 0xffff].is_door != 0)) {
                    if (has_wall(g_floorplan[i + 64].wall_row))
                        p->tex_outer[1] = (int16_t)(p->tex_outer[1] ^ 0x8000);
                    if (has_wall(p->wall_col))
                        p->tex_outer[2] = (int16_t)(p->tex_outer[2] ^ 0x8000);
                    if (has_wall(p->wall_row))
                        p->tex_outer[3] = (int16_t)(p->tex_outer[3] ^ 0x8000);
                    if (has_wall(g_floorplan[i + 1].wall_col))
                        p->tex_outer[4] = (int16_t)(p->tex_outer[4] ^ 0x8000);
                }
            }
        }
    }
}

void building_rotatefloorplan(void)
{
    static const int texture_turn[8] = {0, 4, 1, 2, 3, 5, 6, 7};
    Building *b = &g_building;
    int sx = b->build_size_x, sz = b->build_size_z;
    int f, r, c, i, k;
    for (f = 0; f < b->build_floors; f++)
        for (r = 0; r <= sz; r++)
            for (c = 0; c <= sx; c++) {
                i = cell_index(f, r, c);
                b->rot_cells[i] = b->build_cells[i];
            }
    memset(b->build_cells, 0, sizeof b->build_cells);
    memcpy(b->rot_areas, b->build_areas, sizeof b->rot_areas);
    for (f = 0; f < b->build_floors; f++) {
        for (r = 0; r <= sz; r++) {
            for (c = 0; c <= sx; c++) {
                const BuildingCell *s = &b->rot_cells[cell_index(f, r, c)];
                int index = f * 4096 + c * 64 + sz - 1 - r;
                if (r < sz) {
                    BuildingCell *d = &b->build_cells[index];
                    for (k = 0; k < 2; k++) {
                        uint32_t word = k ? s->inner : s->outer;
                        uint32_t *target;
                        int adj = 0;
                        if ((word & 0xe0000000u) == 0x80000000u) {
                            int id = word & 0xffff;
                            adj = 1 - g_area_block[id].size_z;
                            if (g_blocklist.turn_first <= id && id < g_blocklist.turn_end)
                                id = ((id + 1) & 3) == 0 ? id - 3 : id + 1;
                            word = (uint32_t)id | 0x80000000u;
                        }
                        target = k ? &b->build_cells[index + adj].inner : &b->build_cells[index + adj].outer;
                        if (*target == 0)
                            *target = word;
                    }
                    for (k = 0; k < 8; k++) {
                        d->tex_outer[k] = s->tex_outer[texture_turn[k]];
                        d->tex_inner[k] = s->tex_inner[texture_turn[k]];
                    }
                    d->wall_row = s->wall_col;
                    d->interior_bit18 = s->interior_bit18;
                    d->interior_bit16 = s->interior_bit16;
                    d->color = s->color;
                    d->guard_dir = s->guard_dir == 0 ? 0 : (s->guard_dir + 1 > 4 ? 1 : s->guard_dir + 1);
                    d->prize = s->prize;
                }
                b->build_cells[index + 1].wall_col = s->wall_row;
            }
        }
    }
    for (i = 0; i < 16; i++) {
        const BuildingArea *s = &b->rot_areas[i];
        BuildingArea *d = &b->build_areas[i];
        d->x = sz - s->z - s->size_z;
        d->z = s->x;
        d->size_x = s->size_z;
        d->size_z = s->size_x;
    }
    b->build_size_x = sz;
    b->build_size_z = sx;
}

static int has_door_wall(int wall)
{
    return wall > 0 && wall < 10;
}

static uint32_t interior_code(const FloorplanCell *p)
{
    uint32_t code = ((((((uint32_t)p->interior_bit18 * 2 | (uint32_t)p->interior_bit17) * 2 |
        (uint32_t)p->interior_bit16) << 4 | (uint32_t)p->wall_col) << 4 |
        (uint32_t)p[64].wall_row) << 4 | (uint32_t)p[1].wall_col) << 4 | (uint32_t)p->wall_row;
    if (p->wall_row == 0 && p->wall_col == 0 &&
        (has_door_wall(p[-1].wall_row) || has_door_wall(p[-64].wall_col)))
        code |= 0xb;
    if (p->wall_row == 0 && p[1].wall_col == 0 &&
        (has_door_wall(p[1].wall_row) || has_door_wall(p[-63].wall_col)))
        code |= 0xb0;
    if (p[64].wall_row == 0) {
        if (p[1].wall_col == 0 && (has_door_wall(p[65].wall_row) || has_door_wall(p[65].wall_col)))
            code |= 0xb00;
        if (p->wall_col == 0 && (has_door_wall(p[63].wall_row) || has_door_wall(p[64].wall_col)))
            code |= 0xb000;
    }
    return code;
}

static void setup_team_base(int team, int x, int y, int z, int size_x, int size_z)
{
    const BuildingArea *a = &g_building.build_areas[0];
    vec3 *base_min = (vec3 *)TEAM_FIELD(team0_base_min, team);
    vec3 *base_max = (vec3 *)TEAM_FIELD(team0_base_max, team);
    vec3 *safe_min = (vec3 *)TEAM_FIELD(team0_safe_min, team);
    vec3 *safe_max = (vec3 *)TEAM_FIELD(team0_safe_max, team);
    const float angle = *(float *)TEAM_FIELD(team0_angle, team);
    int *num_spots = (int *)TEAM_FIELD(team0_num_car_spots, team);
    TeamCarSpot *spots = (TeamCarSpot *)TEAM_FIELD(team0_car_spots, team);
    vec3 p, d, rot[3];
    int k;
    *base_min = (vec3){(float)(x * 4.0), (float)(y * 4.0), (float)(z * 4.0)};
    *base_max = (vec3){(float)((x + size_x) * 4.0), (float)((y + 1) * 4.0), (float)((z + size_z) * 4.0)};
    *safe_min = (vec3){(float)((a->x + x) * 4.0), (float)(y * 4.0), (float)((a->z + z) * 4.0)};
    *safe_max = (vec3){(float)((a->size_x + a->x + x) * 4.0), (float)((y + 1) * 4.0),
                       (float)((a->size_z + a->z + z) * 4.0)};
    p.x = (float)(((double)safe_max->x + safe_min->x) * 0.5);
    p.y = (float)(((double)safe_max->y + safe_min->y) * 0.5);
    p.z = (float)(((double)safe_max->z + safe_min->z) * 0.5);
    resetorientation(rot);
    rotateorientation(rot, &rot[1], (float)(angle - 1.570796326795));
    p.x = (float)(p.x - rot[2].x * 44.0);
    p.y = (float)(p.y - rot[2].y * 44.0);
    p.z = (float)(p.z - rot[2].z * 44.0);
    rotateorientation(rot, &rot[1], 3.1415927410125732f);
    p.x = (float)(p.x - rot[0].x * 16.0);
    p.y = (float)(p.y - rot[0].y * 16.0);
    p.z = (float)(p.z - rot[0].z * 16.0);
    d.x = (float)(rot[0].x * 4.0);
    d.y = (float)(rot[0].y * 4.0);
    d.z = (float)(rot[0].z * 4.0);
    for (k = 0; k < 8; k++) {
        TeamCarSpot *spot = &spots[*num_spots];
        spot->used = 0;
        spot->pos = p;
        memcpy(spot->rot, rot, sizeof rot);
        (*num_spots)++;
        p.x = (float)(p.x + d.x);
        p.y = (float)(p.y + d.y);
        p.z = (float)(p.z + d.z);
    }
}

static int finish_building(int b, int x, int y, int z, int size_x, int size_z, int type)
{
    static const int kinds[6] = {3, 2, 5, 4, 7, 6};
    const BuildingArea *a = &g_building.build_areas[0];
    vec3 *pos = BLD_PTR(vec3, building_pos, b);
    vec3 *min = BLD_PTR(vec3, building_min, b);
    vec3 *max = BLD_PTR(vec3, building_max, b);
    int result = b * 0x9dc;
    if (type < 10 || type > 15)
        return type;
    BLD_INT(building_type, b) = kinds[type - 10];
    *pos = (vec3){(float)(x * 4.0), (float)(y * 4.0), (float)(z * 4.0)};
    *min = *pos;
    *max = (vec3){(float)((x + size_x) * 4.0), (float)((y + 1) * 4.0), (float)((z + size_z) * 4.0)};
    switch (type) {
    case 10:
        result = z + size_z;
        break;
    case 11: {
        vec3 *car = BLD_PTR(vec3, car_spawn_pos, b);
        car->x = (float)((g_building.build_size_x + 2 + x) * 4.0);
        car->y = (float)((double)pos->y + 2.0);
        car->z = (float)((g_building.build_size_z - 6 + z) * 4.0);
        BLD_INT(shop_count, b) = 0;
        result = g_building.build_size_z - 6 + z;
        break;
    }
    case 12:
    case 14:
        BLD_INT(shop_count, b) = 0;
        break;
    case 13:
        BLD_INT(shop_count, b) = 0;
        if (sr_server) {
            int *rows = BLD_PTR(int, shop_rows, b);
            rows[0] = 0;
            rows[1] = 5000;
            rows[3] = 1;
            rows[4] = 1000;
            BLD_INT(shop_count, b) = 2;
            result = (int)(uintptr_t)BLD_PTR(int, shop_count, b);
        }
        break;
    case 15:
        min->x = (float)((a->x + x) * 4.0);
        min->z = (float)((a->z + z) * 4.0);
        max->x = (float)((a->size_x + a->x + x) * 4.0);
        max->z = (float)((a->size_z + a->z + z) * 4.0);
        result = a->size_z + a->z + z;
        break;
    }
    g_city.num_buildings++;
    return result;
}

int building_buildfloorplan(uint32_t area, int x, int y, int z,
                           int size_x, int size_z, int floors, int type)
{
    static const uint16_t slab_tex[8] = {3, 3, 3, 3, 3, 3, 3, 3};
    const FloorplanCell *const floorplan = g_floorplan;
    City *const city = &g_city;
    const BlockList *const blocks = &g_blocklist;
    int *num_guards;
    GuardSpot *guards;
    vec3 *prize_pos;
    int team = type >= 0 && type <= 4 ? type : -1;
    int f, r, c, i, b, best_index = 0;
    float best = 1024.0f;
    for (f = 0; f < floors; f++)
        for (r = 0; r < size_z; r++)
            for (c = 0; c < size_x; c++)
                if (floorplan[cell_index(f, r, c)].outer != 0)
                    area_deleteblock(area, x + c, y + f, z + r);
    for (r = 0; r < size_z; r++)
        for (c = 0; c < size_x; c++)
            area_createblock(area, x + c, y - 1, z + r, 0x40000000u, slab_tex);
    for (f = 0; f < floors; f++) {
        for (r = 0; r < size_z; r++) {
            for (c = 0; c < size_x; c++) {
                const FloorplanCell *p = &floorplan[cell_index(f, r, c)];
                if (p->outer != 0) {
                    uint16_t tex[8];
                    memcpy(tex, p->tex_outer, sizeof tex);
                    area_createblock(area, x + c, y + f, z + r, p->outer, tex);
                    area_setblockcolor(area, x + c, y + f, z + r, (uint32_t)p->color);
                }
            }
        }
    }
    for (f = 0; f < floors; f++) {
        for (r = 0; r < size_z; r++) {
            for (c = 1; c <= size_x; c++) {
                const FloorplanCell *p = &floorplan[cell_index(f, r, c - 1)];
                uint32_t inner = p->inner;
                int xx = x + c - 1, yy = y + f, zz = z + r;
                if (inner != 0) {
                    uint16_t tex[8];
                    memcpy(tex, p->tex_inner, sizeof tex);
                    if (inner == 0xffff) {
                        uint32_t id = (uint32_t)area_block_createbuildinginterior(0, interior_code(p));
                        if (area_interior_getblock(area, xx, yy, zz) == 0)
                            area_interior_createblock(area, xx, yy, zz, id | 0x80000000u, tex);
                    } else {
                        area_interior_createblock(area, xx, yy, zz, inner | 0x80000000u, tex);
                    }
                    if (team != -1 && blocks->base_door_first <= (int)(inner & 0xffff) &&
                        (int)(inner & 0xffff) <= blocks->base_door_last) {
                        DoorBlock *doors = (DoorBlock *)TEAM_FIELD(team0_doors, team);
                        int turn = ((int)(inner & 0xffff) - blocks->base_door_first - 2) & 3;
                        doors[0] = (DoorBlock){xx, yy, zz, turn};
                        doors[1] = doors[0];
                        if (turn == 0 || turn == 2)
                            doors[1].x = xx + 1;
                        else
                            doors[1].z = zz + 1;
                    }
                    if (p->prize != 0)
                        area_interior_setobject(area, xx, yy, zz, 0x401);
                }
            }
        }
    }
    b = city->num_buildings;
    num_guards = BLD_PTR(int, num_guard_spots, b);
    guards = BLD_PTR(GuardSpot, guard_spots, b);
    prize_pos = BLD_PTR(vec3, prize_pos, b);
    *num_guards = 0;
    for (f = 0; f < floors; f++) {
        for (r = 0; r < size_z; r++) {
            for (c = 0; c < size_x; c++) {
                const FloorplanCell *p = &floorplan[cell_index(f, r, c)];
                int xx = x + c, yy = y + f, zz = z + r;
                if (p->prize != 0) {
                    *prize_pos = (vec3){(float)(xx * 4.0 + 2.0), (float)(yy * 4.0 + 2.25),
                                       (float)(zz * 4.0 + 2.0)};
                }
                if (p->guard_dir > 0 && *num_guards < 64) {
                    GuardSpot *spot = &guards[*num_guards];
                    spot->pos = (vec3){(float)(xx * 4.0 + 2.0), (float)(yy * 4.0 + 1.25),
                                      (float)(zz * 4.0 + 2.0)};
                    spot->dir = p->guard_dir - 1;
                    (*num_guards)++;
                }
            }
        }
    }
    if (team != -1)
        setup_team_base(team, x, y, z, size_x, size_z);
    for (i = 0; i < city->num_intersections; i++) {
        const CityIntersection *intersection = &city->intersections[i];
        float dx = (float)(x - intersection->x);
        float dz = (float)(z - intersection->z);
        float d2 = (float)((double)dx * dx + 0.0 + (double)dz * dz);
        float distance = (float)x87_sqrt(d2);
        if (distance < best) {
            best = distance;
            best_index = i;
        }
    }
    BLD_PTR(int, street_roads, b)[0] = city->intersections[best_index].roads[0];
    BLD_PTR(int, street_roads, b)[1] = city->intersections[best_index].roads[1];
    return finish_building(b, x, y, z, size_x, size_z, type);
}
