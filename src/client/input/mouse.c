/* Mouse polling records buttons, releases and cursor coordinates.
 * Absolute coordinates use a 1024 by 640 canvas.
 * Relative polling places the cursor at the canvas center.
 */
#include "input/mouse.h"
#include "sr_runtime.h"
#include "system_libs.h"

#define MOUSE_INTEGER_LIMIT 2147483648.0

/* Truncate a coordinate to 32 bits. Invalid conversions return INT32_MIN. */
static int cl_ftoi(double value)
{
    if (!(value >= INT32_MIN && value < MOUSE_INTEGER_LIMIT))
        return INT32_MIN;
    return (int)value;
}

/* Set button state and release transitions from an SDL button mask. */
static void mouse_buttons(unsigned buttons)
{
    InputMouse *mouse = &cl_data_input_mouse;
    const InputMouse *previous = &cl_data_input_mouse_previous;
    mouse->buttons = buttons & MOUSE_BUTTON_MASK;
    mouse->left = (buttons & MOUSE_LEFT) != 0;
    mouse->right = (buttons & MOUSE_RIGHT) != 0;
    mouse->left_released = !mouse->left && previous->left == 1;
    mouse->right_released = !mouse->right && previous->right == 1;
}

/* Poll absolute coordinates. The screen dimensions determine coordinate scaling. */
void checkmouse(void)
{
    unsigned buttons;
    InputMouse *mouse = &cl_data_input_mouse;
    SDL_PumpEvents();
    cl_data_input_mouse_previous = *mouse;
    buttons = SDL_GetMouseState(&mouse->x, &mouse->y);
    mouse->x = cl_ftoi((double)mouse->x * INPUT_WIDTH / cl_data_render_width);
    mouse->y = cl_ftoi((double)mouse->y * INPUT_HEIGHT / cl_data_render_height);
    mouse_buttons(buttons);
}

/* Poll relative movement and button releases. */
void checkmouserelative(void)
{
    unsigned buttons;
    InputMouse *mouse = &cl_data_input_mouse;
    SDL_PumpEvents();
    cl_data_input_mouse_previous = *mouse;
    buttons = SDL_GetRelativeMouseState(&mouse->dx, &mouse->dy);
    mouse->x = INPUT_WIDTH / 2;
    mouse->y = INPUT_HEIGHT / 2;
    mouse_buttons(buttons);
}
