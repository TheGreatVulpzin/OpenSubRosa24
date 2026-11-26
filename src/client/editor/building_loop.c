/* The main loop of the building editor.
 *
 * Each frame makes the menu items and the picker tiles, reads the keys and the mouse, draws the
 * floor in work, the block previews and the texts, and then edits the cells under the mouse.
 * The function keys choose the mode: F1 blocks, F2 walls, F3 areas, F4 markers. Exit or Escape
 * saves the building and builds it again in the world.
 */
#include "editor/building_loop.h"

#include <stdlib.h>
#include <string.h>

#include "area.h"
#include "system_libs.h"
#include "buildings.h"
#include "client.h"
#include "input/key_events.h"
#include "editor/building.h"
#include "editor/menus.h"
#include "input/keyboard.h"
#include "menu/draw.h"
#include "menu/items.h"
#include "render/draw2d.h"
#include "render/view.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "input/mouse.h"
#include "render/text.h"

#define g_editor cl_data_building_editor
#define g_view cl_data_building_view
#define g_tiles cl_data_editor_tiles
#define g_tile_count cl_data_editor_tile_count
#define g_building sr_data_building
#define g_mouse cl_data_input_mouse
#define g_mouse_previous cl_data_input_mouse_previous
#define g_keys cl_data_keyboard


#define KEY_G 10
#define KEY_H 11
#define KEY_J 13
#define KEY_MINUS 45
#define KEY_EQUALS 46
#define KEY_LEFT_BRACKET 47
#define KEY_RIGHT_BRACKET 48
#define KEY_F1 58
#define KEY_F3 60
#define KEY_F4 61
#define KEY_F5 62
#define OPTION_KEY 1
#define OPTION_TEXT 4
#define OPTION_CHOICE 11
#define FILE_NAME_LENGTH 28
#define MODE_BLOCKS 0
#define MODE_WALLS 1
#define MODE_AREAS 2
#define MODE_MARKERS 3
#define KIND_NORMAL 0
#define KIND_INTERIOR 1
#define KIND_OUTER 2
#define TILE_TEXTURE 0
#define TILE_BLOCK 2
#define GRID 64
#define LAST_FLOOR 63
#define LAST_PIECE 3
#define LAST_COLOR 15
#define LAST_MARKER 4
#define GROUP_MASK 0xe0000000u
#define GROUP_FLAG 0x80000000u
#define NORMAL_BLOCK 0x40000000u
#define EMPTY_INTERIOR 0xffff
#define BLOCK_INDEX_MASK 0x3ff
#define STEP_MS 16
#define MAX_STEPS 4
#define CAMERA_STEP 0.13333334f
#define CAMERA_FAST_STEP 0.53333336f
#define BUILD_X 36
#define BUILD_Y 6
#define BUILD_Z 36
#define NUM_INTERIOR_MODELS 0x1000
#define GL_LEQUAL 0x0203
#define GL_SCISSOR_TEST 0x0c11
#define GL_COLOR_BUFFER 0x4000
#define GL_CLEAR_MASK 0x4100 /* the color and the depth buffers */

/* The cell of the building at x, floor, z. */
#define CELL(x, floor, z) (&g_building.cells[(floor) * 0x1000 + (z) * 0x40 + (x)])

/* A texture tile of the block mode: its place and the texture slot it sets. */
typedef struct SlotTile { float x, y; int inner; int slot; } SlotTile;

/* The 6 outer slots, then 7 inner slots. Tiles 1 to 4 show the color of a side. */
static const SlotTile slot_tiles[] = {
    {880.0f, 80.0f, 0, 0},   {880.0f, 148.0f, 0, 1},  {812.0f, 80.0f, 0, 2},  {880.0f, 12.0f, 0, 3},
    {948.0f, 80.0f, 0, 4},   {948.0f, 12.0f, 0, 5},   {880.0f, 500.0f, 1, 0}, {880.0f, 568.0f, 1, 3},
    {812.0f, 500.0f, 1, 4},  {948.0f, 500.0f, 1, 2},  {880.0f, 432.0f, 1, 1}, {948.0f, 432.0f, 1, 5},
    {812.0f, 432.0f, 1, 6},
};

/* The wall tools of the wall mode, with their keys 1 to 0. */
static const char *const wall_names[] = {
    "Wall", "Door", "OutDoor", "Window0", "Window1", "Window2", "Window3", "Window4", "Window5", "Window6",
};
static const int wall_tools[] = {9, 10, 12, 1, 2, 3, 4, 5, 6, 7};

/* Find the game material of each material name and the game block of each block name of the
 * building. The last match counts. A name with no match gives 0. */
static void building_find_types(void)
{
    int i, j;

    for (i = 0; i < 256; i++)
        g_building.material_textures[i] = 0;
    for (i = 0; i < g_building.num_materials; i++)
        for (j = 0; j < sr_data_materiallist.num_materials; j++)
            if (strcmp(&g_building.materials[i * 64], sr_data_materials.records[j].name) == 0)
                g_building.material_textures[i] = j;
    for (i = 0; i < 1024; i++)
        g_building.block_types[i] = 0;
    for (i = 0; i < g_building.num_blocks; i++)
        for (j = 0; j < sr_data_blocklist.num_blocks; j++)
            if (strcmp(&g_building.blocks[i * 64], sr_data_area_block[j].name) == 0)
                g_building.block_types[i] = j;
}

/* Add a picker tile. */
static void add_tile(int kind, float x, float y, float size, int *target, int count)
{
    EditorTile *tile = &g_tiles[g_tile_count++];

    tile->kind = kind;
    tile->x = x;
    tile->y = y;
    tile->width = size;
    tile->height = size;
    tile->target = target;
    tile->start = 0;
    tile->count = count;
}

/* Make the items: Exit, the file name, the wall tools in the wall mode. The block mode gets a
 * tile for each texture slot and a tile for the special block. */
static void building_items(void)
{
    BuildingEditor *editor = &g_editor;
    int i;

    cl_data_menu_item_count = 0;
    createmenubutton("Exit", 0.0f, 0.0f, 128.0f, 20.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_KEY, KEY_ESCAPE);
    createmenubutton("Name", 200.0f, 0.0f, 192.0f, 16.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_TEXT, editor->file, FILE_NAME_LENGTH);
    setmenuitem(OPTION_KEY, KEY_RETURN);
    g_tile_count = 0;
    if (editor->mode == MODE_WALLS)
        for (i = 0; i < 10; i++) {
            createmenubutton(wall_names[i], 832.0f, (float)(32 + i * 24), 160.0f, 24.0f, 16.0f, 0, 1.0f, 1.0f,
                             1.0f, 1.0f);
            setmenuitem(OPTION_CHOICE, &editor->tool, wall_tools[i]);
            setmenuitem(OPTION_KEY, KEY_1 + i);
        }
    if (editor->mode != MODE_BLOCKS)
        return;
    for (i = 0; i < (int)(sizeof slot_tiles / sizeof slot_tiles[0]); i++) {
        const SlotTile *t = &slot_tiles[i];
        int *slots = t->inner ? editor->inner_slots : editor->outer_slots;

        add_tile(TILE_TEXTURE, t->x, t->y, 64.0f, &slots[t->slot], g_building.num_materials);
    }
    if (editor->block_kind == KIND_INTERIOR)
        add_tile(TILE_BLOCK, 824.0f, 232.0f, 160.0f, &editor->inner_block, g_building.num_blocks);
    else if (editor->block_kind == KIND_OUTER)
        add_tile(TILE_BLOCK, 824.0f, 232.0f, 160.0f, &editor->outer_block, g_building.num_blocks);
}

/* Draw the floor in work from above. */
static void building_draw_view(void)
{
    const EditorView *view = &g_view;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(1);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_CLEAR_MASK);
    setuporthoviewport(cl_ftoi(view->x), cl_ftoi(view->y), cl_ftoi(view->width), cl_ftoi(view->height), view->zoom,
                       (float)(view->zoom * view->scale), (float)(view->zoom + view->zoom));
    setupviewpoint(&view->camera, view->orientation);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0.125f, 0.125f, 0.125f, 0.0f);
    glClear(GL_COLOR_BUFFER);
    glDisable(GL_SCISSOR_TEST);
    buildingedit_render();
}

/* Draw the block of a block tile in its own viewport. A group block gets the turn. */
static void building_draw_block_tile(const EditorTile *tile)
{
    const BuildingEditor *editor = &g_editor;
    const AreaBlock *type;
    uint32_t block = (uint32_t)g_building.block_types[*tile->target & BLOCK_INDEX_MASK];
    vec3 center;
    float half;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(1);
    glDisable(GL_BLEND);
    if ((uint32_t)sr_data_blocklist.turn_first <= block && block < (uint32_t)sr_data_blocklist.turn_end)
        block += (uint32_t)editor->turn;
    type = &sr_data_area_block[block];
    half = (float)((float)(type->size_x < type->size_z ? type->size_z : type->size_x) * 0.5);
    setuporthoviewport(cl_ftoi(tile->x), cl_ftoi(tile->y), cl_ftoi(tile->width), cl_ftoi(tile->height),
                       (float)(half * 1.25), (float)(half * 1.25), 1024.0f);
    memset(&center, 0, sizeof center);
    setupviewpoint(&center, g_view.orientation);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0.125f, 0.125f, 0.125f, 0.0f);
    glClear(GL_COLOR_BUFFER);
    glDisable(GL_SCISSOR_TEST);
    buildingeditor_renderblock2(block & 0xffff, editor->block_kind == KIND_OUTER ? 0 : 1, -half, -half, 1.0f, 1.0f,
                                1.0f, 1.0f);
}

/* Draw the name of the mode and the state of the editor. */
static void building_draw_texts(void)
{
    static const char *const kind_names[] = {"Block", "Interior Spec", "Exterior Spec"};
    static const char *const mode_names[] = {NULL, "Edge", "Area", "Object"};
    const BuildingEditor *editor = &g_editor;

    if (editor->mode == MODE_BLOCKS && editor->block_kind >= KIND_NORMAL && editor->block_kind <= KIND_OUTER)
        drawtext(kind_names[editor->block_kind], 512.0f, editor->block_kind == KIND_NORMAL ? 4.0f : 24.0f, 20.0f, 1,
                 1.0f, 1.0f, 1.0f, 1.0f);
    if (editor->mode >= MODE_WALLS && editor->mode <= MODE_MARKERS)
        drawtext(mode_names[editor->mode], 512.0f, 4.0f, 20.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f);
    drawtext("B:/i", 4.0f, 20.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, editor->unused_04);
    drawtext("Y:/i", 4.0f, 320.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, editor->floor);
    drawtext("H:/i", 4.0f, 340.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, g_building.floors);
    drawtext(sr_data_buildblocktype[editor->cell_field_10].name, 4.0f, 360.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    drawtext(editor->cell_field_14 == 0 ? "no floor" : "floor", 4.0f, 400.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* Draw the tiles: a block tile shows the name of its block, a texture tile its texture. Tiles 1
 * to 4 take the color of their side. */
static void building_draw_tiles(void)
{
    const BuildingEditor *editor = &g_editor;
    int i;

    glDisable(GL_BLEND);
    for (i = 0; i < g_tile_count; i++) {
        const EditorTile *tile = &g_tiles[i];

        if (tile->kind == TILE_BLOCK)
            drawtext(sr_data_area_block[g_building.block_types[*tile->target]].name,
                     (float)(tile->width * 0.5 + tile->x), tile->y, 16.0f, 9, 1.0f, 1.0f, 1.0f, 1.0f);
        if (tile->kind == TILE_TEXTURE) {
            int texture = sr_data_texturelist.material[g_building.material_textures[*tile->target]];

            if (i < 1 || i > 4) {
                drawbitmap(texture, tile->x, tile->y, tile->width, tile->height, 0, 1.0f, 1.0f, 1.0f, 1.0f);
            } else {
                vec3 color = sr_data_buildingtype.parts[editor->packed_parts[4]].colors[editor->packed_parts[i - 1]];

                drawbitmap(texture, tile->x, tile->y, tile->width, tile->height, 0, color.x, color.y, color.z, 1.0f);
            }
        }
    }
    glEnable(GL_BLEND);
}

/* Move the camera with the A, D, W and S keys, at most 4 steps of 16 ms for each frame. Shift
 * makes the steps 4 times longer. After 2 steps, the clock jumps to the time now. */
static void building_move_camera(uint32_t *last)
{
    vec3 *camera = &g_view.camera;
    float step;
    int steps = 0;

    while (SDL_GetTicks() - *last > STEP_MS - 1 && steps < MAX_STEPS) {
        steps++;
        if (steps < 3)
            *last += STEP_MS;
        else
            *last = SDL_GetTicks();
        step = CAMERA_STEP;
        if (g_keys[KEY_SHIFT] != 0)
            step = CAMERA_FAST_STEP;
        if (cl_data_audio_input_blocked != 0)
            continue;
        if (g_keys[KEY_A] != 0)
            camera->x = (float)(camera->x - step);
        if (g_keys[KEY_D] != 0)
            camera->x = (float)(camera->x + step);
        if (g_keys[KEY_W] != 0)
            camera->z = (float)(camera->z - step);
        if (g_keys[KEY_S] != 0)
            camera->z = (float)(camera->z + step);
    }
}

/* The keys of the editor state. F5 and F6 open the texture and the block pickers. Y switches
 * the floor of new blocks, Q turns new special blocks, minus and equals choose the block piece,
 * the brackets change the side colors. H sets the height of the building to the floor in work
 * plus 1. In the block mode, 1, 2 and 3 choose the kind of new blocks. */
static void building_keys(uint32_t *last)
{
    BuildingEditor *editor = &g_editor;
    int i;

    if (input_pressed(KEY_F1))
        editor->mode = 0;
    if (input_pressed(KEY_F2))
        editor->mode = 1;
    if (input_pressed(KEY_F3))
        editor->mode = 2;
    if (input_pressed(KEY_F4))
        editor->mode = 3;
    if (input_pressed(KEY_F5)) {
        buildingedit_texturemenu();
        *last = SDL_GetTicks();
    }
    if (input_pressed(KEY_F6)) {
        buildingedit_blockmenu();
        *last = SDL_GetTicks();
    }
    if (input_pressed(KEY_Y))
        editor->cell_field_14 ^= 1;
    if (input_pressed(KEY_Q))
        editor->turn = (editor->turn + 1) & 3;
    if (input_pressed(KEY_MINUS))
        editor->cell_field_10--;
    if (input_pressed(KEY_EQUALS))
        editor->cell_field_10++;
    if (input_pressed(KEY_LEFT_BRACKET))
        for (i = 0; i < 4; i++)
            if (editor->packed_parts[i] > 0)
                editor->packed_parts[i]--;
    if (input_pressed(KEY_RIGHT_BRACKET))
        for (i = 0; i < 4; i++)
            if (editor->packed_parts[i] < LAST_COLOR)
                editor->packed_parts[i]++;
    if (editor->cell_field_10 < 0)
        editor->cell_field_10 = 0;
    if (editor->cell_field_10 > LAST_PIECE)
        editor->cell_field_10 = LAST_PIECE;
    if (g_keys[KEY_H] != 0)
        g_building.floors = editor->floor + 1;
    if (editor->mode == MODE_BLOCKS) {
        if (g_keys[KEY_1] != 0)
            editor->block_kind = KIND_NORMAL;
        if (g_keys[KEY_2] != 0)
            editor->block_kind = KIND_INTERIOR;
        if (g_keys[KEY_3] != 0)
            editor->block_kind = KIND_OUTER;
    }
}

/* Copy the cells of floor from to floor to: x and z go up to the size of the building. */
static void copy_floor(int to, int from)
{
    int x, z;

    for (z = 0; z <= g_building.size_z; z++)
        for (x = 0; x <= g_building.size_x; x++)
            memcpy(CELL(x, to, z), CELL(x, from, z), sizeof(BuildingCell));
}

/* F goes one floor down and R one floor up. With shift, the new floor gets a copy of the floor
 * it came from. F lowers the floor also below 0: the clamp comes later. */
static void building_floor_keys(void)
{
    BuildingEditor *editor = &g_editor;

    if (input_pressed(KEY_F)) {
        editor->floor--;
        if (editor->floor >= 0 && g_keys[KEY_SHIFT] != 0)
            copy_floor(editor->floor, editor->floor + 1);
    }
    if (input_pressed(KEY_R) && editor->floor < LAST_FLOOR) {
        editor->floor++;
        if (editor->floor < LAST_FLOOR && g_keys[KEY_SHIFT] != 0)
            copy_floor(editor->floor, editor->floor - 1);
    }
}

/* The block and the interior word of a new block of the chosen kind. */
static void new_block(uint32_t *block, uint32_t *interior)
{
    const BuildingEditor *editor = &g_editor;
    uint32_t turn = (uint32_t)editor->turn << 24;

    *block = NORMAL_BLOCK;
    if (editor->block_kind == KIND_OUTER)
        *block = turn | (uint32_t)editor->outer_block | GROUP_FLAG;
    *interior = 0;
    if (editor->block_kind == KIND_NORMAL)
        *interior = EMPTY_INTERIOR;
    if (editor->block_kind == KIND_INTERIOR)
        *interior = turn | (uint32_t)editor->inner_block | GROUP_FLAG;
}

/* The keys of a drag box: E adds blocks, T sets the texture slots, U sets the colors and Delete
 * removes the blocks of each cell in the box. */
static void building_box_keys(void)
{
    BuildingEditor *editor = &g_editor;
    int x0 = editor->drag_end_x < editor->drag_start_x ? editor->drag_end_x : editor->drag_start_x;
    int z0 = editor->drag_end_z < editor->drag_start_z ? editor->drag_end_z : editor->drag_start_z;
    int width = abs(editor->drag_start_x - editor->drag_end_x);
    int depth = abs(editor->drag_start_z - editor->drag_end_z);
    int x, z, k;

    if (cl_data_audio_input_blocked != 0)
        return;
    if (input_pressed(KEY_E))
        for (z = z0; z <= z0 + depth; z++)
            for (x = x0; x <= x0 + width; x++) {
                uint32_t block, interior;

                new_block(&block, &interior);
                buildingedit_addblock(x, editor->floor, z, block, interior);
            }
    if (input_pressed(KEY_T))
        for (z = z0; z <= z0 + depth; z++)
            for (x = x0; x <= x0 + width; x++) {
                BuildingCell *cell = CELL(x, editor->floor, z);

                for (k = 0; k < 8; k++) {
                    cell->tex_outer[k] = (int16_t)editor->outer_slots[k];
                    cell->tex_inner[k] = (int16_t)editor->inner_slots[k];
                }
            }
    if (input_pressed(KEY_U))
        for (z = z0; z <= z0 + depth; z++)
            for (x = x0; x <= x0 + width; x++)
                CELL(x, editor->floor, z)->color = editor->packed_parts[0] * 0x40 + editor->packed_parts[4] +
                    editor->packed_parts[1] * 0x400 + editor->packed_parts[2] * 0x4000 +
                    editor->packed_parts[3] * 0x40000;
    if (input_pressed(KEY_DELETE))
        for (z = z0; z <= z0 + depth; z++)
            for (x = x0; x <= x0 + width; x++)
                buildingedit_deleteblock(x, editor->floor, z);
}

/* 1 when the mouse is in the rectangle. */
static int mouse_in(float x, float y, float width, float height)
{
    float mx = (float)g_mouse.x, my = (float)g_mouse.y;

    return x <= mx && mx < x + width && y <= my && my < y + height;
}

/* The marker mode: a left click counts the marker of the cell from 1 to 4 and then to 1 again.
 * A right click clears it. With key 1 held the click changes the prize, else the guard
 * direction. */
static void building_edit_marker(BuildingCell *cell)
{
    int *value = g_keys[KEY_1] == 0 ? &cell->guard_dir : &cell->prize;

    if (g_mouse.right != 0)
        *value = 0;
    if (g_mouse.left != 0 && g_mouse_previous.left == 0) {
        ++*value;
        if ((uint32_t)*value > LAST_MARKER)
            *value = 1;
    }
}

/* The wall mode: a left click sets the wall of the chosen direction to the tool, a right click
 * clears it. */
static void building_edit_wall(BuildingCell *cell)
{
    const BuildingEditor *editor = &g_editor;
    int *wall = editor->wall_direction == 0 ? &cell->wall_row : &cell->wall_col;

    if (g_mouse.right != 0)
        *wall = 0;
    if (g_mouse.left != 0)
        *wall = editor->tool;
}

/* G takes the blocks, the texture slots and the colors of the cell into the editor. */
static void building_pick_cell(const BuildingCell *cell)
{
    BuildingEditor *editor = &g_editor;
    int k;

    if ((cell->outer & GROUP_MASK) == GROUP_FLAG)
        editor->outer_block = (int)(cell->outer & BLOCK_INDEX_MASK);
    if ((cell->inner & GROUP_MASK) == GROUP_FLAG)
        editor->inner_block = (int)(cell->inner & BLOCK_INDEX_MASK);
    editor->cell_field_10 = cell->interior_bit18;
    for (k = 0; k < 6; k++)
        editor->outer_slots[k] = (uint16_t)cell->tex_outer[k];
    for (k = 0; k < 8; k++)
        editor->inner_slots[k] = (uint16_t)cell->tex_inner[k];
    editor->packed_parts[4] = (uint32_t)cell->color & 0x3f;
    editor->packed_parts[0] = (uint32_t)cell->color >> 6 & 0xf;
    editor->packed_parts[1] = (uint32_t)cell->color >> 10 & 0xf;
    editor->packed_parts[2] = (uint32_t)cell->color >> 14 & 0xf;
    editor->packed_parts[3] = (uint32_t)cell->color >> 18 & 0xf;
}

/* The block mode. G picks the cell. A right click removes the block, or only the interior for
 * an interior block. A left click adds a block. Shift and the left button drag a box, and the
 * box stays after the release of the right button. */
static void building_edit_block(int x, int z)
{
    BuildingEditor *editor = &g_editor;
    BuildingCell *cell = CELL(x, editor->floor, z);

    if (cl_data_audio_input_blocked == 0 && g_keys[KEY_G] != 0 && (cell->outer != 0 || cell->inner != 0))
        building_pick_cell(cell);
    if (g_mouse.right != 0 && editor->drag == 0) {
        if (editor->block_kind == KIND_NORMAL || editor->block_kind == KIND_OUTER)
            buildingedit_deleteblock(x, editor->floor, z);
        else
            cell->inner = EMPTY_INTERIOR;
    }
    if (g_mouse.right == 0 && g_mouse_previous.right != 0)
        editor->drag = 0;
    if (g_mouse.left == 0)
        return;
    if (g_keys[KEY_SHIFT] == 0) {
        uint32_t block, interior;

        new_block(&block, &interior);
        buildingedit_addblock(x, editor->floor, z, block, interior);
        return;
    }
    editor->drag = 1;
    if (g_mouse_previous.left == 0) {
        editor->drag_start_x = x;
        editor->drag_start_z = z;
    }
    editor->drag_end_x = x;
    editor->drag_end_z = z;
}

/* Edit the cell under the mouse. The markers and the blocks need a cell inside the building,
 * the walls also take the cells on the far edges. */
static void building_edit(int x, int z)
{
    const BuildingEditor *editor = &g_editor;
    const EditorView *view = &g_view;

    if (!mouse_in(view->x, view->y, view->width, view->height))
        return;
    if (editor->mode == MODE_MARKERS && z >= 0 && z < g_building.size_z && x >= 0 && x < g_building.size_x)
        building_edit_marker(CELL(x, editor->floor, z));
    if (editor->mode == MODE_WALLS && z >= 0 && z <= g_building.size_z && x >= 0 && x <= g_building.size_x)
        building_edit_wall(CELL(x, editor->floor, z));
    if (editor->mode == MODE_BLOCKS && z >= 0 && z < g_building.size_z && x >= 0 && x < g_building.size_x)
        building_edit_block(x, z);
}

/* Save the building, remove its blocks from the world, build it again and free all interior
 * meshes. */
static void building_finish(void)
{
    int x, y, z, i;

    building_save(g_editor.file);
    for (y = 0; y < GRID; y++)
        for (z = 0; z < GRID; z++)
            for (x = 0; x < GRID; x++)
                area_deleteblock(0, x + BUILD_X, y + BUILD_Y, z + BUILD_Z);
    building_setupbuild();
    building_setupfloorplannewer();
    building_buildfloorplan(0, BUILD_X, BUILD_Y, BUILD_Z, g_building.build_size_x, g_building.build_size_z,
                            g_building.build_floors, 0);
    sr_data_interior.count = 0;
    for (i = 0; i < NUM_INTERIOR_MODELS; i++) {
        InteriorMesh *mesh = &((InteriorMesh *)&sr_data_interior)[i];

        if (mesh->sector != -1)
            sr_data_area_sector.records[mesh->sector].interior_models[mesh->slot] = -1;
        mesh->sector = -1;
    }
}

/* The wheel over a tile steps the value of the tile. The value wraps from start to count - 1.
 * With shift, a texture tile of the outer sides (1 to 4) or of the inner sides (7 to 10) gives
 * its value to the other tiles of its group. */
static void building_wheel_tiles(void)
{
    int i, k;

    for (i = 0; i < g_tile_count; i++) {
        EditorTile *tile = &g_tiles[i];
        int changed = 0;

        if (!mouse_in(tile->x, tile->y, tile->width, tile->height))
            continue;
        for (; g_mouse.wheel_up > 0; g_mouse.wheel_up--) {
            ++*tile->target;
            changed = 1;
        }
        for (; g_mouse.wheel_down > 0; g_mouse.wheel_down--) {
            --*tile->target;
            changed = 1;
        }
        if (*tile->target < tile->start)
            *tile->target = tile->count - 1;
        if (*tile->target >= tile->count)
            *tile->target = tile->start;
        if (!changed || g_keys[KEY_SHIFT] == 0 || tile->kind != TILE_TEXTURE)
            continue;
        if (i > 0 && i < 5)
            for (k = 1; k < 5; k++)
                *g_tiles[k].target = *tile->target;
        if (i > 6 && i < 11)
            for (k = 7; k < 11; k++)
                *g_tiles[k].target = *tile->target;
    }
}

/* The distance of a fraction to the nearest whole number. */
static float edge_distance(float fraction)
{
    return 0.5 <= fraction ? (float)(1.0 - fraction) : fraction;
}

/* Find the cell under the mouse and edit the view. J sets the size of the building to the cell
 * plus 1. The wall mode takes the edge nearest to the mouse when no button is down: the wall
 * goes along z when the mouse is nearer a line of whole x, else along x. The area mode sets the start of the chosen
 * area with a left click and its size while the button stays down. The wheel changes the zoom
 * from 2 to 256. */
static void building_view_mouse(void)
{
    BuildingEditor *editor = &g_editor;
    EditorView *view = &g_view;
    float x, z, fx, fz;

    x = (float)((float)((float)((double)g_mouse.x - (view->width * 0.5 + view->x)) * view->zoom) /
                (view->width * 0.5));
    z = (float)((float)((float)((double)g_mouse.y - (view->height * 0.5 + view->y)) * view->zoom) /
                (view->width * 0.5));
    x = (float)(x + view->camera.x);
    z = (float)(z + view->camera.z);
    editor->cursor_x = cl_ftoi(x);
    editor->cursor_z = cl_ftoi(z);
    fx = (float)(x - (float)editor->cursor_x);
    fz = (float)(z - (float)editor->cursor_z);
    if (cl_data_audio_input_blocked == 0 && g_keys[KEY_J] != 0) {
        g_building.size_x = editor->cursor_x + 1;
        g_building.size_z = editor->cursor_z + 1;
    }
    if (editor->mode == MODE_BLOCKS || editor->mode == MODE_AREAS || editor->mode == MODE_MARKERS) {
        if (editor->cursor_x < 0)
            editor->cursor_x = 0;
        if (editor->cursor_x > g_building.size_x - 1)
            editor->cursor_x = g_building.size_x - 1;
        if (editor->cursor_z < 0)
            editor->cursor_z = 0;
        if (editor->cursor_z > g_building.size_z - 1)
            editor->cursor_z = g_building.size_z - 1;
    }
    if (editor->mode == MODE_WALLS) {
        if (editor->cursor_x < 0)
            editor->cursor_x = 0;
        if (editor->cursor_x > g_building.size_x)
            editor->cursor_x = g_building.size_x;
        if (editor->cursor_z < 0)
            editor->cursor_z = 0;
        if (editor->cursor_z > g_building.size_z)
            editor->cursor_z = g_building.size_z;
        if (g_mouse.left == 0 && g_mouse.right == 0)
            editor->wall_direction = edge_distance(fx) <= edge_distance(fz) ? 1 : 0;
        if (editor->wall_direction == 0 && 0.5 <= fz)
            editor->cursor_z++;
        if (editor->wall_direction == 1 && 0.5 <= fx)
            editor->cursor_x++;
    }
    if (editor->mode == MODE_AREAS) {
        BuildingArea *area = &g_building.areas[editor->area];

        if (g_mouse.left != 0 && g_mouse_previous.left == 0) {
            area->x = editor->cursor_x;
            area->y = editor->floor;
            area->z = editor->cursor_z;
        }
        if (g_mouse.left != 0) {
            area->size_x = editor->cursor_x - area->x;
            area->size_y = editor->floor - area->y;
            area->size_z = editor->cursor_z - area->z;
        }
    }
    for (; g_mouse.wheel_up > 0; g_mouse.wheel_up--)
        view->zoom = (float)(view->zoom - 1.0);
    for (; g_mouse.wheel_down > 0; g_mouse.wheel_down--)
        view->zoom = (float)(view->zoom + 1.0);
    if (view->zoom < 2.0f)
        view->zoom = 2.0f;
    if (view->zoom > 256.0f)
        view->zoom = 256.0f;
}

/* Read the mouse of the building editor: first the tiles, then the view. Wheel steps out of the
 * tiles and the view are lost. */
void __cdecl building_checkeditor(void)
{
    const EditorView *view = &g_view;

    building_wheel_tiles();
    if (mouse_in(view->x, view->y, view->width, view->height))
        building_view_mouse();
    if (g_mouse.wheel_up > 0)
        g_mouse.wheel_up = 0;
    if (g_mouse.wheel_down > 0)
        g_mouse.wheel_down = 0;
}

/* Run the building editor until Exit or Escape. A left button that is down as the editor starts
 * counts only after its release. */
void __cdecl building_editorloop(void)
{
    BuildingEditor *editor = &g_editor;
    uint32_t last;
    int i;

    building_find_types();
    cl_data_building_click_wait = 0;
    checkmouse();
    if (g_mouse.left != 0)
        cl_data_building_click_wait = 1;
    last = SDL_GetTicks();
    resetmenuitems();
    while (cl_data_menu_items[0].pressed == 0 && cl_data_audio_menu_back == 0) {
        int cursor_x, cursor_z;

        building_items();
        checksystemmessages();
        checkkeyboard();
        checkmouse();
        if (cl_data_building_click_wait != 0) {
            if (g_mouse.left == 0)
                cl_data_building_click_wait = 0;
            else
                g_mouse.left = 0;
        }
        checkmenuitems();
        building_checkeditor();
        building_draw_view();
        for (i = 0; i < g_tile_count; i++)
            if (g_tiles[i].kind == TILE_BLOCK)
                building_draw_block_tile(&g_tiles[i]);
        setuptextdisplay();
        building_draw_texts();
        building_draw_tiles();
        drawmenuitems();
        drawmousecursor(g_mouse.x, g_mouse.y, 16, 1.0f, 1.0f, 1.0f, 1.0f);
        SDL_GL_SwapWindow(cl_data_audio_window);
        building_move_camera(&last);
        if (cl_data_audio_input_blocked == 0) {
            building_keys(&last);
            building_floor_keys();
        }
        if (editor->drag != 0)
            building_box_keys();
        cursor_x = editor->cursor_x;
        cursor_z = editor->cursor_z;
        if (editor->floor < 0)
            editor->floor = 0;
        if (editor->floor > LAST_FLOOR)
            editor->floor = LAST_FLOOR;
        building_edit(cursor_x, cursor_z);
    }
    building_finish();
    resetmenuitems();
}
