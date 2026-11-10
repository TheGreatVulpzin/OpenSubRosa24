/* Noise and curves for the generated textures and models.
 *
 * noise1d and noise2d turn an integer or a grid point into a fixed value from -1 to 1. serp and
 * cerp blend two values with a smooth weight. hermiteinterpolation and engine_bezierinterpolation
 * give a point on a curve. The texture generators, the landscape and the model builders call them.
 */
#include "textures/noise.h"

#include "sr_math.h"

#define PI 3.14159265359

/* The largest value of the hash plus one. */
#define HASH_RANGE 1073741824.0

/* Mix the bits of n and give a value from -1 to 1. */
static float hash_unit(uint32_t n)
{
    int v;
    n = (n << 13) ^ n;
    v = (int)(n * n * 15731u + 789221u);
    return (float)(1.0 - (double)(int)(((uint32_t)(v * v) + 0x5208dd0du) & 0x7fffffff) / HASH_RANGE);
}

/* Blend a to b with the weight t * t * (3 - 2 t). t goes from 0 to 1. */
float __cdecl serp(float a, float b, float t)
{
    float weight = (float)((3.0 - t * 2.0) * (t * t));
    return (float)(b * weight + (1.0 - weight) * a);
}

/* Blend a to b with a cosine weight. t goes from 0 to 1. */
float __cdecl cerp(float a, float b, float t)
{
    float weight = (float)((1.0 - x87_cos(t * PI)) * 0.5);
    return (float)(b * weight + (1.0 - weight) * a);
}

/* The fixed noise value of an integer. */
float __cdecl noise1d(uint32_t n)
{
    return hash_unit(n);
}

/* The fixed noise value of a grid point. */
float __cdecl noise2d(int x, int y)
{
    return hash_unit((uint32_t)(y * 57 + x));
}

/* The fixed noise value of a grid point in 3D. */
float __cdecl noise3d(int x, int y, int z)
{
    return hash_unit((uint32_t)(y * 57 + x + z * 54323));
}

/* Put the point at t of a cubic Hermite curve into out. The curve goes from start to end with
 * the tangents start_tangent and end_tangent. */
void __cdecl hermiteinterpolation(float *out, float t, const float *start, const float *start_tangent,
                                  const float *end, const float *end_tangent)
{
    int axis;
    for (axis = 0; axis < 3; axis++) {
        out[axis] = (float)(((t * 2.0 * t * t - t * 3.0 * t) + 1.0) * start[axis]);
        out[axis] = (float)(((t * t * t - t * 2.0 * t) + t) * start_tangent[axis] + out[axis]);
        out[axis] = (float)((t * t * t - t * t) * end_tangent[axis] + out[axis]);
        out[axis] = (float)((t * 3.0 * t + t * -2.0 * t * t) * end[axis] + out[axis]);
    }
}

/* The value at t of a Bezier curve of degree 1, 2 or 3. Degree 1 uses p0 and p3. Degree 2 uses
 * p0, p1 and p3. Another degree gives 0. */
float __cdecl engine_bezierinterpolation(int degree, float t, float p0, float p1, float p2, float p3)
{
    float s = (float)(1.0 - t);
    if (degree == 1)
        return (float)(p3 * t + p0 * s);
    if (degree == 2)
        return (float)(p3 * t * t + ((p1 + p1) * s * t + p0 * s * s));
    if (degree == 3)
        return (float)(p3 * t * t * t + (p2 * 3.0 * s * t * t + (p1 * 3.0 * s * s * t + p0 * s * s * s)));
    return 0.0f;
}
