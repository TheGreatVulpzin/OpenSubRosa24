/* Absolute and relative mouse polling. */
#ifndef CL_INPUT_MOUSE_H
#define CL_INPUT_MOUSE_H
enum { INPUT_WIDTH = 1024, INPUT_HEIGHT = 640, MOUSE_LEFT = 1, MOUSE_RIGHT = 4,
       MOUSE_BUTTON_MASK = 255 };
void checkmouse(void);
void checkmouserelative(void);
#endif
