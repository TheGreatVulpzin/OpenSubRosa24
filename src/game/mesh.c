/* Curves of degree 1, 2 and 3. */
#include "mesh.h"

#include <string.h>

float tesselatebezier(int degree, float t, float a, float b, float c, float d)
{
    double u = (float)(1.0 - t);
    double v = t;
    switch (degree) {
    case 1:
        return (float)(u * a + d * v);
    case 2:
        return (float)(u * a * u + v * (u * ((double)b + b)) + v * d * v);
    case 3:
        return (float)(u * (c * 3.0) * v * v
            + (u * a * u * u + v * (u * (u * (b * 3.0)))) + v * d * v * v);
    default:
        /* Other degrees return the complement of t. */
        return (float)u;
    }
}

float tesselatebezierderivative(int degree, float t, float a, float b, float c, float d)
{
    double v = t;
    float result;
    switch (degree) {
    case 1:
        return (float)((double)d - a);
    case 2:
        return (float)((v * 2.0 * ((a - b * 2.0) + d) - a * 2.0) + b * 2.0);
    case 3:
        return (float)(b * 3.0 + (((v + v) * ((a * 3.0 - b * 6.0) + c * 3.0)
            + (((b * 3.0 - a) - c * 3.0) + d) * (v * (v * 3.0))) - a * 3.0));
    default:
        /* For another degree, the result is the bit pattern of degree. */
        memcpy(&result, &degree, sizeof(result));
        return result;
    }
}
