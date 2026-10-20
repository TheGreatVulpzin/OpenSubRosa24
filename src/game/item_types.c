/* setupitemtypes: the table of the 27 item types.
 *
 * The guns have a name, a price, a mass, a bullet type, a fire delay, the speed of the bullet and
 * the shape of the model. The magazines name the gun they fit. The inertia of every type comes
 * last: it is the inertia of a box of the size of the type.
 */
#include <string.h>

#include "bullets.h"
#include "items.h"
#include "sr_runtime.h"

/* A gun. The recoil is the mass of the bullet times its speed, times recoil_scale. */
typedef struct GunRow {
    int type;
    const char *name;
    int price;
    float mass;
    int bullet_type;
    int fire_delay;
    int client_param;
    float bullet_speed;
    float shape[4];
    float muzzle_z;
    float grip[3];
    vec3 size;
    double recoil_scale;
    int one_handed;
} GunRow;

/* A magazine or a box of bullets. */
typedef struct MagazineRow {
    int type;
    const char *name;
    int price; /* -1: the price stays as it is */
    int ammo;
    int gun;
    float mass;
} MagazineRow;

/* An item that is not a gun and not a magazine. fits_in_case: it goes into the open briefcase. */
typedef struct OtherRow {
    int type;
    const char *name;
    int price; /* -1: the price stays as it is */
    int ammo;  /* -1: the ammo stays as it is */
    float mass;
    vec3 size;
    int fits_in_case;
} OtherRow;

#define SPREAD 0.03333333507180214f
#define SPEED_AK 10.666666984558105f
#define SPEED_M16 14.666666984558105f
#define SPEED_SMG 6.666666507720947f
#define SPEED_MAGNUM 7.666666507720947f

static const GunRow guns[] = {
    { ITEM_AK47, "AK-47", 500, 4.0f, 0, 8, 90, SPEED_AK, { 0.0390625f, 0.5f, 0.25f, 0.0390625f }, -0.625f,
      { 0.0f, -0.0625f, 0.125f }, { 0.0625f, 0.09375f, 0.625f }, 0.25, 0 },
    { ITEM_M16, "M-16", 750, 3.25f, 1, 7, 90, SPEED_M16, { 0.0390625f, 0.5f, 0.25f, 0.0390625f }, -0.625f,
      { 0.0f, -0.0625f, 0.125f }, { 0.0625f, 0.09375f, 0.625f }, 0.25, 0 },
    { ITEM_MP5, "MP5", 300, 2.5f, 2, 5, 90, SPEED_SMG, { 0.0390625f, 0.5f, 0.25f, 0.0390625f }, -0.625f,
      { 0.0f, -0.0625f, 0.125f }, { 0.0625f, 0.09375f, 0.625f }, 0.25, 0 },
    { ITEM_UZI, "Uzi", 1000, 2.5f, 2, 6, 90, SPEED_SMG, { 0.0390625f, 0.3125f, 0.25f, 0.0390625f }, -0.375f,
      { 0.0f, -0.0625f, 0.0625f }, { 0.0625f, 0.09375f, 0.3125f }, 0.25, 1 },
    { ITEM_MAGNUM, "Magnum", 10000, 1.25f, 3, 30, 18, SPEED_MAGNUM, { 0.0625f, 0.375f, 0.125f, 0.0625f }, -0.4375f,
      { 0.0f, -0.125f, 0.0625f }, { 0.0625f, 0.09375f, 0.1875f }, 0.5, 1 },
    { ITEM_9MM, "9mm", 0, 1.0f, 2, 30, 32, SPEED_SMG, { 0.03125f, 0.25f, 0.125f, 0.03125f }, -0.3125f,
      { 0.0f, -0.125f, 0.0f }, { 0.125f, 0.1875f, 0.375f }, 0.5, 1 },
};

static const MagazineRow magazines[] = {
    { ITEM_AK47_MAGAZINE, "AK-47 Magazine", 50, 30, ITEM_AK47, 0.75f },
    { ITEM_M16_MAGAZINE, "M-16 Magazine", 75, 30, ITEM_M16, 0.75f },
    { ITEM_MP5_MAGAZINE, "MP5 Magazine", 30, 30, ITEM_MP5, 0.625f },
    { ITEM_UZI_MAGAZINE, "Uzi Magazine", 100, 20, ITEM_UZI, 0.625f },
    { ITEM_45_BULLETS, ".45 Bullets", 1000, 6, ITEM_MAGNUM, 0.5f },
    { ITEM_9MM_MAGAZINE, "9mm Magazine", -1, 12, ITEM_9MM, 0.5f },
};

static const OtherRow others[] = {
    { ITEM_BANDAGE, "Bandage", 100, 2, 1.0f, { 0.25f, 0.125f, 0.25f }, 0 },
    { ITEM_BRIEFCASE, "Briefcase", 50, -1, 4.0f, { 0.375f, 0.1875f, 0.375f }, 0 },
    { ITEM_OPEN_BRIEFCASE, "Open Briefcase", -1, -1, 4.0f, { 0.375f, 0.1875f, 0.375f }, 0 },
    { ITEM_CASH, "Cash", -1, -1, 0.10000000149011612f, { 0.125f, 0.125f, 0.25f }, 1 },
    { ITEM_CASH_2, "Cash", -1, -1, 0.009999999776482582f, { 0.125f, 0.0625f, 0.25f }, 1 },
    { ITEM_BLACK_DISK, "Black Disk", -1, -1, 1.0f, { 0.25f, 0.09375f, 0.25f }, 1 },
    { ITEM_GREEN_DISK, "Green Disk", -1, -1, 1.0f, { 0.25f, 0.09375f, 0.25f }, 1 },
    { ITEM_BLUE_DISK, "Blue Disk", -1, -1, 1.0f, { 0.25f, 0.09375f, 0.25f }, 1 },
    { ITEM_WHITE_DISK, "White Disk", -1, -1, 1.0f, { 0.25f, 0.09375f, 0.25f }, 1 },
    { ITEM_GOLD_DISK, "Gold Disk", -1, -1, 1.0f, { 0.25f, 0.09375f, 0.25f }, 1 },
    { ITEM_RED_DISK, "Red Disk", -1, -1, 1.0f, { 0.25f, 0.09375f, 0.25f }, 1 },
    { ITEM_CELL_PHONE, "Cell Phone", -1, -1, 1.0f, { 0.078125f, 0.234375f, 0.078125f }, 0 },
    { ITEM_KEY, "Key", -1, -1, 1.0f, { 0.0625f, 0.0625f, 0.125f }, 0 },
    { ITEM_DOOR, "Door", -1, -1, 20.0f, { 0.5f, 1.0625f, 0.03125f }, 0 },
    { ITEM_NEWSPAPER, "Newspaper", -1, -1, 0.5f, { 0.5f, 0.125f, 0.5f }, 0 },
};

static void set_gun(const GunRow *g)
{
    ItemType *t = &sr_itemtypes[g->type];
    int k;

    strcpy(t->name, g->name);
    t->price = g->price;
    t->mass = g->mass;
    t->is_gun = 1;
    t->bullet_type = g->bullet_type;
    t->fire_delay = g->fire_delay;
    t->client_param = g->client_param;
    t->ammo = 0;
    t->bullet_speed = g->bullet_speed;
    t->spread = SPREAD;
    for (k = 0; k < 4; k++)
        t->shape[k] = g->shape[k];
    t->muzzle.x = 0.0f;
    t->muzzle.y = 0.0f;
    t->muzzle.z = g->muzzle_z;
    t->grip.x = g->grip[0];
    t->grip.y = g->grip[1];
    t->grip.z = g->grip[2];
    t->size = g->size;
    /* The product stays in a double register. Only the last store rounds. */
    t->recoil = (float)((double)g_bullettypes[g->bullet_type].mass * g->bullet_speed * g->recoil_scale);
    if (g->one_handed)
        t->one_handed = 1;
    /* The Uzi takes the recoil of the MP5. */
    if (g->type == ITEM_UZI)
        t->recoil = sr_itemtypes[ITEM_MP5].recoil;
}

static void set_magazine(const MagazineRow *m)
{
    ItemType *t = &sr_itemtypes[m->type];

    strcpy(t->name, m->name);
    if (m->price >= 0)
        t->price = m->price;
    t->is_gun = 0;
    t->ammo = m->ammo;
    t->fits_in[m->gun] = 1;
    t->mass = m->mass;
    t->size.x = 0.0625f;
    t->size.y = 0.125f;
    t->size.z = 0.25f;
}

static void set_other(const OtherRow *o)
{
    ItemType *t = &sr_itemtypes[o->type];

    strcpy(t->name, o->name);
    if (o->price >= 0)
        t->price = o->price;
    if (o->ammo >= 0)
        t->ammo = o->ammo;
    t->is_gun = 0;
    t->mass = o->mass;
    t->size = o->size;
    if (o->fits_in_case)
        t->fits_in[ITEM_OPEN_BRIEFCASE] = 1;
}

/* The inverse of the moment of inertia of a box of mass 3 around each axis. The sums and the
 * division stay in double registers. */
static float box_inverse(float a, float b)
{
    return (float)(1.0 / (((double)a * a + (double)b * b) / 3.0));
}

void setupitemtypes(void)
{
    int k;

    for (k = 0; k < (int)(sizeof guns / sizeof guns[0]); k++)
        set_gun(&guns[k]);
    for (k = 0; k < (int)(sizeof magazines / sizeof magazines[0]); k++)
        set_magazine(&magazines[k]);
    for (k = 0; k < (int)(sizeof others / sizeof others[0]); k++)
        set_other(&others[k]);
    for (k = 0; k < NUM_ITEM_TYPES; k++) {
        ItemType *t = &sr_itemtypes[k];

        t->inv_inertia.x = box_inverse(t->size.y, t->size.z);
        t->inv_inertia.y = box_inverse(t->size.x, t->size.z);
        t->inv_inertia.z = box_inverse(t->size.x, t->size.y);
    }
}
