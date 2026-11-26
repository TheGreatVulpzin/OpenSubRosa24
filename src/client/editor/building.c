/* The building editor: the editor views, the grid of cells and the drawing of the blocks.
 * A building has 64 floors of 64 by 64 cells. Each cell holds a block, an interior, two walls
 * and the texture slots of its outer and inner layer. */
#include "editor/building.h"

#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "math/small_vectors.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"
#include "block_mesh.h"

#define HALF_PI_F 1.5707963705062866f
#define GROUP_MASK 0xe0000000u
#define GROUP_FLAG 0x80000000u
#define INTERIOR_BLOCK 0xffff
#define LAST_SLOT 7
#define WALL_FIXED 9
#define GRID_SIZE 64
#define GRID_HALF 8
#define BORDER_GAP 0.125
#define CELL_FILL 0.9375f
#define GL_LINES 0x0001
#define GL_CULL_FACE 0x0b44
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_LINE 0x1b01
#define GL_FILL 0x1b02

GL_IMPORT(glVertex3f, (float x, float y, float z), (x, y, z))
GL_IMPORT(glVertex3fv, (const float *vertex), (vertex))
GL_IMPORT(glTexCoord2f, (float s, float t), (s, t))
GL_IMPORT(glTexCoord2fv, (const float *coord), (coord))
GL_IMPORT(glPolygonMode, (uint32_t face, uint32_t mode), (face, mode))

GL_IMPORT(glVertex2fv, (const float *vertex), (vertex))

#define MATERIAL_NAME 64
#define DELETE_LIMIT 31

/* Return one for a window wall. The fixed wall also counts when requested. */
int building_iswindow(int x, int floor, int z, int direction, int fixed)
{
    const FloorplanCell *cell = &sr_data_floorplan[x + z * 64 + floor * 4096];
    int wall = direction == 0 ? cell->wall_row : cell->wall_col;
    return (wall > 0 && wall < 9) || (fixed && wall == 9);
}

/* Map material names to game texture indices. The last matching name wins. */
void building_reloadtextures(void)
{
    Building *building = &sr_data_building;
    int entry, material;
    memset(building->material_textures, 0, sizeof building->material_textures);
    for (entry = 0; entry < building->num_materials; ++entry)
        for (material = 0; material < sr_data_materiallist.num_materials; ++material)
            if (strcmp(building->materials + entry * MATERIAL_NAME,
                       sr_data_materials.records[material].name) == 0)
                building->material_textures[entry] = material;
}

/* The cell of the building at x, floor, z: cells[floor][z][x]. */
#define CELL(x, floor, z) (&sr_data_building.cells[(floor) * 0x1000 + (z) * 0x40 + (x)])

/* Set the view of an editor: the viewport, the camera place, a camera that looks down and the
 * zoom. */
static void editor_view_setup(EditorView *view, float zoom, float x, float z)
{
    view->x = 32.0f;
    view->y = 32.0f;
    view->width = 768.0f;
    view->height = 576.0f;
    view->scale = 0.75f;
    view->zoom = zoom;
    view->camera.x = x;
    view->camera.y = 0.0f;
    view->camera.z = z;
    resetorientation((vec3 *)view->orientation);
    rotateorientation((vec3 *)view->orientation, (const vec3 *)view->orientation, HALF_PI_F);
}

/* Start the building editor: the view, the first tool and a building of 24 by 24 by 32. */
void __cdecl buildingedit_setup(void)
{
    BuildingEditor *editor = &cl_data_building_editor;
    int i;

    editor_view_setup(&cl_data_building_view, 12.0f, 12.0f, 9.0f);
    editor->tool = 9;
    for (i = 0; i < 5; i++)
        editor->packed_parts[i] = 0;
    editor->cell_field_14 = 1;
    editor->unused_04 = 0;
    editor->block_kind = 0;
    editor->outer_block = 0;
    editor->inner_block = 0;
    for (i = 0; i < 6; i++)
        editor->outer_slots[i] = 0;
    for (i = 0; i < 8; i++)
        editor->inner_slots[i] = 0;
    sr_data_building.size_x = 24;
    sr_data_building.size_z = 24;
    sr_data_building.floors = 32;
}

/* Start the level editor: the view and no chosen block. */
void __cdecl leveledit_setup(void)
{
    editor_view_setup(&cl_data_level_view, 64.0f, 160.0f, 160.0f);
    sr_data_level_editor.street_lanes_a = 3;
    sr_data_level_editor.street_lanes_b = 3;
    sr_data_level_editor.place_type = 0;
    sr_data_level_editor.chosen_a = -1;
    sr_data_level_editor.chosen_b = -1;
    sr_data_level_editor.chosen_street = -1;
}

/* Draw a flat quad on the ground (y = 0) from x, z with a size and a color. Texture -1 draws
 * it with no texture. */
void __cdecl buildingeditor_drawquad(int texture, float x, float z, float width, float depth, float red,
                                     float green, float blue, float alpha)
{
    if (texture == -1)
        glDisable(GL_TEXTURE_2D);
    else
        glBindTexture(GL_TEXTURE_2D, cl_data_texture[texture].gl_name);
    glBegin(GL_QUADS);
    glColor4f(red, green, blue, alpha);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(x, 0.0f, z);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f((float)(x + width), 0.0f, z);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f((float)(x + width), 0.0f, (float)(z + depth));
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(x, 0.0f, (float)(z + depth));
    glEnd();
    if (texture == -1)
        glEnable(GL_TEXTURE_2D);
}

/* The block of a group number: the first block of the group plus the turn (bits 24 and 25),
 * in 16 bits. */
static uint32_t group_block(uint32_t value)
{
    return (uint32_t)(sr_data_building.block_types[value & 0x3ff] + (int)(value >> 24 & 3)) & 0xffff;
}

/* Draw the block of one cell. An interior cell gets a block made from the walls of the cell
 * and of the cells after it in x and z. Layer 1 is the interior layer. */
void __cdecl buildingeditor_drawblock(int x, int floor, int z)
{
    const BuildingCell *cell = CELL(x, floor, z);
    uint32_t block = cell->outer, interior = cell->inner;

    if (interior == INTERIOR_BLOCK) {
        uint32_t key = (uint32_t)cell->interior_bit18 << 18 | (uint32_t)CELL(x, floor, z + 1)->wall_row << 8 |
                       (uint32_t)cell->wall_col << 12 | (uint32_t)CELL(x + 1, floor, z)->wall_col << 4 |
                       (uint32_t)cell->interior_bit16 << 16 | (uint32_t)cell->wall_row;

        buildingeditor_renderblock(area_block_createbuildinginterior(1, key), 1, x, floor, z, 1.0f, 1.0f, 1.0f,
                                   1.0f);
    } else if (interior == 0) {
        if (block != 0) {
            if ((block & GROUP_MASK) == GROUP_FLAG)
                buildingeditor_renderblock(group_block(block), 0, x, floor, z, 1.0f, 1.0f, 1.0f, 1.0f);
            else
                buildingeditor_renderblock(block & 0xffff, 0, x, floor, z, 1.0f, 1.0f, 1.0f, 1.0f);
        }
    } else if ((interior & GROUP_MASK) == GROUP_FLAG) {
        buildingeditor_renderblock(group_block(interior), 1, x, floor, z, 1.0f, 1.0f, 1.0f, 1.0f);
    }
}

/* The texture of a slot of a cell: the outer layer or the inner layer. */
static int cell_texture(const BuildingCell *cell, int layer, int slot)
{
    /* The slots hold unsigned 16-bit material numbers. */
    int material = layer == 0 ? (uint16_t)cell->tex_outer[slot] : (uint16_t)cell->tex_inner[slot];

    return sr_data_texturelist.material[sr_data_building.material_textures[material]];
}

/* Send one corner of a block quad: shade it by its height and move it to its place. The y of
 * the corner tilts it a quarter to the view and darkens it to the top. x and z are the place
 * of the block: an int cell or a float place. */
static void block_corner(const vec3 *corner, const float *texcoord, double x, double z, float red, float green,
                         float blue, float alpha)
{
    vec3 v;
    float shade;

    copyvector((unsigned int *)&v, (const unsigned int *)corner);
    shade = (float)(1.0 - v.y);
    shade = (float)(shade * 0.5 + 0.5);
    glColor4f((float)(red * shade), (float)(green * shade), (float)(blue * shade), alpha);
    v.x = (float)(x + v.x);
    v.z = (float)(z + v.z);
    v.x = (float)(v.y * 0.25 + v.x);
    v.z = (float)(v.z - v.y * 0.25);
    glTexCoord2fv(texcoord);
    glVertex3fv(&v.x);
}

/* Draw the quads of a block type with the textures that texture() gives for each quad slot. */
static void block_quads(const AreaBlock *type, const BuildingCell *cell, int layer, const int *slots, double x,
                        double z, float red, float green, float blue, float alpha)
{
    int quad, k;

    for (quad = 0; quad < type->num_faces; quad++) {
        int slot = ((const int *)(uintptr_t)type->face_slots)[quad], texture;

        if (slot > LAST_SLOT)
            slot = LAST_SLOT;
        texture = slots != NULL ? sr_data_texturelist.material[sr_data_building.material_textures[slots[slot]]]
                                : cell_texture(cell, layer, slot);
        glBindTexture(GL_TEXTURE_2D, cl_data_texture[texture].gl_name);
        glBegin(GL_QUADS);
        glColor4f(red, green, blue, alpha);
        for (k = 0; k < 4; k++) {
            int corner = ((const int *)(uintptr_t)type->faces)[quad * 4 + k];

            block_corner((const vec3 *)(uintptr_t)type->verts + corner, (const float *)(uintptr_t)type->uvs + corner * 2,
                         x, z, red, green, blue, alpha);
        }
        glEnd();
    }
}

/* Draw a block at a cell. A block number below 0xffff draws its quads with the textures of the
 * cell. 0xffff draws the interior mesh that area_block_createbuildinginterior made, with no
 * face culling. Layer 0 takes the outer textures, layer 1 the inner ones. */
void __cdecl buildingeditor_renderblock(uint32_t block, int layer, int x, int floor, int z, float red, float green,
                                        float blue, float alpha)
{
    const BuildingCell *cell = CELL(x, floor, z);
    int quad, k;

    if (block < INTERIOR_BLOCK) {
        block_quads(&sr_data_area_block[block], cell, layer, NULL, x, z, red, green, blue, alpha);
        return;
    }
    glDisable(GL_CULL_FACE);
    for (quad = 0; quad < sr_data_tempmesh.num_quads; quad++) {
        int slot = sr_data_tempmesh.quad_slots[quad], texture;

        if (slot > LAST_SLOT)
            slot = LAST_SLOT;
        if (slot == 6)
            texture = sr_data_texturelist.material[sr_data_materiallist.flagged_material];
        else if (layer == 0)
            texture = cell_texture(cell, 0, slot);
        else if (slot < LAST_SLOT)
            texture = cell_texture(cell, 1, slot);
        else
            texture = cell_texture(cell, 0, 1);
        glBindTexture(GL_TEXTURE_2D, cl_data_texture[texture].gl_name);
        glBegin(GL_QUADS);
        glColor4f(red, green, blue, alpha);
        for (k = 0; k < 4; k++) {
            int corner = sr_data_tempmesh.quads[quad].verts[k];

            block_corner(&sr_data_tempmesh.verts[corner], &sr_data_tempmesh.uvs[corner].x, x, z, red, green, blue,
                         alpha);
        }
        glEnd();
    }
    glEnable(GL_CULL_FACE);
}

/* Draw a grid of 17 by 17 lines around x, z and the preview of a block there: the front faces
 * filled and the back faces as lines. The textures come from the slots of the editor. */
void __cdecl buildingeditor_renderblock2(uint32_t block, int layer, float x, float z, float red, float green,
                                         float blue, float alpha)
{
    const BuildingEditor *editor = &cl_data_building_editor;
    int i;

    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    for (i = -GRID_HALF; i <= GRID_HALF; i++) {
        glVertex3f((float)(x + -8.0), 0.0f, (float)((double)i + z));
        glVertex3f((float)(x + 8.0), 0.0f, (float)((double)i + z));
        glVertex3f((float)((double)i + x), 0.0f, (float)(z + -8.0));
        glVertex3f((float)((double)i + x), 0.0f, (float)(z + 8.0));
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
    glPolygonMode(GL_FRONT, GL_FILL);
    glPolygonMode(GL_BACK, GL_LINE);
    if (block < INTERIOR_BLOCK)
        block_quads(&sr_data_area_block[block], NULL, layer, layer == 0 ? editor->outer_slots : editor->inner_slots,
                    x, z, red, green, blue, alpha);
    glPolygonMode(GL_FRONT, GL_FILL);
    glPolygonMode(GL_BACK, GL_FILL);
}

/* Put a block in a cell with the textures and settings of the editor. Then set the walls of
 * the four edges of the cell: a wall is 1 where the cell meets an empty cell or the border of
 * the building, and 0 where it meets a cell with a block. A wall of 9 or more does not change.
 * At the far border, a wall gets 1 from size - 2 on, but loses it only below size - 1. */
void __cdecl buildingedit_addblock(int x, int floor, int z, uint32_t block, uint32_t interior)
{
    const BuildingEditor *editor = &cl_data_building_editor;
    BuildingCell *cell = CELL(x, floor, z), *next_z = CELL(x, floor, z + 1), *next_x = CELL(x + 1, floor, z);
    int i;

    cell->outer = block;
    cell->inner = interior;
    for (i = 0; i < 6; i++)
        cell->tex_outer[i] = (int16_t)editor->outer_slots[i];
    cell->tex_outer[6] = (int16_t)((editor->extra_high << 4) + editor->extra_low);
    for (i = 0; i < 8; i++)
        cell->tex_inner[i] = (int16_t)editor->inner_slots[i];
    cell->interior_bit18 = editor->cell_field_10;
    if (cell->wall_row < WALL_FIXED && (z == 0 || CELL(x, floor, z - 1)->outer == 0))
        cell->wall_row = 1;
    if (next_z->wall_row < WALL_FIXED && (z >= sr_data_building.size_z - 2 || next_z->outer == 0))
        next_z->wall_row = 1;
    if (cell->wall_col < WALL_FIXED && (x == 0 || CELL(x - 1, floor, z)->outer == 0))
        cell->wall_col = 1;
    if (next_x->wall_col < WALL_FIXED && (x >= sr_data_building.size_x - 2 || next_x->outer == 0))
        next_x->wall_col = 1;
    if (z > 0 && CELL(x, floor, z - 1)->outer != 0 && cell->wall_row < WALL_FIXED)
        cell->wall_row = 0;
    if (z < sr_data_building.size_z - 1 && next_z->outer != 0 && next_z->wall_row < WALL_FIXED)
        next_z->wall_row = 0;
    if (x > 0 && CELL(x - 1, floor, z)->outer != 0 && cell->wall_col < WALL_FIXED)
        cell->wall_col = 0;
    if (x < sr_data_building.size_x - 1 && next_x->outer != 0 && next_x->wall_col < WALL_FIXED)
        next_x->wall_col = 0;
    cell->interior_bit16 = editor->cell_field_14;
    cell->color = editor->packed_parts[0] * 0x40 + editor->packed_parts[4] + editor->packed_parts[1] * 0x400 +
                  editor->packed_parts[2] * 0x4000 + editor->packed_parts[3] * 0x40000;
}

/* Remove the block of a cell. Then set the walls of the four edges of the cell: a wall is 1
 * where the cell meets a cell with a block, else 0. The block and the interior are cleared on
 * the floor of the editor, not on the floor parameter. The far neighbors count only below
 * 31, also on a larger building. */
void __cdecl buildingedit_deleteblock(int x, int floor, int z)
{
    BuildingCell *cell = CELL(x, floor, z), *next_z = CELL(x, floor, z + 1), *next_x = CELL(x + 1, floor, z);
    BuildingCell *shown = CELL(x, cl_data_building_editor.floor, z);

    shown->outer = 0;
    shown->inner = 0;
    cell->wall_row = 0;
    next_z->wall_row = 0;
    cell->wall_col = 0;
    next_x->wall_col = 0;
    if (z > 0 && CELL(x, floor, z - 1)->outer != 0)
        cell->wall_row = 1;
    if (z < DELETE_LIMIT && next_z->outer != 0)
        next_z->wall_row = 1;
    if (x > 0 && CELL(x - 1, floor, z)->outer != 0)
        cell->wall_col = 1;
    if (x < DELETE_LIMIT && next_x->outer != 0)
        next_x->wall_col = 1;
}

/* Change the material number in the low byte of each texture slot of the cells: 0 clears a
 * slot of the removed material, -1 moves each higher number down by one. The loops also visit
 * the row and the column past the size of the building. */
static void texture_slots_shift(int material, int step)
{
    const Building *b = &sr_data_building;
    int floor, z, x, i;

    for (floor = 0; floor < b->floors; floor++)
        for (z = 0; z <= b->size_z; z++)
            for (x = 0; x <= b->size_x; x++) {
                BuildingCell *cell = CELL(x, floor, z);
                int16_t *slots[2] = {cell->tex_outer, cell->tex_inner};
                int layer;

                for (layer = 0; layer < 2; layer++)
                    for (i = 0; i < 8; i++) {
                        uint16_t value = (uint16_t)slots[layer][i];
                        int low = value & 0xff;

                        if (step < 0 && low == material)
                            slots[layer][i] = 0;
                        else if (step < 0 ? material < low : material <= low)
                            slots[layer][i] = (int16_t)((value & 0xff00) | (low + step));
                    }
            }
}

/* Remove a material from the building: clear the slots that use it, move the higher numbers
 * down, close the gap in the name list and load the textures again. */
void __cdecl buildingedit_removetexture(int material)
{
    Building *b = &sr_data_building;
    int i;

    if (b->num_materials <= 0 || material >= b->num_materials)
        return;
    texture_slots_shift(material, -1);
    b->num_materials--;
    for (i = material; i < b->num_materials; i++)
        strcpy(b->materials + i * MATERIAL_NAME, b->materials + (i + 1) * MATERIAL_NAME);
    building_reloadtextures();
}

/* Insert a material name at a place in the list: move the slot numbers from that place up by
 * one, open a gap in the name list and load the textures again. A low byte of 255 becomes 256:
 * it sets bit 8 of the slot. */
void __cdecl buildingedit_inserttexture(const char *name, int place)
{
    Building *b = &sr_data_building;
    int i;

    if (place < 0 || place > b->num_materials)
        return;
    texture_slots_shift(place, 1);
    for (i = b->num_materials; i > place; i--)
        strcpy(b->materials + i * MATERIAL_NAME, b->materials + (i - 1) * MATERIAL_NAME);
    b->num_materials++;
    strcpy(b->materials + place * MATERIAL_NAME, name);
    building_reloadtextures();
}

/* Draw a block type flat on the screen at x, y with a scale, for a picker. The textures come
 * from the inner slots of the editor, as texture numbers. The y of each corner tilts it and
 * darkens it, as in the editor view. */
void __cdecl buildingeditor_draw_sblock(uint32_t block, float x, float y, float scale_x, float scale_y, float red,
                                        float green, float blue, float alpha)
{
    const AreaBlock *type;
    int quad, k;

    if (block >= INTERIOR_BLOCK)
        return;
    type = &sr_data_area_block[block];
    for (quad = 0; quad < type->num_faces; quad++) {
        int slot = ((const int *)(uintptr_t)type->face_slots)[quad];

        if (slot > LAST_SLOT)
            slot = LAST_SLOT;
        glBindTexture(GL_TEXTURE_2D, cl_data_texture[cl_data_building_editor.inner_slots[slot]].gl_name);
        glBegin(GL_QUADS);
        glColor4f(red, green, blue, alpha);
        for (k = 0; k < 4; k++) {
            int corner = ((const int *)(uintptr_t)type->faces)[quad * 4 + k];
            vec3 v;
            float shade, screen[2];

            copyvector((unsigned int *)&v, (const unsigned int *)((const vec3 *)(uintptr_t)type->verts + corner));
            shade = (float)((float)(1.0 - v.y) * 0.5 + 0.5);
            glColor4f((float)(red * shade), (float)(green * shade), (float)(blue * shade), alpha);
            v.x = (float)(v.y * 0.25 + v.x);
            v.z = (float)(v.z - v.y * 0.25);
            screen[0] = (float)(v.x * scale_x + x);
            screen[1] = (float)(v.z * scale_y + y);
            glTexCoord2fv((const float *)(uintptr_t)type->uvs + corner * 2);
            glVertex2fv(screen);
        }
        glEnd();
    }
}

/* The floor grid: a line for each cell, brighter each fourth line, and the building border in
 * red. */
static void editor_grid(void)
{
    int i;

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    for (i = 0; i < GRID_SIZE; i++) {
        glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
        if ((i & 3) == 0)
            glColor4f(0.75f, 0.75f, 0.75f, 0.5f);
        glVertex3f((float)i, 0.0f, 0.0f);
        glVertex3f((float)i, 0.0f, 64.0f);
        glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
        if ((i & 3) == 0)
            glColor4f(0.75f, 0.75f, 0.75f, 0.5f);
        glVertex3f(0.0f, 0.0f, (float)i);
        glVertex3f(64.0f, 0.0f, (float)i);
    }
    glColor4f(1.0f, 0.0f, 0.0f, 0.75f);
    glVertex3f((float)((double)sr_data_building.size_x + BORDER_GAP), 0.0f, 0.0f);
    glVertex3f((float)((double)sr_data_building.size_x + BORDER_GAP), 0.0f,
               (float)((double)sr_data_building.size_z + BORDER_GAP));
    glColor4f(1.0f, 0.0f, 0.0f, 0.75f);
    glVertex3f(0.0f, 0.0f, (float)((double)sr_data_building.size_z + BORDER_GAP));
    glVertex3f((float)((double)sr_data_building.size_x + BORDER_GAP), 0.0f,
               (float)((double)sr_data_building.size_z + BORDER_GAP));
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

/* The marker of a cell in mode 3: a cell quad, a half quad on the side of the direction, and a
 * small purple quad for the prize. */
static void editor_marker(const BuildingCell *cell, int x, int z)
{
    if (cell->guard_dir != 0) {
        buildingeditor_drawquad(-1, (float)x, (float)z, CELL_FILL, CELL_FILL, 1.0f, 1.0f, 0.0f, 0.25f);
        if (cell->guard_dir == 1)
            buildingeditor_drawquad(-1, (float)x, (float)z, 1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.25f);
        if (cell->guard_dir == 2)
            buildingeditor_drawquad(-1, (float)((double)x + 0.5), (float)z, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.25f);
        if (cell->guard_dir == 3)
            buildingeditor_drawquad(-1, (float)x, (float)((double)z + 0.5), 1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.25f);
        if (cell->guard_dir == 4)
            buildingeditor_drawquad(-1, (float)x, (float)z, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.25f);
    }
    if (cell->prize != 0)
        buildingeditor_drawquad(-1, (float)((double)x + 0.25), (float)((double)z + 0.25), 0.5f, 0.5f, 1.0f, 0.0f,
                                1.0f, 0.5f);
}

/* Draw the building editor view: the grid, the blocks of the floor and the overlay of the
 * mode: the wall cursor, the drag box, the areas or the guard markers. */
void __cdecl buildingedit_render(void)
{
    const BuildingEditor *editor = &cl_data_building_editor;
    int x, z, i;

    editor_grid();
    glDisable(GL_BLEND);
    for (z = 0; z < GRID_SIZE; z++)
        for (x = GRID_SIZE - 1; x >= 0; x--)
            buildingeditor_drawblock(x, editor->floor, z);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(0);
    glEnable(GL_BLEND);
    if (editor->mode == 1) {
        if (editor->wall_direction == 0)
            buildingeditor_drawquad(-1, (float)editor->cursor_x, (float)((double)editor->cursor_z - 0.0625), 1.0f,
                                    0.125f, 1.0f, 1.0f, 1.0f, 0.5f);
        else
            buildingeditor_drawquad(-1, (float)((double)editor->cursor_x - 0.0625), (float)editor->cursor_z, 0.125f,
                                    1.0f, 1.0f, 1.0f, 1.0f, 0.5f);
    }
    if (editor->mode == 0 && editor->drag != 0) {
        int left = editor->drag_end_x < editor->drag_start_x ? editor->drag_end_x : editor->drag_start_x;
        int top = editor->drag_end_z < editor->drag_start_z ? editor->drag_end_z : editor->drag_start_z;
        int wide = abs(editor->drag_start_x - editor->drag_end_x);
        int deep = abs(editor->drag_start_z - editor->drag_end_z);

        for (z = top; z <= top + deep; z++)
            for (x = left; x <= left + wide; x++)
                buildingeditor_drawquad(-1, (float)x, (float)z, CELL_FILL, CELL_FILL, 1.0f, 1.0f, 1.0f, 0.25f);
    }
    if (editor->mode == 2)
        for (i = 0; i < 16; i++) {
            const BuildingArea *area = &sr_data_building.areas[i];

            for (z = area->z; z < area->z + area->size_z; z++)
                for (x = area->x; x < area->x + area->size_x; x++)
                    buildingeditor_drawquad(-1, (float)x, (float)z, CELL_FILL, CELL_FILL, 1.0f, 1.0f, 0.0f, 0.25f);
        }
    if (editor->mode == 3)
        for (z = 0; z < GRID_SIZE; z++)
            for (x = GRID_SIZE - 1; x >= 0; x--)
                editor_marker(CELL(x, editor->floor, z), x, z);
}
