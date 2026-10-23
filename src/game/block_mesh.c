/* Block files and temporary mesh builders. */
#include "block_mesh.h"

#include <stdio.h>
#include <string.h>

#include "mesh.h"
#include "sr_os.h"
#include "sr_runtime.h"

#define g_blocktemp (*SR_AT(BlockTemp, SR_RVA_BLOCKTEMP))
#define g_area_block SR_AT(AreaBlock, SR_RVA_AREA_BLOCK)
#define g_tempmesh (*SR_AT(TempMesh, SR_RVA_TEMPMESH))
#define g_parser (*SR_AT(Parser, SR_RVA_PARSER))
#define g_level_editor (*SR_AT(LevelEditor, SR_RVA_LEVEL_EDITOR))
#define g_surface SR_AT(vec3, SR_RVA_SURFACETEMP)
#define g_surface_uv SR_AT(float, SR_RVA_SURFACETEMP + 0xc0)
#define g_cubevertex SR_AT(vec3, SR_RVA_CUBEVERTEX)
#define g_cubenormal SR_AT(vec3, 0x60078)
#define g_cubetangent SR_AT(vec3, 0x600c0)
#define g_cubequad ((const int (*)[4])SR_AT(int, SR_RVA_CUBEQUAD))

typedef size_t(__cdecl *block_io_fn)(void *, size_t, size_t, void *);
#define game_fread (*SR_AT(block_io_fn, 0x91022a4))
#define game_fwrite (*SR_AT(block_io_fn, 0x13bf2d00))

static const int cube_sides[6] = {0x30000, 0x60000, 0x10000, 0x50000, 0x20000, 0x40000};
static const int surface_corners[4] = {0, 3, 15, 12};

void loadblock(const char *name)
{
    char path[64];
    int version, i, k;
    sprintf(path, "data/block/%s.sbl", name);
    g_parser.file = os_fopen(path, "rb");
    if (!g_parser.file)
        return;
    game_fread(&version, 4, 1, g_parser.file);
    if ((uint32_t)version - 1u >= 2) {
        os_fclose(g_parser.file);
        return;
    }
    game_fread(&g_blocktemp.size_x, 4, 1, g_parser.file);
    game_fread(&g_blocktemp.size_y, 4, 1, g_parser.file);
    game_fread(&g_blocktemp.size_z, 4, 1, g_parser.file);
    game_fread(&g_blocktemp.floor, 4, 1, g_parser.file);
    game_fread(&g_blocktemp.ceiling, 4, 1, g_parser.file);
    game_fread(&g_blocktemp.wall_z1, 4, 1, g_parser.file);
    game_fread(&g_blocktemp.wall_x0, 4, 1, g_parser.file);
    game_fread(&g_blocktemp.wall_z0, 4, 1, g_parser.file);
    game_fread(&g_blocktemp.wall_x1, 4, 1, g_parser.file);
    game_fread(&g_blocktemp.num_surfaces, 4, 1, g_parser.file);
    /* File counts can extend past the record arrays. */
    for (i = 0; i < g_blocktemp.num_surfaces; i++) {
        BlockSurface *surface = &g_blocktemp.surfaces[i];
        game_fread(&surface->tu, 4, 1, g_parser.file);
        game_fread(&surface->nu, 4, 1, g_parser.file);
        game_fread(&surface->tv, 4, 1, g_parser.file);
        game_fread(&surface->nv, 4, 1, g_parser.file);
        game_fread(&surface->slot, 4, 1, g_parser.file);
        game_fread(&surface->auto_side, 4, 1, g_parser.file);
        for (k = 0; k < 16; k++) {
            game_fread(&surface->ctrl[k], 4, 3, g_parser.file);
            game_fread(surface->uv[k], 4, 2, g_parser.file);
        }
    }
    g_blocktemp.num_boxes = 0;
    if (version > 1) {
        game_fread(&g_blocktemp.num_boxes, 4, 1, g_parser.file);
        for (i = 0; i < g_blocktemp.num_boxes; i++) {
            BlockBox *box = &g_blocktemp.boxes[i];
            for (k = 0; k < 8; k++)
                game_fread(&box->verts[k], 4, 3, g_parser.file);
            for (k = 0; k < 6; k++)
                game_fread(&box->slots[k], 4, 1, g_parser.file);
            game_fread(&box->faces, 4, 1, g_parser.file);
        }
    }
    os_fclose(g_parser.file);
}

static void level_transfer(block_io_fn io)
{
    int i;
    /* The file counts have no array limit. */
    io(&g_level_editor.num_intersections, 4, 1, g_parser.file);
    for (i = 0; i < g_level_editor.num_intersections; i++) {
        LevelIntersection *intersection = &g_level_editor.intersections[i];
        io(&intersection->x, 4, 1, g_parser.file);
        io(&intersection->y, 4, 1, g_parser.file);
        io(&intersection->z, 4, 1, g_parser.file);
    }
    io(&g_level_editor.num_streets, 4, 1, g_parser.file);
    for (i = 0; i < g_level_editor.num_streets; i++) {
        LevelStreet *street = &g_level_editor.streets[i];
        io(&street->a, 4, 1, g_parser.file);
        io(&street->b, 4, 1, g_parser.file);
        io(&street->axis, 4, 1, g_parser.file);
        io(&street->lanes_a, 4, 1, g_parser.file);
        io(&street->lanes_b, 4, 1, g_parser.file);
        io(street->name, 1, 0x20, g_parser.file);
    }
    io(g_level_editor.tiles, 1, 0x100000, g_parser.file);
    io(&g_level_editor.num_buildings, 4, 1, g_parser.file);
    for (i = 0; i < g_level_editor.num_buildings; i++) {
        LevelBuilding *building = &g_level_editor.buildings[i];
        io(building->name, 1, 0x40, g_parser.file);
        io(&building->x, 4, 1, g_parser.file);
        io(&building->y, 4, 1, g_parser.file);
        io(&building->z, 4, 1, g_parser.file);
        io(&building->turns, 4, 1, g_parser.file);
    }
}

void leveleditor_load(void)
{
    int version;
    g_parser.file = os_fopen("data/city.sbc", "rb");
    if (!g_parser.file)
        return;
    game_fread(&version, 4, 1, g_parser.file);
    if (version == 6)
        level_transfer(game_fread);
    os_fclose(g_parser.file);
}

void leveleditor_save(void)
{
    int version = 6;
    g_parser.file = os_fopen("data/city.sbc", "wb");
    if (!g_parser.file)
        return;
    game_fwrite(&version, 4, 1, g_parser.file);
    level_transfer(game_fwrite);
    os_fclose(g_parser.file);
}

static void replace_buffer(int *pointer, size_t size)
{
    os_free((void *)(uintptr_t)*pointer);
    *pointer = (int)(uintptr_t)os_malloc(size);
}

void area_setupsblock(int block)
{
    AreaBlock *b = &g_area_block[block];
    int q, k, i, n = g_tempmesh.num_verts;
    int nq = g_tempmesh.num_quads, nt = g_tempmesh.num_tris;
    for (q = 0; q < nq; q++) {
        int side = g_tempmesh.quad_slots[q] & 0xf0000;
        for (k = 0; k < 4; k++) {
            int v = g_tempmesh.quads[q].verts[k];
            const vec3 *p = &g_tempmesh.verts[v];
            vec3 *uv = &g_tempmesh.uvs[v];
            switch (side) {
            case 0x10000: uv->x = p->z; uv->y = p->y; break;
            case 0x20000: uv->x = (float)(1.0 - p->z); uv->y = p->y; break;
            case 0x30000: uv->x = p->x; uv->y = p->z; break;
            case 0x40000: uv->x = p->x; uv->y = (float)(1.0 - p->z); break;
            case 0x50000: uv->x = (float)(1.0 - p->x); uv->y = p->y; break;
            case 0x60000: uv->x = p->x; uv->y = p->y; break;
            }
        }
    }
    b->num_verts = n;
    replace_buffer(&b->verts, (size_t)n * 12);
    replace_buffer(&b->normals, (size_t)n * 12);
    replace_buffer(&b->tangents, (size_t)n * 12);
    replace_buffer(&b->uvs, (size_t)n * 8);
    memcpy((void *)(uintptr_t)b->verts, g_tempmesh.verts, (size_t)n * 12);
    memcpy((void *)(uintptr_t)b->normals, g_tempmesh.normals, (size_t)n * 12);
    memcpy((void *)(uintptr_t)b->tangents, g_tempmesh.tangents, (size_t)n * 12);
    for (i = 0; i < n; i++)
        memcpy((char *)(uintptr_t)b->uvs + i * 8, &g_tempmesh.uvs[i], 8);
    b->num_faces = nq;
    b->num_tris = nt;
    replace_buffer(&b->faces, (size_t)nq * 16);
    replace_buffer(&b->face_slots, (size_t)nq * 4);
    replace_buffer(&b->tris, (size_t)nt * 12);
    replace_buffer(&b->tri_slots, (size_t)nt * 4);
    memcpy((void *)(uintptr_t)b->faces, g_tempmesh.quads, (size_t)nq * 16);
    memcpy((void *)(uintptr_t)b->face_slots, g_tempmesh.quad_slots, (size_t)nq * 4);
    memcpy((void *)(uintptr_t)b->tris, g_tempmesh.tris, (size_t)nt * 12);
    memcpy((void *)(uintptr_t)b->tri_slots, g_tempmesh.tri_slots, (size_t)nt * 4);
    for (i = 0; i < nq; i++)
        ((uint32_t *)(uintptr_t)b->face_slots)[i] &= 0xffff;
}

static int next_side(int side)
{
    switch (side) {
    case 0x60000: return 0x10000;
    case 0x10000: return 0x50000;
    case 0x50000: return 0x20000;
    case 0x20000: return 0x60000;
    default: return side;
    }
}

int area_block_rotatesurface(int turns, int side)
{
    vec3 *surface = g_surface;
    int turn, i;
    for (turn = 0; turn < turns; turn++) {
        for (i = 0; i < 16; i++) {
            uint32_t x;
            memcpy(&x, &surface[i].x, 4);
            surface[i].x = (float)(1.0 - surface[i].z);
            memcpy(&surface[i].z, &x, 4);
        }
        side = next_side(side);
    }
    return side;
}

static void turn_vector(vec3 *p)
{
    uint32_t x, z;
    memcpy(&x, &p->x, 4);
    memcpy(&z, &p->z, 4);
    z ^= 0x80000000u;
    memcpy(&p->x, &z, 4);
    memcpy(&p->z, &x, 4);
}

void area_copyrotateblock(int block, const int size[3])
{
    AreaBlock *b = &g_area_block[block];
    int i, n;
    b->size_x = size[2];
    b->size_y = size[1];
    b->size_z = size[0];
    n = size[2];
    for (i = 0; i < g_tempmesh.num_verts; i++) {
        uint32_t x;
        memcpy(&x, &g_tempmesh.verts[i].x, 4);
        g_tempmesh.verts[i].x = (float)((double)(float)n - g_tempmesh.verts[i].z);
        memcpy(&g_tempmesh.verts[i].z, &x, 4);
        turn_vector(&g_tempmesh.normals[i]);
        turn_vector(&g_tempmesh.tangents[i]);
    }
    for (i = 0; i < g_tempmesh.num_quads; i++) {
        int side = g_tempmesh.quad_slots[i] & 0xf0000;
        int *q = g_tempmesh.quads[i].verts;
        if (side == 0x30000 || side == 0x40000) {
            int t = q[0];
            q[0] = q[3]; q[3] = q[2]; q[2] = q[1]; q[1] = t;
        }
        if (side == 0x60000 || side == 0x10000 || side == 0x50000 || side == 0x20000) {
            int slot = g_tempmesh.quad_slots[i] & 0xffff;
            if (slot >= 1 && slot <= 3)
                slot++;
            else if (slot == 4)
                slot = 1;
            g_tempmesh.quad_slots[i] = slot | next_side(side);
        }
    }
}

void area_block_addbox(float px, float py, float pz, float sx, float sy, float sz, uint32_t slot)
{
    int face, k;
    for (face = 0; face < 6; face++) {
        int base = g_tempmesh.num_verts;
        int q = g_tempmesh.num_quads;
        for (k = 0; k < 4; k++) {
            const vec3 *c = &g_cubevertex[g_cubequad[face][k]];
            int n = g_tempmesh.num_verts;
            g_tempmesh.verts[n].x = (float)((double)(float)((double)c->x * sx) + px);
            g_tempmesh.verts[n].y = (float)((double)(float)((double)c->y * sy) + py);
            g_tempmesh.verts[n].z = (float)((double)(float)((double)c->z * sz) + pz);
            g_tempmesh.normals[n] = g_cubenormal[face];
            g_tempmesh.tangents[n] = g_cubetangent[face];
            g_tempmesh.num_verts++;
        }
        for (k = 0; k < 4; k++)
            g_tempmesh.quads[q].verts[k] = base + k;
        g_tempmesh.quad_slots[q] = cube_sides[face] | slot;
        g_tempmesh.num_quads++;
    }
}

void area_block_addbeam(int turns, float px, float py, float pz, float sx, float sy, float sz,
                        uint32_t faces, const uint32_t tex[6])
{
    const vec3 *cube_verts = g_cubevertex;
    const int (*cube_faces)[4] = g_cubequad;
    vec3 *surface = g_surface;
    int face, k;
    for (face = 0; face < 6; face++) {
        int side, slot;
        if (!(faces & (1u << face)))
            continue;
        for (k = 0; k < 4; k++) {
            const vec3 *c = &cube_verts[cube_faces[face][k]];
            vec3 *p = &surface[surface_corners[k]];
            p->x = (float)((double)c->x * sx + px);
            p->y = (float)((double)c->y * sy + py);
            p->z = (float)((double)c->z * sz + pz);
        }
        side = area_block_rotatesurface(turns, cube_sides[face]);
        slot = face == 0 ? 5 : face == 5 ? 0 : face;
        addbeziersurface((const float *)g_surface, g_surface_uv, 1, 1, 1, 1, tex[slot] | side);
    }
}
