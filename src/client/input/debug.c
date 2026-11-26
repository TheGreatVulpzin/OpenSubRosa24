/* Debug keys select camera targets and display modes.
 * Server controls also select diagnostic views.
 * Input blocking permits the two display controls outside the player controls.
 */
#include "input/debug.h"
#include "input/keyboard.h"
#include "client.h"
#include "sr_runtime.h"
#include "math/small_vectors.h"
#include "setup/timers.h"

enum { INTERIOR_TIMERS = 5, DEBUG_MODES = 6, DEBUG_MONEY = 10000 };

/* Apply key presses and camera selection in key order. */
void debug_getinputs(void)
{
    const char *labels[INTERIOR_TIMERS] = {
        cl_data_input_label_timer_build, cl_data_input_label_timer_optimize,
        cl_data_input_label_timer_setup, cl_data_input_label_timer_alloc,
        cl_data_input_label_timer_shadow
    };
    const int timers[INTERIOR_TIMERS] = {
        sr_data_timerlist.buildinterior, sr_data_timerlist.optimizeinterior,
        sr_data_timerlist.setupinterior, sr_data_timerlist.allocinterior,
        sr_data_timerlist.shadowinterior
    };
    int timer;
    if (!cl_data_audio_input_blocked) {
        if (cl_data_keyboard[KEY_6]) {
            sr_game.death_camera_ticks = 0;
            sr_data_view = -1;
        }
        if (cl_data_keyboard[KEY_7]) sr_data_view = 0;
        if (cl_data_keyboard[KEY_8]) sr_data_view = 1;
        if (cl_data_keyboard[KEY_9]) sr_data_view = 2;
        if (cl_data_keyboard[KEY_0]) sr_data_view = 3;
        if (input_pressed(KEY_F10)) cl_data_input_debug_toggle ^= 1;
        if (input_pressed(KEY_U))
            for (timer = 0; timer < INTERIOR_TIMERS; timer++)
                resettimer(
                    timers[timer], labels[timer]);
        if (cl_data_keyboard[KEY_INSERT] && sr_players[0].human_id != -1)
            ((InputCamera *)sr_data_camera)[0].pos = sr_humans[sr_players[0].human_id].bones[2].pos;
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
