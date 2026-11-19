/* The test of a point against a circle and the engine torque. See circles.c. */
#ifndef CL_MATH_CIRCLES_H
#define CL_MATH_CIRCLES_H

int __cdecl pointintersectcircle(float *out, float *distance, const float *point, const float *center, float radius);
float __cdecl getenginetorque(float rpm);

#endif /* CL_MATH_CIRCLES_H */
