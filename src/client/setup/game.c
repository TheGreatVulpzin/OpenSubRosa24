/* Small hooks of the game loop of the client. */
#include "client.h"
#include "sr_runtime.h"
#include "render/frame_textures.h"
#include "render/programs.h"

#include "system_libs.h"

#define g_relative_mouse sr_data_game.relative_mouse

/* The ticket update has no code. */
void __cdecl updateplayticket(void)
{
}

/* Make the frame textures and the shader programs again, for example after the window came
 * back. The relative mouse mode of SDL comes on first when the flag is set. */
void __cdecl reactivategame(void)
{
    if (g_relative_mouse != 0) {
        SDL_SetRelativeMouseMode(1);
    }
    setupframetextures();
    setupprogramobjects();
}
