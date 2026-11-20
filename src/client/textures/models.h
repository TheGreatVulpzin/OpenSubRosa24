/* The model in work and the builders that fill it. See models.c. */
#ifndef CL_TEXTURES_MODELS_H
#define CL_TEXTURES_MODELS_H

#include "sr_types.h"

int __cdecl modeltempvertsequal(int a, int b);
int __cdecl modeltemp_addvertex(const vec3 *pos, float tolerance);
void __cdecl copymodel(int model);
void __cdecl setupmodel(int model, int flat);
int modeltemp_addtri(int a, int b, int c, int shading);
void modeltemp_quadtotris(int quad);
void modeltemp_smoothnormals(void);
void __cdecl addboxtomodel(const vec3 *center, float half_x, float half_y, float half_z);
void __cdecl model_settritexcoord(int tri, float u, float v);
void __cdecl model_setquadtexcoord(int quad, float u, float v);
void __cdecl model_extrudequad(int quad, const vec3 *offset);
void __cdecl model_extrudequad2(int quad);
void __cdecl model_extrudequadscale(int quad, const vec3 *offset, float scale_x, float scale_y, float scale_z);
void __cdecl createmodel_cityobject_type(int model, int type);
void __cdecl createmodel_cityobject_elevator(int model, int type);
void __cdecl createmodel_cityobject_desk(int model);
void __cdecl createmodel_cityobject(int model, int type);
void __cdecl createhairmodel(int model);
void __cdecl createhumanmodel(int model);
void addhalfboxtomodel(const vec3 *center, const vec3 rot[3], float half_x, float half_y, float half_z, int open_face);
void addwaisttomodel(const vec3 *center, float half_x, float half_y, float half_z);
void createfootmodel(int model, float half_x, float half_y, float half_z);
void createrotormodel(int model, float half_x, float half_y, float half_z);
void createsunglassmodel(int model);
void createheadmodel(int model);
void createcellphonemodel(int model, float half_x, float half_y, float half_z);
void createkeymodel(int model);
void sethumanmodelvert(int vertex, int bone, float x, float y, float z);
void sethumanmodelvertface(int vertex, int face, float x, float y, float z);
void tesselatebeziertriangle(float out[3], float u, float v, float w, const int points[10]);
void savemodelcsm(int model, const char *path);
int loadmodelcsm(int model, const char *path);

#endif /* CL_TEXTURES_MODELS_H */
