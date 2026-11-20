/* Sector triangles form closed shadow volumes along the light direction.
 * Each volume has two caps and a side for each edge that faces the light.
 * The vertex list uses client arrays or an OpenGL buffer.
 */
#include "render/lists.h"
#include "client.h"
#include "gl_slots.h"
#include "debug/debug.h"
#include "sr_runtime.h"
#include "setup/timers.h"
#include "textures/opengl.h"
#include "vectors.h"

GL_IMPORT(glEnableClientState, (uint32_t array), (array))
GL_IMPORT(glDisableClientState, (uint32_t array), (array))
GL_IMPORT(glDrawArrays, (uint32_t mode, int first, int count), (mode, first, count))

enum { MAX_SECTORS = 8192, ARRAY_POSITION = 0x8074,
       EXT_BUFFERS = 6, SHADOW_BUFFER_BYTES = 0x300000, BUFFER_DYNAMIC = 0x88e8 };


/* Bind the sector vertex buffer on the array target. */
static void bind_shadow_buffer(uint32_t buffer)
{
    cl_glBindBufferARB(GL_ARRAY_BUFFER, buffer);
}

/* Add six ordered corners to the shared volume list. */
static void emit_shadow_face(const uint8_t order[6])
{
    int i;
    for (i = 0; i < 6; ++i)
        cl_data_sector_shadow_vertices[cl_data_sector_shadow_count++] = cl_data_sector_shadow_corners[order[i]];
}

/* Build the list unless light y is negative. A rejected light leaves the timer active. */
void __cdecl render_buildareasectorshadow(void)
{
    static const uint8_t sides[3][6] = {{1,0,3,1,3,4}, {2,1,4,2,4,5}, {0,2,5,0,5,3}};
    static const uint8_t caps[6] = {0,1,2,5,4,3};
    const vec3 *light = (const vec3 *)cl_data_light_position;
    int s, t, edge;
    resettimer(cl_data_timerlist.buildsectorshadow, "buildsectorshadow");
    starttimer(cl_data_timerlist.buildsectorshadow);
    if (light->y < 0.0)
        return;
    cl_data_sector_shadow_count = 0;
    for (s = 0; s < MAX_SECTORS; ++s) {
        BlockSector *sector = &cl_data_block_sectors[s];
        float extrusion;
        if (!sector->used)
            continue;
        extrusion = !(0.125 < light->y) ? 0.0f :
            (float)(-(float)(sector->pos[1] + 1) * 8.0 * sr_data_area[sector->area].block_size / light->y);
        for (t = 0; t < sector->num_tris; ++t) {
            if (!(dotproduct(&sector->tri_normals[t], light) < 0.0))
                continue;
            for (edge = 0; edge < 3; ++edge) {
                const vec3 *vertex = &sector->verts[sector->tris[t][edge]].pos;
                cl_data_sector_shadow_corners[edge] = *vertex;
                scaleaddvectors(&cl_data_sector_shadow_corners[edge + 3], vertex, light, extrusion);
            }
            for (edge = 0; edge < 3; ++edge)
                if (0.0 <= dotproduct(&sector->edge_normals[t][edge], light))
                    emit_shadow_face(sides[edge]);
            emit_shadow_face(caps);
        }
    }
    if (cl_data_glext[EXT_BUFFERS]) {
        bind_shadow_buffer(cl_data_vbo[cl_data_sector_shadow_vbo].gl_name);
        if (!cl_data_sector_shadow_buffer_ready) {
            cl_glBufferDataARB(GL_ARRAY_BUFFER, SHADOW_BUFFER_BYTES,
                cl_data_sector_shadow_vertices, BUFFER_DYNAMIC);
            cl_data_sector_shadow_buffer_ready = 1;
        }
        cl_glBufferSubDataARB(
            GL_ARRAY_BUFFER, 0, cl_data_sector_shadow_count * 12, cl_data_sector_shadow_vertices);
        bind_shadow_buffer(0);
    }
    stoptimer(cl_data_timerlist.buildsectorshadow);
}

/* Draw the sector volume list and report its vertex count. */
void __cdecl render_areasectorshadow(void)
{
    starttimer(cl_data_timerlist.rendersectorshadow);
    glEnableClientState(ARRAY_POSITION);
    if (!cl_data_glext[EXT_BUFFERS])
        glVertexPointer(3, GL_FLOAT, 12, cl_data_sector_shadow_vertices);
    else {
        bind_shadow_buffer(cl_data_vbo[cl_data_sector_shadow_vbo].gl_name);
        glVertexPointer(3, GL_FLOAT, 12, 0);
    }
    glDrawArrays(GL_TRIANGLES, 0, cl_data_sector_shadow_count);
    if (cl_data_glext[EXT_BUFFERS])
        bind_shadow_buffer(0);
    glDisableClientState(ARRAY_POSITION);
    debug_addataint("shadowverts", cl_data_sector_shadow_count);
    stoptimer(cl_data_timerlist.rendersectorshadow);
}
