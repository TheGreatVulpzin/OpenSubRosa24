/* Acesso às globais do jogo enquanto o código roda dentro do subrosadedicated024d.exe.
 *
 * O 024d usa ASLR: o endereço real de cada global é sr_base + RVA.
 * Na Fase 4 (servidor próprio), estas macros passam a apontar para arrays nossos.
 */
#ifndef SR_RUNTIME_H
#define SR_RUNTIME_H

#include <stdint.h>

#include "sr_globals_024d.h"
#include "sr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Base do módulo do jogo. O DLL de hooks preenche no início. */
extern uintptr_t sr_base;

#define SR_AT(type, rva) ((type *)(sr_base + (rva)))

#define sr_server       (*SR_AT(int, SR_RVA_SERVER))
#define sr_game         (*SR_AT(Game, SR_RVA_GAME))
#define sr_events       SR_AT(Event, SR_RVA_EVENT)
#define sr_numofevents  (*SR_AT(uint32_t, SR_RVA_NUMOFEVENTS))
#define sr_players      SR_AT(Player, SR_RVA_PLAYER)

#ifdef __cplusplus
}
#endif

#endif /* SR_RUNTIME_H */
