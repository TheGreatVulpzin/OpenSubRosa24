/* The in-game menu of the local player. See player.c. */
#ifndef CL_MENU_PLAYER_H
#define CL_MENU_PLAYER_H

void __cdecl playermenu(int index);
void __cdecl playermenu_team(int index);
void __cdecl playermenu_spawn(int index);
void __cdecl playermenu_buy(int index);
void __cdecl playermenu_buy_weapons(int index);
void __cdecl playermenu_buy_ammo(int index);
void __cdecl playermenu_buy_vehicles(int index);
void __cdecl playermenu_buy_equipment(int index);
void __cdecl playermenu_buy_stocks(int index);
void __cdecl playermenu_building_main(int index);
void __cdecl playermenu_building_buy(int index);
void __cdecl playermenu_banking(int index);

#endif /* CL_MENU_PLAYER_H */
