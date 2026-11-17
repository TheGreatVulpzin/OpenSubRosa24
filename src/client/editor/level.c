/* The level editor: the player draws the streets, the sidewalk tiles and the buildings of the
 * city on a map of 512 by 512 cells. */
#include "editor/level.h"

#include <stdlib.h>
#include <string.h>

#include "audio/imports.h"
#include "client.h"
#include "menu/draw.h"
#include "menu/items.h"
#include "render/draw2d.h"
#include "render/lists.h"
#include "render/map.h"
#include "render/view.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"

#define PI_F 3.1415927410125732f
#define MAP_SIZE 512
#define TILE_ROW 1024
#define KEY_A 0x04
#define KEY_D 0x07
#define KEY_E 0x08
#define KEY_S 0x16
#define KEY_W 0x1a
#define KEY_RETURN 0x28
#define KEY_ESCAPE 0x29
#define KEY_LEFT_SHIFT 0xe1
#define OPTION_KEY 1
#define OPTION_TEXT 4
#define OPTION_CHOICE 11
#define FILE_LIST 0
#define NAME_LENGTH 24
#define MODE_STREETS 0
#define MODE_TILES 1
#define MODE_BUILDINGS 2
#define STEP_MS 16
#define MAX_STEPS 4
#define CAMERA_STEP 0.13333334f
#define CAMERA_FAST_STEP 0.53333336f
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_POINT_SMOOTH 0x0b10
#define GL_LINE_SMOOTH 0x0b20
#define GL_CLEAR_MASK 0x4100 /* the color and the depth buffers */

AUDIO_IMPORT("SDL2.DLL", __cdecl, uint32_t, SDL_GetTicks, (void))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_GL_SwapWindow, (void *))

GL_IMPORT(glClearColor, (float red, float green, float blue, float alpha), (red, green, blue, alpha))
GL_IMPORT(glClear, (uint32_t mask), (mask))
GL_IMPORT(glColor3f, (float red, float green, float blue), (red, green, blue))
GL_IMPORT(glVertex3f, (float x, float y, float z), (x, y, z))
GL_IMPORT(glLineWidth, (float width), (width))
GL_IMPORT(glPointSize, (float size), (size))

/* Game functions of other files. */
#define building_load CL_FN(0x4614e0, void(__cdecl *)(const char *name, int rotate))
#define listfiles CL_FN(0x4fdde0, void(__cdecl *)(const char *pattern, int list, int flags))
#define removeflieextentions CL_FN(0x4fe200, void(__cdecl *)(int list))
#define checksystemmessages CL_FN(0x5060d0, void(__cdecl *)(void))
#define checkkeyboard CL_FN(0x4fec50, void(__cdecl *)(void))
#define checkmouse CL_FN(0x4feca0, void(__cdecl *)(void))
#define level_checkeditor CL_FN(0x49f400, void(__cdecl *)(void))
#define leveleditor_save CL_FN(0x4a0740, void(__cdecl *)(void))
#define createlevel CL_FN(0x49b740, void(__cdecl *)(void))
#define drawtext CL_FN(0x506b30, void(__cdecl *)(const char *text, float x, float y, float size, uint32_t flags, \
                                                 float red, float green, float blue, float alpha, ...))

static const char *const tile_kind_names[] = {
    "Normal", "Wall Up", "Wall Right", "Wall Down", "Wall Left",
    "Corner Up Left", "Corner Up Right", "Corner Down Right", "Corner Down Left",
};

/* The mark of each tile kind: four corners as parts of the cell, x then z. A kind with no
 * mark has 0 in its first corner count. */
typedef struct TileMark {
    int count;
    float red, green, blue;
    float corner[4][2];
} TileMark;

static const TileMark tile_marks[16] = {
    [2] = {4, 0.375f, 0.375f, 0.0f, {{0.25f, 0.0f}, {0.75f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}},
    [3] = {4, 0.375f, 0.375f, 0.0f, {{0.0f, 0.0f}, {1.0f, 0.25f}, {1.0f, 0.75f}, {0.0f, 1.0f}}},
    [4] = {4, 0.375f, 0.375f, 0.0f, {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.75f, 1.0f}, {0.25f, 1.0f}}},
    [5] = {4, 0.375f, 0.375f, 0.0f, {{0.0f, 0.25f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.75f}}},
    [6] = {4, 0.375f, 0.375f, 0.375f, {{0.25f, 0.25f}, {0.75f, 0.25f}, {0.75f, 0.75f}, {0.25f, 0.75f}}},
    [8] = {4, 0.375f, 0.375f, 0.375f, {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.25f}, {0.0f, 0.25f}}},
    [9] = {4, 0.375f, 0.375f, 0.375f, {{0.75f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.75f, 1.0f}}},
    [10] = {4, 0.375f, 0.375f, 0.375f, {{0.0f, 0.75f}, {1.0f, 0.75f}, {1.0f, 1.0f}, {0.0f, 1.0f}}},
    [11] = {4, 0.375f, 0.375f, 0.375f, {{0.0f, 0.0f}, {0.25f, 0.0f}, {0.25f, 1.0f}, {0.0f, 1.0f}}},
    [12] = {4, 0.375f, 0.375f, 0.375f, {{0.0f, 0.0f}, {0.25f, 0.0f}, {0.25f, 0.25f}, {0.0f, 0.25f}}},
    [13] = {4, 0.375f, 0.375f, 0.375f, {{0.75f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.25f}, {0.75f, 0.25f}}},
    [14] = {4, 0.375f, 0.375f, 0.375f, {{0.75f, 0.75f}, {1.0f, 0.75f}, {1.0f, 1.0f}, {0.75f, 1.0f}}},
    [15] = {4, 0.375f, 0.375f, 0.375f, {{0.0f, 0.75f}, {0.25f, 0.75f}, {0.25f, 1.0f}, {0.0f, 1.0f}}},
};

/* The type index of a placed building is in the 4 bytes before its record. */
static int *placed_type(int index)
{
    return (int *)((uint8_t *)&sr_data_level_editor.buildings[index] - sizeof(int));
}

/* The width and the depth of a placed building on the map. Turns 1 and 3 swap them. */
static void placed_size(int index, int *width, int *depth)
{
    const LevelBuilding *building = &sr_data_level_editor.buildings[index];
    const LevelBuildingType *type = &sr_data_level_editor.types[*placed_type(index)];

    if (building->turns == 0 || building->turns == 2) {
        *width = type->size_x;
        *depth = type->size_z;
    } else {
        *width = type->size_z;
        *depth = type->size_x;
    }
}

/* Load each building file, keep its name and size, and give each placed building with the
 * same name its type index. */
static void level_load_types(void)
{
    LevelEditor *level = &sr_data_level_editor;
    const FileListSlot *files = &sr_data_filelist.list[FILE_LIST];
    int i, j;

    listfiles("data/building/*.sbb", FILE_LIST, 0);
    removeflieextentions(FILE_LIST);
    for (i = 0; i < files->count; i++) {
        LevelBuildingType *type = &level->types[i];

        building_load(files->name[i], 1);
        strcpy(type->name, files->name[i]);
        type->size_x = sr_data_building.size_x;
        type->size_z = sr_data_building.size_z;
        type->floors = sr_data_building.floors;
        for (j = 0; j < level->num_buildings; j++) {
            if (strcmp(level->buildings[j].name, type->name) == 0)
                *placed_type(j) = i;
        }
    }
}

/* Make the items: Exit, then the items of the mode. */
static void level_items(void)
{
    LevelEditor *level = &sr_data_level_editor;
    const FileListSlot *files = &sr_data_filelist.list[FILE_LIST];
    float y;
    int i;

    cl_data_menu_item_count = 0;
    createmenubutton("Exit", 0.0f, 0.0f, 128.0f, 20.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_KEY, KEY_ESCAPE);
    if (level->mode == MODE_BUILDINGS) {
        y = 64.0f;
        for (i = 0; i < files->count; i++) {
            createmenubutton(files->name[i], 816.0f, y, 192.0f, 24.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
            setmenuitem(OPTION_CHOICE, &level->place_type, i);
            y = (float)(y + 24.0);
        }
    }
    if (level->mode == MODE_STREETS && level->chosen_street != -1) {
        createmenubutton("Name", 800.0f, 400.0f, 192.0f, 16.0f, 12.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_TEXT, level->streets[level->chosen_street].name, NAME_LENGTH);
        setmenuitem(OPTION_KEY, KEY_RETURN);
    }
    if (level->mode == MODE_TILES) {
        for (i = 0; i < 9; i++) {
            createmenubutton(tile_kind_names[i], 832.0f, (float)(64 + i * 16), 128.0f, 16.0f, 12.0f, 0, 1.0f, 1.0f,
                             1.0f, 1.0f);
            setmenuitem(OPTION_CHOICE, &level->tile_kind, i);
        }
    }
}

/* Set the view of the map from above. */
static void level_view(void)
{
    const EditorView *view = &cl_data_level_view;

    setuporthoviewport(cl_ftoi(view->x), cl_ftoi(view->y), cl_ftoi(view->width), cl_ftoi(view->height), view->zoom,
                       (float)(view->zoom * view->scale), (float)(view->zoom + view->zoom));
}

/* Draw the lines of the cells. Each 16th line is lighter. */
static void level_draw_grid(void)
{
    int i;

    glBegin(GL_LINES);
    for (i = 0; i < MAP_SIZE; i++) {
        glColor4f(0.375f, 0.375f, 0.375f, 0.5f);
        if ((i & 0xf) == 0)
            glColor4f(0.75f, 0.75f, 0.75f, 0.5f);
        glVertex3f((float)i, 0.0f, 0.0f);
        glVertex3f((float)i, 0.0f, 512.0f);
        glColor4f(0.375f, 0.375f, 0.375f, 0.5f);
        if ((i & 0xf) == 0)
            glColor4f(0.75f, 0.75f, 0.75f, 0.5f);
        glVertex3f(0.0f, 0.0f, (float)i);
        glVertex3f(512.0f, 0.0f, (float)i);
    }
    glEnd();
}

/* Draw each street: the middle line, and the two sides of the lanes. Axis 0 moves the sides
 * on z, axis 1 on x. */
static void level_draw_streets(void)
{
    const LevelEditor *level = &sr_data_level_editor;
    int i;

    glEnable(GL_LINE_SMOOTH);
    glLineWidth((float)(128.0 / cl_data_level_view.zoom));
    glBegin(GL_LINES);
    glColor3f(0.5f, 0.5f, 0.5f);
    for (i = 0; i < level->num_streets; i++) {
        const LevelStreet *street = &level->streets[i];
        const LevelIntersection *a = &level->intersections[street->a];
        const LevelIntersection *b = &level->intersections[street->b];

        glVertex3f((float)a->x, (float)a->y, (float)a->z);
        glVertex3f((float)b->x, (float)b->y, (float)b->z);
        if (street->axis == 0) {
            glVertex3f((float)a->x, (float)a->y, (float)(a->z - street->lanes_a));
            glVertex3f((float)b->x, (float)b->y, (float)(b->z - street->lanes_a));
            glVertex3f((float)a->x, (float)a->y, (float)(a->z + street->lanes_b));
            glVertex3f((float)b->x, (float)b->y, (float)(b->z + street->lanes_b));
        }
        if (street->axis == 1) {
            glVertex3f((float)(a->x - street->lanes_a), (float)a->y, (float)a->z);
            glVertex3f((float)(b->x - street->lanes_a), (float)b->y, (float)b->z);
            glVertex3f((float)(a->x + street->lanes_b), (float)a->y, (float)a->z);
            glVertex3f((float)(b->x + street->lanes_b), (float)b->y, (float)b->z);
        }
    }
    glEnd();
    glDisable(GL_LINE_SMOOTH);
    glLineWidth(1.0f);
}

/* Draw the intersections as round points. The two chosen ones have other colors. */
static void level_draw_points(void)
{
    const LevelEditor *level = &sr_data_level_editor;
    int i;

    glEnable(GL_POINT_SMOOTH);
    glPointSize((float)(512.0 / cl_data_level_view.zoom));
    glBegin(GL_POINTS);
    for (i = 0; i < level->num_intersections; i++) {
        const LevelIntersection *point = &level->intersections[i];

        glColor3f(0.0f, 0.75f, 0.0f);
        if (i == level->chosen_b)
            glColor3f(0.75f, 0.75f, 0.0f);
        if (i == level->chosen_a)
            glColor3f(0.75f, 0.75f, 0.75f);
        glVertex3f((float)point->x, (float)point->y, (float)point->z);
    }
    glEnd();
    glDisable(GL_POINT_SMOOTH);
    glPointSize(1.0f);
}

/* Draw each tile that is not 0: a cell with the color of its material, then the mark of its
 * kind. In the tile mode, draw the drag box too. */
static void level_draw_tiles(void)
{
    const LevelEditor *level = &sr_data_level_editor;
    int x, z, k;

    glBegin(GL_QUADS);
    for (z = 0; z < MAP_SIZE; z++) {
        for (x = 0; x < MAP_SIZE; x++) {
            uint8_t tile = level->tiles[z * TILE_ROW + x];
            const TileMark *mark = &tile_marks[tile >> 4];

            if (tile == 0)
                continue;
            if ((tile & 0xf) == 0)
                glColor3f(0.75f, 0.75f, 0.75f);
            else if ((tile & 0xf) == 1)
                glColor3f(0.25f, 0.25f, 0.25f);
            else
                glColor3f(0.0f, 0.25f, 0.0f);
            glVertex3f((float)x, 0.0f, (float)z);
            glVertex3f((float)((float)x + 1.0), 0.0f, (float)z);
            glVertex3f((float)((float)x + 1.0), 0.0f, (float)((float)z + 1.0));
            glVertex3f((float)x, 0.0f, (float)((float)z + 1.0));
            if (mark->count == 0)
                continue;
            glColor3f(mark->red, mark->green, mark->blue);
            for (k = 0; k < 4; k++)
                glVertex3f((float)((float)x + mark->corner[k][0]), 0.0f, (float)((float)z + mark->corner[k][1]));
        }
    }
    if (level->mode == MODE_TILES && level->dragging != 0) {
        int left = level->drag_x1 < level->drag_x0 ? level->drag_x1 : level->drag_x0;
        int top = level->drag_z1 < level->drag_z0 ? level->drag_z1 : level->drag_z0;
        int width = abs(level->drag_x0 - level->drag_x1);
        int depth = abs(level->drag_z0 - level->drag_z1);

        glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
        glVertex3f((float)left, 0.0f, (float)top);
        glVertex3f((float)(left + width), 0.0f, (float)top);
        glVertex3f((float)(left + width), 0.0f, (float)(top + depth));
        glVertex3f((float)left, 0.0f, (float)(top + depth));
    }
    glEnd();
}

/* Draw the ground plan of a placed building. */
static void level_draw_plan(int index)
{
    const LevelBuilding *building = &sr_data_level_editor.buildings[index];
    int width, depth;

    placed_size(index, &width, &depth);
    glVertex3f((float)building->x, 0.0f, (float)building->z);
    glVertex3f((float)(building->x + width), 0.0f, (float)building->z);
    glVertex3f((float)(building->x + width), 0.0f, (float)(building->z + depth));
    glVertex3f((float)building->x, 0.0f, (float)(building->z + depth));
}

/* Draw the placed buildings. In the building mode, draw the next building too: the record
 * after the last one. It is brighter while the E key is down. */
static void level_draw_buildings(void)
{
    const LevelEditor *level = &sr_data_level_editor;
    int i;

    glBegin(GL_QUADS);
    for (i = 0; i < level->num_buildings; i++) {
        glColor4f(0.5f, 0.5f, 0.5f, 0.5f);
        level_draw_plan(i);
    }
    if (level->mode == MODE_BUILDINGS) {
        glColor4f(0.5f, 0.5f, 0.5f, 0.125f);
        if (cl_data_audio_input_blocked == 0 && cl_data_audio_keys[KEY_E] != 0)
            glColor4f(0.5f, 0.5f, 0.5f, 0.25f);
        level_draw_plan(level->num_buildings);
    }
    glEnd();
}

/* Write the names of the streets at their middles, and in the building mode the names of the
 * buildings at their middles. The view turns by pi so that the text is upright. */
static void level_draw_labels(void)
{
    const LevelEditor *level = &sr_data_level_editor;
    vec3 position;
    vec3 rotation[3];
    float x, z;
    int i, width, depth;

    level_view();
    position.x = cl_data_level_view.camera.x;
    position.y = cl_data_level_view.camera.z;
    position.z = 0.0f;
    resetorientation(rotation);
    rotateorientation(rotation, &rotation[0], PI_F);
    setupviewpoint(&position, rotation);
    for (i = 0; i < level->num_streets; i++) {
        const LevelStreet *street = &level->streets[i];
        const LevelIntersection *a = &level->intersections[street->a];
        const LevelIntersection *b = &level->intersections[street->b];

        x = (float)((float)(a->x + b->x) * 0.5);
        z = (float)((float)(a->z + b->z) * 0.5);
        drawtext(street->name, x, z, 2.0f, 5, 1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (level->mode != MODE_BUILDINGS)
        return;
    for (i = 0; i <= level->num_buildings; i++) {
        const LevelBuilding *building = &level->buildings[i];

        placed_size(i, &width, &depth);
        x = (float)((float)width * 0.5 + (float)building->x);
        z = (float)((float)depth * 0.5 + (float)building->z);
        drawtext(building->name, x, z, 2.0f, 5, 1.0f, 1.0f, 0.0f, 1.0f);
    }
}

/* Write the name of the mode. The building mode also shows the type and the turns of the next
 * building. */
static void level_draw_texts(void)
{
    const LevelEditor *level = &sr_data_level_editor;

    setuptextdisplay();
    if (level->mode == MODE_STREETS)
        drawtext("Street", 512.0f, 4.0f, 20.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f);
    if (level->mode == MODE_TILES)
        drawtext("Sidewalk", 512.0f, 4.0f, 20.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f);
    if (level->mode == MODE_BUILDINGS) {
        drawtext("Building", 512.0f, 4.0f, 20.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f);
        drawtext("B:/i", 4.0f, 64.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, level->place_type);
        drawtext("R:/i", 4.0f, 84.0f, 20.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, level->place_turns);
    }
}

/* Move the camera with the A, D, W and S keys, at most 4 steps of 16 ms for each frame. Shift
 * makes the steps 4 times longer. After 2 steps, the clock jumps to the time now. */
static void level_move_camera(uint32_t *last)
{
    vec3 *camera = &cl_data_level_view.camera;
    float step;
    int steps = 0;

    while (audio_SDL_GetTicks()() - *last > STEP_MS - 1 && steps < MAX_STEPS) {
        steps++;
        if (steps < 3)
            *last += STEP_MS;
        else
            *last = audio_SDL_GetTicks()();
        step = CAMERA_STEP;
        if (cl_data_audio_keys[KEY_LEFT_SHIFT] != 0)
            step = CAMERA_FAST_STEP;
        if (cl_data_audio_input_blocked != 0)
            continue;
        if (cl_data_audio_keys[KEY_A] != 0)
            camera->x = (float)(camera->x - step);
        if (cl_data_audio_keys[KEY_D] != 0)
            camera->x = (float)(camera->x + step);
        if (cl_data_audio_keys[KEY_W] != 0)
            camera->z = (float)(camera->z - step);
        if (cl_data_audio_keys[KEY_S] != 0)
            camera->z = (float)(camera->z + step);
    }
}

/* Run the level editor until Exit or Escape. At the end, save the level and build the city of
 * it again. */
void __cdecl leveleditor_loop(void)
{
    uint32_t last;

    sr_data_level_editor.chosen_a = -1;
    sr_data_level_editor.chosen_b = -1;
    level_load_types();
    last = audio_SDL_GetTicks()();
    resetmenuitems();
    while (cl_data_menu_items[0].pressed == 0 && cl_data_audio_menu_back == 0) {
        level_items();
        checksystemmessages();
        checkkeyboard();
        checkmouse();
        checkmenuitems();
        level_checkeditor();
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_CLEAR_MASK);
        level_view();
        setupviewpoint(&cl_data_level_view.camera, cl_data_level_view.orientation);
        glDisable(GL_TEXTURE_2D);
        level_draw_grid();
        level_draw_streets();
        level_draw_points();
        level_draw_tiles();
        level_draw_buildings();
        glEnable(GL_TEXTURE_2D);
        level_draw_labels();
        level_draw_texts();
        drawmenuitems();
        drawmousecursor(cl_data_input_mouse.x, cl_data_input_mouse.y, 16, 1.0f, 1.0f, 1.0f, 1.0f);
        audio_SDL_GL_SwapWindow()(cl_data_audio_window);
        level_move_camera(&last);
    }
    leveleditor_save();
    createlevel();
    rendermap();
    render_buildareasectorshadow();
    resetmenuitems();
}
