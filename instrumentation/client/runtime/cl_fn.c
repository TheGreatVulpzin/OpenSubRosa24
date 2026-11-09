/* The lookup hook of CL_FN (src/client/client.h). The standalone client sets it. */
#include "client.h"

void *(*cl_fn_hook)(unsigned va);
