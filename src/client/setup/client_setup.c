/* The start of the client: it makes every model, texture and sound of the game.
 *
 * setupgame gives each model, texture and sound an identifier. This function then builds the
 * thing behind each identifier. The calls run in a fixed order.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "items.h"
#include "setup/identifiers.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/landscape.h"
#include "textures/models.h"
#include "textures/opengl.h"
#include "textures/sphere.h"
#include "textures/textures.h"

#define g_modellist sr_data_modellist
#define g_texturelist sr_data_texturelist
#define g_textures cl_data_texture
#define g_soundlist sr_data_soundlist
#define g_materiallist sr_data_materiallist
#define g_materials cl_data_material_records
#define g_logo ((const int *)sr_data_cslogo)

/* Game functions of other files. */
#define setupframetextures CL_FN(0x4742f0, void(__cdecl *)(void))
#define setuptexture CL_FN(0x50ac40, void(__cdecl *)(int texture))
#define loadtexture CL_FN(0x50a0f0, int(__cdecl *)(int texture, const char *path, int mipmap, int wrap_s, int wrap_t, int mag_filter, int min_filter))
#define loadtexturesetalpha CL_FN(0x50a2f0, int(__cdecl *)(int texture, const char *path, int mipmap, int wrap_s, int wrap_t, int mag_filter, int min_filter, unsigned alpha))
#define loadwav CL_FN(0x433af0, void(__cdecl *)(int sound, const char *path, float volume))
#define createwav CL_FN(0x44f7d0, void(__cdecl *)(int sound, float volume))
#define createbulletmodel CL_FN(0x4ad5d0, void(__cdecl *)(int model, int type))
#define createammoshapemodel CL_FN(0x4af7c0, void(__cdecl *)(int model, int shape, int variant))
#define createsunglassmodel CL_FN(0x4affb0, void(__cdecl *)(int model))
#define creategunmodel2 CL_FN(0x4b3250, void(__cdecl *)(int model, int item))
#define createcashmodel CL_FN(0x4b3900, void(__cdecl *)(int model, int item))
#define createbriefcasemodel CL_FN(0x4b3fc0, void(__cdecl *)(int model, float x, float y, float z))
#define createboxmodel CL_FN(0x4b41d0, void(__cdecl *)(int model, float x, float y, float z))
#define createwheelmodel CL_FN(0x4b4520, void(__cdecl *)(int model, float radius, float width))
#define createfootmodel CL_FN(0x4b4710, void(__cdecl *)(int model, float x, float y, float z))
#define createspritemodel CL_FN(0x4b4820, void(__cdecl *)(int model, int kind))
#define createrotormodel CL_FN(0x4b4930, void(__cdecl *)(int model, float x, float y, float z))
#define createheadmodel CL_FN(0x4b54c0, void(__cdecl *)(int model))
#define createcellphonemodel CL_FN(0x4bf260, void(__cdecl *)(int model, float x, float y, float z))
#define createkeymodel CL_FN(0x4bf820, void(__cdecl *)(int model))
#define createbillmodel CL_FN(0x4bfbf0, void(__cdecl *)(int model, float x, float y, float z))

enum {
    NUM_BULLET_MODELS = 16,
    NUM_AMMO_SHAPES = 4,
    NUM_CITY_OBJECTS = 10,
    NUM_FILE_MATERIALS = 128,
    NUM_HUMAN_TEXTURES = 4,
    NUM_PAINT_TEXTURES = 8,
    NUM_STREET_TEXTURES = 10,
    NUM_PANEL_TEXTURES = 6,
    NUM_RICOCHETS = 8,
    NUM_DIGITS = 10,
    NUM_BILL_TEXTURES = 7,
    LOGO_WIDTH = 32,
    LOGO_HEIGHT = 16
};

/* A texture without mipmaps, from a generator. */
static void plain_texture(int texture)
{
    generatetexture(texture, 1, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_NEAREST);
}

/* The item models. Each one is a box, or a call of a builder with the item. */
static void make_item_models(void)
{
    const int *item = g_modellist.item;

    creategunmodel2(item[ITEM_AK47], ITEM_AK47);
    creategunmodel2(item[ITEM_M16], ITEM_M16);
    creategunmodel2(item[ITEM_MP5], ITEM_MP5);
    createboxmodel(item[ITEM_AK47_MAGAZINE], 0.03125f, 0.0625f, 0.0625f);
    createboxmodel(item[ITEM_M16_MAGAZINE], 0.03125f, 0.0625f, 0.0625f);
    createboxmodel(item[ITEM_MP5_MAGAZINE], 0.03125f, 0.0625f, 0.0625f);
    createboxmodel(item[ITEM_9MM_MAGAZINE], 0.03125f, 0.0625f, 0.0625f);
    createboxmodel(item[ITEM_UZI_MAGAZINE], 0.03125f, 0.078125f, 0.0625f);
    createammoshapemodel(item[ITEM_45_BULLETS], 3, 0);
    createcashmodel(item[ITEM_MAGNUM], ITEM_MAGNUM);
    createcashmodel(item[ITEM_UZI], ITEM_UZI);
    createcashmodel(item[ITEM_9MM], ITEM_9MM);
    createbriefcasemodel(item[ITEM_BRIEFCASE], 0.375f, 0.125f, 0.25f);
    createbriefcasemodel(item[ITEM_OPEN_BRIEFCASE], 0.375f, 0.125f, 0.25f);
}

/* The remaining item models and the models of the wheels. */
static void make_other_item_models(void)
{
    const int *item = g_modellist.item;
    int i;

    createwheelmodel(g_modellist.wheel[0], 0.25f, 0.375f);
    createwheelmodel(g_modellist.wheel[1], 0.25f, 0.225f);
    for (i = ITEM_BLACK_DISK; i <= ITEM_RED_DISK; ++i)
        createboxmodel(item[i], 0.1875f, 0.03125f, 0.1875f);
    createboxmodel(item[ITEM_BANDAGE], 0.125f, 0.0625f, 0.125f);
    createkeymodel(item[ITEM_KEY]);
    createboxmodel(item[ITEM_DOOR], 0.5f, 1.0625f, 0.03125f);
    createbillmodel(item[ITEM_CASH], 0.078125f, 0.03125f, 0.1875f);
    createbillmodel(item[ITEM_CASH_2], 0.068359375f, 0.01171875f, 0.1640625f);
    createboxmodel(item[ITEM_NEWSPAPER], 0.375f, 0.015625f, 0.375f);
    createrotormodel(g_modellist.rotor, 0.125f, 0.03125f, 2.5f);
    createcellphonemodel(item[ITEM_CELL_PHONE], 0.0625f, 0.1875f, 0.0625f);
    createsunglassmodel(g_modellist.sunglasses);
}

static void make_models(void)
{
    int i;

    for (i = 0; i < NUM_BULLET_MODELS; ++i)
        createbulletmodel(g_modellist.bullet[i], i);
    make_item_models();
    make_other_item_models();
    createhumanmodel(g_modellist.human[0]);
    for (i = 0; i < NUM_AMMO_SHAPES; ++i) {
        createammoshapemodel(g_modellist.ammo[i], i, 0);
        createammoshapemodel(g_modellist.ammo_b[i], i, 1);
    }
    for (i = 0; i < NUM_CITY_OBJECTS; ++i)
        createmodel_cityobject(g_modellist.city_object[i], i);
    createheadmodel(g_modellist.head);
    createhairmodel(g_modellist.hair);
    createfootmodel(g_modellist.foot, 0.09375f, 0.0625f, 0.1875f);
    createspritemodel(g_modellist.sprite[1], 1);
}

/* The textures that a generator draws. */
static void make_generated_textures(void)
{
    int i;

    for (i = 0; i < g_materiallist.num_materials; ++i)
        generatematerialtexture(i, 1, GL_REPEAT, GL_REPEAT, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    plain_texture(g_texturelist.disc);
    plain_texture(g_texturelist.rough_disc);
    plain_texture(g_texturelist.plain_green);
    for (i = 0; i < NUM_HUMAN_TEXTURES; ++i)
        plain_texture(g_texturelist.human[i]);
    for (i = 0; i < NUM_PAINT_TEXTURES; ++i)
        plain_texture(g_texturelist.paint[i]);
    plain_texture(g_texturelist.grey_disc);
    for (i = 0; i < NUM_STREET_TEXTURES; ++i)
        plain_texture(g_texturelist.street[i]);
    for (i = 0; i < NUM_PANEL_TEXTURES; ++i)
        plain_texture(g_texturelist.panel[i]);
    plain_texture(g_texturelist.half_grey);
    plain_texture(g_texturelist.half_brown);
    plain_texture(g_texturelist.plain_orange);
    landscape_generatedetail(1, GL_REPEAT, GL_REPEAT, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
}

/* The textures that come from files. The table of random vectors comes after the desert. */
static void load_file_textures(void)
{
    static const char *const bill_files[NUM_BILL_TEXTURES] = {
        "texture/cash_dollar.png", "texture/cash_five.png", "texture/cash_ten.png",
        "texture/cash_twenty.png", "texture/cash_fifty.png", "texture/cash_hundred.png",
        "texture/cash_thousand.png"};
    char path[260];
    int i;

    loadtexture(g_texturelist.desert, "texture/desert01.png", 1, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    generatetexture_randomvec(g_texturelist.random_vectors);
    loadtexture(g_texturelist.twitter, "texture/twitter01.png", 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR);
    loadtexture(g_texturelist.mic, "texture/mic.png", 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR);
    for (i = 0; i < NUM_BILL_TEXTURES; ++i)
        loadtexturesetalpha(g_texturelist.cash[i], bill_files[i], 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_LINEAR, 0);
    /* One texture for each file of the texture folder, up to the first entry that is not
     * positive. */
    for (i = 0; i < NUM_FILE_MATERIALS && g_materiallist.file_materials[i] > 0; ++i) {
        int material = g_materiallist.file_materials[i];

        sprintf(path, "data/texture/%s.png", g_materials[material].name);
        loadtexturesetalpha(g_texturelist.material[material], path, 1, GL_REPEAT, GL_REPEAT, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, 0);
    }
}

/* The logo texture: 32 by 16 pixels, white where the bitmap is not zero, clear elsewhere. The
 * bitmap starts at the bottom row. */
static void make_logo_texture(void)
{
    Texture *texture = &g_textures[g_texturelist.logo];
    int x, y;

    texture->width = LOGO_WIDTH;
    texture->height = LOGO_HEIGHT;
    texture->num_levels = 1;
    texture->format = GL_RGBA;
    texture->internal_format = GL_RGBA;
    texture->spare = 0;
    texture->wrap_s = GL_CLAMP_TO_EDGE;
    texture->wrap_t = GL_CLAMP_TO_EDGE;
    texture->mag_filter = GL_NEAREST;
    texture->min_filter = GL_NEAREST;
    os_free(texture->levels[0]);
    texture->levels[0] = os_malloc(texture->width * texture->height * 4);
    for (y = 0; y < texture->height; ++y)
        for (x = 0; x < texture->width; ++x)
            texture->levels[0][y * texture->width + x] =
                g_logo[(LOGO_HEIGHT - 1 - y) * LOGO_WIDTH + x] == 0 ? 0u : 0xffffffffu;
    setuptexture(g_texturelist.logo);
}

static void load_sounds(void)
{
    char path[260];
    int i;

    loadwav(g_soundlist.shot_magnum, "44magnum.wav", 10.0f);
    loadwav(g_soundlist.shot_smg, "submachinegun01.wav", 10.0f);
    loadwav(g_soundlist.shot_rifle, "rifle01.wav", 10.0f);
    loadwav(g_soundlist.engine, "4cylinder.wav", 10.0f);
    loadwav(g_soundlist.tire, "tiresqueal.wav", 10.0f);
    for (i = 0; i < NUM_RICOCHETS; ++i) {
        sprintf(path, "ricochet%02d.wav", i + 1);
        loadwav(g_soundlist.ricochet[i], path, 10.0f);
    }
    loadwav(g_soundlist.crash_small, "carcrash01.wav", 32.0f);
    loadwav(g_soundlist.crash, "carcrash02.wav", 32.0f);
    loadwav(g_soundlist.hit_body, "bullethitbody01.wav", 10.0f);
    loadwav(g_soundlist.humanhit, "bullethitbody02.wav", 10.0f);
    loadwav(g_soundlist.hit_metal[0], "bullethitmetal01.wav", 10.0f);
    loadwav(g_soundlist.hit_metal[1], "bullethitmetal02.wav", 10.0f);
    loadwav(g_soundlist.glass_break, "glassbreak01.wav", 10.0f);
    loadwav(g_soundlist.phone_ring, "phonering01.wav", 1.0f);
    for (i = 0; i < NUM_DIGITS; ++i)
        createwav(g_soundlist.digit[i], 0.25f);
    loadwav(g_soundlist.magazine, "reload01.wav", 1.0f);
    loadwav(g_soundlist.shell_hit, "shell_hit_cement.wav", 2.0f);
    loadwav(g_soundlist.gear, "gearshift01.wav", 1.0f);
    loadwav(g_soundlist.engine_large, "helicopter02.wav", 32.0f);
    createwav(g_soundlist.engine_train, 32.0f);
}

/* Build every model, texture and sound. */
void __cdecl setupclient(void)
{
    setupspheremodel();
    landscape_buildsectorlods();
    setupgame();
    make_models();
    make_generated_textures();
    load_file_textures();
    make_logo_texture();
    setupframetextures();
    load_sounds();
}
