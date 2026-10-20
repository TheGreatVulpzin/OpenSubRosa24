/* Server accounts (user/server.srk, docs/formatos.md). */
#include <stddef.h>
#include <stdint.h>

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
#ifdef SR_STANDALONE
void *sr_fn(uintptr_t va);
#define genrand_int32       ((uint32_t(__cdecl *)(void))sr_fn(0x400000u + 0xae50))
#else
#define genrand_int32       ((uint32_t(__cdecl *)(void))(sr_base + 0xae50))
#endif

/* Original: 024d RVA 0xe2e0 (loadaccountsserver in the 0.24c ELF).
 *
 * Read the accounts from user/server.srk. The key gets a new random value first. It stays
 * when the file is missing. Versions 1 to 3 are read, other versions are ignored. Version 1
 * has no look: the game rolls it with rand(). Version 2 rolls it again when it is all zero.
 *
 * The count comes from the file and is not checked against the 65536 accounts, as in the
 * original. Calls go to the game CRT (its FILE objects and its rand() state) and to the fread
 * pointer of the game. The original reads an uninitialized version when the file is empty.
 * This version reads 0 and ignores the file. */
void __cdecl sr_loadaccountsserver(void)
{
    void *(__cdecl *fopen_)(const char *, const char *);
    int(__cdecl *fclose_)(void *);
    int(__cdecl *rand_)(void);
    int version = 0, i;

    *(void **)&fopen_ = sr_import("MSVCR100.dll", "fopen");
    *(void **)&fclose_ = sr_import("MSVCR100.dll", "fclose");
    *(void **)&rand_ = sr_import("MSVCR100.dll", "rand");

    account_key = genrand_int32();
    numofaccounts = 0;
    account_file = fopen_("user/server.srk", "rb");
    if (!account_file)
        return;
    game_fread(&version, 4, 1, account_file);
    if ((unsigned)(version - 1) < 3) {
        game_fread(&account_key, 4, 1, account_file);
        game_fread(&numofaccounts, 4, 1, account_file);
        for (i = 0; i < numofaccounts; i++) {
            Account *a = &accounts[i];
            game_fread(&a->id[0], 4, 1, account_file);
            game_fread(&a->id[1], 4, 1, account_file);
            game_fread(a->name, 1, 32, account_file);
            game_fread(&a->money, 4, 1, account_file);
            if (version < 2) {
                a->look_a = rand_() % 3;
                a->look_b = rand_() & 7;
                a->look_c = rand_() & 3;
                continue;
            }
            game_fread(&a->playtime, 4, 1, account_file);
            game_fread(&a->look_a, 4, 1, account_file);
            game_fread(&a->look_b, 4, 1, account_file);
            game_fread(&a->look_c, 4, 1, account_file);
            if (version == 2 && a->look_a == 0 && a->look_b == 0 && a->look_c == 0) {
                a->look_a = rand_() % 3;
                a->look_b = rand_() & 7;
                a->look_c = rand_() & 3;
            }
        }
    }
    fclose_(account_file);
}
