/* The sky pass: one quad over the whole screen that the atmosphere program colors. */
#include "render/atmosphere.h"

#include "client.h"
#include "setup/timers.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"

#define g_textures cl_data_texture
#define g_texturelist sr_data_texturelist
#define g_timerlist cl_data_timerlist
#define g_programs cl_data_programs
#define g_camera cl_data_audio_camera
#define g_light_position cl_data_light_position
#define g_world_position cl_data_world_position
#define g_viewport_width cl_data_viewport_width
#define g_viewport_height cl_data_viewport_height

enum {
    PROGRAM_ATMOSPHERE = 7,
    UNIFORM_WORLD_POSITION = 2,
    UNIFORM_LIGHT_DIRECTION = 4,
    GL_CULL_FACE = 0x0b44,
    GL_CW = 0x0900,
    GL_LEQUAL = 0x0203,
    GL_LIGHT0 = 0x4000,
    GL_POSITION = 0x1203,
    GL_ONE = 1,
    GL_SRC_ALPHA_VALUE = 0x0302,
    GL_TEXTURE1 = 0x84c1
};

#define SLOT_USE_PROGRAM 0x115b18
#define SLOT_UNIFORM_3FV 0x115b48
#define SLOT_MULTI_TEXCOORD_3FV 0x1158c8
#define SKY_DEPTH (-16383.9375f)

GL_IMPORT(glFrontFace, (uint32_t mode), (mode))
GL_IMPORT(glDepthFunc, (uint32_t function), (function))
GL_IMPORT(glVertex3f, (float x, float y, float z), (x, y, z))
GL_IMPORT(glLightfv, (uint32_t light, uint32_t name, const float *value), (light, name, value))

/* Add one corner of the sky quad. The corner looks along a direction of the camera: across by
 * sx, up by sy times the aspect, and straight ahead. */
static void sky_corner(float x, float y, float across, float up)
{
    vec3 direction;

    scalevector(&direction, &g_camera.rot[0], across);
    scaleaddvectors(&direction, &direction, &g_camera.rot[1], up);
    scaleaddvectors(&direction, &direction, &g_camera.rot[2], -1.0f);
    (*(void(__stdcall **)(uint32_t, const float *))(sr_base + SLOT_MULTI_TEXCOORD_3FV))(GL_TEXTURE1,
                                                                                       (const float *)&direction);
    glVertex3f(x, y, SKY_DEPTH);
}

/* Draw the sky. The scale is the height of the view over its width. */
void __cdecl renderframeatmosphere(float scale)
{
    ProgramObject *program = &g_programs[PROGRAM_ATMOSPHERE];

    starttimer(g_timerlist.renderatmosphere);
    glColorMask(1, 1, 1, 0);
    glDepthMask(0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC_ALPHA_VALUE);
    glViewport(0, 0, cl_ftoi(g_viewport_width), cl_ftoi(g_viewport_height));
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, 0.25, 16384.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glLightfv(GL_LIGHT0, GL_POSITION, g_light_position);
    (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_USE_PROGRAM))(program->program);
    (*(void(__stdcall **)(int, int, const float *))(sr_base + SLOT_UNIFORM_3FV))(
        program->uniforms[UNIFORM_WORLD_POSITION], 1, (const float *)&g_world_position);
    (*(void(__stdcall **)(int, int, const float *))(sr_base + SLOT_UNIFORM_3FV))(
        program->uniforms[UNIFORM_LIGHT_DIRECTION], 1, g_light_position);
    glBindTexture(GL_TEXTURE_2D, g_textures[g_texturelist.sky].gl_name);
    glBegin(GL_QUADS);
    sky_corner(0.0f, 1.0f, (float)(g_camera.projection * -1.0), (float)(scale * g_camera.projection));
    sky_corner(1.0f, 1.0f, (float)(g_camera.projection * 1.0), (float)(scale * g_camera.projection));
    sky_corner(1.0f, 0.0f, (float)(g_camera.projection * 1.0), (float)(-scale * g_camera.projection));
    sky_corner(0.0f, 0.0f, (float)(g_camera.projection * -1.0), (float)(-scale * g_camera.projection));
    glEnd();
    (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_USE_PROGRAM))(0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(1);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    stoptimer(g_timerlist.renderatmosphere);
}
