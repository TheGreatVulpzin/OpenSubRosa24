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
#define sr_items        SR_AT(Item, SR_RVA_ITEM)
#define sr_itemtypes    SR_AT(ItemType, SR_RVA_ITEMTYPE)
#define sr_humans       SR_AT(Human, SR_RVA_HUMAN)
#define sr_objects      SR_AT(Object, SR_RVA_OBJECT)
#define sr_bullets      SR_AT(Bullet, SR_RVA_BULLET)
#define sr_numofbullets (*SR_AT(int, SR_RVA_NUMOFBULLETS))
#define sr_stats        (*SR_AT(Stats, SR_RVA_STATS))
#define sr_particles    SR_AT(Particle, SR_RVA_PARTICLE)
#define sr_numofparticles (SR_AT(AreaSector, SR_RVA_AREA_SECTOR)->num_particles)
#define sr_bonds        SR_AT(Bond, SR_RVA_BOND)
#define sr_numofbonds   (*SR_AT(int, SR_RVA_NUMOFBONDS))
#define sr_bondopt      SR_AT(BondOpt, SR_RVA_BONDOPT)
#define sr_bondparticles SR_AT(BondParticle, SR_RVA_BONDPARTICLE)
#define sr_bondhumans   SR_AT(BondHuman, SR_RVA_BONDHUMAN)
#define sr_bsdsocket    (*SR_AT(BsdSocket, SR_RVA_BSDSOCKET))
#define sr_socketid     (*SR_AT(uint32_t, 0x60d08u)) /* "7DFP": the first 4 bytes of each packet */

#ifdef __cplusplus
}
#endif

#endif /* SR_RUNTIME_H */
