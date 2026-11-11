/* Drawing of the whole landscape: every sector that was built. */
#include "render/landscape_draw.h"

#include "client.h"
#include "setup/timers.h"
#include "sr_runtime.h"
#include "textures/landscape.h"
#include "textures/opengl.h"

#define g_textures cl_data_texture
#define g_texturelist sr_data_texturelist
#define g_timerlist sr_data_timerlist
#define g_programs cl_data_programs
#define g_sector_lod cl_data_landsector_lod
#define g_light_position cl_data_light_position

/* Game functions of other files. */
#define stoptimer CL_FN(0x4fb040, void(__cdecl *)(int timer))

enum {
    NUM_SECTORS = 1024,
    SECTOR_NOT_BUILT = 10,
    PROGRAM_LANDSCAPE = 9,
    UNIFORM_LIGHT_DIRECTION = 1,
    GL_VERTEX_ARRAY = 0x8074,
    GL_TEXTURE0 = 0x84c0,
    GL_TEXTURE1 = 0x84c1,
    GL_TEXTURE2 = 0x84c2
};

#define SLOT_ACTIVE_TEXTURE 0x115874
#define SLOT_BIND_BUFFER 0x115aa8
#define SLOT_USE_PROGRAM 0x115b18
#define SLOT_UNIFORM_3FV 0x115b48

GL_IMPORT(glEnableClientState, (uint32_t array), (array))
GL_IMPORT(glDisableClientState, (uint32_t array), (array))

static void bind_texture(int texture)
{
    glBindTexture(GL_TEXTURE_2D, g_textures[texture].gl_name);
}

/* Draw each sector with the landscape program (mode 0) or with no program (any other mode,
 * for the depth and shadow passes). The program mixes the normal map, the detail map and the
 * color map. */
void __cdecl renderlandscape(int mode)
{
    ProgramObject *program = &g_programs[PROGRAM_LANDSCAPE];
    int sector;

    starttimer(g_timerlist.renderlandscape);
    if (mode == 0) {
        (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_USE_PROGRAM))(program->program);
        (*(void(__stdcall **)(int, int, const float *))(sr_base + SLOT_UNIFORM_3FV))(
            program->uniforms[UNIFORM_LIGHT_DIRECTION], 1, g_light_position);
        bind_texture(g_texturelist.landscape_normal);
        (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_ACTIVE_TEXTURE))(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        bind_texture(g_texturelist.landscape_detail);
        (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_ACTIVE_TEXTURE))(GL_TEXTURE2);
        glEnable(GL_TEXTURE_2D);
        bind_texture(g_texturelist.desert);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    for (sector = 0; sector < NUM_SECTORS; ++sector)
        if (g_sector_lod[sector].level != SECTOR_NOT_BUILT)
            landscape_rendersector(sector);
    (*(void(__stdcall **)(uint32_t, uint32_t))(sr_base + SLOT_BIND_BUFFER))(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    if (mode == 0) {
        glDisable(GL_TEXTURE_2D);
        (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_ACTIVE_TEXTURE))(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_ACTIVE_TEXTURE))(GL_TEXTURE0);
        (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_USE_PROGRAM))(0);
    }
    stoptimer(g_timerlist.renderlandscape);
}
