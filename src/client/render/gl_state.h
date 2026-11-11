/* The OpenGL states of the render passes. See gl_state.c. */
#ifndef CL_RENDER_GL_STATE_H
#define CL_RENDER_GL_STATE_H

void __cdecl enabletwosidedstencil(void);
void __cdecl disabletwosidedstencil(void);
void __cdecl glstateshadowtwoside(void);
void __cdecl glstatenormal(void);

#endif /* CL_RENDER_GL_STATE_H */
