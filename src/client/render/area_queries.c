/* Questions about the blocks and the sectors of an area that the client asks. */
#include "render/area_queries.h"

#include <math.h>

#include "area.h"
#include "client.h"
#include "render/sector_model.h"
#include "sr_runtime.h"

#define g_sectors cl_data_block_sectors
#define NUM_SECTORS 0x2000
#define SECTOR_NEAR 9.999999747378752e-05f
#define BLOCK_KIND_MASK 0xe0000000u
#define BLOCK_LINK 0xc0000000u
#define MATERIAL_FACE 7

/* Game functions of other files. */
#define area_submitsectormodel CL_FN(0x44db70, void(__cdecl *)(int sector))

/* The outer block word at (x, y, z), or 0. A link to a larger block gives the word of the block
 * that the link points to. */
uint32_t __cdecl area_getblocklink(int area, int x, int y, int z)
{
    int cell, sector;
    uint32_t word;

    if (area < 0 || area > 3)
        return 0;
    sector = area_blocksector((uint32_t)area, x, y, z, &cell);
    if (sector == -1)
        return 0;
    word = g_sectors[sector].blocks[cell];
    if ((word & BLOCK_KIND_MASK) == BLOCK_LINK)
        word = area_getblock((uint32_t)area, x - (int)(word & 0xff), y - (int)((word >> 16) & 0xff),
                             z - (int)((word >> 8) & 0xff));
    return word;
}

/* The material of the last face of the outer block at (x, y, z), or -1. */
int __cdecl area_getblockmaterial(int area, int x, int y, int z)
{
    int cell, sector;

    if (area < 0 || area > 3)
        return -1;
    sector = area_blocksector((uint32_t)area, x, y, z, &cell);
    if (sector == -1)
        return -1;
    return g_sectors[sector].textures[cell][MATERIAL_FACE];
}

/* Tell if two vertices of the outer mesh of a sector are at the same place: each axis closer than 1e-4. */
int __cdecl area_sectorvertequal(int sector, int a, int b)
{
    const SectorVertex *verts;

    if (sector < 0 || sector >= NUM_SECTORS)
        return 0;
    verts = g_sectors[sector].verts;
    return (float)fabsf((float)(verts[a].pos.x - verts[b].pos.x)) < SECTOR_NEAR &&
           (float)fabsf((float)(verts[a].pos.y - verts[b].pos.y)) < SECTOR_NEAR &&
           (float)fabsf((float)(verts[a].pos.z - verts[b].pos.z)) < SECTOR_NEAR;
}

/* Make the models of a sector in use again, empty its four interior model slots and clear its
 * changed flag. A sector number outside the list, or a sector that is not in use, changes nothing. */
void __cdecl area_resetsector(int sector)
{
    int k;

    if (sector < 0 || sector >= NUM_SECTORS || g_sectors[sector].used == 0)
        return;
    area_buildsectormodel(sector);
    area_submitsectormodel(sector);
    for (k = 0; k < 4; k++)
        g_sectors[sector].interior_models[k] = -1;
    g_sectors[sector].dirty = 0;
}
