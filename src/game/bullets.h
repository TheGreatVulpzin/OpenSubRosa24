/* The bullets. See bullets.c. */
#ifndef SR_BULLETS_H
#define SR_BULLETS_H

#include "sr_runtime.h"
#include "sr_types.h"

/* A bullet type: real cartridge data in kilograms and meters. ItemType.bullet_type selects
 * one. */


/* The four bullet types (setupbullettypes). */
#define g_bullettypes sr_data_bullettypes

void setupbullettypes(void);
int createbullet(int type, const vec3 *pos, const vec3 *vel, int owner);
void bullettimetolive(void);

/* bullet_sim.c */
void bulletsimulation(void);

#endif /* SR_BULLETS_H */
