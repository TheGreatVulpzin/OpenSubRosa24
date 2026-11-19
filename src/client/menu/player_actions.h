/* The actions of a player. See player_actions.c. */
#ifndef CL_MENU_PLAYER_ACTIONS_H
#define CL_MENU_PLAYER_ACTIONS_H

#include "sr_types.h"

void __cdecl player_addaction_chat(int player, const char *text);
void __cdecl player_addaction_item(int player, int item_a, int item_b);

#endif /* CL_MENU_PLAYER_ACTIONS_H */
