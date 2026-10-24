/* Asset identifiers and game reset. */
#include "logic.h"
#include "level.h"

#include <string.h>

#include "block_mesh.h"
#include "buildings.h"
#include "bullets.h"
#include "economy.h"
#include "events.h"
#include "files.h"
#include "humans.h"
#include "items.h"
#include "object_types.h"
#include "objects.h"
#include "player.h"
#include "scenario.h"
#include "server.h"
#include "sr_os.h"
#include "sr_server.h"
#include "vectors.h"

#define g_materiallist (*SR_AT(MaterialList, SR_RVA_MATERIALLIST))
#define g_soundlist (*SR_AT(SoundList, SR_RVA_SOUNDLIST))
#define g_filelist (*SR_AT(FileList, SR_RVA_FILELIST))
#define g_area_sector (*SR_AT(AreaSector, SR_RVA_AREA_SECTOR))
#define g_blocktemp (*SR_AT(BlockTemp, SR_RVA_BLOCKTEMP))
#define g_camera SR_AT(float, 0xf437940)
#define g_voice SR_AT(VoiceChannel, 0xeca4bc0)
#define g_track SR_AT(TrackPiece, SR_RVA_TRACK)
#define g_cityobjects SR_AT(CityObjectRec, SR_RVA_CITYOBJECT)
#define g_yaxis SR_AT(vec3, 0x60ce0)
#define g_numoftracks (*SR_AT(int, SR_RVA_NUMOFTRACKS))
#define g_numofsprites (*SR_AT(int, 0x761e600))
#define g_numofsprite_models (*SR_AT(int, 0x12a81c44))
#define g_sprite_render SR_AT(uint8_t, 0x761eb00)
#define g_tiremarkstart (*SR_AT(int, 0xb7e8ff4))
#define g_tiremarkend (*SR_AT(int, 0xb7a8c80))

static void test_inventory(int player, vec3 *pos, vec3 *rot, int second_gun)
{
    static const int types[5] = {2, 3, 3, 3, 4};
    static const int slots[5] = {0, 1, 3, 3, 2};
    int j;
    for (j = 0; j < (second_gun ? 5 : 4); ++j) {
        int item = createitem(types[j], pos, NULL, rot);
        linkitem(item, UINT32_MAX, sr_players[player].human_id, slots[j]);
    }
}

static void test_items(vec3 *pos, vec3 *rot)
{
    static const int types[12] = {23, 23, 13, 12, 16, 16, 20, 19, 0, 1, 1, 1};
    int j;
    pos->x = (float)((double)pos->x + 2.0);
    createobject(6, pos, NULL, rot, 1);
    for (j = 0; j < 12; ++j) {
        int item = createitem(types[j], pos, NULL, rot);
        pos->z = (float)((double)pos->z + 1.0);
        if (j < 2)
            sr_items[item].phone_id = (j + 1) * 0x457;
        else if (j == 4 || j == 5) {
            sr_items[item].bills_mask = j == 4 ? 0x06006001u : 0x0402b001u;
            sr_items[item].last_bill = 9;
        }
    }
    pos->x = (float)((double)pos->x + 1.5);
    pos->y = (float)((double)(float)((double)pos->y + 0.125) - 1.0);
    if (sr_debugit)
        createitem(25, pos, NULL, rot);
    pos->x = (float)((double)pos->x + 1.0);
}

static void test_players(void)
{
    const vec3 *start = &g_city.intersections[g_city.roads[0].ends[0]].pos;
    vec3 pos, rot[3];
    int i;
    for (i = 0; i < 4; ++i) {
        Player *p = &sr_players[i];
        pos.x = (float)((double)i * 2.0 + 280.0);
        pos.y = 26.0f;
        pos.z = 192.0f;
        if (i < 2) {
            pos.x = i ? (float)((double)start->x - 8.0) : start->x;
            pos.y = (float)((double)start->y + 2.0);
            pos.z = (float)((double)start->z + (i ? 80.0 : 64.0));
        } else if (i == 2) {
            pos.x = 206.5f;
            pos.y = 25.25f;
            pos.z = 143.5f;
        }
        p->active = 1;
        if (i > 0)
            p->is_bot = 1;
        p->team = 0;
        resetorientation(rot);
        p->human_id = createhuman(&pos.x, &rot[0].x, i);
        if (i == 0) {
            resetorientation(rot);
            test_inventory(i, &pos, rot, 0);
            test_items(&pos, rot);
        } else if (i == 2) {
            int item = createitem(22, &pos, NULL, rot);
            linkitem(item, UINT32_MAX, sr_players[2].human_id, 4);
        } else if (i == 1) {
            p->ai_waypoint = 0;
            p->num_path = 1;
            p->waypoints[0].pos.x = 281.0f;
            p->waypoints[0].pos.y = 26.0f;
            p->waypoints[0].pos.z = 128.0f;
        }
    }
    /* The search can use the index just after the last building. */
    if (g_city.num_buildings > 0) {
        int building;
        for (building = 0; building < g_city.num_buildings; ++building)
            if (*(int *)((uint8_t *)&g_city.building_type + building * 0x9dc) == 3)
                break;
        if (sr_game.gametype == 2)
            scenario_createguards(building, 22, 10000000);
    }
    resetorientation(rot);
    if (sr_debugit) {
        resetorientation(rot);
        rotateorientation(rot, g_yaxis, -1.5707963705062866f);
        scenario_createcar(1, 1, 0, 0, 1, 64.0f);
    }
    i = os_rand() % 6;
    scenario_createcar(11, i, 0, 1, 1, 80.0f);
}

void resetgame(void)
{
    int i, j, round;
    const vec3 *start;
    vec3 pos, rot[3];
    sr_serverinfo.ban_candidate = -1;
    if (sr_game.gametype == 3 && sr_server)
        saveaccountsserver();
    sr_game.state = 1;
    sr_game.timer = 0;
    if (sr_game.gametype == 3)
        sr_game.timer = sr_game.wait_time;
    else if (sr_game.gametype >= 0 && sr_game.gametype <= 2) {
        sr_game.state = 2;
        sr_game.timer = sr_game.round_time;
    }
    sr_game.unused_257e8 = 0;
    g_camera[0] = 160.0f;
    g_camera[1] = 32.0f;
    g_camera[2] = 128.0f;
    sr_game.unused_257dc = -1;
    sr_game.unused_257e0 = 0;
    sr_game.unused_257ec = 0;
    for (i = 0; i < 5; ++i) {
        *(int *)TEAM_FIELD(team0_unused_8c, i) = 0;
        *(int *)TEAM_FIELD(team0_members, i) = 0;
        *(int *)TEAM_FIELD(team0_cash, i) = 0;
        *(int *)TEAM_FIELD(team0_manager, i) = -1;
        for (j = 0; j < 16; ++j)
            team_missions(i)[j].active = 0;
    }
    for (i = 0; i < 5; ++i) {
        Stock *s = &g_market.stocks[i];
        if (sr_game.gametype == 4 || sr_game.round_number == 0) {
            s->price = 100.0f;
            s->price_int = 100;
            s->shares = 100;
        }
        if (sr_game.gametype == 3)
            s->open_price = (float)s->price_int;
    }
    sr_game.unused_25978 = 0;
    sr_game.unused_25974 = 0;
    sr_game.num_newspapers = 0;
    sr_game.unused_22fd8 = 0;
    round = sr_server ? sr_game.round_number + 1 : g_area_sector.client_game_id;
    sr_game.ticks = 0;
    g_area_sector.unused_0 = 0;
    *SR_AT(int, 0x9c1d8a4) = 0;
    sr_game.round_number = round;
    if (!sr_server)
        g_numofheadlines = 0;
    else {
        newspaper_load();
        if (sr_server) {
            int n = os_rand() & 255;
            sr_game.start_angle_a = (float)(0.39269908169875 - (double)n * 3.1415927410125732 * 0.25 * 0.00390625);
            sr_game.start_angle_b = 0.5f;
        }
    }
    sr_numofparticles = 0;
    sr_numofbonds = 0;
    for (i = 0; i < 256; ++i) {
        Player *p = &sr_players[i];
        if (p->is_bot)
            p->active = 0;
        p->num_purchases = 0;
        p->unused_74 = 0;
        p->num_actions = 0;
        p->action_tail = 0;
        p->move_mode = 1;
        p->human_id = -1;
        if (sr_game.gametype == 4) {
            p->stocks = 0;
            p->team = 7;
        } else if (sr_game.gametype == 0)
            p->team = 0;
        p->ready = 0;
        p->checkpoints = 0;
        p->checkpoint_mask = 0;
        p->unused_60 = 0;
        p->wait_isec = -1;
        sr_speex.voice_ring[i].voice_off = 1;
        p->menu = sr_game.gametype == 3 ? 2 : 0;
    }
    for (i = 0; i < 512; ++i)
        sr_humans[i].active = 0;
    for (i = 0; i < 1024; ++i)
        sr_objects[i].active = 0;
    for (i = 0; i < 1024; ++i)
        sr_items[i].active = 0;
    for (i = 0; i < 1024; ++i)
        g_cityobjects[i].active = 0;
    g_numoftracks = 0;
    sr_numofbullets = 0;
    g_numofsprites = 0;
    g_numofsprite_models = 0;
    *(int *)(g_sprite_render + 0) = 0;
    *(int *)(g_sprite_render + 0x50004) = 0;
    *(int *)(g_sprite_render + 0xb0008) = 0;
    *(int *)(g_sprite_render + 0x8b000c) = 0;
    g_tiremarkstart = 0;
    g_tiremarkend = 0;
    sr_numofevents = 0;
    g_blocktemp.unused_7602c = 0;
    for (i = 0; i < sr_numofconnections; ++i) {
        sr_connections[i].event_ack = 0;
        for (j = 0; j < 8; ++j)
            sr_connections[i].voice[j].active = 0;
    }
    for (i = 0; i < 8; ++i)
        g_voice[i].active = 0;
    createlevel();
    os_srand((unsigned)os_time64(NULL));
    start = &g_city.intersections[g_city.roads[0].ends[0]].pos;
    g_camera[0] = start->x;
    g_camera[2] = start->z;
    g_camera[1] = (float)((double)start->y + 16.0);
    g_camera[7] = (float)((double)g_game_world.spawn.x + 32.0);
    g_camera[8] = (float)((double)g_game_world.spawn.y + 16.0);
    g_camera[9] = g_game_world.spawn.z;
    g_camera[10] = -1.5707963705062866f;
    g_camera[11] = 0;
    if (!sr_server)
        return;
    if (sr_game.gametype == 1) {
        pos.x = 138.0f;
        pos.y = 26.0f;
        pos.z = 138.0f;
        sr_players[0].active = 1;
        sr_players[0].team = 0;
        resetorientation(rot);
        rotateorientation(rot, &rot[1], 3.1415927410125732f);
        sr_players[0].human_id = createhuman(&pos.x, &rot[0].x, 0);
        sr_humans[sr_players[0].human_id].axis = 3.1415927410125732f;
        sr_players[0].input_mode = 0;
        sr_players[0].unused_cc = 0;
        resetorientation(rot);
        test_inventory(0, &pos, rot, 1);
    }
    if (!sr_server)
        return;
    resetorientation(rot);
    rotateorientation(rot, g_yaxis, -1.5707963705062866f);
    pos = g_track[74].pos;
    pos.y = (float)((double)pos.y + 1.625);
    createvehicle(0, &pos, rot);
    pos = g_track[2].pos;
    rotateorientation(rot, g_yaxis, 3.1415927410125732f);
    pos.y = (float)((double)pos.y + 1.625);
    i = createvehicle(0, &pos, rot);
    sr_objects[i].track_target = 74;
    if (!sr_server)
        return;
    if (sr_game.gametype == 0 || sr_game.gametype == 2)
        test_players();
    if (!sr_server)
        return;
    economy_carmarket();
    economy_gunmarket();
    createevent_updatestocks();
    j = g_numofheadlines;
    for (i = 0; i < j; ++i) {
        sr_createevent_message(3, g_headlines[i], i, 0);
        sr_createevent_message(3, g_headlines[i] + 0x3f, i, 1);
    }
    for (i = 0; i < 256; ++i)
        if (sr_players[i].active) {
            createevent_updateplayer(i);
            createevent_updateplayer_finance(i);
        }
}

static void asset_list(int *ids, int offset, int count, int *next)
{
    int i;
    for (i = 0; i < count; ++i)
        ids[offset / 4 + i] = (*next)++;
}

void setupgame(void)
{
    static const int texture_first[] = {0x404,0x408,0x40c,0x410,0x414,0x41c,0x420,0x418,0x424,0x428,0x42c,0x430,
        0x1034,0x1038,0x103c,0x1040,0x1044,0x1048,0x104c,0x1050};
    static const int texture_mid[] = {0x10d4,0x1114,0x1118,0x111c,0x1120,0x1124,0x1128,0x112c,0x1130};
    static const int texture_last[] = {0x10d8,0x10dc,0x10e0,0x10e4,0x10e8,0x10ec,0x10f0,0x10f4,0x10f8,
        0x10fc,0x1100,0x1104,0x1108,0x110c,0x1110,0x1134,0x1138,0x113c,0x1140};
    static const int sound_offset[] = {4,8,12,16,20,24,28,32,36,40,0x34,0x38,0x3c,0x40,0x44,0x48,0x4c,0x50,0x54,
        0x78,0x7c,0x88,0x8c,0x98,0x9c,0xb8,0xbc,0xc8,0xcc,0xd0,0xd4,0xd8,0xdc,0xe0,0xe4,0xe8,0xec,0xf0,
        0x10c,0x12c,0x130,0x134,0x138,0xa8,0xac,0xb0,0xb4};
    int i, next = 0, result;
    asset_list(g_modellist, 4, 32, &next);
    asset_list(g_modellist, 0x84, 32, &next);
    asset_list(g_modellist, 0x104, 32, &next);
    asset_list(g_modellist, 0x184, 256, &next);
    for (i = 0; i < 32; ++i) {
        g_modellist[0x584 / 4 + i] = next++;
        g_modellist[0x604 / 4 + i] = next++;
    }
    asset_list(g_modellist, 0x684, 32, &next);
    g_modellist[0x790 / 4] = next++;
    asset_list(g_modellist, 0x704, 3, &next);
    asset_list(g_modellist, 0x710, 32, &next);
    g_modellist[0x7a0 / 4] = next++;
    g_modellist[0] = next;
    next = 0;
    asset_list(g_texturelist, 4, 256, &next);
    for (i = 0; i < (int)(sizeof(texture_first) / sizeof(texture_first[0])); ++i)
        g_texturelist[texture_first[i] / 4] = next++;
    asset_list(g_texturelist, 0x834, 512, &next);
    for (i = 0; i < (int)(sizeof(texture_mid) / sizeof(texture_mid[0])); ++i)
        g_texturelist[texture_mid[i] / 4] = next++;
    asset_list(g_texturelist, 0x1054, 32, &next);
    for (i = 0; i < (int)(sizeof(texture_last) / sizeof(texture_last[0])); ++i)
        g_texturelist[texture_last[i] / 4] = next++;
    g_texturelist[0] = next;
    strcpy(g_materials[0].name, "concrete");
    g_materiallist.preset_end = 21;
    g_materiallist.num_materials = 21;
    for (i = 0; i < 4; ++i) {
        int id = g_materiallist.preset_end;
        if (i == 1)
            strcpy(g_materials[id].name, "brick");
        else if (i == 2)
            strcpy(g_materials[id].name, "cinder");
        g_materiallist.presets[i * 10] = id;
        ++g_materiallist.num_materials;
        g_materiallist.presets[i * 10 + 2] = g_materiallist.num_materials;
        g_materiallist.preset_end = ++g_materiallist.num_materials;
        g_materials[g_materiallist.presets[i * 10 + 2]].flags = 1;
    }
    g_materiallist.tile_material = 30;
    strcpy(g_materials[30].name, "grass");
    g_materiallist.street_x_left = 31;
    g_materiallist.street_x_right = 32;
    g_materiallist.street_z_left = 33;
    g_materiallist.street_z_right = 34;
    g_materiallist.carpet = 35;
    strcpy(g_materials[35].name, "carpet");
    g_materiallist.flagged_material = 36;
    g_materiallist.misc_2a0 = 37;
    g_materiallist.misc_2a4 = 38;
    g_materiallist.ceiling = 39;
    strcpy(g_materials[39].name, "ceiling0");
    g_materiallist.misc_2c4 = 40;
    g_materiallist.misc_2b0 = 41;
    g_materiallist.marble = 42;
    strcpy(g_materials[42].name, "marble");
    g_materiallist.sign = 43;
    strcpy(g_materials[43].name, "sign");
    g_materiallist.num_materials = 44;
    os_chdir("data");
    result = os_chdir("texture");
    listfiles("*.png", 0, 0);
    removeflieextentions(0);
    if (result == 0) {
        os_chdir("..");
        os_chdir("..");
    }
    next = g_filelist.list[0].count;
    for (i = 0; i < next; ++i) {
        int id = g_materiallist.num_materials;
        strcpy(g_materials[id].name, g_filelist.list[0].name[i]);
        g_materiallist.file_materials[i] = id;
        ++g_materiallist.num_materials;
    }
    for (i = 0; i < 47; ++i)
        ((int *)&g_soundlist)[sound_offset[i] / 4] = i;
    g_soundlist.crash = 20;
    g_soundlist.humanhit = 22;
    g_soundlist.phone_ring = 27;
    for (i = 0; i < 10; ++i)
        g_soundlist.digit[i] = i + 28;
    g_soundlist.magazine = 38;
    g_soundlist.gear = 40;
    g_soundlist.num_sounds = 47;
    setupbuildingtypes();
    area_createsblocks();
    setupbody();
    setupbullettypes();
    setupitemtypes();
    setupobjecttypes_cars();
    setupobjecttypes_helicopters();
    setupobjectypes_train();
    setupcityobjecttypes();
    for (i = 0; i < 256; ++i) {
        g_materials[i].parameters[0] = 0.25f;
        g_materials[i].parameters[1] = 0.25f;
        g_materials[i].parameters[2] = 1.0f;
        g_materials[i].parameters[3] = 1.2f;
    }
    g_materials[g_materiallist.tile_material].parameters[3] = 0.9f;
    g_materials[17].parameters[0] = g_materials[17].parameters[1] = 0.25f;
    g_materials[1].parameters[0] = g_materials[1].parameters[1] = 0.25f;
    g_materials[20].parameters[0] = g_materials[20].parameters[1] = 0.25f;
    g_materials[1].flags = 1;
    g_materials[20].flags = 1;
    g_materials[17].flags = 1;
    g_materials[g_materiallist.flagged_material].flags = 1;
    g_materials[g_materiallist.preset_end].flags = 1;
}
