/* Account identifiers, taxes and balance changes. */
#ifndef SR_ACCOUNT_ACTIONS_H
#define SR_ACCOUNT_ACTIONS_H

int createclientaccount(int *first, int *second, int key);
void accountdeathtax(int account);
void playerdebt(int player);
void addpointstoteam(int team, int amount);
void logic_splitteamcash(int team, int amount);

#endif
