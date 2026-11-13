/* Keyboard state and press transitions. */
#ifndef CL_INPUT_KEYBOARD_H
#define CL_INPUT_KEYBOARD_H

#include "sr_runtime.h"

#if defined(_MSC_VER)
#define INPUT_INLINE __forceinline
#else
#define INPUT_INLINE inline
#endif

enum {
    KEY_A = 4, KEY_B = 5, KEY_C = 6, KEY_D = 7, KEY_E = 8, KEY_F = 9,
    KEY_I = 12, KEY_O = 18, KEY_P = 19, KEY_Q = 20, KEY_R = 21,
    KEY_S = 22, KEY_T = 23, KEY_U = 24, KEY_W = 26, KEY_Y = 28,
    KEY_Z = 29, KEY_1 = 30, KEY_2 = 31, KEY_3 = 32, KEY_4 = 33,
    KEY_5 = 34, KEY_6 = 35, KEY_7 = 36, KEY_8 = 37, KEY_9 = 38,
    KEY_0 = 39, KEY_RETURN = 40, KEY_ESCAPE = 41, KEY_TAB = 43,
    KEY_SPACE = 44, KEY_GRAVE = 53, KEY_F2 = 59, KEY_F6 = 63,
    KEY_F10 = 67, KEY_F11 = 68, KEY_INSERT = 73, KEY_DELETE = 76,
    KEY_PAD_1 = 89, KEY_PAD_2 = 90, KEY_PAD_3 = 91, KEY_PAD_4 = 92,
    KEY_PAD_5 = 93, KEY_PAD_6 = 94, KEY_PAD_7 = 95, KEY_PAD_8 = 96,
    KEY_PAD_9 = 97, KEY_PAD_0 = 98, KEY_CONTROL = 224,
    KEY_SHIFT = 225, KEY_RIGHT_SHIFT = 229
};

void checkkeyboard(void);

/* Return nonzero for a key press with no press in the transition buffer. */
static INPUT_INLINE int input_pressed(int key)
{
    return cl_data_audio_keys[key] && !cl_data_audio_keys_previous[key];
}
#endif
