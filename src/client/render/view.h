/* The view of a pass. See view.c. */
#ifndef CL_RENDER_VIEW_H
#define CL_RENDER_VIEW_H

void __cdecl setuptextdisplay(void);
void __cdecl setupperspectiveviewport(int x, int y, int width, int height, float near_plane, float far_plane,
                                      float scale);
void __cdecl setuporthoviewport(int x, int y, int width, int height, float half_width, float half_height,
                                float half_depth);
void __cdecl setupviewpoint(const void *position, const void *rotation);
void __cdecl setupobjectrender(const void *position, const void *rotation);

#endif /* CL_RENDER_VIEW_H */
