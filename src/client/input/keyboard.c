/* Keyboard polling copies the current keys to the transition buffer.
 * The key buffer contains SDL scan codes.
 * Either shift key supplies the left shift state.
 */
#include "input/keyboard.h"
#include <string.h>
#include "sr_runtime.h"

void *sr_import(const char *dll, const char *name);

enum { KEY_COPY_COUNT = 323 };

/* Poll the key buffer. SDL must supply at least 323 key bytes. */
void checkkeyboard(void)
{
    typedef unsigned char *(__cdecl *KeyboardState)(int *);
    static KeyboardState state;
    if (!state)
        *(void **)&state = sr_import("SDL2.DLL", "SDL_GetKeyboardState");
    memcpy(cl_data_audio_keys_previous, cl_data_audio_keys, KEY_COPY_COUNT);
    memcpy(cl_data_audio_keys, state(NULL), KEY_COPY_COUNT);
    if (cl_data_audio_keys[KEY_RIGHT_SHIFT])
        cl_data_audio_keys[KEY_SHIFT] = 1;
}
