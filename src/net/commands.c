/* The chat of the server: the word filter and the commands of an admin.
 *
 * server_playermessage runs for each chat text of a player. It returns 1 when the text is a
 * command (or the player is muted) and the text must not go to the chat. A text that starts
 * with "/" is a command. A command other than /admin works only for an admin.
 *
 * Every message of a command goes to everybody as a type 4 message, through the global text
 * buffer of the game (msgbuf). The prefix tests are plain strncmp: "/arm" also starts "/army".
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "events.h"
#include "items.h"
#include "server.h"
#include "sr_server.h"

/* Game functions of other files. */
int __cdecl lf_28190(int human, int kind, int count); /* scenario_armhuman */
int __cdecl lf_31800(int type, vec3 *pos, int *vel, vec3 rot[3], int flag); /* createobject */

#define msgbuf SR_AT(char, 0x61848)                  /* the scratch text of the game, 64 bytes */
#define filtered_word SR_AT(const char, 0x5efec)     /* the word that the chat filter replaces */
#define FILTER_LENGTH 6
#define MAX_ADMIN_TRIES 5
#define PHONE_ID 0x1a0a /* phone_id of the phones of an admin (/phone) */

static void say(const char *text)
{
    sr_createevent_message(4, text, -1, 0);
}

/* The admin password check. The connection and the player both get the flag. */
static void admin_login(int player, const char *text)
{
    Player *p = &sr_players[player];
    int i, found = -1;
    if (strncmp(text, "/admin ", 7) != 0 || ++p->admin_tries >= MAX_ADMIN_TRIES)
        return;
    if (strcmp(text + 7, sr_option.password) != 0)
        return;
    for (i = 0; i < sr_numofconnections; i++) {
        if (sr_connections[i].player_id == player)
            found = i;
    }
    p->is_admin = 1;
    if (found != -1)
        sr_connections[found].is_admin = 1;
    sprintf(msgbuf, "%s admin", p->name);
    say(msgbuf);
}

/* List the connections from first to last (exclusive): the name and the account. */
static void list_connections(int first, int last)
{
    int c;
    for (c = first; c < sr_numofconnections; c++) {
        int pid = sr_connections[c].player_id;
        if (c < last && pid != -1) {
            sprintf(msgbuf, "%s  %d", sr_players[pid].name, sr_players[pid].account);
            say(msgbuf);
        }
    }
}

/* /phone: remove the phones of the admins and give the player a new one. The phone goes in
 * slot 6 of the human, or stays on the floor when the slot is full. */
static void give_phone(int player)
{
    int i;
    int human;
    uint32_t it;
    for (i = 0; i < SR_MAX_ITEM; i++) {
        Item *item = &sr_items[i];
        if (item->active != 0 && item->type == 0x17 && item->phone_id == PHONE_ID) {
            if (item->linked_item != -1) {
                Item *linked = &sr_items[item->linked_item];
                linked->call_state = 0;
                linked->linked_item = -1;
                linked->info = 0;
                createevent_updateiteminfo((uint32_t)item->linked_item);
            }
            item->time_to_live = 0;
        }
    }
    human = sr_players[player].human_id;
    it = createitem(0x17, &sr_humans[human].bones[0].pos, NULL, sr_humans[human].bones[0].rot);
    human = sr_players[player].human_id;
    sr_items[it].phone_id = PHONE_ID;
    if (it >= 0x400)
        return;
    if (human == -1) {
        if (sr_items[it].parent_item != -1) {
            item_removeitemlink(it, (uint32_t)sr_items[it].parent_item);
            createevent_updateitem(it);
        } else if (sr_items[it].parent_human != -1) {
            item_removehumanlink(it, sr_items[it].parent_human, sr_items[it].parent_slot);
            createevent_updateitem(it);
        }
    } else {
        InventorySlot *slot = &sr_humans[human].inventory[6];
        if (slot->count < 2 && sr_itemtypes[sr_items[it].type].is_gun == 0) {
            if (sr_items[it].parent_item != -1) {
                item_removeitemlink(it, (uint32_t)sr_items[it].parent_item);
                sr_items[it].parent_item = -1;
            }
            if (sr_items[it].parent_human != -1)
                item_removehumanlink(it, sr_items[it].parent_human, sr_items[it].parent_slot);
            if (slot->count < 8) {
                slot->items[slot->count] = (int)it;
                slot->count++;
                sr_items[it].parent_human = human;
                sr_items[it].parent_slot = 6;
            }
            createevent_updateitem(it);
        }
    }
}

/* /heli: make a helicopter 4 units behind the human. */
static void make_heli(int human)
{
    const Bone *b = &sr_humans[human].bones[0];
    vec3 pos, mat[3];
    pos.x = (float)(b->pos.x - b->rot[2].x * 4.0);
    pos.y = (float)(b->pos.y - b->rot[2].y * 4.0);
    pos.z = (float)(b->pos.z - b->rot[2].z * 4.0);
    memset(mat, 0, sizeof mat);
    mat[0].x = 1.0f;
    mat[1].y = 1.0f;
    mat[2].z = 1.0f;
    lf_31800(0xb, &pos, NULL, mat, 1);
}

/* /kick NAME: kick the last connection whose player has this name, and add its ip to the
 * kick list (the list has no size limit). */
static void kick_player(const char *name)
{
    int i, found = -1;
    for (i = 0; i < sr_numofconnections; i++) {
        int pid = sr_connections[i].player_id;
        if (pid != -1 && strcmp(name, sr_players[pid].name) == 0)
            found = i;
    }
    if (found != -1) {
        sr_kickedip[sr_numofkicked] = sr_connections[found].ip;
        sr_numofkicked++;
        sr_connections[found].idle_ticks = 0x4b0;
    }
}

/* /give ACCOUNT CASH. The sscanf variables start at -1 and 0, so an input with no number does nothing. */
static void give_cash(const char *args)
{
    int a = -1, b = 0, i;
    strcpy(msgbuf, args);
    if (msgbuf[0] == '\0' || sscanf(msgbuf, "%d %d", &a, &b) == -1 || a < 0 || a >= sr_numofaccounts)
        return;
    sr_accounts[a].money += b;
    for (i = 0; i < SR_MAX_PLAYER; i++) {
        if (sr_players[i].active != 0 && sr_players[i].account == a)
            sr_players[i].money += b;
    }
    sprintf(msgbuf, "%s(%d) %d cash", sr_accounts[a].name, a, b);
    say(msgbuf);
}

/* The commands of an admin. n is the loop counter of the word filter: /bany uses it as the ban
 * time when the text has no number. */
static void admin_commands(int player, const char *text, int n)
{
    Player *p = &sr_players[player];
    int i, a;

    if (strncmp(text, "/hlist", 6) == 0) {
        for (i = 0; i < 10; i++) {
            const Human *h = &sr_humans[i];
            if (h->active != 0) {
                if (h->player_id == -1)
                    sprintf(msgbuf, "%d:%d", i, -1);
                else
                    sprintf(msgbuf, "%d:%d %s", i, h->player_id, sr_players[h->player_id].name);
                say(msgbuf);
            }
        }
    }
    if (strncmp(text, "/list1", 6) == 0)
        list_connections(0, 10);
    if (strncmp(text, "/list2", 6) == 0 && 10 < sr_numofconnections)
        list_connections(10, 20);
    if (strncmp(text, "/list3", 6) == 0 && 20 < sr_numofconnections)
        list_connections(20, 0x7fffffff);
    if (strncmp(text, "/godmode", 8) == 0) {
        p->godmode ^= 1;
        say(p->godmode == 0 ? "godmode off" : "godmode on");
    }
    if (strncmp(text, "/heli", 5) == 0 && p->human_id != -1)
        make_heli(p->human_id);
    if (strncmp(text, "/phone", 6) == 0)
        give_phone(player);
    if (strncmp(text, "/arm", 4) == 0 && p->human_id != -1)
        lf_28190(p->human_id, 2, 4);
    if (strncmp(text, "/cash", 5) == 0)
        p->money += 100000;
    if (strncmp(text, "/resetgame", 10) == 0)
        sr_serverinfo.reset_request = 1;
    if (strncmp(text, "/ban1 ", 6) == 0) {
        a = -1;
        strcpy(msgbuf, text + 6);
        if (msgbuf[0] != '\0' && sscanf(msgbuf, "%d", &a) != -1 && a >= 0 && a < sr_numofaccounts) {
            sr_serverinfo.ban_candidate = a;
            sprintf(msgbuf, "%s %d ban?", sr_accounts[a].name, a);
            say(msgbuf);
        }
    }
    if (strncmp(text, "/give ", 6) == 0)
        give_cash(text + 6);
    if (strncmp(text, "/bany ", 6) == 0) {
        strcpy(msgbuf, text + 6);
        if (msgbuf[0] != '\0' && sscanf(msgbuf, "%d", &n) != -1) {
            a = sr_serverinfo.ban_candidate;
            if (a >= 0 && a < sr_numofaccounts) {
                sr_accounts[a].ban_time = n;
                sprintf(msgbuf, "%s %d banned %d", sr_accounts[a].name, a, n);
                say(msgbuf);
                sr_serverinfo.ban_candidate = -1;
            }
        }
    }
    if (strncmp(text, "/bann", 5) == 0)
        sr_serverinfo.ban_candidate = -1;
    if (strncmp(text, "/kick ", 6) == 0)
        kick_player(text + 6);
    if (strncmp(text, "/message ", 9) == 0) {
        memcpy(msgbuf, text + 9, 64); /* reads past the text */
        sr_createevent_message(0, msgbuf, -1, 0);
    }
    if (strncmp(text, "/clearkick", 10) == 0)
        sr_numofkicked = 0;
}

/* Filter and handle a chat text of a player. text has 64 bytes. */
int server_playermessage(int player, char *text)
{
    Player *p;
    int len, n, total;
    if (player == -1)
        return 0;
    p = &sr_players[player];
    if (p->chat_block > 0)
        return 1;
    text[63] = '\0';
    len = (int)strlen(text);
    if (len > 63)
        len = 63;
    for (n = 0; n < len; n++)
        msgbuf[n] = (char)tolower(text[n]);
    msgbuf[len] = '\0';
    p->orig_text[0] = '\0';
    total = (int)strlen(msgbuf) - 5;
    for (n = 0; n < total; n++) {
        if (strncmp(msgbuf + n, filtered_word, FILTER_LENGTH) == 0) {
            strcpy(p->orig_text, text);
            strcpy(text, "i'm an idiot");
        }
    }
    if (text[0] != '/')
        return 0;
    admin_login(player, text);
    if (p->is_admin != 0)
        admin_commands(player, text, n);
    return 1;
}
