/* Access to the game globals while the code runs inside subrosadedicated024d.exe.
 *
 * The 024d uses ASLR: the real address of each global is sr_base + RVA.
 * In Phase 4 (own server), these macros will point to our own arrays.
 */
#ifndef SR_RUNTIME_H
#define SR_RUNTIME_H

#include <stdint.h>

#include "sr_globals_024d.h"
#include "sr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Base of the game module. The hook DLL sets it at startup. */
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
