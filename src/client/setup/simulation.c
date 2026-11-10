/* The tick of the client.
 *
 * checksimulation runs from the main loop. It waits until the tick time has passed (16 ms, or
 * 100 ms in slow mode). Then it reads the keys of the display, runs the simulation and updates
 * the values that the display reads: the flash of a hit, the fade of death, the timer of the
 * microphone text, the traffic lights and the light angles. A client sends its input and
 * receives the state of the server. A host runs the server tick itself.
 */
#include "setup/simulation.h"

#include <stdint.h>

#include "client.h"
#include "sr_runtime.h"

void *sr_import(const char *dll, const char *name);

#define g_game sr_data_game
#define g_server sr_data_server
#define g_player sr_data_player
#define g_human sr_data_human
#define g_city sr_data_city
#define g_debug sr_data_debug
#define g_debugit sr_data_debugit
#define g_world sr_data_game_world
#define g_area sr_data_area_sector
#define g_state cl_data_game_state
#define g_view cl_data_view_player
#define g_keyboard cl_data_keyboard
#define g_prevkeyboard cl_data_prevkeyboard
#define g_messages cl_data_chatmessage

/* Game functions of other files. */
#define getinputs CL_FN(0x490bf0, void(__cdecl *)(int player))
#define movecamera CL_FN(0x47d0e0, void(__cdecl *)(int mode))
#define resetgame CL_FN(0x479ad0, void(__cdecl *)(void))
#define clientsend CL_FN(0x468030, void(__cdecl *)(void))
#define clientrecv CL_FN(0x4676d0, void(__cdecl *)(void))
#define humansimulationclient CL_FN(0x4896d0, void(__cdecl *)(void))
#define itemsimulationclient CL_FN(0x499500, void(__cdecl *)(void))
#define playerai CL_FN(0x4348d0, void(__cdecl *)(int player))
#define servercycle CL_FN(0x4df550, void(__cdecl *)(void))
#define serverrecv CL_FN(0x4df6a0, void(__cdecl *)(void))
#define playersimulation CL_FN(0x4d4b80, void(__cdecl *)(void))
#define physicssimulation CL_FN(0x4d4700, void(__cdecl *)(void))
#define logicsimulation CL_FN(0x4a3940, void(__cdecl *)(void))
#define serversend CL_FN(0x4deb20, void(__cdecl *)(void))
#define bulletsimulation CL_FN(0x4646b0, void(__cdecl *)(void))
#define eventsimulation CL_FN(0x4704b0, void(__cdecl *)(void))
#define spritesimulation CL_FN(0x4e2790, void(__cdecl *)(void))
#define sprite_model_simulation CL_FN(0x4e52e0, void(__cdecl *)(void))
#define tiremark_simulation CL_FN(0x4e5f80, void(__cdecl *)(void))
#define timetolive_records CL_FN(0x4645e0, void(__cdecl *)(void))
#define bullettimetolive CL_FN(0x4e26a0, void(__cdecl *)(void))
#define spritetimetolive CL_FN(0x484f00, void(__cdecl *)(void))
#define render_buildareasectorshadow CL_FN(0x4d8290, void(__cdecl *)(void))

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
    static unsigned(__cdecl * get_ticks)(void);

    if (!get_ticks)
        *(void **)&get_ticks = sr_import("SDL2.DLL", "SDL_GetTicks");
    return get_ticks();
}

/* The display keys: voice on and off, hide the display, the gain of the microphone, restart
 * the game of a host. */
static void read_display_keys(void)
{
    if (g_debugit == 0 ? key_pressed(KEY_F1) : key_pressed(KEY_EQUALS))
        cl_data_voice_on ^= 1;
    if (key_pressed(KEY_F4))
        cl_data_hide_hud ^= 1;
    if (key_pressed(KEY_PAGE_UP)) {
        cl_data_mic_gain_percent += MIC_GAIN_STEP;
        if (cl_data_mic_gain_percent > MIC_GAIN_MAX)
            cl_data_mic_gain_percent = MIC_GAIN_MAX;
        cl_data_mic_text_timer = MIC_TEXT_SECONDS;
    }
    if (key_pressed(KEY_PAGE_DOWN)) {
        cl_data_mic_gain_percent -= MIC_GAIN_STEP;
        if (cl_data_mic_gain_percent < MIC_GAIN_MIN)
            cl_data_mic_gain_percent = MIC_GAIN_MIN;
        cl_data_mic_text_timer = MIC_TEXT_SECONDS;
    }
    cl_data_audio_voice_gain = (float)((float)cl_data_mic_gain_percent / 100.0);
    if (cl_data_audio_voice_gain < 0.125)
        cl_data_audio_voice_gain = 0.125f;
    if (4.0 < cl_data_audio_voice_gain)
        cl_data_audio_voice_gain = 4.0f;
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
    cl_data_game_tick++;
}

/* The lights of every intersection by the tick of the host: the sides take turns. */
static void set_traffic_lights(void)
{
    unsigned phase = cl_data_game_tick & LIGHT_CYCLE_MASK;
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
        cl_data_last_health = 0;
    } else {
        if (viewed->health < cl_data_last_health)
            cl_data_damage_flash = 1.0f;
        cl_data_last_health = viewed->health;
    }
    if (cl_data_damage_flash <= 0.0625)
        cl_data_damage_flash = 0.0f;
    else
        cl_data_damage_flash = (float)(cl_data_damage_flash - 0.0625);
    if (viewed == 0 || 0 < viewed->health || g_state != STATE_RUNNING)
        cl_data_death_fade = 0.0f;
    else if (viewed->health < -4 || cl_data_death_fade < 0.75)
        cl_data_death_fade = (float)(cl_data_death_fade + 0.004166666883975267);
    if (1.0 < cl_data_death_fade)
        cl_data_death_fade = 1.0f;
    if (cl_data_mic_text_timer <= 0.0)
        cl_data_mic_text_timer = 0.0f;
    else
        cl_data_mic_text_timer = (float)(cl_data_mic_text_timer - 0.01666666753590107);
}

/* The light angles: gametype 4 follows the clock. With the debug flag on, [ and ] turn the
 * first angle, and Shift with them turns the second. */
static void update_light_angles(void)
{
    if (g_game.gametype == GAMETYPE_DAY_CYCLE) {
        cl_data_light_angle_a = (float)(((float)g_world.clock * 3.14159265359) / 2592000.0 + 3.14159265359);
        cl_data_light_angle_b = 0.5f;
    }
    if (g_debugit == 0)
        return;
    if (g_keyboard[KEY_LEFT_SHIFT] == 0) {
        if (g_keyboard[KEY_LEFT_BRACKET] != 0)
            cl_data_light_angle_a = (float)(cl_data_light_angle_a - 0.0078125);
        if (g_keyboard[KEY_RIGHT_BRACKET] != 0)
            cl_data_light_angle_a = (float)(cl_data_light_angle_a + 0.0078125);
    } else {
        if (g_keyboard[KEY_LEFT_BRACKET] != 0)
            cl_data_light_angle_b = (float)(cl_data_light_angle_b - 0.024543692606171875);
        if (g_keyboard[KEY_RIGHT_BRACKET] != 0)
            cl_data_light_angle_b = (float)(cl_data_light_angle_b + 0.024543692606171875);
    }
    if (g_keyboard[KEY_LEFT_BRACKET] != 0 || g_keyboard[KEY_RIGHT_BRACKET] != 0)
        render_buildareasectorshadow();
}

/* Run the simulation of one tick. */
static void simulate(void)
{
    g_debug.num_lines = 0;
    cl_data_debug_extra_count = 0;
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
    cl_data_sim_ticks++;
    if (SIMULATING(g_state))
        bulletsimulation();
    eventsimulation();
    if (SIMULATING(g_state)) {
        spritesimulation();
        sprite_model_simulation();
        tiremark_simulation();
        timetolive_records();
        bullettimetolive();
        if (g_server.active == 0)
            set_traffic_lights();
        spritetimetolive();
    }
    update_effects();
}

/* Keep the time of this tick in the ring of the last 32, and the mean of the ring. */
static void record_frame_time(void)
{
    int i;

    cl_data_frame_times[cl_data_frame_time_index] = (int)(ticks_now() - cl_data_tick_start_ms);
    cl_data_frame_time_index = (cl_data_frame_time_index + 1) & (NUM_FRAME_TIMES - 1);
    cl_data_frame_time_average = 0.0f;
    for (i = 0; i < NUM_FRAME_TIMES; i++)
        cl_data_frame_time_average = (float)((float)cl_data_frame_times[i] + cl_data_frame_time_average);
    cl_data_frame_time_average = (float)(cl_data_frame_time_average / 32.0);
}

/* Run one tick when its time has come. The result is 1 when it ran and 0 when it is too
 * early. With allow_input, the keys and the camera of the viewer update first. */
int __cdecl checksimulation(int allow_input)
{
    unsigned interval = cl_data_slow_tick != 0 ? SLOW_TICK_MS : TICK_MS;
    unsigned now = ticks_now();
    int i;

    if (now - cl_data_last_tick_ms < interval)
        return 0;
    if (now - cl_data_last_tick_ms < MAX_LATE_MS)
        now = cl_data_last_tick_ms + interval;
    cl_data_last_tick_ms = now;
    cl_data_tick_start_ms = ticks_now();
    if (allow_input != 0) {
        getinputs(g_view);
        if (g_view == -1)
            movecamera(0);
        read_display_keys();
    }
    if (cl_data_step_once != 0)
        cl_data_paused = 0;
    if (cl_data_paused == 0)
        simulate();
    for (i = 0; i < NUM_MESSAGES; i++) {
        if (0 < g_messages[i].timer)
            g_messages[i].timer--;
    }
    update_light_angles();
    if (cl_data_step_once != 0) {
        cl_data_paused = 1;
        cl_data_step_once = 0;
    }
    record_frame_time();
    return 1;
}
