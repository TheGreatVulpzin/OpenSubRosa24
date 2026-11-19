/* The sectors that a ray crosses. See raycast_sector.c. */
#ifndef CL_RENDER_RAYCAST_SECTOR_H
#define CL_RENDER_RAYCAST_SECTOR_H

#include "sr_types.h"

void __cdecl raycast_addsector(int area, int x, int y, int z);

#endif /* CL_RENDER_RAYCAST_SECTOR_H */
