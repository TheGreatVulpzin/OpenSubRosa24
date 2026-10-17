/* The bullets. See bullets.c. */
#ifndef SR_BULLETS_H
#define SR_BULLETS_H

#include "sr_types.h"

void setupbullettypes(void);
int createbullet(int type, const vec3 *pos, const vec3 *vel, int owner);
void bullettimetolive(void);

#endif /* SR_BULLETS_H */
