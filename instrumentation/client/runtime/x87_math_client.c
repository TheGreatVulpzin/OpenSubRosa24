/* Math functions of the clean game code, for the client.
 * The server calls the _CI forms of the CRT (argument in ST0). The client calls the plain CRT
 * functions (sqrt, sin, cos, tan, acos, atan2, pow): the argument is on the stack. Each thunk jumps
 * to the same function the client uses, so the result has the same bits. */
#include "sr_math.h"

void *sr_import(const char *dll, const char *name);

#pragma warning(disable : 4100)

static void *p_sqrt, *p_sin, *p_cos, *p_tan, *p_acos, *p_atan2, *p_pow;

void x87_init(void)
{
    p_sqrt = sr_import("MSVCR100.DLL", "sqrt");
    p_sin = sr_import("MSVCR100.DLL", "sin");
    p_cos = sr_import("MSVCR100.DLL", "cos");
    p_tan = sr_import("MSVCR100.DLL", "tan");
    p_acos = sr_import("MSVCR100.DLL", "acos");
    p_atan2 = sr_import("MSVCR100.DLL", "atan2");
    p_pow = sr_import("MSVCR100.DLL", "pow");
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
