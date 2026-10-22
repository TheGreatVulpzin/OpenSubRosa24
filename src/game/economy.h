/* Stock prices, shops, player taxes and mission rewards. */
#ifndef SR_ECONOMY_H
#define SR_ECONOMY_H

#include <stdint.h>

void economysimulation(void);
void economy_carmarket(void);
void economy_gunmarket(void);
void playerbuystock(int player, int count);
void playersellstock(int player, int count);
void player_calcassets(int player);
void playerdeathtax(uint32_t player);
void playerteamkilltax(int player, int damage);
void logic_addteamcash(int team, int amount);
void logic_teammissioncomplete(int team, int mission);
void scenario_setmissioninactive(int scenario);
int scenario_getnumofmissions(int team);

#endif
