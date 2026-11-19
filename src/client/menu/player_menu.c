/* The menu of a player: the action that a menu button adds, and the two empty draw functions. */
#include "menu/player_menu.h"

#include "client.h"
#include "sr_runtime.h"

#define PLAYER_MENU_TEAM 2
#define ACTION_MASK 0x3f

/* Add a menu action to the ring of actions of a player. The ring has 64 places. */
void __cdecl player_addaction_menu(int player, int arg_a, int arg_b)
{
    Player *p = &sr_data_player[player];
    PlayerAction *action = &p->actions[p->num_actions];

    action->kind = 0;
    action->arg_a = arg_a;
    action->arg_b = arg_b;
    p->num_actions = (p->num_actions + 1) & ACTION_MASK;
}

/* The team menu draws nothing. */
void __cdecl drawplayermenu_team(void)
{
}

/* Draw the menu of a player. Only the team menu, with the cursor on, has a draw function. */
void __cdecl drawplayermenu(int player)
{
    if (cl_data_input_cursor != 0 && sr_data_player[player].menu == PLAYER_MENU_TEAM)
        drawplayermenu_team();
}

/* The player menus need no setup. */
void __cdecl setupplayermenus(void)
{
}
