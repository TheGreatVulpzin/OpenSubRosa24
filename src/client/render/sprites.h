/* Particles, glass, tire strips, muzzle flashes and checkpoints. */
#ifndef CL_RENDER_SPRITES_H
#define CL_RENDER_SPRITES_H

#include "sr_types.h"

enum {
    SPRITE_MARK,
    SPRITE_STAIN,
    SPRITE_SPARK,
    SPRITE_GLASS,
    SPRITE_TRIANGLE,
    SPRITE_TRACE
};

void __cdecl spritesimulation(void);
void __cdecl rendersprites(void);
void __cdecl createwindowsprites(int object, int face, const vec3 *hit, const vec3 *impulse);
void __cdecl tiremark_simulation(void);
void __cdecl tiremark_render(void);
void __cdecl rendermuzzleflashes(void);
void __cdecl rendercheckpoints(void);
int pointinsidecheckpoint(const vec3 *position, int checkpoint);
void __cdecl createspritemodel(int model, int kind);
void __cdecl selectnearitem(void);

#endif
