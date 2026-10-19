/* The people. See humans.c. */
#ifndef SR_HUMANS_H
#define SR_HUMANS_H

#include <stdint.h>

#include "sr_types.h"

void humansimulation(void);

/* human_body.c */
void deletehuman(uint32_t human);
int createjointbond(int human, int bone_a, int target, const vec3 *pos);
void human_createmuscleball(int human, int parent, int bone, const vec3 *pos);
void human_buildmuscles(int human);
void setitempositionhuman(int item, int human);
void setupbody(void);
int createhuman(float *pos, float *rot, int player);

/* human_pose.c */
void human_calculatelegangles(int human);
void human_collisionhuman(int human);

#endif /* SR_HUMANS_H */
