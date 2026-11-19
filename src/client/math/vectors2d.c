/* The vectors of two floats: the first two floats of a vector are x and y. */
#include "math/vectors2d.h"

#include <math.h>

/* The dot product of two vectors of two floats. */
float __cdecl dotproduct2d(const float *a, const float *b)
{
    return (float)(a[1] * b[1] + a[0] * b[0]);
}

/* The length of a vector of two floats. */
double __cdecl vectorlength2d(const float *v)
{
    return sqrt((double)dotproduct2d(v, v));
}

/* out = a + b. */
void __cdecl addvectors2d(float *out, const float *a, const float *b)
{
    out[0] = (float)(a[0] + b[0]);
    out[1] = (float)(a[1] + b[1]);
}

/* out = a - b. */
void __cdecl subtractvectors2d(float *out, const float *a, const float *b)
{
    out[0] = (float)(a[0] - b[0]);
    out[1] = (float)(a[1] - b[1]);
}

/* out = a * scale. */
void __cdecl scalevector2d(float *out, const float *a, float scale)
{
    out[0] = (float)(a[0] * scale);
    out[1] = (float)(a[1] * scale);
}

/* out = a + b * scale. */
void __cdecl scaleaddvectors2d(float *out, const float *a, const float *b, float scale)
{
    out[0] = (float)(b[0] * scale + a[0]);
    out[1] = (float)(b[1] * scale + a[1]);
}

/* out = (-y, x) of a. The second value is read after the first is written, so with the same vector
 * for both the result is (-y, -y). */
void __cdecl tangentvector2d(float *out, const float *a)
{
    out[0] = -a[1];
    out[1] = a[0];
}
