/* The ray that finds the sectors to draw marks the sector of each block it crosses. */
#include "render/raycast_sector.h"

#include "area.h"
#include "client.h"
#include "sr_runtime.h"

#define g_block_sectors cl_data_block_sectors

/* Mark the sector that holds the block (x, y, z) of an area as near the camera, and mark the
 * interior slot of the block as in use. Each slot covers two block layers. A block outside the
 * area, or in a place with no sector, changes nothing. */
void __cdecl raycast_addsector(int area, int x, int y, int z)
{
    int cell;
    int sector = area_blocksector((uint32_t)area, x, y, z, &cell);

    if (sector != -1) {
        g_block_sectors[sector].near_camera = 1;
        g_block_sectors[sector].interior_used[(y & 7) >> 1] = 1;
    }
}
