/* The simulation of objects and vehicles. See vehicles.c, wheels.c and objects.c. */
#ifndef SR_OBJECTS_H
#define SR_OBJECTS_H

#include "sr_math.h"
#include "sr_types.h"

void objectsimulation(void);
void objecttimetolive(void);
void applydamageobject(int obj, int damage);

void objectsimulation_car(int obj);
void objectsimulation_heli(int obj);
void objecttrainsimulation(int obj);
void object_rotorsimulation(int obj);
void objectenginesimulation2(void);
float getenginetorque2(int obj);
void addobjectrotation(int obj, const vec3 *center, const vec3 *axis, float k);

void objectwheelsimulation2(void);
void getobjectrotation3(vec3 *out, int obj);
void objectwheelcollision(int obj, int wheel);
void objectwheelcollision2(int obj, int body);

/* Normalize a vector and return its length. A vector of length 0 gives (0 0 0). The sums and
 * the rounding points are fixed: the result must stay bit exact. */
static __inline float object_normalize(vec3 *out, const vec3 *in)
{
  float len2 = (float)((in->y * in->y + in->x * in->x) + in->z * in->z);
  float s = (float)x87_sqrt(len2);
  if (s == 0.0) {
    out->x = 0.0f;
    out->y = 0.0f;
    out->z = 0.0f;
  } else {
    float inv = (float)(1.0 / s);
    out->x = (float)(inv * in->x);
    out->y = (float)(inv * in->y);
    out->z = (float)(inv * in->z);
  }
  return s;
}

#endif /* SR_OBJECTS_H */
