/* Math for 3D vectors (vec3), orientations (3 rows of vec3), quaternions and angles.
 */
#include "vectors.h"

#include "sr_math.h"
#include "sr_runtime.h"

/* The values of pi that the game uses (double). */
#define PI 3.14159265359
#define TWO_PI 6.28318530718

/* Negate each component in order. */
void negvector(vec3 *out, const vec3 *v)
{
    out->x = -v->x;
    out->y = -v->y;
    out->z = -v->z;
}

/* Transpose a matrix. The input can also hold the result. */
void transposematrix(vec3 out[3], const vec3 in[3])
{
    vec3 rows[3];
    rows[0] = in[0];
    rows[1] = in[1];
    rows[2] = in[2];
    out[0].x = rows[0].x;
    out[0].y = rows[1].x;
    out[0].z = rows[2].x;
    out[1].x = rows[0].y;
    out[1].y = rows[1].y;
    out[1].z = rows[2].y;
    out[2].x = rows[0].z;
    out[2].y = rows[1].z;
    out[2].z = rows[2].z;
}

/* Copy quaternion components in order. */
void copyquaternion(quat *out, const quat *in)
{
    out->x = in->x;
    out->y = in->y;
    out->z = in->z;
    out->w = in->w;
}

/* Form an orientation from the rounded quaternion products. */
void quaterniontoorientation(vec3 out[3], const quat *q)
{
    float wx = (float)(q->w * q->x + q->w * q->x);
    float wy = (float)(q->w * q->y + q->w * q->y);
    float wz = (float)(q->w * q->z + q->w * q->z);
    float xx = (float)(q->x * q->x + q->x * q->x);
    float xy = (float)(q->x * q->y + q->x * q->y);
    float xz = (float)(q->x * q->z + q->x * q->z);
    float yy = (float)(q->y * q->y + q->y * q->y);
    float yz = (float)(q->y * q->z + q->y * q->z);
    float zz = (float)(q->z * q->z + q->z * q->z);
    out[0].x = (float)(1.0 - ((double)yy + zz));
    out[0].y = xy - wz;
    out[0].z = xz + wy;
    out[1].x = xy + wz;
    out[1].y = (float)(1.0 - ((double)xx + zz));
    out[1].z = yz - wx;
    out[2].x = xz - wy;
    out[2].y = yz + wx;
    out[2].z = (float)(1.0 - ((double)xx + yy));
}

/* Return the rounded two component dot product. */
float dotproduct2d(const float a[2], const float b[2])
{
    return (float)((double)a[1] * b[1] + (double)a[0] * b[0]);
}

/* Keep the square root in double precision. */
double vectorlength2d(const float v[2])
{
    return x87_sqrt(dotproduct2d(v, v));
}

/* Add two vectors in component order. */
void addvectors2d(float out[2], const float a[2], const float b[2])
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
}

/* Subtract two vectors in component order. */
void subtractvectors2d(float out[2], const float a[2], const float b[2])
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
}

/* Scale two components in order. */
void scalevector2d(float out[2], const float v[2], float scale)
{
    out[0] = v[0] * scale;
    out[1] = v[1] * scale;
}

/* Add a scaled vector in component order. */
void scaleaddvectors2d(float out[2], const float a[2], const float b[2], float scale)
{
    out[0] = (float)((double)b[0] * scale + a[0]);
    out[1] = (float)((double)b[1] * scale + a[1]);
}

/* Turn a vector one quarter turn in component order. */
void tangentvector2d(float out[2], const float v[2])
{
    out[0] = -v[1];
    out[1] = v[0];
}

/* a . b: x and y first, then z. */
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
    out->y = (float)(((float)(ax - ox)) * sr_triangle_edge(bz - oz) - ((float)(bx - ox)) * sr_triangle_edge(az - oz));
    out->z = (float)(sr_triangle_edge(bx - ox) * sr_triangle_edge(ay - oy) - sr_triangle_edge(ax - ox) * sr_triangle_edge(by - oy));
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

/* The difference a - b of two angles in radians. One turn (2 pi) goes on a when b is at
 * least pi ahead, and on b when a is at least pi ahead. */
float angulardistance(float a, float b)
{
    float diff = b - a;
    if (diff >= PI)
        a = (float)(a + TWO_PI);
    if (a - b >= PI)
        return (float)(a - (float)(b + TWO_PI));
    return (float)(a - b);
}

/* The Euler angles of an orientation: x from row 0 and row 1, y from row 2, then z with the
 * sine and cosine of x. */
void geteulerangles2(vec3 *angles, const vec3 rot[3])
{
    float s, c;
    angles->x = (float)x87_atan2(rot[0].y, rot[1].y);
    angles->y = (float)x87_atan2(-rot[2].y, (float)x87_sqrt(rot[2].x * rot[2].x + rot[2].z * rot[2].z));
    s = (float)x87_sin(angles->x);
    c = (float)x87_cos(angles->x);
    angles->z = (float)x87_atan2(s * rot[1].z - c * rot[0].z, rot[0].x * c - rot[1].x * s);
}

/* The quaternion of an orientation, from its trace. A trace below -1 counts as -1. */
void orientationtoquaternion(quat *q, const vec3 rot[3])
{
    float trace = (float)(rot[1].y + rot[0].x + rot[2].z);
    float s;
    double root, inv; /* not rounded to a float */
    if (trace < -1.0)
        trace = -1.0f;
    root = x87_sqrt(trace + 1.0);
    s = (float)(root + root);
    if (!(1.52587890625E-5 < s)) /* a NaN also gives 0 */
        inv = 0.0;
    else
        inv = 1.0 / s;
    q->w = (float)(s * 0.25);
    q->x = (float)((rot[2].y - rot[1].z) * inv);
    q->y = (float)((rot[0].z - rot[2].x) * inv);
    q->z = (float)((rot[1].x - rot[0].y) * inv);
}

/* out = p * q (the product of two quaternions). The inputs are read before the first write,
 * so out can be p or q. */
void multiplyquaternion(quat *out, const quat *p, const quat *q)
{
    float px = p->x, py = p->y, pz = p->z, pw = p->w;
    float qx = q->x, qy = q->y, qz = q->z, qw = q->w;
    out->x = (float)((py * qz + (qx * pw + px * qw)) - pz * qy);
    out->y = (float)(pz * qx + (py * qw + (qy * pw - px * qz)));
    out->z = (float)(pz * qw + ((qz * pw + qy * px) - py * qx));
    out->w = (float)(((pw * qw - px * qx) - qy * py) - pz * qz);
}

/* Turns v by angle radians around the unit vector axis (Rodrigues). */
void rotatevector(vec3 *v, const vec3 *axis, float angle)
{
    float ax = axis->x, ay = axis->y, az = axis->z;
    float c = (float)x87_cos(angle), s = (float)x87_sin(angle);
    float cx = (float)(az * v->y - ay * v->z);
    float cy = (float)(ax * v->z - v->x * az);
    float cz = (float)(v->x * ay - v->y * ax);
    float dot = (float)(v->z * az + (v->y * ay + v->x * ax));
    v->x = (float)(cx * s + ((float)(((float)(cz * ay - cy * az)) * c + dot * ax)));
    v->y = (float)((float)(c * ((float)(cx * az - cz * ax)) + dot * ay) + s * cy);
    v->z = (float)(cz * s + ((float)(az * dot + ((float)(cy * ax - cx * ay)) * c)));
}

/* Turns each row of rot by angle radians around the unit vector axis, then makes the row a
 * unit vector again. Each step writes the row before the next step reads it. */
void rotateorientation(vec3 rot[3], const vec3 *axis, float angle)
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
        normalizevector(row, row);
    }
}

/* out[j] = (p[0] . q[j], p[1] . q[j], p[2] . q[j]): the product of two orientations. The
 * name comes from the game, but the inputs are orientations, not quaternions. The inputs are
 * read before the first write, so out can be p or q. */
void projectorientation(vec3 out[3], const vec3 p[3], const vec3 q[3])
{
    vec3 r[3];
    int j;
    for (j = 0; j < 3; j++) {
        r[j].x = dotproduct(&p[0], &q[j]);
        r[j].y = dotproduct(&p[1], &q[j]);
        r[j].z = dotproduct(&p[2], &q[j]);
    }
    out[0] = r[0];
    out[1] = r[1];
    out[2] = r[2];
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

/* Turns x and y of v by angle (radians). */
void rotatevector2d(float v[2], float angle)
{
    float x = v[0], y = v[1];
    float c = (float)x87_cos(angle), s = (float)x87_sin(angle);
    v[0] = (float)(s * -y + c * v[0]);
    v[1] = (float)(x * s + v[1] * c);
}

/* out = a * b. Each sum starts at 0, so a product of -0 gives +0. out may be a or b. */
void multiply3x3matrices(vec3 out[3], const vec3 a[3], const vec3 b[3])
{
    vec3 m[3];
    int i;
    for (i = 0; i < 3; i++) {
        m[i].x = (float)(b[2].x * a[i].z + (float)(b[1].x * a[i].y + (float)(b[0].x * a[i].x + 0.0f)));
        m[i].y = (float)(a[i].z * b[2].y + (float)(b[1].y * a[i].y + (float)(a[i].x * b[0].y + 0.0f)));
        m[i].z = (float)(b[2].z * a[i].z + (float)(b[1].z * a[i].y + (float)(b[0].z * a[i].x + 0.0f)));
    }
    out[0] = m[0];
    out[1] = m[1];
    out[2] = m[2];
}
