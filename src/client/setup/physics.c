/* A hosted tick fixes the initial bond count before it moves the game pools.
 * Item logic follows the physical solvers. Expired items, objects, particles
 * and bonds are removed in that order.
 */
#include "setup/physics.h"

#include "client.h"
#include "humans.h"
#include "items.h"
#include "objects.h"
#include "particles.h"
#include "sr_runtime.h"

/* Game functions of other files. */
#define cityobject_simulation CL_FN(0x4665f0, void(__cdecl *)(void))
#define bondsimulation2 CL_FN(0x450700, void(__cdecl *)(void))
#define itemtimetolive CL_FN(0x497130, void(__cdecl *)(void))

/* Run one physical tick and then expire each packed pool. */
void physicssimulation(void)
{
    sr_data_numofstaticbonds = sr_numofbonds;
    particlesimulation();
    objectsimulation();
    cityobject_simulation();
    humansimulation();
    itemsimulation();
    bondsimulation2();
    itemlogic();
    itemtimetolive();
    objecttimetolive();
    particletimetolive();
    bondtimetolive();
}
