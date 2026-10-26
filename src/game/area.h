/* The 4 areas of blocks. See area.c. */
#ifndef SR_AREA_H
#define SR_AREA_H

#include <stdint.h>

#include "sr_types.h"

#define NUM_AREAS 4
#define NUM_SECTORS 0x2000
#define NUM_CHUNKS 0x200

/* The kind of a block word: its high 3 bits. */
#define BLOCK_KIND(w) ((uint32_t)(w) & 0xe0000000u)
#define BLOCK_NORMAL 0x40000000u
#define BLOCK_SPEC 0x80000000u
#define BLOCK_LINK 0xc0000000u
/* A link points to the block at x - LINK_DX, y - LINK_DY, z - LINK_DZ. */
#define LINK_DX(w) ((uint32_t)(w) & 0xff)
#define LINK_DY(w) ((uint32_t)(w) >> 16 & 0xff)
#define LINK_DZ(w) ((uint32_t)(w) >> 8 & 0xff)

#define g_areas sr_data_area
#define g_sectors ((BlockSector *)&sr_data_area_sector.records)

int area_blocksector(uint32_t area, int x, int y, int z, int *cell);
uint32_t area_getblock(uint32_t area, int x, int y, int z);
int area_getblocktexture(uint32_t area, int x, int y, int z, int face);
uint32_t area_interior_getblock(uint32_t area, int x, int y, int z);
int area_interior_getblocktexture(uint32_t area, int x, int y, int z, int face);
uint32_t area_interior_getblocklink(uint32_t area, int x, int y, int z);
int area_getnextchunk(int area);
int area_getnextsector(void);

void area_marksector(uint32_t area, int x, int y, int z);
int area_createchunk(uint32_t area, int cx, int cy, int cz);
int area_createsector(uint32_t area, uint32_t chunk, int sx, int sy, int sz);
void area_setblock(uint32_t area, int x, int y, int z, uint32_t word);
void area_setblockcolor(uint32_t area, int x, int y, int z, uint32_t color);
void area_interior_setblock(uint32_t area, int x, int y, int z, uint32_t word);
void area_interior_setobject(uint32_t area, int x, int y, int z, uint32_t object);
void area_deleteblock(uint32_t area, int x, int y, int z);
int area_breakblock(uint32_t area, int x, int y, int z, uint32_t word, int face,
                    const vec3 *pos, const vec3 *vel);
void area_createblock(uint32_t area, int x, int y, int z, uint32_t word, const uint16_t tex[8]);
void area_interior_createblock(uint32_t area, int x, int y, int z, uint32_t word, const uint16_t tex[8]);
void createarea(uint32_t area, const vec3 *origin, int size_x, int size_y, int size_z, float block_size);

#endif /* SR_AREA_H */
