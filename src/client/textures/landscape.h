/* The mesh and the textures of the landscape. See landscape.c. */
#ifndef CL_TEXTURES_LANDSCAPE_H
#define CL_TEXTURES_LANDSCAPE_H

#include "sr_types.h"

void __cdecl landscape_buildsectorlods(void);
void __cdecl landscape_buildsectormesh(int sector);
void __cdecl landscape_buildmesh(void);
void __cdecl landscape_setheightf(const vec3 *pos, float height);
void __cdecl landscape_rendersector(int sector);
void __cdecl landscape_generatenormaltexture(void);
void __cdecl landscape_generatedetail(int mipmap, int wrap_s, int wrap_t, int mag_filter, int min_filter);

#endif /* CL_TEXTURES_LANDSCAPE_H */
