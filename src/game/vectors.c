/* Math for 3D vectors (vec3) and orientations (3 rows of vec3).
 *
 * The float math runs on the x87 (this file compiles with /arch:IA32). Each sum and product
 * keeps 53-bit precision, and each (float) rounds the value to a float. Keep the order of the
 * terms and each (float): a change gives a different last bit.
 */
#include "sr_math.h"
#include "sr_types.h"

/* a . b. The 024d adds x and y first, then z. */
float dotproduct(const vec3 *a, const vec3 *b)
{
    return (float)(a->z * b->z + (a->x * b->x + a->y * b->y));
}

/* out = a x b. */
void crossproduct(vec3 *out, const vec3 *a, const vec3 *b)
{
    out->x = (float)(a->z * b->y - a->y * b->z);
    out->y = (float)(b->z * a->x - b->x * a->z);
    out->z = (float)(a->y * b->x - a->x * b->y);
}

/* |v|. */
float vectorlength(const vec3 *v)
{
    return (float)x87_sqrt((float)(v->z * v->z + (v->x * v->x + v->y * v->y)));
}

/* out = v / |v|, or zero when |v| is zero. out can be v. */
void normalizevector(vec3 *out, const vec3 *v)
{
    float length = (float)x87_sqrt((float)(v->z * v->z + (v->x * v->x + v->y * v->y)));
    float scale;
    if (length != 0.0) {
        scale = (float)(1.0 / length);
        out->x = scale * v->x;
        out->y = (float)(scale * v->y);
        out->z = (float)(scale * v->z);
        return;
    }
    out->x = 0.0f;
    out->y = 0.0f;
    out->z = 0.0f;
}

/* out = a + b. */
void addvectors(vec3 *out, const vec3 *a, const vec3 *b)
{
    out->x = (float)(a->x + b->x);
    out->y = (float)(a->y + b->y);
    out->z = (float)(a->z + b->z);
}

/* out = a - b. */
void subtractvectors(vec3 *out, const vec3 *a, const vec3 *b)
{
    out->x = (float)(a->x - b->x);
    out->y = (float)(a->y - b->y);
    out->z = (float)(a->z - b->z);
}

/* out = v * s. */
void scalevector(vec3 *out, const vec3 *v, float s)
{
    out->x = s * v->x;
    out->y = (float)(v->y * s);
    out->z = (float)(s * v->z);
}

/* out = a + b * s. */
void scaleaddvectors(vec3 *out, const vec3 *a, const vec3 *b, float s)
{
    out->x = (float)(a->x + s * b->x);
    out->y = (float)(b->y * s + a->y);
    out->z = (float)(s * b->z + a->z);
}

/* |b - a|. Each difference is a float, then the sum of the squares. */
float vectordistance(const vec3 *a, const vec3 *b)
{
    float dx = (float)(b->x - a->x), dy = (float)(b->y - a->y), dz = (float)(b->z - a->z);
    return (float)x87_sqrt((float)(dz * (b->z - a->z) + (dy * (b->y - a->y) + dx * (b->x - a->x))));
}

/* out = v.x * rot[0] + v.y * rot[1] + v.z * rot[2]. v is read before the
 * first write, so out can be v. */
void projectvector(vec3 *out, const vec3 *v, const vec3 rot[3])
{
    float x = v->x, y = v->y, z = v->z;
    out->x = (float)(y * rot[1].x + x * rot[0].x + z * rot[2].x);
    out->y = (float)(rot[2].y * z + (rot[0].y * x + rot[1].y * y));
    out->z = (float)(z * rot[2].z + (rot[1].z * y + rot[0].z * x));
}

/* out = the unit normal of the triangle p0, p1, p2:
 * (p1 - p0) x (p2 - p0), normalized. */
void generatetrinormal(vec3 *out, const vec3 *p0, const vec3 *p1, const vec3 *p2)
{
    float ax = p1->x, ay = p1->y, az = p1->z;
    float bx = p2->x, by = p2->y, bz = p2->z;
    float ox = p0->x, oy = p0->y, oz = p0->z;
    out->x = (float)(((float)(az - oz)) * ((float)(by - oy)) - ((float)(ay - oy)) * ((float)(bz - oz)));
    out->y = (float)(((float)(ax - ox)) * (bz - oz) - ((float)(bx - ox)) * (az - oz));
    out->z = (float)((bx - ox) * (ay - oy) - (ax - ox) * (by - oy));
    normalizevector(out, out);
}

/* rot = the identity matrix. */
void resetorientation(vec3 rot[3])
{
    rot[0].x = 1.0f;
    rot[0].y = 0.0f;
    rot[0].z = 0.0f;
    rot[1].x = 0.0f;
    rot[1].z = 0.0f;
    rot[2].x = 0.0f;
    rot[2].y = 0.0f;
    rot[1].y = 1.0f;
    rot[2].z = 1.0f;
}

/* out = v / |v| for x and y, or zero when |v| is zero. */
void normalizevector2d(float out[2], const float v[2])
{
    float length = (float)x87_sqrt((float)(v[0] * v[0] + v[1] * v[1]));
    float scale;
    if (length != 0.0) {
        scale = (float)(1.0 / length);
        out[0] = scale * v[0];
        out[1] = (float)(scale * v[1]);
        return;
    }
    out[0] = 0.0f;
    out[1] = 0.0f;
}
