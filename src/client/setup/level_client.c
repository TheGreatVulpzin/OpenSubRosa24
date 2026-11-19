/* The parts of the level build that only the client has: the landscape mesh at the start, and
 * at the end the window texture, the buffer of the sector vertices, the meshes of the sectors and
 * the track model. The level builder calls these functions through sr_level_after_landscape and
 * sr_level_after_buildings.
 */
#include "setup/level_client.h"

#include "client.h"
#include "render/track_model.h"
#include "sr_runtime.h"
#include "textures/landscape.h"
#include "textures/opengl.h"
#include "textures/textures.h"

#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88e8
#define GL_CLAMP_TO_EDGE 0x812f
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GLEXT_VERTEX_BUFFER 6
#define SECTOR_BUFFER_SIZE 0x2000000
#define NUM_SECTORS 0x2000

/* Game functions of other files. */
#define area_resetsector CL_FN(0x439370, void(__cdecl *)(int sector))

/* Make the mesh of the landscape and clear the list of window cells. */
void level_client_landscape(void)
{
    landscape_buildmesh();
    cl_data_texture_windowatlas.num_cells = 0;
}

/* Make the window texture of the buildings, empty the buffer of the sector vertices when the
 * machine has buffers, make the mesh of each sector in use that changed, and make the track
 * model. */
void level_client_finish(void)
{
    int sector;

    generatematerialwindowtexture(sr_data_materiallist.preset_end, 1, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_LINEAR,
                                  GL_LINEAR_MIPMAP_LINEAR);
    if (cl_data_glext[GLEXT_VERTEX_BUFFER] != 0) {
        ((GlBindBufferFn)cl_data_gl_bind_buffer)(GL_ARRAY_BUFFER, cl_data_vbo[cl_data_sector_vbo].gl_name);
        ((GlBufferDataFn)cl_data_gl_buffer_data)(GL_ARRAY_BUFFER, SECTOR_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);
        ((GlBindBufferFn)cl_data_gl_bind_buffer)(GL_ARRAY_BUFFER, 0);
    }
    for (sector = 0; sector < NUM_SECTORS; sector++)
        if (sr_data_area_sector.records[sector].used != 0 && sr_data_area_sector.records[sector].dirty != 0)
            area_resetsector(sector);
    buildtrackmodel();
}
