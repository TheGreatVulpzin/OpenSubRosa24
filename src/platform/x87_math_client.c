/* Math functions of the clean game code, for the client.
 * The server calls the _CI forms of the CRT (argument in ST0). The client calls the plain CRT
 * functions (sqrt, sin, cos, tan, acos, atan2, pow, exp): the argument is on the stack. Each thunk jumps
 * to the same function the client uses, so the result has the same bits. */
#include "sr_math.h"

#ifdef SR_CLIENT_NATIVE
/* The native client calls the functions of its own C runtime. The pragma keeps the compiler from
 * replacing a call by an instruction. */
#include <math.h>

#pragma function(sqrt, sin, cos, tan, acos, atan2, pow, exp)

void x87_init(void)
{
}

double __cdecl x87_sqrt(double x) { return sqrt(x); }
double __cdecl x87_sin(double x) { return sin(x); }
double __cdecl x87_cos(double x) { return cos(x); }
double __cdecl x87_tan(double x) { return tan(x); }
double __cdecl x87_acos(double x) { return acos(x); }
double __cdecl x87_atan2(double a, double b) { return atan2(a, b); }
double __cdecl x87_pow(double a, double b) { return pow(a, b); }
double __cdecl x87_exp(double x) { return exp(x); }

#else

void *sr_import(const char *dll, const char *name);

#pragma warning(disable : 4100)

static void *p_sqrt, *p_sin, *p_cos, *p_tan, *p_acos, *p_atan2, *p_pow, *p_exp;

void x87_init(void)
{
    p_sqrt = sr_import("MSVCR100.DLL", "sqrt");
    p_sin = sr_import("MSVCR100.DLL", "sin");
    p_cos = sr_import("MSVCR100.DLL", "cos");
    p_tan = sr_import("MSVCR100.DLL", "tan");
    p_acos = sr_import("MSVCR100.DLL", "acos");
    p_atan2 = sr_import("MSVCR100.DLL", "atan2");
    p_pow = sr_import("MSVCR100.DLL", "pow");
    p_exp = sr_import("MSVCR100.DLL", "exp");
}

#define PLAIN(NAME, PTR, ...)                                      \
    __declspec(naked) double __cdecl NAME(__VA_ARGS__)             \
    {                                                              \
        __asm jmp dword ptr [PTR]                                  \
    }

PLAIN(x87_sqrt, p_sqrt, double x)
PLAIN(x87_sin, p_sin, double x)
PLAIN(x87_cos, p_cos, double x)
PLAIN(x87_tan, p_tan, double x)
PLAIN(x87_acos, p_acos, double x)
PLAIN(x87_atan2, p_atan2, double a, double b)
PLAIN(x87_pow, p_pow, double a, double b)
PLAIN(x87_exp, p_exp, double x)

#endif /* SR_CLIENT_NATIVE */
