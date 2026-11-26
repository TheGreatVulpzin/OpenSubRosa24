/* The request for the list of online servers: one packet of type 0x21 to the master server. */
#include "setup/server_list.h"

#include <stdint.h>

#include "client.h"
#include "packet.h"
#include "sr_runtime.h"

#define g_master sr_data_masterserver

#define PACKET_SERVER_LIST 0x21

/* Send the request. Nothing goes out while the address of the master server is zero. */
void __cdecl getonlineserverlist(void)
{
    uint8_t type = PACKET_SERVER_LIST;

    if (g_master.ip == 0)
        return;
    resetsendpacket();
    cs_pwrite(&type, 1, 1);
    sendpacket(g_master.ip, (uint16_t)g_master.port);
}
