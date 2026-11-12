/* The view cone of the camera. See frustum.c. */
#ifndef CL_RENDER_FRUSTUM_H
#define CL_RENDER_FRUSTUM_H

#include "sr_types.h"

int __cdecl boxinsidefrustum(const vec3 *smallest, const vec3 *largest);
int __cdecl pointinsidefrustum(const vec3 *point);

#endif /* CL_RENDER_FRUSTUM_H */
