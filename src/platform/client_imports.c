/* Look up a function of a system library by name or by number.
 *
 * The game code asks for the functions of the C runtime, the system and the media libraries
 * through sr_import. A function of the C runtime is a function of this executable. Any other
 * function comes from its DLL. A missing function ends the program. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The static C runtime (client_crt.c). */
void *client_crt_lookup(const char *name);

#ifdef SR_SMOKE
/* The test build wraps the input and the frame calls (src/standalone/client_smoke.c). */
void *client_smoke_import(const char *dll, const char *name, void *function);
#define WRAP(dll, name, function) client_smoke_import(dll, name, function)
#else
#define WRAP(dll, name, function) (function)
#endif

void *sr_import(const char *dll, const char *name)
{
    HMODULE module;
    void *function = NULL;
    if (!_stricmp(dll, "MSVCR100.DLL"))
        function = client_crt_lookup(name);
    if (function)
        return function;
    module = LoadLibraryA(dll);
    function = module ? (void *)GetProcAddress(module, name) : NULL;
    if (!function) {
        fprintf(stderr, "import %s!%s not found\n", dll, name);
        exit(1);
    }
    return WRAP(dll, name, function);
}

void *sr_import_ordinal(const char *dll, unsigned ordinal)
{
    HMODULE module = LoadLibraryA(dll);
    void *function = module ? (void *)GetProcAddress(module, (LPCSTR)(uintptr_t)ordinal) : NULL;
    if (!function) {
        fprintf(stderr, "import %s!#%u not found\n", dll, ordinal);
        exit(1);
    }
    return function;
}
