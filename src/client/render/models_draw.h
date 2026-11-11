/* Drawing of the models with OpenGL. See models_draw.c. */
#ifndef CL_RENDER_MODELS_DRAW_H
#define CL_RENDER_MODELS_DRAW_H



void __cdecl rendersphere(const void *placement, float scale, int level);
void __cdecl rendermodel(int model, const void *placement, const void *scale, int mode, const int *textures,
                         const float *color);
void __cdecl rendermodelwire(int model, const void *placement, const void *scale_place, const unsigned *color,
                             float scale);
void __cdecl rendermodelnormals(int model, const void *placement, const void *scale);
void __cdecl rendertrackmodel(int mode);

#endif /* CL_RENDER_MODELS_DRAW_H */
