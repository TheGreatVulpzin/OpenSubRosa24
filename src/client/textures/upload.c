/* The upload of a texture to OpenGL: the filter and wrap parameters, then every level of pixels. */
#include "textures/upload.h"

#include "client.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define g_textures cl_data_texture

#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_UNSIGNED_BYTE 0x1401

GL_IMPORT(glTexParameteri, (uint32_t target, uint32_t name, int value), (target, name, value))
GL_IMPORT(glTexImage2D,
          (uint32_t target, int level, int internal_format, int width, int height, int border, uint32_t format,
           uint32_t type, const void *pixels),
          (target, level, internal_format, width, height, border, format, type, pixels))

/* Bind a texture and give OpenGL its parameters and the pixels of each level. A level is half the
 * size of the level before it. */
void __cdecl setuptexture(int texture)
{
    const Texture *record = &g_textures[texture];
    int level;

    glBindTexture(GL_TEXTURE_2D, record->gl_name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, record->wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, record->wrap_t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, record->mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, record->min_filter);
    for (level = 0; level < record->num_levels; ++level)
        glTexImage2D(GL_TEXTURE_2D, level, record->internal_format, record->width >> (level & 0x1f),
                     record->height >> (level & 0x1f), 0, (uint32_t)record->format, GL_UNSIGNED_BYTE,
                     record->levels[level]);
}
