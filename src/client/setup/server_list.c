/* The request for the list of online servers: one packet of type 0x21 to the master server. */
#include "setup/server_list.h"

#include <stdint.h>

#include "client.h"
#include "sr_runtime.h"

#define g_master sr_data_masterserver

#define PACKET_SERVER_LIST 0x21

/* Game functions of other files. */
#define resetsendpacket CL_FN(0x4fe4f0, void(__cdecl *)(void))
#define sendpacket CL_FN(0x4fe520, void(__cdecl *)(uint32_t address, uint16_t port))
#define cs_pwrite CL_FN(0x4fe5b0, int(__cdecl *)(const void *data, int size, int count))

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
