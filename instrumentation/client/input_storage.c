/* Shared input fields use client addresses in the image adapter.
 * Packet fields can have a different offset from the server workspace.
 * This check evaluates addresses without reading image memory.
 */
#include <stdio.h>
#include "sr_runtime.h"

uintptr_t sr_base = 0x20000000u;

typedef struct InputAddress {
    const char *name;
    uintptr_t address;
    unsigned rva;
} InputAddress;

/* Return nonzero when an input field resolves to a different client address. */
int main(void)
{
    const InputAddress fields[] = {
        {"server", (uintptr_t)&sr_data_server.active, 0x75a5800},
        {"invertmouse", (uintptr_t)&sr_data_option.invertmouse, 0x90631ec},
        {"view", (uintptr_t)&sr_data_view, 0xfefe2a0},
        {"connections", (uintptr_t)&sr_data_numofconnections, 0x1b9b4e0},
        {"link_state", (uintptr_t)&sr_game.link_state, 0x9065304},
        {"gametype", (uintptr_t)&sr_game.gametype, 0x9065308},
        {"state", (uintptr_t)&sr_game.state, 0x9065320},
        {"option_menu", (uintptr_t)&sr_game.option_menu, 0x9065310},
        {"voice_menu", (uintptr_t)&sr_game.voice_menu, 0x9065314},
        {"connections_menu", (uintptr_t)&sr_game.connections_menu, 0x9065318},
        {"spectator_human", (uintptr_t)&sr_game.spectator_human, 0x908aad8},
        {"camera_flag", (uintptr_t)&sr_game.unused_257dc, 0x908aadc},
        {"map_flag", (uintptr_t)&sr_game.unused_257e0, 0x908aae0},
        {"debug_p", (uintptr_t)&sr_game.debug_p, 0x906533c},
        {"debug_o", (uintptr_t)&sr_game.debug_o, 0x9065340},
        {"debug_i", (uintptr_t)&sr_game.debug_i, 0x9065344},
        {"debug_display", (uintptr_t)&sr_data_debug.display, 0x14a8680},
        {"debug_mode", (uintptr_t)&sr_data_debug.display_mode, 0x14a8684},
        {"debug_world", (uintptr_t)&sr_data_debug.world_view, 0x14a8688},
        {"debug_human", (uintptr_t)&sr_data_debug.human_view, 0x14f92e0},
        {"kickedip", (uintptr_t)sr_data_kickedip, 0x1033ed20},
        {"numofkicked", (uintptr_t)&sr_data_numofkicked, 0x103675a4}
    };
    unsigned index;
    for (index = 0; index < sizeof(fields) / sizeof(fields[0]); index++) {
        if (fields[index].address != sr_base + fields[index].rva) {
            fprintf(stderr, "input address mismatch for %s (%08x, %08x)\n", fields[index].name,
                    (unsigned)fields[index].address, (unsigned)(sr_base + fields[index].rva));
            return 1;
        }
    }
    printf("%u client input addresses match\n", (unsigned)(sizeof(fields) / sizeof(fields[0])));
    return 0;
}
