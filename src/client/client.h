/* Shared header of the clean client code (src/client).
 *
 * CL_FN(va, type) calls a game function that is still lifted C. va is the address in subrosa.exe
 * (base 0x400000). In the standalone client it finds the C function. In the test tools it is the
 * original code in the image. When the function is rewritten in src/client, call it directly. */
#ifndef CL_CLIENT_H
#define CL_CLIENT_H

#include <stdint.h>

extern uintptr_t sr_base;
extern void *(*cl_fn_hook)(unsigned va);

static __inline void *cl_fn(unsigned va)
{
    return cl_fn_hook ? cl_fn_hook(va) : (void *)(sr_base + va - 0x400000u);
}

#define CL_FN(va, type) ((type)cl_fn(va))

#endif
