/* The drawing of the menu items. See draw.c. */
#ifndef CL_MENU_DRAW_H
#define CL_MENU_DRAW_H

#include <stdint.h>

void __cdecl generatemenutextures(void);
void __cdecl drawmenubutton(float x, float y, float width, float height, float fade, uint32_t flags, float red,
                            float green, float blue, float alpha);
void __cdecl drawmenubutton2(float x, float y, float width, float height, int lit, uint32_t flags, float red,
                             float green, float blue, float alpha);
void __cdecl menutextbox(void);
void __cdecl drawmenutextfield(int index, const char *text, float x, float y, float width, float height,
                               float size, uint32_t flags, float red, float green, float blue, float alpha);
void __cdecl drawtextbox(const char *text, float x, float y, float width, float height, float size,
                         uint32_t flags, float red, float green, float blue, float alpha);
void __cdecl drawmenuitems(void);

#endif /* CL_MENU_DRAW_H */
