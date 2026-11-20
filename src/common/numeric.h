/* Scalar rounding and distance measures. */
#ifndef SR_NUMERIC_H
#define SR_NUMERIC_H

#include <stdint.h>
#include "sr_types.h"

float fround(float value, float step);
int net_calcdistancefactor(float distance);
float raydistancesphere2(const vec3 *center, const vec3 *direction, float radius);
int pointintersectcircle(float direction[2], float *distance, const float point[2],
                         const float center[2], float radius);

#endif
