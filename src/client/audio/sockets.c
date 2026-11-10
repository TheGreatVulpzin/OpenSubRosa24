/* Datagram sockets share an IPv4 address and eight handle slots.
 * Server sockets bind to the configured port.
 * Client sockets use a port that the system selects on the first send.
 */
#include "audio/sockets.h"
#include "audio/imports.h"
#include "sr_runtime.h"
#include <string.h>

enum { SOCKET_IPV4 = 2, SOCKET_DATAGRAM = 2, SOCKET_UDP = 17,
       SOCKET_CREATE = 23, SOCKET_BIND = 2, SOCKET_IOCTL = 12,
       SOCKET_ADDRESS_BYTES = 16 };
#define SOCKET_NONBLOCK 0x8004667eu

/* Fill the shared address and create a handle. Socket errors remain in the handle slot. */
static void open_socket(int index)
{
    static unsigned (__stdcall *create_socket)(int, int, int);
    uint16_t family = SOCKET_IPV4, port;
    uint32_t address = 0;
    if (!create_socket)
        *(void **)&create_socket = sr_import_ordinal("WSOCK32.DLL", SOCKET_CREATE);
    sr_bsdsocket.port = (int16_t)sr_data_option.port;
    sr_bsdsocket.handle[index] = (int)create_socket(SOCKET_IPV4, SOCKET_DATAGRAM, SOCKET_UDP);
    port = (uint16_t)sr_bsdsocket.port;
    port = (uint16_t)((port >> 8) | (port << 8));
    memcpy(sr_bsdsocket.addr, &family, sizeof(family));
    memcpy(sr_bsdsocket.addr + 2, &port, sizeof(port));
    memcpy(sr_bsdsocket.addr + 4, &address, sizeof(address));
    sr_bsdsocket.addr_len = SOCKET_ADDRESS_BYTES;
}

/* Request nonblocking I/O and mark the slot enabled even if the request fails. */
static void enable_socket(int index)
{
    static int (__stdcall *control_socket)(unsigned, long, unsigned long *);
    unsigned long enabled = 1;
    if (!control_socket)
        *(void **)&control_socket = sr_import_ordinal("WSOCK32.DLL", SOCKET_IOCTL);
    control_socket(sr_bsdsocket.handle[index], SOCKET_NONBLOCK, &enabled);
    sr_bsdsocket.enabled[index] = 1;
}

/* Enable a server socket only when its bind succeeds. */
void enablesocketserver(int index)
{
    static int (__stdcall *bind_socket)(unsigned, void *, int);
    open_socket(index);
    if (!bind_socket)
        *(void **)&bind_socket = sr_import_ordinal("WSOCK32.DLL", SOCKET_BIND);
    if (bind_socket(sr_bsdsocket.handle[index], sr_bsdsocket.addr, SOCKET_ADDRESS_BYTES) == 0)
        enable_socket(index);
}

/* Create a client socket and mark its slot enabled. */
void enablesocketclient(int index)
{
    open_socket(index);
    enable_socket(index);
}
