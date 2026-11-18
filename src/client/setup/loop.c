/* The game loop of the client: the start of a game, one frame for each pass (the textures,
 * the camera, the render, the voice, the display texts and the ticks), and the end of the game.
 * A hosted game runs the server in the same process. A joined game first waits until the
 * events of the server are in.
 */
#include "setup/loop.h"

#include <math.h>
#include <string.h>

#include "audio/imports.h"
#include "client.h"
#include "account_actions.h"
#include "debug/debug.h"
#include "editor/level.h"
#include "math/small_vectors.h"
#include "menu/draw.h"
#include "menu/front.h"
#include "menu/items.h"
#include "render/display.h"
#include "render/draw2d.h"
#include "render/frame.h"
#include "render/help.h"
#include "render/hud.h"
#include "render/lists.h"
#include "render/messages.h"
#include "render/text.h"
#include "render/view.h"
#include "setup/simulation.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "textures/textures.h"
#include "vectors.h"

#define g_game sr_data_game
#define g_human sr_data_human
#define g_player sr_data_player
#define g_debugit sr_data_debugit
#define g_view cl_data_view_player
#define g_keys cl_data_audio_keys

#define NUM_PLAYERS 256
#define NUM_HUMANS 512
#define NUM_SECTORS 0x2000
#define NUM_OBJECTS 1024
#define NUM_CHANNELS 8
#define NUM_FRAMES 64
#define FRAME_RING 32
#define NO_LOOK -2
#define GAME_ROUND 3
#define GAME_WORLD 4
#define STATE_RUNNING 2
#define LINK_LOST 2
#define DEBUG_MONEY 10000
#define START_TEAM 7
#define NO_TEAM_COLOR 2
#define LIVE_CAMERA_TICKS 180
#define ALL_EVENTS 0x10000
#define FIRST_WAIT_STEPS 20
#define EXTRA_TICKS 4
#define CATCH_UP_STEPS 50
#define WAVE_POINTS 0x13f
#define WAVE_STEP 32
#define QUIT_PACKET 7
#define BONE_HEAD 2
#define EYE_STEP 0.0625f
#define DEBUG_EYE_BACK 4.0f
#define KEY_L 15
#define KEY_X 27
#define KEY_COMMA 54
#define KEY_F1 58
#define KEY_F3 60
#define KEY_ESCAPE 41
#define OPTION_KEY 1
#define GL_LINES 0x0001
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_TEXTURE_2D 0x0de1
#define GL_BLEND 0x0be2
#define AL_FORMAT_MONO16 0x1101
#define AL_GAIN 0x100a
#define AL_BUFFERS_PROCESSED 0x1016
#define VOICE_RATE 8000
#define VOICE_BYTES 0x500

AUDIO_IMPORT("SDL2.DLL", __cdecl, uint32_t, SDL_GetTicks, (void))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_GL_SwapWindow, (void *))
AUDIO_IMPORT("SDL2.DLL", __cdecl, int, SDL_SetRelativeMouseMode, (int))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_Delay, (uint32_t))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClearColor, (float, float, float, float))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClear, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glColor3f, (float, float, float))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glFlush, (void))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glFinish, (void))

/* Game functions of other files. */
#define createaccount CL_FN(0x433be0, int(__cdecl *)(int player))
#define createplayer CL_FN(0x4d4750, int(__cdecl *)(void))
#define fixplayername CL_FN(0x4d5200, void(__cdecl *)(int player))
#define deleteplayer CL_FN(0x4d4a20, void(__cdecl *)(int player))
#define resetgame CL_FN(0x479ad0, void(__cdecl *)(void))
#define checkkeyboard CL_FN(0x4fec50, void(__cdecl *)(void))
#define checksystemmessages CL_FN(0x5060d0, void(__cdecl *)(void))
#define checkmouse CL_FN(0x4feca0, void(__cdecl *)(void))
#define checkmouserelative CL_FN(0x4fedb0, void(__cdecl *)(void))
#define debug_getinputs CL_FN(0x469190, void(__cdecl *)(void))
#define area_resetsector CL_FN(0x439370, void(__cdecl *)(int sector))
#define calculateplayervoice CL_FN(0x4fbe70, void(__cdecl *)(int connection, int player))
#define soundsimulation CL_FN(0x44e190, void(__cdecl *)(const vec3 *position, const vec3 *rotation, \
                                                         const vec3 *velocity))
#define getaudioinput CL_FN(0x4fb6f0, void(__cdecl *)(int player))
#define frame_drawvoiceicons CL_FN(0x479330, void(__cdecl *)(void))
#define deletesound CL_FN(0x44f720, void(__cdecl *)(int sound))
#define building_editorloop CL_FN(0x458d20, void(__cdecl *)(void))
#define voiceoptionsmenu CL_FN(0x4d38c0, void(__cdecl *)(void))
#define resetsendpacket CL_FN(0x4fe4f0, void(__cdecl *)(void))
#define cs_pwrite CL_FN(0x4fe5b0, int(__cdecl *)(const void *data, int size, int count))
#define sendpacket CL_FN(0x4fe520, void(__cdecl *)(uint32_t address, uint16_t port))

/* Clear the chat, the players and the human looks. A hosted game makes the player of the host:
 * the name of the options, and in a round game an account with its money (at least 10000 with
 * the debug flag) and team 7. */
static void game_start_players(void)
{
    int i, player;

    cl_data_input_cameras[0].pos.x = 512.0f;
    cl_data_input_cameras[0].pos.y = 64.0f;
    cl_data_input_cameras[0].pos.z = 512.0f;
    cl_data_audio_voice_mode = 1;
    cl_data_chat_head = 0;
    memset(cl_data_chatmessage, 0, sizeof(ChatMessage) * NUM_PLAYERS);
    for (i = 0; i < NUM_PLAYERS; i++) {
        g_player[i].active = 0;
        g_player[i].num_actions = 0;
        g_player[i].action_tail = 0;
        cl_data_player_marks[i] = NO_LOOK;
    }
    for (i = 0; i < NUM_HUMANS; i++)
        cl_data_texture_human[i].team = NO_LOOK;
    if (sr_data_server.active == 0)
        return;
    createclientaccount(&cl_data_client_account_id[0], &cl_data_client_account_id[1], (int)g_game.client_account_key);
    player = createplayer();
    strcpy(g_player[player].name, sr_data_option.name);
    fixplayername(player);
    if (g_game.gametype != GAME_ROUND)
        return;
    g_player[player].account_id[0] = (uint32_t)cl_data_client_account_id[0];
    g_player[player].account_id[1] = (uint32_t)cl_data_client_account_id[1];
    g_player[player].account = createaccount(player);
    if (g_player[player].account != -1)
        g_player[player].money = sr_data_accounts.records[g_player[player].account].money;
    if (g_debugit != 0 && g_player[player].money < DEBUG_MONEY)
        g_player[player].money = DEBUG_MONEY;
    g_player[player].team = START_TEAM;
}

/* A joined game waits until the server sent all its events, or until the player leaves. A
 * total of 0x10000 events draws nothing. The bar shows the events that came in. */
static void game_wait_events(void)
{
    SrSectorStore *link = &sr_data_area_sector;

    link->client_connected = 0;
    while (cl_data_menu_items[0].pressed == 0 && cl_data_audio_menu_back == 0 &&
           (link->client_connected == 0 || (int)sr_data_numofevents < link->client_event_total)) {
        if (link->client_event_total != ALL_EVENTS) {
            audio_glClearColor()(cl_data_menu_clear_color[0], cl_data_menu_clear_color[1], cl_data_menu_clear_color[2],
                                 0.0f);
            audio_glClear()(GL_COLOR_BUFFER_BIT);
            cl_data_menu_item_count = 0;
            createmenuitem("", 0.0f, 0.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
            setmenuitem(OPTION_KEY, KEY_ESCAPE);
            checkkeyboard();
            checksystemmessages();
            checkmenuitems();
            setuptextdisplay();
            drawtext("Synchronizing...", 512.0f, 32.0f, 16.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f);
            if (g_debugit != 0) {
                debug_getinputs();
                displaydebuginfo();
            }
            drawbitmap(-1, 512.0f, 160.0f, 320.0f, 20.0f, 1, 0.25f, 0.25f, 0.25f, 1.0f);
            if ((int)sr_data_numofevents > 0)
                drawbitmap(-1, 352.0f, 160.0f,
                           (float)((double)(float)(int)sr_data_numofevents * 320.0 / (float)link->client_event_total),
                           20.0f, 0, 0.75f, 0.75f, 0.75f, 1.0f);
            audio_SDL_GL_SwapWindow()(cl_data_audio_window);
        }
        checksimulation(0);
        checksimulation(0);
    }
}

/* Fill the four OpenAL buffers of each voice channel with its samples and queue them. */
static void game_start_voice(void)
{
    int i, k;

    for (i = 0; i < NUM_CHANNELS; i++) {
        AudioChannel *channel = &cl_data_audio_channels[i];

        for (k = 0; k < 4; k++)
            audio_alBufferData()(channel->buffers[k], AL_FORMAT_MONO16, channel->samples, VOICE_BYTES, VOICE_RATE);
        audio_alSourceQueueBuffers()(channel->source, 4, channel->buffers);
    }
}

/* Start a game: the players, the reset, the wait of a joined game, the cursor, the voice and
 * the relative mouse. */
static void game_start(void)
{
    game_start_players();
    g_game.running = 1;
    resetgame();
    if (sr_data_server.active == 0)
        game_wait_events();
    cl_data_input_cursor = 0;
    if (g_view != -1 && g_player[g_view].human_id == -1 && (g_game.state == 1 || g_game.gametype == GAME_WORLD))
        cl_data_input_cursor = 1;
    game_start_voice();
    checkmouse();
    checkmouse();
    checkmouserelative();
    audio_SDL_SetRelativeMouseMode()(1);
    cl_data_relative_mouse = 1;
    checkmouserelative();
    checkmouserelative();
    g_game.link_state = 0;
    audio_SDL_GetTicks()();
    resetmenuitems();
}

/* Make the textures that wait: the newspaper, the human looks and the sectors that changed. */
static void frame_textures(void)
{
    int i;

    if (sr_data_newspaper.texture_done == 0)
        generatetexture_newspaper();
    for (i = 0; i < NUM_HUMANS; i++)
        if (g_human[i].active != 0)
            generatehumantexture(i);
    for (i = 0; i < NUM_SECTORS; i++)
        if (sr_data_area_sector.records[i].used != 0 && sr_data_area_sector.records[i].dirty != 0)
            area_resetsector(i);
}

/* Turn the camera from a yaw and a pitch, at a distance behind its place. */
static void camera_from_angles(const InputCamera *from, int back)
{
    vec3 *rot = cl_data_camera_rotation;

    cl_data_camera_position = from->pos;
    cl_data_view_zoom = 1.0f;
    resetorientation(rot);
    rotateorientation(rot, &rot[1], from->yaw);
    rotateorientation(rot, rot, from->pitch);
    if (back)
        scaleaddvectors(&cl_data_camera_position, &cl_data_camera_position, &rot[2], from->zoom);
}

/* The camera with no living human: the free camera with no viewed player, else the camera
 * that follows the spectated human. It waits until the death count is 0. */
static void frame_camera_free(void)
{
    InputCamera *follow = &cl_data_input_cameras[1];

    if (g_game.death_camera_ticks > 0)
        g_game.death_camera_ticks--;
    if (g_game.death_camera_ticks != 0)
        return;
    if (g_view == -1) {
        cl_data_camera_position = cl_data_input_cameras[0].pos;
        resetorientation(cl_data_camera_rotation);
        rotateorientation(cl_data_camera_rotation, &cl_data_camera_rotation[1], cl_data_input_cameras[0].yaw);
        rotateorientation(cl_data_camera_rotation, cl_data_camera_rotation, cl_data_input_cameras[0].pitch);
        cl_data_view_zoom = 1.0f;
        zerovector((unsigned int *)&cl_data_camera_velocity);
        return;
    }
    camera_from_angles(follow, 1);
    zerovector((unsigned int *)&cl_data_camera_velocity);
    if (g_game.spectator_human != -1 && g_human[g_game.spectator_human].active == 0)
        g_game.spectator_human = -1;
    if (g_game.spectator_human == -1)
        return;
    follow->pos = g_human[g_game.spectator_human].pos;
    camera_from_angles(follow, 1);
    cl_data_camera_velocity = g_human[g_game.spectator_human].vel;
}

/* Turn the head view a part of the way to where the human looks: the yaw from the view, the
 * pitch from the view against the floor. On foot, the turn is the cube of the up part of the
 * second row (no turn when it points down). In a vehicle, the angles are against the rows of
 * the vehicle and the turn is full. */
static void frame_camera_look(const Human *human, int index)
{
    vec3 *rot = cl_data_camera_rotation;
    float weight, angle;

    if (human->vehicle_id == -1) {
        weight = rot[1].y <= 0.0f ? 0.0f : (float)fabs((double)rot[1].y * rot[1].y * rot[1].y);
        angle = (float)atan2((double)-rot[2].x, (double)rot[2].z);
        angle = angulardistance((float)(human->axis + human->arm_axis), angle);
        rotateorientation(rot, &rot[1], (float)(angle * 0.75 * weight));
        angle = (float)atan2((double)rot[2].y, sqrt((double)rot[2].z * rot[2].z + (double)rot[2].x * rot[2].x));
        angle = angulardistance((float)(human->pitch + human->arm_pitch), angle);
        rotateorientation(rot, rot, (float)(angle * 0.75 * weight));
    } else {
        const vec3 *vehicle = sr_data_object[human->vehicle_id].rot;
        float side = dotproduct(&rot[2], &vehicle[0]);
        float up = dotproduct(&rot[2], &vehicle[1]);
        float ahead = dotproduct(&rot[2], &vehicle[2]);

        weight = 1.0f;
        angle = (float)atan2((double)-side, (double)ahead);
        angle = angulardistance(g_human[index].vehicle_axis, angle);
        rotateorientation(rot, &rot[1], (float)(angle * 0.75 * weight));
        angle = (float)atan2((double)up, sqrt((double)ahead * ahead + (double)side * side));
        angle = angulardistance(g_human[index].vehicle_pitch, angle);
        rotateorientation(rot, rot, (float)(angle * 0.75 * weight));
    }
}

/* The camera at the head of the viewed human. The death count starts again while the human
 * lives, and counts down after the death with the dead human as the spectated one. The X key
 * with the debug flag moves the camera 4 back with no turn. */
static void frame_camera_head(int index)
{
    const Human *human = &g_human[index];
    const Bone *head = &human->bones[BONE_HEAD];
    vec3 *rot = cl_data_camera_rotation;

    if (human->health < 1) {
        g_game.spectator_human = index;
        g_game.death_camera_ticks--;
    } else {
        g_game.death_camera_ticks = LIVE_CAMERA_TICKS;
    }
    resetorientation(rot);
    cl_data_camera_position = head->pos;
    rotateorientation(rot, &rot[1], human->axis);
    rotateorientation(rot, rot, (float)(human->pitch * 0.5));
    rotateorientation(rot, rot, (float)(human->pitch * 0.5));
    cl_data_view_zoom = (float)(1.0 - g_player[g_view].zoom);
    rot[0] = head->rot[0];
    rot[1] = head->rot[1];
    rot[2] = head->rot[2];
    scaleaddvectors(&cl_data_camera_position, &cl_data_camera_position, &rot[1], EYE_STEP);
    frame_camera_look(human, index);
    if (g_debugit != 0 && cl_data_audio_input_blocked == 0 && g_keys[KEY_X] != 0) {
        resetorientation(rot);
        scaleaddvectors(&cl_data_camera_position, &cl_data_camera_position, &rot[2], DEBUG_EYE_BACK);
    }
    cl_data_camera_velocity = head->vel;
}

/* Place the camera: at the head of the viewed human while it lives (or the death count still
 * runs), else the free camera. */
static void frame_camera(void)
{
    int index = g_view == -1 ? -1 : g_player[g_view].human_id;

    if (index == -1 || (g_human[index].health < 1 && g_game.death_camera_ticks < 1))
        frame_camera_free();
    else
        frame_camera_head(index);
}

/* A hosted game: find the speakers that the host hears and copy their frames to the voice
 * channels. */
static void frame_voice(void)
{
    VoiceSlot *slots = sr_data_connection[NUM_PLAYERS - 1].voice;
    int i, k;

    sr_data_connection[NUM_PLAYERS - 1].spectate = g_game.spectator_human;
    calculateplayervoice(NUM_PLAYERS - 1, g_view);
    for (i = 0; i < NUM_CHANNELS; i++) {
        AudioChannel *channel = &cl_data_audio_channels[i];
        const VoiceRing *ring;

        channel->active = slots[i].active;
        if (slots[i].active == 0)
            continue;
        channel->player = slots[i].player;
        channel->human = slots[i].human;
        channel->item = slots[i].item;
        ring = &sr_data_speex.voice_ring[slots[i].player];
        channel->head = ring->head;
        for (k = 0; k < NUM_FRAMES; k++) {
            channel->size[k] = ring->size[k];
            memcpy(&channel->data[k * 256], ring->data[k], (size_t)ring->size[k]);
        }
    }
}

/* The frame texture over the screen, or the shadow map with L and the debug flag. */
static void frame_overlay(void)
{
    int texture = sr_data_texturelist.frame_color;

    if (g_debugit != 0 && g_keys[KEY_L] != 0 && cl_data_audio_input_blocked == 0)
        texture = sr_data_texturelist.shadow_map;
    glDisable(GL_BLEND);
    drawbitmap(texture, 0.0f, 0.0f, 1024.0f, 640.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_BLEND);
}

/* The chat label of the input line: Chat with no viewed player, else the voice mode. */
static void frame_chat_label(void)
{
    if (g_view == -1) {
        drawtext("Chat:", 4.0f, 580.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }
    if (cl_data_audio_voice_mode == 0)
        drawtext("Whisper:", 4.0f, 580.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    if (cl_data_audio_voice_mode == 1)
        drawtext("Say:", 4.0f, 580.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    if (cl_data_audio_voice_mode == 2)
        drawtext("Yell:", 4.0f, 580.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* The spectator texts of a dead player in a running round game: the name of the spectated
 * player in its team color, and the keys. */
static void frame_spectator_texts(void)
{
    if (g_view == -1 || g_game.gametype != GAME_ROUND || g_game.state != STATE_RUNNING ||
        g_player[g_view].human_id != -1 || g_game.death_camera_ticks != 0)
        return;
    drawtext("Spectating", 512.0f, 512.0f, 16.0f, 0x21, 1.0f, 1.0f, 1.0f, 1.0f);
    if (g_game.spectator_human != -1 && g_human[g_game.spectator_human].player_id != -1) {
        int owner = g_human[g_game.spectator_human].player_id;
        int team = g_player[owner].team;

        if (team == -1)
            team = NO_TEAM_COLOR;
        drawtext(g_player[owner].name, 512.0f, 528.0f, 20.0f, 0x21, sr_data_teamcolor[team * 3],
                 sr_data_teamcolor[team * 3 + 1], sr_data_teamcolor[team * 3 + 2], 1.0f);
    }
    drawtext("Press Space to switch player", 512.0f, 548.0f, 16.0f, 0x21, 1.0f, 1.0f, 1.0f, 1.0f);
    drawtext("Press 1 for freecam", 512.0f, 564.0f, 16.0f, 0x21, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* The wave of the voice ring of the viewed player: one sample of each 32, as a line. */
static void frame_voice_wave(void)
{
    const int16_t *samples = &sr_data_speex.voice_ring[g_view].samples[0][0];
    int i;

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    audio_glColor3f()(1.0f, 1.0f, 1.0f);
    for (i = 0; i < WAVE_POINTS; i++) {
        glVertex2f((float)((float)i + 16.0), (float)((float)samples[i * WAVE_STEP] / 1024.0 + 32.0));
        glVertex2f((float)((float)(i + 1) + 16.0), (float)((float)samples[(i + 1) * WAVE_STEP] / 1024.0 + 32.0));
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

/* The values of the debug display: the team of player 0, the bonds, the humans, the parts of
 * human 0 and the mouse button. */
static void frame_debug_values(void)
{
    Debug *debug = &sr_data_debug;
    int i;

    debug->shown_team = g_player[0].team;
    debug->shown_bonds = sr_data_numofbonds;
    debug->shown_humans = 0;
    for (i = 0; i < NUM_HUMANS; i++)
        if (g_human[i].active != 0)
            debug->shown_humans++;
    debug->shown_parts = g_human[0].num_joints + g_human[0].num_muscles;
    debug->shown_mouse = cl_data_input_mouse.left + 100;
    displaydebuginfo();
}

/* The display: the HUD, the voice icons, the messages, the texts of the state and of the
 * voice, the help, the debug texture and the mouse cursor. */
static void frame_display(void)
{
    int voice_shown = g_view != -1 && sr_data_speex.voice_ring[g_view].voice_off == 0;

    setuptextdisplay();
    if (sr_data_option.frame_overlay != 0)
        frame_overlay();
    if (cl_data_hide_hud == 0 && g_game.state != 0 && g_view != -1)
        drawhud(g_view);
    if (cl_data_hide_hud == 0)
        frame_drawvoiceicons();
    if (cl_data_hide_messages == 0)
        frame_drawmessages();
    if (cl_data_hide_hud == 0)
        drawdisplay();
    if (voice_shown) {
        drawbitmap(-1, 20.0f, 596.0f, 32.0f, 32.0f, 5, 0.5f, 0.5f, 0.5f, 0.5f);
        drawtext("O", 20.0f, 592.0f, 16.0f, 5, 1.0f, 1.0f, 1.0f, 0.5f);
        drawtext("V", 20.0f, 600.0f, 16.0f, 5, 1.0f, 1.0f, 1.0f, 0.5f);
    }
    if (cl_data_input_chat != 0)
        frame_chat_label();
    if (cl_data_death_fade > 0.0f) {
        drawbitmap(-1, 0.0f, 0.0f, 1024.0f, 640.0f, 0, 0.0f, 0.0f, 0.0f, cl_data_death_fade);
        if (cl_data_death_fade > 0.9375f)
            drawtext("You are dead", 512.0f, 320.0f, 16.0f, 0x21, 1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (g_game.link_state > 0)
        drawbitmap(-1, 0.0f, 0.0f, 1024.0f, 640.0f, 0, 0.0f, 0.0f, 0.0f, 0.5f);
    drawmenuitems();
    frame_spectator_texts();
    drawtext("Mic:/i", 4.0f, 64.0f, 12.0f, 0x20, 1.0f, 1.0f, 1.0f, cl_data_mic_text_timer,
             cl_data_mic_gain_percent);
    if (voice_shown)
        frame_voice_wave();
    if (cl_data_show_help != 0)
        drawhelp();
    if (g_debugit != 0) {
        glDisable(GL_BLEND);
        if (g_keys[KEY_COMMA] != 0 && cl_data_audio_input_blocked == 0)
            drawbitmap(sr_data_texturelist.material[sr_data_materiallist.preset_end], 0.0f, 0.0f, 512.0f, 512.0f,
                       0, 1.0f, 1.0f, 1.0f, 1.0f);
        glEnable(GL_BLEND);
    }
    if (cl_data_input_cursor != 0)
        drawmousecursor(cl_data_input_mouse.x, cl_data_input_mouse.y, 16, 1.0f, 1.0f, 1.0f, 1.0f);
    if (g_debugit != 0)
        frame_debug_values();
}

/* Keep the time of the frame in a ring of 32 and its mean. F3 with the debug flag clears the
 * longest time. */
static void frame_timing(void)
{
    Debug *debug = &sr_data_debug;
    int i;

    debug->frame_ms[debug->frame_ms_index] = (int)(audio_SDL_GetTicks()() - debug->frame_start);
    debug_addataint("mspf", debug->frame_ms[debug->frame_ms_index]);
    if (debug->frame_ms_max < debug->frame_ms[debug->frame_ms_index])
        debug->frame_ms_max = debug->frame_ms[debug->frame_ms_index];
    debug_addataint("mspfmax", debug->frame_ms_max);
    if (g_keys[KEY_F3] != 0)
        debug->frame_ms_max = 0;
    debug->frame_ms_index = (debug->frame_ms_index + 1) & (FRAME_RING - 1);
    debug->frame_ms_average = 0.0f;
    for (i = 0; i < FRAME_RING; i++)
        debug->frame_ms_average = (float)((float)debug->frame_ms[i] + debug->frame_ms_average);
    debug->frame_ms_average = (float)(debug->frame_ms_average / 32.0);
}

/* Run the ticks of the frame: wait up to 20 ms for the first tick, then 4 more. A joined game
 * then runs up to 50 more ticks until the server sent its state. */
static void frame_ticks(void)
{
    int i;

    sr_data_area_sector.client_connected = 0;
    for (i = 0; checksimulation(1) == 0 && i < FIRST_WAIT_STEPS; i++)
        audio_SDL_Delay()(1);
    for (i = 0; i < EXTRA_TICKS; i++)
        checksimulation(1);
    if (sr_data_server.active != 0)
        return;
    for (i = 0; sr_data_area_sector.client_connected == 0 && i < CATCH_UP_STEPS; i++) {
        checksimulation(1);
        audio_SDL_Delay()(1);
    }
}

/* Run a menu or an editor from the game, with the relative mouse off. */
static void frame_menus(void)
{
    int i;

    if (g_debugit != 0 && g_keys[KEY_F1] != 0) {
        for (i = cl_data_audio_sound_count; i >= 0; i--)
            deletesound(i);
        checkmouserelative();
        checkmouse();
        if (g_game.gametype == 1)
            building_editorloop();
        else
            leveleditor_loop();
        cl_data_last_tick_ms = audio_SDL_GetTicks()();
        checkmouse();
        checkmouserelative();
    }
    if (g_game.option_menu != 0) {
        checkmouserelative();
        checkmouse();
        optionsmenu();
        g_game.option_menu = 0;
        checkmouse();
        checkmouserelative();
    }
    if (g_game.voice_menu != 0) {
        checkmouserelative();
        checkmouse();
        voiceoptionsmenu();
        g_game.voice_menu = 0;
        checkmouse();
        checkmouserelative();
    }
}

/* End a game: the players of a host go, a joined game sends the quit packet, the voice
 * channels stop and empty their queues, and the objects and the sounds go. */
static void game_end(void)
{
    int i, count, buffer;
    uint8_t quit = QUIT_PACKET;

    g_game.running = 0;
    cl_data_relative_mouse = 0;
    audio_SDL_SetRelativeMouseMode()(0);
    if (sr_data_server.active != 0) {
        for (i = 0; i < NUM_PLAYERS; i++)
            if (g_player[i].active != 0)
                deleteplayer(i);
    } else {
        resetsendpacket();
        cs_pwrite(&quit, 1, 1);
        sendpacket(cl_data_server_address, cl_data_server_port);
    }
    for (i = 0; i < NUM_CHANNELS; i++) {
        uint32_t source = cl_data_audio_channels[i].source;

        if (source == 0xffffffffu)
            continue;
        audio_alSourcef()(source, AL_GAIN, 0.0f);
        audio_alSourceStop()(source);
        audio_alGetSourcei()(source, AL_BUFFERS_PROCESSED, &count);
        for (; count > 0; count--)
            audio_alSourceUnqueueBuffers()(source, 1, (unsigned *)&buffer);
    }
    for (i = 0; i < NUM_OBJECTS; i++)
        sr_data_object[i].active = 0;
    for (i = cl_data_audio_sound_count; i >= 0; i--)
        deletesound(i);
}

/* Run a game until the link is lost or the window closes. */
void __cdecl gameloop(void)
{
    game_start();
    while (g_game.link_state < LINK_LOST && cl_data_audio_menu_back == 0) {
        sr_data_debug.frame_start = audio_SDL_GetTicks()();
        frame_textures();
        render_buildareasectorshadow();
        frame_camera();
        renderframe();
        if (sr_data_server.active != 0)
            frame_voice();
        soundsimulation(&cl_data_camera_position, cl_data_camera_rotation, &cl_data_camera_velocity);
        getaudioinput(g_view == -1 ? 0 : g_view);
        sr_data_area_sector.client_connected = 0;
        frame_display();
        audio_glFlush()();
        frame_timing();
        frame_ticks();
        audio_glFinish()();
        audio_SDL_GL_SwapWindow()(cl_data_audio_window);
        frame_menus();
    }
    game_end();
}
