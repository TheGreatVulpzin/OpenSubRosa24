/* Math intrinsics take arguments on the x87 stack and return in ST0.
 * Each thunk loads C arguments onto that stack and jumps to the selected CRT routine. */
#include "sr_math.h"

/* Inline assembly reads the arguments from the stack. */
#pragma warning(disable : 4100)

static void *p_sqrt, *p_sin, *p_cos, *p_tan, *p_acos, *p_atan2, *p_pow;

#ifdef SR_OS_DIRECT
/* The x87 routines of the static C runtime. They take their arguments on the x87 stack, so no
 * header declares them. */
void __cdecl _CIsqrt(void);
void __cdecl _CIsin(void);
void __cdecl _CIcos(void);
void __cdecl _CItan(void);
void __cdecl _CIacos(void);
void __cdecl _CIatan2(void);
void __cdecl _CIpow(void);

void x87_init(void)
{
    p_sqrt = (void *)_CIsqrt;
    p_sin = (void *)_CIsin;
    p_cos = (void *)_CIcos;
    p_tan = (void *)_CItan;
    p_acos = (void *)_CIacos;
    p_atan2 = (void *)_CIatan2;
    p_pow = (void *)_CIpow;
}
#else
void *sr_import(const char *dll, const char *name);

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
#endif

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
