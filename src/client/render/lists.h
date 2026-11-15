/* Frame models, sector shadows and display modes. */
#ifndef CL_RENDER_LISTS_H
#define CL_RENDER_LISTS_H
#include "sr_types.h"
void __cdecl render_buildareasectorshadow(void);
void __cdecl render_areasectorshadow(void);
void __cdecl buildhumanmodel(int human, int model);
void __cdecl frame_addmodel(int model, const int textures[4], uint32_t flags, const vec3 *pos, const vec3 rot[3]);
void __cdecl rendermodellist(int mode);
void __cdecl frame_rendermessages(void);
void __cdecl screenshot(void);
void __cdecl listvideomodes(void);
#endif
