/* The items. See items.c. */
#ifndef SR_ITEMS_H
#define SR_ITEMS_H

#include <stdint.h>

#include "sr_types.h"

/* The item types (setupitemtypes gives each one its name, price and size). */
enum {
    ITEM_AK47 = 0,
    ITEM_AK47_MAGAZINE = 1,
    ITEM_M16 = 2,
    ITEM_M16_MAGAZINE = 3,
    ITEM_MAGNUM = 4,
    ITEM_45_BULLETS = 5,
    ITEM_MP5 = 6,
    ITEM_MP5_MAGAZINE = 7,
    ITEM_UZI = 8,
    ITEM_UZI_MAGAZINE = 9,
    ITEM_9MM = 10,
    ITEM_9MM_MAGAZINE = 11,
    ITEM_BANDAGE = 12,
    ITEM_BRIEFCASE = 13,
    ITEM_OPEN_BRIEFCASE = 14,
    ITEM_CASH = 15,
    ITEM_CASH_2 = 16, /* the game also names it "Cash" */
    ITEM_BLACK_DISK = 17,
    ITEM_GREEN_DISK = 18,
    ITEM_BLUE_DISK = 19,
    ITEM_WHITE_DISK = 20,
    ITEM_GOLD_DISK = 21,
    ITEM_RED_DISK = 22,
    ITEM_CELL_PHONE = 23,
    ITEM_KEY = 24,
    ITEM_DOOR = 25,
    ITEM_NEWSPAPER = 26,
    NUM_ITEM_TYPES = 27
};

int item_billvalue(int item);
int item_addbill(int item, int pos, int bill);
void item_removebill(int item, int pos);
int linkitemtoitem(int parent, int child);
int item_addhumanlink(int item, int human, int slot);
void item_removehumanlink(uint32_t item, int human, int slot);
void item_removeitemlink(uint32_t child, uint32_t parent);
uint32_t createitem(int type, const vec3 *pos, const vec3 *vel, const vec3 rot[3]);
int linkitem(uint32_t item, uint32_t child, int human, int slot);
void itemtimetolive(void);

#endif /* SR_ITEMS_H */
