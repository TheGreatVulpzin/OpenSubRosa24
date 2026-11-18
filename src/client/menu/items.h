/* The menu items: buttons, text fields, toggles, choices and sliders. See items.c. */
#ifndef CL_MENU_ITEMS_H
#define CL_MENU_ITEMS_H

#include <stdint.h>

void __cdecl setupmenuitems(void);
void __cdecl resetmenuitems(void);
void __cdecl createmenuitemempty(void);
void __cdecl createmenuitem(const char *text, float x, float y, float size, uint32_t flags,
                            float red, float green, float blue, float alpha);
void __cdecl createmenubutton(const char *text, float x, float y, float width, float height, float size,
                              uint32_t flags, float red, float green, float blue, float alpha);
void __cdecl setmenuiteminfo(const char *text, float x, float y, float size, uint32_t flags,
                             float red, float green, float blue, float alpha);
void __cdecl setmenuitem(int option, ...);
void __cdecl checkmenuitems(void);
void __cdecl menuslider(void);
void __cdecl menutoggle(void);
void __cdecl menuset(void);
float __cdecl text_getlength(const char *text, float scale);

#endif /* CL_MENU_ITEMS_H */
