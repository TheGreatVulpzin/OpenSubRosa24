/* Support header for C code lifted from Ghidra pseudo-C (instrumentation/lift/lift.py).
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

/* Structs of the game (Human, Object, Player...): ApplyTypes.java puts the same types into
 * Ghidra, so the pseudo-C uses them. */
#include "sr_types_lifted.h"

extern uintptr_t sr_base;

/* Image address (base 0x400000) -> real address in this process. */
#define SR_VA(va) ((void *)(sr_base + (uintptr_t)(va) - 0x400000u))

/* Image address of a function -> the function to call. With the hooks, the code of the 024d
 * (its hook jumps to the C version). In the standalone server there is no 024d code:
 * sr_fn() finds the C version (src/standalone/fn_table.c). */
#ifdef SR_STANDALONE
void *sr_fn(uintptr_t va);
#define SR_FN(va) sr_fn((uintptr_t)(va))
#else
#define SR_FN(va) SR_VA(va)
#endif

/* The relocation delta: how far the 024d moved from its file base. */
#define LF_RELOC ((int)(sr_base - 0x400000u))

/* Address of an import of the game (the value stored in its IAT slot). */
void *sr_import(const char *dll, const char *name);
void *sr_import_ordinal(const char *dll, unsigned ordinal);

/* x87 intrinsics of the game CRT, called with normal cdecl arguments. */
#include "sr_math.h"

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
typedef int code();  /* Ghidra "code *": a pointer to a function, called as (*p)() */
typedef void *pointer;    /* Ghidra generic pointer type */
typedef void *pointer32;

#define true 1
#define false 0

/* Ghidra x87 operations. ROUND = FRNDINT (current rounding mode, default to nearest). */
#define ABS(x) fabs(x)
#define ROUND(x) rint(x)
/* A FISTP after FLDCW with the rounding control "chop" (LiftExport.java, choppedRound). */
#define LF_CHOP(x) trunc(x)

/* RDTSC: CPU cycle counter. The game uses it only for its profiler (the timer table,
 * see sv_verify_ignore in instrumentation/hook/hooks_024d.c). */
unsigned __int64 __rdtsc(void);
#pragma intrinsic(__rdtsc)
#define rdtsc() __rdtsc()

/* Ghidra prints a NaN constant as "NAN" (the lifter writes LF_NAN). */
static const union { unsigned int u; float f; } lf_nan_bits = { 0x7fc00000u };
#define LF_NAN (lf_nan_bits.f)

/* Bit copy between a float and a 4-byte pointer. Ghidra merges a stack slot that holds
 * a pointer at one time and a float at another time into one variable. */
static __inline float lf_p2f(const void *p) { union { const void *p; float f; } u; u.p = p; return u.f; }
static __inline void *lf_f2p(float f) { union { void *p; float f; } u; u.f = f; return u.p; }

/* Bit copy between a float and a 4-byte int (an argument that the caller and the callee
 * type differently: the original pushes the same 4 bytes). */
static __inline float lf_u2f(unsigned int x) { union { unsigned int u; float f; } u; u.u = x; return u.f; }
static __inline unsigned int lf_f2u(float f) { union { unsigned int u; float f; } u; u.f = f; return u.u; }

/* Bit copies between a double and an 8-byte int. */
static __inline double lf_u2d(unsigned long long x) { union { unsigned long long u; double d; } u; u.u = x; return u.d; }
static __inline unsigned long long lf_d2u(double d) { union { unsigned long long u; double d; } u; u.d = d; return u.u; }

/* 16-byte SSE values. Ghidra declares them "undefined1 auVar1 [16]", and C cannot assign
 * an array: instrumentation/lift/lift.py uses this struct. lf_dpps runs the same DPPS instruction as
 * the original (bondsimulation2), so the result is the same to the bit. */
#include <smmintrin.h>
typedef struct { __declspec(align(16)) unsigned char b[16]; } lf_m128;
static __inline lf_m128 lf_dpps(lf_m128 a, lf_m128 b, int imm)
{
    lf_m128 r;
    __m128 x = _mm_loadu_ps((const float *)a.b), y = _mm_loadu_ps((const float *)b.b);
    switch (imm) {  /* the immediate must be a constant */
    case 0x7f: _mm_storeu_ps((float *)r.b, _mm_dp_ps(x, y, 0x7f)); break;
    case 0xff: _mm_storeu_ps((float *)r.b, _mm_dp_ps(x, y, 0xff)); break;
    case 0x71: _mm_storeu_ps((float *)r.b, _mm_dp_ps(x, y, 0x71)); break;
    default: __debugbreak();
    }
    return r;
}

/* The x87 control word (fnstcw). Ghidra shows it as in_FPUControlWord (packobjectpacket). */
static __inline unsigned short lf_fpucw(void)
{
    unsigned short cw;
    __asm fnstcw cw
    return cw;
}

/* Ghidra "builtin_strncpy(dst, "text", n)": the original stores the n bytes of the
 * constant (with its NUL) as immediate moves. */
static __inline void lf_strncpy(void *d, const void *s, unsigned int n)
{
    unsigned char *a = (unsigned char *)d;
    const unsigned char *b = (const unsigned char *)s;
    while (n--)
        *a++ = *b++;
}
#define builtin_strncpy(d, s, n) lf_strncpy((d), (s), (n))

/* Bit casts that instrumentation/ghidra/LiftExport.java marks (p-code CAST, not INT2FLOAT/TRUNC). */
#define LF_BITS_F(x) lf_u2f((unsigned int)(x))
#define LF_BITS_U(x) lf_f2u((float)(x))
#define LF_BITS_D(x) lf_u2d((unsigned long long)(x))
#define LF_BITS_U64(x) lf_d2u((double)(x))

/* The same bit casts on an lvalue, with integer loads and stores: no value passes the x87.
 * A small integer in a float is a denormal, and an x87 load of it takes about 100 cycles.
 * Hand-written code in src/game/ uses these (instrumentation/lift/int_bits.py). */
#define LF_UINT(x) (*(unsigned int *)&(x))
#define LF_SET_BITS(x, v) (*(unsigned int *)&(x) = (unsigned int)(v))
#define LF_COPY(dst, src) (*(unsigned int *)&(dst) = *(const unsigned int *)&(src))

/* The 4 bytes of a CONCAT piece: the bits of a float, the value of an int. */
#define LF_U32(x) _Generic((x), float: lf_f2u((float)(x)), double: lf_f2u((float)(x)), \
                           long double: lf_f2u((float)(x)), default: (uint)(x))

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
#define CONCAT44(a, b) ((ulonglong)(((ulonglong)LF_U32(a) << 32) | LF_U32(b)))

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
