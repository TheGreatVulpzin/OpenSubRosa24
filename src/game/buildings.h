/* Building plans, type tables and city objects. */
#ifndef SR_BUILDINGS_H
#define SR_BUILDINGS_H

#include "sr_runtime.h"

#define BLD_PTR(T, field, b) ((T *)((uint8_t *)&g_city.field + (b) * 0x9dc))
#define BLD_INT(field, b) (*BLD_PTR(int, field, b))

int createcityobject(int type, const vec3 *pos, float angle);
int city_createelevator(const vec3 *pos);
void cityobject_addbox(int type, float x, float y, float z, float sx, float sy, float sz);
void setupcityobjecttypes(void);
void setupbuildingtypes(void);
void newspaper_load(void);

#endif
