/* The list of the sectors to draw and the pass that draws their outer blocks. clearsectorlist
 * chooses the sectors and sorts the ones with an interior by their distance. It also collects
 * the runs of triangles of the sectors for each material. render_sectorlist draws the runs. */
#include "render/sectorlist.h"

#include "client.h"
#include "render/frustum.h"
#include "setup/timers.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"

#define g_timerlist cl_data_timerlist
#define g_texturelist cl_data_texturelist
#define g_textures cl_data_texture
#define g_materials cl_data_material_records
#define g_materiallist sr_data_materiallist
#define g_programs cl_data_programs
#define g_block_sectors cl_data_block_sectors
#define g_glext cl_data_glext
#define g_vbo cl_data_vbo
#define g_camera cl_data_camera_position
#define g_light_position cl_data_light_position
#define g_world_position cl_data_world_position
#define g_part_count cl_data_sector_part_count
#define g_draw_counts cl_data_sector_draw_counts
#define g_draw_offsets cl_data_sector_draw_offsets
#define g_sector_count cl_data_sector_count
#define g_sector_list cl_data_sector_list
#define g_sector_entries cl_data_sector_entries
#define g_interior_model_count cl_data_interior_model_count
#define g_sort_count cl_data_sort_count
#define g_sort_order cl_data_sort_order
#define g_sort_keys cl_data_sort_keys

/* Game functions of other files. */
#define sortlist CL_FN(0x4e0f80, void(__cdecl *)(void))
#define markinteriors CL_FN(0x4d9fa0, void(__cdecl *)(void))
#define area_interior_buildsectormodel CL_FN(0x4442b0, int(__cdecl *)(int sector, int slot))

/* The slots of the extension functions: the client holds a pointer to each one. */
#define SLOT_ACTIVE_TEXTURE 0x115874
#define SLOT_BIND_BUFFER 0x115aa8
#define SLOT_USE_PROGRAM 0x115b18
#define SLOT_UNIFORM_3FV 0x115b48
#define SLOT_MULTI_DRAW 0x115ec0

GL_IMPORT(glEnableClientState, (uint32_t array), (array))
GL_IMPORT(glDisableClientState, (uint32_t array), (array))
GL_IMPORT(glColorPointer, (int size, uint32_t type, int stride, const void *pointer), (size, type, stride, pointer))
GL_IMPORT(glNormalPointer, (uint32_t type, int stride, const void *pointer), (type, stride, pointer))
GL_IMPORT(glTexCoordPointer, (int size, uint32_t type, int stride, const void *pointer), (size, type, stride, pointer))
GL_IMPORT(glLightfv, (uint32_t light, uint32_t name, const float *value), (light, name, value))

enum {
    MAX_SECTORS = 0x2000,
    MATERIALS = 0x40,
    CLEARED_PART_COUNTS = 0x80,
    RUNS_PER_MATERIAL = 0x4000,
    SLOTS_PER_SECTOR = 4,
    TRIANGLE_INDEX_SIZE = 12,
    VERTEX_SIZE = 0x40,
    NORMAL_OFFSET = 0x0c,
    TEXCOORD_OFFSET = 0x24,
    COLOR_OFFSET = 0x30,
    GLEXT_BUFFERS = 6,
    GLEXT_MULTI_DRAW = 12,
    PROGRAM_SECTOR = 0,
    PROGRAM_WINDOW = 1,
    UNIFORM_CAMERA = 1,
    UNIFORM_WORLD = 4,
    UNIFORM_LIGHT = 5,
    MODE_LIT = 0,
    MODE_DEPTH = 1,
    MODE_PLAIN = 2,
    GL_VERTEX_ARRAY = 0x8074,
    GL_NORMAL_ARRAY = 0x8075,
    GL_COLOR_ARRAY = 0x8076,
    GL_TEXTURE_COORD_ARRAY = 0x8078,
    GL_LIGHT0 = 0x4000,
    GL_POSITION = 0x1203,
    TEXTURE_UNIT_1 = 0x84c1,
    TEXTURE_UNIT_0 = 0x84c0
};
#define NEAR_DISTANCE 32.0f

static void select_texture_unit(uint32_t unit)
{
    (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_ACTIVE_TEXTURE))(unit);
}

static void bind_buffer(uint32_t target, uint32_t buffer)
{
    (*(void(__stdcall **)(uint32_t, uint32_t))(sr_base + SLOT_BIND_BUFFER))(target, buffer);
}

static void use_program(uint32_t program)
{
    (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_USE_PROGRAM))(program);
}

static void set_uniform(int location, const float *value)
{
    (*(void(__stdcall **)(int, int, const float *))(sr_base + SLOT_UNIFORM_3FV))(location, 1, value);
}

/* The middle of a sector. */
static void sector_center(const BlockSector *sector, vec3 *center)
{
    float block = sr_data_area->block_size;

    center->x = (float)(block * 4.0 + (float)sector->pos[0] * 8.0 * block);
    center->y = (float)(block * 4.0 + (float)sector->pos[1] * 8.0 * block);
    center->z = (float)(block * 4.0 + (float)sector->pos[2] * 8.0 * block);
}

/* The corners of the box of a sector. */
static void sector_box(const BlockSector *sector, vec3 *smallest, vec3 *largest)
{
    float block = sr_data_area->block_size;

    smallest->x = (float)((float)sector->pos[0] * 8.0 * block);
    smallest->y = (float)((float)sector->pos[1] * 8.0 * block);
    smallest->z = (float)((float)sector->pos[2] * 8.0 * block);
    largest->x = (float)((float)(sector->pos[0] + 1) * 8.0 * block);
    largest->y = (float)((float)(sector->pos[1] + 1) * 8.0 * block);
    largest->z = (float)((float)(sector->pos[2] + 1) * 8.0 * block);
}

/* Add the runs of triangles of a sector to the lists of their materials. A sector near the
 * camera leaves out the materials with windows. */
static void add_runs(const BlockSector *sector)
{
    int part;

    for (part = 0; part < sector->num_meshes; ++part) {
        int material = sector->parts[part].material;
        int run = g_part_count[material];

        if (sector->near_camera != 0 && g_materials[material].flags != 0)
            continue;
        g_draw_counts[material * RUNS_PER_MATERIAL + run] = sector->parts[part].num_triangles * 3;
        g_draw_offsets[material * RUNS_PER_MATERIAL + run] =
            (void *)((const char *)&cl_data_sector_indices + sector->parts[part].first_index * TRIANGLE_INDEX_SIZE);
        g_part_count[material] = run + 1;
    }
}

/* Choose the sectors to draw. The mode 0 draws every sector, any other mode only the sectors
 * in the view cone or near the camera. The sectors with an interior go into a list that is
 * sorted from the nearest. */
void __cdecl clearsectorlist(int mode)
{
    int sector;
    int slot;

    for (slot = 0; slot < CLEARED_PART_COUNTS; ++slot)
        g_part_count[slot] = 0;
    g_sector_count = 0;
    g_interior_model_count = 0;
    for (sector = 0; sector < MAX_SECTORS; ++sector) {
        BlockSector *block = &g_block_sectors[sector];
        vec3 center;

        if (block->used == 0)
            continue;
        block->near_camera = 0;
        for (slot = 0; slot < SLOTS_PER_SECTOR; ++slot)
            block->interior_used[slot] = 0;
        sector_center(block, &center);
        if ((float)vectordistance(&center, &g_camera) < NEAR_DISTANCE)
            block->near_camera = 1;
    }
    markinteriors();
    for (sector = 0; sector < MAX_SECTORS; ++sector) {
        BlockSector *block = &g_block_sectors[sector];
        int found = 0;
        vec3 center;

        if (block->used == 0 || block->near_camera == 0)
            continue;
        for (slot = 0; slot < SLOTS_PER_SECTOR; ++slot)
            if (block->interior_used[slot] != 0 && -1 < area_interior_buildsectormodel(sector, slot))
                found = 1;
        if (found == 0)
            continue;
        sector_center(block, &center);
        g_sector_entries[g_sector_count].sector = sector;
        g_sector_entries[g_sector_count].distance = (float)vectordistance(&center, &g_camera);
        ++g_sector_count;
    }
    for (sector = 0; sector < MAX_SECTORS; ++sector) {
        BlockSector *block = &g_block_sectors[sector];
        vec3 smallest;
        vec3 largest;

        if (block->used == 0)
            continue;
        if (mode != 0) {
            sector_box(block, &smallest, &largest);
            if (boxinsidefrustum(&smallest, &largest) == 0 && block->near_camera == 0)
                continue;
        }
        add_runs(block);
    }
    for (slot = 0; slot < g_sector_count; ++slot)
        g_sort_keys[slot] = cl_ftoi(16777216.0 - g_sector_entries[slot].distance * 256.0);
    g_sort_count = g_sector_count;
    sortlist();
    for (slot = 0; slot < g_sector_count; ++slot)
        g_sector_list[slot] = g_sector_entries[g_sort_order[slot]].sector;
}

/* Point the vertex arrays at the vertices of the sectors. They are in a buffer when the
 * extension for buffers is on. */
static void set_arrays(int mode)
{
    const char *vertices = (const char *)cl_data_sector_vertices;
    int buffered = g_glext[GLEXT_BUFFERS] != 0;

    if (buffered) {
        bind_buffer(GL_ARRAY_BUFFER, g_vbo[cl_data_sector_vbo].gl_name);
        vertices = 0;
    }
    glVertexPointer(3, GL_FLOAT, VERTEX_SIZE, vertices);
    if (mode == MODE_LIT)
        glNormalPointer(GL_FLOAT, VERTEX_SIZE, vertices + NORMAL_OFFSET);
    if (mode != MODE_DEPTH) {
        glTexCoordPointer(3, GL_FLOAT, VERTEX_SIZE, vertices + TEXCOORD_OFFSET);
        glColorPointer(3, GL_FLOAT, VERTEX_SIZE, vertices + COLOR_OFFSET);
    }
}

/* Choose the program for a material and give it the camera, the world place and the light. */
static void select_program(int material)
{
    const ProgramObject *program = &g_programs[material == g_materiallist.preset_end ? PROGRAM_WINDOW : PROGRAM_SECTOR];

    use_program(program->program);
    set_uniform(program->uniforms[UNIFORM_CAMERA], &g_camera.x);
    set_uniform(program->uniforms[UNIFORM_WORLD], &g_world_position.x);
    set_uniform(program->uniforms[UNIFORM_LIGHT], g_light_position);
}

/* Draw the runs of triangles of one material. */
static void draw_runs(int material)
{
    const uint32_t *counts = (const uint32_t *)&g_draw_counts[material * RUNS_PER_MATERIAL];
    const void *const *offsets = (const void *const *)&g_draw_offsets[material * RUNS_PER_MATERIAL];
    int run;

    if (g_glext[GLEXT_MULTI_DRAW] == 0) {
        for (run = 0; run < g_part_count[material]; ++run)
            glDrawElements(GL_TRIANGLES, (int)counts[run], GL_UNSIGNED_INT, offsets[run]);
    } else {
        (*(void(__stdcall **)(uint32_t, const int *, uint32_t, const void *const *, int))(sr_base + SLOT_MULTI_DRAW))(
            GL_TRIANGLES, (const int *)counts, GL_UNSIGNED_INT, offsets, g_part_count[material]);
    }
}

/* Draw the outer blocks of the sectors that clearsectorlist chose. The mode 0 draws them lit
 * with a shadow map, the mode 1 draws only the shapes for the depth pass and the mode 2 draws
 * them with their textures only. */
void __cdecl render_sectorlist(int mode)
{
    int material;

    starttimer(g_timerlist.rendersectors);
    glEnableClientState(GL_VERTEX_ARRAY);
    if (mode == MODE_LIT)
        glEnableClientState(GL_NORMAL_ARRAY);
    if (mode != MODE_DEPTH) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
    }
    set_arrays(mode);
    for (material = 0; material < MATERIALS; ++material) {
        if (g_part_count[material] <= 0)
            continue;
        if (mode == MODE_LIT) {
            select_program(material);
            glBindTexture(GL_TEXTURE_2D, g_textures[g_texturelist.material[material]].gl_name);
            select_texture_unit(TEXTURE_UNIT_1);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, g_textures[g_texturelist.shadow_map].gl_name);
            glLightfv(GL_LIGHT0, GL_POSITION, g_light_position);
        } else if (mode == MODE_PLAIN) {
            glBindTexture(GL_TEXTURE_2D, g_textures[g_texturelist.material[material]].gl_name);
        }
        draw_runs(material);
        if (mode == MODE_LIT) {
            glDisable(GL_TEXTURE_2D);
            select_texture_unit(TEXTURE_UNIT_0);
        }
    }
    if (g_glext[GLEXT_BUFFERS] != 0)
        bind_buffer(GL_ARRAY_BUFFER, 0);
    if (mode != MODE_DEPTH) {
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    if (mode == MODE_LIT)
        glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    if (mode == MODE_LIT)
        use_program(0);
    stoptimer(g_timerlist.rendersectors);
}
