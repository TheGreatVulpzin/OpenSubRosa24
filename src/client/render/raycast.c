/* The rays that find the sectors near the camera: 80 rows of 128 rays leave the camera in a cone,
 * and each ray marks the sectors that it crosses.
 */
#include "render/raycast.h"

#include "area.h"
#include "client.h"
#include "render/raycast_sector.h"
#include "setup/timers.h"
#include "sr_runtime.h"
#include "vectors.h"

#define RAY_ROWS 0x50
#define RAY_COLUMNS 0x80
#define RAY_STEPS 0x30
#define RAY_END 0x30
#define BLOCK_KIND_MASK 0xe0000000u
#define BLOCK_SOLID 0x40000000u
#define BLOCK_LINK 0xc0000000u

/* Game functions of other files. */
#define g_sectors sr_data_area_sector.records

/* Mark the origin of a block that is a link to a larger block. The link holds the offset to the origin. */
static void add_link_origin(uint32_t word, int x, int y, int z)
{
    if ((word & BLOCK_KIND_MASK) == BLOCK_LINK)
        raycast_addsector(0, x - (int)(word & 0xff), y - (int)((word >> 16) & 0xff), z - (int)((word >> 8) & 0xff));
}

/* Mark what one ray crosses at a block of the ray. step is the number of the step along the ray.
 * The result is the step number to go on with: a ray ends at once in a solid block with no interior. */
static int mark_block(int step, int x, int y, int z)
{
    int cell;
    int sector = area_blocksector(0, x, y, z, &cell);
    uint32_t block, interior;

    if (sector == -1)
        return step;
    g_sectors[sector].near_camera = 1;
    if (step < 0x10) {
        g_sectors[sector].interior_used[(y & 7) >> 1] = 1;
    } else {
        g_sectors[sector].interior_used[0] = 1;
        g_sectors[sector].interior_used[1] = 1;
        g_sectors[sector].interior_used[2] = 1;
        g_sectors[sector].interior_used[3] = 1;
    }
    if (step < 2) {
        raycast_addsector(0, x, y + 1, z);
        raycast_addsector(0, x, y + 2, z);
        raycast_addsector(0, x, y + 3, z);
    }
    if (step < 4) {
        raycast_addsector(0, x - 8, y, z);
        raycast_addsector(0, x + 8, y, z);
        raycast_addsector(0, x, y, z - 8);
        raycast_addsector(0, x, y, z + 8);
    }
    block = g_sectors[sector].blocks[cell];
    interior = g_sectors[sector].interior[cell];
    add_link_origin(block, x, y, z);
    if (interior == 0 && (block & BLOCK_KIND_MASK) == BLOCK_SOLID)
        step = RAY_END;
    add_link_origin(interior, x, y, z);
    return step;
}

/* Cast the rays of the view and mark the sectors that they cross. Each ray starts at the camera in
 * steps of 1/256 of a block, and it goes on for 48 steps. */
void __cdecl render_raycast2(void)
{
    int row, column;

    starttimer(sr_data_timerlist.raycast);
    for (row = 0; row < RAY_ROWS; row++) {
        for (column = 0; column < RAY_COLUMNS; column++) {
            vec3 dir, step;
            int px, py, pz, i;
            float scale;
            float step_x, step_y, step_z;

            dir.x = (float)(((float)column - 64.0) * 0.0078125);
            dir.y = (float)(((float)row - 40.0) * 0.0078125);
            dir.z = -0.5f;
            normalizevector(&dir, &dir);
            scalevector(&dir, &dir, 0.699999988079071f);
            projectvector(&step, &dir, cl_data_audio_camera.rot);
            px = (int)(long long)((cl_data_audio_camera.rot[2].x * 0.0 + cl_data_audio_camera.pos.x) *
                                  sr_data_area[0].inv_block_size * 256.0);
            py = (int)(long long)((cl_data_audio_camera.rot[2].y * 0.0 + cl_data_audio_camera.pos.y) *
                                  sr_data_area[0].inv_block_size * 256.0);
            pz = (int)(long long)((cl_data_audio_camera.rot[2].z * 0.0 + cl_data_audio_camera.pos.z) *
                                  sr_data_area[0].inv_block_size * 256.0);
            scale = (float)((float)(column & 7) + 248.0);
            step_x = step.x * scale;
            step_y = step.y * scale;
            step_z = step.z * scale;
            for (i = 0; i < RAY_STEPS; i++) {
                i = mark_block(i, px >> 8, py >> 8, pz >> 8);
                px += (int)(long long)step_x;
                py += (int)(long long)step_y;
                pz += (int)(long long)step_z;
            }
        }
    }
    stoptimer(sr_data_timerlist.raycast);
}
