/* Make every OpenGL object again after the context changes: the shader programs, the frame
 * textures and the textures that have pixels.
 */
#include "render/gl_reload.h"

#include "client.h"
#include "render/frame_textures.h"
#include "render/programs.h"
#include "sr_runtime.h"
#include "textures/upload.h"

#define TEXTURE_COUNT 0x400

/* Rebuild the shader programs and the textures. A texture with a width of 0 is empty and is skipped. */
void __cdecl reloadopengl(void)
{
    int texture;

    setupprogramobjects();
    setupframetextures();
    for (texture = 0; texture < TEXTURE_COUNT; texture++)
        if (cl_data_image_textures[texture].width != 0)
            setuptexture(texture);
}
