/* The flat surfaces that the block builders add to the temporary mesh: a floor, a ceiling, and the
 * pair that makes the floor and the ceiling of a thin room. Each surface is a bezier of degree 1
 * with 4 corner points. The corner points are points 0, 3, 15 and 12 of the surface record.
 */
#include "render/area_surfaces.h"

#include <string.h>

#include "client.h"
#include "mesh.h"
#include "sr_runtime.h"

#define g_surface ((vec3 *)&sr_data_surface.points)
#define SURFACE_FLOOR_SIDE 0x40000
#define SURFACE_CEILING_SIDE 0x30000
#define THIN_FLOOR_BITS 0x3d800000u /* 1/16 */
#define THIN_CEILING_BITS 0x3f600000u /* 7/8 */

/* Set the 4 corners of a horizontal surface at height y. The corners run around the square
 * x 0 to 1 and z 0 to 1. A ceiling runs the other way, so that it faces down. */
static void set_square(uint32_t y_bits, int facing_down)
{
    float near_z = facing_down ? 1.0f : 0.0f;
    float far_z = facing_down ? 0.0f : 1.0f;

    g_surface[0].x = 0.0f;
    memcpy(&g_surface[0].y, &y_bits, sizeof y_bits);
    g_surface[0].z = near_z;
    g_surface[3].x = 1.0f;
    memcpy(&g_surface[3].y, &y_bits, sizeof y_bits);
    g_surface[3].z = near_z;
    g_surface[15].x = 1.0f;
    memcpy(&g_surface[15].y, &y_bits, sizeof y_bits);
    g_surface[15].z = far_z;
    g_surface[12].x = 0.0f;
    memcpy(&g_surface[12].y, &y_bits, sizeof y_bits);
    g_surface[12].z = far_z;
}

/* Add a floor with a texture slot. The height is a float given as its 32 bits and stored as it is. */
void __cdecl area_block_addfloor(uint32_t y_bits, uint32_t slot)
{
    set_square(y_bits, 0);
    addbeziersurface((const float *)g_surface, sr_data_surface.uv, 1, 1, 1, 1, (int)(slot | SURFACE_FLOOR_SIDE));
}

/* Add a ceiling with a texture slot. The height is a float given as its 32 bits. */
void __cdecl area_block_addceil(uint32_t y_bits, uint32_t slot)
{
    set_square(y_bits, 1);
    addbeziersurface((const float *)g_surface, sr_data_surface.uv, 1, 1, 1, 1, (int)(slot | SURFACE_CEILING_SIDE));
}

/* Add the floor of a thin room at 1/16 and its ceiling at 7/8. The floor uses slot 0 and the ceiling slot 1. */
void __cdecl area_block_addfloorceil(void)
{
    set_square(THIN_FLOOR_BITS, 0);
    addbeziersurface((const float *)g_surface, sr_data_surface.uv, 1, 1, 1, 1, SURFACE_FLOOR_SIDE);
    set_square(THIN_CEILING_BITS, 1);
    addbeziersurface((const float *)g_surface, sr_data_surface.uv, 1, 1, 1, 1, SURFACE_CEILING_SIDE | 1);
}

/* Open two quads in the list of quads of the temporary mesh at a place: every quad from the place on moves
 * two places up with its texture slot, and the count grows by two. The new places keep the old values. */
void __cdecl area_block_insertquad(int unused, int place)
{
    int i;

    (void)unused;
    for (i = sr_data_tempmesh.num_quads + 1; place + 2 <= i; i--) {
        sr_data_tempmesh.quads[i] = sr_data_tempmesh.quads[i - 2];
        sr_data_tempmesh.quad_slots[i] = sr_data_tempmesh.quad_slots[i - 2];
    }
    sr_data_tempmesh.num_quads += 2;
}
