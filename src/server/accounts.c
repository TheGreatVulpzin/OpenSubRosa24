/* Server accounts (user/server.srk, docs/formatos.md). */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "random.h"
#include "sr_runtime.h"
#include "sr_server.h"

void *sr_import(const char *dll, const char *name);

typedef size_t(__cdecl *fread_fn)(void *, size_t, size_t, void *);

#define G(type, rva) (*(type *)(sr_base + (rva)))

#define account_key         G(uint32_t, SR_RVA_ACCOUNT)      /* random key, saved in the file */
#define numofaccounts       G(int, SR_RVA_ACCOUNT + 4)
#define accounts            ((Account *)(sr_base + SR_RVA_ACCOUNT + 8))
#define account_file        G(void *, 0x13e22e68)            /* FILE * of the save and load functions */
#define game_fread          G(fread_fn, 0x91022a4)           /* fread pointer, set by getpaths */


/* Read account versions 1 to 3. A file with invalid counts or short reads gives no accounts.
 * The key keeps its new random value when the file is absent or invalid.
 * Version 1 rolls each look. Version 2 rolls a look again when all three values are zero. */
void __cdecl sr_loadaccountsserver(void)
{
    void *(__cdecl *fopen_)(const char *, const char *);
    int(__cdecl *fclose_)(void *);
    int(__cdecl *rand_)(void);
    int version = 0, i, count;
    uint32_t key;
    Account *pending = NULL;

    *(void **)&fopen_ = sr_import("MSVCR100.dll", "fopen");
    *(void **)&fclose_ = sr_import("MSVCR100.dll", "fclose");
    *(void **)&rand_ = sr_import("MSVCR100.dll", "rand");

    account_key = genrand_int32();
    numofaccounts = 0;
    account_file = fopen_("user/server.srk", "rb");
    if (!account_file)
        return;
    if (game_fread(&version, 4, 1, account_file) != 1)
        goto close;
    if ((unsigned)(version - 1) < 3) {
        if (game_fread(&key, 4, 1, account_file) != 1 ||
            game_fread(&count, 4, 1, account_file) != 1 || (unsigned)count > 65535)
            goto close;
        if (count != 0) {
            pending = malloc(count * sizeof *pending);
            if (!pending)
                goto close;
            memcpy(pending, accounts, count * sizeof *pending);
        }
        for (i = 0; i < count; i++) {
            Account *a = &pending[i];
            if (game_fread(&a->id[0], 4, 1, account_file) != 1 ||
                game_fread(&a->id[1], 4, 1, account_file) != 1 ||
                game_fread(a->name, 1, 32, account_file) != 32 ||
                game_fread(&a->money, 4, 1, account_file) != 1)
                goto close;
            a->name[31] = 0;
            if (version >= 2 &&
                (game_fread(&a->playtime, 4, 1, account_file) != 1 ||
                 game_fread(&a->look_a, 4, 1, account_file) != 1 ||
                 game_fread(&a->look_b, 4, 1, account_file) != 1 ||
                 game_fread(&a->look_c, 4, 1, account_file) != 1))
                goto close;
        }
        for (i = 0; i < count; i++) {
            Account *a = &pending[i];
            if (version < 2 ||
                (version == 2 && a->look_a == 0 && a->look_b == 0 && a->look_c == 0)) {
                a->look_a = rand_() % 3;
                a->look_b = rand_() & 7;
                a->look_c = rand_() & 3;
            }
        }
        if (count != 0)
            memcpy(accounts, pending, count * sizeof *pending);
        numofaccounts = count;
        account_key = key;
    }
close:
    free(pending);
    fclose_(account_file);
}
