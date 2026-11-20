/* The joysticks: up to 16 open at the start, read each frame when the joystick option is on.
 * Each joystick keeps its axes, buttons and hats, and a copy of the frame before. */
#include "input/joystick.h"

#include <stdint.h>
#include <string.h>

#include "audio/imports.h"
#include "client.h"
#include "sr_runtime.h"

AUDIO_IMPORT("SDL2.DLL", __cdecl, int, SDL_NumJoysticks, (void))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void *, SDL_JoystickOpen, (int))
AUDIO_IMPORT("SDL2.DLL", __cdecl, int, SDL_JoystickInstanceID, (void *))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void *, SDL_JoystickGetGUID, (void *, void *))
AUDIO_IMPORT("SDL2.DLL", __cdecl, const char *, SDL_JoystickNameForIndex, (int))
AUDIO_IMPORT("SDL2.DLL", __cdecl, int, SDL_JoystickNumAxes, (void *))
AUDIO_IMPORT("SDL2.DLL", __cdecl, int, SDL_JoystickNumButtons, (void *))
AUDIO_IMPORT("SDL2.DLL", __cdecl, int, SDL_JoystickNumHats, (void *))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void *, SDL_HapticOpenFromJoystick, (void *))
AUDIO_IMPORT("SDL2.DLL", __cdecl, int, SDL_JoystickEventState, (int))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_JoystickUpdate, (void))
AUDIO_IMPORT("SDL2.DLL", __cdecl, int16_t, SDL_JoystickGetAxis, (void *, int))
AUDIO_IMPORT("SDL2.DLL", __cdecl, uint8_t, SDL_JoystickGetButton, (void *, int))
AUDIO_IMPORT("SDL2.DLL", __cdecl, uint8_t, SDL_JoystickGetHat, (void *, int))

#define MAX_JOYSTICKS 16
#define HAT_UP 1
#define HAT_RIGHT 2
#define HAT_DOWN 4
#define HAT_LEFT 8
#define AXIS_RANGE 32768.0 /* an SDL axis goes from -32768 to 32767 */

/* Open each joystick (at most 16) and read its id, GUID, name, counts and haptic device. The
 * count of joysticks can be above 16: the other joysticks stay closed. The joystick events of
 * SDL go off: the client reads the state each frame. */
void __cdecl setupjoysticks(void)
{
    int i;

    cl_data_num_joysticks = audio_SDL_NumJoysticks()();
    for (i = 0; i < cl_data_num_joysticks; i++) {
        Joystick *joy = &cl_data_joysticks[i];
        uint8_t guid[16];
        void *handle;

        if (i >= MAX_JOYSTICKS)
            continue;
        handle = audio_SDL_JoystickOpen()(i);
        cl_data_joystick_handles[i] = handle;
        joy->instance = audio_SDL_JoystickInstanceID()(handle);
        memcpy(joy->guid, audio_SDL_JoystickGetGUID()(guid, handle), sizeof joy->guid);
        strcpy(joy->name, audio_SDL_JoystickNameForIndex()(i));
        joy->num_axes = audio_SDL_JoystickNumAxes()(handle);
        joy->num_buttons = audio_SDL_JoystickNumButtons()(handle);
        joy->num_hats = audio_SDL_JoystickNumHats()(handle);
        joy->haptic = audio_SDL_HapticOpenFromJoystick()(handle);
    }
    audio_SDL_JoystickEventState()(0);
}

/* Read the state of each open joystick when the joystick option is on. The state before goes
 * into joysticks_previous. An axis goes from -1 to 1. A hat gives four flags: up, down, left
 * and right, with the diagonals in two of them. */
void __cdecl checkjoystick(void)
{
    int i, k;

    if (sr_data_config.joystick == 0)
        return;
    for (i = 0; i < cl_data_num_joysticks; i++)
        memcpy(&cl_data_joysticks_previous[i], &cl_data_joysticks[i], sizeof(Joystick));
    audio_SDL_JoystickUpdate()();
    for (i = 0; i < cl_data_num_joysticks; i++) {
        Joystick *joy = &cl_data_joysticks[i];
        void *handle = cl_data_joystick_handles[i];

        if (handle == NULL)
            continue;
        for (k = 0; k < joy->num_axes; k++)
            joy->axes[k] = (float)((float)audio_SDL_JoystickGetAxis()(handle, k) / AXIS_RANGE);
        for (k = 0; k < joy->num_buttons; k++)
            joy->buttons[k] = audio_SDL_JoystickGetButton()(handle, k);
        for (k = 0; k < joy->num_hats; k++) {
            int hat = audio_SDL_JoystickGetHat()(handle, k);
            int *flags = &joy->hats[k * 4];

            flags[0] = hat == HAT_UP || hat == (HAT_UP | HAT_RIGHT) || hat == (HAT_UP | HAT_LEFT);
            flags[1] = hat == HAT_DOWN || hat == (HAT_DOWN | HAT_RIGHT) || hat == (HAT_DOWN | HAT_LEFT);
            flags[2] = hat == HAT_LEFT || hat == (HAT_UP | HAT_LEFT) || hat == (HAT_DOWN | HAT_LEFT);
            flags[3] = hat == HAT_RIGHT || hat == (HAT_UP | HAT_RIGHT) || hat == (HAT_DOWN | HAT_RIGHT);
        }
    }
}
