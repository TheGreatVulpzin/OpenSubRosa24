/* Small item models of boxes and cylinders. */
#ifndef CL_TEXTURES_SMALL_MODELS_H
#define CL_TEXTURES_SMALL_MODELS_H

void __cdecl createfootmodel(int model, float x, float y, float z);
void __cdecl createrotormodel(int model, float x, float y, float z);
void __cdecl createsunglassmodel(int model);
void __cdecl createkeymodel(int model);
void __cdecl createcellphonemodel(int model, float x, float y, float z);

void __cdecl createbriefcasemodel(int model, float x, float y, float z);
void __cdecl createboxmodel(int model, float x, float y, float z);
void __cdecl createwheelmodel(int model, float length, float radius);

#endif /* CL_TEXTURES_SMALL_MODELS_H */
