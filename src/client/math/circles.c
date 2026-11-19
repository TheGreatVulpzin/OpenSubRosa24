/* The test of a point against a circle, and the torque curve of an engine. */
#include "math/circles.h"

#include "math/vectors2d.h"
#include "mesh.h"

#define ENGINE_MAX_RPM 8000.0

/* Tell if a point is inside a circle. out gets the vector from the center to the point. When the
 * point is inside, out is made unit length and distance is divided by the radius. The result uses the
 * distance before that division. */
int __cdecl pointintersectcircle(float *out, float *distance, const float *point, const float *center, float radius)
{
    float length;

    subtractvectors2d(out, point, center);
    *distance = (float)vectorlength2d(out);
    length = *distance;
    if (length < radius) {
        scalevector2d(out, out, (float)(1.0 / *distance));
        *distance = (float)(*distance / radius);
    }
    return length < radius;
}

/* The torque of an engine at a speed in turns per minute, from 0 to 1. The speed is cut to 0 to
 * 8000 and goes through two bezier curves. */
float __cdecl getenginetorque(float rpm)
{
    float t = (float)(rpm / ENGINE_MAX_RPM);
    float first;

    if (t < 0.0f)
        t = 0.0f;
    if (1.0f < t)
        t = 1.0f;
    first = tesselatebezier(3, t, 0.0f, 0.125f, 0.875f, 1.0f);
    return (float)tesselatebezier(3, first, 0.75f, 1.0f, 1.0f, 0.75f);
}
