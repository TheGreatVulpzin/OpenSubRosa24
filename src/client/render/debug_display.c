/* The debug display shows frame timing, counters, players and network state.
 * Each display mode selects a separate set of rows.
 * The player count updates even when the display is off.
 */
#include <stdio.h>
#include "render/frame.h"
#include "client.h"
#include "render/text.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

/* Draw one integer row and advance by the text height. */
static void debug_integer_row(const char *format, float x, float *y, float height, int value)
{
    drawtext(format, x, *y, height, 0, 1.0f, 1.0f, 1.0f, 1.0f, value);
    *y = (float)(*y + height);
}

/* Count nonbot players and draw the selected diagnostic page. */
void __cdecl displaydebuginfo(void)
{
    char text[68];
    int i;
    float x, y;
    const float height = 20.0f;
    sr_data_debug.player_count = 0;
    for (i = 0; i < 256; ++i)
        if (sr_data_player[i].active && !sr_data_player[i].is_bot)
            ++sr_data_debug.player_count;
    if (sr_data_debug.display_mode > 0) {
        sprintf(text, "%.2f", (double)sr_data_debug.frame_ms_average);
        drawtext(text, 0.0f, 500.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        sprintf(text, "%.2f", (double)sr_data_debug.frame_time_average);
        drawtext(text, 0.0f, 516.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        y = 532.0f;
        debug_integer_row("Toggle 1:/i", 0.0f, &y, 16.0f, sr_data_debug.human_view);
        debug_integer_row("Toggle 2:/i", 0.0f, &y, 16.0f, sr_data_debug.toggle_two);
        debug_integer_row("Toggle 3:/i", 0.0f, &y, 16.0f, sr_data_debug.toggle_three);
        debug_integer_row("Cheat:/i", 0.0f, &y, 16.0f, cl_data_input_debug_toggle);
        for (i = 0; i < 8; ++i)
            drawtext("V:/i", 1020.0f, (float)((float)i * 16.0 + 320.0), 16.0f, 2,
                1.0f, 1.0f, 1.0f, 1.0f, ((AudioChannel *)sr_data_voice)[i].active);
    }
    if (sr_data_debug.display_mode == 1) {
        for (i = 0; i < sr_data_debug.num_values; ++i) {
            const DebugValue *value = &sr_data_debug.values[i];
            y = (float)((float)i * height + 128.0);
            drawtext(value->name, 0.0f, y, height, 0, 1.0f, 1.0f, 1.0f, 1.0f);
            if (value->is_float == 0)
                drawtext("/i", 128.0f, y, height, 0, 1.0f, 1.0f, 1.0f, 1.0f, value->int_value);
            if (value->is_float == 1)
                drawtext("/f", 128.0f, y, height, 0, 1.0f, 1.0f, 1.0f, 1.0f, (double)value->float_value);
        }
    }
    if (sr_data_debug.display_mode == 2) {
        for (i = 0; i < 16; ++i)
            drawtext("/i", 0.0f, (float)((float)i * height + 128.0), height, 0x20,
                1.0f, 1.0f, 1.0f, 1.0f, sr_data_debug.counters[i]);
        y = 64.0f;
        for (i = 0; i < 256; ++i) {
            if (sr_data_player[i].active && !sr_data_player[i].is_bot) {
                drawtext(sr_data_player[i].name, 512.0f, y, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
                y = (float)(y + 16.0);
            }
        }
    }
    if (sr_data_debug.display_mode == 3) {
        x = 4.0f;
        y = 128.0f;
        for (i = 64; i < sr_data_timerlist.numoftimers; ++i) {
            drawtext(sr_data_timer[i].name, x, y, height, 0x20, 1.0f, 1.0f, 1.0f, 1.0f);
            drawtext("/u", (float)(height * 10.0 + x), y, height, 0x20,
                1.0f, 1.0f, 1.0f, 1.0f, sr_data_timer[i].ms);
            y = (float)(y + height);
        }
    }
    if (sr_data_debug.display_mode == 4) {
        for (i = 0; i < 16; ++i) {
            y = (float)((float)i * height + 150.0);
            drawtext(sr_data_timer[i].name, 0.0f, y, height, 0, 1.0f, 1.0f, 1.0f, 1.0f);
            drawtext("/u", (float)(height * 10.0), y, height, 0, 1.0f, 1.0f, 1.0f, 1.0f,
                sr_data_timer[i].total_lo, sr_data_timer[i].total_hi);
        }
    }
    if (sr_data_debug.display_mode == 5) {
        y = 160.0f;
        if (!sr_data_server.active) {
            debug_integer_row("Round:/i", 4.0f, &y, height, sr_data_game.round_number);
            debug_integer_row("ClientRound:/i", 4.0f, &y, height, sr_data_area_sector.client_game_id);
        } else
            debug_integer_row("Numofconnections:/i", 4.0f, &y, height, sr_data_numofconnections);
        debug_integer_row("Packetsize:/i", 4.0f, &y, height, sr_data_server.last_packet_size);
        debug_integer_row("MaxPacketsize:/i", 4.0f, &y, height, sr_data_server.max_packet_size);
        debug_integer_row("Numofevents:/i", 4.0f, &y, height, sr_data_numofevents);
        debug_integer_row("objpacketsize:/i", 4.0f, &y, height, sr_data_debug.net_flag);
        debug_integer_row("conn0bytes:/i", 4.0f, &y, height, sr_data_connection[0].credit);
        debug_integer_row("players:/i", 4.0f, &y, height, sr_data_debug.player_count);
    }
}
