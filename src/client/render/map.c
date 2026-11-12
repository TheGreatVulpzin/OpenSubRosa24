/* The map of the city: a picture seen from above, drawn into a texture of 512 by 512 pixels. */
#include "render/map.h"

#include "client.h"
#include "render/draw2d.h"
#include "render/indicators.h"
#include "render/text.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"

#define g_textures cl_data_texture
#define g_texturelist sr_data_texturelist
#define g_view_player cl_data_view_player
#define g_keyboard cl_data_keyboard

/* Game functions of other files. */
#define setuptexture CL_FN(0x50ac40, void(__cdecl *)(int texture))
#define setuporthoviewport CL_FN(0x506480, void(__cdecl *)(int x, int y, int width, int height, float half_width, float half_height, float depth))
#define setupviewpoint CL_FN(0x5065d0, void(__cdecl *)(const vec3 *position, const vec3 *rotation))
#define render_sectorlist CL_FN(0x4d7ed0, void(__cdecl *)(int mode))
#define clearsectorlist CL_FN(0x4d7850, void(__cdecl *)(int mode))
#define setuptextdisplay CL_FN(0x5079d0, void(__cdecl *)(void))

enum {
    MAP_SIZE = 512,
    SCREEN_WIDTH = 0x400,
    SCREEN_HEIGHT = 0x280,
    GL_RGB8 = 0x8051,
    GL_COLOR_AND_DEPTH = 0x4100,
    GL_LEQUAL = 0x0203,
    GL_CULL_FACE = 0x0b44,
    GL_CW = 0x0900,
    SECTOR_MODE_DEPTH_ONLY = 2
};
#define MAP_CENTER 1152.0f
#define MAP_RANGE 1024.0f
#define QUARTER_TURN 1.5707963705062866

GL_IMPORT(glFrontFace, (uint32_t mode), (mode))
GL_IMPORT(glDepthFunc, (uint32_t function), (function))
GL_IMPORT(glClear, (uint32_t mask), (mask))
GL_IMPORT(glClearColor, (float red, float green, float blue, float alpha), (red, green, blue, alpha))
GL_IMPORT(glLineWidth, (float width), (width))

/* The overlay of the map. It is drawn on a square of 512 pixels, one pixel for each 4 units of
 * the city, with the corner of the city at 128 units. */
enum {
    OVERLAY_SIZE = 512,
    OVERLAY_TEXT_FLAGS_SAME_X = 0x105,
    OVERLAY_TEXT_FLAGS = 5,
    TEAM_SLOTS = 8,
    TEAMS_ON_MAP = 5,
    MAX_PLAYERS = 256,
    GAMETYPE_DRIVING = 2,
    NO_PLAYER = -1,
    KEY_WIDE_VIEW = 11,
    /* Offsets in the game block of the client. A team record has 0x117c bytes. */
    TEAM_RECORD_SIZE = 0x117c,
    TEAM_BOX_OFFSET = 0x54,
    TEAM_MEMBERS_OFFSET = 0x8c,
    CHECKPOINT_COUNT_OFFSET = 0x257ec,
    CHECKPOINT_LIST_OFFSET = 0x257f0,
    CHECKPOINT_SIZE = 12
};

/* A float or an int of the game block at a byte offset. */
static float game_float(unsigned offset)
{
    return *(const float *)((const char *)&sr_data_game + offset);
}

static int *game_int(unsigned offset)
{
    return (int *)((char *)&sr_data_game + offset);
}

/* The place of a point of the city on the overlay, for one axis. */
static float overlay_axis(float value)
{
    return (float)((value - 128.0) * 0.25);
}

/* Move a point in the middle of two intersections of a road to the overlay. */
static float road_axis(int first, int second)
{
    return (float)(((float)((float)(first + second) * 0.5 * 4.0 - 128.0)) * 0.25);
}

/* Draw the city from above and copy the picture into the map texture. The camera hangs over
 * the middle of the city and looks straight down. */
void __cdecl rendermap(void)
{
    int map = g_texturelist.map;
    Texture *texture = &g_textures[map];
    vec3 eye[4];

    texture->width = MAP_SIZE;
    texture->height = MAP_SIZE;
    texture->num_levels = 1;
    texture->format = GL_RGB;
    texture->internal_format = GL_RGB8;
    texture->wrap_s = GL_CLAMP_TO_EDGE;
    texture->wrap_t = GL_CLAMP_TO_EDGE;
    texture->mag_filter = GL_LINEAR;
    texture->min_filter = GL_LINEAR;
    os_free(texture->levels[0]);
    texture->levels[0] = 0;
    setuptexture(map);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(1);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_AND_DEPTH);
    eye[0].x = MAP_CENTER;
    eye[0].y = 0.0f;
    eye[0].z = MAP_CENTER;
    resetorientation(&eye[1]);
    rotateorientation(&eye[1], &eye[1], (float)QUARTER_TURN);
    setuporthoviewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, MAP_RANGE, MAP_RANGE, MAP_RANGE);
    setupviewpoint(&eye[0], &eye[1]);
    glViewport(0, 0, MAP_SIZE, MAP_SIZE);
    clearsectorlist(0);
    render_sectorlist(SECTOR_MODE_DEPTH_ONLY);
    glBindTexture(GL_TEXTURE_2D, texture->gl_name);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, MAP_SIZE, MAP_SIZE);
}

/* The checkpoints of a driving game: green until the viewed player reaches one. */
static void draw_checkpoints(void)
{
    int index;

    glLineWidth(2.0f);
    if (sr_data_game.gametype != GAMETYPE_DRIVING)
        return;
    for (index = 0; index < *game_int(CHECKPOINT_COUNT_OFFSET); ++index) {
        unsigned offset = CHECKPOINT_LIST_OFFSET + index * CHECKPOINT_SIZE;
        float x = (float)(game_float(offset) * 0.25);
        float y = (float)(game_float(offset + 8) * 0.25);

        if (g_view_player == NO_PLAYER ||
            ((1 << (index & 0x1f)) & sr_data_player[g_view_player].checkpoint_mask) == 0)
            drawcircle((float)(x + 0.0), (float)(y + 0.0), 10.0f, 0.0f, 0.75f, 0.0f, 1.0f);
        else
            drawcircle((float)(x + 0.0), (float)(y + 0.0), 10.0f, 0.25f, 0.25f, 0.25f, 1.0f);
    }
}

/* One circle for the base of each team, and a smaller one inside it when the team has players. */
static void draw_bases(void)
{
    int team;

    for (team = 0; team < TEAMS_ON_MAP; ++team) {
        unsigned box = TEAM_BOX_OFFSET + team * TEAM_RECORD_SIZE;
        float x = (float)(((game_float(box) + game_float(box + 12)) * 0.5 - 128.0) * 0.25);
        float y = (float)(((game_float(box + 8) + game_float(box + 20)) * 0.5 - 128.0) * 0.25);
        const float *color = &sr_data_teamcolor[team * 3];

        drawcircle((float)(x + 0.0), (float)(y + 0.0), 16.0f, color[0], color[1], color[2], 1.0f);
        if (*game_int(TEAM_MEMBERS_OFFSET + team * TEAM_RECORD_SIZE) > 0)
            drawcircle((float)(x + 0.0), (float)(y + 0.0), 12.0f, color[0], color[1], color[2], 1.0f);
    }
}

/* The name of each road, at the middle of the road. The text of a road along x is centered. */
static void draw_road_names(void)
{
    int index;

    glLineWidth(1.0f);
    for (index = 0; index < sr_data_city.num_roads; ++index) {
        const CityRoad *road = &sr_data_city.roads[index];
        int first_x = sr_data_city.intersections[road->ends[0]].x;
        int second_x = sr_data_city.intersections[road->ends[1]].x;
        float x = road_axis(first_x, second_x);
        float y = road_axis(sr_data_city.intersections[road->ends[0]].z, sr_data_city.intersections[road->ends[1]].z);

        drawtext(road->name, (float)(x + 0.0), (float)(y + 0.0), 6.0f,
                 first_x == second_x ? OVERLAY_TEXT_FLAGS_SAME_X : OVERLAY_TEXT_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f);
    }
}

/* Draw the map of the city with the players, the bases, the checkpoints and the names of the
 * roads. The picture follows the human of the player. The number -1 has no human: the code
 * then reads the record before the first human and draws no pointer. */
void __cdecl drawmap(int player)
{
    float scale = 0.25f;
    vec3 eye;
    vec3 rotation[3];
    int index;

    if (g_keyboard[KEY_WIDE_VIEW] != 0)
        scale = 0.125f;
    eye.x = overlay_axis(sr_data_human[player].pos.x);
    eye.y = overlay_axis(sr_data_human[player].pos.z);
    eye.z = 0.0f;
    resetorientation(rotation);
    rotation[1].x = -rotation[1].x;
    rotation[1].y = -rotation[1].y;
    rotation[1].z = -rotation[1].z;
    if (eye.x < scale * 512.0)
        eye.x = (float)(scale * 512.0);
    if (eye.y < scale * 512.0)
        eye.y = (float)(scale * 512.0);
    if (320.0 - scale * 512.0 < eye.x)
        eye.x = (float)(320.0 - scale * 512.0);
    if (256.0 - scale * 512.0 < eye.y)
        eye.y = (float)(256.0 - scale * 512.0);
    setuporthoviewport(0, 0, OVERLAY_SIZE, OVERLAY_SIZE, (float)(scale * 512.0), (float)(scale * 512.0), 512.0f);
    setupviewpoint(&eye, rotation);
    for (index = 0; index < TEAM_SLOTS; ++index)
        *game_int(TEAM_MEMBERS_OFFSET + index * TEAM_RECORD_SIZE) = 0;
    for (index = 0; index < MAX_PLAYERS; ++index) {
        int team = sr_data_player[index].team;

        if (sr_data_player[index].active != 0 && -1 < team && team < TEAMS_ON_MAP)
            ++*game_int(TEAM_MEMBERS_OFFSET + team * TEAM_RECORD_SIZE);
    }
    drawbitmap(g_texturelist.map, 0.0f, 0.0f, 512.0f, 512.0f, 0, 1.0f, 1.0f, 1.0f, 0.5f);
    draw_checkpoints();
    draw_bases();
    draw_road_names();
    if (player != NO_PLAYER)
        drawplayerindicator((float)((sr_data_human[player].pos.x - 128.0) * 0.25 + 0.0),
                            (float)((sr_data_human[player].pos.z - 128.0) * 0.25 + 0.0), 4.0f, 1.0f, 1.0f, 1.0f,
                            0.75f, sr_data_human[player].axis);
    setuptextdisplay();
    drawtext("Press C to toggle info", 512.0f, 0.0f, 20.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f);
}
