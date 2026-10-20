/* The network of the server: receive, send and voice. See server.c and docs/game/network.md. */
#ifndef SR_NET_SERVER_H
#define SR_NET_SERVER_H

#include <stdint.h>

#include "sr_runtime.h"

/* Globals of the network code. */
#define sr_serverinfo     (*SR_AT(Server, SR_RVA_SERVER))
#define sr_option         (*SR_AT(Option, SR_RVA_OPTION))
#define sr_masterserver   (*SR_AT(MasterServer, SR_RVA_MASTERSERVER))
#define sr_connections    SR_AT(Connection, SR_RVA_CONNECTION)
#define sr_speex          (*SR_AT(Speex, SR_RVA_SPEEX))
#define sr_market         (*SR_AT(Market, SR_RVA_MARKET))
#define sr_objectpacket   (*SR_AT(ObjectPacket, SR_RVA_OBJECTPACKET))
#define sr_radix          (*SR_AT(Radix, SR_RVA_RADIX))
#define sr_numofconnections (*SR_AT(int, 0x1c4afc0))
#define sr_kickedip       SR_AT(uint32_t, 0x103485a0)
#define sr_numofkicked    (*SR_AT(int, 0x10370c00))
#define sr_numofaccounts  (*SR_AT(int, SR_RVA_ACCOUNT + 4))
#define sr_account_key    (*SR_AT(uint32_t, SR_RVA_ACCOUNT))
#define sr_accounts       SR_AT(Account, SR_RVA_ACCOUNT + 8)
#define sr_currentversion (*SR_AT(int, 0x603c8))
#define sr_debugit        (*SR_AT(int, 0x204020))

void serverrecv(void);
void servercycle(void);
void disablesocket(int index);
int getmasterserverip(void);
int server_playermessage(int player, char *text);
void calculateplayervoice(int conn, int player);
void serversend(void);

#endif /* SR_NET_SERVER_H */
