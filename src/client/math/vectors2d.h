/* The vectors of two floats. See vectors2d.c. */
#ifndef CL_MATH_VECTORS2D_H
#define CL_MATH_VECTORS2D_H

float __cdecl dotproduct2d(const float *a, const float *b);
double __cdecl vectorlength2d(const float *v);
void __cdecl addvectors2d(float *out, const float *a, const float *b);
void __cdecl subtractvectors2d(float *out, const float *a, const float *b);
void __cdecl scalevector2d(float *out, const float *a, float scale);
void __cdecl scaleaddvectors2d(float *out, const float *a, const float *b, float scale);
void __cdecl tangentvector2d(float *out, const float *a);

#endif /* CL_MATH_VECTORS2D_H */
