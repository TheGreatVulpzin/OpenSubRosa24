/* The shadow volume of a model. See shadow.c. */
#ifndef CL_RENDER_SHADOW_H
#define CL_RENDER_SHADOW_H

#include "sr_types.h"

void __cdecl rendermodelshadow(int model, const void *placement, const vec3 rotation[3]);

#endif /* CL_RENDER_SHADOW_H */
