/* The x87 math functions with normal cdecl arguments (x87_math.c).
 *
 * They give the same bits as the CRT of the game: it computes sin, cos and the others with its
 * own code, and a different libm differs in the last bit. Call x87_init once at the start.
 *
 * All the float math of the game runs on the x87: the files compile with /arch:IA32. Each sum
 * and product keeps 53-bit precision, and each (float) rounds the value to a float. So the
 * order of the terms and each (float) matter: a change gives a different last bit, and the
 * physics drifts away from the game.
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
