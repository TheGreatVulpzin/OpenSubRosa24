/* sr_fn(): the C version of a 024d function, by its image address.
 *
 * The lifted code calls other functions through SR_FN(va) (lift_support.h). With the hooks
 * that is the 024d code. The standalone server has no 024d code, so this table maps each
 * address to the lifted function (lf_table), or to a function written by hand. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "lifted_table.h"

void __cdecl sr_createevent_message(int kind, const char *text, int target, int extra);
void __cdecl sr_createevent_updatedoor(int a, int b);
void __cdecl sr_createevent_updatemission(int team, int mission);
void __cdecl sr_fixplayername(int id);
void __cdecl sr_loadaccountsserver(void);
void __cdecl sr_dedicatedserverloop(void);
void __cdecl sr_getpaths(void);

int __cdecl comparestrings(const void *a, const void *b);

static const struct {
    uint32_t rva;
    void *fn;
} g_hand[] = {
    { 0x6260, (void *)sr_createevent_message },
    { 0x6660, (void *)sr_createevent_updatedoor },
    { 0x6760, (void *)sr_createevent_updatemission },
    { 0xa580, (void *)sr_fixplayername },
    { 0xd320, (void *)comparestrings },
    { 0xe2e0, (void *)sr_loadaccountsserver },
    { 0x5c8d0, (void *)sr_dedicatedserverloop },
    { 0x5cd10, (void *)sr_getpaths },
};

/* Direct map: functions start on 16-byte boundaries. Index = (rva - 0x1000) / 16. */
#define TEXT_RVA 0x1000u
#define TEXT_SIZE 0x5d000u
static void *g_map[TEXT_SIZE / 16];

static void put(uint32_t rva, void *fn)
{
    if (rva < TEXT_RVA || rva >= TEXT_RVA + TEXT_SIZE || (rva & 15)) {
        fprintf(stderr, "fn_table: RVA 0x%x is not on a 16-byte boundary\n", rva);
        exit(1);
    }
    g_map[(rva - TEXT_RVA) / 16] = fn;
}

void sr_fn_init(void)
{
    int i;
    for (i = 0; i < lf_table_count; i++)
        put(lf_table[i].rva, lf_table[i].lifted);
    for (i = 0; i < (int)(sizeof g_hand / sizeof g_hand[0]); i++)
        put(g_hand[i].rva, g_hand[i].fn);
}

void *sr_fn(uintptr_t va)
{
    uint32_t rva = (uint32_t)(va - 0x400000u);
    void *fn = rva >= TEXT_RVA && rva < TEXT_RVA + TEXT_SIZE && !(rva & 15) ? g_map[(rva - TEXT_RVA) / 16] : NULL;
    if (!fn) {
        fprintf(stderr, "fn_table: no C function for RVA 0x%x\n", rva);
        exit(1);
    }
    return fn;
}
