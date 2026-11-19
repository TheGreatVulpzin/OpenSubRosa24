/* The outer mesh of a sector.
 *
 * area_buildsectormodel collects the blocks of a sector in the temporary mesh: a quad for each
 * visible face of a normal block and the mesh of each special block. It then joins the quads,
 * splits them into triangles, removes the vertices that no triangle uses and copies the result
 * into the sector. Each triangle also gets the triangle on the other side of each edge, for the
 * shadow volumes.
 */
#include "render/sector_model.h"

#include <stdint.h>
#include <string.h>

#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/tempmesh.h"
#include "vectors.h"

#define g_tempmesh sr_data_tempmesh
#define g_area sr_data_area
#define g_area_block sr_data_area_block
#define g_buildingtype sr_data_buildingtype
#define g_materiallist sr_data_materiallist
#define g_materials cl_data_material_records
#define g_block_sectors cl_data_block_sectors
#define g_cubevertex sr_data_cubevertex
#define g_cubenormal sr_data_cubenormal
#define g_cubetangent sr_data_cubetangent
#define g_cubebinormal sr_data_cubebinormal
#define g_cubetexcoord sr_data_cubetexcoord
#define g_cubequad sr_data_cubequad

/* Game functions of other files. */
#define area_sectorvertequal CL_FN(0x50b590, int(__cdecl *)(int sector, int a, int b))

#define SECTOR_SIDE 8
#define BLOCK_KIND_MASK 0xe0000000u
#define BLOCK_NORMAL 0x40000000u
#define BLOCK_SPECIAL 0x80000000u
#define BLOCK_FACE_MASK 0x3f
#define BLOCK_TYPE_MASK 0xffff
#define FACES 6
#define FACE_TEXTURES 8
#define TEXTURE_PRESET 0x8000 /* the texture is a preset: bits 8 to 14 give its layer */
#define LAYER_SCALE 1.0000100135803223
#define SIGN_CELL 0.125

/* The place of a block in its sector and the data of its block word. */
typedef struct SectorBlock {
    int x, y, z;
    int type;     /* the building type: bits 0 to 5 of the color word */
    int color[5]; /* color k (1 to 4) of the side faces: 4 bits for each, from bit 6 */
} SectorBlock;

/* The x, y and z of a vertex without the fraction, in bits 0, 10 and 20. */
static int vertex_key(const vec3 *v)
{
    uint32_t key = (uint32_t)cl_ftoi(v->x);

    key |= (uint32_t)cl_ftoi(v->y) << 10;
    key |= (uint32_t)cl_ftoi(v->z) << 20;
    return (int)key;
}

/* The place in the sector of a corner of a block: corner is in blocks. */
static void block_point(vec3 *out, const SectorBlock *b, const vec3 *corner, float size)
{
    out->x = (float)((float)b->x * size + corner->x * size);
    out->y = (float)((float)b->y * size + corner->y * size);
    out->z = (float)((float)b->z * size + corner->z * size);
}

/* Read the 8 face textures of a normal block. A texture word with bit 15 is a preset: the
 * texture is the last preset material and bits 8 to 14 give its layer. Else the low byte is
 * the material, and the high byte is the cell of a sign texture. */
static void normal_textures(const BlockSector *s, int index, uint16_t textures[FACE_TEXTURES],
                            uint16_t extra[FACE_TEXTURES])
{
    int f;

    for (f = 0; f < FACE_TEXTURES; f++) {
        textures[f] = s->textures[index][f];
        extra[f] = 0;
        if ((textures[f] & TEXTURE_PRESET) == 0) {
            if ((uint32_t)(textures[f] & 0xff) == (uint32_t)g_materiallist.sign)
                extra[f] = textures[f] >> 8;
            textures[f] &= 0xff;
        } else {
            extra[f] = (textures[f] >> 8) & 0x7f;
            textures[f] = (uint16_t)g_materiallist.preset_end;
        }
    }
}

/* Add a quad for each visible face of a normal block. */
static void add_normal_block(const BlockSector *s, int index, uint32_t word, const SectorBlock *b,
                             const vec3 *offset, const Area *area)
{
    TempMesh *tm = &g_tempmesh;
    uint16_t textures[FACE_TEXTURES], extra[FACE_TEXTURES];
    float size = area->block_size;
    int f, k;

    normal_textures(s, index, textures, extra);
    for (f = 0; f < FACES; f++) {
        uint32_t tex;
        int q;

        if (((1u << f) & word) == 0)
            continue;
        tex = textures[f];
        q = tm->num_quads;
        for (k = 0; k < 4; k++) {
            int n = tm->num_verts;
            vec3 *v = &tm->verts[n];
            vec3 *uv = &tm->uvs[n];

            block_point(v, b, &g_cubevertex[g_cubequad[f * 4 + k]], size);
            tm->keys[n] = vertex_key(v);
            tm->normals[n] = g_cubenormal[f];
            tm->tangents[n] = g_cubetangent[f];
            uv->x = (float)(dotproduct(v, &g_cubetangent[f]) * g_materials[tex].parameters[0]);
            uv->y = (float)(dotproduct(v, &g_cubebinormal[f]) * g_materials[tex].parameters[1]);
            uv->z = 0.0f;
            addvectors(v, v, offset);
            addvectors(v, v, &area->origin);
            if (f < 1 || f > 4) {
                tm->colors[n].x = 1.0f;
                tm->colors[n].y = 1.0f;
                tm->colors[n].z = 1.0f;
            } else {
                tm->colors[n] = g_buildingtype.parts[b->type].colors[b->color[f]];
                if (tex == (uint32_t)g_materiallist.preset_end) {
                    uv->z = (float)((float)(extra[f] >> 4) * LAYER_SCALE);
                    uv->z = (float)((float)(extra[f] & 0xf) * 0.0625 + uv->z);
                }
            }
            if (tex == (uint32_t)g_materiallist.sign) {
                float cell_u = (float)((float)(extra[f] & 0xf) * SIGN_CELL);
                float cell_v = (float)((float)(extra[f] >> 4) * SIGN_CELL);

                uv->x = (float)(g_cubetexcoord[k * 2] * SIGN_CELL + cell_u);
                uv->y = (float)(g_cubetexcoord[k * 2 + 1] * SIGN_CELL + cell_v);
            }
            tm->quads[q].verts[k] = n;
            tm->num_verts++;
        }
        tm->quad_slots[q] = (int)tex;
        tm->quad_normals[q] = g_cubenormal[f];
        tm->num_quads++;
    }
}

/* A special block can fill more than one cell: each cell has the same block word. The result is
 * 1 when the block at this place in the area is in the mesh already, else the place goes into
 * the list. */
static int special_seen(const BlockSector *s, const SectorBlock *b)
{
    TempMesh *tm = &g_tempmesh;
    int x = b->x + s->pos[0] * SECTOR_SIDE, y = b->y + s->pos[1] * SECTOR_SIDE;
    int z = b->z + s->pos[2] * SECTOR_SIDE, i;

    for (i = 0; i < tm->num_specials; i++)
        if (x == tm->specials[i * 3] && y == tm->specials[i * 3 + 1] && z == tm->specials[i * 3 + 2])
            return 1;
    tm->specials[tm->num_specials * 3] = x;
    tm->specials[tm->num_specials * 3 + 1] = y;
    tm->specials[tm->num_specials * 3 + 2] = z;
    tm->num_specials++;
    return 0;
}

/* Add the mesh of a special block. The texture slot of each face and triangle selects one of
 * the 8 face textures of the block. Faces with slot 1 to 4 take the color of that side. The
 * normals of the loose triangles stay as they are. */
static void add_special_block(const BlockSector *s, int index, uint32_t word, const SectorBlock *b,
                              const vec3 *offset, const Area *area)
{
    TempMesh *tm = &g_tempmesh;
    const AreaBlock *block = &g_area_block[word & BLOCK_TYPE_MASK];
    const vec3 *verts = (const vec3 *)(uintptr_t)block->verts;
    const vec3 *normals = (const vec3 *)(uintptr_t)block->normals;
    const vec3 *tangents = (const vec3 *)(uintptr_t)block->tangents;
    const float *uvs = (const float *)(uintptr_t)block->uvs;
    const int (*faces)[4] = (const int (*)[4])(uintptr_t)block->faces;
    const int *face_slots = (const int *)(uintptr_t)block->face_slots;
    const int (*tris)[3] = (const int (*)[3])(uintptr_t)block->tris;
    const int *tri_slots = (const int *)(uintptr_t)block->tri_slots;
    uint16_t textures[FACE_TEXTURES];
    float size = area->block_size;
    int first = tm->num_verts, i, k;

    for (i = 0; i < FACE_TEXTURES; i++)
        textures[i] = s->textures[index][i];
    for (i = 0; i < block->num_verts; i++) {
        int n = tm->num_verts;
        vec3 *v = &tm->verts[n];

        block_point(v, b, &verts[i], size);
        tm->keys[n] = vertex_key(v);
        addvectors(v, v, offset);
        addvectors(v, v, &area->origin);
        tm->normals[n] = normals[i];
        tm->tangents[n] = tangents[i];
        tm->colors[n].x = 1.0f;
        tm->colors[n].y = 1.0f;
        tm->colors[n].z = 1.0f;
        tm->uvs[n].x = uvs[i * 2];
        tm->uvs[n].y = uvs[i * 2 + 1];
        tm->uvs[n].z = 0.0f;
        tm->num_verts++;
    }
    for (i = 0; i < block->num_faces; i++) {
        int q = tm->num_quads, slot = face_slots[i];
        int *quad = tm->quads[q].verts;

        for (k = 0; k < 4; k++)
            quad[k] = first + faces[i][k];
        tm->quad_slots[q] = textures[slot];
        if (slot > 0 && slot < 5)
            for (k = 0; k < 4; k++)
                tm->colors[quad[k]] = g_buildingtype.parts[b->type].colors[b->color[slot]];
        generatetrinormal(&tm->quad_normals[q], &tm->verts[quad[0]], &tm->verts[quad[1]], &tm->verts[quad[2]]);
        tm->num_quads++;
    }
    for (i = 0; i < block->num_tris; i++) {
        int t = tm->num_tris;

        for (k = 0; k < 3; k++)
            tm->tris[t].verts[k] = first + tris[i][k];
        tm->tri_slots[t] = textures[tri_slots[i]];
        tm->num_tris++;
    }
}

/* Add triangle a, b, c of quad q. Its normal is the normal of the first quad vertex. */
static void quad_tri(int q, int a, int b, int c)
{
    TempMesh *tm = &g_tempmesh;
    const int *quad = tm->quads[q].verts;
    int t = tm->num_tris;

    tm->tris[t].verts[0] = quad[a];
    tm->tris[t].verts[1] = quad[b];
    tm->tris[t].verts[2] = quad[c];
    tm->tri_normals[t] = tm->normals[quad[0]];
    tm->tri_slots[t] = tm->quad_slots[q];
    tm->num_tris++;
}

static int vertex_used(int v)
{
    const TempMesh *tm = &g_tempmesh;
    int t;

    for (t = 0; t < tm->num_tris; t++)
        if (tm->tris[t].verts[0] == v || tm->tris[t].verts[1] == v || tm->tris[t].verts[2] == v)
            return 1;
    return 0;
}

/* Remove each vertex that no triangle uses: the last vertex moves into its place. */
static void remove_unused_verts(void)
{
    TempMesh *tm = &g_tempmesh;
    int v = 0, t, k;

    while (v < tm->num_verts) {
        int last;

        if (vertex_used(v)) {
            v++;
            continue;
        }
        last = --tm->num_verts;
        if (v >= last)
            continue;
        tm->verts[v] = tm->verts[last];
        tm->normals[v] = tm->normals[last];
        tm->tangents[v] = tm->tangents[last];
        tm->uvs[v] = tm->uvs[last];
        tm->colors[v] = tm->colors[last];
        for (t = 0; t < tm->num_tris; t++)
            for (k = 0; k < 3; k++)
                if (tm->tris[t].verts[k] == last)
                    tm->tris[t].verts[k] = v;
    }
}

/* Free the old buffer of a sector and give a new one of size bytes. */
static void *replace_buffer(void *old, size_t size)
{
    os_free(old);
    return os_malloc(size);
}

/* Copy the temporary mesh into the sector. */
static void store_mesh(BlockSector *s)
{
    const TempMesh *tm = &g_tempmesh;
    int i, n = tm->num_tris;

    s->num_verts = tm->num_verts;
    s->verts = replace_buffer(s->verts, (size_t)tm->num_verts * sizeof(SectorVertex));
    for (i = 0; i < tm->num_verts; i++) {
        SectorVertex *v = &s->verts[i];

        memcpy(&v->pos, &tm->verts[i], sizeof(vec3));
        memcpy(&v->normal, &tm->normals[i], sizeof(vec3));
        memcpy(&v->tangent, &tm->tangents[i], sizeof(vec3));
        memcpy(&v->uv, &tm->uvs[i], sizeof(vec3));
        memcpy(&v->color, &tm->colors[i], sizeof(vec3));
    }
    s->num_tris = n;
    s->tris = replace_buffer(s->tris, (size_t)n * 12);
    s->tri_slots = replace_buffer(s->tri_slots, (size_t)n * 4);
    s->tri_normals = replace_buffer(s->tri_normals, (size_t)n * 12);
    s->neighbors = replace_buffer(s->neighbors, (size_t)n * 12);
    s->edge_normals = replace_buffer(s->edge_normals, (size_t)n * 36);
    memcpy(s->tris, tm->tris, (size_t)n * 12);
    memcpy(s->tri_slots, tm->tri_slots, (size_t)n * 4);
    memcpy(s->tri_normals, tm->tri_normals, (size_t)n * 12);
}

/* For each edge of each triangle, find the triangle that has the same edge in the other
 * direction. Each edge gets the first free match. */
static void find_neighbors(int sector, BlockSector *s)
{
    int i, j, a, b;

    for (i = 0; i < s->num_tris; i++)
        for (a = 0; a < 3; a++) {
            s->neighbors[i][a] = -1;
            memset(&s->edge_normals[i][a], 0, sizeof(vec3));
        }
    for (i = 0; i < s->num_tris; i++)
        for (j = 0; j < s->num_tris; j++) {
            if (i == j)
                continue;
            for (a = 0; a < 3; a++)
                for (b = 0; b < 3; b++)
                    if (s->neighbors[i][a] == -1 && s->neighbors[j][b] == -1 &&
                        area_sectorvertequal(sector, s->tris[i][a], s->tris[j][(b + 1) % 3]) &&
                        area_sectorvertequal(sector, s->tris[i][(a + 1) % 3], s->tris[j][b])) {
                        s->neighbors[i][a] = j;
                        s->edge_normals[i][a] = s->tri_normals[j];
                        s->neighbors[j][b] = i;
                        s->edge_normals[j][b] = s->tri_normals[i];
                    }
        }
}

/* Build the outer mesh of a sector. */
void __cdecl area_buildsectormodel(int sector)
{
    TempMesh *tm = &g_tempmesh;
    BlockSector *s;
    const Area *area;
    SectorBlock b;
    vec3 offset;
    int q;

    if (sector < 0 || sector >= 0x2000 || !g_block_sectors[sector].used)
        return;
    s = &g_block_sectors[sector];
    area = &g_area[s->area];
    offset.x = (float)((float)s->pos[0] * 8.0 * area->block_size);
    offset.y = (float)((float)s->pos[1] * 8.0 * area->block_size);
    offset.z = (float)((float)s->pos[2] * 8.0 * area->block_size);
    tm->num_specials = 0;
    tm->num_verts = 0;
    tm->num_quads = 0;
    tm->num_tris = 0;
    memset(&b, 0, sizeof b);
    for (b.y = 0; b.y < SECTOR_SIDE; b.y++)
        for (b.z = 0; b.z < SECTOR_SIDE; b.z++)
            for (b.x = 0; b.x < SECTOR_SIDE; b.x++) {
                int index = b.y * 64 + b.x + b.z * 8;
                uint32_t word = s->blocks[index];

                if (word != 0) {
                    uint32_t colors = s->colors[index];

                    b.type = colors & 0x3f;
                    b.color[1] = colors >> 6 & 0xf;
                    b.color[2] = colors >> 10 & 0xf;
                    b.color[3] = colors >> 14 & 0xf;
                    b.color[4] = colors >> 18 & 0xf;
                }
                if ((word & BLOCK_KIND_MASK) == BLOCK_NORMAL)
                    add_normal_block(s, index, word & BLOCK_FACE_MASK, &b, &offset, area);
                else if ((word & BLOCK_KIND_MASK) == BLOCK_SPECIAL && !special_seen(s, &b))
                    add_special_block(s, index, word, &b, &offset, area);
            }
    tempmesh_optimize();
    for (q = 0; q < tm->num_quads; q++) {
        quad_tri(q, 0, 1, 2);
        quad_tri(q, 0, 2, 3);
    }
    remove_unused_verts();
    store_mesh(s);
    find_neighbors(sector, s);
}
