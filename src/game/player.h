/* Player accounts, input and saved equipment. */
#ifndef SR_PLAYER_H
#define SR_PLAYER_H

int account_nameexists(const char *name);
int createaccount(int player);
void saveaccountsserver(void);
void saveaccountsclient(void);
void loadaccountsclient(void);
int createplayer(void);
void deleteplayer(int player);
void playersimulation(void);
void player_saveinventory(int player);
void player_loadinventory(int player);

#endif
