/* The accounts that the client keeps for the players that it meets. */
#include "setup/client_account.h"

#include "random.h"
#include "sr_runtime.h"

#define ACCOUNT_LIMIT 0xffff

/* Find the account of a key and give its two numbers. A key that is new gets an account with two
 * random numbers. The result is the index of the account, or -1 when the list is full. */
int __cdecl createclientaccount(int *first, int *second, int key)
{
    SrClientAccountStore *store = &sr_data_clientaccounts;
    int i;

    for (i = 0; i < store->count; i++) {
        if (key == store->records[i][0]) {
            *first = store->records[i][1];
            *second = store->records[i][2];
            return i;
        }
    }
    if (store->count >= ACCOUNT_LIMIT)
        return -1;
    store->records[store->count][0] = key;
    store->records[store->count][1] = (int)genrand_int32();
    store->records[store->count][2] = (int)genrand_int32();
    *first = store->records[store->count][1];
    *second = store->records[store->count][2];
    i = store->count;
    store->count++;
    return i;
}
