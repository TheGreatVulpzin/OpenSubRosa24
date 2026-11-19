/* Small edits of the model in work and of the temporary mesh. See mesh_edit.c. */
#ifndef CL_TEXTURES_MESH_EDIT_H
#define CL_TEXTURES_MESH_EDIT_H

#include "sr_types.h"

void __cdecl model_deletequad(int quad);
int __cdecl model_addvertex(const vec3 *place, float tolerance);
void __cdecl tempmesh_deletequad(int quad);
int __cdecl tempmesh_vertnormalequal(int a, int b);
void __cdecl addboxtomodelquad(const vec3 *center, float size_x, float size_y, float size_z);
float __cdecl lerp(float a, float b, float t);

#endif /* CL_TEXTURES_MESH_EDIT_H */
