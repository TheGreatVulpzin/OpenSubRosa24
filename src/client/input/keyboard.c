/* Keyboard polling copies the current keys to the transition buffer.
 * The key buffer contains SDL scan codes.
 * Either shift key supplies the left shift state.
 */
#include "input/keyboard.h"
#include <string.h>
#include "sr_runtime.h"
#include "system_libs.h"

enum { KEY_COPY_COUNT = 323 };

/* Poll the key buffer. SDL must supply at least 323 key bytes. */
void checkkeyboard(void)
{
    memcpy(cl_data_prevkeyboard, cl_data_keyboard, KEY_COPY_COUNT);
    memcpy(cl_data_keyboard, SDL_GetKeyboardState(NULL), KEY_COPY_COUNT);
    if (cl_data_keyboard[KEY_RIGHT_SHIFT])
        cl_data_keyboard[KEY_SHIFT] = 1;
}
