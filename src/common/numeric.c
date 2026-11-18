/* Scalar rounding and distance measures.
 * Intermediate float stores retain the precision of each public result.
 * The circle query writes its direction and distance even when it misses.
 */
#include "numeric.h"

#include <math.h>
#include "sr_math.h"
#include "vectors.h"

/* Round a value to the nearest multiple of the step, with ties toward positive infinity. */
float fround(float value, float step)
{
    double whole = floor((double)value / step + 0.5);
    return (float)(whole * step);
}

/* Convert a distance into the integer network weight. */
int net_calcdistancefactor(float distance)
{
    float fraction = (float)(x87_pow(distance, 1.75) / 256.0);
    return (int)((1.0 - (double)fraction) * 256.0);
}

/* Return the first sphere intersection distance along a unit ray. */
float raydistancesphere2(const vec3 *center, const vec3 *direction, float radius)
{
    float projection = -dotproduct(center, direction);
    float squared = dotproduct(center, center);
    float perpendicular = (float)((double)squared - (double)projection * projection);
    volatile float root = (float)x87_sqrt((double)radius * radius - perpendicular);
    return (float)(projection - root);
}

/* Return one inside the circle. A hit replaces distance with its fraction of the radius. */
int pointintersectcircle(float direction[2], float *distance, const float point[2],
                         const float center[2], float radius)
{
    float length;
    subtractvectors2d(direction, point, center);
    *distance = (float)vectorlength2d(direction);
    length = *distance;
    if (length < radius) {
        scalevector2d(direction, direction, (float)(1.0 / *distance));
        *distance = (float)((double)*distance / radius);
    }
    return length < radius;
}
