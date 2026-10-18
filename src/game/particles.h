/* Particles and bonds: the points with a mass that make up people, vehicles and objects,
 * and the links between them. */
#ifndef SR_PARTICLES_H
#define SR_PARTICLES_H

#include "sr_types.h"

int createparticle(int type, const vec3 *pos, const vec3 *vel, float mass, int object);
void particlesimulation(void);
void particletimetolive(void);

int createbond(int subtype, int particle_a, int particle_b, int object);
void deletebond(int bond);
void bondtimetolive(void);

#endif
