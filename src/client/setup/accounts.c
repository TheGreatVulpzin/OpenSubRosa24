/* The accounts of a server that the client keeps for the host panel (user/server.srk). */
#include "setup/accounts.h"

#include <stdint.h>

#include "client.h"
#include "random.h"
#include "sr_os.h"
#include "sr_runtime.h"

#define account_key sr_data_accounts.key
#define numofaccounts sr_data_accounts.count
#define accounts sr_data_accounts.records
#define account_file cl_data_open_file
#define account_handle ((void *)(uintptr_t)account_file)
#define game_fread sr_data_file_read

/* The file versions that the reader knows. */
#define ACCOUNT_FIRST_VERSION 1
#define ACCOUNT_LAST_VERSION 3

/* Give an account a random look: a shirt from 3, a hat from 8 and a skin from 4. */
static void roll_look(Account *a)
{
    a->look_a = os_rand() % 3;
    a->look_b = (int)((unsigned)os_rand() & 7);
    a->look_c = (int)((unsigned)os_rand() & 3);
}

/* Read the accounts of versions 1 to 3. The reads are not checked: a short file leaves the rest of
 * a record as it was, and a count above the table size reads past the table. The key and the count
 * come from the file. A file with another version, or no file, leaves the new random key and
 * a count of 0. The result is the result of closing the file, or 0 when the file does not open. Version 1 rolls the look of each account. Version 2 rolls it again when all three
 * values are 0. */
int __cdecl loadaccountsserver(void)
{
    int version = 0, i;

    account_key = (uint32_t)genrand_int32();
    numofaccounts = 0;
    account_file = (int)(uintptr_t)os_fopen("user/server.srk", "rb");
    if (account_file == 0)
        return 0;
    game_fread(&version, 4, 1, account_handle);
    if (version >= ACCOUNT_FIRST_VERSION && version <= ACCOUNT_LAST_VERSION) {
        game_fread(&account_key, 4, 1, account_handle);
        game_fread(&numofaccounts, 4, 1, account_handle);
        for (i = 0; i < numofaccounts; i++) {
            Account *a = &accounts[i];

            game_fread(&a->id[0], 4, 1, account_handle);
            game_fread(&a->id[1], 4, 1, account_handle);
            game_fread(a->name, 1, 32, account_handle);
            game_fread(&a->money, 4, 1, account_handle);
            if (version < 2) {
                roll_look(a);
            } else {
                game_fread(&a->playtime, 4, 1, account_handle);
                game_fread(&a->look_a, 4, 1, account_handle);
                game_fread(&a->look_b, 4, 1, account_handle);
                game_fread(&a->look_c, 4, 1, account_handle);
                if (version == 2 && a->look_a == 0 && a->look_b == 0 && a->look_c == 0)
                    roll_look(a);
            }
        }
    }
    return os_fclose(account_handle);
}
