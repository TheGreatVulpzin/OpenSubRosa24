/* The OpenGL states of the render passes. See gl_state.c. */
#ifndef CL_RENDER_GL_STATE_H
#define CL_RENDER_GL_STATE_H

#include <stdint.h>

void __cdecl enabletwosidedstencil(void);
void __cdecl disabletwosidedstencil(void);
void __cdecl glstateshadowtwoside(void);
void __cdecl glstatenormal(void);
void __cdecl glstatediffuse(void);
void __cdecl glstateambient(void);
void __cdecl settwosidedstencil(uint32_t back_function, uint32_t front_function, int reference, uint32_t mask,
                                uint32_t back_fail, uint32_t back_depth_fail, uint32_t back_pass,
                                uint32_t front_fail, uint32_t front_depth_fail, uint32_t front_pass);

#endif /* CL_RENDER_GL_STATE_H */
