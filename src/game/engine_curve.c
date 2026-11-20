/* The engine curve for an RPM input.
 * RPM selects a point on one cubic curve.
 * A second cubic curve converts that point to a torque multiplier.
 */
#include "objects.h"
#include "mesh.h"

/* Clamp RPM to the curve and return its torque multiplier. */
float getenginetorque(float rpm)
{
    float position = (float)((double)rpm / 8000.0);
    float blend;
    if (position < 0.0)
        position = 0.0f;
    if (position > 1.0)
        position = 1.0f;
    blend = tesselatebezier(3, position, 0.0f, 0.125f, 0.875f, 1.0f);
    return tesselatebezier(3, blend, 0.75f, 1.0f, 1.0f, 0.75f);
}
