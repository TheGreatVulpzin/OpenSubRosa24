/* The identifiers of the models, textures, sounds and materials, and the game tables.
 *
 * Every model, texture and sound has a number. The lists below hold the number of each thing
 * by name. setupgame hands the numbers out in a fixed order, so a saved number always means
 * the same thing. setupclient then builds what is behind each number.
 */
#include "setup/identifiers.h"

#include <string.h>

#include "client.h"
#include "files.h"
#include "sr_os.h"
#include "sr_runtime.h"

#define g_modellist cl_data_modellist
#define g_texturelist cl_data_texturelist
#define g_soundlist sr_data_soundlist
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

enum {
    NUM_MODEL_SLOTS = 32,
    NUM_HUMAN_MODELS = 256,
    NUM_MATERIAL_TEXTURES = 256,
    NUM_HUMAN_TEXTURES = 4,
    NUM_PAINT_TEXTURES = 8,
    NUM_HUMAN_LOOK_TEXTURES = 512,
    NUM_STREET_SLOTS = 32,
    NUM_CASH_TEXTURES = 8,
    NUM_RICOCHETS = 8,
    NUM_DIGITS = 10,
    NUM_MATERIALS = 256,
    NUM_PRESETS = 4,
    FIRST_PRESET_MATERIAL = 21
};

/* The text of the texture folder. */
static const char *const texture_folder = "texture";

static int *texture_slot(int offset)
{
    return (int *)((char *)&g_texturelist + offset);
}

static int *sound_slot(int offset)
{
    return (int *)((char *)&g_soundlist + offset);
}

static void assign_model_ids(void)
{
    int next = 0;
    int i;

    for (i = 0; i < NUM_MODEL_SLOTS; ++i)
        g_modellist.bullet[i] = next++;
    for (i = 0; i < NUM_MODEL_SLOTS; ++i)
        g_modellist.item[i] = next++;
    for (i = 0; i < NUM_MODEL_SLOTS; ++i)
        g_modellist.wheel[i] = next++;
    for (i = 0; i < NUM_HUMAN_MODELS; ++i)
        g_modellist.human[i] = next++;
    for (i = 0; i < NUM_MODEL_SLOTS; ++i) {
        g_modellist.ammo[i] = next++;
        g_modellist.ammo_b[i] = next++;
    }
    for (i = 0; i < NUM_MODEL_SLOTS; ++i)
        g_modellist.city_object[i] = next++;
    g_modellist.sunglasses = next;
    g_modellist.head = next + 1;
    g_modellist.foot = next + 2;
    g_modellist.hair = next + 3;
    next += 4;
    for (i = 0; i < NUM_MODEL_SLOTS; ++i)
        g_modellist.sprite[i] = next++;
    g_modellist.rotor = next++;
    g_modellist.count = next;
}

static void assign_texture_ids(void)
{
    /* Slots that no name covers yet: the renderer reads them by offset. */
    static const int first_slots[] = {0x404, 0x408, 0x40c, 0x410, 0x414};
    static const int middle_slots[] = {0x10fc, 0x110c};
    int next = 0;
    int i;

    for (i = 0; i < NUM_MATERIAL_TEXTURES; ++i)
        g_texturelist.material[i] = next++;
    for (i = 0; i < 5; ++i)
        *texture_slot(first_slots[i]) = next++;
    g_texturelist.disc = next;
    g_texturelist.rough_disc = next + 1;
    g_texturelist.plain_green = next + 2;
    next += 3;
    for (i = 0; i < NUM_HUMAN_TEXTURES; ++i)
        g_texturelist.human[i] = next++;
    for (i = 0; i < NUM_PAINT_TEXTURES; ++i)
        g_texturelist.paint[i] = next++;
    for (i = 0; i < NUM_HUMAN_LOOK_TEXTURES; ++i)
        g_texturelist.human_textures[i] = next++;
    g_texturelist.grey_disc = next++;
    for (i = 0; i < NUM_CASH_TEXTURES; ++i)
        g_texturelist.cash[i] = next++;
    for (i = 0; i < NUM_STREET_SLOTS; ++i)
        g_texturelist.street[i] = next++;
    for (i = 0; i < 6; ++i)
        g_texturelist.panel[i] = next++;
    g_texturelist.half_grey = next++;
    g_texturelist.half_brown = next++;
    g_texturelist.plain_orange = next++;
    *texture_slot(middle_slots[0]) = next++;
    g_texturelist.mic = next++;
    g_texturelist.twitter = next++;
    g_texturelist.logo = next++;
    *texture_slot(middle_slots[1]) = next++;
    g_texturelist.random_vectors = next++;
    g_texturelist.landscape_normal = next++;
    g_texturelist.landscape_detail = next++;
    g_texturelist.desert = next++;
    g_texturelist.newspaper = next++;
    g_texturelist.count = next;
}

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

static void assign_sound_ids(void)
{
    /* Slots that no name covers yet. */
    static const int shot_slots[] = {0x10, 0x14, 0x18, 0x1c, 0x20};
    static const int gap_slots[] = {0xa8, 0xac, 0xb0, 0xb4};
    int next = 0;
    int i;

    g_soundlist.num_sounds = 0;
    g_soundlist.shot_magnum = next++;
    g_soundlist.shot_smg = next++;
    g_soundlist.shot_rifle = next++;
    for (i = 0; i < 5; ++i)
        *sound_slot(shot_slots[i]) = next++;
    g_soundlist.engine = next++;
    *sound_slot(0x28) = next++;
    g_soundlist.tire = next++;
    for (i = 0; i < NUM_RICOCHETS; ++i)
        g_soundlist.ricochet[i] = next++;
    g_soundlist.crash_small = next++;
    g_soundlist.crash = next++;
    g_soundlist.hit_body = next++;
    g_soundlist.humanhit = next++;
    g_soundlist.hit_metal[0] = next++;
    g_soundlist.hit_metal[1] = next++;
    g_soundlist.glass_break = next++;
    *sound_slot(0xbc) = next++;
    g_soundlist.phone_ring = next++;
    for (i = 0; i < NUM_DIGITS; ++i)
        g_soundlist.digit[i] = next++;
    g_soundlist.magazine = next++;
    g_soundlist.shell_hit = next++;
    g_soundlist.gear = next++;
    g_soundlist.engine_large = next++;
    g_soundlist.engine_train = next++;
    for (i = 0; i < 4; ++i)
        *sound_slot(gap_slots[i]) = next++;
    g_soundlist.num_sounds = next;
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
    assign_model_ids();
    assign_texture_ids();
    assign_material_ids();
    add_file_materials();
    assign_sound_ids();
    setupbuildingtypes();
    area_createsblocks();
    setupbody();
    setupbullettypes();
    setupitemtypes();
    setupobjecttypes();
    setupcityobjecttypes();
    set_material_parameters();
}
