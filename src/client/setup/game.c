/* Small hooks of the game loop of the client. */
#include "client.h"
#include "sr_runtime.h"

void *sr_import(const char *dll, const char *name);

#define g_relative_mouse cl_data_relative_mouse

/* Game functions of other files. */
#define setupframetextures CL_FN(0x4742f0, void(__cdecl *)(void))
#define setupprogramobjects CL_FN(0x4d5690, void(__cdecl *)(void))

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
