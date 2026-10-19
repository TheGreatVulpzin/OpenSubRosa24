/* Math for 3D vectors (vec3) and orientations (3 rows of vec3). See vectors.c. */
#ifndef SR_VECTORS_H
#define SR_VECTORS_H

#include "sr_types.h"

/* A rotation as a quaternion. w is the scalar part. */
typedef struct quat {
    float x, y, z, w;
} quat;

float dotproduct(const vec3 *a, const vec3 *b);
void crossproduct(vec3 *out, const vec3 *a, const vec3 *b);
float vectorlength(const vec3 *v);
void normalizevector(vec3 *out, const vec3 *v);
void addvectors(vec3 *out, const vec3 *a, const vec3 *b);
void subtractvectors(vec3 *out, const vec3 *a, const vec3 *b);
void scalevector(vec3 *out, const vec3 *v, float s);
void scaleaddvectors(vec3 *out, const vec3 *a, const vec3 *b, float s);
float vectordistance(const vec3 *a, const vec3 *b);
void projectvector(vec3 *out, const vec3 *v, const vec3 rot[3]);
void generatetrinormal(vec3 *out, const vec3 *p0, const vec3 *p1, const vec3 *p2);
void resetorientation(vec3 rot[3]);
void normalizevector2d(float out[2], const float v[2]);
void rotatevector2d(float v[2], float angle);
float angulardistance(float a, float b);
void geteulerangles2(vec3 *angles, const vec3 rot[3]);
void orientationtoquaternion(quat *q, const vec3 rot[3]);
void projectorientation(quat *out, const quat *p, const quat *q);
void rotatevector(vec3 *v, const vec3 *axis, float angle);
void rotateorientation(vec3 rot[3], const vec3 *axis, float angle);
void multiplyquaternion(vec3 out[3], const vec3 p[3], const vec3 q[3]);
void multiply3x3matrices(vec3 out[3], const vec3 a[3], const vec3 b[3]);

#endif /* SR_VECTORS_H */
