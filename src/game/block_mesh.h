/* Block files and temporary mesh builders. */
#ifndef SR_BLOCK_MESH_H
#define SR_BLOCK_MESH_H

#include <stdint.h>

void loadblock(const char *name);
void leveleditor_load(void);
void leveleditor_save(void);
void area_setupsblock(int block);
int area_block_rotatesurface(int turns, int side);
void area_copyrotateblock(int block, const int size[3]);
void area_block_addbox(float px, float py, float pz, float sx, float sy, float sz, uint32_t slot);
void area_block_addbeam(int turns, float px, float py, float pz, float sx, float sy, float sz,
                        uint32_t faces, const uint32_t tex[6]);

#endif
