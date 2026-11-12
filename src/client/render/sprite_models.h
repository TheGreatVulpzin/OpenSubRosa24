/* The pieces that fly and land. See sprite_models.c. */
#ifndef CL_RENDER_SPRITE_MODELS_H
#define CL_RENDER_SPRITE_MODELS_H

#include "sr_types.h"

int __cdecl createsprite_model(int kind, const vec3 *pos, const vec3 *vel, const vec3 *rot);
void __cdecl deletesprite_model(int index);
void __cdecl sprite_modeltimetolive(void);
void __cdecl sprite_model_simulation(void);
void __cdecl renderspritemodels(int mode);

#endif /* CL_RENDER_SPRITE_MODELS_H */
