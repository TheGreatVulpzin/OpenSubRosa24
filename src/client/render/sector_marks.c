/* The table of free sector mesh places: one bit for each group of 16 places of the sector mesh
 * buffer. A bit of 1 means the group is used. The table has 1024 words of 32 bits. */
#include "render/sector_marks.h"

#include <stdint.h>

#include "client.h"
#include "sr_runtime.h"

#define g_marks ((uint32_t *)sr_data_level_marks)
#define MARK_WORDS 0x400
#define MARK_LAST_WORD 0x3ff
#define GROUP_PLACES 16
#define NUM_SECTORS 0x2000

/* Set the mark of the group that holds a place to a value. The place counts in units of one place:
 * the group is the place divided by 16, rounded toward zero. A place past the table changes nothing. */
void __cdecl area_setsectormesh(int place, int value)
{
    int rounded = place + ((place >> 31) & (GROUP_PLACES - 1));
    uint8_t bit = (uint8_t)(rounded >> 4) & 0x1f;
    int word = rounded >> 9;

    if (word < MARK_LAST_WORD) {
        g_marks[word] &= ~(1u << bit);
        g_marks[word] |= (unsigned)value << bit;
    }
}

/* Find a free group and give its first place. A word that is full is skipped. The table must have
 * a free group: with none the search does not end. */
int __cdecl area_getnextsectormesh(void)
{
    int word, bit;

    for (word = 0; g_marks[word] == 0xffffffffu && word < MARK_LAST_WORD; word++)
        ;
    bit = 0;
    while (((g_marks[word] >> (bit & 0x1f)) & 1) != 0)
        bit++;
    return (word * 0x20 + bit) * GROUP_PLACES;
}

/* Free the places of the meshes of a sector and forget its meshes. A sector number outside the
 * list changes nothing. */
void __cdecl area_removesectormodel(int sector)
{
    int k;

    if (sector < 0 || sector >= NUM_SECTORS)
        return;
    for (k = 0; k < cl_data_block_sectors[sector].num_meshes; k++)
        area_setsectormesh(cl_data_block_sectors[sector].parts[k].first_index, 0);
    cl_data_block_sectors[sector].num_meshes = 0;
}
