/* The 4 areas of blocks (the buildings and the streets). An area is a grid of chunks of
 * 8 x 8 x 8 sectors, and a sector holds 8 x 8 x 8 blocks. Only the used chunks and sectors
 * have a record. */
#include "area.h"

#include "sr_runtime.h"

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
