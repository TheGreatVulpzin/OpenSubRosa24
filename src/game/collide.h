/* The collision tests against the level. See collide.c. */
#ifndef SR_COLLIDE_H
#define SR_COLLIDE_H

#include "sr_types.h"

int sphereintersectlandscape(float *pos, float radius);
int lineintersectlandscape(float *from, float *to);
int circleintersectlandscape(float *pos, float *rot, float radius);
int sphereintersectlevel(float *pos, float radius);
int lineintersectlevel(float *from, float *to, int with_objects);
int lineintersectall(float *from, float *to);
int circleintersectlevel(float *pos, float *rot, float radius);
int sphereintersectobject(int obj, float *pos, float radius);
int lineintersectobject(int obj, float *from, float *to, int with_wheels);
int lineintersecttrackrail(float *from, float *to, int k);

int sphereintersectcityobjecttype(vec3 *hit, vec3 *normal, float *dist, int type, const vec3 *pos, const vec3 rot[3],
                                  const vec3 *center, float r);

int lineintersectcityobject(float *hit, float *normal, float *t, int obj, float *from, float *to);

int pointinsidebase(float *pos, int team, float margin);
int pointinsidebasesafe(float *pos, int team, float margin);
int pointinsidebuilding(int building, float *pos, float margin);

#endif /* SR_COLLIDE_H */
