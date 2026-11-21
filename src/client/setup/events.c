/* The client consumes events from a fixed ring.
 * A host applies visual effects but skips replicated state changes.
 * Sounds expire after 100 ticks. State changes have no age limit.
 * Packed event words carry indices, flags and signed values.
 */
#include "setup/events.h"
#include "setup/messages.h"
#include "setup/newspaper_line.h"
#include "audio/sound_create.h"
#include "render/sprites.h"
#include "render/sprite_pool.h"
#include "render/sprite_models.h"
#include "area.h"
#include "building_actions.h"
#include "bullets.h"
#include "logic.h"
#include "random.h"
#include "sr_os.h"
#include "vectors.h"
#include <string.h>

enum {
    EVENT_SHOT = 0, EVENT_HIT = 1, EVENT_MESSAGE = 2,
    EVENT_OBJECT = 3, EVENT_OBJECT_DAMAGE = 4, EVENT_ITEM = 5,
    EVENT_ITEM_INFO = 6, EVENT_PLAYER = 7, EVENT_BALANCE = 8,
    EVENT_SOUND = 9, EVENT_DOOR = 10, EVENT_SCENARIO = 11,
    EVENT_MISSION = 12, EVENT_STOCKS = 13, EVENT_BLOCK = 15,
    EVENT_BLOCK_DAMAGE = 16, EVENT_HUMAN = 17,
    EVENT_RING_MASK = 0xffff, SOUND_AGE_LIMIT = 100,
    ITEM_INDEX_MASK = 0x3ff, PLAYER_INDEX_MASK = 0xff,
    WORD_MASK = 0xffff, BYTE_MASK = 0xff, NIBBLE_MASK = 15,
    ITEM_MAGNUM = 10, ITEM_SMG = 6, MUZZLE_TICKS = 4,
    OBJECT_HEALTH = 100, FACE_HEALTH = 100, FACE_REPAIR_HEALTH = 8,
    DAMAGE_WINDOW = 0, DAMAGE_TIRE = 1, DAMAGE_HEALTH = 2,
    DAMAGE_REMOVE = 3, DAMAGE_FACE_REMOVE = 4, DAMAGE_FACE_REPAIR = 5,
    HIT_WALL = 0, HIT_BODY = 1, HIT_METAL = 2, HIT_BLOOD = 3,
    MODEL_CASING = 0, MODEL_DEBRIS = 1,
    MESSAGE_NEWS = 3, MESSAGE_EXTRA = 4, TICKS_PER_SECOND = 60
};

#define CASING_SPEED 0.01666666753590107f
#define CASING_AXIS_SPEED 0.02500000037252903f
#define DEBRIS_SPEED 0.05000000074505806f
#define HIT_SPEED 0.03333333507180214f
#define HIT_MIN_SPEED 0.008333333767950535f
#define POPPED_RADIUS 0.25f

/* Compare tick words with unsigned wrap. */
static int sound_is_recent(const Event *event)
{
    return (uint32_t)((uint32_t)sr_game_ticks - (uint32_t)event->tick) < SOUND_AGE_LIMIT;
}

/* Play one sound at the event position. */
static void event_sound(const Event *event, int sound, float pitch)
{
    createsound(sound, &event->pos, NULL, 1.0f, pitch);
}

/* Select glass pitch even when the sound has expired. */
static void glass_sound(const Event *event)
{
    unsigned choice = (unsigned)os_rand();
    if (sound_is_recent(event))
        event_sound(event, sr_data_soundlist.glass_break,
                    (float)((float)(choice & BYTE_MASK) / 512.0 + 0.75));
}

/* Spawn the bullet and its casing, then mark the muzzle flash. */
static void event_shot(const Event *event)
{
    Item *item = &sr_items[event->attach_type];
    vec3 velocity;
    int sound;
    if (!sr_server)
        createbullet(event->param, &event->pos, &event->vec, 0);
    getrandomvector3d((float *)&velocity, CASING_SPEED, 0.0f);
    scaleaddvectors(&velocity, &velocity, &item->rot[0], CASING_AXIS_SPEED);
    scaleaddvectors(&velocity, &velocity, &item->rot[1], CASING_AXIS_SPEED);
    createsprite_model(MODEL_CASING, &event->pos, &velocity, item->rot);
    if (sound_is_recent(event)) {
        int type = sr_items[event->attach_type].type;
        sound = type == ITEM_MAGNUM ? sr_data_soundlist.shot_magnum :
                type == ITEM_SMG ? sr_data_soundlist.shot_smg : sr_data_soundlist.shot_rifle;
        event_sound(event, sound, 1.0f);
    }
    sr_items[event->attach_type].countdown = MUZZLE_TICKS;
}

/* Apply damage to an object and emit glass pieces for a window hit. */
static void event_object_damage(const Event *event)
{
    Object *object = &sr_objects[event->param];
    switch (event->attach_type) {
    case DAMAGE_FACE_REMOVE:
        if (!sr_server) object->faces[event->attach_id].health = 0;
        break;
    case DAMAGE_FACE_REPAIR:
        if (!sr_server) object->faces[event->attach_id].health = FACE_REPAIR_HEALTH;
        break;
    case DAMAGE_WINDOW:
        if (!sr_server) object->faces[event->attach_id].health = 0;
        createwindowsprites(event->param, event->attach_id, &event->pos, &event->vec);
        glass_sound(event);
        break;
    case DAMAGE_TIRE:
        if (!sr_server) {
            object->wheels[event->attach_id].deflated = 1;
            object->wheels[event->attach_id].radius = POPPED_RADIUS;
        }
        break;
    case DAMAGE_HEALTH:
        if (!sr_server) object->health = event->attach_id;
        break;
    case DAMAGE_REMOVE:
        if (!sr_server) object->active = 0;
        break;
    }
}

/* Set object flags and restore face and wheel state from its type. */
static void event_object(const Event *event)
{
    Object *object = &sr_objects[event->param];
    const ObjectType *type;
    int index;
    object->active = 1;
    object->health = OBJECT_HEALTH;
    object->type = event->attach_type;
    object->color = event->attach_id;
    type = &sr_data_objecttype[object->type];
    for (index = 0; index < type->num_faces; ++index)
        object->faces[index].health = FACE_HEALTH;
    for (index = 0; index < type->num_wheels; ++index) {
        object->wheels[index].deflated = type->wheels[index].deflated;
        object->wheels[index].radius = type->wheels[index].radius;
    }
}

/* Decode item ownership. A new item starts at the world origin. */
static void event_item(const Event *event)
{
    Item *item = &sr_items[(unsigned)event->param & ITEM_INDEX_MASK];
    int was_active = item->active;
    item->active = (event->param >> 10) & 1;
    if (item->active && !was_active)
        item->pos.x = item->pos.y = item->pos.z = 0.0f;
    item->type = (event->param >> 11) & 63;
    item->parent_human = event->attach_type;
    item->parent_item = event->attach_id;
    item->parent_slot = event->range_mode;
}

/* Decode a player and copy the team and suit flags to its human. */
static void event_player(const Event *event)
{
    Player *player = &sr_players[(unsigned)event->param & PLAYER_INDEX_MASK];
    player->active = (event->param >> 9) & 1;
    if (!player->active) return;
    player->is_bot = (event->param >> 8) & 1;
    player->team = (event->param >> 10) - 1;
    player->human_id = event->attach_type;
    player->money = event->attach_id;
    player->look_a = (unsigned)event->range_mode & 3;
    player->look_b = (event->range_mode >> 2) & 7;
    player->look_c = (event->range_mode >> 5) & 3;
    player->look_d = (event->range_mode >> 7) & 1;
    player->look_e = (event->range_mode >> 8) & 1;
    player->colors[0] = (vec3){0.046875f, 0.046875f, -0.15000000596046448f};
    player->colors[1] = (vec3){1.0f, 1.0f, 1.0f};
    player->colors[2] = (vec3){-0.046875f, 0.046875f, -0.15000000596046448f};
    player->colors[3] = (vec3){1.0f, 1.0f, 1.0f};
    memcpy(player->name, event->message, sizeof(player->name));
    if (player->human_id != -1) {
        Human *human = &sr_humans[player->human_id];
        human->team = player->team;
        human->look_d = player->look_d;
        human->look_e = player->look_e;
    }
}

/* Link or remove a human. Removal does not clear its player field. */
static void event_human(const Event *event)
{
    Human *human = &sr_humans[event->param];
    int owner = event->attach_id;
    human->active = event->attach_type;
    if (!human->active) {
        if (owner != -1) sr_players[owner].human_id = -1;
    } else {
        human->player_id = owner;
        if (owner != -1) {
            Player *player = &sr_players[owner];
            player->human_id = event->param;
            memcpy(human->colors, player->colors, sizeof(human->colors));
            human->team = player->team;
            human->look_b = player->look_b;
            human->look_c = player->look_c;
            human->look_a = player->look_a;
        }
    }
}

/* Decode the scenario state and the selected scenario record. */
static void event_scenario(const Event *event)
{
    unsigned index = (event->param >> 4) & NIBBLE_MASK;
    ScenarioEventData *data = &cl_data_scenario_event_data[index];
    sr_game.round_mode = (unsigned)event->param & NIBBLE_MASK;
    *(int *)TEAM_FIELD(team0_scenario_state, 0) = (event->param >> 8) & 3;
    *(int *)TEAM_FIELD(team0_scenario_state, 1) = (event->param >> 10) & 3;
    *(int *)TEAM_FIELD(team0_scenario_state, 2) = (event->param >> 12) & 3;
    data->first_low = (unsigned)event->attach_type & WORD_MASK;
    data->first_high = (event->attach_type >> 16) & BYTE_MASK;
    data->second_low = (unsigned)event->attach_id & WORD_MASK;
    data->second_high = (event->attach_id >> 16) & WORD_MASK;
    data->third = (unsigned)event->range_mode & WORD_MASK;
    if (sr_game.unused_22fd8 < (int)(index + 1)) sr_game.unused_22fd8 = index + 1;
}

/* Decode the active mission fields. Inactive missions retain their payload. */
static void event_mission(const Event *event)
{
    unsigned slot = (event->param >> 1) & 7;
    unsigned team = (event->param >> 4) & NIBBLE_MASK;
    Mission *mission = &team_missions(team)[slot];
    mission->active = (unsigned)event->param & 1;
    if (!mission->active) return;
    mission->kind = (event->param >> 8) & NIBBLE_MASK;
    mission->item_type = (event->param >> 16) & BYTE_MASK;
    mission->partner = (unsigned)event->attach_type & NIBBLE_MASK;
    mission->partner_detail = (event->attach_type >> 4) & NIBBLE_MASK;
    mission->cash = event->attach_type >> 8;
    mission->expire = ((unsigned)event->attach_id & WORD_MASK) * TICKS_PER_SECOND;
    mission->site = (int)(((unsigned)event->attach_id >> 16) - 1u);
    mission->stake = event->range_mode;
}

/* Emit marks, blood and debris in their random draw order. */
static void event_hit(const Event *event)
{
    vec3 velocity, rotation[3];
    unsigned choice;
    int index, count, kind = event->attach_type;
    if (kind == HIT_WALL) {
        choice = (unsigned)os_rand();
        createsprite(SPRITE_MARK, &event->pos, &event->vec,
                     (float)((float)(choice & 31) / 1024.0 + 0.03125));
        if (sound_is_recent(event)) {
            choice = (unsigned)os_rand();
            event_sound(event, sr_data_soundlist.ricochet[choice & 7], 1.0f);
        }
        for (index = 0; index < 3; ++index) {
            getrandomvector3d((float *)&velocity, DEBRIS_SPEED, 0.0f);
            scaleaddvectors(&velocity, &velocity, &event->vec, CASING_SPEED);
            resetorientation(rotation);
            createsprite_model(MODEL_DEBRIS, &event->pos, &velocity, rotation);
        }
        return;
    }
    if (kind != HIT_BODY && kind != HIT_METAL && kind != HIT_BLOOD) return;
    if (kind != HIT_BLOOD && sound_is_recent(event)) {
        choice = (unsigned)os_rand();
        event_sound(event, kind == HIT_BODY ? ((choice & 1) ? sr_data_soundlist.humanhit : sr_data_soundlist.hit_body) :
                    sr_data_soundlist.hit_metal[choice & 1], 1.0f);
    }
    count = kind == HIT_BODY ? 16 : kind == HIT_METAL ? 8 : 4;
    for (index = 0; index < count; ++index) {
        getrandomvector3d((float *)&velocity, HIT_SPEED, HIT_MIN_SPEED);
        choice = (unsigned)os_rand();
        if (kind == HIT_METAL)
            createsprite(SPRITE_SPARK, &event->pos, &velocity,
                         (float)((float)(choice & 31) / 4096.0 + 0.015625));
        else
            createsprite(SPRITE_STAIN, &event->pos, &velocity,
                         (float)((float)(choice & 63) / 1024.0 + 0.0625));
    }
}

/* Break a special block window and set the face pair bits. */
static void event_block_damage(const Event *event)
{
    unsigned area = (unsigned)event->param & WORD_MASK;
    int x = event->param >> 16;
    unsigned y = (unsigned)event->attach_type & WORD_MASK;
    int z = event->attach_type >> 16;
    uint32_t word = area_interior_getblock(area, x, y, z);
    int face = event->range_mode;
    if (BLOCK_KIND(word) != BLOCK_SPEC) return;
    if (face != -1) {
        createwindowsprites_block(area, x, y, z, word, face, &event->pos, (const float *)&event->vec);
        glass_sound(event);
        word |= (3u << (((unsigned)face & ~1u) & 31)) << 16;
    }
    if (!sr_server) area_interior_setblock(area, x, y, z, word);
}

/* Apply each event before advancing the ring cursor with word wrap. */
void client_eventsimulation(void)
{
    for (; cl_data_last_event != sr_numofevents;
         cl_data_last_event = (cl_data_last_event + 1) & EVENT_RING_MASK) {
        const Event *event = &sr_events[cl_data_last_event];
        switch (event->type) {
        case EVENT_SOUND:
            if (sound_is_recent(event))
                createsound(event->param, &event->pos, NULL, event->sound_a, event->sound_b);
            break;
        case EVENT_SHOT: event_shot(event); break;
        case EVENT_OBJECT_DAMAGE: event_object_damage(event); break;
        case EVENT_OBJECT: if (!sr_server) event_object(event); break;
        case EVENT_ITEM: if (!sr_server) event_item(event); break;
        case EVENT_ITEM_INFO:
            if (!sr_server) {
                Item *item = &sr_items[(unsigned)event->param & ITEM_INDEX_MASK];
                item->call_state = event->attach_type;
                item->info = event->attach_id;
                item->phone_line = event->range_mode;
            }
            break;
        case EVENT_PLAYER: if (!sr_server) event_player(event); break;
        case EVENT_HUMAN: if (!sr_server) event_human(event); break;
        case EVENT_BALANCE:
            if (!sr_server) {
                sr_players[event->param].money = event->attach_type;
                sr_players[event->param].stocks = event->attach_id;
            }
            break;
        case EVENT_SCENARIO: if (!sr_server) event_scenario(event); break;
        case EVENT_MISSION: if (!sr_server) event_mission(event); break;
        case EVENT_STOCKS:
            if (!sr_server) {
                sr_data_market.stocks[0].price_int = (unsigned)event->param & WORD_MASK;
                sr_data_market.stocks[1].price_int = event->param >> 16;
                sr_data_market.stocks[2].price_int = (unsigned)event->attach_type & WORD_MASK;
                sr_data_market.stocks[3].price_int = event->attach_type >> 16;
                sr_data_market.stocks[4].price_int = (unsigned)event->attach_id & WORD_MASK;
            }
            break;
        case EVENT_HIT: event_hit(event); break;
        case EVENT_BLOCK:
            if (!sr_server)
                area_interior_setblock((unsigned)event->param & WORD_MASK, event->param >> 16,
                              (unsigned)event->attach_type & WORD_MASK, event->attach_type >> 16,
                              (unsigned)event->attach_id);
            break;
        case EVENT_BLOCK_DAMAGE: event_block_damage(event); break;
        case EVENT_MESSAGE:
            if (sound_is_recent(event) || event->attach_type == MESSAGE_NEWS) {
                if (event->attach_type < MESSAGE_NEWS || event->attach_type == MESSAGE_EXTRA)
                    createmessage(event->attach_type, event->message, event->attach_id, event->range_mode);
                if (event->attach_type == MESSAGE_NEWS)
                    newspaper_addline(event->message, event->attach_id, event->range_mode);
            }
            break;
        case EVENT_DOOR: building_addgaragedoor(event->param, event->attach_type); break;
        }
    }
}
