/* The start of a new game in the client: the loading screen and the clean up of the sound and the
 * chat. The shared game reset calls this function through sr_resetgame_screen.
 */
#include "setup/reset_screen.h"

#include <math.h>
#include <stdint.h>

#include "audio/imports.h"
#include "audio/sound.h"
#include "client.h"
#include "render/lists.h"
#include "render/map.h"
#include "render/text.h"
#include "render/view.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define CLEAR_COLOR_DEPTH_STENCIL 0x4100
#define LOADING_X 512.0f
#define LOADING_Y 32.0f
#define LOADING_SIZE 16.0f
#define LOADING_ALIGN 1
#define NUM_CHAT_MESSAGES 0x100
#define GAMETYPE_LOBBY 3

GL_IMPORT(glClearColor, (float r, float g, float b, float a), (r, g, b, a))
GL_IMPORT(glClear, (uint32_t mask), (mask))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_GL_SwapWindow, (void *))

/* Set the position of the light from the two start angles: x is sin(a) cos(b), y is cos(a) and
 * z is sin(a) sin(b). The fourth value is 0. */
void resetgame_client_light(void)
{
    double angle_a = (double)sr_game.start_angle_a;
    double angle_b = (double)sr_game.start_angle_b;

    cl_data_light_position[0] = (float)(sin(angle_a) * cos(angle_b));
    cl_data_light_position[1] = (float)cos(angle_a);
    cl_data_light_position[2] = (float)(sin(angle_a) * sin(angle_b));
    cl_data_light_position[3] = 0.0f;
}

/* Draw the map and the shadow volumes of the sectors of the new level. */
void resetgame_client_level_built(void)
{
    rendermap();
    render_buildareasectorshadow();
}

/* Draw the text Generating... on the menu color and show it. Then remove every sound, show the cursor
 * in the lobby game type, and stop the chat messages that are on the screen. */
void resetgame_client_screen(void)
{
    int i;

    glClearColor(sr_data_menucolor[0], sr_data_menucolor[1], sr_data_menucolor[2], 0.0f);
    glClear(CLEAR_COLOR_DEPTH_STENCIL);
    setuptextdisplay();
    drawtext("Generating...", LOADING_X, LOADING_Y, LOADING_SIZE, LOADING_ALIGN, 1.0f, 1.0f, 1.0f, 1.0f);
    audio_SDL_GL_SwapWindow()(cl_data_audio_window);
    for (i = cl_data_audio_sound_count; i >= 0; i--)
        deletesound(i);
    cl_data_input_cursor = sr_game.gametype == GAMETYPE_LOBBY;
    for (i = 0; i < NUM_CHAT_MESSAGES; i++) {
        if (cl_data_chatmessage[i].kind != 0) {
            cl_data_chatmessage[i].delay = 0;
            cl_data_chatmessage[i].timer = 0;
        }
    }
}

/* The client views no human at the start of a game and its death camera counter is zero. The free
 * camera copies its place to the view camera only when the counter is zero. */
void resetgame_client_camera_counter(void)
{
    sr_game.spectator_human = -1;
    sr_game.death_camera_ticks = 0;
}
