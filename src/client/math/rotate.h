/* Rotation of an orientation matrix. See rotate.c. */
#ifndef CL_MATH_ROTATE_H
#define CL_MATH_ROTATE_H

#include "sr_types.h"

void __cdecl rotateorientationfast(vec3 rot[3], const vec3 *axis, float angle);

#endif /* CL_MATH_ROTATE_H */
