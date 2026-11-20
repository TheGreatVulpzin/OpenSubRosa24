/* The initial WGL loader table.
 * Each entry points to the resolver for one WGL extension function.
 * A resolver replaces its entry when the extension becomes available.
 */
#include "gl/loaders.h"
#include "sr_runtime.h"

/* Fill the table with the resolver entry points. */
void initWGLLoadFunctions(void)
{
    static const uintptr_t resolvers[] = {
        0x2ec10, 0x2ecd0, 0x2ece0, 0x2ed30, 0x2edd0, 0x2ee50, 0x2ef30,
        0x2efd0, 0x2efe0, 0x2f030, 0x2f0b0, 0x2f150, 0x2f230, 0x2f240,
        0x2f250, 0x2f260, 0x2f2e0, 0x2f3a0, 0x2f580, 0x2f640, 0x2f700,
        0x2f710, 0x2f720, 0x2f730, 0x2f740, 0x2f750, 0x2f760, 0x2f770,
        0x2f810, 0x2f920, 0x2fa00, 0x2fac0, 0x2fb10, 0x2fb90, 0x2fc10,
        0x2fd20, 0x2fde0
    };
    unsigned entry;
    for (entry = 0; entry < sizeof resolvers / sizeof resolvers[0]; ++entry)
        cl_data_wgl_loaders[entry] = (void *)(sr_base + resolvers[entry]);
}
