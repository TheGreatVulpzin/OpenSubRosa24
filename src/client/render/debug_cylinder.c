/* The debug view of a wheel. */
#include "render/debug_cylinder.h"

/* The debug cylinder of a wheel is not drawn. Only the debug view of vehicles calls this. */
void __cdecl debug_rendercylinder(const vec3 *position, const vec3 *rotation)
{
    (void)position;
    (void)rotation;
}
