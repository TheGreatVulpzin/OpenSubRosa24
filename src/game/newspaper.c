/* News text and the account money ranking. */
#include "buildings.h"

#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "util.h"

#define g_newspaper (*SR_AT(Newspaper, 0x9bdd8a0))
#define g_newspaper_line SR_AT(char, 0x60d40)
#define g_newspaper_money SR_AT(char, 0x203f20)
#define g_parser (*SR_AT(Parser, SR_RVA_PARSER))
#define g_radix (*SR_AT(Radix, SR_RVA_RADIX))
#define g_numofaccounts (*SR_AT(int, SR_RVA_ACCOUNT + 4))
#define g_accounts SR_AT(Account, SR_RVA_ACCOUNT + 8)

void newspaper_load(void)
{
    Newspaper *np = &g_newspaper;
    int i, n;
    loadtextfile("readme.txt");
    np->num_lines = 0;
    while (g_parser.pos < g_parser.size) {
        cs_getline((char *)np->lines + np->num_lines * 256, 0x7c);
        np->num_lines++;
        if (np->num_lines == 0x3fe)
            break;
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
            /* A full file lets the ranking extend past the line array. */
            strcpy((char *)np->lines + np->num_lines * 256, g_newspaper_line);
            np->num_lines++;
        }
    }
}
