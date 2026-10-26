/* News text and the account money ranking. */
#include "buildings.h"

#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "util.h"

#define g_newspaper sr_data_newspaper
#define g_newspaper_line sr_data_newspaper_line
#define g_newspaper_money sr_data_newspaper_money
#define g_parser sr_data_parser
#define g_radix sr_data_radix
#define g_numofaccounts (*((int *)&sr_data_accounts.count))
#define g_accounts ((Account *)&sr_data_accounts.records)

void newspaper_load(void)
{
    Newspaper *np = &g_newspaper;
    int i, n;
    loadtextfile("readme.txt");
    np->num_lines = 0;
    while (g_parser.pos < g_parser.size && np->num_lines < 1013) {
        cs_getline((char *)np->lines + np->num_lines * 256, 0x7c);
        np->num_lines++;
    }
    strcpy((char *)np->lines + np->num_lines * 256, "Top 10 list");
    np->num_lines++;
    n = g_numofaccounts;
    g_radix.count = n;
    for (i = 0; i < n; i++)
        g_radix.keys[i] = 0x8fffffffu - (uint32_t)g_accounts[i].money;
    csradixsort();
    for (i = 0; i < 10; i++) {
        if (i < g_numofaccounts) {
            Account *account = &g_accounts[g_radix.order[i]];
            sprintf(g_newspaper_line, "%d. %s   $", i + 1, account->name);
            sprintf_comma(g_newspaper_money, account->money);
            strcat(g_newspaper_line, g_newspaper_money);
            strcpy((char *)np->lines + np->num_lines * 256, g_newspaper_line);
            np->num_lines++;
        }
    }
}
