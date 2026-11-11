/* The identifiers of the models, textures, sounds and materials, and the game tables.
 *
 * setupgame hands out the numbers of the models, textures and sounds (assign_asset_ids), then
 * the numbers of the materials, and fills the game tables. setupclient then builds what is
 * behind each number.
 */
#include "setup/identifiers.h"

#include <string.h>

#include "asset_ids.h"
#include "client.h"
#include "files.h"
#include "sr_os.h"
#include "sr_runtime.h"

#define g_materiallist sr_data_materiallist
#define g_materials cl_data_material_records
#define g_filelist sr_data_filelist

/* Game functions of other files. */
#define removeflieextentions CL_FN(0x4fe200, void(__cdecl *)(int list))
#define area_createsblocks CL_FN(0x4395a0, void(__cdecl *)(void))
#define setupbuildingtypes CL_FN(0x455bc0, void(__cdecl *)(void))
#define setupbullettypes CL_FN(0x464110, void(__cdecl *)(void))
#define setupcityobjecttypes CL_FN(0x466810, void(__cdecl *)(void))
#define setupbody CL_FN(0x48e8d0, void(__cdecl *)(void))
#define setupitemtypes CL_FN(0x4951c0, void(__cdecl *)(void))
#define setupobjecttypes CL_FN(0x4eb730, void(__cdecl *)(void))

enum { NUM_MATERIALS = 256, NUM_PRESETS = 4, FIRST_PRESET_MATERIAL = 21 };

/* The text of the texture folder. */
static const char *const texture_folder = "texture";

/* Name a material and give it the next number. */
static int add_material(const char *name)
{
    int id = g_materiallist.num_materials++;

    if (name)
        strcpy(g_materials[id].name, name);
    return id;
}

static void assign_material_ids(void)
{
    int next = FIRST_PRESET_MATERIAL;
    int i;

    strcpy(g_materials[0].name, "concrete");
    g_materiallist.num_materials = next;
    for (i = 0; i < NUM_PRESETS; ++i) {
        int flat;

        if (i == 1)
            strcpy(g_materials[g_materiallist.num_materials].name, "brick");
        if (i == 2)
            strcpy(g_materials[g_materiallist.num_materials].name, "cinder");
        g_materiallist.presets[i * 10] = g_materiallist.num_materials++;
        flat = g_materiallist.num_materials++;
        g_materiallist.presets[i * 10 + 2] = flat;
        g_materials[flat].flags = 1;
    }
    g_materiallist.preset_end = g_materiallist.num_materials++;
    strcpy(g_materials[g_materiallist.num_materials].name, "grass");
    g_materiallist.tile_material = g_materiallist.num_materials;
    g_materiallist.street_x_left = g_materiallist.num_materials + 1;
    g_materiallist.street_x_right = g_materiallist.num_materials + 2;
    g_materiallist.street_z_left = g_materiallist.num_materials + 3;
    g_materiallist.street_z_right = g_materiallist.num_materials + 4;
    g_materiallist.carpet = g_materiallist.num_materials + 5;
    g_materiallist.num_materials += 6;
    strcpy(g_materials[g_materiallist.carpet].name, "carpet");
    g_materiallist.flagged_material = g_materiallist.num_materials;
    g_materiallist.misc_2a0 = g_materiallist.num_materials + 1;
    g_materiallist.misc_2a4 = g_materiallist.num_materials + 2;
    g_materiallist.num_materials += 3;
    strcpy(g_materials[g_materiallist.num_materials].name, "ceiling0");
    g_materiallist.ceiling = g_materiallist.num_materials;
    g_materiallist.misc_2c4 = g_materiallist.num_materials + 1;
    g_materiallist.misc_2b0 = g_materiallist.num_materials + 2;
    g_materiallist.num_materials += 3;
    strcpy(g_materials[g_materiallist.num_materials].name, "marble");
    g_materiallist.marble = g_materiallist.num_materials++;
    strcpy(g_materials[g_materiallist.num_materials].name, "sign");
    g_materiallist.sign = g_materiallist.num_materials++;
}

/* One material for each picture of the texture folder. */
static void add_file_materials(void)
{
    int in_folder;
    int i;

    os_chdir("data");
    in_folder = os_chdir(texture_folder);
    listfiles("*.png", 0, 0);
    removeflieextentions(0);
    if (in_folder == 0) {
        os_chdir("..");
        os_chdir("..");
    }
    for (i = 0; i < g_filelist.list[0].count; ++i) {
        int id = add_material(g_filelist.list[0].name[i]);

        g_materiallist.file_materials[i] = id;
    }
}

/* The light and friction of every material. Four materials have their own values. */
static void set_material_parameters(void)
{
    int i;

    for (i = 0; i < NUM_MATERIALS; ++i) {
        g_materials[i].parameters[0] = 0.25f;
        g_materials[i].parameters[1] = 0.25f;
        g_materials[i].parameters[2] = 1.0f;
        g_materials[i].parameters[3] = 1.2f;
    }
    g_materials[g_materiallist.tile_material].parameters[3] = 0.9f;
    g_materials[17].parameters[0] = 0.25f;
    g_materials[17].parameters[1] = 0.25f;
    g_materials[1].parameters[0] = 0.25f;
    g_materials[1].parameters[1] = 0.25f;
    g_materials[20].parameters[0] = 0.25f;
    g_materials[20].parameters[1] = 0.25f;
    g_materials[1].flags = 1;
    g_materials[20].flags = 1;
    g_materials[17].flags = 1;
    g_materials[g_materiallist.flagged_material].flags = 1;
    g_materials[g_materiallist.preset_end].flags = 1;
}

void __cdecl setupgame(void)
{
    assign_asset_ids();
    assign_material_ids();
    add_file_materials();
    setupbuildingtypes();
    area_createsblocks();
    setupbody();
    setupbullettypes();
    setupitemtypes();
    setupobjecttypes();
    setupcityobjecttypes();
    set_material_parameters();
}
