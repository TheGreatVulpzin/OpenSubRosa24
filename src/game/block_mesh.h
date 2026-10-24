/* Block files and temporary mesh builders. */
#ifndef SR_BLOCK_MESH_H
#define SR_BLOCK_MESH_H

#include <stdint.h>
#include "sr_types.h"

void loadblock(const char *name);
void leveleditor_load(void);
void leveleditor_save(void);
void area_setupsblock(int block);
int area_block_rotatesurface(int turns, int side);
void area_copyrotateblock(int block, const int size[3]);
void area_block_addbox(float px, float py, float pz, float sx, float sy, float sz, uint32_t slot);
void area_block_addbeam(int turns, float px, float py, float pz, float sx, float sy, float sz,
                        uint32_t faces, const uint32_t tex[6]);
void area_block_addbasicsurface(uint32_t face, const vec3 *pos);
int area_block_addwindow(int turns, float x, float width, float y, float height, int block, uint32_t slot);
void area_block_adddoor(int turns, float width, float height);
void area_block_addoutdoor(int turns, float unused, float height);
void area_block_addcorner(int turns, float x, float z);
void area_block_addwindowvertical(int turns, int type, int row);
void area_block_addwindowhorizontal(int turns, int type, int row, int has_floor, int has_ceiling);
int area_block_createbuildinginterior(int scratch, uint32_t code);

#endif
