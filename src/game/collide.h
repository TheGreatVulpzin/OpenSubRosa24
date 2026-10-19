/* The collision tests against the level. See collide.c. */
#ifndef SR_COLLIDE_H
#define SR_COLLIDE_H

#include <stdint.h>

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
int lineintersecttrack(float *from, float *to, int k);
int lineintersecthuman(int human, float *from, float *to, float r);
int item_sphereintersect(int item, float *pos, float r);

int sphereintersectcityobjecttype(vec3 *hit, vec3 *normal, float *dist, int type, const vec3 *pos, const vec3 rot[3],
                                  const vec3 *center, float r);

int lineintersectcityobject(float *hit, float *normal, float *t, int obj, float *from, float *to);
int lineintersectcityobjecttype(float *hit, float *normal, float *t, int type, float *pos, float *rot, float *from,
                                float *to);

/* collide_area.c: the tests against the blocks of the areas. */
int lineintersectnormalblock(vec3 *hit, vec3 *normal, float *t, const vec3 *from, const vec3 *to, const int cell[3],
                             uint32_t word, float size);
int lineintersectspecblock(vec3 *hit, vec3 *normal, float *t, const vec3 *from, const vec3 *to, const int cell[3],
                           uint32_t word, float size);
int lineintersectarea(float *hit, float *normal, float *t, uint32_t area, float *from, float *to);
int sphereintersectnormalblock(vec3 *hit, vec3 *normal, float *dist, const vec3 *center, float r, const int cell[3],
                               uint32_t word, float size);
int sphereintersectspecblock(vec3 *hit, vec3 *normal, float *dist, const vec3 *center, float r, const int cell[3],
                             uint32_t word, float size);
int sphereintersectarea(float *hit, float *normal, float *dist, uint32_t area, float *pos, float r);
int circleintersectnormalblock(vec3 *hit, vec3 *normal, float *dist, const vec3 *pos, const vec3 *rot, float r,
                               const int cell[3], uint32_t word, float size);
int circleintersectspecblock(vec3 *hit, vec3 *normal, float *dist, const vec3 *pos, const vec3 *rot, float r,
                             const int cell[3], uint32_t word, float size);
int circleintersectarea(float *hit, float *normal, float *dist, uint32_t area, float *pos, float *rot, float r);

int pointinsidebase(float *pos, int team, float margin);
int pointinsidebasesafe(float *pos, int team, float margin);
int pointinsidebuilding(int building, float *pos, float margin);

#endif /* SR_COLLIDE_H */
