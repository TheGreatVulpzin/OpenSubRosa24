/* The menus of the building editor: the list of building files. */
#include "editor/menus.h"

#include <string.h>

#include "audio/imports.h"
#include "client.h"
#include "menu/draw.h"
#include "menu/items.h"
#include "render/draw2d.h"
#include "sr_runtime.h"

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

/* Game functions of other files. */
#define building_load CL_FN(0x4614e0, void(__cdecl *)(const char *name, int rotate))
#define building_setupfloorplannewer CL_FN(0x462ab0, void(__cdecl *)(void))
#define gameloop CL_FN(0x47b100, void(__cdecl *)(void))
#define listfiles CL_FN(0x4fdde0, void(__cdecl *)(const char *pattern, int list, int flags))
#define removeflieextentions CL_FN(0x4fe200, void(__cdecl *)(int list))
#define checksystemmessages CL_FN(0x5060d0, void(__cdecl *)(void))
#define checkkeyboard CL_FN(0x4fec50, void(__cdecl *)(void))
#define checkmouse CL_FN(0x4feca0, void(__cdecl *)(void))
#define setuptextdisplay CL_FN(0x5079d0, void(__cdecl *)(void))

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
