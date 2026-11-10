/* Debug drawing and debug values of the client. See debug.c. */
#ifndef CL_DEBUG_DEBUG_H
#define CL_DEBUG_DEBUG_H

#include "sr_types.h"

void __cdecl debug_addcross(const vec3 *center, float size);
void __cdecl debug_addataint(const char *name, int value);
void __cdecl debug_addatafloat(const char *name, float value);
void __cdecl debug_writelogfile(const char *text);

#endif /* CL_DEBUG_DEBUG_H */
