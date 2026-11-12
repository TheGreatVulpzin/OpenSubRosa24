/* The help lines at the left of the screen: they tell which key does something useful with
 * what the viewed human holds. */
#include "render/help.h"

#include "client.h"
#include "render/text.h"
#include "sr_runtime.h"

#define g_view_player cl_data_view_player

enum {
    HELP_X = 16,
    HELP_FIRST_Y = 64,
    HELP_SECOND_Y = 112,
    HELP_LINE = 24,
    HELP_SIZE = 24,
    HELP_FLAGS = 0x20,
    ITEM_BRIEFCASE = 13,
    ITEM_OPEN_BRIEFCASE = 14,
    NO_HUMAN = -1,
    HUD_STATE_SHIFT = 8,
    HUD_STATE_LOADED = 3,
    HUD_STATE_NONE = 0
};

/* Write one help line at a row. */
static void help_line(const char *text, float y)
{
    drawtext(text, (float)HELP_X, y, (float)HELP_SIZE, HELP_FLAGS, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* Tell if the item is a gun. */
static int item_is_gun(int item)
{
    return sr_itemtypes[sr_items[item].type].is_gun != 0;
}

/* The lines for a gun in the right hand: what to do with the magazine in the left hand. The
 * result is the next row. */
static float gun_lines(const Human *body, const Player *player, float y)
{
    int right_item = body->inventory[0].items[0];
    int left_item = body->inventory[1].items[0];
    int right_state = player->hud_inventory[0].items[0] >> HUD_STATE_SHIFT;
    int left_state = player->hud_inventory[1].items[0] >> HUD_STATE_SHIFT;

    if (body->inventory[1].count < 1) {
        if (right_state == HUD_STATE_LOADED) {
            help_line("Press Shift+R to unload magazine", y);
            y = (float)(y + HELP_LINE);
        }
        if (right_state == HUD_STATE_NONE) {
            help_line("Press Shift+2-5 to put a magazine in your left hand", y);
            y = (float)(y + HELP_LINE);
        }
    } else if (sr_items[left_item].type - 1 == sr_items[right_item].type) {
        if (right_state < 1) {
            if (left_state < 1)
                help_line("Press Shift+Tilde to drop empty magazine", y);
            else
                help_line("Press Shift+R to load magazine", y);
            y = (float)(y + HELP_LINE);
        } else {
            if (left_state < 1)
                help_line("Press Shift+Tilde to drop magazine", y);
            else {
                help_line("The gun already has a magazine loaded", y);
                y = (float)(y + HELP_LINE);
                help_line("Press Shift+2-5 to put away this magazine", y);
            }
            y = (float)(y + HELP_LINE);
        }
    }
    return y;
}

/* Draw the help lines for the viewed player. The first line always shows. The other lines
 * depend on the hands of the viewed human: a gun in the right hand and a magazine in the left
 * hand, or a briefcase. */
void __cdecl drawhelp(void)
{
    int player = g_view_player;
    const Human *body;
    int human;
    float y;

    if (player == NO_HUMAN)
        return;
    human = sr_players[player].human_id;
    help_line("Press F1 to toggle help", (float)HELP_FIRST_Y);
    y = (float)HELP_SECOND_Y;
    if (human == NO_HUMAN)
        return;
    body = &sr_humans[human];
    if (0 < body->inventory[1].count && item_is_gun(body->inventory[1].items[0])) {
        help_line("Press F to swap your weapon to your right hand", y);
        y = 136.0f;
    }
    if (0 < body->inventory[0].count) {
        int right_item = body->inventory[0].items[0];
        int right_type = sr_items[right_item].type;

        if (item_is_gun(right_item))
            y = gun_lines(body, &sr_players[player], y);
        if (right_type == ITEM_BRIEFCASE) {
            help_line("Left-click to open briefcase", y);
            y = (float)(y + HELP_LINE);
        }
        if (right_type == ITEM_OPEN_BRIEFCASE) {
            help_line("Left-click to close briefcase", y);
            if (body->inventory[1].count < 1)
                help_line("Press Shift+R to unload item from briefcase", (float)(y + HELP_LINE));
            else
                help_line("Press Shift+R to load item into briefcase", (float)(y + HELP_LINE));
        }
    }
}
