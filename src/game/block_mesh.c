/* Block files and temporary mesh builders. */
#include "block_mesh.h"

#include <stdio.h>
#include <string.h>

#include "files.h"
#include "mesh.h"
#include "vectors.h"
#include "sr_os.h"
#include "sr_runtime.h"

#define g_blocktemp sr_data_blocktemp
#define g_area_block sr_data_area_block
#define g_tempmesh sr_data_tempmesh
#define g_parser sr_data_parser
#define g_level_editor sr_data_level_editor
#define g_buildblocktype sr_data_buildblocktype
#define g_buildingtype sr_data_buildingtype
#define g_blocklist sr_data_blocklist
#define g_filelist sr_data_filelist
#define g_surface ((vec3 *)&sr_data_surface.points)
#define g_surface_uv ((float *)&sr_data_surface.uv)
#define g_cubevertex sr_data_cubevertex
#define g_cubenormal sr_data_cubenormal
#define g_cubetangent sr_data_cubetangent
#define g_cubequad ((const int (*)[4])sr_data_cubequad)

typedef size_t(__cdecl *block_io_fn)(void *, size_t, size_t, void *);
#define game_fread (*((block_io_fn *)&sr_data_file_read))
#define game_fwrite (*((block_io_fn *)&sr_data_file_write))

static const int cube_sides[6] = {0x30000, 0x60000, 0x10000, 0x50000, 0x20000, 0x40000};
static const int surface_corners[4] = {0, 3, 15, 12};

void loadblock(const char *name)
{
    static BlockTemp pending;
    char path[64];
    BlockTemp *block = &pending;
    int version, i, k, length;
    if (strnlen(name, 64) == 64 || strpbrk(name, "/\\") || strstr(name, ".."))
        return;
    length = snprintf(path, sizeof path, "data/block/%s.sbl", name);
    if (length < 0 || length >= sizeof path)
        return;
    g_parser.file = os_fopen(path, "rb");
    if (!g_parser.file)
        return;
    if (game_fread(&version, 4, 1, g_parser.file) != 1 || (uint32_t)version - 1u >= 2) {
        os_fclose(g_parser.file);
        return;
    }
    /* Keep the current block until all file checks pass. */
#define READ_BLOCK(pointer, size, count) \
    do { if (game_fread(pointer, size, count, g_parser.file) != (size_t)(count)) goto reject; } while (0)
    READ_BLOCK(&block->size_x, 4, 1);
    READ_BLOCK(&block->size_y, 4, 1);
    READ_BLOCK(&block->size_z, 4, 1);
    READ_BLOCK(&block->floor, 4, 1);
    READ_BLOCK(&block->ceiling, 4, 1);
    READ_BLOCK(&block->wall_z1, 4, 1);
    READ_BLOCK(&block->wall_x0, 4, 1);
    READ_BLOCK(&block->wall_z0, 4, 1);
    READ_BLOCK(&block->wall_x1, 4, 1);
    READ_BLOCK(&block->num_surfaces, 4, 1);
    if ((unsigned)block->num_surfaces > 1024)
        goto reject;
    for (i = 0; i < block->num_surfaces; i++) {
        BlockSurface *surface = &block->surfaces[i];
        READ_BLOCK(&surface->tu, 4, 1);
        READ_BLOCK(&surface->nu, 4, 1);
        READ_BLOCK(&surface->tv, 4, 1);
        READ_BLOCK(&surface->nv, 4, 1);
        READ_BLOCK(&surface->slot, 4, 1);
        READ_BLOCK(&surface->auto_side, 4, 1);
        for (k = 0; k < 16; k++) {
            float point[5];
            READ_BLOCK(point, 4, 5);
            memcpy(&surface->ctrl[k], point, sizeof surface->ctrl[k]);
            memcpy(surface->uv[k], point + 3, sizeof surface->uv[k]);
        }
    }
    block->num_boxes = 0;
    if (version > 1) {
        READ_BLOCK(&block->num_boxes, 4, 1);
        if ((unsigned)block->num_boxes > 1024)
            goto reject;
        for (i = 0; i < block->num_boxes; i++) {
            BlockBox *box = &block->boxes[i];
            READ_BLOCK(box->verts, 4, 24);
            READ_BLOCK(box->slots, 4, 6);
            READ_BLOCK(&box->faces, 4, 1);
        }
    }
    /* Keep fields that the file does not contain. */
    memcpy(&g_blocktemp, block, offsetof(BlockTemp, surfaces));
    for (i = 0; i < block->num_surfaces; i++)
        memcpy(&g_blocktemp.surfaces[i].nu, &block->surfaces[i].nu,
               sizeof(BlockSurface) - offsetof(BlockSurface, nu));
    g_blocktemp.num_boxes = block->num_boxes;
    memcpy(g_blocktemp.boxes, block->boxes, block->num_boxes * sizeof *block->boxes);
reject:
    os_fclose(g_parser.file);
#undef READ_BLOCK
}

static int level_transfer(block_io_fn io, LevelEditor *level)
{
    int i;
#define TRANSFER(pointer, size, count) \
    do { if (io(pointer, size, count, g_parser.file) != (size_t)(count)) return 0; } while (0)
    TRANSFER(&level->num_intersections, 4, 1);
    if ((unsigned)level->num_intersections > 1024)
        return 0;
    if (level->num_intersections != 0)
        TRANSFER(level->intersections, 4, level->num_intersections * 3);
    TRANSFER(&level->num_streets, 4, 1);
    if ((unsigned)level->num_streets > 512)
        return 0;
    for (i = 0; i < level->num_streets; i++) {
        LevelStreet *street = &level->streets[i];
        TRANSFER(&street->a, 4, 5);
        TRANSFER(street->name, 1, 0x20);
    }
    TRANSFER(level->tiles, 1, 0x100000);
    TRANSFER(&level->num_buildings, 4, 1);
    if ((unsigned)level->num_buildings > 1024)
        return 0;
    for (i = 0; i < level->num_buildings; i++) {
        LevelBuilding *building = &level->buildings[i];
        TRANSFER(building->name, 1, 0x40);
        TRANSFER(&building->x, 4, 4);
    }
    return 1;
#undef TRANSFER
}

static int level_read_count(const uint8_t *data, size_t size, size_t *position, int limit, int *value)
{
    if (*position > size || size - *position < sizeof *value)
        return 0;
    memcpy(value, data + *position, sizeof *value);
    *position += sizeof *value;
    return (unsigned)*value <= (unsigned)limit;
}

void leveleditor_load(void)
{
    static uint8_t pending[3 * sizeof(int) + 1024 * sizeof(LevelIntersection) +
        512 * sizeof(LevelStreet) + 0x100000 + 1024 * offsetof(LevelBuilding, unsaved)];
    int version, i, num_intersections, num_streets, num_buildings;
    size_t size, position = 0, bytes, intersections, streets, tiles, buildings;
    const size_t building_stride = offsetof(LevelBuilding, unsaved);
    g_parser.file = os_fopen("data/city.sbc", "rb");
    if (!g_parser.file)
        return;
    if (game_fread(&version, 4, 1, g_parser.file) == 1 && version == 6) {
        /* Read once. Check all byte ranges before the editor receives data. */
        size = game_fread(pending, 1, sizeof pending, g_parser.file);
        if (os_fgetc(g_parser.file) != EOF)
            goto close;
        if (!level_read_count(pending, size, &position, 1024, &num_intersections))
            goto close;
        intersections = position;
        bytes = (size_t)num_intersections * sizeof(LevelIntersection);
        if (bytes > size - position)
            goto close;
        position += bytes;
        if (!level_read_count(pending, size, &position, 512, &num_streets))
            goto close;
        streets = position;
        bytes = (size_t)num_streets * sizeof(LevelStreet);
        if (bytes > size - position)
            goto close;
        position += bytes;
        tiles = position;
        if (sizeof g_level_editor.tiles > size - position)
            goto close;
        position += sizeof g_level_editor.tiles;
        if (!level_read_count(pending, size, &position, 1024, &num_buildings))
            goto close;
        buildings = position;
        if ((size_t)num_buildings * building_stride != size - position)
            goto close;
        g_level_editor.num_intersections = num_intersections;
        memcpy(g_level_editor.intersections, pending + intersections,
               num_intersections * sizeof(LevelIntersection));
        g_level_editor.num_streets = num_streets;
        memcpy(g_level_editor.streets, pending + streets, num_streets * sizeof(LevelStreet));
        memcpy(g_level_editor.tiles, pending + tiles, sizeof g_level_editor.tiles);
        g_level_editor.num_buildings = num_buildings;
        for (i = 0; i < num_buildings; i++)
            memcpy(&g_level_editor.buildings[i], pending + buildings + i * building_stride, building_stride);
    }
close:
    os_fclose(g_parser.file);
}

void leveleditor_save(void)
{
    int version = 6;
    g_parser.file = os_fopen("data/city.sbc", "wb");
    if (!g_parser.file)
        return;
    game_fwrite(&version, 4, 1, g_parser.file);
    level_transfer(game_fwrite, &g_level_editor);
    os_fclose(g_parser.file);
}

static void replace_buffer(int *pointer, size_t size)
{
    os_free((void *)(uintptr_t)*pointer);
    *pointer = (int)(uintptr_t)os_malloc(size);
}

static void tempmesh_uvs(int clear_side)
{
    int q, k, nq = g_tempmesh.num_quads;
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
        if (clear_side && side != 0)
            g_tempmesh.quad_slots[q] &= 0xffff;
    }
}

void area_setupsblock(int block)
{
    AreaBlock *b = &g_area_block[block];
    int i, n = g_tempmesh.num_verts;
    int nq = g_tempmesh.num_quads, nt = g_tempmesh.num_tris;
    tempmesh_uvs(0);
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

/* Inner control points keep their previous values. */
#define surface_set(ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz) do { \
    vec3 *corners = g_surface; \
    corners[0].x = (ax); corners[0].y = (ay); corners[0].z = (az); \
    corners[3].x = (bx); corners[3].y = (by); corners[3].z = (bz); \
    corners[15].x = (cx); corners[15].y = (cy); corners[15].z = (cz); \
    corners[12].x = (dx); corners[12].y = (dy); corners[12].z = (dz); \
} while (0)

static __forceinline void surface_add(int turns, int side, uint32_t slot)
{
    int rotated = area_block_rotatesurface(turns, side);
    addbeziersurface((const float *)g_surface, g_surface_uv, 1, 1, 1, 1, slot | rotated);
}

void area_block_addbasicsurface(uint32_t face, const vec3 *pos)
{
    vec3 *surface = g_surface;
    int side, i;
    surface[0].x = 0;
    if (face == 0) {
        surface_set(0, .0625f, 0, 1, .0625f, 0, 1, .0625f, 1, 0, .0625f, 1);
        side = 0x40000;
    } else if (face == 5) {
        surface_set(0, .875f, 1, 1, .875f, 1, 1, .875f, 0, 0, .875f, 0);
        side = 0x30000;
    } else {
        surface_set(0, 1, .0625f, 1, 1, .0625f, 1, 0, .0625f, 0, 0, .0625f);
        side = area_block_rotatesurface((int)(face - 1u), 0x60000);
    }
    for (i = 0; i < 4; i++) {
        vec3 *p = &surface[surface_corners[i]];
        p->x = (float)((double)pos->x + p->x);
        p->y = (float)((double)pos->y + p->y);
        p->z = (float)((double)pos->z + p->z);
    }
    addbeziersurface((const float *)surface, g_surface_uv, 1, 1, 1, 1, side | face);
}

int area_block_addwindow(int turns, float x, float width, float y, float height, int block, uint32_t slot)
{
    float x1 = (float)((double)x + width);
    float y1 = (float)((double)y + height);
    AreaBlock *b = &g_area_block[block];
    surface_set(x1, y1, .03125f, x, y1, .03125f, x, y, .03125f, x1, y, .03125f);
    surface_add(turns, 0x50000, slot);
    surface_set(x, y1, .03125f, x1, y1, .03125f, x1, y, .03125f, x, y, .03125f);
    surface_add(turns, 0x60000, slot);
    b->num_hideable += 2;
    /* The return value is the address of the counter. */
    return (int)(uintptr_t)&b->num_hideable;
}

void area_block_adddoor(int turns, float width, float height)
{
    uint32_t slot = (uint32_t)turns + 1u;
    float x2;
    surface_set(0, 1, .0625f, width, 1, .0625f, width, 0, .0625f, 0, 0, .0625f);
    surface_add(turns, 0x60000, slot);
    surface_set(width, 1, .0625f, width, 1, 0, width, 0, 0, width, 0, .0625f);
    surface_add(turns, 0x20000, (((uint32_t)turns - 1u) & 3u) + 1u);
    x2 = (float)(1.0 - width);
    surface_set(x2, 1, .0625f, 1, 1, .0625f, 1, 0, .0625f, x2, 0, .0625f);
    surface_add(turns, 0x60000, slot);
    surface_set(x2, 1, 0, x2, 1, .0625f, x2, 0, .0625f, x2, 0, 0);
    surface_add(turns, 0x10000, (((uint32_t)turns + 1u) & 3u) + 1u);
    surface_set(0, 1, .0625f, 1, 1, .0625f, 1, height, .0625f, 0, height, .0625f);
    surface_add(turns, 0x60000, slot);
    surface_set(0, height, .0625f, 1, height, .0625f, 1, height, 0, 0, height, 0);
    surface_add(turns, 0x30000, slot);
}

void area_block_addoutdoor(int turns, float unused, float height)
{
    uint32_t tex[6] = {0, 1, 2, (((uint32_t)turns - 2u) & 3u) + 9u, 4, 5};
    float top;
    (void)unused;
    area_block_addbeam(turns, 0, 0, 0, .3125f, 1, .0625f, 0x1a, tex);
    area_block_addbeam(turns, .6875f, 0, 0, .3125f, 1, .0625f, 0xe, tex);
    top = (float)(1.0 - height);
    area_block_addbeam(turns, 0, height, 0, 1, top, .0625f, 0xb, tex);
    area_block_addbeam(turns, 0, 0, 0, 1, .0625f, .0625f, 8, tex);
}

void area_block_addcorner(int turns, float x, float z)
{
    surface_set(0, 1, z, x, 1, z, x, 0, z, 0, 0, z);
    surface_add(turns, 0x60000, (uint32_t)turns + 1u);
    surface_set(x, 1, z, x, 1, 0, x, 0, 0, x, 0, z);
    surface_add(turns, 0x20000, (((uint32_t)turns - 1u) & 3u) + 1u);
}

void area_block_addwindowvertical(int turns, int type, int row)
{
    const BuildPiece *piece = &g_buildblocktype[type].pieces[row];
    float x = piece->x, width = piece->width;
    int flags = piece->flags;
    uint32_t a = (((uint32_t)turns - 2u) & 3u) + 9u;
    uint32_t b = (uint32_t)turns + 1u;
    if (flags & 1) {
        surface_set(x, 1, 0, 0, 1, 0, 0, 0, 0, x, 0, 0);
        surface_add(turns, 0x50000, a);
        surface_set(x, 1, .0625f, x, 1, 0, x, 0, 0, x, 0, .0625f);
        surface_add(turns, 0x20000, a);
        surface_set(0, 1, .0625f, x, 1, .0625f, x, 0, .0625f, 0, 0, .0625f);
        surface_add(turns, 0x60000, b);
    }
    if (flags & 2) {
        float x2 = (float)((double)width + x);
        surface_set(1, 1, 0, x2, 1, 0, x2, 0, 0, 1, 0, 0);
        surface_add(turns, 0x50000, a);
        surface_set(x2, 1, 0, x2, 1, .0625f, x2, 0, .0625f, x2, 0, 0);
        surface_add(turns, 0x10000, a);
        surface_set(x2, 1, .0625f, 1, 1, .0625f, 1, 0, .0625f, x2, 0, .0625f);
        surface_add(turns, 0x60000, b);
    }
}

void area_block_addwindowhorizontal(int turns, int type, int row, int has_floor, int has_ceiling)
{
    const BuildPiece *piece = &g_buildblocktype[type].pieces[row];
    float y = piece->y, height = piece->height;
    int flags = piece->flags;
    uint32_t a = (((uint32_t)turns - 2u) & 3u) + 9u;
    uint32_t b = (uint32_t)turns + 1u;
    if (flags & 4) {
        surface_set(1, y, 0, 0, y, 0, 0, 0, 0, 1, 0, 0);
        surface_add(turns, 0x50000, a);
        surface_set(0, y, 0, 1, y, 0, 1, y, .0625f, 0, y, .0625f);
        surface_add(turns, 0x40000, a);
        surface_set(0, y, .0625f, 1, y, .0625f, 1, 0, .0625f, 0, 0, .0625f);
        surface_add(turns, 0x60000, b);
    } else if (has_floor) {
        surface_set(1, .0625f, 0, 0, .0625f, 0, 0, 0, 0, 1, 0, 0);
        surface_add(turns, 0x50000, a);
    }
    if (flags & 8) {
        float y2 = (float)((double)height + y);
        surface_set(1, 1, 0, 0, 1, 0, 0, y2, 0, 1, y2, 0);
        surface_add(turns, 0x50000, a);
        surface_set(0, y2, .0625f, 1, y2, .0625f, 1, y2, 0, 0, y2, 0);
        surface_add(turns, 0x30000, a);
        surface_set(0, 1, .0625f, 1, 1, .0625f, 1, y2, .0625f, 0, y2, .0625f);
        surface_add(turns, 0x60000, b);
    } else if (has_ceiling) {
        surface_set(1, 1, 0, 0, 1, 0, 0, .875f, 0, 1, .875f, 0);
        surface_add(turns, 0x50000, a);
    }
}

int area_block_createbuildinginterior(int scratch, uint32_t code)
{
    uint32_t c = code & 0x3fffff;
    int block, sides[4], k;
    int type = c >> 18;
    int has_floor = (code & 0x10000) != 0;
    int has_ceiling = (code & 0x20000) != 0;
    AreaBlock *b;
    if (scratch == 0) {
        int existing = g_buildingtype.interior_blocks[c];
        if (existing != -1)
            return existing;
        if (g_blocklist.num_blocks > 0xffff)
            return 0;
        block = g_blocklist.num_blocks++;
        g_buildingtype.interior_blocks[c] = block;
    } else {
        block = 0xffff;
    }
    g_tempmesh.num_verts = 0;
    g_tempmesh.num_quads = 0;
    g_tempmesh.num_tris = 0;
    b = &g_area_block[block];
    b->num_hideable = 0;
    for (k = 0; k < 4; k++) {
        int side = (c >> (4 * k)) & 15;
        sides[k] = side;
        if (side >= 1 && side <= 8) {
            const BuildPiece *piece = &g_buildblocktype[type].pieces[side - 1];
            if (piece->kind == 0)
                area_block_addwindow(k, piece->x, piece->width, piece->y, piece->height, block, 6);
        }
    }
    b->open_sides = 0;
    if (has_floor) {
        surface_set(0, .0625f, 0, 1, .0625f, 0, 1, .0625f, 1, 0, .0625f, 1);
        addbeziersurface((const float *)g_surface, g_surface_uv, 1, 1, 1, 1, 0x40000);
        b->open_sides |= 1;
    }
    if (has_ceiling) {
        surface_set(0, .875f, 1, 1, .875f, 1, 1, .875f, 0, 0, .875f, 0);
        addbeziersurface((const float *)g_surface, g_surface_uv, 1, 1, 1, 1, 0x30005);
        b->open_sides |= 0x20;
    }
    for (k = 0; k < 4; k++) {
        int side = sides[k];
        if (side >= 1 && side <= 8) {
            area_block_addwindowvertical(k, type, side - 1);
            area_block_addwindowhorizontal(k, type, side - 1, has_floor, has_ceiling);
        } else if (side == 9) {
            surface_set(0, 1, .0625f, 1, 1, .0625f, 1, 0, .0625f, 0, 0, .0625f);
            surface_add(k, 0x60000, k + 1);
            b->open_sides |= 2 << k;
        } else if (side == 10) {
            area_block_adddoor(k, .3125f, .625f);
        } else if (side == 11) {
            area_block_addcorner(k, .0625f, .0625f);
        } else if (side == 12) {
            area_block_addoutdoor(k, .25f, .625f);
        }
    }
    b->size_x = 1;
    b->size_y = 1;
    b->size_z = 1;
    if (scratch == 0)
        area_setupsblock(block);
    else
        tempmesh_uvs(1);
    return block;
}

static void mesh_reset(void)
{
    g_tempmesh.num_verts = 0;
    g_tempmesh.num_quads = 0;
    g_tempmesh.num_tris = 0;
}

static int normal_side(const vec3 *n)
{
    if (n->y > .707f) return 0x40000;
    if (n->y < -.707f) return 0x30000;
    if (n->x > .707f) return 0x20000;
    if (n->x < -.707f) return 0x10000;
    if (n->z > .707f) return 0x60000;
    return 0x50000;
}

static void block_finish(int block, int sx, int sy, int sz)
{
    int k;
    g_area_block[block].size_x = sx;
    g_area_block[block].size_y = sy;
    g_area_block[block].size_z = sz;
    area_setupsblock(block);
    for (k = 1; k < 4; k++) {
        area_copyrotateblock(block + k, &g_area_block[block + k - 1].size_x);
        area_setupsblock(block + k);
    }
}

static void surface_emit(uint32_t flags)
{
    addbeziersurface((const float *)g_surface, g_surface_uv, 1, 1, 1, 1, flags);
}

void area_createinteriorblocks(void)
{
    uint32_t tex[6] = {0, 1, 2, 3, 4, 5};
    int i, j, k, face, block, result;
    float y, z, t;
    os_chdir("data");
    result = os_chdir("block");
    listfiles("*.sbl", 0, 0);
    removeflieextentions(0);
    if (result == 0) {
        os_chdir("..");
        os_chdir("..");
    }
    for (i = 0; i < g_filelist.list[0].count; i++) {
        char name[64];
        int garage, count;
        block = g_blocklist.file_blocks[4 * i];
        strcpy(name, g_filelist.list[0].name[i]);
        garage = strcmp(name, "garagedoor") == 0;
        strcpy(g_area_block[block].name, name);
        mesh_reset();
        loadblock(name);
        count = g_blocktemp.num_boxes;
        for (j = 0; j < count; j++) {
            const BlockBox *box = &g_blocktemp.boxes[j];
            for (face = 0; face < 6; face++) {
                vec3 n;
                int base, q;
                if (!(box->faces & (1u << face)))
                    continue;
                generatetrinormal(&n, &box->verts[g_cubequad[face][0]],
                                 &box->verts[g_cubequad[face][1]], &box->verts[g_cubequad[face][2]]);
                base = g_tempmesh.num_verts;
                q = g_tempmesh.num_quads;
                for (k = 0; k < 4; k++) {
                    int v = g_tempmesh.num_verts++;
                    g_tempmesh.verts[v] = box->verts[g_cubequad[face][k]];
                    g_tempmesh.normals[v] = n;
                    /* Box vertices keep the previous tangents. */
                    g_tempmesh.quads[q].verts[k] = base + k;
                }
                /* Each face uses the first slot of the box. */
                g_tempmesh.quad_slots[q] = box->slots[0] | normal_side(&n);
                g_tempmesh.num_quads++;
            }
        }
        for (j = 0; j < g_blocktemp.num_surfaces; j++) {
            const BlockSurface *s = &g_blocktemp.surfaces[j];
            int slot = s->slot;
            memcpy(g_surface, s->ctrl, sizeof(s->ctrl));
            memcpy(g_surface_uv, s->uv, sizeof(s->uv));
            if (s->auto_side != 0) {
                vec3 n;
                generatetrinormal(&n, &g_surface[0], &g_surface[3], &g_surface[15]);
                slot |= normal_side(&n);
            }
            addbeziersurface((const float *)g_surface, g_surface_uv, s->nu, s->tu, s->nv, s->tv, slot);
        }
        if (g_blocktemp.floor > 0)
            for (j = 0; j < g_blocktemp.size_z; j++)
                for (k = 0; k < g_blocktemp.size_x; k++) {
                    vec3 p = {(float)k, 0, (float)j};
                    area_block_addbasicsurface(0, &p);
                }
        if (g_blocktemp.ceiling > 0)
            for (j = 0; j < g_blocktemp.size_z; j++)
                for (k = 0; k < g_blocktemp.size_x; k++) {
                    vec3 p = {(float)k, (float)(g_blocktemp.size_y - 1), (float)j};
                    area_block_addbasicsurface(5, &p);
                }
        if (g_blocktemp.wall_z1 > 0)
            for (j = 0; j < g_blocktemp.size_y; j++)
                for (k = 0; k < g_blocktemp.size_x; k++) {
                    vec3 p = {(float)k, (float)j, (float)(g_blocktemp.size_z - 1)};
                    area_block_addbasicsurface(3, &p);
                }
        if (g_blocktemp.wall_x0 > 0)
            for (j = 0; j < g_blocktemp.size_y; j++)
                for (k = 0; k < g_blocktemp.size_z; k++) {
                    vec3 p = {0, (float)j, (float)k};
                    area_block_addbasicsurface(4, &p);
                }
        if (g_blocktemp.wall_z0 > 0)
            for (j = 0; j < g_blocktemp.size_y; j++)
                for (k = 0; k < g_blocktemp.size_x; k++) {
                    vec3 p = {(float)k, (float)j, 0};
                    area_block_addbasicsurface(1, &p);
                }
        if (g_blocktemp.wall_x1 > 0)
            for (j = 0; j < g_blocktemp.size_y; j++)
                for (k = 0; k < g_blocktemp.size_z; k++) {
                    vec3 p = {(float)(g_blocktemp.size_x - 1), (float)j, (float)k};
                    area_block_addbasicsurface(2, &p);
                }
        g_area_block[block].size_x = g_blocktemp.size_x;
        g_area_block[block].size_y = g_blocktemp.size_y;
        g_area_block[block].size_z = g_blocktemp.size_z;
        if (garage) {
            g_blocklist.base_door_first = block;
            g_blocklist.base_door_mid[0] = block + 1;
            g_blocklist.base_door_mid[1] = block + 2;
            g_blocklist.base_door_last = block + 3;
        }
        g_area_block[block].is_door = garage;
        area_setupsblock(block);
        for (k = 1; k < 4; k++) {
            area_copyrotateblock(block + k, &g_area_block[block + k - 1].size_x);
            g_area_block[block + k].is_door = garage;
            area_setupsblock(block + k);
        }
    }
    for (k = 0; k < 2; k++) {
        block = k ? g_blocklist.ceiling_edge_right[0] : g_blocklist.ceiling_edge[0];
        strcpy(g_area_block[block].name, k ? "ceilingedgerightwall" : "ceilingedge");
        mesh_reset();
        area_block_addbeam(0, 0, .875f, .9375f, 1, .125f, .0625f, 0x3f, tex);
        surface_set(0, .0625f, 0, 1, .0625f, 0, 1, .0625f, 1, 0, .0625f, 1);
        surface_emit(0x40000);
        if (k) {
            surface_set(0, 1, .0625f, 1, 1, .0625f, 1, 0, .0625f, 0, 0, .0625f);
            surface_add(1, 0x60000, 2);
        }
        block_finish(block, 1, 1, 1);
    }
    for (k = 0; k < 3; k++) {
        block = k == 0 ? g_blocklist.railing[0] : k == 1 ? g_blocklist.railing_left[0] : g_blocklist.railing_right[0];
        strcpy(g_area_block[block].name, k == 0 ? "railing" : k == 1 ? "railingleftwall" : "railingrightwall");
        mesh_reset();
        area_block_addbeam(0, 0, 0, .9375f, 1, .25f, .0625f, 0x3f, tex);
        if (k) {
            surface_set(0, 1, .0625f, 1, 1, .0625f, 1, 0, .0625f, 0, 0, .0625f);
            surface_add(k == 1 ? 3 : 1, 0x60000, k == 1 ? 4 : 2);
        }
        surface_set(0, .875f, 1, 1, .875f, 1, 1, .875f, 0, 0, .875f, 0);
        surface_emit(0x30005);
        block_finish(block, 1, 1, 1);
    }
    block = g_blocklist.stairs_balcony[0];
    strcpy(g_area_block[block].name, "stairsbalcony");
    mesh_reset();
    for (k = 0; k < 6; k++) tex[k] = 6;
    y = .0625f;
    z = 0;
    for (i = 0; i < 8; i++) {
        area_block_addbeam(0, 0, y, z, .5f, .0625f, i == 7 ? .75f : .125f, 0x3f, tex);
        y = (float)((double)y + .0625);
        z = (float)((double)z + .125);
    }
    t = (float)((double)y - .0625);
    area_block_addbeam(0, .5f, t, z, .5f, .0625f, .625f, 0x3f, tex);
    for (i = 0; i < 8; i++) {
        z = (float)((double)z - .125);
        area_block_addbeam(0, .5f, y, z, .5f, .0625f, .125f, 0x3f, tex);
        y = (float)((double)y + .0625);
    }
    surface_set(.9375f, 2, 0, .9375f, 2, 2, .9375f, 0, 2, .9375f, 0, 0);
    surface_emit(0x10002);
    surface_set(0, .0625f, 0, 1, .0625f, 0, 1, .0625f, 2, 0, .0625f, 2);
    surface_emit(0x40000);
    surface_set(0, 1.875f, 2, 1, 1.875f, 2, 1, 1.875f, 0, 0, 1.875f, 0);
    surface_emit(0x30005);
    for (k = 0; k < 6; k++) tex[k] = k;
    area_block_addbeam(0, 0, .875f, 0, 1, .125f, .0625f, 0x3f, tex);
    tex[0] = 1;
    area_block_addbeam(0, 0, 1, 0, .5f, .25f, .0625f, 0x3f, tex);
    block_finish(block, 1, 2, 2);
    block = g_blocklist.trim[0];
    mesh_reset();
    surface_set(.9375f, 1, 0, .9375f, 1, 1, .9375f, 0, 1, .9375f, 0, 0);
    surface_emit(0x10002);
    surface_set(1, 1, .9375f, 0, 1, .9375f, 0, 0, .9375f, 1, 0, .9375f);
    surface_emit(0x50002);
    surface_set(.0625f, 1, 1, .0625f, 1, 0, .0625f, 0, 0, .0625f, 0, 1);
    surface_emit(0x20002);
    block_finish(block, 1, 1, 1);
    for (k = 0; k < 3; k++) {
        block = k == 0 ? g_blocklist.stairs[0] : k == 1 ? g_blocklist.stairs_floor[0] : g_blocklist.stairs_ceiling[0];
        strcpy(g_area_block[block].name, k == 0 ? "stairs" : k == 1 ? "stairsfloor" : "stairsceiling");
        mesh_reset();
        y = .0625f;
        z = 0;
        if (k == 2) {
            area_block_addbox(.5f, 0, 0, .5f, .0625f, .125f, 0);
            area_block_addbox(0, 0, 0, .5f, 1, .0625f, 2);
        } else {
            for (i = 0; i < 8; i++) {
                area_block_addbox(0, y, z, .5f, .0625f, i == 7 ? 1 : .125f, 0);
                y = (float)((double)y + .0625);
                z = (float)((double)z + .125);
            }
            t = (float)((double)y - .0625);
            area_block_addbox(.5f, t, z, .5f, .0625f, 1, 0);
            for (i = 0; i < 8; i++) {
                z = (float)((double)z - .125);
                t = i == 7 ? (float)((double)y - 1.0) : y;
                area_block_addbox(.5f, t, z, .5f, .0625f, .125f, 0);
                y = (float)((double)y + .0625);
            }
        }
        surface_set(.0625f, 1, 2, .0625f, 1, 0, .0625f, 0, 0, .0625f, 0, 2);
        surface_emit(0x20004);
        surface_set(.9375f, 1, 0, .9375f, 1, 2, .9375f, 0, 2, .9375f, 0, 0);
        surface_emit(0x10002);
        if (k == 0) {
            surface_set(0, .0625f, 0, 1, .0625f, 0, 1, 0, 0, 0, 0, 0);
            surface_emit(0x50003);
        }
        if (k < 2) {
            surface_set(0, 1, 0, 1, 1, 0, 1, .875f, 0, 0, .875f, 0);
            surface_emit(0x50003);
            if (k == 1) {
                surface_set(0, .0625f, 0, 1, .0625f, 0, 1, .0625f, 2, 0, .0625f, 2);
                surface_emit(0x40000);
            }
        } else {
            surface_set(0, .875f, 2, 1, .875f, 2, 1, .875f, 0, 0, .875f, 0);
            surface_emit(0x30001);
        }
        surface_set(1, 1, 1.5f, 0, 1, 1.5f, 0, 0, 1.5f, 1, 0, 1.5f);
        surface_emit(0x60001);
        block_finish(block, 1, 1, 2);
    }
}

static void block_ids(int *ids, int count)
{
    int i;
    for (i = 0; i < count; i++)
        ids[i] = g_blocklist.num_blocks++;
}

static void block_store(int block, int height)
{
    g_area_block[block].size_x = 1;
    g_area_block[block].size_y = height;
    g_area_block[block].size_z = 1;
    area_setupsblock(block);
}

static void surface_uv_rect(float top, float bottom)
{
    float *uv = g_surface_uv;
    uv[0] = 0; uv[1] = top;
    uv[6] = 1; uv[7] = top;
    uv[30] = 1; uv[31] = bottom;
    uv[24] = 0; uv[25] = bottom;
}

static void terrain_top(int row, float low, float high)
{
    surface_set(0, row == 0 || row == 1 ? low : high, 0,
                1, row == 1 || row == 2 ? low : high, 0,
                1, row == 2 || row == 3 ? low : high, 1,
                0, row == 3 || row == 0 ? low : high, 1);
}

static void terrain_floor(void)
{
    surface_set(0, .0625f, 0, 1, .0625f, 0, 1, .0625f, 1, 0, .0625f, 1);
}

static void terrain_texture(const vec3 *points, int flags)
{
    /* This builder does not read its UV argument. */
    addbeziersurfacetexture((const float *)points, (const float *)(uintptr_t)1, 1, 1, 8, 3, flags);
}

static void terrain_shell(void)
{
    vec3 *s = g_surface;
    vec3 *t = ((vec3 *)&sr_data_surface2.points);
    int row;
    for (row = 0; row < 4; row++) {
        int a = row * 4, b = a + 3;
        t[a] = s[b];
        t[b] = s[a];
        t[a].y = (float)((double)s[b].y + .6875);
        t[b].y = (float)((double)s[a].y + .6875);
    }
    terrain_texture(t, 0x40000);
    for (row = 0; row < 4; row++) {
        int a = row * 4, b = a + 3;
        t[a] = s[b];
        t[b] = s[b];
        t[a].y = 0;
        t[b].y = 2;
    }
    terrain_texture(t, 0x50000);
    for (row = 0; row < 4; row++) {
        int a = row * 4, b = a + 3;
        t[a] = s[a];
        t[b] = s[a];
        t[a].y = 2;
        t[b].y = 0;
    }
    terrain_texture(t, 0x50000);
}

void area_createsblocks(void)
{
    const double sixth = .1666666716337204;
    static const vec3 hills[2][4][2] = {
        {{{0, .0625f, 1.9375f}, {0, .0625f, .0625f}},
         {{1, .0625f, 1.9375f}, {2, .0625f, .0625f}},
         {{2, .3125f, 3}, {4, .3125f, 2}},
         {{2, .5625f, 4}, {4, .5625f, 4}}},
        {{{2, .5625f, 0}, {4, .5625f, 0}},
         {{2, .8125f, 1}, {4, .8125f, 2}},
         {{1, 1.0625f, 2.0625f}, {2, 1.0625f, 3.9375f}},
         {{0, 1.0625f, 2.0625f}, {0, 1.0625f, 3.9375f}}}
    };
    int *turns[18] = {
        g_blocklist.corner_a, g_blocklist.corner_b,
        g_blocklist.ceiling_edge, g_blocklist.ceiling_edge_right,
        g_blocklist.railing, g_blocklist.railing_left, g_blocklist.railing_right,
        g_blocklist.stairs_balcony, g_blocklist.stairs, g_blocklist.stairs_floor,
        g_blocklist.stairs_ceiling, g_blocklist.hill_a, g_blocklist.hill_b,
        g_blocklist.flat, g_blocklist.rail_low, g_blocklist.rail_low2,
        g_blocklist.fence, g_blocklist.fence_end
    };
    uint32_t tex[6] = {1, 1, 1, 1, 1, 1};
    int i, j, row, block, result;
    float h;
    vec3 *s = g_surface;
    vec3 *t = ((vec3 *)&sr_data_surface2.points);
    g_blocklist.num_blocks = 0;
    block_ids(g_blocklist.edge, 16);
    block_ids(g_blocklist.slab, 4);
    block_ids(g_blocklist.trim, 4);
    block_ids(g_blocklist.misc_6c, 2);
    block_ids(&g_blocklist.elevator_block, 1);
    block_ids(g_blocklist.misc_78, 5);
    block_ids(g_blocklist.misc_284, 20);
    block_ids(g_blocklist.misc_8c, 10);
    block_ids(g_blocklist.slope4, 16);
    block_ids(g_blocklist.slope6, 24);
    for (i = 0; i < 24; i++)
        g_blocklist.ramp6[i] = g_blocklist.num_blocks + i;
    /* The counter keeps the last slope id until the turn range starts. */
    g_blocklist.num_blocks += 23;
    g_blocklist.turn_first = (g_blocklist.num_blocks + 4) & ~3;
    for (i = 0; i < 18; i++)
        for (j = 0; j < 4; j++)
            turns[i][j] = g_blocklist.turn_first + i * 4 + j;
    g_blocklist.num_blocks = g_blocklist.turn_first + 0x48;
    os_chdir("data");
    result = os_chdir("block");
    listfiles("*.sbl", 0, 0);
    if (result == 0) {
        os_chdir("..");
        os_chdir("..");
    }
    for (i = 0; i < g_filelist.list[0].count; i++)
        block_ids(&g_blocklist.file_blocks[4 * i], 4);
    for (i = 0; i < 4; i++)
        g_blocklist.corner_up[i] = g_blocklist.num_blocks + i;
    g_blocklist.turn_end = g_blocklist.num_blocks;
    g_blocklist.num_blocks += 4;
    memset(g_buildingtype.interior_blocks, 0xff, sizeof(g_buildingtype.interior_blocks));
    area_createinteriorblocks();

    for (row = 0; row < 4; row++) {
        h = 0;
        for (j = 0; j < 6; j++) {
            double top = (double)h + .0625;
            float low = (float)top, high = (float)(top + sixth);
            float bottom_high = (float)((double)h + sixth);
            mesh_reset();
            terrain_top(row, low, high);
            surface_uv_rect(1, 0);
            surface_emit(0);
            if (row == 0 || row == 2) {
                surface_set(1, row == 2 ? low : high, 0,
                            0, row == 0 ? low : high, 0,
                            0, row == 0 ? h : bottom_high, 0,
                            1, row == 2 ? h : bottom_high, 0);
                surface_uv_rect((float)sixth, 0);
                surface_emit(0);
                surface_set(0, row == 0 ? low : high, 1,
                            1, row == 2 ? low : high, 1,
                            1, row == 2 ? h : bottom_high, 1,
                            0, row == 0 ? h : bottom_high, 1);
            } else {
                surface_set(0, row == 1 ? low : high, 0,
                            0, row == 3 ? low : high, 1,
                            0, row == 3 ? h : bottom_high, 1,
                            0, row == 1 ? h : bottom_high, 0);
                surface_uv_rect((float)sixth, 0);
                surface_emit(0);
                surface_set(1, row == 3 ? low : high, 1,
                            1, row == 1 ? low : high, 0,
                            1, row == 1 ? h : bottom_high, 0,
                            1, row == 3 ? h : bottom_high, 1);
            }
            surface_uv_rect((float)sixth, 0);
            surface_emit(0);
            block_store(g_blocklist.ramp6[row * 6 + j], top + sixth >= 1.0 ? 2 : 1);
            h = (float)((double)h + sixth);
        }
    }
    for (i = 0; i < 2; i++) {
        int count = i == 0 ? 6 : 4;
        int *ids = i == 0 ? g_blocklist.slope6 : g_blocklist.slope4;
        double step = i == 0 ? sixth : .25;
        for (row = 0; row < 4; row++) {
            h = 0;
            for (j = 0; j < count; j++) {
                float high = (float)((double)h + step);
                mesh_reset();
                terrain_top(row, h, high);
                surface_uv_rect(i == 0 ? 0.0f : 1.0f, i == 0 ? 1.0f : 0.0f);
                surface_emit(0);
                block_store(ids[row * count + j], 1);
                h = (float)((double)h + step);
            }
        }
    }
    mesh_reset();
    for (row = 0; row < 4; row++) {
        float z = row == 0 ? 0 : row == 1 ? .25f : row == 2 ? .75f : 1;
        float y = row < 2 ? 0 : .0625f;
        s[row * 4] = (vec3){0, y, z};
        s[row * 4 + 3] = (vec3){1, y, z};
    }
    surface_uv_rect(1, 0);
    addbeziersurface((const float *)s, g_surface_uv, 1, 1, 8, 3, 0x30000);
    block_finish(g_blocklist.slab[0], 1, 1, 1);
    for (i = 0; i < 16; i++) {
        mesh_reset();
        terrain_floor();
        surface_uv_rect(1, 0);
        surface_emit(0);
        if (i & 1) {
            surface_set(1, .0625f, 0, 0, .0625f, 0, 0, 0, 0, 1, 0, 0);
            surface_uv_rect(.0625f, 0);
            surface_emit(0);
        }
        if (i & 8) {
            surface_set(0, .0625f, 0, 0, .0625f, 1, 0, 0, 1, 0, 0, 0);
            surface_uv_rect(.0625f, 0);
            surface_emit(0);
        }
        if (i & 4) {
            surface_set(0, .0625f, 1, 1, .0625f, 1, 1, 0, 1, 0, 0, 1);
            surface_uv_rect(.0625f, 0);
            surface_emit(0);
        }
        if (i & 2) {
            surface_set(1, .0625f, 1, 1, .0625f, 0, 1, 0, 0, 1, 0, 1);
            surface_uv_rect(.0625f, 0);
            surface_emit(0);
        }
        block_store(g_blocklist.edge[i], 1);
    }
    for (i = 0; i < 2; i++) {
        mesh_reset();
        terrain_floor();
        surface_emit(0x40000);
        surface_set(0, .875f, 1, 1, .875f, 1, 1, .875f, 0, 0, .875f, 0);
        surface_emit(0x30001);
        surface_set(1, 1, 0, 0, 1, 0, 0, .875f, 0, 1, .875f, 0);
        surface_emit(0x6000b);
        if (i == 0) {
            surface_set(1, 1, 0, .9375f, 1, 0, .9375f, 0, 0, 1, 0, 0);
        } else {
            surface_set(.0625f, 1, 0, 0, 1, 0, 0, 0, 0, .0625f, 0, 0);
        }
        surface_emit(0x6000b);
        area_block_addcorner(i == 0 ? 1 : 0, .0625f, .0625f);
        block_finish(i == 0 ? g_blocklist.corner_a[0] : g_blocklist.corner_b[0], 1, 1, 1);
    }
    for (i = 0; i < 4; i++) {
        mesh_reset();
        surface_set(0, i == 0 || i == 3 ? 1.0f : 0.0f, 0,
                    1, i == 0 || i == 1 ? 1.0f : 0.0f, 0,
                    1, i == 1 || i == 2 ? 1.0f : 0.0f, 1,
                    0, i == 2 || i == 3 ? 1.0f : 0.0f, 1);
        surface_uv_rect(1, 0);
        surface_emit(0);
        block_store(g_blocklist.corner_up[i], 1);
    }
    for (i = 0; i < 2; i++) {
        mesh_reset();
        for (row = 0; row < 4; row++) {
            s[row * 4] = hills[i][row][0];
            s[row * 4 + 3] = hills[i][row][1];
        }
        terrain_texture(s, 0x30000);
        if (i == 1) {
            for (row = 0; row < 4; row++) {
                int a = row * 4, b = a + 3;
                t[a] = s[a];
                t[b] = s[b];
                t[a].y = (float)((double)s[a].y - 1.0);
                t[b].y = (float)((double)s[b].y - 1.0);
            }
            terrain_texture(t, 0x30000);
        }
        terrain_shell();
        block_finish(i == 0 ? g_blocklist.hill_a[0] : g_blocklist.hill_b[0], 4, 2, 4);
    }
    for (i = 0; i < 5; i++) {
        mesh_reset();
        if (i < 3) {
            terrain_floor();
            surface_uv_rect(1, 0);
            surface_emit(0);
        }
        if (i == 1 || i == 2) {
            area_block_addbeam(0, 0, 0, 0, 1, .3125f, .0625f, 0x2a, tex);
        } else if (i == 3) {
            area_block_addbeam(0, 0, 0, 0, 1, .0625f, 1, 0x22, tex);
            area_block_addbeam(0, 0, 0, 0, 1, .3125f, .0625f, 0x2a, tex);
        } else if (i == 4) {
            area_block_addbeam(0, 0, 0, 0, 1, .0625f, 1, 0x32, tex);
            area_block_addbeam(0, 0, 0, 0, .0625f, .3125f, .0625f, 0x32, tex);
        }
        block = i == 0 ? g_blocklist.flat[0] : i == 1 ? g_blocklist.rail_low[0] :
                i == 2 ? g_blocklist.rail_low2[0] : i == 3 ? g_blocklist.fence[0] : g_blocklist.fence_end[0];
        block_finish(block, 1, 1, 1);
    }
}
