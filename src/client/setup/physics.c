/* A hosted tick fixes the initial bond count before it moves the game pools.
 * Item logic follows the physical solvers. Expired items, objects, particles
 * and bonds are removed in that order.
 */
#include "setup/physics.h"

#include "client.h"
#include "bonds.h"
#include "humans.h"
#include "items.h"
#include "objects.h"
#include "particles.h"
#include "sr_runtime.h"
#include "render/cityobjects.h"

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
