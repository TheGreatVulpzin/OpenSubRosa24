/* Voice icons show the direction and volume of active speakers.
 * The voice menu shows captured samples and accepts a talk key or mouse button.
 * Screen coordinates use a fixed 1024 by 640 canvas.
 */
#include "audio/voice_ui.h"
#include "audio/voice.h"
#include "audio/sound.h"
#include "audio/imports.h"
#include "client.h"
#include "sr_runtime.h"
#include "vectors.h"
#include "sr_math.h"
#include <stdlib.h>

enum { ICON_CHANNELS = 8, ICON_FRAME_SAMPLES = 160, ICON_FRAMES = 64,
       ICON_HEAD_BONE = 2, ICON_CENTER = 5, MENU_KEYS = 323,
       MENU_MOUSE_FIRST = 3, MENU_MOUSE_END = 16, MENU_ESCAPE = 41,
       MENU_WAVE_POINTS = 1024, MENU_WAVE_STRIDE = 10,
       MENU_BUTTON = 1, MENU_SLIDER = 19, MENU_TOGGLE = 10,
       GL_LINES_MODE = 1, GL_TEXTURE_MODE = 0xde1, GL_CLEAR_MASK = 0x4100,
       ICON_SIZE_MAX = 32, ICON_CENTER_X = 512, ICON_CENTER_Y = 320,
       ICON_TOP = 24, ICON_BOTTOM = 616,
       MENU_MARGIN = 4, MENU_BACK_WIDTH = 160, MENU_BACK_HEIGHT = 20,
       MENU_LEFT = 112, MENU_RIGHT = 512, MENU_WIDTH = 256, MENU_HEIGHT = 24,
       MENU_CONTROLS_Y = 400, MENU_VOICE_Y = 512, MENU_WAVE_Y = 192,
       MENU_WAVE_RIGHT = 912, MENU_FONT = 16, MENU_ACTIVE_SIZE = 32,
       MENU_ACTIVE_TITLE_Y = 188, MENU_ACTIVE_DETAIL_Y = 196, MENU_DETAIL_FONT = 14,
       MENU_BIND_X = 240, MENU_BIND_Y = 412, MENU_BIND_HINT_Y = 432,
       MENU_CURSOR_SIZE = 16, MENU_DEBUG_FONT = 10, MENU_KEY_NAME_BYTES = 16,
       MENU_THRESHOLD_MAX = 32768, MENU_THRESHOLD_QUANTUM = 256 };

#define ICON_SAMPLE_SCALE 1024.0
#define ICON_FRAME_DIVISOR 160.0
#define ICON_LEVEL_MIN 0.25
#define ICON_DISTANCE_SCALE 0.125
#define ICON_HEAD_OFFSET 0.5f
#define ICON_BEHIND -0.125f
#define ICON_REAR_COLOR 0.75f
#define ICON_SIZE_SCALE 0.875
#define ICON_ALPHA_SCALE 0.75
#define MENU_WAVE_SCALE 256.0
#define MENU_ACTIVE_ALPHA 0.5f

/* Game functions of other files. */
#define drawbitmap CL_FN(0x507ac0, void (__cdecl *)(int, float, float, float, float, unsigned, float, float, float, float))
#define resetmenuitems CL_FN(0x503970, void (__cdecl *)(void))
#define createmenubutton CL_FN(0x502b70, void (__cdecl *)(int, ...))
#define createmenuitemempty CL_FN(0x5036d0, void (__cdecl *)(void))
#define checksystemmessages CL_FN(0x5060d0, void (__cdecl *)(void))
#define checkkeyboard CL_FN(0x4fec50, void (__cdecl *)(void))
#define checkmouse CL_FN(0x4feca0, void (__cdecl *)(void))
#define checkmenuitems CL_FN(0x501be0, void (__cdecl *)(void))
#define setuptextdisplay CL_FN(0x5079d0, void (__cdecl *)(void))
#define drawmenuitems CL_FN(0x4ff2e0, void (__cdecl *)(void))
#define drawtext CL_FN(0x506b30, void (__cdecl *)(char *, float, float, float, unsigned, float, float, float, float, ...))
#define drawmousecursor CL_FN(0x507ee0, void (__cdecl *)(int, int, int, float, float, float, float))
#define checksimulation CL_FN(0x47d900, int (__cdecl *)(int))

AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glEnable, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glDisable, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glBegin, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glEnd, (void))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glColor3f, (float, float, float))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glVertex2f, (float, float))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClear, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClearColor, (float, float, float, float))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_GL_SwapWindow, (void *))

/* Draw one icon for each active voice. No icon is drawn for an inactive voice. */
void frame_drawvoiceicons(void)
{
    int channel, sample;
    for (channel = 0; channel < ICON_CHANNELS; channel++) {
        AudioChannel *voice = &cl_data_audio_channels[channel];
        vec3 direction;
        float amplitude = 0.0f, horizontal, vertical, depth, size, radius, x, y, color;
        double angle;
        int frame;
        if (!voice->active) continue;
        frame = voice->tail - 1;
        if (frame == -1) frame = ICON_FRAMES - 1;
        for (sample = 0; sample < ICON_FRAME_SAMPLES; sample++)
            amplitude = (float)((double)(float)abs(voice->samples[frame * ICON_FRAME_SAMPLES + sample]) /
                                ICON_SAMPLE_SCALE + amplitude);
        amplitude = ICON_LEVEL_MIN <= amplitude / ICON_FRAME_DIVISOR ?
                    (float)(amplitude / ICON_FRAME_DIVISOR) : (float)ICON_LEVEL_MIN;
        direction = sr_humans[voice->human].bones[ICON_HEAD_BONE].pos;
        scaleaddvectors(&direction, &direction, &cl_data_audio_camera.rot[1], ICON_HEAD_OFFSET);
        subtractvectors(&direction, &direction, &cl_data_audio_camera.pos);
        horizontal = dotproduct(&direction, &cl_data_audio_camera.rot[0]);
        vertical = dotproduct(&direction, &cl_data_audio_camera.rot[1]);
        depth = (float)-dotproduct(&direction, &cl_data_audio_camera.rot[2]);
        size = (float)(ICON_SIZE_MAX / ((double)vectorlength(&direction) * ICON_DISTANCE_SCALE));
        if (ICON_SIZE_MAX < size) size = ICON_SIZE_MAX;
        angle = x87_atan2(vertical, horizontal);
        radius = (float)(x87_sqrt((double)(vertical * vertical + horizontal * horizontal)) * ICON_CENTER_X);
        if (depth <= 1.0f) {
            if (depth < -1.0f) radius = (float)(radius / (-depth * cl_data_audio_camera.projection));
        } else radius = (float)(radius / (depth * cl_data_audio_camera.projection));
        x = (float)(x87_cos(angle) * radius + ICON_CENTER_X);
        y = (float)(ICON_CENTER_Y - x87_sin(angle) * radius);
        if (y < ICON_TOP) y = ICON_TOP;
        if (ICON_BOTTOM < y) y = ICON_BOTTOM;
        color = depth <= ICON_BEHIND ? ICON_REAR_COLOR : 1.0f;
        drawbitmap(
            cl_data_audio_voice_icon, x, y, (float)(size * ICON_SIZE_SCALE), (float)(size * ICON_SIZE_SCALE),
            ICON_CENTER, color, color, color, (float)(amplitude * ICON_ALPHA_SCALE));
    }
}

enum { TEXT_BACK = 0x125d40, TEXT_THRESHOLD = 0x125d48,
       TEXT_PUSH_LABEL = 0x125d58, TEXT_PUSH_KEY = 0x125d6c,
       TEXT_VOICE = 0x125d80, TEXT_SPEAKING = 0x125d8c,
       TEXT_LEVEL = 0x125d90, TEXT_SIZE = 0x125d94,
       TEXT_BIND = 0x125d98, TEXT_EMPTY = 0x127eb5 };

/* Place a menu label in the fixed canvas. */
static void voice_label(unsigned text, float x, float y, float width, float height)
{
    CL_FN(0x5024e0, void (__cdecl *)(char *, float, float, float, float, float, unsigned,
                                  float, float, float, float))(
        (char *)(sr_base + text), x, y, width, height, MENU_FONT, 0, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* Draw the waveform and the two threshold lines. */
static void voice_waveform(void)
{
    int point;
    VoiceRing *ring = &sr_data_speex.voice_ring[0];
    audio_glDisable()(GL_TEXTURE_MODE);
    audio_glBegin()(GL_LINES_MODE);
    audio_glColor3f()(1.0f, 1.0f, 1.0f);
    if (cl_data_audio_voice_enabled) {
        for (point = 0; point < MENU_WAVE_POINTS; point++) {
            const int16_t *samples = &ring->samples[0][0];
            int next = (point + 1) * MENU_WAVE_STRIDE;
            /* The last endpoint reads the low half of the ring head. */
            int16_t endpoint = next == ICON_FRAMES * ICON_FRAME_SAMPLES ? (int16_t)ring->head : samples[next];
            audio_glVertex2f()((float)point,
                (float)(samples[point * MENU_WAVE_STRIDE] / MENU_WAVE_SCALE + MENU_WAVE_Y));
            audio_glVertex2f()((float)(point + 1),
                (float)(endpoint / MENU_WAVE_SCALE + MENU_WAVE_Y));
        }
        audio_glColor3f()(1.0f, 1.0f, 1.0f);
        if (!cl_data_audio_push_to_talk) {
            float low = (float)(MENU_WAVE_Y - (float)cl_data_audio_threshold / MENU_WAVE_SCALE);
            float high = (float)((float)cl_data_audio_threshold / MENU_WAVE_SCALE + MENU_WAVE_Y);
            audio_glVertex2f()(MENU_LEFT, low);
            audio_glVertex2f()(MENU_WAVE_RIGHT, low);
            audio_glVertex2f()(MENU_LEFT, high);
            audio_glVertex2f()(MENU_WAVE_RIGHT, high);
        }
    }
    audio_glEnd()();
    audio_glEnable()(GL_TEXTURE_MODE);
}

/* Store the last newly pressed key or mouse button. Escape ends key selection. */
static void voice_bind_key(void)
{
    int key;
    for (key = 1; key < MENU_KEYS; key++) {
        if (cl_data_audio_key_names[key * MENU_KEY_NAME_BYTES] &&
            cl_data_audio_keys[key] && !cl_data_audio_keys_previous[key]) {
            cl_data_audio_voice_key = key;
            cl_data_audio_bind_key = 0;
        }
    }
    for (key = MENU_MOUSE_FIRST; key < MENU_MOUSE_END; key++) {
        unsigned mask = 1u << key;
        if ((cl_data_audio_mouse_buttons & mask) == mask) {
            cl_data_audio_voice_mouse = mask;
            cl_data_audio_bind_key = 0;
        }
    }
    if (cl_data_audio_keys[MENU_ESCAPE] && !cl_data_audio_keys_previous[MENU_ESCAPE])
        cl_data_audio_bind_key = 0;
}

/* Show voice controls until the window closes or the menu returns. */
void voiceoptionsmenu(void)
{
    int binding = 0, frame;
    resetmenuitems();
    while (!cl_data_audio_menu_exit && !cl_data_audio_menu_back) {
        cl_data_audio_menu_selection = 0;
        voice_label(TEXT_BACK, MENU_MARGIN, MENU_MARGIN, MENU_BACK_WIDTH, MENU_BACK_HEIGHT);
        if (!binding) createmenubutton(MENU_BUTTON, MENU_ESCAPE);
        if (!cl_data_audio_voice_enabled) {
            createmenuitemempty();
        } else if (!cl_data_audio_push_to_talk) {
            createmenuitemempty();
            voice_label(TEXT_THRESHOLD, MENU_LEFT, MENU_CONTROLS_Y, MENU_WIDTH, MENU_HEIGHT);
            createmenubutton(MENU_SLIDER, &cl_data_audio_threshold, 0, MENU_THRESHOLD_MAX, MENU_THRESHOLD_QUANTUM);
            voice_label(TEXT_PUSH_LABEL, MENU_RIGHT, MENU_CONTROLS_Y, MENU_WIDTH, MENU_HEIGHT);
            createmenubutton(MENU_TOGGLE, &cl_data_audio_push_to_talk, 1);
        } else {
            voice_label(TEXT_EMPTY, MENU_LEFT, MENU_CONTROLS_Y, MENU_WIDTH, MENU_HEIGHT);
            voice_label(TEXT_PUSH_KEY, MENU_RIGHT, MENU_CONTROLS_Y, MENU_WIDTH, MENU_HEIGHT);
            createmenubutton(MENU_TOGGLE, &cl_data_audio_push_to_talk, 1);
        }
        voice_label(TEXT_VOICE, MENU_LEFT, MENU_VOICE_Y, MENU_WIDTH, MENU_HEIGHT);
        createmenubutton(MENU_TOGGLE, &cl_data_audio_voice_enabled, 1);
        checksystemmessages();
        checkkeyboard();
        checkmouse();
        checkmenuitems();
        audio_glClearColor()(0.0f, 0.0f, 0.0f, 0.0f);
        audio_glClear()(GL_CLEAR_MASK);
        setuptextdisplay();
        voice_waveform();
        setuptextdisplay();
        drawmenuitems();
        if (cl_data_audio_voice_enabled) {
            if (!sr_data_speex.voice_ring[0].voice_off) {
                drawbitmap(
                    -1, MENU_LEFT, MENU_WAVE_Y, MENU_ACTIVE_SIZE, MENU_ACTIVE_SIZE, ICON_CENTER,
                    MENU_ACTIVE_ALPHA, MENU_ACTIVE_ALPHA, MENU_ACTIVE_ALPHA, MENU_ACTIVE_ALPHA);
                drawtext(
                    (char *)(sr_base + TEXT_SPEAKING), MENU_LEFT, MENU_ACTIVE_TITLE_Y, MENU_FONT,
                    ICON_CENTER, 1.0f, 1.0f, 1.0f, MENU_ACTIVE_ALPHA);
                drawtext(
                    (char *)(sr_base + TEXT_LEVEL), MENU_LEFT, MENU_ACTIVE_DETAIL_Y, MENU_DETAIL_FONT,
                    ICON_CENTER, 1.0f, 1.0f, 1.0f, MENU_ACTIVE_ALPHA);
            }
            if (cl_data_audio_push_to_talk)
                drawtext(
                    cl_data_audio_key_names + cl_data_audio_voice_key * MENU_KEY_NAME_BYTES,
                    MENU_BIND_X, MENU_BIND_Y, MENU_HEIGHT, ICON_CENTER, 1.0f, 1.0f, 1.0f, 1.0f);
        }
        if (cl_data_audio_debug)
            for (frame = 0; frame < ICON_FRAMES; frame++)
                drawtext(
                    (char *)(sr_base + TEXT_SIZE), MENU_MARGIN, (float)frame * MENU_DEBUG_FONT, MENU_DEBUG_FONT, 0,
                    1.0f, 1.0f, 1.0f, 1.0f, sr_data_speex.voice_ring[0].size[frame]);
        if (cl_data_audio_bind_key)
            drawtext(
                (char *)(sr_base + TEXT_BIND), MENU_LEFT, MENU_BIND_HINT_Y, MENU_HEIGHT, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        drawmousecursor(
            cl_data_audio_mouse_x, cl_data_audio_mouse_y, MENU_CURSOR_SIZE, 1.0f, 1.0f, 1.0f, 1.0f);
        audio_SDL_GL_SwapWindow()(cl_data_audio_window);
        if (cl_data_audio_simulation) checksimulation(0);
        binding = cl_data_audio_bind_key != 0;
        if (binding) voice_bind_key();
        soundsimulation(&cl_data_audio_camera.pos, cl_data_audio_camera.rot, &cl_data_audio_camera.vel);
        getaudioinput(0);
    }
    resetmenuitems();
}
