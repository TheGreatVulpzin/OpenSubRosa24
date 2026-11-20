/* The start of a round on the client.
 *
 * cl_resetgame shows "Generating...", clears the round state, builds the level and the map,
 * and, on a client that is also the server, makes the test players, the vehicles and the
 * events of the new round. A plain client takes the round number from the server.
 */
#include "setup/reset.h"

#include <stdint.h>
#include <string.h>

#include "audio/imports.h"
#include "audio/sound.h"
#include "buildings.h"
#include "client.h"
#include "economy.h"
#include "humans.h"
#include "items.h"
#include "objects.h"
#include "render/lists.h"
#include "render/map.h"
#include "render/text.h"
#include "render/view.h"
#include "scenario.h"
#include "sr_limits.h"
#include "sr_math.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"
#include "../../net/events.h"

GL_IMPORT(glClearColor, (float red, float green, float blue, float alpha), (red, green, blue, alpha))
GL_IMPORT(glClear, (uint32_t mask), (mask))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_GL_SwapWindow, (void *))

#define g_game sr_data_game
#define g_server sr_data_server
#define g_players sr_data_player
#define g_city sr_data_city
#define g_camera sr_data_camera

/* Game functions of other files. */
#define createlevel CL_FN(0x49b740, void(__cdecl *)(void))
#define createvehicle CL_FN(0x4fb3d0, int(__cdecl *)(int type, const vec3 *pos, const vec3 *rot))
#define createscenario_driving CL_FN(0x4dc790, void(__cdecl *)(void))
#define createevent_message CL_FN(0x472000, void(__cdecl *)(int type, const char *text, int line, int part))
#define createevent_updatestocks CL_FN(0x472b50, void(__cdecl *)(void))
#define saveaccountsserver CL_FN(0x434010, void(__cdecl *)(void))

/* The game types and the round states. */
enum { GAME_DEMO = 0, GAME_EDITOR = 1, GAME_DEMO_2 = 2, GAME_ROUND = 3, GAME_WORLD = 4 };
enum { STATE_WAIT = 1, STATE_PLAY = 2 };

#define GL_CLEAR_MASK 0x4100 /* the color and the depth buffers */
#define TEXT_CENTER 1        /* the flag of drawtext for a text with its middle at x */
#define NUM_TEAMS 5
#define NUM_MISSIONS 16
#define NUM_CHAT_MESSAGES 256
#define NUM_VOICE_CHANNELS 8
#define NUM_CONNECTION_VOICES 8
#define NUM_TEST_PLAYERS 4
#define TEAM_RECORD_SIZE 0x117c
#define TEAM_NONE 7
#define MENU_NONE 0
#define MENU_ROUND_START 2
#define MOVE_WALK 1
#define START_PRICE 100.0f
#define START_SHARES 100
#define START_ANGLE_MAX 0.39269908169875 /* pi / 8: the sun starts between pi / 8 and -pi / 8 */
#define START_ANGLE_STEPS 256.0
#define START_ANGLE_B 0.5f
#define CAMERA_RISE 16.0
#define SPAWN_VIEW_SIDE 32.0
#define PI_F 3.1415927410125732f
#define HALF_PI_F 1.5707963705062866f
#define HEADLINE_SECOND_LINE 0x3f /* the offset of the second line of a headline */
#define EVENT_HEADLINE 3
#define CITY_BUILDING_SIZE 0x9dc   /* the city keeps the fields of building n at n * this size */
#define BUILDING_MISSION 3
#define MISSION_GUARDS 22
#define MISSION_PRIZE 10000000
#define TRAIN_STATION 74           /* the track piece of the station */
#define TRAIN_START 2              /* the track piece of the second train */
#define TRAIN_HEIGHT 1.625
#define VEHICLE_TRAIN 0
#define OBJECT_TEST_CAR 6
#define PHONE_NUMBER_STEP 1111     /* the phone of team n has number n * 1111 */
#define TEST_BILLS_LAST 9
#define RED_DISK_SLOT 4
#define NUM_TEAM_PHONES 2
#define NUM_CASH_STACKS 2
#define EDITOR_WEAPONS 5        /* the editor player gets all test weapons */
#define DEMO_WEAPONS 4          /* the demo player gets them without the magnum */
#define PI_D 3.1415927410125732
#define TEST_CAR 1              /* scenario_createcar: type, color, road, side, lane, distance */
#define TEST_CAR_COLOR 1
#define TEST_CAR_LANE 1
#define TEST_CAR_DISTANCE 64.0f
#define TRAFFIC_CAR 11
#define TRAFFIC_CAR_COLORS 6
#define TRAFFIC_SIDE 1
#define TRAFFIC_LANE 1
#define TRAFFIC_DISTANCE 80.0f
#define BOT_ROW_X 280.0         /* the bots stand in a row: x = 280 + 2 * player */
#define BOT_ROW_Y 26.0f
#define BOT_ROW_Z 192.0f
#define SPAWN_RISE 2.0          /* the test humans stand 2 units above the first crossing */
#define PLAYER_SPAWN_BACK 64.0
#define BOT_SPAWN_SIDE 8.0
#define BOT_SPAWN_BACK 80.0
#define ITEM_ROW_STEP 1.0
#define ITEM_ROW_SIDE 2.0
#define DOOR_SIDE 1.5
#define DOOR_DROP 0.125
#define TEXT_X 512.0f
#define TEXT_Y 32.0f
#define TEXT_SIZE 16.0f

/* The field of team t: the team records follow the record of team 0. */
#define TEAM(field, team) ((void *)((uint8_t *)&g_game.field + (team) * TEAM_RECORD_SIZE))

/* The weapons of the test player: an M16, three magazines and a magnum, and their slots. */
static const int test_weapons[EDITOR_WEAPONS] = {ITEM_M16, ITEM_M16_MAGAZINE, ITEM_M16_MAGAZINE, ITEM_M16_MAGAZINE, ITEM_MAGNUM};
static const int test_weapon_slots[EDITOR_WEAPONS] = {0, 1, 3, 3, 2};

/* The bills of the two test stacks of cash: one bit for each bill. */
static const uint32_t test_bills[NUM_CASH_STACKS] = {0x06006001u, 0x0402b001u};

/* The items in a row next to the test player. */
static const int test_row[] = {ITEM_BRIEFCASE, ITEM_BANDAGE};
static const int test_row_end[] = {ITEM_WHITE_DISK, ITEM_BLUE_DISK, ITEM_AK47, ITEM_AK47_MAGAZINE,
                                   ITEM_AK47_MAGAZINE, ITEM_AK47_MAGAZINE};

/* The places of the camera at the start, of the test humans and of the test player of the
 * editor. */
static const vec3 camera_start = {160.0f, 32.0f, 128.0f};
static const vec3 editor_spawn = {138.0f, 26.0f, 138.0f};
static const vec3 test_spawn_2 = {206.5f, 25.25f, 143.5f};
static const vec3 bot_waypoint = {281.0f, 26.0f, 128.0f};

/* Show "Generating..." while the level builds. */
static void reset_screen(void)
{
    glClearColor(sr_data_menucolor[0], sr_data_menucolor[1], sr_data_menucolor[2], 0.0f);
    glClear(GL_CLEAR_MASK);
    setuptextdisplay();
    drawtext("Generating...", TEXT_X, TEXT_Y, TEXT_SIZE, TEXT_CENTER, 1.0f, 1.0f, 1.0f, 1.0f);
    audio_SDL_GL_SwapWindow()(cl_data_audio_window);
}

/* The teams and the stock market of a new round. */
static void reset_teams(void)
{
    int i, j;

    for (i = 0; i < NUM_TEAMS; i++) {
        Mission *missions = (Mission *)TEAM(team0_missions, i);

        *(int *)TEAM(team0_unused_8c, i) = 0;
        *(int *)TEAM(team0_members, i) = 0;
        *(int *)TEAM(team0_cash, i) = 0;
        *(int *)TEAM(team0_manager, i) = -1;
        for (j = 0; j < NUM_MISSIONS; j++)
            missions[j].active = 0;
    }
    for (i = 0; i < NUM_TEAMS; i++) {
        Stock *s = &sr_data_market.stocks[i];

        if (g_game.gametype == GAME_WORLD || g_game.round_number == 0) {
            s->price = START_PRICE;
            s->price_int = cl_ftoi(s->price);
            s->shares = START_SHARES;
        }
        if (g_game.gametype == GAME_ROUND)
            s->open_price = (float)s->price_int;
    }
}

/* Clear the players of the round. A bot leaves. */
static void reset_players(void)
{
    int i;

    for (i = 0; i < SR_MAX_PLAYER; i++) {
        Player *p = &g_players[i];

        if (p->is_bot != 0)
            p->active = 0;
        p->num_purchases = 0;
        p->unused_74 = 0;
        p->num_actions = 0;
        p->action_tail = 0;
        p->move_mode = MOVE_WALK;
        p->human_id = -1;
        if (g_game.gametype == GAME_WORLD) {
            p->stocks = 0;
            p->team = TEAM_NONE;
        }
        if (g_game.gametype == GAME_DEMO)
            p->team = 0;
        p->ready = 0;
        p->menu = g_game.gametype == GAME_ROUND ? MENU_ROUND_START : MENU_NONE;
        p->checkpoints = 0;
        p->checkpoint_mask = 0;
        p->round_points = 0;
        p->wait_isec = -1;
        sr_data_speex.voice_ring[i].voice_off = 1;
    }
}

/* Clear the records and the lists of the world. */
static void reset_world(void)
{
    int i, j;

    for (i = 0; i < SR_MAX_HUMAN; i++)
        sr_data_human[i].active = 0;
    for (i = 0; i < SR_MAX_OBJECT; i++)
        sr_data_object[i].active = 0;
    for (i = 0; i < SR_MAX_ITEM; i++)
        sr_data_item[i].active = 0;
    for (i = 0; i < SR_MAX_ITEM; i++)
        sr_data_cityobject[i].active = 0;
    sr_data_numoftracks = 0;
    sr_data_numofbullets = 0;
    sr_data_numofsprites = 0;
    sr_data_numofsprite_models = 0;
    sr_data_sprite_points = 0;
    sr_data_sprite_lines = 0;
    sr_data_sprite_quads = 0;
    sr_data_sprite_triangles = 0;
    sr_data_tiremarkstart = 0;
    sr_data_tiremarkend = 0;
    sr_data_numofevents = 0;
    sr_data_blocktemp.unused_7602c = 0;
    for (i = 0; i < sr_data_numofconnections; i++) {
        sr_data_connection[i].event_ack = 0;
        for (j = 0; j < NUM_CONNECTION_VOICES; j++)
            sr_data_connection[i].voice[j].active = 0;
    }
    for (i = 0; i < NUM_VOICE_CHANNELS; i++)
        sr_data_voice[i].active = 0;
}

/* The light comes from the two start angles of the round. */
static void reset_light(void)
{
    float *light = cl_data_light_position;
    double a = (double)g_game.start_angle_a, b = (double)g_game.start_angle_b;

    light[0] = (float)(x87_sin(a) * x87_cos(b));
    light[1] = (float)x87_cos(a);
    light[2] = (float)(x87_sin(a) * x87_sin(b));
    light[3] = 0.0f;
}

/* The place of the first crossing of the first road. */
static const vec3 *first_crossing(void)
{
    return &g_city.intersections[g_city.roads[0].ends[0]].pos;
}

/* Give the human of player 0 the test weapons. */
static void give_weapons(const vec3 *pos, const vec3 *rot, int count)
{
    int k;

    for (k = 0; k < count; k++)
        linkitem(createitem(test_weapons[k], pos, NULL, rot), UINT32_MAX, g_players[0].human_id, test_weapon_slots[k]);
}

/* Put the test items in a row along z next to the test player: two team phones, a briefcase, a
 * bandage, two stacks of cash, two disks and an AK47 with three magazines. With debugit, a door
 * goes to the side. */
static void test_items(vec3 *pos, const vec3 *rot)
{
    int it, k;

    pos->x = (float)(pos->x + ITEM_ROW_SIDE);
    createobject(OBJECT_TEST_CAR, pos, NULL, rot, 1);
    for (k = 1; k <= NUM_TEAM_PHONES; k++) {
        it = createitem(ITEM_CELL_PHONE, pos, NULL, rot);
        sr_data_item[it].phone_id = k * PHONE_NUMBER_STEP;
        pos->z = (float)(pos->z + ITEM_ROW_STEP);
    }
    for (k = 0; k < (int)(sizeof test_row / sizeof test_row[0]); k++) {
        createitem(test_row[k], pos, NULL, rot);
        pos->z = (float)(pos->z + ITEM_ROW_STEP);
    }
    for (k = 0; k < NUM_CASH_STACKS; k++) {
        it = createitem(ITEM_CASH_2, pos, NULL, rot);
        sr_data_item[it].bills_mask = test_bills[k];
        sr_data_item[it].last_bill = TEST_BILLS_LAST;
        pos->z = (float)(pos->z + ITEM_ROW_STEP);
    }
    for (k = 0; k < (int)(sizeof test_row_end / sizeof test_row_end[0]); k++) {
        createitem(test_row_end[k], pos, NULL, rot);
        pos->z = (float)(pos->z + ITEM_ROW_STEP);
    }
    pos->x = (float)(pos->x + DOOR_SIDE);
    pos->y = (float)((float)(pos->y + DOOR_DROP) - ITEM_ROW_STEP);
    if (sr_data_debugit != 0)
        createitem(ITEM_DOOR, pos, NULL, rot);
}

/* The test players of the demo game types: the player near the first crossing, a bot that
 * drives to a waypoint, a third human with a red disk and a fourth one. In the second demo the
 * mission building gets guards. */
static void test_players(void)
{
    vec3 pos, rot[3];
    int i, building;

    for (i = 0; i < NUM_TEST_PLAYERS; i++) {
        Player *p = &g_players[i];

        pos.x = (float)((float)i + (float)i + BOT_ROW_X);
        pos.y = BOT_ROW_Y;
        pos.z = BOT_ROW_Z;
        if (i == 0) {
            pos = *first_crossing();
            pos.y = (float)(pos.y + SPAWN_RISE);
            pos.z = (float)(pos.z + PLAYER_SPAWN_BACK);
        }
        if (i == 1) {
            pos = *first_crossing();
            pos.x = (float)(pos.x - BOT_SPAWN_SIDE);
            pos.y = (float)(pos.y + SPAWN_RISE);
            pos.z = (float)(pos.z + BOT_SPAWN_BACK);
        }
        if (i == 2)
            pos = test_spawn_2;
        p->active = 1;
        if (i > 0)
            p->is_bot = 1;
        p->team = 0;
        resetorientation(rot);
        p->human_id = createhuman(&pos.x, &rot[0].x, i);
        if (i == 0) {
            resetorientation(rot);
            give_weapons(&pos, rot, DEMO_WEAPONS);
            test_items(&pos, rot);
        }
        if (i == 2)
            linkitem(createitem(ITEM_RED_DISK, &pos, NULL, rot), UINT32_MAX, g_players[2].human_id, RED_DISK_SLOT);
        if (i == 1) {
            p->ai_waypoint = 0;
            p->waypoints[0].pos = bot_waypoint;
            p->num_path = 1;
        }
    }
    if (g_city.num_buildings > 0) {
        for (building = 0; building < g_city.num_buildings; building++)
            if (*(int *)((uint8_t *)&g_city.building_type + building * CITY_BUILDING_SIZE) == BUILDING_MISSION)
                break;
        if (g_game.gametype == GAME_DEMO_2)
            scenario_createguards(building, MISSION_GUARDS, MISSION_PRIZE);
    }
    if (sr_data_debugit != 0)
        scenario_createcar(TEST_CAR, TEST_CAR_COLOR, 0, 0, TEST_CAR_LANE, TEST_CAR_DISTANCE);
    scenario_createcar(TRAFFIC_CAR, os_rand() % TRAFFIC_CAR_COLORS, 0, TRAFFIC_SIDE, TRAFFIC_LANE, TRAFFIC_DISTANCE);
}

/* The two trains: one at the station, one at the start of the track that drives to the
 * station. */
static void place_trains(void)
{
    vec3 pos, rot[3];
    int i;

    resetorientation(rot);
    rotateorientation(rot, &sr_data_yaxis, -HALF_PI_F);
    pos = sr_data_track[TRAIN_STATION].pos;
    pos.y = (float)(pos.y + TRAIN_HEIGHT);
    createvehicle(VEHICLE_TRAIN, &pos, rot);
    pos = sr_data_track[TRAIN_START].pos;
    rotateorientation(rot, &sr_data_yaxis, PI_F);
    pos.y = (float)(pos.y + TRAIN_HEIGHT);
    i = createvehicle(VEHICLE_TRAIN, &pos, rot);
    sr_data_object[i].track_target = TRAIN_STATION;
}

/* The test player of the editor game type. */
static void editor_player(void)
{
    vec3 pos = editor_spawn, rot[3];

    g_players[0].active = 1;
    g_players[0].team = 0;
    resetorientation(rot);
    rotateorientation(rot, &rot[1], PI_F);
    g_players[0].human_id = createhuman(&pos.x, &rot[0].x, 0);
    sr_data_human[g_players[0].human_id].axis = PI_F;
    g_players[0].input_mode = 0;
    g_players[0].unused_cc = 0;
    resetorientation(rot);
    give_weapons(&pos, rot, EDITOR_WEAPONS);
}

/* Start a new round. */
void __cdecl cl_resetgame(void)
{
    int server = sr_data_server.active != 0;
    int i;

    g_server.ban_candidate = -1;
    reset_screen();
    for (i = cl_data_audio_sound_count; i >= 0; i--)
        deletesound(i);
    cl_data_input_cursor = g_game.gametype == GAME_ROUND;
    for (i = 0; i < NUM_CHAT_MESSAGES; i++)
        if (cl_data_chatmessage[i].kind != 0) {
            cl_data_chatmessage[i].delay = 0;
            cl_data_chatmessage[i].timer = 0;
        }
    if (g_game.gametype == GAME_ROUND && server)
        saveaccountsserver();
    g_game.timer = 0;
    if (g_game.gametype == GAME_ROUND)
        g_game.timer = g_game.wait_time;
    g_game.state = STATE_WAIT;
    if (g_game.gametype == GAME_DEMO || g_game.gametype == GAME_DEMO_2 || g_game.gametype == GAME_EDITOR) {
        g_game.state = STATE_PLAY;
        g_game.timer = g_game.round_time;
    }
    memcpy(g_camera, &camera_start, sizeof camera_start);
    g_game.unused_257ec = 0.0f;
    g_game.unused_257e8 = 0;
    g_game.spectator_human = -1;
    g_game.death_camera_ticks = 0;
    reset_teams();
    g_game.unused_25978 = 0;
    g_game.unused_25974 = 0;
    g_game.num_newspapers = 0;
    g_game.unused_22fd8 = 0;
    if (!server)
        g_game.round_number = sr_data_area_sector.client_game_id;
    else
        g_game.round_number++;
    g_game.ticks = 0;
    sr_data_area_sector.unused_0 = 0;
    sr_data_newspaper.texture_done = 0;
    if (!server)
        sr_data_newspaper.num_lines = 0;
    else
        newspaper_load();
    if (server) {
        unsigned step = (unsigned)os_rand() & 0xff;

        g_game.start_angle_a = (float)(START_ANGLE_MAX - ((float)step * PI_D * 0.25) / START_ANGLE_STEPS);
        g_game.start_angle_b = START_ANGLE_B;
    }
    sr_data_area_sector.num_particles = 0;
    sr_data_numofbonds = 0;
    reset_players();
    reset_world();
    reset_light();
    createlevel();
    rendermap();
    render_buildareasectorshadow();
    os_srand((unsigned)os_time64(NULL));
    if (server && g_game.gametype == GAME_DEMO_2)
        createscenario_driving();
    memcpy(g_camera, first_crossing(), sizeof(vec3));
    g_camera[1] = (float)(g_camera[1] + CAMERA_RISE);
    g_camera[7] = (float)(sr_data_game_world.spawn.x + SPAWN_VIEW_SIDE);
    g_camera[8] = (float)(sr_data_game_world.spawn.y + CAMERA_RISE);
    g_camera[9] = sr_data_game_world.spawn.z;
    g_camera[10] = -HALF_PI_F;
    g_camera[11] = 0.0f;
    if (server && g_game.gametype == GAME_EDITOR)
        editor_player();
    if (server)
        place_trains();
    if (server && (g_game.gametype == GAME_DEMO || g_game.gametype == GAME_DEMO_2))
        test_players();
    if (!server)
        return;
    economy_carmarket();
    economy_gunmarket();
    createevent_updatestocks();
    for (i = 0; i < sr_data_newspaper.num_lines; i++) {
        createevent_message(EVENT_HEADLINE, sr_data_newspaper.lines[i], i, 0);
        createevent_message(EVENT_HEADLINE, sr_data_newspaper.lines[i] + HEADLINE_SECOND_LINE, i, 1);
    }
    for (i = 0; i < SR_MAX_PLAYER; i++)
        if (g_players[i].active != 0) {
            createevent_updateplayer(i);
            createevent_updateplayer_finance(i);
        }
}
