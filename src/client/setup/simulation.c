/* The tick of the client.
 *
 * checksimulation runs from the main loop. It waits until the tick time has passed (16 ms, or
 * 100 ms in slow mode). Then it reads the keys of the display, runs the simulation and updates
 * the values that the display reads: the flash of a hit, the fade of death, the timer of the
 * microphone text, the traffic lights and the light angles. A client sends its input and
 * receives the state of the server. A host runs the server tick itself.
 */
#include "setup/simulation.h"
#include "setup/physics.h"
#include "setup/events.h"

#include <stdint.h>

#include "client.h"
#include "render/sprite_pool.h"
#include "render/sprites.h"
#include "render/lists.h"
#include "render/sprite_models.h"
#include "sr_runtime.h"
#include "bullets.h"
#include "input/camera.h"
#include "input/dispatch.h"
#include "logic.h"
#include "player.h"
#include "server.h"
#include "setup/interpolation.h"
#include "setup/send.h"

#include "system_libs.h"

#define g_game sr_data_game
#define g_server sr_data_server
#define g_player sr_data_player
#define g_human sr_data_human
#define g_city sr_data_city
#define g_debug sr_data_debug
#define g_debugit sr_data_debugit
#define g_world sr_data_game_world
#define g_area sr_data_area_sector
#define g_state sr_data_game.state
#define g_view cl_data_audio_camera.player
#define g_keyboard cl_data_keyboard
#define g_prevkeyboard cl_data_prevkeyboard
#define g_messages cl_data_chatmessage

/* SDL scancodes of the keys. */
enum {
    KEY_V = 25,
    KEY_EQUALS = 46,
    KEY_LEFT_BRACKET = 47,
    KEY_RIGHT_BRACKET = 48,
    KEY_F1 = 58,
    KEY_F4 = 61,
    KEY_F5 = 62,
    KEY_PAGE_UP = 75,
    KEY_PAGE_DOWN = 78,
    KEY_LEFT_SHIFT = 225
};

#define TICK_MS 16
#define SLOW_TICK_MS 100
/* A tick that is this late or later does not catch up: the next tick starts from now. */
#define MAX_LATE_MS 250

#define MIC_GAIN_STEP 10
#define MIC_GAIN_MIN 30
#define MIC_GAIN_MAX 250
#define MIC_TEXT_SECONDS 4.0f

/* The states of the game in which the world is simulated. */
#define SIMULATING(state) ((state) == 0 || (state) == 1 || (state) == 2)
#define STATE_RUNNING 2

#define NUM_PLAYERS 0x100
#define NUM_MESSAGES 0x100
#define NUM_FRAME_TIMES 0x20

/* The input modes of a player that move a vehicle. */
#define INPUT_ON_FOOT 1

#define GAMETYPE_DAY_CYCLE 4

/* Traffic lights: the length of the cycle, and the tick where each phase ends. */
#define LIGHT_CYCLE_MASK 0x7ff
#define LIGHT_PHASE_ENDS_A 0x380
#define LIGHT_PHASE_ENDS_B 0x400
#define LIGHT_PHASE_ENDS_C 0x780
#define LIGHT_GREEN 0
#define LIGHT_YELLOW 1
#define LIGHT_RED 2

/* One key went down in this tick. */
static int key_pressed(int key)
{
    return g_keyboard[key] != 0 && g_prevkeyboard[key] == 0;
}

/* Copy the 32 bits of a float without a float load. */
static void copy_bits(float *to, const float *from)
{
    *(int *)to = *(const int *)from;
}

static unsigned ticks_now(void)
{
    return SDL_GetTicks();
}

/* The display keys: voice on and off, hide the display, the gain of the microphone, restart
 * the game of a host. */
static void read_display_keys(void)
{
    if (g_debugit == 0 ? key_pressed(KEY_F1) : key_pressed(KEY_EQUALS))
        sr_data_option.help ^= 1;
    if (key_pressed(KEY_F4))
        sr_data_game.hide_hud ^= 1;
    if (key_pressed(KEY_PAGE_UP)) {
        sr_data_option.voiceboost += MIC_GAIN_STEP;
        if (sr_data_option.voiceboost > MIC_GAIN_MAX)
            sr_data_option.voiceboost = MIC_GAIN_MAX;
        sr_data_game.mic_text_timer = MIC_TEXT_SECONDS;
    }
    if (key_pressed(KEY_PAGE_DOWN)) {
        sr_data_option.voiceboost -= MIC_GAIN_STEP;
        if (sr_data_option.voiceboost < MIC_GAIN_MIN)
            sr_data_option.voiceboost = MIC_GAIN_MIN;
        sr_data_game.mic_text_timer = MIC_TEXT_SECONDS;
    }
    sr_data_option.voiceboost_scale = (float)((float)sr_data_option.voiceboost / 100.0);
    if (sr_data_option.voiceboost_scale < 0.125)
        sr_data_option.voiceboost_scale = 0.125f;
    if (4.0 < sr_data_option.voiceboost_scale)
        sr_data_option.voiceboost_scale = 4.0f;
    if (g_server.active != 0 && key_pressed(KEY_F5))
        resetgame();
}

/* A client gives the body of the viewed player the aim that the player has: on foot the
 * body and the arm, in a vehicle the vehicle aim. */
static void apply_view_aim(void)
{
    Player *player;
    Human *human;

    if (g_view == -1 || g_player[g_view].human_id == -1)
        return;
    player = &g_player[g_view];
    human = &g_human[player->human_id];
    if (human->vehicle_id == -1 && player->input_mode == INPUT_ON_FOOT) {
        copy_bits(&human->axis, &player->yaw);
        copy_bits(&human->pitch, &player->input_pitch);
        copy_bits(&human->arm_axis, &player->input_extra[0]);
        copy_bits(&human->arm_pitch, &player->input_extra[1]);
    } else if (player->input_mode == 2 || player->input_mode == 3) {
        copy_bits(&human->vehicle_axis, &player->yaw);
        copy_bits(&human->vehicle_pitch, &player->input_pitch);
    }
}

/* A host runs the server tick: the bots, the network, the physics and the logic. */
static void host_tick(void)
{
    int i;

    if (g_debugit != 0 && key_pressed(KEY_V))
        g_human[1].health = 100;
    if (g_state == STATE_RUNNING) {
        for (i = 0; i < NUM_PLAYERS; i++) {
            if (g_player[i].active != 0 && g_player[i].is_bot != 0)
                playerai(i);
        }
    }
    servercycle();
    serverrecv();
    playersimulation();
    if (SIMULATING(g_state))
        physicssimulation();
    logicsimulation();
    serversend();
    sr_data_game.ticks++;
}

/* The lights of every intersection by the tick of the host: the sides take turns. */
static void set_traffic_lights(void)
{
    unsigned phase = sr_data_game.ticks & LIGHT_CYCLE_MASK;
    int a, b, i;

    if (phase < LIGHT_PHASE_ENDS_A) {
        a = LIGHT_RED;
        b = LIGHT_GREEN;
    } else if (phase < LIGHT_PHASE_ENDS_B) {
        a = LIGHT_RED;
        b = LIGHT_YELLOW;
    } else if (phase < LIGHT_PHASE_ENDS_C) {
        a = LIGHT_GREEN;
        b = LIGHT_RED;
    } else {
        a = LIGHT_YELLOW;
        b = LIGHT_RED;
    }
    for (i = 0; i < g_city.num_intersections; i++) {
        g_city.intersections[i].red[0] = a;
        g_city.intersections[i].red[1] = b;
        g_city.intersections[i].red[2] = a;
        g_city.intersections[i].red[3] = b;
    }
}

/* The flash of a hit, the fade of death and the timer of the microphone text. */
static void update_effects(void)
{
    const Human *viewed = 0;

    if (g_view != -1 && g_player[g_view].human_id != -1)
        viewed = &g_human[g_player[g_view].human_id];
    if (viewed == 0) {
        sr_data_game.last_health = 0;
    } else {
        if (viewed->health < sr_data_game.last_health)
            sr_data_game.damage_flash = 1.0f;
        sr_data_game.last_health = viewed->health;
    }
    if (sr_data_game.damage_flash <= 0.0625)
        sr_data_game.damage_flash = 0.0f;
    else
        sr_data_game.damage_flash = (float)(sr_data_game.damage_flash - 0.0625);
    if (viewed == 0 || 0 < viewed->health || g_state != STATE_RUNNING)
        sr_data_game.death_fade = 0.0f;
    else if (viewed->health < -4 || sr_data_game.death_fade < 0.75)
        sr_data_game.death_fade = (float)(sr_data_game.death_fade + 0.004166666883975267);
    if (1.0 < sr_data_game.death_fade)
        sr_data_game.death_fade = 1.0f;
    if (sr_data_game.mic_text_timer <= 0.0)
        sr_data_game.mic_text_timer = 0.0f;
    else
        sr_data_game.mic_text_timer = (float)(sr_data_game.mic_text_timer - 0.01666666753590107);
}

/* The light angles: gametype 4 follows the clock. With the debug flag on, [ and ] turn the
 * first angle, and Shift with them turns the second. */
static void update_light_angles(void)
{
    if (g_game.gametype == GAMETYPE_DAY_CYCLE) {
        sr_data_game.start_angle_a = (float)(((float)g_world.clock * 3.14159265359) / 2592000.0 + 3.14159265359);
        sr_data_game.start_angle_b = 0.5f;
    }
    if (g_debugit == 0)
        return;
    if (g_keyboard[KEY_LEFT_SHIFT] == 0) {
        if (g_keyboard[KEY_LEFT_BRACKET] != 0)
            sr_data_game.start_angle_a = (float)(sr_data_game.start_angle_a - 0.0078125);
        if (g_keyboard[KEY_RIGHT_BRACKET] != 0)
            sr_data_game.start_angle_a = (float)(sr_data_game.start_angle_a + 0.0078125);
    } else {
        if (g_keyboard[KEY_LEFT_BRACKET] != 0)
            sr_data_game.start_angle_b = (float)(sr_data_game.start_angle_b - 0.024543692606171875);
        if (g_keyboard[KEY_RIGHT_BRACKET] != 0)
            sr_data_game.start_angle_b = (float)(sr_data_game.start_angle_b + 0.024543692606171875);
    }
    if (g_keyboard[KEY_LEFT_BRACKET] != 0 || g_keyboard[KEY_RIGHT_BRACKET] != 0)
        render_buildareasectorshadow();
}

/* Run the simulation of one tick. */
static void simulate(void)
{
    g_debug.num_lines = 0;
    sr_data_debug.sphere_count = 0;
    if (g_server.active == 0) {
        clientsend();
        clientrecv();
        apply_view_aim();
        humansimulationclient();
        itemsimulationclient();
        if (g_area.client_game_id != g_game.round_number)
            resetgame();
    } else {
        host_tick();
    }
    sr_data_area_sector.sim_ticks++;
    if (SIMULATING(g_state))
        bulletsimulation();
    client_eventsimulation();
    if (SIMULATING(g_state)) {
        spritesimulation();
        sprite_model_simulation();
        tiremark_simulation();
        bullettimetolive();
        spritetimetolive();
        if (g_server.active == 0)
            set_traffic_lights();
        selectnearitem();
    }
    update_effects();
}

/* Keep the time of this tick in the ring of the last 32, and the mean of the ring. */
static void record_frame_time(void)
{
    int i;

    sr_data_debug.frame_times[sr_data_debug.frame_time_index] = (int)(ticks_now() - sr_data_debug.tick_start_ms);
    sr_data_debug.frame_time_index = (sr_data_debug.frame_time_index + 1) & (NUM_FRAME_TIMES - 1);
    sr_data_debug.frame_time_average = 0.0f;
    for (i = 0; i < NUM_FRAME_TIMES; i++)
        sr_data_debug.frame_time_average = (float)((float)sr_data_debug.frame_times[i] + sr_data_debug.frame_time_average);
    sr_data_debug.frame_time_average = (float)(sr_data_debug.frame_time_average / 32.0);
}

/* Run one tick when its time has come. The result is 1 when it ran and 0 when it is too
 * early. With allow_input, the keys and the camera of the viewer update first. */
int __cdecl checksimulation(int allow_input)
{
    unsigned interval = sr_data_game.slow_tick != 0 ? SLOW_TICK_MS : TICK_MS;
    unsigned now = ticks_now();
    int i;

    if (now - sr_data_game.last_tick_ms < interval)
        return 0;
    if (now - sr_data_game.last_tick_ms < MAX_LATE_MS)
        now = sr_data_game.last_tick_ms + interval;
    sr_data_game.last_tick_ms = now;
    sr_data_debug.tick_start_ms = ticks_now();
    if (allow_input != 0) {
        getinputs(g_view);
        if (g_view == -1)
            movecamera(0);
        read_display_keys();
    }
    if (sr_data_game.step_once != 0)
        sr_data_game.paused = 0;
    if (sr_data_game.paused == 0)
        simulate();
    for (i = 0; i < NUM_MESSAGES; i++) {
        if (0 < g_messages[i].timer)
            g_messages[i].timer--;
    }
    update_light_angles();
    if (sr_data_game.step_once != 0) {
        sr_data_game.paused = 1;
        sr_data_game.step_once = 0;
    }
    record_frame_time();
    return 1;
}
