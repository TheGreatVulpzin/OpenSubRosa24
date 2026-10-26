/* City intersections and streets. */
#ifndef SR_CITY_H
#define SR_CITY_H

#include "sr_runtime.h"

#define g_blocklist sr_data_blocklist
#define g_materiallist sr_data_materiallist

void city_addintersection(int x, int y, int z);
void addcitystreet(int a, int b, int width_a, int width_b, const char *name);
void buildcityinterection(int intersection);
void buildcitystreet(int road);

#endif
