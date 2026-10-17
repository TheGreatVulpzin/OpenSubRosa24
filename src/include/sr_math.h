/* The x87 math functions of the game CRT, with normal cdecl arguments (x87_intrinsics.c).
 *
 * They give the same bits as the 024d: the CRT computes sin, cos and the others with its own
 * code, and a different libm differs in the last bit. Call x87_init once at the start.
 */
#ifndef SR_MATH_H
#define SR_MATH_H

void x87_init(void);
double __cdecl x87_sqrt(double x);
double __cdecl x87_sin(double x);
double __cdecl x87_cos(double x);
double __cdecl x87_tan(double x);
double __cdecl x87_acos(double x);
double __cdecl x87_atan2(double y, double x);
double __cdecl x87_pow(double base, double exponent);

#endif /* SR_MATH_H */
