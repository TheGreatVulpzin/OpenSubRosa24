/* Math intrinsics take arguments on the x87 stack and return in ST0.
 * Each thunk loads C arguments onto that stack and jumps to the selected CRT routine. */
#include "sr_math.h"

void *sr_import(const char *dll, const char *name);

/* Inline assembly reads the arguments from the stack. */
#pragma warning(disable : 4100)

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
