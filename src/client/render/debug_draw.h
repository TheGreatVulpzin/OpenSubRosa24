/* The debug shapes of the 3D view. See debug_draw.c. */
#ifndef CL_RENDER_DEBUG_DRAW_H
#define CL_RENDER_DEBUG_DRAW_H

void __cdecl debug_renderboundingbox(const float *low, const float *high);
void __cdecl debug_renderbox(const void *placement, const void *rotation, float x, float y, float z);

#endif /* CL_RENDER_DEBUG_DRAW_H */
