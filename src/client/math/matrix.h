/* Matrices of 3 by 3 floats. See matrix.c. */
#ifndef CL_MATH_MATRIX_H
#define CL_MATH_MATRIX_H

void __cdecl quaterniontoorientation(float *rot, const float *q);
void __cdecl transposematrix(float *out, const float *in);

#endif /* CL_MATH_MATRIX_H */
