/* Open a web page in the default browser of the system. */
#include "menu/web.h"

#include <stddef.h>

void *sr_import(const char *dll, const char *name);

enum { SHOW_NORMAL = 1 };

/* Ask the shell to open the address. The text can be a web address or a file path. */
void __cdecl launchwebpage(const char *url)
{
    static void *(__stdcall * execute)(void *, const char *, const char *, const char *, const char *, int);
    if (!execute)
        *(void **)&execute = sr_import("SHELL32.DLL", "ShellExecuteA");
    execute(NULL, "open", url, NULL, NULL, SHOW_NORMAL);
}
