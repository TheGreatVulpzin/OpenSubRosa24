/* Support header for C code lifted from Ghidra pseudo-C (tools/lift/lift.py).
 *
 * It defines the Ghidra data types and helper macros (CONCAT, SUB, ZEXT...) so that
 * the decompiler output compiles with MSVC. Lifted code reaches the game memory with
 * SR_VA(va): va is the address in the file image (base 0x400000), relocated at run time.
 */
#ifndef SR_LIFT_SUPPORT_H
#define SR_LIFT_SUPPORT_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

extern uintptr_t sr_base;

/* Image address (base 0x400000) -> real address in this process. */
#define SR_VA(va) ((void *)(sr_base + (uintptr_t)(va) - 0x400000u))

/* Address of an import of the game (the value stored in its IAT slot). */
void *sr_import(const char *dll, const char *name);

/* x87 intrinsics of the game CRT, called with normal cdecl arguments (x87_intrinsics.c). */
void lf_x87_init(void);
double __cdecl lf_CIsqrt(double x);
double __cdecl lf_CIsin(double x);
double __cdecl lf_CIcos(double x);
double __cdecl lf_CItan(double x);
double __cdecl lf_CIacos(double x);
double __cdecl lf_CIatan2(double a, double b);
double __cdecl lf_CIpow(double a, double b);

/* ---------------------------------------------------------------- Ghidra types */

typedef unsigned char undefined;
typedef unsigned char undefined1;
typedef unsigned short undefined2;
typedef unsigned int undefined3;
typedef unsigned int undefined4;
typedef unsigned long long undefined5;
typedef unsigned long long undefined6;
typedef unsigned long long undefined7;
typedef unsigned long long undefined8;
typedef unsigned char byte;
typedef unsigned char uchar;
typedef signed char sbyte;
typedef unsigned short ushort;
typedef unsigned short word;
typedef unsigned int uint;
typedef unsigned int dword;
typedef unsigned long ulong;
typedef long long longlong;
typedef unsigned long long ulonglong;
typedef unsigned long long qword;
typedef unsigned char bool;
typedef unsigned short wchar16;
typedef double float10;   /* x87 80-bit in the original; MSVC has no 80-bit type */
typedef void code;

#define true 1
#define false 0

/* Ghidra x87 operations. ROUND = FRNDINT (current rounding mode, default to nearest). */
#define ABS(x) fabs(x)
#define ROUND(x) rint(x)

/* Ghidra NAN(x): x is not a number (x87 compare with the parity flag). */
#define NAN(x) ((x) != (x))

/* ---------------------------------------------------------------- Ghidra helper ops */

#define CONCAT11(a, b) ((ushort)(((uint)(uchar)(a) << 8) | (uchar)(b)))
#define CONCAT12(a, b) ((uint)(((uint)(uchar)(a) << 16) | (ushort)(b)))
#define CONCAT13(a, b) ((uint)(((uint)(uchar)(a) << 24) | ((uint)(b) & 0xffffffu)))
#define CONCAT21(a, b) ((uint)(((uint)(ushort)(a) << 8) | (uchar)(b)))
#define CONCAT22(a, b) ((uint)(((uint)(ushort)(a) << 16) | (ushort)(b)))
#define CONCAT31(a, b) ((uint)(((uint)(a) << 8) | (uchar)(b)))
#define CONCAT14(a, b) ((ulonglong)(((ulonglong)(uchar)(a) << 32) | (uint)(b)))
#define CONCAT44(a, b) ((ulonglong)(((ulonglong)(uint)(a) << 32) | (uint)(b)))

#define SUB41(x, n) ((uchar)((uint)(x) >> ((n) * 8)))
#define SUB42(x, n) ((ushort)((uint)(x) >> ((n) * 8)))
#define SUB43(x, n) ((uint)(((uint)(x) >> ((n) * 8)) & 0xffffffu))
#define SUB81(x, n) ((uchar)((ulonglong)(x) >> ((n) * 8)))
#define SUB82(x, n) ((ushort)((ulonglong)(x) >> ((n) * 8)))
#define SUB84(x, n) ((uint)((ulonglong)(x) >> ((n) * 8)))
#define SUB21(x, n) ((uchar)((ushort)(x) >> ((n) * 8)))

#define ZEXT12(x) ((ushort)(uchar)(x))
#define ZEXT14(x) ((uint)(uchar)(x))
#define ZEXT24(x) ((uint)(ushort)(x))
#define ZEXT48(x) ((ulonglong)(uint)(x))
#define SEXT12(x) ((short)(signed char)(x))
#define SEXT14(x) ((int)(signed char)(x))
#define SEXT24(x) ((int)(short)(x))
#define SEXT48(x) ((longlong)(int)(x))

#define CARRY1(a, b) ((uchar)((uchar)(a) + (uchar)(b)) < (uchar)(a))
#define CARRY2(a, b) ((ushort)((ushort)(a) + (ushort)(b)) < (ushort)(a))
#define CARRY4(a, b) ((uint)((uint)(a) + (uint)(b)) < (uint)(a))
#define SCARRY4(a, b) ((((int)(a) < 0) == ((int)(b) < 0)) && (((int)((uint)(a) + (uint)(b)) < 0) != ((int)(a) < 0)))
#define SBORROW4(a, b) ((((int)(a) < 0) != ((int)(b) < 0)) && (((int)((uint)(a) - (uint)(b)) < 0) != ((int)(a) < 0)))
#define SBORROW1(a, b) ((((signed char)(a) < 0) != ((signed char)(b) < 0)) && (((signed char)((uchar)(a) - (uchar)(b)) < 0) != ((signed char)(a) < 0)))

#endif /* SR_LIFT_SUPPORT_H */
