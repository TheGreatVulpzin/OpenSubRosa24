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
#include "render/sector_model.h"

enum { SECTOR_LIMIT = 8192, MESH_WORDS = 1024, WORD_BITS = 32,
       MESH_SIZE = 16, VERTEX_BYTES = 64, MATERIAL_LIMIT = 256, GLEXT_BUFFERS = 6 };

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
    part->first_index = cl_data_sector_block;
    return part;
}

/* Mark the current block, then select the next free one. */
static void finish_part(BlockSector *sector)
{
    area_setsectormesh(cl_data_sector_block, 1);
    cl_data_sector_block = area_getnextsectormesh();
    ++sector->num_meshes;
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
