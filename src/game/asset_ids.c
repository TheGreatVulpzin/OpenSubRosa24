/* The identifiers of the models, textures and sounds.
 *
 * Every model, texture and sound has a number. The lists hold the number of each thing by
 * name. The numbers go out in a fixed order, so a number always means the same thing.
 */
#include "asset_ids.h"

#include "sr_runtime.h"

#define g_modellist sr_data_modellist
#define g_texturelist sr_data_texturelist
#define g_soundlist sr_data_soundlist

enum {
    NUM_MODEL_SLOTS = 32,
    NUM_HUMAN_MODELS = 256,
    NUM_MATERIAL_TEXTURES = 256,
    NUM_HUMAN_TEXTURES = 4,
    NUM_PAINT_TEXTURES = 8,
    NUM_HUMAN_LOOK_TEXTURES = 512,
    NUM_STREET_SLOTS = 32,
    NUM_PANEL_TEXTURES = 6,
    NUM_CASH_TEXTURES = 8,
    NUM_RICOCHETS = 8,
    NUM_DIGITS = 10
};

/* Slots that no name covers yet. The renderer reads them by offset. */
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
    g_modellist.sunglasses = next++;
    g_modellist.head = next++;
    g_modellist.foot = next++;
    g_modellist.hair = next++;
    for (i = 0; i < NUM_MODEL_SLOTS; ++i)
        g_modellist.sprite[i] = next++;
    g_modellist.rotor = next++;
    g_modellist.count = next;
}

static void assign_texture_ids(void)
{
    static const int first_slots[] = {0x404, 0x408, 0x40c, 0x410, 0x414};
    int next = 0;
    int i;

    for (i = 0; i < NUM_MATERIAL_TEXTURES; ++i)
        g_texturelist.material[i] = next++;
    for (i = 0; i < 5; ++i)
        *texture_slot(first_slots[i]) = next++;
    g_texturelist.disc = next++;
    g_texturelist.rough_disc = next++;
    g_texturelist.plain_green = next++;
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
    for (i = 0; i < NUM_PANEL_TEXTURES; ++i)
        g_texturelist.panel[i] = next++;
    g_texturelist.half_grey = next++;
    g_texturelist.half_brown = next++;
    g_texturelist.plain_orange = next++;
    *texture_slot(0x10fc) = next++;
    g_texturelist.mic = next++;
    g_texturelist.twitter = next++;
    g_texturelist.logo = next++;
    *texture_slot(0x110c) = next++;
    g_texturelist.random_vectors = next++;
    g_texturelist.landscape_normal = next++;
    g_texturelist.landscape_detail = next++;
    g_texturelist.desert = next++;
    g_texturelist.newspaper = next++;
    g_texturelist.count = next;
}

static void assign_sound_ids(void)
{
    static const int shot_slots[] = {0x10, 0x14, 0x18, 0x1c, 0x20};
    static const int gap_slots[] = {0xa8, 0xac, 0xb0, 0xb4};
    int next = 0;
    int i;

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

/* Give a number to every model, texture and sound. */
void assign_asset_ids(void)
{
    assign_model_ids();
    assign_texture_ids();
    assign_sound_ids();
}
