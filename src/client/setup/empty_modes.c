/* Callbacks for inactive local simulation modes.
 * These callback entries have no local state to update.
 * Each returns without changing the game records.
 */
#include "setup/empty_modes.h"

/* No bond check is required by this callback. */
void checkbonds(void)
{
}

/* This angle callback has no local work. */
void human_calculateanglesobject(void)
{
}

/* This walking callback has no local work. */
void human_walksimulation2(void)
{
}

/* This mission callback has no local work. */
void createscenario_mission(void)
{
}

/* This cooperative callback has no local work. */
void createscenario_coop(void)
{
}

/* This driving callback has no local work. */
void createscenario_driving(void)
{
}

/* This traffic callback has no local work. */
void scenario_createaicar(void)
{
}

/* This checkpoint callback has no local work. */
void scenario_addcheckpoint(void)
{
}
