/* Hooks of the client proxy: the lifted functions of the lifter and the record-only hooks. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "hook.h"
#include "verify.h"

int sv_hooks_lifted_install(void);
void sv_hooks_lifted_summary(void);
int sv_hooks_record_install(void);

int cl_hooks_install(void)
{
    int n = 0;
    sv_verify_init();
    /* subvulpes.ini lifted=1 runs the lifted C of the pure functions. The default is off. */
    if (sv_config_int("lifted", 0))
        n += sv_hooks_lifted_install();
    /* Record-only hooks of the side-effect functions (record_imports=1). */
    n += sv_hooks_record_install();
    return n;
}

void cl_hooks_summary(void)
{
    sv_hooks_lifted_summary();
}
