/* The text input of the menus. See textinput.c. */
#ifndef CL_MENU_TEXTINPUT_H
#define CL_MENU_TEXTINPUT_H

void __cdecl addmenuinputchar(char letter);
int __cdecl deleteselectedtext(void);
void __cdecl formattextbox(const char *text, float width, float height, float size);
int __cdecl gettextinputposition(void);
void __cdecl menu_textbox_putchar(int letter);

#endif /* CL_MENU_TEXTINPUT_H */
