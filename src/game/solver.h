/* The bond solver (solver.c). */
#ifndef SR_SOLVER_H
#define SR_SOLVER_H

#include "sr_types.h"

void bond_partpart(int i);
void bond_bodybody(int i);

/* Friction against a surface with the normal n: v keeps its part along n, and its sideways
 * part shrinks, at most by friction times the part along n. Returns the lost sideways speed
 * scaled by 480 (0 when the sideways part is very short). */
float calculatefriction(vec3 *v, const vec3 *n, float friction);

#endif
