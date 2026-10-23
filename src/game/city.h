/* City intersections and streets. */
#ifndef SR_CITY_H
#define SR_CITY_H

#include "sr_runtime.h"

#define g_blocklist (*SR_AT(BlockList, SR_RVA_BLOCKLIST))
#define g_materiallist (*SR_AT(MaterialList, SR_RVA_MATERIALLIST))

void city_addintersection(int x, int y, int z);
void addcitystreet(int a, int b, int width_a, int width_b, const char *name);
void buildcityinterection(int intersection);
void buildcitystreet(int road);

#endif
