/* Text on the 2D layer. See text.c. */
#ifndef CL_RENDER_TEXT_H
#define CL_RENDER_TEXT_H

void __cdecl drawtext(const char *text, float x, float y, float size, unsigned flags, float red, float green,
                      float blue, float alpha, ...);

#endif /* CL_RENDER_TEXT_H */
