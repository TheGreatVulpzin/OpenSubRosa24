/* Input dispatch builds controls for chat, pause and player menus.
 * It polls keys and mouse state, handles capture changes and queues menu actions.
 * Human health, seat and vehicle type select the movement controls.
 */
#include "input/dispatch.h"
#include "input/keyboard.h"
#include "input/mouse.h"
#include "input/camera.h"
#include "input/debug.h"
#include "input/player_inputs.h"
#include "client.h"
#include "sr_runtime.h"
#include <string.h>

enum {
    MENU_KEY = 1, MENU_TEXT = 5, MENU_SET = 11, MENU_HOVER = 23,
    CONTROL_NONE = 0, CONTROL_HUMAN = 1, CONTROL_CAR = 2, CONTROL_HELI = 3,
    GAME_ROUND = 3, GAME_WORLD = 4, ROUND_START = 1, ROUND_END = 3,
    HUMAN_MASK = 511, TEAM_COUNT = 3, CONNECTION_COLUMNS = 4,
    CHAT_HISTORY_MASK = 7, CHAT_CAPACITY = 60, CONNECTION_TIMEOUT = 1200,
    INPUT_AXIS_COUNT = 4, SPECTATOR_SLOT_KEYS = 4,
    INPUT_SLOT_1 = 64, INPUT_RELOAD = 0x1000
};

/* Define a menu rectangle and its text. */
static void menu_label(const char *text, float x, float y, float width, float height, float size, int align)
{
    CL_FN(0x5024e0, void (__cdecl *)(const char *, float, float, float, float, float, int, float, float, float, float))(
        text, x, y, width, height, size, align, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* Set a menu target and attach its keyboard shortcut. */
static void menu_value(int *target, int value, int key)
{
    CL_FN(0x502b70, void (__cdecl *)(int, ...))(MENU_SET, target, value);
    if (key)
        CL_FN(0x502b70, void (__cdecl *)(int, ...))(MENU_KEY, key);
}

/* Add the chat control and pause or connection menu. */
static void input_menu(int *connection)
{
    int index;
    float x, y;
    if (!cl_data_input_chat) {
        CL_FN(0x5036d0, void (__cdecl *)(void))();
    } else {
        menu_label(cl_data_input_label_chat, 60.0f, 580.0f, 512.0f, 20.0f, 16.0f, 0);
        CL_FN(0x502b70, void (__cdecl *)(int, ...))(MENU_TEXT, cl_data_input_chat_text, CHAT_CAPACITY);
    }
    if (!sr_game.link_state) {
        menu_label(cl_data_input_label_pause, 0.0f, 0.0f, 96.0f, 24.0f, 16.0f, 0);
        menu_value(&sr_game.link_state, 1, 0);
        CL_FN(0x502b70, void (__cdecl *)(int, ...))(MENU_HOVER);
        CL_FN(0x502b70, void (__cdecl *)(int, ...))(MENU_KEY, KEY_ESCAPE);
    } else if (!sr_game.connections_menu) {
        menu_label(cl_data_input_label_return, 512.0f, 200.0f, 192.0f, 32.0f, 12.0f, 1);
        menu_value(&sr_game.link_state, 0, KEY_ESCAPE);
        menu_label(cl_data_input_label_options, 512.0f, 232.0f, 192.0f, 32.0f, 12.0f, 1);
        menu_value(&sr_game.option_menu, 1, 0);
        menu_label(cl_data_input_label_voice, 512.0f, 264.0f, 192.0f, 32.0f, 12.0f, 1);
        menu_value(&sr_game.voice_menu, 1, 0);
        menu_label(cl_data_input_label_exit, 512.0f, 296.0f, 192.0f, 32.0f, 12.0f, 1);
        menu_value(&sr_game.link_state, 2, KEY_E);
    } else {
        menu_label(cl_data_input_label_return_connections, 4.0f, 4.0f, 192.0f, 32.0f, 12.0f, 0);
        menu_value(&sr_game.connections_menu, 0, KEY_ESCAPE);
        x = 32.0f;
        y = 128.0f;
        for (index = 0; index < sr_data_numofconnections; index++) {
            int player = sr_data_connection[index].player_id;
            menu_label(player == -1 ? cl_data_input_label_empty_connection : sr_players[player].name,
                       x, y, 192.0f, 32.0f, 16.0f, 0);
            menu_value(connection, index, 0);
            x = (float)((double)x + 192.0);
            if ((index + 1) % CONNECTION_COLUMNS == 0) {
                x = 32.0f;
                y = (float)((double)y + 32.0);
            }
        }
    }
}

/* Update capture rules from the player menu and game state. */
static void player_menu_state(Player *player)
{
    if (!cl_data_audio_input_blocked) {
        if (input_pressed(KEY_C)) sr_game.unused_257e0 ^= 1;
        if (input_pressed(KEY_TAB))
            cl_data_input_cursor = !cl_data_input_cursor && player->menu != 0;
    }
    if (sr_game.gametype == GAME_WORLD) {
        if (player->human_id == -1 && player->menu == 1) cl_data_input_cursor = 1;
        if (player->menu_previous == 1 && player->menu == 0) cl_data_input_cursor = 0;
    }
    if (sr_game.gametype == GAME_ROUND) {
        if (player->menu_previous == 2 && player->menu != 2) cl_data_input_cursor = 0;
        if (sr_game.state == ROUND_END) cl_data_input_cursor = 0;
    }
    if (sr_game.gametype != GAME_WORLD && sr_game.state == ROUND_START && player->human_id == -1)
        cl_data_input_cursor = 1;
    player->menu_previous = player->menu;
}

/* Send chat after Return release unless Escape cancels it. */
static void chat_input(void)
{
    if (!cl_data_input_chat) {
        if (input_pressed(KEY_RETURN)) {
            cl_data_input_chat_text[0] = '\0';
            cl_data_audio_menu_exit = 1;
        }
        if (cl_data_audio_keys[KEY_T]) cl_data_audio_voice_mode = 1;
        if (cl_data_audio_keys[KEY_Y]) cl_data_audio_voice_mode = 2;
        if (cl_data_audio_keys[KEY_U]) cl_data_audio_voice_mode = 0;
    }
    if (!cl_data_audio_menu_exit) {
        if (!cl_data_audio_keys[KEY_RETURN]) {
            if (cl_data_input_chat && cl_data_input_chat_text[0] && !cl_data_audio_keys[KEY_ESCAPE]) {
                cl_data_input_chat_length = (uint32_t)strlen(cl_data_input_chat_text);
                CL_FN(0x4d5380, void (__cdecl *)(int, const char *))(
                    sr_data_view, cl_data_input_chat_text);
                cl_data_input_chat_head = (cl_data_input_chat_head + 1) & CHAT_HISTORY_MASK;
            }
            cl_data_input_chat = 0;
        }
    } else {
        cl_data_input_chat = 1;
    }
}

/* Select the next live human on a playable team. */
static void spectator_input(void)
{
    unsigned index, stop;
    if (sr_game.gametype != GAME_ROUND || sr_game.unused_257dc || cl_data_audio_input_blocked)
        return;
    if (cl_data_audio_keys[KEY_1]) sr_game.spectator_human = -1;
    if (!input_pressed(KEY_SPACE))
        return;
    stop = sr_game.spectator_human;
    if (sr_game.spectator_human < 0) {
        index = 0;
        stop = HUMAN_MASK;
    } else {
        index = sr_game.spectator_human + 1;
    }
    index &= HUMAN_MASK;
    stop &= HUMAN_MASK;
    do {
        if (sr_humans[index].active && sr_humans[index].team >= 0 && sr_humans[index].team < TEAM_COUNT) {
            sr_game.spectator_human = index;
            break;
        }
        index = (index + 1) & HUMAN_MASK;
    } while (index != stop);
}

/* Choose movement from the body, health, seat and vehicle type. */
static int player_control_mode(const Player *player)
{
    const Human *human;
    if (player->human_id == -1)
        return CONTROL_NONE;
    human = &sr_humans[player->human_id];
    if (human->health <= 0)
        return CONTROL_NONE;
    if (human->vehicle_id == -1)
        return CONTROL_HUMAN;
    if (sr_data_objecttype[sr_objects[human->vehicle_id].type].state == 1 || human->vehicle_seat != 0)
        return CONTROL_CAR;
    return CONTROL_HELI;
}

/* Poll inputs and dispatch controls. Player -1 omits player menu and movement updates. */
void getinputs(int player_index)
{
    int connection = -1;
    int running;
    Player *player = player_index == -1 ? NULL : &sr_players[player_index];
    cl_data_audio_menu_selection = 0;
    input_menu(&connection);
    if (player) player_menu_state(player);
    running = sr_game.link_state != 1;
    if (!running) cl_data_input_cursor = 1;
    cl_data_input_game_running = running;
    if (!sr_game.link_state && player && cl_data_input_cursor)
        CL_FN(0x47e500, void (__cdecl *)(int))(player_index);
    CL_FN(0x5060d0, void (__cdecl *)(void))();
    checkkeyboard();
    if (!cl_data_input_cursor) checkmouserelative();
    else checkmouse();
    CL_FN(0x501be0, void (__cdecl *)(void))();
    if (!sr_game.link_state && !running) cl_data_input_cursor = 0;
    if (!cl_data_input_cursor) {
        if (!cl_data_input_mouse.left) cl_data_input_fire_blocked = 0;
    } else {
        cl_data_input_fire_blocked = 1;
    }
    if (!cl_data_input_cursor && cl_data_input_cursor_previous) {
        checkmouserelative();
        checkmouserelative();
    }
    cl_data_input_cursor_previous = cl_data_input_cursor;
    if (sr_server && connection != -1) {
        sr_data_kickedip[sr_data_numofkicked] = sr_data_connection[connection].ip;
        sr_data_numofkicked++;
        sr_data_connection[connection].idle_ticks = CONNECTION_TIMEOUT;
    }
    chat_input();
    if (cl_data_audio_debug) debug_getinputs();
    if (!player)
        return;
    if (player->menu_action) {
        CL_FN(0x4d5490, void (__cdecl *)(int, int, int))(player_index, player->menu, player->menu_action);
        player->menu_action = 0;
    }
    spectator_input();
    player->input_mode = player_control_mode(player);
    if (player->input_mode == CONTROL_NONE) {
        int key;
        for (key = 0; key < INPUT_AXIS_COUNT; key++) player->input_axes[key] = 0.0f;
        player->input_buttons = 0;
        player->zoom = 0.0f;
        player->zoom_target = 0.0f;
        if (sr_game.gametype == GAME_ROUND && !cl_data_input_cursor) movecamera2(1);
        if (!cl_data_input_cursor && !cl_data_audio_input_blocked) {
            for (key = 0; key < SPECTATOR_SLOT_KEYS; key++)
                if (cl_data_audio_keys[KEY_1 + key]) player->input_buttons |= INPUT_SLOT_1 << key;
            if (cl_data_audio_keys[KEY_R]) player->input_buttons |= INPUT_RELOAD;
        }
    }
    if (player->input_mode == CONTROL_HUMAN) getinputs_human(player_index);
    else if (player->input_mode == CONTROL_CAR) getinputs_car(player_index);
    else if (player->input_mode == CONTROL_HELI) getinputs_heli(player_index);
    player->unused_cc = player->input_mode;
}
