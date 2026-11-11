/* The map of the city: a picture seen from above, drawn into a texture of 512 by 512 pixels. */
#include "render/map.h"

#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"

#define g_textures cl_data_texture
#define g_texturelist sr_data_texturelist

/* Game functions of other files. */
#define setuptexture CL_FN(0x50ac40, void(__cdecl *)(int texture))
#define setuporthoviewport CL_FN(0x506480, void(__cdecl *)(int x, int y, int width, int height, float half_width, float half_height, float depth))
#define setupviewpoint CL_FN(0x5065d0, void(__cdecl *)(const vec3 *position, const vec3 *rotation))
#define render_sectorlist CL_FN(0x4d7ed0, void(__cdecl *)(int mode))
#define clearsectorlist CL_FN(0x4d7850, void(__cdecl *)(int mode))

enum {
    MAP_SIZE = 512,
    SCREEN_WIDTH = 0x400,
    SCREEN_HEIGHT = 0x280,
    GL_RGB8 = 0x8051,
    GL_COLOR_AND_DEPTH = 0x4100,
    GL_LEQUAL = 0x0203,
    GL_CULL_FACE = 0x0b44,
    GL_CW = 0x0900,
    SECTOR_MODE_DEPTH_ONLY = 2
};
#define MAP_CENTER 1152.0f
#define MAP_RANGE 1024.0f
#define QUARTER_TURN 1.5707963705062866

GL_IMPORT(glFrontFace, (uint32_t mode), (mode))
GL_IMPORT(glDepthFunc, (uint32_t function), (function))
GL_IMPORT(glClear, (uint32_t mask), (mask))
GL_IMPORT(glClearColor, (float red, float green, float blue, float alpha), (red, green, blue, alpha))

/* Draw the city from above and copy the picture into the map texture. The camera hangs over
 * the middle of the city and looks straight down. */
void __cdecl rendermap(void)
{
    int map = g_texturelist.map;
    Texture *texture = &g_textures[map];
    vec3 eye[4];

    texture->width = MAP_SIZE;
    texture->height = MAP_SIZE;
    texture->num_levels = 1;
    texture->format = GL_RGB;
    texture->internal_format = GL_RGB8;
    texture->wrap_s = GL_CLAMP_TO_EDGE;
    texture->wrap_t = GL_CLAMP_TO_EDGE;
    texture->mag_filter = GL_LINEAR;
    texture->min_filter = GL_LINEAR;
    os_free(texture->levels[0]);
    texture->levels[0] = 0;
    setuptexture(map);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(1);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_AND_DEPTH);
    eye[0].x = MAP_CENTER;
    eye[0].y = 0.0f;
    eye[0].z = MAP_CENTER;
    resetorientation(&eye[1]);
    rotateorientation(&eye[1], &eye[1], (float)QUARTER_TURN);
    setuporthoviewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, MAP_RANGE, MAP_RANGE, MAP_RANGE);
    setupviewpoint(&eye[0], &eye[1]);
    glViewport(0, 0, MAP_SIZE, MAP_SIZE);
    clearsectorlist(0);
    render_sectorlist(SECTOR_MODE_DEPTH_ONLY);
    glBindTexture(GL_TEXTURE_2D, texture->gl_name);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, MAP_SIZE, MAP_SIZE);
}
