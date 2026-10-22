/* Account names and account files. */
#include "player.h"

#include <stdio.h>
#include <string.h>

#include "logic.h"
#include "server.h"
#include "sr_os.h"

#define g_parser (*SR_AT(Parser, SR_RVA_PARSER))
#define g_sysinfo (*SR_AT(SysInfo, SR_RVA_SYSINFO))
typedef size_t (__cdecl *file_transfer)(void *, size_t, size_t, void *);
#define game_fread (*SR_AT(file_transfer, 0x91022a4))
#define game_fwrite (*SR_AT(file_transfer, 0x13bf2d00))

int account_nameexists(const char *name)
{
    size_t len = strlen(name);
    int i;
    if (len > 24)
        len = 24;
    for (i = 0; i < sr_numofaccounts; ++i)
        if (strncmp(name, sr_accounts[i].name, len) == 0)
            return 1;
    return 0;
}

int createaccount(int player)
{
    Player *p = &sr_players[player];
    Account *a;
    int i, k;
    size_t len;
    char suffix[32];

    for (i = 0; i < sr_numofaccounts; ++i) {
        a = &sr_accounts[i];
        if (p->account_id[0] != a->id[0])
            continue;
        if (p->account_id[1] != a->id[1])
            return -1;
        if (a->name_locked) {
            memcpy(p->name, a->name, 32);
        } else {
            memcpy(a->name, p->name, 32);
            a->name_locked = 5;
        }
        return i;
    }
    if (sr_numofaccounts > 0xfffe)
        return -1;
    a = &sr_accounts[i];
    memset(a, 0, sizeof(*a));
    a->id[0] = p->account_id[0];
    a->id[1] = p->account_id[1];
    len = strlen(p->name);
    if (len > 24)
        len = 24;
    for (k = 1; account_nameexists(p->name) && k < 1000; ++k) {
        p->name[len] = 0;
        sprintf(suffix, "(%d)", k);
        strcat(p->name, suffix);
        p->name[31] = 0;
    }
    memcpy(a->name, p->name, 32);
    a->money = 1000;
    a->look_a = os_rand() % 3;
    a->look_b = os_rand() & 7;
    a->look_c = os_rand() & 3;
    ++sr_numofaccounts;
    return i;
}

void saveaccountsserver(void)
{
    int version = 3, i;
    g_parser.file = os_fopen("user/server.srk", "wb");
    if (!g_parser.file)
        return;
    game_fwrite(&version, 4, 1, g_parser.file);
    game_fwrite(&sr_account_key, 4, 1, g_parser.file);
    game_fwrite(&sr_numofaccounts, 4, 1, g_parser.file);
    for (i = 0; i < sr_numofaccounts; ++i) {
        Account *a = &sr_accounts[i];
        game_fwrite(&a->id[0], 4, 1, g_parser.file);
        game_fwrite(&a->id[1], 4, 1, g_parser.file);
        game_fwrite(a->name, 1, 32, g_parser.file);
        game_fwrite(&a->money, 4, 1, g_parser.file);
        game_fwrite(&a->playtime, 4, 1, g_parser.file);
        game_fwrite(&a->look_a, 4, 1, g_parser.file);
        game_fwrite(&a->look_b, 4, 1, g_parser.file);
        game_fwrite(&a->look_c, 4, 1, g_parser.file);
    }
    os_fclose(g_parser.file);
}

void saveaccountsclient(void)
{
    int version = 1, i, j;
    char name[64];
    os_chdir(g_sysinfo.config_dir);
    sprintf(name, "client.srk");
    g_parser.file = os_fopen(name, "wb");
    if (g_parser.file) {
        game_fwrite(&version, 4, 1, g_parser.file);
        game_fwrite(&g_numofclientaccounts, 4, 1, g_parser.file);
        for (i = 0; i < g_numofclientaccounts; ++i)
            for (j = 0; j < 4; ++j)
                game_fwrite(&g_clientaccounts[i][j], 4, 1, g_parser.file);
        os_fclose(g_parser.file);
    }
    os_chdir(g_sysinfo.start_dir);
}

void loadaccountsclient(void)
{
    int version, i, j;
    char name[64];
    g_numofclientaccounts = 0;
    os_chdir(g_sysinfo.config_dir);
    sprintf(name, "client.srk");
    g_parser.file = os_fopen(name, "rb");
    if (g_parser.file) {
        game_fread(&version, 4, 1, g_parser.file);
        if (version == 1) {
            game_fread(&g_numofclientaccounts, 4, 1, g_parser.file);
            for (i = 0; i < g_numofclientaccounts; ++i)
                for (j = 0; j < 4; ++j)
                    game_fread(&g_clientaccounts[i][j], 4, 1, g_parser.file);
        }
        os_fclose(g_parser.file);
    }
    os_chdir(g_sysinfo.start_dir);
}
