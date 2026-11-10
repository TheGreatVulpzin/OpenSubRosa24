/* Noise and curves for the generated textures and models. See noise.c. */
#ifndef CL_TEXTURES_NOISE_H
#define CL_TEXTURES_NOISE_H

#include <stdint.h>

float __cdecl serp(float a, float b, float t);
float __cdecl cerp(float a, float b, float t);
float __cdecl noise1d(uint32_t n);
float __cdecl noise2d(int x, int y);
float __cdecl noise3d(int x, int y, int z);
void __cdecl hermiteinterpolation(float *out, float t, const float *start, const float *start_tangent,
                                  const float *end, const float *end_tangent);
float __cdecl engine_bezierinterpolation(int degree, float t, float p0, float p1, float p2, float p3);

#endif /* CL_TEXTURES_NOISE_H */
