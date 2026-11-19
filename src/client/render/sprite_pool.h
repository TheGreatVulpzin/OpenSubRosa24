/* The list of particles. See sprite_pool.c. */
#ifndef CL_RENDER_SPRITE_POOL_H
#define CL_RENDER_SPRITE_POOL_H

#include "sr_types.h"

int __cdecl createsprite(int kind, const vec3 *pos, const vec3 *vel, float size);
void __cdecl deletesprite(int index);
void __cdecl spritetimetolive(void);
void __cdecl createwindowsprites_block(int area, int x, int y, int z, uint32_t word, int face, const vec3 *apex,
                                       const float *impulse);

#endif /* CL_RENDER_SPRITE_POOL_H */
