/* Building files and working floor plans. */
#include "buildings.h"

#include <stdio.h>
#include <string.h>

#include "sr_os.h"

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
