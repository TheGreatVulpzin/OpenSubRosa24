/* The in-game menu of the local player: team, spawn, the shops, the buildings and the bank.
 * Each function builds the items of one menu. A button writes its number to the menu action
 * of the player, and getinputs sends the action to the server. The number keys 1 to 8 press
 * the buttons, and the key under Escape goes back. */
#include "menu/player.h"

#include <stdio.h>
#include <string.h>

#include "client.h"
#include "../../game/items.h"
#include "menu/items.h"
#include "sr_runtime.h"

#define BUTTON_FONT 16.0f
#define PRICE_FONT 14.0f
#define BUTTON_WIDTH 252.0f
#define BUTTON_HEIGHT 32.0f
#define NO_TEAM 7
#define CAR_COLORS 6
#define ACTION_BACK 16

/* SDL scancodes. */
#define KEY_1 30
#define KEY_R 21
#define KEY_BACK 0x35

enum { OPTION_KEY = 1, OPTION_CHOICE = 11, OPTION_DISABLED = 20 };

enum {
    MENU_SPAWN = 1, MENU_TEAM = 2, MENU_BUY = 3, MENU_BUY_WEAPONS = 4, MENU_BUY_AMMO = 5, MENU_BUY_VEHICLES = 6,
    MENU_BUY_EQUIPMENT = 7, MENU_BUY_STOCKS = 8, MENU_BUILDING = 9, MENU_BUILDING_BUY = 10, MENU_BANK = 11,
    MENU_BANK_2 = 12
};

enum { BUILDING_CAR_MARKET = 2, BUILDING_CLOTHES = 4, BUILDING_GUNS = 7 };

/* A field of one building of the city: records of 0x9dc bytes. */
#define CITY_BUILDING(field, b) (((int *)((uint8_t *)&sr_data_city.field + (b) * 0x9dc)))

/* Add a white button of 252 by 32 at x and y. */
static void menu_button(const char *text, float x, float y)
{
    createmenubutton(text, x, y, BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_FONT, 0, 1.0f, 1.0f, 1.0f, 1.0f);
}

/* Add a button that writes value to action, with a key. Key 0 adds no key. */
static void action_button(const char *text, float x, float y, int *action, int value, int key)
{
    menu_button(text, x, y);
    setmenuitem(OPTION_CHOICE, action, value);
    if (key != 0)
        setmenuitem(OPTION_KEY, key);
}

/* The Done button: it releases the mouse cursor. */
static void done_button(float y)
{
    menu_button("Done", 4.0f, y);
    setmenuitem(OPTION_CHOICE, &cl_data_input_cursor, 0);
}

/* The Back button: it sends action 16. */
static void back_button(float y, int *action)
{
    action_button("Back", 4.0f, y, action, ACTION_BACK, KEY_BACK);
}

/* A buy button: the label, its price in small text at the corner, the action value and the
 * key of the value. text is the buffer of the label: the price text goes into it after the
 * button copies the label. */
static void buy_button(char *text, float y, int *action, int value, int price)
{
    menu_button(text, 4.0f, y);
    sprintf(text, "$%d", price);
    setmenuiteminfo(text, (float)(4.0 + 2.0), (float)(y + 2.0), PRICE_FONT, 0, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_CHOICE, action, value);
    setmenuitem(OPTION_KEY, KEY_1 + value - 1);
}

/* The name of a car: its color and its model. A color number of 6 or more gives no color. */
static void car_name(char *text, int color, int type)
{
    if (color < CAR_COLORS)
        strcpy(text, cl_data_car_color_names[color]);
    else
        text[0] = '\0';
    strcat(text, " ");
    strcat(text, sr_data_objecttype[type].name);
}

/* The team menu: three team buttons with no text for a player with no team, else the
 * observer button, and the Ready button at the right. */
void __cdecl playermenu_team(int index)
{
    Player *player = &sr_data_player[index];
    float x = 4.0f;
    int team;

    for (team = 0; team < 3; team++) {
        if (player->team == NO_TEAM)
            action_button("", x, 96.0f, &player->menu_action, team + 1, KEY_1 + team);
        x = (float)(x + 252.0);
    }
    if (player->team != NO_TEAM)
        action_button("Observer", x, 96.0f, &player->menu_action, 4, KEY_1 + 3);
    createmenubutton("Ready", 1020.0f, 32.0f, 192.0f, BUTTON_HEIGHT, BUTTON_FONT, 2, 1.0f, 1.0f, 1.0f, 1.0f);
    setmenuitem(OPTION_CHOICE, &player->menu_action, 5);
    setmenuitem(OPTION_KEY, KEY_R);
}

/* The spawn menu: one button that enters the city. */
void __cdecl playermenu_spawn(int index)
{
    action_button("Enter City", 4.0f, 128.0f, &sr_data_player[index].menu_action, 1, KEY_1);
}

/* The buy menu: the shops and the bank. */
void __cdecl playermenu_buy(int index)
{
    static const char *const labels[5] = {"Weapons", "Ammo", "Equipment", "Banking", "Stocks"};
    int *action = &sr_data_player[index].menu_action;
    int i;

    for (i = 0; i < 5; i++)
        action_button(labels[i], 4.0f, (float)(128 + 32 * i), action, i + 1, KEY_1 + i);
    done_button(320.0f);
}

/* A shop of four item types with their prices. */
static void item_shop(int index, const char *const names[4], const int types[4])
{
    int *action = &sr_data_player[index].menu_action;
    int i;

    for (i = 0; i < 4; i++) {
        char text[64];

        strcpy(text, names[i]);
        buy_button(text, (float)(128 + 32 * i), action, i + 1, sr_data_itemtype[types[i]].price);
    }
    back_button(288.0f, action);
}

/* The guns of the shop. */
void __cdecl playermenu_buy_weapons(int index)
{
    static const char *const names[4] = {"9mm", "MP5", "AK-47", "M-16"};
    static const int types[4] = {ITEM_9MM, ITEM_MP5, ITEM_AK47, ITEM_M16};

    item_shop(index, names, types);
}

/* The magazines of the shop. */
void __cdecl playermenu_buy_ammo(int index)
{
    static const char *const names[4] = {"9mm Magazine", "MP5 Magazine", "AK-47 Magazine", "M-16 Magazine"};
    static const int types[4] = {ITEM_9MM_MAGAZINE, ITEM_MP5_MAGAZINE, ITEM_AK47_MAGAZINE, ITEM_M16_MAGAZINE};

    item_shop(index, names, types);
}

/* The equipment of the shop. */
void __cdecl playermenu_buy_equipment(int index)
{
    int *action = &sr_data_player[index].menu_action;
    char text[64];

    strcpy(text, "Bandage");
    buy_button(text, 128.0f, action, 1, sr_data_itemtype[ITEM_BANDAGE].price);
    strcpy(text, "Briefcase");
    buy_button(text, 160.0f, action, 2, sr_data_itemtype[ITEM_BRIEFCASE].price);
    back_button(256.0f, action);
}

/* The cars of the car market. A car with no price, or a car that the player cannot pay for,
 * is disabled. */
void __cdecl playermenu_buy_vehicles(int index)
{
    Player *player = &sr_data_player[index];
    const int *rows = sr_data_market.car_rows;
    float y = 128.0f;
    int i;

    for (i = 0; i < sr_data_market.car_base; i++) {
        char text[64];

        car_name(text, rows[i * 3 + 1], rows[i * 3]);
        buy_button(text, y, &player->menu_action, i + 1, rows[i * 3 + 2]);
        if (rows[i * 3 + 2] == 0 || player->money < rows[i * 3 + 2])
            setmenuitem(OPTION_DISABLED);
        y = (float)(y + 32.0);
    }
    back_button((float)((float)(y + 32.0) + 32.0), &player->menu_action);
}

/* The trades of the stock market. Buy and sell 5000 use the keys of 100. */
void __cdecl playermenu_buy_stocks(int index)
{
    static const char *const labels[8] = {"Buy Share", "Sell Share", "Buy 10 Shares", "Sell 10 Shares",
                                          "Buy 100 Shares", "Sell 100 Shares", "Buy 5000 Shares",
                                          "Sell 5000 Shares"};
    static const int keys[8] = {30, 31, 32, 33, 34, 35, 34, 35};
    int *action = &sr_data_player[index].menu_action;
    int i;

    for (i = 0; i < 8; i++)
        action_button(labels[i], 4.0f, (float)(128 + 32 * i), action, i + 1, keys[i]);
    back_button(448.0f, action);
}

/* The menu of a building: Buy and Done. */
void __cdecl playermenu_building_main(int index)
{
    action_button("Buy", 4.0f, 128.0f, &sr_data_player[index].menu_action, 1, KEY_1);
    done_button(192.0f);
}

/* The shop of the building of the player: cars, clothes or guns. A row the player cannot pay
 * for is disabled, and a piece of clothing that the player has is disabled. A clothes row
 * after the second keeps the text of the row before: the price text of that row. */
void __cdecl playermenu_building_buy(int index)
{
    Player *player;
    char text[64];
    float y = 128.0f;
    int building, i;

    if (index == -1)
        return;
    player = &sr_data_player[index];
    building = player->menu_building;
    if (building == -1)
        return;
    for (i = 0; i < *CITY_BUILDING(shop_count, building); i++) {
        const int *row = CITY_BUILDING(shop_rows, building) + i * 3;
        int type = *CITY_BUILDING(building_type, building);

        if (type == BUILDING_CAR_MARKET) {
            car_name(text, row[2], row[0]);
            buy_button(text, y, &player->menu_action, i + 1, row[1]);
            if (player->money < row[1])
                setmenuitem(OPTION_DISABLED);
            y = (float)(y + 32.0);
        }
        if (type == BUILDING_CLOTHES) {
            if (i == 0)
                strcpy(text, "Fancy Suit");
            if (i == 1)
                strcpy(text, "Sunglasses");
            buy_button(text, y, &player->menu_action, i + 1, row[1]);
            if (player->money < row[1])
                setmenuitem(OPTION_DISABLED);
            if (i == 0 && player->look_d == 1)
                setmenuitem(OPTION_DISABLED);
            if (i == 1 && player->look_e == 1)
                setmenuitem(OPTION_DISABLED);
            y = (float)(y + 32.0);
        }
        if (type == BUILDING_GUNS) {
            text[0] = '\0';
            strcat(text, sr_data_itemtype[row[0]].name);
            buy_button(text, y, &player->menu_action, i + 1, row[1]);
            if (player->money < row[1])
                setmenuitem(OPTION_DISABLED);
            y = (float)(y + 32.0);
        }
    }
    done_button((float)(y + 32.0));
}

/* The bank: deposit and the withdrawals. The bank of the buy menu ends with Back, the bank of
 * a building with Done. */
void __cdecl playermenu_banking(int index)
{
    static const char *const labels[7] = {"Deposit", "Withdrawl $5", "Withdrawl $10", "Withdrawl $20",
                                          "Withdrawl $50", "Withdrawl $100", "Withdrawl $1000"};
    static const int values[7] = {7, 1, 2, 3, 4, 5, 6};
    Player *player = &sr_data_player[index];
    int i;

    for (i = 0; i < 7; i++)
        action_button(labels[i], 4.0f, (float)(128 + 32 * i), &player->menu_action, values[i], KEY_1 + i);
    if (player->menu == MENU_BANK)
        back_button(416.0f, &player->menu_action);
    else
        done_button(416.0f);
}

/* Build the items of the menu of a player. */
void __cdecl playermenu(int index)
{
    switch (sr_data_player[index].menu) {
    case MENU_TEAM: playermenu_team(index); break;
    case MENU_SPAWN: playermenu_spawn(index); break;
    case MENU_BUY: playermenu_buy(index); break;
    case MENU_BUY_WEAPONS: playermenu_buy_weapons(index); break;
    case MENU_BUY_AMMO: playermenu_buy_ammo(index); break;
    case MENU_BUY_VEHICLES: playermenu_buy_vehicles(index); break;
    case MENU_BUY_EQUIPMENT: playermenu_buy_equipment(index); break;
    case MENU_BUY_STOCKS: playermenu_buy_stocks(index); break;
    case MENU_BUILDING: playermenu_building_main(index); break;
    case MENU_BUILDING_BUY: playermenu_building_buy(index); break;
    case MENU_BANK:
    case MENU_BANK_2: playermenu_banking(index); break;
    default: done_button(128.0f); break;
    }
}
