/* Calls to the MSVC x87 intrinsics of the game CRT (MSVCR100.dll).
 *
 * _CIsqrt, _CIsin... take their arguments on the x87 stack (one in ST0, or two in ST1
 * and ST0) and return in ST0. Lifted code calls x87_*() with normal cdecl arguments.
 * Each thunk loads the arguments on the x87 stack and jumps to the game routine, which
 * returns straight to the caller. The result is bit-for-bit the same as in the original.
 */
#include "lift_support.h"

static void *p_sqrt, *p_sin, *p_cos, *p_tan, *p_acos, *p_atan2, *p_pow;

void x87_init(void)
{
    p_sqrt = sr_import("MSVCR100.dll", "_CIsqrt");
    p_sin = sr_import("MSVCR100.dll", "_CIsin");
    p_cos = sr_import("MSVCR100.dll", "_CIcos");
    p_tan = sr_import("MSVCR100.dll", "_CItan");
    p_acos = sr_import("MSVCR100.dll", "_CIacos");
    p_atan2 = sr_import("MSVCR100.dll", "_CIatan2");
    p_pow = sr_import("MSVCR100.dll", "_CIpow");
}

#define X87_ONE(NAME, PTR)                                         \
    __declspec(naked) double __cdecl NAME(double x)                \
    {                                                              \
        __asm fld qword ptr [esp + 4]                              \
        __asm jmp dword ptr [PTR]                                  \
    }

/* Two arguments: the first goes to ST1, the second to ST0. */
#define X87_TWO(NAME, PTR)                                         \
    __declspec(naked) double __cdecl NAME(double a, double b)      \
    {                                                              \
        __asm fld qword ptr [esp + 4]                              \
        __asm fld qword ptr [esp + 12]                             \
        __asm jmp dword ptr [PTR]                                  \
    }

X87_ONE(x87_sqrt, p_sqrt)
X87_ONE(x87_sin, p_sin)
X87_ONE(x87_cos, p_cos)
X87_ONE(x87_tan, p_tan)
X87_ONE(x87_acos, p_acos)
X87_TWO(x87_atan2, p_atan2)
X87_TWO(x87_pow, p_pow)
