/* Access to the bone vectors and flags of the solver workspace. */
#ifndef SR_BOND_STORAGE_H
#define SR_BOND_STORAGE_H

#include "sr_runtime.h"

#ifndef BOND_HUMAN_VEL
#define BOND_HUMAN_VEL(h, j) (sr_bondhumans[h].bones[j].vel)
#define BOND_HUMAN_DV(h, j) (sr_bondhumans[h].bones[j].dv)
#define BOND_HUMAN_MOMENTUM(h, j) (sr_bondhumans[h].bones[j].ang_momentum)
#define BOND_HUMAN_W(h, j) (sr_bondhumans[h].bones[j].w)
#define BOND_HUMAN_DW(h, j) (sr_bondhumans[h].bones[j].dw)
#define BOND_HUMAN_ACTIVE(h) (sr_bondhumans[h].active != 0)
#define BOND_HUMAN_SOLVES(h, j) (((unsigned)sr_bondhumans[h].mask & (1u << (j))) != 0)
#define BOND_HUMAN_CLEAR(h, j) BOND_HUMAN_SOLVES(h, j)
#define BOND_HUMAN_RESET(h) (sr_bondhumans[h].active = 0)
#define BOND_HUMAN_PREPARE(h) (sr_bondhumans[h].active = 1, sr_bondhumans[h].mask = 0)
#define BOND_HUMAN_MARK(h, j) (sr_bondhumans[h].mask |= 1 << (j))
#define BOND_HUMAN_AXIS(h, j, k) (sr_bondhumans[h].bones[j].axes[k].dir)
#define BOND_HUMAN_INERTIA(h, j, axis) (sr_bondhumans[h].bones[j].axes[axis].k)
#define BOND_HUMAN_COPY_AXIS(h, j, k, value, inertia) \
  (BOND_HUMAN_AXIS(h, j, k) = (value), BOND_HUMAN_INERTIA(h, j, k) = (inertia))
#define BOND_COPY_MODE 1
#define BOND_MODE(b, o) ((uint16_t)(o)->mode)
#define BOND_BALANCE_NONNEGATIVE(value) (0.0 <= (value))
#define BOND_BONE_SPRING 0
#endif

#endif
