/* Server network events.
 *
 * Events live in a ring buffer of 65536 slots (sr_events).
 * The server sends each client the events that the client did not confirm yet
 * (Connection.event_ack). See docs/structs.md, "Protocol findings".
 */
#include <string.h>

#include "sr_runtime.h"
#include "sr_server.h"

/* Original: 024d RVA 0x6260 (createevent_message in the 0.24c ELF).
 *
 * Create a text message event (type 2).
 *   kind   = message kind (goes to Event.attach_type)
 *   text   = text; the game stores at most 63 characters
 *   target = target (-1 = everybody); goes to Event.attach_id
 *   extra  = goes to Event.range_mode
 * Works only on the server (sr_server != 0).
 */
void __cdecl sr_createevent_message(int kind, const char *text, int target, int extra)
{
    uint32_t index;
    int tick;
    int len;
    Event *ev;

    if (sr_server == 0)
        return;

    index = sr_numofevents;
    tick = sr_game.ticks;
    len = (int)strlen(text);
    if (len > 63)
        len = 63;

    ev = &sr_events[index];
    ev->type = 2;
    ev->tick = tick;
    memcpy(ev->message, text, (size_t)len);
    ev->message[len] = '\0';
    ev->param = len;
    ev->attach_type = kind;
    ev->attach_id = target;
    ev->range_mode = extra;

    sr_numofevents = (index + 1) & 0xffff;
}

/* Original: 024d RVA 0x6660 (createevent_updatedoor in the 0.24c ELF).
 * Event type 10: change the state of a door. */
void __cdecl sr_createevent_updatedoor(int a, int b)
{
    uint32_t index;
    Event *ev;

    if (sr_server == 0)
        return;

    index = sr_numofevents;
    ev = &sr_events[index];
    ev->type = 10;
    ev->tick = sr_game.ticks;
    ev->param = a;
    ev->attach_type = b;
    sr_numofevents = (index + 1) & 0xffff;
}

/* Mission data: game + 0xd0 + team * 0x117c + mission * 0x40 (0x40 bytes per mission).
 * The fields have no names yet. The offset is relative to the start of the mission. */
static int mission_field(int team, int mission, int offset)
{
    return *SR_AT(int, SR_RVA_GAME + 0xd0 + (uint32_t)team * 0x117c + (uint32_t)mission * 0x40 + (uint32_t)offset);
}

/* Original: 024d RVA 0x6760 (createevent_updatemission in the 0.24c ELF).
 * Event type 12: pack the state of one team mission into 4 integers. */
void __cdecl sr_createevent_updatemission(int team, int mission)
{
    uint32_t index;
    Event *ev;

    if (sr_server == 0)
        return;

    index = sr_numofevents;
    ev = &sr_events[index];
    ev->type = 12;
    ev->tick = sr_game.ticks;
    /* Unsigned math: the original multiplies and adds with 32-bit wrap-around. */
    ev->param = (int)((uint32_t)mission_field(team, mission, 0x20) * 0x10000u + (uint32_t)mission * 2u +
                      (uint32_t)mission_field(team, mission, 0x00) + (uint32_t)team * 0x10u +
                      (uint32_t)mission_field(team, mission, 0x08) * 0x100u);
    ev->attach_type = (int)((uint32_t)mission_field(team, mission, 0x24) * 0x100u +
                            (uint32_t)mission_field(team, mission, 0x10) +
                            (uint32_t)mission_field(team, mission, 0x14) * 0x10u);
    ev->attach_id = (int)((uint32_t)(mission_field(team, mission, 0x28) / 60) +
                          ((uint32_t)mission_field(team, mission, 0x2c) + 1u) * 0x10000u);
    ev->range_mode = mission_field(team, mission, 0x30);
    sr_numofevents = (index + 1) & 0xffff;
}
