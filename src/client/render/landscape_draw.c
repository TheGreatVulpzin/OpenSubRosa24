/* Drawing of the whole landscape: every sector that was built, and the detail level of each
 * sector for the frame. */
#include "render/landscape_draw.h"

#include "client.h"
#include "gl_slots.h"
#include "render/frustum.h"
#include "setup/timers.h"
#include "sr_runtime.h"
#include "textures/landscape.h"
#include "textures/opengl.h"
#include "vectors.h"

#define g_textures cl_data_texture
#define g_texturelist sr_data_texturelist
#define g_timerlist cl_data_timerlist
#define g_programs cl_data_programs
#define g_sector_lod cl_data_landsector_lod
#define g_light_position cl_data_light_position

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
        cl_glUseProgramObjectARB(program->program);
        cl_glUniform3fvARB(
            program->uniforms[UNIFORM_LIGHT_DIRECTION], 1, g_light_position);
        bind_texture(g_texturelist.landscape_normal);
        cl_glActiveTextureARB(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        bind_texture(g_texturelist.landscape_detail);
        cl_glActiveTextureARB(GL_TEXTURE2);
        glEnable(GL_TEXTURE_2D);
        bind_texture(g_texturelist.desert);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    for (sector = 0; sector < NUM_SECTORS; ++sector)
        if (g_sector_lod[sector].level != SECTOR_NOT_BUILT)
            landscape_rendersector(sector);
    cl_glBindBufferARB(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    if (mode == 0) {
        glDisable(GL_TEXTURE_2D);
        cl_glActiveTextureARB(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        cl_glActiveTextureARB(GL_TEXTURE0);
        cl_glUseProgramObjectARB(0);
    }
    stoptimer(g_timerlist.renderlandscape);
}

/* Level 10 is the same value as SECTOR_NOT_BUILT: renderlandscape skips the sector. */
enum { LAND_SECTORS = 32, LOD_HIDDEN = 10, LOD_FAR = 4 };

/* The lower of the two levels. */
static int lower_level(int level, int side)
{
    return side < level ? side : level;
}

/* Choose the detail level of each landscape sector for this frame. A sector outside the view
 * gets level 10. A visible sector gets 0 to 4 by its flat distance to the camera, one level for
 * each 512 units. Each edge level is the lower level of the sector and of its neighbor on that
 * side, so two neighbors meet with the same detail. */
void __cdecl setupframelandscapelod(void)
{
    LandSectorLod *lod = cl_data_landsector_lod;
    const vec3 *origin = (const vec3 *)&sr_data_landscape.origin;
    int row, column;

    for (row = 0; row < LAND_SECTORS; ++row)
        for (column = 0; column < LAND_SECTORS; ++column) {
            LandSectorLod *sector = &lod[row * LAND_SECTORS + column];
            vec3 low, high, center;
            float distance;

            low.x = (float)((float)column * 64.0 * 8.0);
            high.x = (float)(low.x + 512.0);
            low.z = (float)((float)row * 64.0 * 8.0);
            high.z = (float)(low.z + 512.0);
            low.y = 0.0f;
            high.y = 2048.0f;
            addvectors(&low, &low, origin);
            addvectors(&high, &high, origin);
            if (!boxinsidefrustum(&low, &high)) {
                sector->level = LOD_HIDDEN;
                continue;
            }
            addvectors(&center, &low, &high);
            scalevector(&center, &center, 0.5f);
            subtractvectors(&center, &center, &cl_data_camera_position);
            center.y = 0.0f;
            distance = vectorlength(&center);
            /* A NaN distance fails the first test and gives level 0. */
            if (!(512.0f <= distance))
                sector->level = 0;
            else if (!(1024.0f <= distance))
                sector->level = 1;
            else if (!(1536.0f <= distance))
                sector->level = 2;
            else if (!(2048.0f <= distance))
                sector->level = 3;
            else
                sector->level = LOD_FAR;
        }
    /* This pass changes only the edge levels, so the order of the sectors does not matter. */
    for (row = 0; row < LAND_SECTORS; ++row)
        for (column = 0; column < LAND_SECTORS; ++column) {
            LandSectorLod *sector = &lod[row * LAND_SECTORS + column];
            int level = sector->level;

            if (level == LOD_HIDDEN)
                continue;
            sector->edge_level[0] = level;
            sector->edge_level[1] = level;
            sector->edge_level[2] = level;
            sector->edge_level[3] = level;
            if (row > 0)
                sector->edge_level[0] = lower_level(level, sector[-LAND_SECTORS].level);
            if (column < LAND_SECTORS - 1)
                sector->edge_level[1] = lower_level(level, sector[1].level);
            if (row < LAND_SECTORS - 1)
                sector->edge_level[2] = lower_level(level, sector[LAND_SECTORS].level);
            if (column > 0)
                sector->edge_level[3] = lower_level(level, sector[-1].level);
        }
}
