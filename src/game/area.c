/* The 4 areas of blocks (the buildings and the streets). An area is a grid of chunks of
 * 8 x 8 x 8 sectors, and a sector holds 8 x 8 x 8 blocks. Only the used chunks and sectors
 * have a record. */
#include "area.h"

#include "sr_runtime.h"
#include "sr_os.h"
#include "events.h"

#include <string.h>

#define g_area_block SR_AT(AreaBlock, SR_RVA_AREA_BLOCK)

/* The index of a block in its sector, or of a sector in its chunk. */
#define CELL(x, y, z) (((x) & 7) + (((z) & 7) + ((y) & 7) * 8) * 8)

/* The sector that holds block (x, y, z) of area `area`, or -1. *cell gets the index of the
 * block in that sector. The caller makes sure that area is below NUM_AREAS. */
int area_blocksector(uint32_t area, int x, int y, int z, int *cell)
{
    const Area *a = &g_areas[area];
    int chunk, sector;

    if (x < 0 || y < 0 || z < 0)
        return -1;
    if (!(x >> 6 < a->size_x && y >> 6 < a->size_y && z >> 6 < a->size_z))
        return -1;
    chunk = ((const int *)(uintptr_t)a->chunk_index)[(a->size_z * (y >> 6) + (z >> 6)) * a->size_x + (x >> 6)];
    if (chunk == -1)
        return -1;
    sector = a->chunks[chunk].sectors[CELL(x >> 3, y >> 3, z >> 3)];
    if (sector == -1)
        return -1;
    *cell = CELL(x, y, z);
    return sector;
}

/* The outer block word at (x, y, z), or 0. */
uint32_t area_getblock(uint32_t area, int x, int y, int z)
{
    int s, cell;

    if (area >= NUM_AREAS || (s = area_blocksector(area, x, y, z, &cell)) == -1)
        return 0;
    return g_sectors[s].blocks[cell];
}

/* The texture of face `face` of the outer block at (x, y, z), or -1. */
int area_getblocktexture(uint32_t area, int x, int y, int z, int face)
{
    int s, cell;

    if (area >= NUM_AREAS || (s = area_blocksector(area, x, y, z, &cell)) == -1)
        return -1;
    return g_sectors[s].textures[cell][face];
}

/* The interior block word at (x, y, z), or 0. */
uint32_t area_interior_getblock(uint32_t area, int x, int y, int z)
{
    int s, cell;

    if (area >= NUM_AREAS || (s = area_blocksector(area, x, y, z, &cell)) == -1)
        return 0;
    return g_sectors[s].interior[cell];
}

/* The texture of face `face` of the interior block at (x, y, z), or -1. */
int area_interior_getblocktexture(uint32_t area, int x, int y, int z, int face)
{
    int s, cell;

    if (area >= NUM_AREAS || (s = area_blocksector(area, x, y, z, &cell)) == -1)
        return -1;
    return g_sectors[s].interior_textures[cell][face];
}

/* The interior block word at (x, y, z). A link gives the word of the block it points to. */
uint32_t area_interior_getblocklink(uint32_t area, int x, int y, int z)
{
    uint32_t w = area_interior_getblock(area, x, y, z);

    if (BLOCK_KIND(w) == BLOCK_LINK)
        w = area_interior_getblock(area, x - LINK_DX(w), y - LINK_DY(w), z - LINK_DZ(w));
    return w;
}

/* The first free chunk of area `area`, or -1. */
int area_getnextchunk(int area)
{
    int i;

    for (i = 0; i < NUM_CHUNKS; i++)
        if (g_areas[area].chunks[i].used == 0)
            return i;
    return -1;
}

/* The first free sector, or -1. */
int area_getnextsector(void)
{
    int i;

    for (i = 0; i < NUM_SECTORS; i++)
        if (g_sectors[i].used == 0)
            return i;
    return -1;
}

void area_marksector(uint32_t area, int x, int y, int z)
{
    int s, cell;
    if (area < NUM_AREAS && (s = area_blocksector(area, x, y, z, &cell)) != -1)
        g_sectors[s].dirty = 1;
}

int area_createchunk(uint32_t area, int cx, int cy, int cz)
{
    Area *a;
    int c, i;
    if (area >= NUM_AREAS || cx < 0 || cy < 0 || cz < 0)
        return -1;
    a = &g_areas[area];
    if (cx >= a->size_x || cy >= a->size_y || cz >= a->size_z)
        return -1;
    c = area_getnextchunk(area);
    if (c == -1)
        return -1;
    ((int *)(uintptr_t)a->chunk_index)[(a->size_z * cy + cz) * a->size_x + cx] = c;
    a->chunks[c].used = 1;
    for (i = 0; i < 512; i++)
        a->chunks[c].sectors[i] = -1;
    return c;
}

int area_createsector(uint32_t area, uint32_t chunk, int sx, int sy, int sz)
{
    Area *a;
    BlockSector *sector;
    int s, i;
    if (area >= NUM_AREAS || chunk >= NUM_CHUNKS || sx < 0 || sy < 0 || sz < 0)
        return -1;
    a = &g_areas[area];
    if ((sx >> 3) >= a->size_x || (sy >> 3) >= a->size_y || (sz >> 3) >= a->size_z)
        return -1;
    s = area_getnextsector();
    if (s == -1)
        return -1;
    sector = &g_sectors[s];
    sector->pos[2] = sz;
    sector->area = area;
    a->chunks[chunk].sectors[CELL(sx, sy, sz)] = s;
    sector->used = 1;
    sector->pos[0] = sx;
    sector->pos[1] = sy;
    memset(sector->blocks, 0, sizeof(sector->blocks));
    memset(sector->interior, 0, sizeof(sector->interior));
    memset(sector->cityobjects, 0, sizeof(sector->cityobjects));
    sector->num_meshes = 0;
    a->num_sectors++;
    for (i = 0; i < 4; i++)
        sector->interior_models[i] = -1;
    sector->unused_584c = 0;
    return s;
}

static int area_cellsector(uint32_t area, int x, int y, int z, int *cell)
{
    Area *a = &g_areas[area];
    int chunk, s;
    if (x < 0 || y < 0 || z < 0 || (x >> 6) >= a->size_x ||
        (y >> 6) >= a->size_y || (z >> 6) >= a->size_z)
        return -1;
    chunk = ((int *)(uintptr_t)a->chunk_index)[(a->size_z * (y >> 6) + (z >> 6)) * a->size_x + (x >> 6)];
    if (chunk == -1)
        chunk = area_createchunk(area, x >> 6, y >> 6, z >> 6);
    if (chunk == -1)
        return -1;
    s = a->chunks[chunk].sectors[CELL(x >> 3, y >> 3, z >> 3)];
    if (s == -1)
        s = area_createsector(area, chunk, x >> 3, y >> 3, z >> 3);
    *cell = CELL(x, y, z);
    return s;
}

static void interior_release(int sector, int y)
{
    int k = (y >> 1) & 3;
    int model = g_sectors[sector].interior_models[k];
    if (model >= 0) {
        *SR_AT(int, SR_RVA_AREA_INTERIOR + 4 + model * 0x2a30) = -1;
        g_sectors[sector].interior_models[k] = -1;
    }
}

void area_setblock(uint32_t area, int x, int y, int z, uint32_t word)
{
    int s, cell;
    if (area < NUM_AREAS && (s = area_cellsector(area, x, y, z, &cell)) != -1) {
        g_sectors[s].blocks[cell] = word;
        area_marksector(area, x, y, z);
    }
}

void area_setblockcolor(uint32_t area, int x, int y, int z, uint32_t color)
{
    int s, cell;
    if (area < NUM_AREAS && (s = area_cellsector(area, x, y, z, &cell)) != -1) {
        g_sectors[s].colors[cell] = color;
        area_marksector(area, x, y, z);
    }
}

void area_interior_setblock(uint32_t area, int x, int y, int z, uint32_t word)
{
    int s, cell;
    if (area < NUM_AREAS && (s = area_blocksector(area, x, y, z, &cell)) != -1) {
        g_sectors[s].interior[cell] = word;
        interior_release(s, y);
        area_marksector(area, x, y, z);
    }
}

void area_interior_setobject(uint32_t area, int x, int y, int z, uint32_t object)
{
    int s, cell;
    if (area < NUM_AREAS && (s = area_blocksector(area, x, y, z, &cell)) != -1) {
        g_sectors[s].cityobjects[cell] = object;
        interior_release(s, y);
    }
}

static const struct {
    int dx, dy, dz;
    uint32_t self, neighbour;
} neighbours[6] = {
    {0, -1, 0, 1, 0x20}, {0, 1, 0, 0x20, 1},
    {0, 0, 1, 2, 8}, {0, 0, -1, 8, 2},
    {-1, 0, 0, 4, 0x10}, {1, 0, 0, 0x10, 4}
};

void area_deleteblock(uint32_t area, int x, int y, int z)
{
    int s, cell, i;
    if (area >= NUM_AREAS || (s = area_blocksector(area, x, y, z, &cell)) == -1)
        return;
    if (BLOCK_KIND(g_sectors[s].blocks[cell]) == BLOCK_NORMAL) {
        for (i = 0; i < 6; i++) {
            int nx = x + neighbours[i].dx, ny = y + neighbours[i].dy, nz = z + neighbours[i].dz;
            uint32_t w = area_getblock(area, nx, ny, nz);
            if (BLOCK_KIND(w) == BLOCK_NORMAL)
                area_setblock(area, nx, ny, nz, w | neighbours[i].neighbour);
        }
    }
    g_sectors[s].blocks[cell] = 0;
    g_sectors[s].interior[cell] = 0;
    area_marksector(area, x, y, z);
}

int area_breakblock(uint32_t area, int x, int y, int z, uint32_t word, int face,
                    const vec3 *pos, const vec3 *vel)
{
    if (area >= NUM_AREAS || x < 0 || BLOCK_KIND(word) != BLOCK_SPEC ||
        g_area_block[word & 0xffff].num_hideable <= face)
        return 0;
    createevent_updateblock_quad(area, x, y, z, word, face, pos, vel);
    area_interior_setblock(area, x, y, z, ((3u << (face & 0x1e)) << 16) | word);
    return 1;
}

void area_createblock(uint32_t area, int x, int y, int z, uint32_t word, const uint16_t tex[8])
{
    int s, cell, i, dx, dy, dz;
    BlockSector *sector;
    if (area >= NUM_AREAS || word == 0 || (s = area_cellsector(area, x, y, z, &cell)) == -1)
        return;
    sector = &g_sectors[s];
    if (BLOCK_KIND(word) == BLOCK_NORMAL) {
        uint32_t w = BLOCK_NORMAL | 0x3f;
        for (i = 0; i < 8; i++) {
            sector->textures[cell][i] = tex[i];
            if (!(tex[i] & 0x8000))
                w |= 0x40u << i;
        }
        sector->blocks[cell] = w;
        sector->colors[cell] = 0;
        for (i = 0; i < 6; i++) {
            int nx = x + neighbours[i].dx, ny = y + neighbours[i].dy, nz = z + neighbours[i].dz;
            w = area_getblock(area, nx, ny, nz);
            if (BLOCK_KIND(w) == BLOCK_NORMAL || (i == 0 && y == 0)) {
                sector->blocks[cell] &= ~neighbours[i].self;
                area_setblock(area, nx, ny, nz, w & ~neighbours[i].neighbour);
            }
        }
        area_marksector(area, x, y, z);
    } else if (BLOCK_KIND(word) == BLOCK_SPEC) {
        AreaBlock *block = &g_area_block[word & 0xffff];
        sector->blocks[cell] = word;
        for (i = 0; i < 8; i++)
            sector->textures[cell][i] = tex[i];
        sector->colors[cell] = 0;
        for (dy = 0; dy < block->size_y; dy++)
            for (dz = 0; dz < block->size_z; dz++)
                for (dx = 0; dx < block->size_x; dx++) {
                    if (dx == 0 && dy == 0 && dz == 0)
                        continue;
                    area_setblock(area, x + dx, y + dy, z + dz,
                                  BLOCK_LINK | (uint32_t)dy << 16 | (uint32_t)dz << 8 | (uint32_t)dx);
                    area_marksector(area, x + dx, y + dy, z + dz);
                }
        area_marksector(area, x, y, z);
    }
}

void area_interior_createblock(uint32_t area, int x, int y, int z, uint32_t word, const uint16_t tex[8])
{
    int s, cell, i, dx, dy, dz;
    if (area >= NUM_AREAS || (s = area_blocksector(area, x, y, z, &cell)) == -1)
        return;
    if (BLOCK_KIND(word) == BLOCK_SPEC) {
        const AreaBlock *block = &g_area_block[word & 0xffff];
        int size_x = block->size_x, size_y = block->size_y, size_z = block->size_z;
        g_sectors[s].interior[cell] = word;
        for (i = 0; i < 8; i++)
            g_sectors[s].interior_textures[cell][i] = (uint8_t)tex[i];
        for (dy = 0; dy < size_y; dy++)
            for (dz = 0; dz < size_z; dz++)
                for (dx = 0; dx < size_x; dx++) {
                    if (dx == 0 && dy == 0 && dz == 0)
                        continue;
                    area_interior_setblock(area, x + dx, y + dy, z + dz,
                        BLOCK_LINK | (uint32_t)dy << 16 | (uint32_t)dz << 8 | (uint32_t)dx);
                }
    }
    interior_release(s, y);
    area_marksector(area, x, y, z);
}

void createarea(uint32_t area, const vec3 *origin, int size_x, int size_y, int size_z, float block_size)
{
    Area *a;
    int *index;
    int x, y, z, i;
    if (area >= NUM_AREAS)
        return;
    a = &g_areas[area];
    a->active = 1;
    a->origin = *origin;
    a->axes[0] = (vec3){1, 0, 0};
    a->axes[1] = (vec3){0, 1, 0};
    a->axes[2] = (vec3){0, 0, 1};
    a->block_size = block_size;
    a->size_x = size_x;
    a->size_y = size_y;
    a->size_z = size_z;
    a->inv_block_size = (float)(1.0 / (double)block_size);
    os_free((void *)(uintptr_t)a->chunk_index);
    index = os_malloc((uint32_t)size_x * (uint32_t)size_y * (uint32_t)size_z * 4u);
    a->chunk_index = (int)(uintptr_t)index;
    for (y = 0; y < size_y; y++)
        for (z = 0; z < size_z; z++)
            for (x = 0; x < size_x; x++)
                index[(size_z * y + z) * size_x + x] = -1;
    for (i = 0; i < NUM_CHUNKS; i++)
        a->chunks[i].used = 0;
    for (i = 0; i < NUM_SECTORS; i++)
        if (g_sectors[i].area == (int)area)
            g_sectors[i].used = 0;
    a->box_min = *origin;
    a->box_max.x = (float)((double)origin->x + (double)block_size * ((double)size_x * 8.0 * 8.0));
    a->box_max.y = (float)((double)size_y * 8.0 * 8.0 * (double)block_size + (double)origin->y);
    a->box_max.z = (float)((double)size_z * 8.0 * 8.0 * (double)block_size + (double)origin->z);
    a->unused_0064 = 0;
}
