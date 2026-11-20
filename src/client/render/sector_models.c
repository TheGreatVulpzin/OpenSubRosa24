/* Sector meshes occupy fixed blocks of sixteen vertices and sixteen triangles.
 * A bitmap records the occupied blocks. Each sector has a material list whose
 * entries select a block and the number of triangles in that block.
 */
#include "render/sector_models.h"

#include <math.h>
#include <string.h>

#include "area.h"
#include "client.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

enum { SECTOR_LIMIT = 8192, MESH_WORDS = 1024, WORD_BITS = 32,
       MESH_SIZE = 16, VERTEX_BYTES = 64, MATERIAL_LIMIT = 256, GLEXT_BUFFERS = 6 };

#define area_buildsectormodel CL_FN(0x44a650, void(__cdecl *)(int))

typedef void(__stdcall *BufferSubData)(uint32_t, int, int, const void *);

/* Return the first free mesh, as an offset in the vertex and triangle arrays. */
int area_getnextsectormesh(void)
{
    int word = 0, bit = 0;
    while (cl_data_sector_mesh_bitmap[word] == UINT32_MAX && word < MESH_WORDS - 1)
        ++word;
    while ((cl_data_sector_mesh_bitmap[word] >> (bit & 31)) & 1u)
        ++bit;
    return (word * WORD_BITS + bit) * MESH_SIZE;
}

/* Set a mesh bit. The last bitmap word is outside the writable range. */
void area_setsectormesh(int first, int used)
{
    int adjusted = first + (first < 0 ? MESH_SIZE - 1 : 0);
    int word = adjusted >> 9;
    int bit = (adjusted >> 4) & 31;
    if (word < MESH_WORDS - 1) {
        uint32_t mask = 1u << bit;
        cl_data_sector_mesh_bitmap[word] &= ~mask;
        cl_data_sector_mesh_bitmap[word] |= (uint32_t)used << bit;
    }
}

/* Release the meshes of a sector, including a sector whose used flag is zero. */
void area_removesectormodel(int sector)
{
    if ((unsigned)sector < SECTOR_LIMIT) {
        BlockSector *block = &cl_data_block_sectors[sector];
        int part;
        for (part = 0; part < block->num_meshes; ++part)
            area_setsectormesh(block->parts[part].first_index, 0);
        block->num_meshes = 0;
    }
}

/* Start an entry even for a material that has no triangles. */
static SectorPart *start_part(BlockSector *sector, int material)
{
    SectorPart *part = &sector->parts[sector->num_meshes];
    part->material = (uint8_t)material;
    part->num_triangles = 0;
    part->first_index = cl_data_sector_mesh_next;
    return part;
}

/* Mark the current block, then select the next free one. */
static void finish_part(BlockSector *sector)
{
    area_setsectormesh(cl_data_sector_mesh_next, 1);
    cl_data_sector_mesh_next = area_getnextsectormesh();
    ++sector->num_meshes;
}

/* Group triangles by material and copy their vertices into fixed mesh blocks. */
void area_submitsectormodel(int sector)
{
    BlockSector *block;
    const SectorShadowMesh *source;
    int material, triangle, corner;
    int (*indices)[3] = (int (*)[3])&cl_data_sector_indices;
    if ((unsigned)sector >= SECTOR_LIMIT || !cl_data_block_sectors[sector].used)
        return;
    block = &cl_data_block_sectors[sector];
    source = (const SectorShadowMesh *)block;
    area_removesectormodel(sector);
    cl_data_sector_mesh_next = area_getnextsectormesh();
    for (material = 0; material < MATERIAL_LIMIT; ++material) {
        int vertices = 0;
        SectorPart *part = start_part(block, material);
        for (triangle = 0; triangle < source->num_triangles; ++triangle) {
            if (source->triangle_material[triangle] != material)
                continue;
            if (vertices + 3 > MESH_SIZE - 1 || part->num_triangles + 1 > MESH_SIZE - 1) {
                finish_part(block);
                vertices = 0;
                part = start_part(block, material);
            }
            for (corner = 0; corner < 3; ++corner) {
                int local, vertex = source->triangles[triangle][corner];
                for (local = 0; local < vertices; ++local)
                    if (cl_data_sector_mesh_remap[local] == vertex)
                        break;
                indices[cl_data_sector_mesh_next + part->num_triangles][corner] = cl_data_sector_mesh_next + local;
                if (local == vertices) {
                    memcpy(cl_data_sector_vertices + (cl_data_sector_mesh_next + local) * VERTEX_BYTES,
                           source->vertices + vertex * VERTEX_BYTES, VERTEX_BYTES);
                    cl_data_sector_mesh_remap[vertices++] = vertex;
                }
            }
            ++part->num_triangles;
        }
        if (part->num_triangles)
            finish_part(block);
    }
    if (cl_data_glext[GLEXT_BUFFERS]) {
        GlBindBufferFn bind = (GlBindBufferFn)cl_data_gl_bind_buffer;
        BufferSubData upload = (BufferSubData)cl_data_gl_buffer_sub_data;
        int part;
        bind(GL_ARRAY_BUFFER, cl_data_vbo[cl_data_sector_vbo].gl_name);
        for (part = 0; part < block->num_meshes; ++part) {
            int first = block->parts[part].first_index;
            upload(GL_ARRAY_BUFFER, first * VERTEX_BYTES, MESH_SIZE * VERTEX_BYTES,
                   cl_data_sector_vertices + first * VERTEX_BYTES);
        }
        bind(GL_ARRAY_BUFFER, 0);
    }
}

/* Build and submit a used sector, then invalidate its four interior models. */
void area_resetsector(int sector)
{
    if ((unsigned)sector < SECTOR_LIMIT && cl_data_block_sectors[sector].used) {
        BlockSector *block = &cl_data_block_sectors[sector];
        int slot;
        area_buildsectormodel(sector);
        area_submitsectormodel(sector);
        for (slot = 0; slot < 4; ++slot)
            block->interior_models[slot] = -1;
        block->dirty = 0;
    }
}

/* Positions agree only when each float difference is less than the tolerance. */
int area_sectorvertequal(int sector, int first, int second)
{
    const SectorShadowMesh *mesh;
    const float *a, *b;
    int axis;
    if ((unsigned)sector >= SECTOR_LIMIT)
        return 0;
    mesh = (const SectorShadowMesh *)&cl_data_block_sectors[sector];
    a = (const float *)(mesh->vertices + first * VERTEX_BYTES);
    b = (const float *)(mesh->vertices + second * VERTEX_BYTES);
    for (axis = 0; axis < 3; ++axis) {
        float distance = (float)(a[axis] - b[axis]);
        if (!(fabsf(distance) < 0.0001f))
            return 0;
    }
    return 1;
}
