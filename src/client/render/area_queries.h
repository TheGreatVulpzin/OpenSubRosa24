/* Questions about the blocks and the sectors of an area. See area_queries.c. */
#ifndef CL_RENDER_AREA_QUERIES_H
#define CL_RENDER_AREA_QUERIES_H

#include "sr_types.h"

uint32_t __cdecl area_getblocklink(int area, int x, int y, int z);
int __cdecl area_getblockmaterial(int area, int x, int y, int z);
int __cdecl area_sectorvertequal(int sector, int a, int b);
void __cdecl area_resetsector(int sector);

#endif /* CL_RENDER_AREA_QUERIES_H */
