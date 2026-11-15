/* The color of the sun light and of the sky (sky_light.c). */
#ifndef CL_RENDER_SKY_LIGHT_H
#define CL_RENDER_SKY_LIGHT_H

#include "sr_types.h"

float __cdecl raydistancesphere(const vec3 *from, const vec3 *dir, float radius);
void __cdecl getsuncolor(float out[3], const vec3 *from, const vec3 *to);
void __cdecl getsuncolors(void);
void __cdecl getskycolor(float color[3]);

#endif /* CL_RENDER_SKY_LIGHT_H */
