/* Account identifiers and the balance changes of player actions.
 * Client account identifiers occupy sixteen-byte records.
 * A matching key returns its existing identifiers without advancing the generator.
 */
#include "account_actions.h"

#include "economy.h"
#include "logic.h"
#include "random.h"

enum { CLIENT_ACCOUNT_LIMIT = 65535, DEATH_TAX_ALLOWANCE = 10000 };

/* Find an account key or append two random identifiers. A full store returns minus one. */
int createclientaccount(int *first, int *second, int key)
{
    int index;
    for (index = 0; index < g_numofclientaccounts; ++index) {
        if (g_clientaccounts[index][0] == key) {
            *first = g_clientaccounts[index][1];
            *second = g_clientaccounts[index][2];
            return index;
        }
    }
    if (g_numofclientaccounts >= CLIENT_ACCOUNT_LIMIT)
        return -1;
    g_clientaccounts[index][0] = key;
    g_clientaccounts[index][1] = (int)genrand_int32();
    g_clientaccounts[index][2] = (int)genrand_int32();
    *first = g_clientaccounts[index][1];
    *second = g_clientaccounts[index][2];
    ++g_numofclientaccounts;
    return index;
}

/* Take five percent of the balance above the allowance. */
void accountdeathtax(int account)
{
    Account *record;
    int taxable;
    if (account < 0 || account >= sr_data_accounts.count)
        return;
    record = &sr_data_accounts.records[account];
    taxable = (int)((uint32_t)record->money - DEATH_TAX_ALLOWANCE);
    if (taxable > 0)
        record->money -= (int)(long long)((double)taxable * 0.05000000074505806);
}

/* Sell all shares for a player in the pool. */
void playerdebt(int player)
{
    if ((unsigned)player < SR_MAX_PLAYER)
        playersellstock(player, sr_players[player].stocks);
}

/* Add an amount to a team's balance. */
void addpointstoteam(int team, int amount)
{
    int *balance = TEAM_FIELD(team0_cash, team);
    *balance = (int)((uint32_t)*balance + (uint32_t)amount);
}

/* Split an amount between active members. Integer division leaves the remainder unassigned. */
void logic_splitteamcash(int team, int amount)
{
    int count = 0, player;
    for (player = 0; player < SR_MAX_PLAYER; ++player)
        if (sr_players[player].active && sr_players[player].team == team)
            ++count;
    if (count == 0)
        return;
    for (player = 0; player < SR_MAX_PLAYER; ++player)
        if (sr_players[player].active && sr_players[player].team == team)
            sr_players[player].money = (int)((uint32_t)sr_players[player].money + (uint32_t)(amount / count));
}
