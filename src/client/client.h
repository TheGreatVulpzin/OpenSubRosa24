/* Shared header of the clean client code (src/client).
 *
 * The client code calls every game function by its name. The test tools of instrumentation/ use
 * CL_FN(va, type) to call a function of the Windows executable by its address. */
#ifndef CL_CLIENT_H
#define CL_CLIENT_H

#include <emmintrin.h>
#include <stdint.h>

#ifndef SR_CLIENT_NATIVE
extern uintptr_t sr_base;
extern void *(*cl_fn_hook)(unsigned va);

static __inline void *cl_fn(unsigned va)
{
    return cl_fn_hook ? cl_fn_hook(va) : (void *)(sr_base + va - 0x400000u);
}

#define CL_FN(va, type) ((type)cl_fn(va))
#endif

/* Convert a float value to an int. The value rounds to a double, then loses its fraction. A
 * value that is not a number, or that does not fit in an int, gives 0x80000000. */
static __inline int cl_ftoi(double value)
{
    return _mm_cvttsd_si32(_mm_set_sd(value));
}

#endif
