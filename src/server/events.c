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
