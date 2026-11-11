/* The shader programs of the renderer. See programs.c. */
#ifndef CL_RENDER_PROGRAMS_H
#define CL_RENDER_PROGRAMS_H

void __cdecl setupprogramobjects(void);
int __cdecl compileprogramobject(int program, const char *vertex_source, const char *fragment_source);

#endif /* CL_RENDER_PROGRAMS_H */
