/* Rigid body helpers of the objects: values of the whole body from its particles. */
#include "rigid.h"

#include "sr_runtime.h"

/* Game globals. */
#define g_xaxis sr_data_xaxis /* (1 0 0) */
#define g_yaxis sr_data_yaxis /* (0 1 0) */
#define g_zaxis sr_data_zaxis /* (0 0 1) */

/* The angular velocity of object obj around center: for each axis, the sum over the particles
 * of the velocity along (axis x r), where r is the particle position from the center, times
 * 1/128. */
void getobjectrotation2(vec3 *out, int obj, const vec3 *center)
{
    const Object *ob = &sr_objects[obj];
    const Particle *q;
    float rx, ry, rz;
    int i;

    out->x = 0.0f;
    out->y = 0.0f;
    out->z = 0.0f;
    for (i = 0; i < ob->num_particles; i++) {
        q = &sr_particles[ob->particles[i]];
        rx = (float)(q->pos.x - center->x);
        ry = (float)(q->pos.y - center->y);
        rz = (float)(q->pos.z - center->z);
        out->x = (float)(((float)(q->vel.z * ((float)(g_xaxis.y * rx - g_xaxis.x * ry)) +
                                  (q->vel.x * ((float)(ry * g_xaxis.z - rz * g_xaxis.y)) +
                                   q->vel.y * ((float)(g_xaxis.x * rz - rx * g_xaxis.z))))) *
                             0.125 * 0.125 * 0.5 +
                         out->x);
        out->y = (float)(((float)(q->vel.z * ((float)(g_yaxis.y * rx - g_yaxis.x * ry)) +
                                  (q->vel.x * ((float)(g_yaxis.z * ry - g_yaxis.y * rz)) +
                                   q->vel.y * ((float)(g_yaxis.x * rz - g_yaxis.z * rx))))) *
                             0.125 * 0.125 * 0.5 +
                         out->y);
        out->z = (float)(((float)(q->vel.z * ((float)(g_zaxis.y * rx - ry * g_zaxis.x)) +
                                  (((float)(g_zaxis.z * ry - g_zaxis.y * rz)) * q->vel.x +
                                   q->vel.y * ((float)(g_zaxis.x * rz - g_zaxis.z * rx))))) *
                             0.125 * 0.125 * 0.5 +
                         out->z);
    }
}
