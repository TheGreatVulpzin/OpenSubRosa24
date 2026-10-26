/* The network of the server: receive, send and voice. See server.c and docs/game/network.md. */
#ifndef SR_NET_SERVER_H
#define SR_NET_SERVER_H

#include <stdint.h>

#include "sr_runtime.h"

/* Globals of the network code. */
#define sr_serverinfo     sr_data_server
#define sr_option         sr_data_option
#define sr_masterserver   sr_data_masterserver
#define sr_connections    sr_data_connection
#define sr_speex          sr_data_speex
#define sr_city           sr_data_city
#define sr_game_world     sr_data_game_world
#define sr_market         sr_data_market
#define sr_objectpacket   sr_data_objectpacket
#define sr_radix          sr_data_radix
#define sr_numofconnections sr_data_numofconnections
#define sr_kickedip       sr_data_kickedip
#define sr_numofkicked    sr_data_numofkicked
#define sr_numofaccounts  (*((int *)&sr_data_accounts.count))
#define sr_account_key    (*((uint32_t *)&sr_data_accounts.key))
#define sr_accounts       ((Account *)&sr_data_accounts.records)
#define sr_currentversion sr_data_currentversion
#define sr_debugit        sr_data_debugit

void serverrecv(void);
void servercycle(void);
void disablesocket(int index);
int getmasterserverip(void);
int server_playermessage(int player, char *text);
void calculateplayervoice(int conn, int player);
void serversend(void);
void packevent(int index, int conn);
void packobjectpacket(int conn, int player);
void packobjectpacket2(int conn, int player);
int item_is_sent(const Item *it, int index, uint32_t sequence, const vec3 *ref);
void unpackevent(int index);
void unpackobjectpacket(void);
void clientrecv(void);

#endif /* SR_NET_SERVER_H */
