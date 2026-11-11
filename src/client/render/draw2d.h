/* The flat shapes of the 2D layer. See draw2d.c. */
#ifndef CL_RENDER_DRAW2D_H
#define CL_RENDER_DRAW2D_H

void __cdecl drawbitmap(int texture, float x, float y, float width, float height, unsigned flags, float red,
                        float green, float blue, float alpha);
void __cdecl drawbitmaptexcoord(int texture, float x, float y, float width, float height, unsigned flags,
                                float red, float green, float blue, float alpha, const float *coords);
void __cdecl drawlinebox(float x, float y, float width, float height, unsigned flags, float red, float green,
                         float blue, float alpha);
void __cdecl drawmousecursor(int x, int y, int size, float red, float green, float blue, float alpha);
void __cdecl drawcircle(float x, float y, float radius, float red, float green, float blue, float alpha);
void __cdecl drawcirclefill(float x, float y, float radius, float red, float green, float blue, float alpha);

#endif /* CL_RENDER_DRAW2D_H */
