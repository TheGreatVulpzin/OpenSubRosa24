/* Eventos de rede do servidor.
 *
 * Os eventos ficam num buffer circular de 65536 posições (sr_events).
 * O servidor envia para cada cliente os eventos que ele ainda não confirmou
 * (Connection.event_ack). Ver docs/structs.md, "Achados de protocolo".
 */
#include <string.h>

#include "sr_runtime.h"
#include "sr_server.h"

/* Original: 024d RVA 0x6260 (createevent_message no ELF 0.24c).
 *
 * Cria um evento de mensagem de texto (tipo 2).
 *   kind   = tipo da mensagem (vai para Event.attach_type)
 *   text   = texto; o jogo grava no máximo 63 caracteres
 *   target = destino (-1 = todos); vai para Event.attach_id
 *   extra  = vai para Event.range_mode
 * Só funciona no servidor (sr_server != 0).
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

/* Original: 024d RVA 0x6660 (createevent_updatedoor no ELF 0.24c).
 * Evento tipo 10: muda o estado de uma porta. */
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

/* Dados de uma missão: game + 0xd0 + time * 0x117c + missão * 0x40 (0x40 bytes por missão).
 * Os campos ainda não têm nome. O offset é relativo ao início da missão. */
static int mission_field(int team, int mission, int offset)
{
    return *SR_AT(int, SR_RVA_GAME + 0xd0 + (uint32_t)team * 0x117c + (uint32_t)mission * 0x40 + (uint32_t)offset);
}

/* Original: 024d RVA 0x6760 (createevent_updatemission no ELF 0.24c).
 * Evento tipo 12: empacota o estado de uma missão de um time em 4 inteiros. */
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
    /* Contas em unsigned: o original multiplica e soma com estouro em 32 bits. */
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
