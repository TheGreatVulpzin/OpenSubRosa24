/* Small updates of the player records. See player_records.c. */
#ifndef CL_SETUP_PLAYER_RECORDS_H
#define CL_SETUP_PLAYER_RECORDS_H

#include "sr_types.h"

void __cdecl addplayerwaypoint(int player, uint32_t x_bits, uint32_t y_bits, uint32_t z_bits);
void __cdecl logic_splitteamcash(int team, int amount);
int __cdecl scenario_isseekmission(int team);

#endif /* CL_SETUP_PLAYER_RECORDS_H */
