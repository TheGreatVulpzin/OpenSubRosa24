/* The model in work and the builders that fill it. See models.c. */
#ifndef CL_TEXTURES_MODELS_H
#define CL_TEXTURES_MODELS_H

#include "sr_types.h"

int __cdecl modeltempvertsequal(int a, int b);
int __cdecl modeltemp_addvertex(const vec3 *pos, float tolerance);
void __cdecl copymodel(int model);
void __cdecl addboxtomodel(const vec3 *center, float half_x, float half_y, float half_z);
void __cdecl model_settritexcoord(int tri, float u, float v);
void __cdecl model_setquadtexcoord(int quad, float u, float v);
void __cdecl model_extrudequad(int quad, const vec3 *offset);
void __cdecl model_extrudequad2(int quad);
void __cdecl model_extrudequadscale(int quad, const vec3 *offset, float scale_x, float scale_y, float scale_z);
void __cdecl createmodel_cityobject_type(int model, int type);
void __cdecl createmodel_cityobject_elevator(int model, int type);
void __cdecl createmodel_cityobject(int model, int type);

#endif /* CL_TEXTURES_MODELS_H */
