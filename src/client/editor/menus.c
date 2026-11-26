/* The menus of the building editor: the list of building files and the two pickers of blocks
 * and textures. */
#include "editor/menus.h"
#include "editor/building.h"

#include <string.h>

#include "audio/imports.h"
#include "client.h"
#include "input/key_events.h"
#include "editor/building.h"
#include "menu/draw.h"
#include "menu/items.h"
#include "render/draw2d.h"
#include "sr_runtime.h"
#include "buildings.h"
#include "files.h"
#include "input/keyboard.h"
#include "input/mouse.h"
#include "render/text.h"
#include "render/view.h"
#include "setup/loop.h"

#define KEY_ESCAPE 0x29
#define OPTION_KEY 1
#define OPTION_CHOICE 11
#define ROUND_TIME 0x6270 /* 7 minutes of 3600 ticks */
#define FILE_LIST 0
#define NEW_SIZE 24
#define NEW_FLOORS 4

AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClearColor, (float, float, float, float))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClear, (unsigned))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_GL_SwapWindow, (void *))

/* List the building files without their extensions. */
static void list_buildings(void)
{
    listfiles("data/building/*.sbb", FILE_LIST, 0);
    removeflieextentions(FILE_LIST);
}

/* Make the items: Exit, one button for each building file in columns of 23, and New Building. */
static void building_menu_items(int *chosen, int *create)
{
    const FileListSlot *files = &sr_data_filelist.list[FILE_LIST];
    float x = 4.0f, y = 64.0f;
    int i;

    cl_data_menu_item_count = 0;
    createmenubutton("Exit", 0.0f, 0.0f, 128.0f, 20.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_KEY, KEY_ESCAPE);
    for (i = 0; i < files->count; i++) {
        createmenubutton(files->name[i], x, y, 192.0f, 24.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_CHOICE, chosen, i);
        y = (float)(y + 24.0);
        if (y > 600.0) {
            x = (float)(x + 224.0);
            y = 64.0f;
        }
    }
    createmenubutton("New Building", 1024.0f, 0.0f, 192.0f, 24.0f, 16.0f, 2, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_CHOICE, create, 1);
}

/* Make an empty building of 24 by 24 cells and 4 floors with the one material concrete. */
static void building_new(void)
{
    Building *b = &sr_data_building;

    strcpy(cl_data_building_editor.file, "newbuilding");
    b->size_x = NEW_SIZE;
    b->size_z = NEW_SIZE;
    b->floors = NEW_FLOORS;
    memset(b->cells, 0, sizeof b->cells);
    b->num_materials = 0;
    b->num_blocks = 0;
    strcpy(b->materials, "concrete");
    b->num_materials++;
}

/* --- the picker menus --- */

#define TILE_SIZE 64.0f
#define TILE_STEP 72.0
#define TILE_ROW_END 960.0
#define NAME_SIZE 64
#define BLOCK_TYPE_COUNT 0x400
#define GL_BLEND 0xbe2

enum { TILE_BUILDING_TEXTURE = 0, TILE_BUILDING_BLOCK = 2, TILE_GAME_TEXTURE = 3, TILE_GAME_BLOCK = 4 };

AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glEnable, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glDisable, (unsigned))

/* Add a tile of 64 by 64 at x, y that writes index into target when clicked. Move the place to
 * the next tile: 72 units on, and to the start of a new row at x 960. */
static void tile_add(int kind, float *x, float *y, int *target, int index)
{
    EditorTile *tile = &cl_data_editor_tiles[cl_data_editor_tile_count];

    tile->kind = kind;
    tile->x = *x;
    tile->y = *y;
    tile->width = TILE_SIZE;
    tile->height = TILE_SIZE;
    tile->target = target;
    tile->index = index;
    cl_data_editor_tile_count++;
    *x = (float)(*x + TILE_STEP);
    if (*x >= TILE_ROW_END) {
        *x = 16.0f;
        *y = (float)(*y + TILE_STEP);
    }
}

/* Draw a white frame around a chosen tile. */
static void tile_frame(const EditorTile *tile)
{
    drawbitmap(-1, (float)(tile->x - 4.0), (float)(tile->y - 4.0), (float)(tile->width + 8.0),
               (float)(tile->height + 8.0), 0, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* Draw the name of a tile, centered over it. */
static void tile_name(const EditorTile *tile, const char *name)
{
    drawtext(name, (float)(tile->width * 0.5 + tile->x), (float)(tile->y - 2.0), 12.0f, 9, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* A new left click on a tile of one of the two kinds writes its index into its target. */
static void tile_clicks(int kind_a, int kind_b)
{
    const InputMouse *mouse = &cl_data_input_mouse;
    int i;

    for (i = 0; i < cl_data_editor_tile_count; i++) {
        const EditorTile *t = &cl_data_editor_tiles[i];
        float mx = (float)mouse->x, my = (float)mouse->y;

        if ((t->kind == kind_a || t->kind == kind_b) && t->x <= mx && (double)mx < (double)t->x + t->width &&
            t->y <= my && (double)my < (double)t->y + t->height && mouse->left != 0 &&
            cl_data_input_mouse_previous.left == 0)
            *t->target = t->index;
    }
}

/* The picker loop starts each frame alike: clear the screen and make the Exit item. */
static void picker_start(void)
{
    audio_glClearColor()(0.0f, 0.0f, 0.0f, 0.0f);
    audio_glClear()(0x4000);
    cl_data_menu_item_count = 0;
    createmenubutton("Exit", 0.0f, 0.0f, 128.0f, 20.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_KEY, KEY_ESCAPE);
}

/* The picker loop ends each frame alike: read the input before the tiles draw. */
static void picker_input(void)
{
    checksystemmessages();
    checkkeyboard();
    checkmouse();
    checkmenuitems();
    setuptextdisplay();
}

/* Show the frame with the items and the mouse cursor. */
static void picker_show(void)
{
    drawmenuitems();
    drawmousecursor(cl_data_input_mouse.x, cl_data_input_mouse.y, 16, 1.0f, 1.0f, 1.0f, 1.0f);
    audio_SDL_GL_SwapWindow()(cl_data_audio_window);
}

/* Fill the list of game blocks from the block list of the files: each group of four turns gives
 * one entry, until an empty group or 0x400 groups. */
static void blockmenu_list(void)
{
    int i;

    cl_data_game_block_count = 0;
    for (i = 0; i < 0x400 && sr_data_blocklist.file_blocks[i * 4] > 0; i++) {
        EditorEntry *e = &cl_data_game_blocks[cl_data_game_block_count];
        int block = sr_data_blocklist.file_blocks[i * 4];

        strcpy(e->name, sr_data_area_block[block].name);
        e->index = block;
        cl_data_game_block_count++;
    }
}

/* Add the chosen game block to the block names of the building. Then find the game type of each
 * block name again: the last game block with the same name, or 0. */
static void blockmenu_insert(int game_block)
{
    Building *b = &sr_data_building;
    int i, j;

    strcpy(b->blocks + b->num_blocks * NAME_SIZE, cl_data_game_blocks[game_block].name);
    b->num_blocks++;
    for (i = 0; i < BLOCK_TYPE_COUNT; i++)
        b->block_types[i] = 0;
    for (i = 0; i < b->num_blocks; i++)
        for (j = 0; j < sr_data_blocklist.num_blocks; j++)
            if (strcmp(b->blocks + i * NAME_SIZE, sr_data_area_block[j].name) == 0)
                b->block_types[i] = j;
}

/* The block picker: the blocks of the building at the top, the game blocks below. Insert adds
 * the chosen game block to the building. A right click clears both choices. */
void __cdecl buildingedit_blockmenu(void)
{
    const Building *b = &sr_data_building;
    int building_block = -1, game_block = -1, insert;

    blockmenu_list();
    resetmenuitems();
    while (cl_data_menu_items[0].pressed == 0 && cl_data_audio_menu_back == 0) {
        float x = 16.0f, y = 64.0f;
        int i;

        insert = -1;
        picker_start();
        if (game_block != -1) {
            createmenubutton("Insert", 800.0f, 0.0f, 128.0f, 20.0f, 12.0f, 2, 1.0f, 1.0f, 1.0f, 1.0f);
            setmenuitem(OPTION_CHOICE, &insert, 1);
        }
        cl_data_editor_tile_count = 0;
        for (i = 0; i < b->num_blocks; i++)
            tile_add(TILE_BUILDING_BLOCK, &x, &y, &building_block, i);
        x = 16.0f;
        y = 320.0f;
        for (i = 0; i < cl_data_game_block_count; i++)
            tile_add(TILE_GAME_BLOCK, &x, &y, &game_block, i);
        picker_input();
        audio_glDisable()(GL_BLEND);
        for (i = 0; i < cl_data_editor_tile_count; i++) {
            const EditorTile *t = &cl_data_editor_tiles[i];
            int chosen = t->kind == TILE_BUILDING_BLOCK ? building_block : game_block;

            if (t->kind != TILE_BUILDING_BLOCK && t->kind != TILE_GAME_BLOCK)
                continue;
            if (chosen == t->index)
                tile_frame(t);
            drawbitmap(-1, t->x, t->y, t->width, t->height, 0, 0.5f, 0.5f, 0.5f, 1.0f);
        }
        audio_glEnable()(GL_BLEND);
        for (i = 0; i < cl_data_editor_tile_count; i++) {
            const EditorTile *t = &cl_data_editor_tiles[i];

            if (t->kind == TILE_BUILDING_BLOCK)
                tile_name(t, b->blocks + t->index * NAME_SIZE);
            if (t->kind == TILE_GAME_BLOCK)
                tile_name(t, cl_data_game_blocks[t->index].name);
        }
        picker_show();
        if (insert == 1) {
            if (game_block != -1)
                blockmenu_insert(game_block);
            building_block = -1;
            game_block = -1;
        }
        tile_clicks(TILE_BUILDING_BLOCK, TILE_GAME_BLOCK);
        if (cl_data_input_mouse.right != 0 && cl_data_input_mouse_previous.right == 0) {
            building_block = -1;
            game_block = -1;
        }
    }
    resetmenuitems();
}

enum { TEXTURE_REMOVE = 0, TEXTURE_INSERT = 1, TEXTURE_REPLACE = 2 };

/* Fill the list of game textures: each game material with a name. */
static void texturemenu_list(void)
{
    int i;

    cl_data_game_texture_count = 0;
    for (i = 0; i < sr_data_materiallist.num_materials; i++)
        if (sr_data_materials.records[i].name[0] != '\0') {
            EditorEntry *e = &cl_data_game_textures[cl_data_game_texture_count];

            strcpy(e->name, sr_data_materials.records[i].name);
            e->index = i;
            cl_data_game_texture_count++;
        }
}

/* Make the action items: Remove for a chosen building texture, Insert for a chosen game texture,
 * Replace for both. */
static void texturemenu_items(int building_texture, int game_texture, int *action)
{
    if (building_texture != -1) {
        createmenubutton("Remove", 1024.0f, 0.0f, 128.0f, 20.0f, 12.0f, 2, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_CHOICE, action, TEXTURE_REMOVE);
    }
    if (game_texture != -1) {
        createmenubutton("Insert", 800.0f, 0.0f, 128.0f, 20.0f, 12.0f, 2, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_CHOICE, action, TEXTURE_INSERT);
    }
    if (building_texture != -1 && game_texture != -1) {
        createmenubutton("Replace", 576.0f, 0.0f, 128.0f, 20.0f, 12.0f, 2, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_CHOICE, action, TEXTURE_REPLACE);
    }
}

/* Run the chosen action. Insert puts the game texture before the chosen building texture, or at
 * the end. Replace writes the game texture name over the building texture name. */
static void texturemenu_action(int action, int building_texture, int game_texture)
{
    Building *b = &sr_data_building;

    if (action == TEXTURE_REMOVE)
        buildingedit_removetexture(building_texture);
    if (action == TEXTURE_INSERT)
        buildingedit_inserttexture(cl_data_game_textures[game_texture].name,
                               building_texture == -1 ? b->num_materials : building_texture);
    if (action == TEXTURE_REPLACE && building_texture != -1 && game_texture != -1) {
        strcpy(b->materials + building_texture * NAME_SIZE, cl_data_game_textures[game_texture].name);
        building_reloadtextures();
    }
}

/* The texture picker: the textures of the building at the top, the game textures below. A right
 * click clears both choices. */
void __cdecl buildingedit_texturemenu(void)
{
    const Building *b = &sr_data_building;
    int building_texture = -1, game_texture = -1, action;

    texturemenu_list();
    resetmenuitems();
    while (cl_data_menu_items[0].pressed == 0 && cl_data_audio_menu_back == 0) {
        float x = 16.0f, y = 64.0f;
        int i;

        action = -1;
        picker_start();
        texturemenu_items(building_texture, game_texture, &action);
        cl_data_editor_tile_count = 0;
        for (i = 0; i < b->num_materials; i++)
            tile_add(TILE_BUILDING_TEXTURE, &x, &y, &building_texture, i);
        x = 16.0f;
        y = 320.0f;
        for (i = 0; i < cl_data_game_texture_count; i++)
            tile_add(TILE_GAME_TEXTURE, &x, &y, &game_texture, i);
        picker_input();
        audio_glDisable()(GL_BLEND);
        for (i = 0; i < cl_data_editor_tile_count; i++) {
            const EditorTile *t = &cl_data_editor_tiles[i];
            int material;

            if (t->kind == TILE_BUILDING_TEXTURE) {
                material = b->material_textures[t->index];
                if (building_texture == t->index)
                    tile_frame(t);
            } else if (t->kind == TILE_GAME_TEXTURE) {
                material = cl_data_game_textures[t->index].index;
                if (game_texture == t->index)
                    tile_frame(t);
            } else {
                continue;
            }
            drawbitmap(sr_data_texturelist.material[material], t->x, t->y, t->width, t->height, 0, 1.0f, 1.0f, 1.0f,
                       1.0f);
        }
        audio_glEnable()(GL_BLEND);
        for (i = 0; i < cl_data_editor_tile_count; i++) {
            const EditorTile *t = &cl_data_editor_tiles[i];

            if (t->kind == TILE_BUILDING_TEXTURE)
                tile_name(t, b->materials + t->index * NAME_SIZE);
            if (t->kind == TILE_GAME_TEXTURE)
                tile_name(t, cl_data_game_textures[t->index].name);
        }
        picker_show();
        if (action != -1) {
            texturemenu_action(action, building_texture, game_texture);
            building_texture = -1;
            game_texture = -1;
        }
        tile_clicks(TILE_BUILDING_TEXTURE, TILE_GAME_TEXTURE);
        if (cl_data_input_mouse.right != 0 && cl_data_input_mouse_previous.right == 0) {
            building_texture = -1;
            game_texture = -1;
        }
    }
    resetmenuitems();
}

/* The building editor menu: choose a building file or make a new building, then edit it in a
 * game on this machine. */
void __cdecl buildingedit_menu(void)
{
    int chosen = -1, create = 0;

    sr_data_game.round_time = ROUND_TIME;
    strcpy(sr_data_server.name, sr_data_option.servername);
    sr_data_server.max_players = 1;
    sr_data_server.active = 1;
    sr_data_option.net_on = 0;
    sr_data_option.master_on = 0;
    list_buildings();
    resetmenuitems();
    while (cl_data_menu_items[0].pressed == 0 && cl_data_audio_menu_back == 0) {
        audio_glClearColor()(0.0f, 0.0f, 0.0f, 0.0f);
        audio_glClear()(0x4000);
        building_menu_items(&chosen, &create);
        checksystemmessages();
        checkkeyboard();
        checkmouse();
        checkmenuitems();
        setuptextdisplay();
        drawmenuitems();
        drawmousecursor(cl_data_input_mouse.x, cl_data_input_mouse.y, 16, 1.0f, 1.0f, 1.0f, 1.0f);
        audio_SDL_GL_SwapWindow()(cl_data_audio_window);
        if (chosen != -1) {
            strcpy(cl_data_building_editor.file, sr_data_filelist.list[FILE_LIST].name[chosen]);
            building_load(cl_data_building_editor.file, 0);
            checkmouse();
            gameloop();
            list_buildings();
            chosen = -1;
        }
        if (create != 0) {
            building_new();
            building_setupfloorplannewer();
            checkmouse();
            gameloop();
            list_buildings();
            chosen = -1;
            create = 0;
        }
    }
    resetmenuitems();
}
