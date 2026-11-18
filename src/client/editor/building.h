/* The building editor. See building.c. */
#ifndef CL_EDITOR_BUILDING_H
#define CL_EDITOR_BUILDING_H

#include <stdint.h>

void __cdecl buildingedit_setup(void);
void __cdecl leveledit_setup(void);
void __cdecl buildingeditor_drawquad(int texture, float x, float z, float width, float depth, float red,
                                     float green, float blue, float alpha);
void __cdecl buildingeditor_drawblock(int x, int floor, int z);
void __cdecl buildingeditor_renderblock(uint32_t block, int layer, int x, int floor, int z, float red, float green,
                                        float blue, float alpha);
void __cdecl buildingeditor_renderblock2(uint32_t block, int layer, float x, float z, float red, float green,
                                         float blue, float alpha);
void __cdecl buildingedit_addblock(int x, int floor, int z, uint32_t block, uint32_t interior);
void __cdecl buildingedit_render(void);
void __cdecl buildingedit_deleteblock(int x, int floor, int z);
void __cdecl buildingedit_removetexture(int material);
void __cdecl buildingedit_inserttexture(const char *name, int place);
void __cdecl buildingeditor_draw_sblock(uint32_t block, float x, float y, float scale_x, float scale_y, float red,
                                        float green, float blue, float alpha);

#endif /* CL_EDITOR_BUILDING_H */
