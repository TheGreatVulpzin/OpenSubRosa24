/* Matrices of 3 by 3 floats. */
#include "math/matrix.h"

/* Make the 3 by 3 orientation matrix of a unit quaternion (x, y, z, w). The matrix has 9 floats,
 * row by row. */
void __cdecl quaterniontoorientation(float *rot, const float *q)
{
    float xw = (float)(q[3] * q[0] + q[3] * q[0]);
    float yw = (float)(q[3] * q[1] + q[3] * q[1]);
    float zw = (float)(q[3] * q[2] + q[3] * q[2]);
    float xx = (float)(q[0] * q[0] + q[0] * q[0]);
    float xy = (float)(q[0] * q[1] + q[0] * q[1]);
    float xz = (float)(q[0] * q[2] + q[0] * q[2]);
    float yy = (float)(q[1] * q[1] + q[1] * q[1]);
    float yz = (float)(q[1] * q[2] + q[1] * q[2]);
    float zz = (float)(q[2] * q[2] + q[2] * q[2]);

    rot[0] = (float)(1.0 - (yy + zz));
    rot[1] = (float)(xy - zw);
    rot[2] = (float)(xz + yw);
    rot[3] = (float)(xy + zw);
    rot[4] = (float)(1.0 - (xx + zz));
    rot[5] = (float)(yz - xw);
    rot[6] = (float)(xz - yw);
    rot[7] = (float)(yz + xw);
    rot[8] = (float)(1.0 - (xx + yy));
}

/* Write the transpose of the matrix in into out. The two can be the same matrix. */
void __cdecl transposematrix(float *out, const float *in)
{
    float m[9];
    int i;

    for (i = 0; i < 9; i++)
        m[i] = in[i];
    out[0] = m[0];
    out[1] = m[3];
    out[2] = m[6];
    out[3] = m[1];
    out[4] = m[4];
    out[5] = m[7];
    out[6] = m[2];
    out[7] = m[5];
    out[8] = m[8];
}
