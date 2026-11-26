/* The frame list: each model that the frame draws, with its place, rotation and textures.
 * buildframelist fills it from the humans, vehicles, items, bullets, street objects and the
 * objects in the interiors of the visible sectors, then sorts it by the first texture. It also
 * fills the list of vehicle windows. */
#include "render/frame_list.h"

#include "client.h"
#include "debug/debug.h"
#include "items.h"
#include "math/rotate.h"
#include "math/small_vectors.h"
#include "render/lists.h"
#include "setup/timers.h"
#include "sr_runtime.h"
#include "util.h"
#include "vectors.h"

#define g_modellist sr_data_modellist
#define g_textures sr_data_texturelist
#define g_interiors ((const InteriorMesh *)&sr_data_interior)
#define g_client_game (*(const ClientGameView *)&sr_data_game)

enum {
    HUMANS = 256, OBJECTS = 1024, ITEMS = 1024, CITY_OBJECTS = 1024,
    FLAG_SHADOW = 1, FLAG_HIDDEN = 2,
    OBJECT_BROKEN = 3, OBJECT_HELICOPTER = 11,
    CITY_ELEVATOR = 2, FIXTURE_LAMP = 4, FIXTURE_LIGHT = 5,
    HEAD_BONE = 2, LEFT_FOOT_BONE = 11, RIGHT_FOOT_BONE = 14,
    KEY_FREE_VIEW = 27
};

/* The value of pi that the game rounds to a float. */
static const double PI_FLOAT = 3.1415927410125732;

/* Add a model with two texture numbers. The last two texture words are 0. */
static void add_model(int model, int texture, int second, uint32_t flags, const vec3 *pos, const vec3 rot[3])
{
    int textures[4];

    textures[0] = texture;
    textures[1] = second;
    textures[2] = 0;
    textures[3] = 0;
    frame_addmodel(model, textures, flags, pos, rot);
}

/* The head gets flag 3 (hidden, with shadow) when the local player looks out of the eyes of the
 * human: the player has this human, and it lives or the death view still holds. The free view
 * key of the debug mode shows the head. */
static uint32_t head_flags(int human)
{
    int view = sr_data_view;

    if (sr_data_debugit != 0 && cl_data_audio_input_blocked == 0 && cl_data_keyboard[KEY_FREE_VIEW] != 0)
        return FLAG_SHADOW;
    if (view == -1 || sr_data_player[view].human_id == -1 || sr_data_player[view].human_id != human)
        return FLAG_SHADOW;
    if (sr_data_human[human].health > 0 || g_client_game.death_view_timer > 0)
        return FLAG_SHADOW | FLAG_HIDDEN;
    return FLAG_SHADOW;
}

/* The body, head, hair, sunglasses and the two feet of each human. */
static void add_humans(void)
{
    static const vec3 origin = {0.0f, 0.0f, 0.0f};
    int i;

    for (i = 0; i < HUMANS; ++i)
        if (sr_data_human[i].active != 0)
            buildhumanmodel(i, g_modellist.human[i]);
    for (i = 0; i < HUMANS; ++i) {
        const Human *human = &sr_data_human[i];
        const Bone *head = &human->bones[HEAD_BONE];
        int skin = g_textures.human_textures[i];
        vec3 rot[3], pos;
        uint32_t flags;

        if (human->active == 0)
            continue;
        resetorientation(rot);
        add_model(g_modellist.human[i], skin, skin, FLAG_SHADOW, &origin, rot);
        flags = head_flags(i);
        scaleaddvectors(&pos, &head->pos, &head->rot[1], 0.0625f);
        add_model(g_modellist.head, skin, skin, flags, &pos, head->rot);
        add_model(g_modellist.hair, skin, skin, flags, &pos, head->rot);
        if (human->look_e != 0) {
            scaleaddvectors(&pos, &pos, &head->rot[1], -0.046875f);
            add_model(g_modellist.sunglasses, g_textures.panel[0], g_textures.panel[0], flags, &pos, head->rot);
        }
        add_model(g_modellist.foot, g_textures.half_brown, g_textures.half_brown, FLAG_SHADOW,
                  &human->bones[LEFT_FOOT_BONE].pos, human->bones[LEFT_FOOT_BONE].rot);
        add_model(g_modellist.foot, g_textures.half_brown, g_textures.half_brown, FLAG_SHADOW,
                  &human->bones[RIGHT_FOOT_BONE].pos, human->bones[RIGHT_FOOT_BONE].rot);
    }
}

/* The wheels of a vehicle. A wheel far from the camera (128 units or more) casts no shadow. */
static void add_wheels(const Object *object, float distance)
{
    const ObjectType *type = &sr_data_objecttype[object->type];
    int w;

    for (w = 0; w < type->num_wheels; ++w) {
        const Wheel *wheel = &object->wheels[w];
        const vec3 *offset = &type->wheels[w].pos;
        vec3 rot[3], pos;

        copyvector((unsigned int *)&rot[0], (const unsigned int *)&object->rot[0]);
        copyvector((unsigned int *)&rot[1], (const unsigned int *)&object->rot[1]);
        copyvector((unsigned int *)&rot[2], (const unsigned int *)&object->rot[2]);
        scaleaddvectors(&pos, &object->pos, &rot[0], offset->x);
        scaleaddvectors(&pos, &pos, &rot[1], offset->y);
        scaleaddvectors(&pos, &pos, &rot[2], offset->z);
        scaleaddvectors(&pos, &pos, &rot[1], wheel->travel);
        /* The axis is a row of the matrix that turns. */
        rotateorientationfast(rot, &rot[1], wheel->steer);
        rotateorientationfast(rot, &rot[0], wheel->angle);
        add_model(g_modellist.wheel[wheel->deflated], g_textures.grey_disc, g_textures.grey_disc,
                  128.0f <= distance ? 0 : FLAG_SHADOW, &pos, rot);
    }
}

/* The two blades of a helicopter rotor, half a turn apart. */
static void add_rotor(const Object *object)
{
    vec3 rot[3], pos;

    copyvector((unsigned int *)&rot[0], (const unsigned int *)&object->rot[0]);
    copyvector((unsigned int *)&rot[1], (const unsigned int *)&object->rot[1]);
    copyvector((unsigned int *)&rot[2], (const unsigned int *)&object->rot[2]);
    scaleaddvectors(&pos, &object->pos, &rot[1], 1.5f);
    rotateorientationfast(rot, &rot[1], object->wheels[0].angle);
    add_model(g_modellist.rotor, g_textures.grey_disc, g_textures.grey_disc, FLAG_SHADOW, &pos, rot);
    rotateorientationfast(rot, &rot[1], (float)PI_FLOAT);
    add_model(g_modellist.rotor, g_textures.grey_disc, g_textures.grey_disc, FLAG_SHADOW, &pos, rot);
}

/* Each window of a vehicle that is not broken goes into the window list: its corners in the
 * world, the distance of its middle to the camera, its normal, and 1 when the camera is on the
 * side of the normal. The list has no size limit. */
static void add_windows(const Object *object)
{
    const Model *model = &cl_data_model[g_modellist.vehicle[object->type]];
    int w, c;

    for (w = 0; w < model->num_windows; ++w) {
        WindowQuad *quad;
        vec3 middle, corner, to_camera;

        if (object->faces[model->windows[w].face].health <= 0)
            continue;
        quad = &cl_data_window_quads[cl_data_window_quad_count];
        zerovector((unsigned int *)&middle);
        for (c = 0; c < 4; ++c) {
            projectvector(&corner, &model->windows[w].corners[c], object->rot);
            addvectors(&corner, &corner, &object->pos);
            copyvector((unsigned int *)&quad->corners[c], (const unsigned int *)&corner);
            addvectors(&middle, &middle, &corner);
        }
        scalevector(&middle, &middle, 0.25f);
        quad->distance = vectordistance(&middle, &cl_data_camera_position);
        generatetrinormal(&quad->normal, &quad->corners[0], &quad->corners[1], &quad->corners[2]);
        subtractvectors(&to_camera, &cl_data_camera_position, &quad->corners[0]);
        /* A NaN gives 1. */
        quad->facing = dotproduct(&to_camera, &quad->normal) < 0.0f ? 0 : 1;
        ++cl_data_window_quad_count;
    }
}

/* Each vehicle that is not a wreck: the body in its paint, a dark body when its health is gone,
 * the wheels, the rotor of a helicopter and the windows. */
static void add_objects(void)
{
    int i;

    for (i = 0; i < OBJECTS; ++i) {
        const Object *object = &sr_data_object[i];
        float distance;

        if (object->active == 0 || object->state == OBJECT_BROKEN)
            continue;
        distance = vectordistance(&cl_data_camera_position, &object->pos);
        add_model(g_modellist.vehicle[object->type], g_textures.paint[object->color], g_textures.paint[0],
                  FLAG_SHADOW, &object->pos, object->rot);
        if (object->health < 1) {
            /* The last entry of the list, also when the list was full. */
            FrameEntry *last = &cl_data_frame_models[cl_data_frame_model_count - 1];
            last->color.x = 0.125f;
            last->color.y = 0.125f;
            last->color.z = 0.125f;
        }
        add_wheels(object, distance);
        if (object->type == OBJECT_HELICOPTER)
            add_rotor(object);
        add_windows(object);
    }
}

/* The bullets in flight, with no shadow. */
static void add_bullets(void)
{
    int i;

    for (i = 0; i < sr_data_numofbullets; ++i) {
        const Bullet *bullet = &sr_data_bullet[i];
        add_model(g_modellist.bullet[bullet->type], g_textures.plain_orange, g_textures.plain_orange, 0,
                  &bullet->pos, &bullet->side);
    }
}

/* An item shows when it lies free, or when it sits in a hand or in a hand slot of a parent item
 * (slot 0 to 2). */
static int item_shows(const Item *item)
{
    const Item *parent;

    if (item->active == 0)
        return 0;
    if (item->parent_item != -1) {
        parent = &sr_data_item[item->parent_item];
        if (parent->parent_human != -1 && parent->parent_slot >= 3)
            return 0;
    }
    return item->parent_human == -1 || item->parent_slot < 3;
}

/* A stack of bills: one thin model for each bill, from the top down, in the color of its value.
 * In a hand the stack fans out, and the last bill turns more. */
static void add_bills(const Item *item)
{
    vec3 rot[3], pos;
    float turn;
    int b;

    copyvector((unsigned int *)&pos, (const unsigned int *)&item->pos);
    copyvector((unsigned int *)&rot[0], (const unsigned int *)&item->rot[0]);
    copyvector((unsigned int *)&rot[1], (const unsigned int *)&item->rot[1]);
    copyvector((unsigned int *)&rot[2], (const unsigned int *)&item->rot[2]);
    scaleaddvectors(&pos, &pos, &rot[1], (float)((float)item->last_bill * 0.015625 * 0.5));
    for (b = 0; b <= item->last_bill; ++b) {
        int value = (int)(item->bills_mask >> ((b * 3) & 0x1f) & 7);
        int cash = g_textures.cash[value];

        add_model(g_modellist.item[item->type], cash, cash, FLAG_SHADOW, &pos, rot);
        scaleaddvectors(&pos, &pos, &rot[1], -0.015625f);
        if (item->parent_human == -1)
            continue;
        if (item->parent_slot == 0)
            turn = -0.03125f;
        else if (item->parent_slot == 1)
            turn = 0.03125f;
        else
            turn = 0.0f;
        /* The axis is a row of the matrix that turns. */
        rotateorientation(rot, &rot[1], (float)(turn * PI_FLOAT * 0.5));
        if (b == item->num_bills - 1) {
            rotateorientation(rot, &rot[1], (float)(turn * PI_FLOAT * 0.5));
            scaleaddvectors(&pos, &pos, &rot[0], turn);
        }
    }
}

/* The texture of an item type. */
static int item_texture(const Item *item)
{
    int texture = item->type == ITEM_45_BULLETS ? g_textures.plain_orange : g_textures.half_brown;

    if (item->type == ITEM_CASH)
        texture = g_textures.cash[1];
    /* The six disks use the six texture numbers from the first panel texture. */
    if (item->type >= ITEM_BLACK_DISK && item->type <= ITEM_RED_DISK)
        texture = g_textures.panel[0] + item->type - ITEM_BLACK_DISK;
    if (item->type == ITEM_BANDAGE)
        texture = g_textures.material[0];
    if (item->type == ITEM_CELL_PHONE)
        texture = item->phone_line == 1 ? g_textures.panel[0] : g_textures.half_grey;
    if (item->type == ITEM_KEY)
        texture = g_textures.panel[0];
    if (item->type == ITEM_NEWSPAPER)
        texture = g_textures.newspaper;
    return texture;
}

static void add_items(void)
{
    int i;

    for (i = 0; i < ITEMS; ++i) {
        const Item *item = &sr_data_item[i];
        int texture;

        if (!item_shows(item))
            continue;
        if (item->type == ITEM_CASH_2) {
            add_bills(item);
            continue;
        }
        texture = item_texture(item);
        add_model(g_modellist.item[item->type], texture, texture, FLAG_SHADOW, &item->pos, item->rot);
    }
}

/* The street objects. An elevator also gets its four door parts, at the width of the door. */
static void add_city_objects(void)
{
    int i;

    for (i = 0; i < CITY_OBJECTS; ++i) {
        const CityObjectRec *city = &sr_data_cityobject[i];
        int door = g_modellist.city_object[3];
        int panel = g_textures.panel[0];
        float width;
        vec3 pos;

        if (city->active == 0)
            continue;
        add_model(g_modellist.city_object[city->type], panel, panel, FLAG_SHADOW, &city->pos, city->rot);
        if (city->type != CITY_ELEVATOR)
            continue;
        width = city->net_scalar;
        scaleaddvectors(&pos, &city->pos, &city->rot[2], -1.75f);
        scaleaddvectors(&pos, &pos, &city->rot[0], (float)(width + 0.5));
        add_model(door, panel, panel, FLAG_SHADOW, &pos, city->rot);
        scaleaddvectors(&pos, &city->pos, &city->rot[2], -1.625f);
        scaleaddvectors(&pos, &pos, &city->rot[0], (float)(width * 0.5 + 1.25));
        add_model(door, panel, panel, FLAG_SHADOW, &pos, city->rot);
        scaleaddvectors(&pos, &city->pos, &city->rot[2], -1.75f);
        scaleaddvectors(&pos, &pos, &city->rot[0], (float)-(width + 0.5));
        add_model(door, panel, panel, FLAG_SHADOW, &pos, city->rot);
        scaleaddvectors(&pos, &city->pos, &city->rot[2], -1.625f);
        scaleaddvectors(&pos, &pos, &city->rot[0], (float)-(width * 0.5 + 1.25));
        add_model(door, panel, panel, FLAG_SHADOW, &pos, city->rot);
    }
}

/* The lamps and traffic lights in the interiors of the visible sectors. A traffic light shows
 * red or green by the state of its side of the intersection. */
static void add_fixtures(void)
{
    int s, slot, f;

    for (s = 0; s < cl_data_sector_count; ++s)
        for (slot = 0; slot < 4; ++slot) {
            int m = cl_data_block_sectors[cl_data_sector_list[s]].interior_models[slot];
            const InteriorMesh *mesh;

            if (m < 0)
                continue;
            mesh = &g_interiors[m];
            for (f = 0; f < mesh->num_fixtures; ++f) {
                const InteriorFixture *fixture = &mesh->fixtures[f];
                int texture = g_textures.panel[0];

                if (fixture->type == FIXTURE_LAMP)
                    texture = g_textures.street[4];
                if (fixture->type == FIXTURE_LIGHT) {
                    int red = sr_data_city.intersections[fixture->intersection].red[fixture->side];
                    texture = g_textures.street[5];
                    if (red == 1)
                        texture = g_textures.street[6];
                    if (red == 0)
                        texture = g_textures.street[7];
                }
                add_model(g_modellist.city_object[fixture->type], texture, texture, FLAG_SHADOW,
                          &fixture->pos, fixture->rot);
            }
        }
}

/* Fill the frame list and the window list for the frame, then sort the frame list by the first
 * texture of each model, so the draw changes the texture less often. */
void __cdecl buildframelist(void)
{
    int i;

    starttimer(sr_data_timerlist.buildframelist);
    cl_data_frame_model_count = 0;
    cl_data_window_quad_count = 0;
    add_humans();
    add_objects();
    add_bullets();
    add_items();
    add_city_objects();
    add_fixtures();
    for (i = 0; i < cl_data_frame_model_count; ++i)
        sr_data_radix.keys[i] = (uint32_t)cl_data_frame_models[i].textures[0];
    sr_data_radix.count = cl_data_frame_model_count;
    csradixsort();
    for (i = 0; i < cl_data_frame_model_count; ++i)
        cl_data_frame_model_order[i] = sr_data_radix.order[i];
    debug_addataint("numofmodels", cl_data_frame_model_count);
    stoptimer(sr_data_timerlist.buildframelist);
}
