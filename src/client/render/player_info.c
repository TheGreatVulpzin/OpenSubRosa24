/* The list of the players at the top of the screen: one column for each team and one for the
 * players without a team, each player with the worth of the player. */
#include "render/player_info.h"

#include "client.h"
#include "render/draw2d.h"
#include "render/text.h"
#include "sr_runtime.h"

#define g_game_state cl_data_game_state
#define g_score_count cl_data_score_count
#define g_score_entries cl_data_score_entries
#define g_sort_count cl_data_sort_count
#define g_sort_order cl_data_sort_order
#define g_sort_keys cl_data_sort_keys

/* Game functions of other files. */
#define sortlist CL_FN(0x4e0f80, void(__cdecl *)(void))

enum {
    COLUMNS = 4,
    TEAM_COLUMNS = 3,
    MAX_PLAYERS = 256,
    NO_TEAM = 7,
    NO_VALUE_TEAM = -1,
    HEADER_FLAGS = 0x60,
    VALUE_FLAGS = 0x22,
    NUMBER_FLAGS = 0x20,
    STATE_NEW_ROUND = 0,
    STATE_ROUND_END = 3,
    SPLIT_PERCENT = -0x32
};
#define COLUMN_WIDTH 252.0
#define HEADER_Y 104.0f
#define ROW_SIZE 24.0f
#define FIRST_ROW_Y 128.0f
#define KEY_BASE (-0x70000001)

/* The worth of a player: the money, and for a player of a team also the stocks at the price now. */
static int player_worth(const Player *player)
{
    if (player->team == NO_VALUE_TEAM)
        return player->money;
    return cl_ftoi((double)player->stocks * (double)sr_data_market.stocks[player->team].price_int +
                   (double)player->money);
}

/* Write the price of the stock of a team after its name, and how far it moved from the open
 * price in percent. A price that fell by more than 50 percent shows as a split. */
static void draw_stock_price(int column, float x, const float *color)
{
    const Stock *stock = &sr_data_market.stocks[column];
    float price_x = (float)((x + 252.0) - 24.0);

    if (g_game_state != STATE_ROUND_END) {
        drawtext("$/i", price_x, HEADER_Y, ROW_SIZE, VALUE_FLAGS, color[0], color[1], color[2], 1.0f,
                 stock->price_int);
        return;
    }
    if (stock->open_price != 0.0f) {
        int percent = cl_ftoi(((float)stock->price_int * 100.0) / stock->open_price - 100.0);

        if (percent < SPLIT_PERCENT)
            drawtext("$/i(Split)", price_x, HEADER_Y, ROW_SIZE, VALUE_FLAGS, color[0], color[1], color[2], 1.0f,
                     stock->price_int);
        else if (percent < 1)
            drawtext("$/i(/i%)", price_x, HEADER_Y, ROW_SIZE, VALUE_FLAGS, color[0], color[1], color[2], 1.0f,
                     stock->price_int, percent);
        else
            drawtext("$/i(+/i%)", price_x, HEADER_Y, ROW_SIZE, VALUE_FLAGS, color[0], color[1], color[2], 1.0f,
                     stock->price_int, percent);
    }
}

/* List the players of a column in the order of their worth. */
static void draw_column_players(int column, const float *color)
{
    float x = (float)((float)column * COLUMN_WIDTH + 4.0);
    float y = FIRST_ROW_Y;
    int player;
    int row;

    g_score_count = 0;
    for (player = 0; player < MAX_PLAYERS; ++player) {
        const Player *entry = &sr_data_player[player];

        if (entry->active != 0 && entry->is_bot == 0 &&
            (entry->team == column || (column == COLUMNS - 1 && entry->team == NO_TEAM))) {
            g_score_entries[g_score_count].player = player;
            g_score_entries[g_score_count].worth = player_worth(entry);
            ++g_score_count;
        }
    }
    g_sort_count = g_score_count;
    for (row = 0; row < g_score_count; ++row)
        g_sort_keys[row] = (int)((unsigned)KEY_BASE - (unsigned)g_score_entries[row].worth);
    sortlist();
    for (row = 0; row < g_score_count; ++row) {
        float shade = 1.0f;

        player = g_score_entries[g_sort_order[row]].player;
        if (player < 0 || player > MAX_PLAYERS - 1)
            player = 0;
        if (sr_data_player[player].ready == 0)
            shade = 0.5f;
        drawtext(sr_data_player[player].name, x, y, ROW_SIZE, HEADER_FLAGS, (float)(color[0] * shade),
                 (float)(color[1] * shade), (float)(color[2] * shade), 1.0f);
        sr_data_player[player].net_worth = player_worth(&sr_data_player[player]);
        drawtext("$/I", (float)(x + 240.0), y, ROW_SIZE, VALUE_FLAGS, color[0], color[1], color[2], 1.0f,
                 sr_data_player[player].net_worth);
        y = (float)(y + ROW_SIZE);
    }
}

/* Draw the list: a dark panel, then for each column the name of the team, the price of its
 * stock and its players. */
void __cdecl drawplayerinfo(void)
{
    int column;

    drawbitmap(-1, 2.0f, 126.0f, 1020.0f, 480.0f, 0, 0.0f, 0.0f, 0.0f, 0.5f);
    for (column = 0; column < COLUMNS; ++column) {
        float color[3];
        float x = (float)((float)column * COLUMN_WIDTH + 8.0);

        if (column < TEAM_COLUMNS) {
            color[0] = sr_data_teamcolor[column * 3];
            color[1] = sr_data_teamcolor[column * 3 + 1];
            color[2] = sr_data_teamcolor[column * 3 + 2];
            drawtext(sr_data_teamname[column], x, HEADER_Y, ROW_SIZE, HEADER_FLAGS, color[0], color[1], color[2],
                     1.0f);
            draw_stock_price(column, x, color);
        } else {
            color[0] = 1.0f;
            color[1] = 1.0f;
            color[2] = 1.0f;
            /* The last column has an empty label. */
            drawtext("", x, HEADER_Y, ROW_SIZE, HEADER_FLAGS, color[0], color[1], color[2], 1.0f);
        }
        if (g_game_state == STATE_NEW_ROUND)
            drawtext("/i.", (float)((float)column * COLUMN_WIDTH + 4.0), HEADER_Y, ROW_SIZE, NUMBER_FLAGS,
                     color[0], color[1], color[2], 1.0f, column + 1);
        draw_column_players(column, color);
    }
}
