/* The start of the client: the options, the window, the fonts, the menus and the audio. Then
 * the main menu runs until the player quits, and the client saves the options and stops.
 * The C name is client_main: the test programs have their own main.
 */
#include "setup/main.h"

#include <stdlib.h>

#include "system_libs.h"
#include "client.h"
#include "image/fonts.h"
#include "sr_runtime.h"
#include "audio/setup.h"
#include "menu/draw.h"
#include "menu/front.h"
#include "menu/items.h"
#include "options.h"
#include "profiler.h"
#include "render/lists.h"
#include "setup/byte_order.h"
#include "setup/paths.h"
#include "system.h"
#include "textures/loaders.h"

#define NUM_GL_TEXTURES 0x800
#define FONT_WIDTH 0x400
#define FONT_HEIGHT 0x280
#define FONT_TEXTURE 1000
#define FONT_MAIN 0
#define FONT_DOTS 1
#define FONT_PRINT 2
#define FONT_SIGN 3
#define MOUSE_TEXTURE 0x3f0
#define GL_CLAMP_TO_EDGE 0x812f
#define GL_LINEAR 0x2601
#define SDL_INIT_TIMER_VIDEO 0x21
#define SDL_INIT_JOYSTICK 0x200
#define SDL_GL_RED_SIZE 0
#define SDL_GL_GREEN_SIZE 1
#define SDL_GL_BLUE_SIZE 2
#define SDL_GL_ALPHA_SIZE 3
#define SDL_GL_DOUBLEBUFFER 5
#define SDL_GL_DEPTH_SIZE 6
#define SDL_GL_STENCIL_SIZE 7
#define SDL_WINDOW_OPENGL 2
#define SDL_WINDOW_FULLSCREEN_OPENGL 3
#define WINDOW_PLACE 16
#define WINSOCK_VERSION_1_1 0x101

static int wsock_startup(void)
{
    return WSAStartup(WINSOCK_VERSION_1_1, sr_data_wsadata);
}

static int wsock_cleanup(void)
{
    return WSACleanup();
}

/* Load a font of 1024 by 640 into a font slot. */
static void main_font(int slot, char *path, float size)
{
    cl_data_fontnum = slot;
    cl_data_fonts[slot].cursor = 0;
    cl_data_fonts[slot].resolution_x = FONT_WIDTH;
    cl_data_fonts[slot].resolution_y = FONT_HEIGHT;
    loadfont(slot + FONT_TEXTURE, path, size);
}

/* Make the window from the options: 5 6 5 color bits for a color depth of 16, else 8 of each,
 * and the depth and stencil bits of the options. */
static void main_window(void)
{
    int fullscreen;

    cl_data_render_width = sr_data_config.screenwidth;
    cl_data_render_height = sr_data_config.screenheight;
    cl_data_screen_bits = sr_data_config.colordepth;
    cl_data_render_depth_bits = sr_data_config.depthbits;
    cl_data_render_stencil_bits = sr_data_config.stencilbits;
    cl_data_screen_fullscreen = sr_data_config.fullscreen;
    cl_data_render_aspect = (float)((float)sr_data_config.screenheight / (float)sr_data_config.screenwidth);
    if (cl_data_screen_bits == 16) {
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    } else {
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    }
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, cl_data_render_depth_bits);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, cl_data_render_stencil_bits);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    fullscreen = cl_data_screen_fullscreen != 0;
    cl_data_audio_window = SDL_CreateWindow("Sub Rosa", WINDOW_PLACE, WINDOW_PLACE, cl_data_render_width,
                                                    cl_data_render_height,
                                                    fullscreen ? SDL_WINDOW_FULLSCREEN_OPENGL : SDL_WINDOW_OPENGL);
    cl_data_gl_context = SDL_GL_CreateContext(cl_data_audio_window);
    SDL_GL_SetSwapInterval(sr_data_option.swap_interval);
}

/* SR_DEBUGIT=1 in the environment turns the debug flag on. The client never sets it. The
 * flag opens the building editor and the level editor from the main menu, and it also gives
 * debug money and debug keys. Only for tests. */
static void debug_switch(void)
{
    const char *text = getenv("SR_DEBUGIT");

    if (text != NULL && atoi(text) != 0)
        sr_data_debugit = 1;
}

/* Run the client. The client has no Steam: the Steam flag stays 0. */
int __cdecl client_main(void)
{
    uint32_t systems = SDL_INIT_TIMER_VIDEO;
    int i;

    debug_switch();
    debug_resetlogfile();
    getcpufrequency();
    setuptimers();
    checkbigendian();
    getpaths();
    loadsaveoptions(0);
    cl_data_steam_active = 0;
    if (sr_data_config.joystick != 0)
        systems |= SDL_INIT_JOYSTICK;
    SDL_Init(systems);
    SDL_ShowCursor(0);
    listvideomodes();
    main_window();
    for (i = 0; i < NUM_GL_TEXTURES; i++)
        glGenTextures(1, &cl_data_texture[i].gl_name);
    cl_data_fontlist[1] = FONT_PRINT;
    cl_data_fontlist[0] = FONT_SIGN;
    main_font(FONT_MAIN, "texture/font01.png", 30.0f);
    main_font(FONT_DOTS, "texture/dotmatrixfont.png", 60.0f);
    main_font(cl_data_fontlist[1], "texture/times.png", 60.0f);
    cl_data_fontnum = 0;
    loadtexture(MOUSE_TEXTURE, "texture/mouse01.png", 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR);
    generatemenutextures();
    setupmenuitems();
    setupaudio();
    wsock_startup();
    mainmenu();
    loadsaveoptions(1);
    shutdownaudio();
    wsock_cleanup();
    SDL_Quit();
    return 0;
}
