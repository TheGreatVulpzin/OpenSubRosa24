/* Open a web page in the default browser of the system. */
#include "menu/web.h"

#include <stddef.h>

#include "system_libs.h"

enum { SHOW_NORMAL = 1 };

/* Ask the shell to open the address. The text can be a web address or a file path. */
void __cdecl launchwebpage(const char *url)
{
    ShellExecuteA(NULL, "open", url, NULL, NULL, SHOW_NORMAL);
}
