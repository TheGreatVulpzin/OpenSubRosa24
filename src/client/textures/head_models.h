/* The head models. See head_models.c. */
#ifndef CL_TEXTURES_HEAD_MODELS_H
#define CL_TEXTURES_HEAD_MODELS_H

#include "sr_types.h"

void __cdecl tesselatebeziertriangle(vec3 *out, float a, float b, float c, const int *patch);
void __cdecl createheadmodel2(int model);
void __cdecl createheadmodel3(int model);
void __cdecl createheadmodel(int model);

#endif /* CL_TEXTURES_HEAD_MODELS_H */
