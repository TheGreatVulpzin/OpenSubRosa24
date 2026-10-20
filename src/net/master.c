/* The address of the master server. The server asks a web page of the master server for the
 * list of servers, and reads the address of the master server and 8 text lines from the answer.
 * The page is a text of fields that a TAB splits. See docs/game/network.md.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "server.h"
#include "sr_os.h"

#define RECV_LIMIT 0x2000 /* the page is read into the first 8 KB of the receive buffer */
#define INFO_LINES 8
#define INFO_SIZE 0x40
#define FIONBIO_CODE 0x8004667e
#define SO_ERROR_CODE 0x1007
#define SOL_SOCKET_LEVEL 0xffff
#define WAIT_STEPS 300 /* 300 steps of 100 ms for the send and for the answer */

/* The WinSock records of a 32-bit build. */
typedef struct FdSet {
    unsigned count;
    uint32_t sockets[64];
} FdSet;
typedef struct TimeVal {
    long sec, usec;
} TimeVal;
typedef struct HostEnt {
    char *name;
    char **aliases;
    short type, length;
    char **addr_list;
} HostEnt;
typedef struct ServEnt {
    char *name;
    char **aliases;
    short port;
    char *proto;
} ServEnt;

/* Wait for the connection of a non-blocking socket. Returns 1 when it is open. */
static int wait_connected(uintptr_t s)
{
    FdSet reads, writes;
    TimeVal wait = { 3, 0 };
    int err = 0, len = 4;
    reads.count = 1;
    reads.sockets[0] = (uint32_t)s;
    writes = reads;
    if (os_select((int)s + 1, &reads, &writes, NULL, &wait) == 0)
        return 0;
    if (os_fdisset(s, &reads) == 0 && os_fdisset(s, &writes) == 0)
        return 0;
    return os_getsockopt(s, SOL_SOCKET_LEVEL, SO_ERROR_CODE, (char *)&err, &len) == 0 && err == 0;
}

/* Find the end of a field: the TAB, or the limit. */
static int field_end(const char *buf, int i)
{
    while (i < RECV_LIMIT && buf[i] != '\t')
        i++;
    return i;
}

/* Copy up to 32 characters of a field. Returns the index after the copy. */
static int copy_field(char *dst, const char *buf, int i, int *length)
{
    int n = 0;
    while (n < 0x20 && i <= RECV_LIMIT - 1 && buf[i] != '\t') {
        dst[n++] = buf[i++];
    }
    dst[n] = '\0';
    *length = n;
    return i;
}

/* Read the page in the receive buffer. */
static void parse_page(const char *buf)
{
    MasterServer *ms = &sr_masterserver;
    char ip[0x21];
    int count = 0, i, n, line;
    i = field_end(buf, 0);
    sscanf(buf + i + 1, "%d", &count);
    i++;
    while (i <= RECV_LIMIT - 1 && buf[i] != '\t')
        i++;
    i = copy_field(ip, buf, i + 1, &count);
    ms->ip = _byteswap_ulong(os_inet_addr(ip));
    i = field_end(buf, i);
    sscanf(buf + i + 1, "%d", &count);
    ms->port = (int16_t)count;
    if (ms->ip == 0xffffffffu) {
        ms->ip = _byteswap_ulong(os_inet_addr("216.55.186.104"));
        ms->port = 0x6bc6;
    }
    i++;
    while (i <= RECV_LIMIT - 1 && buf[i] != '\t')
        i++;
    for (line = 0; line < INFO_LINES; line++) {
        i = copy_field(&ms->info[line * INFO_SIZE], buf, i + 1, &n);
    }
}

/* Ask the master server for its address. Returns 1 when the answer came. The sockets are not
 * closed when the send or the answer fails (they leak). */
int getmasterserverip(void)
{
    BsdSocket *bs = &sr_bsdsocket;
    static const char request[] = "GET /anewzero/serverinfo.php HTTP/1.1\r\nHost:www.crypticsea.com\r\n\r\n";
    struct {
        uint16_t family, port;
        uint32_t ip;
        uint8_t zero[8];
    } addr;
    unsigned long one = 1;
    uintptr_t s;
    HostEnt *host;
    ServEnt *service;
    int count, n;

    sr_masterserver.ip = 0;
    s = os_socket(2, 1, 6);
    if (s == (uintptr_t)(uint32_t)-1)
        return 0;
    host = (HostEnt *)os_gethostbyname("www.crypticsea.com");
    if (!host)
        return 0;
    service = (ServEnt *)os_getservbyname("http", "tcp");
    memset(&addr, 0, sizeof addr);
    addr.family = 2;
    memcpy(&addr.ip, host->addr_list[0], 4);
    addr.port = service ? (uint16_t)service->port : (uint16_t)0x5000; /* port 80 in network order */
    strcpy((char *)bs->send_buf, request);
    os_ioctlsocket(s, (long)FIONBIO_CODE, &one);
    /* A connect that does not fail at once also gives up: the socket is non-blocking. */
    if (os_connect(s, &addr, 16) != -1 || !wait_connected(s))
        return 0;
    for (count = 0, n = 0; count < WAIT_STEPS && n <= 0; count++) {
        os_sleep(100);
        n = os_send(s, (const char *)bs->send_buf, (int)strlen((const char *)bs->send_buf), 0);
    }
    if (n <= 0)
        return 0;
    for (count = 0, n = 0; count < WAIT_STEPS && n <= 0; count++) {
        os_sleep(100);
        n = os_recv(s, (char *)bs->recv_buf, RECV_LIMIT, 0);
    }
    if (n <= 0)
        return 0;
    os_closesocket(s);
    parse_page((const char *)bs->recv_buf);
    return 1;
}
