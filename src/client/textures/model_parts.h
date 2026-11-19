/* Parts that the model builders add to the model in work. See model_parts.c. */
#ifndef CL_TEXTURES_MODEL_PARTS_H
#define CL_TEXTURES_MODEL_PARTS_H

#include "sr_types.h"

void __cdecl addcylindertomodel(const vec3 *center, const vec3 *rot, float radius, float length);
void __cdecl addcockpittomodel(const int *corners);
void __cdecl model_extrudeeye(int quad, int bone);
void __cdecl model_extrudeeye2(int quad, int bone);
void __cdecl model_extrudequadface(int quad);

#endif /* CL_TEXTURES_MODEL_PARTS_H */
