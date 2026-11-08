/* The C runtime functions that the client imports from MSVCR100.dll.
 *
 * The standalone client links the runtime statically, so each import is a function of this
 * exe and the client does not need MSVCR100.dll. instrumentation/tests/crt_diff.c compares
 * the math, sscanf, sprintf, rand and qsort functions with MSVCR100.dll. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ctype.h>
#include <direct.h>
#include <io.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* assert.h declares it only when NDEBUG is off. */
void __cdecl _wassert(const wchar_t *message, const wchar_t *file, unsigned line);

/* The tables hold function addresses. Their types do not matter here. */
#pragma warning(disable : 4232)

typedef struct {
    const char *name;
    void *fn;
} Import;

static const Import g_crt[] = {
    { "__daylight", (void *)__daylight },
    { "__timezone", (void *)__timezone },
    { "_chdir", (void *)_chdir },
    { "_findclose", (void *)_findclose },
    { "_findfirst64i32", (void *)_findfirst64i32 },
    { "_findnext64i32", (void *)_findnext64i32 },
    { "_getcwd", (void *)_getcwd },
    { "_mkdir", (void *)_mkdir },
    { "_time64", (void *)_time64 },
    { "_tzset", (void *)_tzset },
    { "_wassert", (void *)_wassert },
    { "abs", (void *)abs },
    { "acos", (void *)acos },
    { "atan2", (void *)atan2 },
    { "cos", (void *)cos },
    { "exp", (void *)exp },
    { "fabs", (void *)fabs },
    { "fclose", (void *)fclose },
    { "feof", (void *)feof },
    { "fgetc", (void *)fgetc },
    { "floor", (void *)floor },
    { "fopen", (void *)fopen },
    { "fprintf", (void *)fprintf },
    { "fputc", (void *)fputc },
    { "fread", (void *)fread },
    { "free", (void *)free },
    { "fseek", (void *)fseek },
    { "ftell", (void *)ftell },
    { "fwrite", (void *)fwrite },
    { "ldexp", (void *)ldexp },
    { "malloc", (void *)malloc },
    { "memcpy", (void *)memcpy },
    { "memset", (void *)memset },
    { "pow", (void *)pow },
    { "qsort", (void *)qsort },
    { "rand", (void *)rand },
    { "realloc", (void *)realloc },
    { "sin", (void *)sin },
    { "sprintf", (void *)sprintf },
    { "sqrt", (void *)sqrt },
    { "srand", (void *)srand },
    { "sscanf", (void *)sscanf },
    { "strcat", (void *)strcat },
    { "strcmp", (void *)strcmp },
    { "strcpy", (void *)strcpy },
    { "strlen", (void *)strlen },
    { "strncmp", (void *)strncmp },
    { "strtol", (void *)strtol },
    { "tan", (void *)tan },
    { "tolower", (void *)tolower },
    { "toupper", (void *)toupper },
};

/* NULL: the client does not link this function. The caller then asks the DLL. */
void *client_crt_lookup(const char *name)
{
    size_t i;
    for (i = 0; i < sizeof g_crt / sizeof *g_crt; i++)
        if (!strcmp(g_crt[i].name, name))
            return g_crt[i].fn;
    return NULL;
}
