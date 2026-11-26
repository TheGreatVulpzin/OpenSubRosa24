/* Small hooks of the game loop of the client. */
#include "client.h"
#include "sr_runtime.h"
#include "render/frame_textures.h"
#include "render/programs.h"

void *sr_import(const char *dll, const char *name);

#define g_relative_mouse sr_data_game.relative_mouse

/* The ticket update has no code. */
void __cdecl updateplayticket(void)
{
}

/* Make the frame textures and the shader programs again, for example after the window came
 * back. The relative mouse mode of SDL comes on first when the flag is set. */
void __cdecl reactivategame(void)
{
    static int(__cdecl * set_relative_mouse_mode)(int);

    if (g_relative_mouse != 0) {
        if (!set_relative_mouse_mode)
            *(void **)&set_relative_mouse_mode = sr_import("SDL2.DLL", "SDL_SetRelativeMouseMode");
        set_relative_mouse_mode(1);
    }
    setupframetextures();
    setupprogramobjects();
}
