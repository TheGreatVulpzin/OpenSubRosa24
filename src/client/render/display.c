/* The display of the game on the 2D layer: the stock prices and the missions of the map page,
 * the clock and the timers, the player list, the money of the player and the chat lines. */
#include "render/display.h"

#include <stdio.h>
#include <string.h>

#include "client.h"
#include "render/draw2d.h"
#include "render/player_info.h"
#include "render/text.h"
#include "sr_runtime.h"

#define g_view_player cl_data_view_player
#define g_game_state cl_data_game_state
#define g_keyboard cl_data_keyboard
#define g_map_shown cl_data_map_shown
#define g_fontnum cl_data_fontnum
#define g_debug_toggle cl_data_input_debug_toggle
#define g_chat_head cl_data_chat_head
#define g_chatmessage cl_data_chatmessage
#define g_display_text cl_data_display_text
#define g_round_ticks cl_data_round_ticks
#define g_round_goal cl_data_round_goal
#define g_input_blocked cl_data_audio_input_blocked

enum {
    NO_PLAYER = -1,
    NO_TEAM = -1,
    TEAMS_ON_MAP = 5,
    TEAMS_WITH_MISSIONS = 7,
    GAMETYPE_TEAMS = 3,
    GAMETYPE_DAY_CYCLE = 4,
    STATE_START = 0,
    STATE_WAIT = 1,
    STATE_PLAY = 2,
    STATE_RESTART = 3,
    ROUND_MODE_TIMED = 3,
    MENU_PLAYER_LIST = 2,
    MAX_MISSIONS = 16,
    MISSION_LINE_HEIGHT = 16,
    TICKS_PER_MINUTE = 0xe10,
    MINUTES_PER_HOUR = 0x3c,
    TEAM_RECORD_SIZE = 0x117c,
    MISSION_OFFSET = 0xcc,
    BUILDING_RECORD_SIZE = 0x9dc,
    KEY_HOLD_LIST = 11,
    KEY_DEBUG_FIRST_TEAM = 13,
    KEY_DEBUG_THIRD_TEAM = 17,
    KEY_DEBUG_FOURTH_TEAM = 16,
    CHAT_RING = 0x100,
    CHAT_PLAYER_KIND = 0,
    CHAT_HUMAN_KIND = 1,
    CHAT_SYSTEM_KIND = 4,
    HOUR_CLOCK_MODULUS = 12
};
#define MISSION_X 520.0f
#define MISSION_STYLE 16.0f, 0, 0.0f, 0.0f, 0.0f, 1.0f
#define TIMER_STYLE 1020.0f, 4.0f, 24.0f, 2, 1.0f, 1.0f, 1.0f, 1.0f

/* The kinds of mission. */
enum { MISSION_FIND = 0, MISSION_LIMO = 1, MISSION_SELL = 2, MISSION_BUY = 3, MISSION_BUY_NOW = 4,
       MISSION_TRADE = 5, MISSION_TRADE_NOW = 6 };

/* The mission of a team in the game block of the client. */
static const Mission *team_mission(int team, int index)
{
    return (const Mission *)((const char *)&sr_data_game + MISSION_OFFSET + team * TEAM_RECORD_SIZE +
                             index * sizeof(Mission));
}

/* One line of a mission without values. */
static void mission_text(const char *text, float y)
{
    drawtext(text, MISSION_X, y, MISSION_STYLE);
}

/* The percent of the value that the stocks of the team rise by. */
static int stock_increase(const Mission *mission)
{
    return cl_ftoi((double)(mission->cash + 1) * 0.0024999999441206455);
}

/* The line with the value of a mission. */
static void mission_value(const Mission *mission, float y)
{
    drawtext("Value:               $/i Bonus", MISSION_X, y, MISSION_STYLE, mission->cash);
}

/* The place of the disk of a find mission: the name of the item and the two roads of the
 * building. */
static void mission_place(const Mission *mission, float y)
{
    const int *roads;

    if (mission->site == -1) {
        mission_text("We do not know where the disk is located.", y);
        return;
    }
    drawtext("The /s is in a building near the", MISSION_X, y, MISSION_STYLE, sr_itemtypes[mission->item_type].name);
    roads = (const int *)((const char *)&sr_data_city.street_roads + mission->site * BUILDING_RECORD_SIZE);
    if (roads[0] != -1 && roads[1] != -1)
        drawtext("/s and /s intersection", MISSION_X, (float)(y + 16.0), MISSION_STYLE, sr_data_city.roads[roads[0]].name,
                 sr_data_city.roads[roads[1]].name);
}

/* The text of one mission. The result is the next row. */
static float draw_mission(const Mission *mission, float y)
{
    int stock = stock_increase(mission);

    if (mission->kind == MISSION_FIND) {
        mission_text("Acquisition:", y);
        mission_text("We have received reports that Megacorp has disks containing", (float)(y + 16.0));
        y = (float)(y + 16.0 + 16.0);
        mission_text("information that will be beneficial to one of our subsidiaries.", y);
        y = (float)(y + 16.0);
        mission_place(mission, y);
        y = (float)(y + 16.0);
        mission_value(mission, (float)(y + 16.0));
        y = (float)(y + 16.0 + 16.0);
        drawtext("Estimated stock increase:         /i%", MISSION_X, y, MISSION_STYLE, stock);
        y = (float)(y + 16.0);
    }
    if (mission->kind == MISSION_LIMO) {
        mission_text("Acquisition:", y);
        mission_text("We have received reports that Megacorp has disks containing", (float)(y + 16.0));
        y = (float)(y + 16.0 + 16.0);
        mission_text("information that will be beneficial to one of our subsidiaries.", y);
        y = (float)(y + 16.0);
        drawtext("The /s is in a limo driving around randomly.", MISSION_X, y, MISSION_STYLE,
                 sr_itemtypes[mission->item_type].name);
        y = (float)(y + 16.0);
        mission_value(mission, y);
        y = (float)(y + 16.0);
        drawtext("Estimated stock increase:         /i%", MISSION_X, y, MISSION_STYLE, stock);
        y = (float)(y + 16.0);
    }
    if (mission->kind == MISSION_SELL) {
        mission_text("Transaction:", y);
        mission_text("We have acquired a disk that has information that can be", (float)(y + 16.0));
        y = (float)(y + 16.0 + 16.0);
        mission_text("traded for money with another corporation.", y);
        y = (float)(y + 16.0);
        mission_text("Negotiate the best terms and execute the deal.", y);
        y = (float)(y + 16.0);
        drawtext("/s value:               $/i Bonus", MISSION_X, y, MISSION_STYLE,
                 sr_itemtypes[mission->item_type].name, mission->cash);
        y = (float)(y + 16.0);
        drawtext("Estimated stock increase:         /i%", MISSION_X, y, MISSION_STYLE, stock);
        y = (float)(y + 16.0);
    }
    if (mission->kind == MISSION_BUY || mission->kind == MISSION_BUY_NOW) {
        mission_text("Transaction:", y);
        drawtext("We have received reports that /s has acquired a disk", MISSION_X, (float)(y + 16.0), MISSION_STYLE,
                 sr_data_teamname[mission->partner]);
        y = (float)(y + 16.0 + 16.0);
        mission_text("containing information that will be beneficial to one of our", y);
        y = (float)(y + 16.0);
        if (mission->kind == MISSION_BUY) {
            mission_text("subsidiaries.  Negotiate the best terms and execute the deal.", y);
            y = (float)(y + 16.0);
            drawtext("You have been provided with $/i.", MISSION_X, y, MISSION_STYLE, mission->stake);
        } else {
            mission_text("subsidiaries.  Acquire the disk by any means neccessary.", y);
        }
        y = (float)(y + 16.0);
        drawtext("/s value:               $/i Bonus", MISSION_X, y, MISSION_STYLE,
                 sr_itemtypes[mission->item_type].name, mission->cash);
        drawtext("Estimated stock increase:         /i%", MISSION_X, (float)(y + 16.0), MISSION_STYLE, stock);
        y = (float)(y + 16.0 + 16.0);
    }
    if (mission->kind == MISSION_TRADE || mission->kind == MISSION_TRADE_NOW) {
        mission_text("Three-way Deal:", y);
        drawtext("We have received reports that /s has acquired a disk", MISSION_X, (float)(y + 16.0), MISSION_STYLE,
                 sr_data_teamname[mission->partner]);
        y = (float)(y + 16.0 + 16.0);
        mission_text("containing information that will be beneficial to one of our", y);
        y = (float)(y + 16.0);
        if (mission->kind == MISSION_TRADE) {
            mission_text("subsidiaries.  We have a disk to trade with another", y);
            y = (float)(y + 16.0);
            mission_text("corporation.  Execute the deal.", y);
        } else {
            mission_text("subsidiaries.  Acquire the disk by any means neccessary.", y);
        }
        y = (float)(y + 16.0);
        drawtext("/s value:               $/i Bonus", MISSION_X, y, MISSION_STYLE,
                 sr_itemtypes[mission->item_type].name, mission->cash);
        drawtext("Estimated stock increase:         /i%", MISSION_X, (float)(y + 16.0), MISSION_STYLE, stock);
        y = (float)(y + 16.0 + 16.0);
    }
    return y;
}

/* The clock in hours of 12 and minutes, for a time in ticks. */
static void clock_parts(int ticks, int *hour, int *minute_tens, int *minute_ones)
{
    int minutes = (ticks / TICKS_PER_MINUTE) % MINUTES_PER_HOUR;

    *hour = ((ticks / TICKS_PER_MINUTE) / MINUTES_PER_HOUR) % HOUR_CLOCK_MODULUS;
    if (*hour == 0)
        *hour = HOUR_CLOCK_MODULUS;
    *minute_tens = (minutes / 10) % 6;
    *minute_ones = minutes % 10;
}

/* The stock prices of the five teams at the top of the map page. */
static void draw_stock_prices(void)
{
    float x = 520.0f;
    float y = 32.0f;
    int team;

    drawbitmap(-1, 512.0f, 32.0f, 512.0f, 512.0f, 0, 0.75f, 0.75f, 0.75f, 1.0f);
    g_fontnum = 1;
    for (team = 0; team < TEAMS_ON_MAP; ++team) {
        sprintf(g_display_text, "%s: $%d", sr_data_teamname[team], sr_data_market.stocks[team].price_int);
        drawtext(g_display_text, x, y, 16.0f, 0, 0.0f, 0.0f, 0.0f, 1.0f);
        x = (float)(x + 176.0);
        if (team == 2) {
            x = 520.0f;
            y = (float)(y + 16.0);
        }
    }
    g_fontnum = 0;
}

/* The missions of the team of the viewed player. */
static void draw_missions(int view)
{
    int team = sr_data_player[view].team;
    float y = 160.0f;
    int shown = 0;
    int index;

    drawtext("Press C to toggle info", 512.0f, 0.0f, 20.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f);
    g_fontnum = 1;
    if (sr_data_debugit != 0) {
        if (g_keyboard[KEY_DEBUG_FIRST_TEAM] != 0)
            team = 0;
        if (g_keyboard[KEY_DEBUG_THIRD_TEAM] != 0)
            team = 2;
        if (g_keyboard[KEY_DEBUG_FOURTH_TEAM] != 0)
            team = 3;
    }
    drawtext("/s", MISSION_X, 96.0f, MISSION_STYLE, sr_data_teamname[team]);
    mission_text("Missions:", 128.0f);
    for (index = 0; index < MAX_MISSIONS; ++index) {
        const Mission *mission = team_mission(team, index);
        int expire;
        int hour;
        int tens;
        int ones;

        if (mission->active == 0)
            continue;
        y = draw_mission(mission, y);
        expire = mission->expire;
        clock_parts(expire, &hour, &tens, &ones);
        drawtext("This mission expires at /i:/i/i", MISSION_X, y, MISSION_STYLE, hour, tens, ones);
        y = (float)((float)(y + 16.0) + 16.0);
        ++shown;
    }
    if (shown == 0)
        mission_text("No current missions", y);
    g_fontnum = 0;
}

/* The timer of a game of type 3, by the state of the game. */
static void draw_round_timer(void)
{
    int timer = sr_data_game.timer;

    if (sr_data_game.state == STATE_START)
        drawtext("Start: /i:/i/i", TIMER_STYLE, timer / TICKS_PER_MINUTE, (timer / 600) % 6, (timer / 0x3c) % 10);
    else if (sr_data_game.state == STATE_WAIT)
        drawtext("Start: /i:/i/i", TIMER_STYLE, timer / TICKS_PER_MINUTE, (timer / 600) % 6, (timer / 0x3c) % 10);
    else if (sr_data_game.state == STATE_PLAY) {
        if (sr_data_game.round_mode == ROUND_MODE_TIMED) {
            if (g_round_ticks < 1)
                drawtext("Get Ready", TIMER_STYLE);
            else
                drawtext("/i:/i/i./i", TIMER_STYLE, g_round_ticks / TICKS_PER_MINUTE, (g_round_ticks / 600) % 6,
                         (g_round_ticks / 0x3c) % 10, (g_round_ticks / 6) % 10);
        } else {
            drawtext("/i:/i/i", TIMER_STYLE, timer / TICKS_PER_MINUTE, (timer / 600) % 6, (timer / 0x3c) % 10);
        }
    } else if (sr_data_game.state == STATE_RESTART) {
        drawtext("Restart: /i:/i/i", TIMER_STYLE, timer / TICKS_PER_MINUTE, (timer / 600) % 6, (timer / 0x3c) % 10);
    }
}

/* The chat lines in the order of the ring, from the oldest. A line fades with its time. */
static void draw_chat(int view)
{
    float y = 400.0f;
    unsigned index = g_chat_head;

    while (index = (index + 1) & 0xff, index != (g_chat_head & 0xff)) {
        const ChatMessage *message = &g_chatmessage[index];
        int timer;
        int speaker;

        if (!((message->kind == CHAT_PLAYER_KIND || message->kind == CHAT_SYSTEM_KIND ||
               (sr_data_debugit != 0 && g_debug_toggle != 0)) &&
              message->timer > 0 && message->delay > 0))
            continue;
        timer = message->timer;
        if (!(g_game_state == STATE_RESTART || view == NO_PLAYER || sr_data_player[view].human_id == -1 ||
              sr_data_human[sr_data_player[view].human_id].health < 1 || message->target == -1 ||
              (sr_data_debugit != 0 && g_debug_toggle != 0)))
            continue;
        speaker = -1;
        if (message->kind == CHAT_HUMAN_KIND && message->target != -1)
            speaker = sr_data_human[message->target].player_id;
        if (message->kind == CHAT_PLAYER_KIND)
            speaker = message->target;
        if (speaker == -1)
            strcpy(g_display_text, message->text);
        else
            sprintf(g_display_text, "%s: %s", sr_data_player[speaker].name, message->text);
        drawtext(g_display_text, 4.0f, y, 20.0f, 0x60, 1.0f, 1.0f, 1.0f, (float)((float)timer / 200.0));
        y = (float)(y + 20.0);
    }
}

/* Draw the display of the game for the viewed player. */
void __cdecl drawdisplay(void)
{
    int view = g_view_player;
    const Player *viewer;

    if (view == NO_PLAYER)
        return;
    viewer = &sr_data_player[view];
    if (g_map_shown != 0)
        draw_stock_prices();
    if ((sr_data_game.gametype == GAMETYPE_DAY_CYCLE || sr_data_game.gametype == GAMETYPE_TEAMS) && g_map_shown != 0 &&
        viewer->team != NO_TEAM && viewer->team < TEAMS_WITH_MISSIONS)
        draw_missions(view);
    if (sr_data_game.gametype == GAMETYPE_DAY_CYCLE) {
        int hour;
        int tens;
        int ones;

        clock_parts(sr_data_game_world.clock, &hour, &tens, &ones);
        drawtext("/i:/i/i", TIMER_STYLE, hour, tens, ones);
    }
    if (sr_data_game.gametype == GAMETYPE_TEAMS)
        draw_round_timer();
    if ((g_game_state == STATE_WAIT && viewer->menu == MENU_PLAYER_LIST) || g_game_state == STATE_RESTART ||
        (sr_data_game.gametype == GAMETYPE_TEAMS && g_input_blocked == 0 && g_keyboard[KEY_HOLD_LIST] != 0))
        drawplayerinfo();
    drawtext("$/I", 4.0f, 4.0f, 24.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, viewer->money);
    if (-1 < viewer->team && viewer->team < TEAMS_ON_MAP)
        drawtext("$/I", 192.0f, 4.0f, 24.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f,
                 viewer->stocks * sr_data_market.stocks[viewer->team].price_int);
    if (sr_data_game.gametype == GAMETYPE_TEAMS && sr_data_game.round_mode == ROUND_MODE_TIMED) {
        if (g_game_state == STATE_PLAY)
            drawtext("/i///i", 512.0f, 16.0f, 32.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, viewer->round_points, g_round_goal);
        if (g_game_state == STATE_RESTART)
            drawtext("/i:/i/i./i", 512.0f, 16.0f, 24.0f, 1, 1.0f, 1.0f, 1.0f, 1.0f, g_round_ticks / TICKS_PER_MINUTE,
                     (g_round_ticks / 600) % 6, (g_round_ticks / 0x3c) % 10, (g_round_ticks / 6) % 10);
    }
    draw_chat(view);
}
