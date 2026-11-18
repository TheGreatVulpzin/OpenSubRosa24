/* The OpenGL states of the render passes. See gl_state.c. */
#ifndef CL_RENDER_GL_STATE_H
#define CL_RENDER_GL_STATE_H

void __cdecl enabletwosidedstencil(void);
void __cdecl disabletwosidedstencil(void);
void __cdecl glstateshadowtwoside(void);
void __cdecl glstatenormal(void);
void settwosidedstencil(unsigned back, unsigned front, int reference, unsigned mask,
                        unsigned back_fail, unsigned back_depth, unsigned back_pass,
                        unsigned front_fail, unsigned front_depth, unsigned front_pass);

#endif /* CL_RENDER_GL_STATE_H */
