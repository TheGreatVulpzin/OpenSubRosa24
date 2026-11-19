/* The flat surfaces of the block builders. See area_surfaces.c. */
#ifndef CL_RENDER_AREA_SURFACES_H
#define CL_RENDER_AREA_SURFACES_H

#include "sr_types.h"

void __cdecl area_block_addfloor(uint32_t y_bits, uint32_t slot);
void __cdecl area_block_addceil(uint32_t y_bits, uint32_t slot);
void __cdecl area_block_addfloorceil(void);
void __cdecl area_block_insertquad(int unused, int place);

#endif /* CL_RENDER_AREA_SURFACES_H */
