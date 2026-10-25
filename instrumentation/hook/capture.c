/* Packet capture: every UDP datagram that the server receives or sends.
 *
 * subvulpes.ini: capture_packets=1. Output: sv_packets.bin in the folder of the server.
 * The file has player names and chat: it stays on the PC (build/ is not in git).
 *
 * Format (little endian):
 *   header  "SVPK" (4 bytes), version u32 = 1, image base u32
 *   record  dir u8 (0 = received, 1 = sent), time_ms u32 (since the first packet),
 *           tick u32 (game+0x38), ip u32 (network order), port u16 (host order),
 *           length u16, then the datagram bytes
 * tools/net/capture.py reads it and decodes the datagrams with srpacket.py.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "capture.h"
#include "hook.h"
#include "sr_runtime.h"

static int g_capture = -1;
static FILE *g_file;
static DWORD g_start;

static void open_capture(void)
{
    char path[MAX_PATH], *slash;
    uint32_t version = 1, base = (uint32_t)sr_base;
    g_capture = sv_config_int("capture_packets", 0);
    if (!g_capture)
        return;
    GetModuleFileNameA(NULL, path, MAX_PATH);
    slash = strrchr(path, '\\');
    if (slash)
        slash[1] = 0;
    strncat(path, "sv_packets.bin", MAX_PATH - strlen(path) - 1);
    g_file = fopen(path, "wb");
    if (!g_file) {
        g_capture = 0;
        sv_log("capture: cannot write %s", path);
        return;
    }
    fwrite("SVPK", 4, 1, g_file);
    fwrite(&version, 4, 1, g_file);
    fwrite(&base, 4, 1, g_file);
    g_start = GetTickCount();
    sv_log("capture: packets go to sv_packets.bin");
}

void sv_capture(int dir, const void *data, int len, const void *addr)
{
    uint8_t d = (uint8_t)dir;
    uint32_t t, tick = 0, ip = 0;
    uint16_t port = 0, n = (uint16_t)len;
    const uint8_t *sa = (const uint8_t *)addr;

    if (g_capture < 0)
        open_capture();
    if (!g_capture || len <= 0 || len > 0xffff)
        return;
    t = GetTickCount() - g_start;
    if (sr_base)
        tick = *SR_AT(uint32_t, SR_RVA_GAME + 0x38);
    if (sa && sa[0] == 2 && sa[1] == 0) {  /* sockaddr_in: AF_INET, port (big endian), address */
        port = (uint16_t)(sa[2] << 8 | sa[3]);
        memcpy(&ip, sa + 4, 4);
    }
    fwrite(&d, 1, 1, g_file);
    fwrite(&t, 4, 1, g_file);
    fwrite(&tick, 4, 1, g_file);
    fwrite(&ip, 4, 1, g_file);
    fwrite(&port, 2, 1, g_file);
    fwrite(&n, 2, 1, g_file);
    fwrite(data, 1, n, g_file);
    fflush(g_file);
}
