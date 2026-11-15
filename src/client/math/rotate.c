/* Rotation of an orientation matrix around an axis. */
#include "math/rotate.h"

#include "sr_math.h"

/* Turn each row of the matrix around the axis by the angle. The axis is copied first, so it can
 * be a row of the matrix. Each step writes the row before the next step reads it. The rows are
 * not made unit vectors again, so their lengths can drift. */
void __cdecl rotateorientationfast(vec3 rot[3], const vec3 *axis, float angle)
{
    float ax = axis->x, ay = axis->y, az = axis->z;
    float c = (float)x87_cos(angle), s = (float)x87_sin(angle);
    int i;

    for (i = 0; i < 3; i++) {
        vec3 *row = &rot[i];
        float cx = (float)(row->y * az - row->z * ay);
        float cy = (float)(row->z * ax - row->x * az);
        float cz = (float)(row->x * ay - row->y * ax);
        float dot = (float)(row->z * az + (row->y * ay + row->x * ax));
        row->x = dot * ax;
        row->y = dot * ay;
        row->z = (float)(az * dot);
        row->x = (float)(row->x + c * ((float)(cz * ay - cy * az)));
        row->y = (float)(((float)(az * cx - cz * ax)) * c + row->y);
        row->z = (float)(c * ((float)(cy * ax - cx * ay)) + row->z);
        row->x = (float)(row->x + s * cx);
        row->y = (float)(row->y + s * cy);
        row->z = (float)(s * cz + row->z);
    }
}
