/* The menu of a player. See player_menu.c. */
#ifndef CL_MENU_PLAYER_MENU_H
#define CL_MENU_PLAYER_MENU_H

#include "sr_types.h"

void __cdecl player_addaction_menu(int player, int arg_a, int arg_b);
void __cdecl drawplayermenu_team(void);
void __cdecl drawplayermenu(int player);
void __cdecl setupplayermenus(void);

#endif /* CL_MENU_PLAYER_MENU_H */
