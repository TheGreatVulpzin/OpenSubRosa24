/* Debug keys select camera targets and display modes.
 * Server controls also select diagnostic views.
 * Input blocking permits the two display controls outside the player controls.
 */
#include "input/debug.h"
#include "input/keyboard.h"
#include "client.h"
#include "sr_runtime.h"

enum { INTERIOR_TIMERS = 5, DEBUG_MODES = 6, DEBUG_MONEY = 10000 };

/* Apply key presses and camera selection in key order. */
void debug_getinputs(void)
{
    const char *labels[INTERIOR_TIMERS] = {
        cl_data_input_label_timer_build, cl_data_input_label_timer_optimize,
        cl_data_input_label_timer_setup, cl_data_input_label_timer_alloc,
        cl_data_input_label_timer_shadow
    };
    int timer;
    if (!cl_data_audio_input_blocked) {
        if (cl_data_audio_keys[KEY_6]) {
            sr_game.unused_257dc = 0;
            sr_data_view = -1;
        }
        if (cl_data_audio_keys[KEY_7]) sr_data_view = 0;
        if (cl_data_audio_keys[KEY_8]) sr_data_view = 1;
        if (cl_data_audio_keys[KEY_9]) sr_data_view = 2;
        if (cl_data_audio_keys[KEY_0]) sr_data_view = 3;
        if (input_pressed(KEY_F10)) cl_data_input_debug_toggle ^= 1;
        if (input_pressed(KEY_U))
            for (timer = 0; timer < INTERIOR_TIMERS; timer++)
                CL_FN(0x4fafe0, void (__cdecl *)(int, const char *))(
                    cl_data_input_interior_timers[timer], labels[timer]);
        if (cl_data_audio_keys[KEY_INSERT] && sr_players[0].human_id != -1)
            CL_FN(0x50b950, void (__cdecl *)(vec3 *, const vec3 *))(
                &cl_data_input_cameras[0].pos, &sr_humans[sr_players[0].human_id].bones[2].pos);
        if (input_pressed(KEY_F2)) sr_data_debug.human_view ^= 1;
        if (input_pressed(KEY_INSERT)) sr_players[0].money += DEBUG_MONEY;
        if (sr_server) {
            if (input_pressed(KEY_I)) sr_game.debug_i ^= 1;
            if (input_pressed(KEY_P)) sr_game.debug_p ^= 1;
            if (input_pressed(KEY_O)) sr_game.debug_o = 1;
        }
        if (input_pressed(KEY_B)) sr_data_debug.world_view ^= 1;
    }
    if (input_pressed(KEY_F6)) sr_data_debug.display ^= 1;
    if (input_pressed(KEY_F11)) {
        sr_data_debug.display_mode++;
        if (sr_data_debug.display_mode >= DEBUG_MODES)
            sr_data_debug.display_mode = 0;
    }
}
