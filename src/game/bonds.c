/* bondsimulation2: the bond solver of the physics, once for each tick.
 *
 * Each section has a profiler timer with its name. bondsetup prepares one BondOpt record for
 * each bond (setup_partpart and the others). bondsetupcopy copies the velocities into the work
 * arrays (setup_copy). Then 32 solver passes go over the bonds: each bond adds its velocity
 * change to dv and dw, and the end of the pass adds them to the velocities. The end copies
 * the velocities back (copy_back).
 */
#include "bonds.h"
#include "bond_storage.h"

#include <stdint.h>

#include "objects.h"
#include "profiler.h"
#include "solver.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define g_yaxis sr_data_yaxis /* (0 1 0) */
#define g_zaxis sr_data_zaxis /* (0 0 1) */
#define g_bondpass sr_data_bondpass          /* the number of the solver pass; the wheels read it */
#define g_numofstaticbonds sr_data_numofstaticbonds /* the bonds before the contacts of this tick */
#define g_numofdynamicbonds sr_data_numofdynamicbonds /* the bonds with the contacts of the last tick */

/* bondsetup, type 0: two particles. The direction from a to b, the stretch, and the share of
 * each side from the masses. */
static void setup_partpart(const Bond *b, BondOpt *o)
{
  const Particle *pa = &sr_particles[b->particle_a];
  const Particle *pb = &sr_particles[b->particle_b];
  vec3 d;
  float len, inv;

  o->b = (short)b->particle_b;
  o->a = (short)b->particle_a;
  d.x = (float)(pb->pos.x - pa->pos.x);
  d.y = (float)(pb->pos.y - pa->pos.y);
  d.z = (float)(pb->pos.z - pa->pos.z);
  len = (float)x87_sqrt((double)(float)(d.z * d.z + (d.x * d.x + d.y * d.y)));
  if (len <= 1.52587890625E-5) {
    d.x = 0.0f;
    d.y = 0.0f;
    d.z = 0.0f;
  } else {
    inv = (float)(1.0 / len);
    d.x = inv * d.x;
    d.y = (float)(inv * d.y);
    d.z = (float)(inv * d.z);
  }
  o->dir = d;
  o->bias = (float)((float)(len - b->rest_length) * b->stiffness);
  o->damping = b->damping;
  inv = (float)(1.0 / (pb->mass + pa->mass));
  o->share_a = (float)(inv * pb->mass);
  o->share_b = (float)(inv * pa->mass);
}

/* bondsetup, type 10: an item in the hand. The two lever arms, the error between the two points
 * (no longer than 0.125), and the share of each side, which goes into the bond. */
static void setup_bodyitem(Bond *b, BondOpt *o)
{
  const Bone *bone = &sr_humans[b->owner].bones[b->bone_a];
  const Item *it = &sr_items[b->target];
  const vec3 la = b->local_a, lb = b->local_b;
  vec3 ra, rb, p, e;
  float len, f, k, inv, m_bone, m_item;

  ra.x = (float)((float)(la.y * bone->rot[1].x + (float)(la.x * bone->rot[0].x)) + la.z * bone->rot[2].x);
  ra.y = (float)(bone->rot[2].y * la.z + (float)(bone->rot[1].y * la.y + (float)(bone->rot[0].y * la.x)));
  ra.z = (float)(la.z * bone->rot[2].z + (float)(la.y * bone->rot[1].z + (float)(la.x * bone->rot[0].z)));
  o->arm_a = ra;
  rb.x = (float)((float)((float)(lb.x * it->rot[0].x) + lb.y * it->rot[1].x) + lb.z * it->rot[2].x);
  rb.y = (float)(it->rot[2].y * lb.z + (float)(it->rot[1].y * lb.y + (float)(it->rot[0].y * lb.x)));
  rb.z = (float)(lb.z * it->rot[2].z + (float)(lb.y * it->rot[1].z + (float)(lb.x * it->rot[0].z)));
  o->arm_b = rb;
  p.x = (float)(it->pos.x + rb.x);
  p.y = (float)(rb.y + it->pos.y);
  p.z = (float)(rb.z + it->pos.z);
  e.x = (float)((float)(bone->pos.x + ra.x) - p.x);
  e.y = (float)((float)(ra.y + bone->pos.y) - p.y);
  e.z = (float)((float)(ra.z + bone->pos.z) - p.z);
  len = (float)x87_sqrt((double)(float)(e.y * e.y + e.x * e.x + e.z * e.z));
  if (0.125 < len) {
    f = (float)(0.125 / len);
    e.x = f * e.x;
    e.y = (float)(e.y * f);
    e.z = (float)(f * e.z);
  }
  k = b->stiffness;
  o->dir.x = k * e.x;
  o->dir.y = (float)(e.y * k);
  o->dir.z = (float)(k * e.z);
  m_bone = bone->mass;
  m_item = it->mass;
  inv = (float)(1.0 / (m_bone + m_item));
  b->weights[0] = inv * m_item;
  b->weights[1] = (float)(m_bone * inv);
}

/* bondsetup, type 8: a joint between two bones of a human. */
static void setup_bodybody(const Bond *b, BondOpt *o)
{
  const Bone *ba = &sr_humans[b->owner].bones[b->bone_a];
  const Bone *bb = &sr_humans[b->owner].bones[b->target];
  const vec3 la = b->local_a, lb = b->local_b;
  vec3 ra, rb, p, e;
  float k, inv;

  o->a = (short)b->owner;
  o->b = (short)b->bone_a;
  o->c = (short)b->target;
  ra.x = (float)(la.z * ba->rot[2].x + (float)(la.x * ba->rot[0].x + la.y * ba->rot[1].x));
  ra.y = (float)(ba->rot[2].y * la.z + (float)(ba->rot[1].y * la.y + (float)(ba->rot[0].y * la.x)));
  ra.z = (float)(la.z * ba->rot[2].z + (float)(la.y * ba->rot[1].z + (float)(la.x * ba->rot[0].z)));
  o->arm_a = ra;
  rb.x = (float)(lb.z * bb->rot[2].x + (float)(lb.x * bb->rot[0].x + lb.y * bb->rot[1].x));
  rb.y = (float)(bb->rot[2].y * lb.z + (float)(bb->rot[1].y * lb.y + (float)(bb->rot[0].y * lb.x)));
  rb.z = (float)(lb.z * bb->rot[2].z + (float)(lb.y * bb->rot[1].z + (float)(lb.x * bb->rot[0].z)));
  o->arm_b = rb;
  p.x = (float)(bb->pos.x + rb.x);
  p.y = (float)(rb.y + bb->pos.y);
  p.z = (float)(rb.z + bb->pos.z);
  e.x = (float)((float)(ba->pos.x + ra.x) - p.x);
  e.y = (float)((float)(ra.y + ba->pos.y) - p.y);
  e.z = (float)((float)(ra.z + ba->pos.z) - p.z);
  k = b->stiffness;
  o->dir.x = (float)(k * e.x);
  o->dir.y = (float)(k * e.y);
  o->dir.z = (float)(k * e.z);
  o->damping = b->damping;
  inv = (float)(1.0 / (ba->mass + bb->mass));
  o->share_a = inv * bb->mass;
  o->share_b = (float)(ba->mass * inv);
}

/* bondsetup, type 9: a muscle between two bones of a human. The direction is the turn that
 * brings bone b back to its target rotation (bone.target_rot), from the Euler angles of the
 * relative rotation, at most a quarter turn on each axis. A dead human has no target, and the
 * angles get a dead zone. The share of each side also has the largest inertia of bone b. */
static void setup_bodyrotbody(const Bond *b, BondOpt *o)
{
  Human *h = &sr_humans[b->owner];
  int ka = b->bone_a, kb = b->target;
  const Bone *ba = &h->bones[ka];
  Bone *bb = &h->bones[kb];
  float m[9];
  vec3 e, r[3];
  float k, e0, e1, lo, hi, inv, sa, sb;
  int row, col;

  sr_timer_start(bondsetuphuman);
  o->b = (short)ka;
  o->a = (short)b->owner;
  o->c = (short)kb;
  /* The rotation of bone b in the frame of bone a. */
  for (row = 0; row < 3; row++)
    for (col = 0; col < 3; col++)
      m[row * 3 + col] = (float)(bb->rot[col].z * ba->rot[row].z +
                                 (bb->rot[col].x * ba->rot[row].x + bb->rot[col].y * ba->rot[row].y));
  if (h->health < 1) {
    bb->target_rot[0].x = 1.0f;
    bb->target_rot[1].y = 1.0f;
    bb->target_rot[2].z = 1.0f;
    bb->target_rot[0].y = 0.0f;
    bb->target_rot[0].z = 0.0f;
    bb->target_rot[1].x = 0.0f;
    bb->target_rot[1].z = 0.0f;
    bb->target_rot[2].x = 0.0f;
    bb->target_rot[2].y = 0.0f;
  }
  multiply3x3matrices((vec3 *)m, (const vec3 *)m, bb->target_rot);
  k = b->damping;
  o->damping = k;
  /* For bones 3 to 8 of a living human, use the direction of the last tick. */
  if (kb < 3 || 8 < kb || h->health < 1) {
    geteulerangles2(&e, (const vec3 *)m);
    e0 = e.x;
    e1 = e.y;
    if (h->health < 1) {
      lo = 0.0f;
      hi = 0.0f;
      if (kb == 9) {
        lo = 0.0f;
        hi = 0.7853981852531433f;
      } else if (kb == 12 || kb == 6) {
        lo = -0.7853981852531433f;
        hi = 0.0f;
      } else if (kb == 3) {
        lo = 0.0f;
        hi = 0.7853981852531433f;
      }
      if (lo <= e.x) {
        if (e.x <= hi)
          e.x = 0.0f;
        else
          e.x = (float)(e.x - hi);
      } else {
        e.x = (float)(e.x - lo);
      }
      lo = -0.7853981852531433f;
      hi = 0.7853981852531433f;
      if (kb == 5 || kb == 8) {
        lo = 0.0f;
        hi = 0.0f;
      }
      if (kb == 4 || kb == 7) {
        lo = -1.3744467496871948f;
        hi = 0.0f;
      }
      if (kb == 10 || kb == 13) {
        lo = 0.0f;
        hi = 1.3744467496871948f;
      }
      if (lo <= e.y) {
        if (e.y <= hi)
          e.y = 0.0f;
        else
          e.y = e.y - hi;
      } else {
        e.y = e.y - lo;
      }
      e.x = e.x * 0.125f;
      e.y = (float)(e.y * 0.125);
      e.z = (float)(e.z * 0.125);
      o->damping = (float)(b->damping * 0.5);
    }
    if (e.x < -0.7853981852531433)
      e.x = -0.7853981852531433f;
    if (0.7853981852531433 < e.x)
      e.x = 0.7853981852531433f;
    if (e.y < -0.7853981852531433)
      e.y = -0.7853981852531433f;
    if (0.7853981852531433 < e.y)
      e.y = 0.7853981852531433f;
    if (e.z < -0.7853981852531433)
      e.z = -0.7853981852531433f;
    if (0.7853981852531433 < e.z)
      e.z = 0.7853981852531433f;
    /* Each angle turns around one axis of bone a. The axis points into r, the matrix that
     * rotateorientation turns, so each turn uses the axis that the turn before it moved. */
    r[0].x = ba->rot[0].x;
    r[0].y = ba->rot[0].y;
    r[0].z = ba->rot[0].z;
    r[1].x = ba->rot[1].x;
    r[1].y = ba->rot[1].y;
    r[1].z = ba->rot[1].z;
    r[2].x = ba->rot[2].x;
    r[2].y = ba->rot[2].y;
    r[2].z = ba->rot[2].z;
    o->dir.x = (float)(r[2].x * e.x);
    o->dir.y = (float)(r[2].y * e.x);
    o->dir.z = (float)(e.x * r[2].z);
    rotateorientation(r, &r[2], e0);
    o->dir.x = (float)(o->dir.x + e.y * r[0].x);
    o->dir.y = (float)(e.y * r[0].y + o->dir.y);
    o->dir.z = (float)(e.y * r[0].z + o->dir.z);
    rotateorientation(r, &r[0], e1);
    o->dir.x = (float)(e.z * r[1].x + o->dir.x);
    o->dir.y = (float)(e.z * r[1].y + o->dir.y);
    o->dir.z = (float)(e.z * r[1].z + o->dir.z);
    k = b->stiffness;
  }
  o->dir.x = (float)(k * o->dir.x);
  o->dir.y = (float)(k * o->dir.y);
  o->dir.z = (float)(k * o->dir.z);
  inv = (float)(1.0 / (ba->mass + bb->mass));
  sa = inv * bb->mass;
  o->share_a = sa;
  sb = ba->mass * inv;
  o->share_b = sb;
  /* arm_a.x holds the gain for this bond type: the largest inertia of bone b. */
  o->arm_a.x = bb->inertia.x;
  if (bb->inertia.x < bb->inertia.y)
    o->arm_a.x = bb->inertia.y;
  if (o->arm_a.x < bb->inertia.z)
    o->arm_a.x = bb->inertia.z;
  o->share_a = o->arm_a.x * sa;
  o->share_b = (float)(sb * o->arm_a.x);
  sr_timer_stop(bondsetuphuman);
}


/* bondsetupcopy: the solver works on copies. For each human with a body: the velocity, the
 * angular velocity and the axes of its 15 bones, and a mask of the bones that take part. For
 * each particle: the velocity. For each bond: the mode. */
static void setup_copy(void)
{
  int h, k, i, n;

  sr_timer_start(bondsetupcopy);
  for (h = 0; h < 512; h++) {
    const Human *hu = &sr_humans[h];
    BOND_HUMAN_RESET(h);
    if (hu->active == 0 || hu->has_physics == 0)
      continue;
    BOND_HUMAN_PREPARE(h);
    for (k = 0; k < 15; k++) {
      const Bone *bone = &hu->bones[k];
      if (0.0 < bone->in_solver)
        BOND_HUMAN_MARK(h, k);
      BOND_HUMAN_VEL(h, k) = bone->vel;
      BOND_HUMAN_W(h, k) = bone->w;
      BOND_HUMAN_MOMENTUM(h, k) = bone->ang_momentum;
      BOND_HUMAN_COPY_AXIS(h, k, 0, bone->rot[0], bone->inv_inertia.x);
      BOND_HUMAN_COPY_AXIS(h, k, 1, bone->rot[1], bone->inv_inertia.y);
      BOND_HUMAN_COPY_AXIS(h, k, 2, bone->rot[2], bone->inv_inertia.z);
    }
  }
  n = sr_numofparticles;
  for (i = 0; i < n; i++)
    sr_bondparticles[i].vel = sr_particles[i].vel;
#if BOND_COPY_MODE
  n = sr_numofbonds;
  for (i = 0; i < n; i++)
    sr_bondopt[i].mode = (short)sr_bonds[i].mode;
#endif
  sr_timer_stop(bondsetupcopy);
}


/* bondloopcopy: the start of each solver pass. The pass number goes into a global (the wheels
 * use it), and each velocity change starts at 0: the particles, the items with physics, the
 * wheel bodies of the objects, and the bones of the humans in the solver. */
static void loop_clear(int pass)
{
  int i, k, n;

  sr_timer_start(bondloopcopy);
  g_bondpass = pass;
  n = sr_numofparticles;
  for (i = 0; i < n; i++) {
    sr_bondparticles[i].dv.x = 0.0f;
    sr_bondparticles[i].dv.y = 0.0f;
    sr_bondparticles[i].dv.z = 0.0f;
  }
  for (i = 0; i < 1024; i++) {
    Item *it = &sr_items[i];
    if (it->active == 0 || it->has_physics == 0)
      continue;
    it->dv.x = 0.0f;
    it->dv.y = 0.0f;
    it->dv.z = 0.0f;
    it->dw.x = 0.0f;
    it->dw.y = 0.0f;
    it->dw.z = 0.0f;
  }
  for (i = 0; i < 1024; i++) {
    Object *o = &sr_objects[i];
    if (o->active == 0)
      continue;
    for (k = 0; k < o->num_wheel_bodies; k++) {
      WheelBody *w = &o->wheel_bodies[k];
      w->dv.x = 0.0f;
      w->dv.y = 0.0f;
      w->dv.z = 0.0f;
      w->dw.x = 0.0f;
      w->dw.y = 0.0f;
      w->dw.z = 0.0f;
    }
  }
  for (i = 0; i < 512; i++) {
    if (!BOND_HUMAN_ACTIVE(i))
      continue;
    for (k = 0; k < 15; k++) {
      if (!BOND_HUMAN_CLEAR(i, k))
        continue;
      BOND_HUMAN_DV(i, k).x = 0.0f;
      BOND_HUMAN_DV(i, k).y = 0.0f;
      BOND_HUMAN_DV(i, k).z = 0.0f;
      BOND_HUMAN_DW(i, k).x = 0.0f;
      BOND_HUMAN_DW(i, k).y = 0.0f;
      BOND_HUMAN_DW(i, k).z = 0.0f;
    }
  }
  sr_timer_stop(bondloopcopy);
}

/* A vector longer than limit gets the length limit. */
static void clamp_length(vec3 *v, float len, float limit)
{
  float inv, z = 0.0f;
  if (limit < len) {
    if (len == 0.0) {
      v->x = 0.0f;
      v->y = 0.0f;
    } else {
      inv = (float)(1.0 / len);
      v->x = (float)(inv * v->x);
      v->y = (float)(inv * v->y);
      z = inv * v->z;
    }
    v->x = (float)(limit * v->x);
    v->y = (float)(limit * v->y);
    v->z = (float)(limit * z);
  }
}

/* bondhuman: a human who stands upright on the ground keeps the balance. Bone 0 gets three
 * turns: one back to the vertical (its length is at most a limit from the stance and the body
 * strength), one toward the direction that the human faces (human.axis), and less of the turn
 * it has now. The sum, with a limit, goes into BondBone.ang_momentum. */
static void loop_balance(void)
{
  const vec3 up = g_yaxis;
  int h;

  sr_timer_start(bondhuman);
  for (h = 0; h < 512; h++) {
    const Human *hu = &sr_humans[h];
    const vec3 *r1 = &hu->bones[0].rot[1], *r2 = &hu->bones[0].rot[2];
    vec3 a, face, t;
    float limit, len, s;

    if (hu->active == 0 || hu->on_ground == 0 || hu->health < 1 || hu->vehicle_id != -1)
      continue;
    if ((hu->input_buttons & 8) == 0 && !(0.8999999761581421 < r1->y))
      continue;
    /* The turn back to the vertical: rot[1] x up. */
    a.x = (float)(up.y * r1->z - up.z * r1->y) * 0.1875f;
    a.y = (float)((float)(r1->x * up.z - r1->z * up.x) * 0.1875);
    a.z = (float)((float)(r1->y * up.x - up.y * r1->x) * 0.1875);
    limit = (float)((float)((float)((2.0 - hu->stance) / 60.0) * hu->body_strength) * 0.5);
    len = (float)x87_sqrt((double)(float)(a.y * a.y + a.x * a.x + a.z * a.z));
    clamp_length(&a, len, limit);
    /* The turn toward the facing direction, around the vertical only. */
    face = g_zaxis;
    rotatevector(&face, &g_yaxis, hu->axis);
    t.x = 0.0f;
    t.y = (float)((float)(r2->x * face.z - r2->z * face.x) * 0.125);
    t.z = 0.0f;
    s = (float)(r1->y + 0.30000001192092896);
    if (BOND_BALANCE_NONNEGATIVE(s)) {
      if (1.0 < s)
        s = 1.0f;
    } else {
      s = 0.0f;
    }
    limit = (float)((s + s) / 60.0);
    len = (float)x87_sqrt((double)(float)(t.y * t.y + 0.0 + 0.0));
    clamp_length(&t, len, limit);
    a.x = (float)(t.x + a.x);
    a.y = (float)(a.y + t.y);
    a.z = (float)(t.z + a.z);
    /* Less of the turn that the bone has now. */
    a.x = (float)(a.x - BOND_HUMAN_W(h, 0).x * 0.5);
    a.y = (float)(a.y - BOND_HUMAN_W(h, 0).y * 0.5);
    a.z = (float)(a.z - BOND_HUMAN_W(h, 0).z * 0.5);
    len = (float)x87_sqrt((double)(float)(a.y * a.y + a.x * a.x + a.z * a.z));
    limit = (float)(hu->body_strength * 0.008888889104127884);
    if (limit < len) {
      s = (float)(limit / len);
      a.x = (float)(s * a.x);
      a.y = (float)(s * a.y);
      a.z = (float)(s * a.z);
    }
    a.x = a.x * 0.125f;
    a.y = (float)(a.y * 0.125);
    a.z = (float)(a.z * 0.125);
    BOND_HUMAN_MOMENTUM(h, 0).x = (float)(BOND_HUMAN_MOMENTUM(h, 0).x + a.x);
    BOND_HUMAN_MOMENTUM(h, 0).y = (float)(a.y + BOND_HUMAN_MOMENTUM(h, 0).y);
    BOND_HUMAN_MOMENTUM(h, 0).z = (float)(a.z + BOND_HUMAN_MOMENTUM(h, 0).z);
  }
  sr_timer_stop(bondhuman);
}

/* The contact bonds hold a point against a hit surface. The velocity change pushes the point
 * out along the normal (the stretch rest_length * stiffness, plus the damped speed against the
 * surface), and calculatefriction removes a part of the sideways speed. A contact only pushes:
 * when the change points into the surface, nothing happens. */

/* Type 1: a particle against a fixed surface that moves with surface_vel. */
static void contact_level(const Bond *b)
{
  BondParticle *pa = &sr_bondparticles[b->particle_a];
  const vec3 n = b->normal;
  float k = (float)(b->rest_length * b->stiffness), d = b->damping, nd = -b->damping;
  vec3 v;

  v.x = (float)((float)(nd * pa->vel.x + (float)(k * n.x)) + d * b->surface_vel.x);
  v.y = (float)(b->surface_vel.y * d + (float)(pa->vel.y * nd + (float)(k * n.y)));
  v.z = (float)(d * b->surface_vel.z + (float)(nd * pa->vel.z + (float)(k * n.z)));
  if (!(0.0 < (float)(n.z * v.z + (n.x * v.x + v.y * n.y))))
    return;
  calculatefriction(&v, &b->normal, b->friction);
  pa->dv.x = (float)(pa->dv.x + v.x);
  pa->dv.y = (float)(pa->dv.y + v.y);
  pa->dv.z = (float)(pa->dv.z + v.z);
}

/* Type 2: particle a against particle b. Each side gets its share of the change from the
 * masses. */
static void contact_particle(const Bond *b)
{
  BondParticle *pa = &sr_bondparticles[b->particle_a], *pb = &sr_bondparticles[b->particle_b];
  const Particle *ma = &sr_particles[b->particle_a], *mb = &sr_particles[b->particle_b];
  const vec3 n = b->normal;
  float k = (float)(b->rest_length * b->stiffness), d = b->damping, nd = -b->damping;
  float len, inv, f;
  vec3 v;

  v.x = (float)(d * pb->vel.x + (float)(nd * pa->vel.x + (float)(k * n.x)));
  v.y = (float)(pb->vel.y * d + (float)(pa->vel.y * nd + (float)(k * n.y)));
  v.z = (float)(d * pb->vel.z + (float)(nd * pa->vel.z + (float)(k * n.z)));
  len = (float)x87_sqrt((double)(float)(v.z * v.z + (v.x * v.x + v.y * v.y)));
  if (len <= 1.52587890625E-5) {
    v.z = 0.0f;
    v.x = 0.0f;
    v.y = 0.0f;
  } else {
    inv = (float)(1.0 / len);
    v.x = inv * v.x;
    v.y = (float)(inv * v.y);
    v.z = inv * v.z;
  }
  len = (float)(len / (mb->mass + ma->mass));
  if (!(0.0 < (float)(n.z * v.z + (v.x * n.x + v.y * n.y))))
    return;
  calculatefriction(&v, &b->normal, b->friction);
  f = (float)(len * mb->mass);
  pa->dv.x = (float)(pa->dv.x + f * v.x);
  pa->dv.y = (float)(pa->dv.y + v.y * f);
  pa->dv.z = (float)(pa->dv.z + v.z * f);
  f = (float)(-len * ma->mass);
  pb->dv.x = (float)(pb->dv.x + f * v.x);
  pb->dv.y = (float)(pb->dv.y + f * v.y);
  pb->dv.z = (float)(f * v.z + pb->dv.z);
}

/* The change of the 3 particles of a hit triangle, each in the ratio of its weight. */
static void push_triangle(const Bond *b, float g, const vec3 *v)
{
  BondParticle *pb = &sr_bondparticles[b->particle_b];
  BondParticle *pc = &sr_bondparticles[b->particle_c];
  BondParticle *pd = &sr_bondparticles[b->particle_d];
  float f;

  f = (float)(b->weights[0] * g);
  pb->dv.x = (float)(pb->dv.x + f * v->x);
  pb->dv.y = (float)(v->y * f + pb->dv.y);
  pb->dv.z = (float)(f * v->z + pb->dv.z);
  f = (float)(b->weights[1] * g);
  pc->dv.x = (float)(pc->dv.x + f * v->x);
  pc->dv.y = (float)(v->y * f + pc->dv.y);
  pc->dv.z = (float)(f * v->z + pc->dv.z);
  f = (float)(g * b->weights[2]);
  pd->dv.x = (float)(pd->dv.x + f * v->x);
  pd->dv.y = (float)(pd->dv.y + f * v->y);
  pd->dv.z = (float)(f * v->z + pd->dv.z);
}

/* The mass of the hit point of a triangle: the masses of its 3 particles with the weights. */
static float triangle_mass(const Bond *b)
{
  return (float)(sr_particles[b->particle_d].mass * b->weights[2] +
                 (sr_particles[b->particle_c].mass * b->weights[1] + sr_particles[b->particle_b].mass * b->weights[0]));
}

/* Type 3: particle a against a triangle of another object (particles b, c and d). */
static void contact_triangle(const Bond *b)
{
  BondParticle *pa = &sr_bondparticles[b->particle_a];
  const BondParticle *pb = &sr_bondparticles[b->particle_b];
  const BondParticle *pc = &sr_bondparticles[b->particle_c];
  const BondParticle *pd = &sr_bondparticles[b->particle_d];
  const vec3 n = b->normal;
  float k = (float)(b->rest_length * b->stiffness), nd = -b->damping;
  float wb = (float)(b->weights[0] * b->damping), wc = (float)(b->weights[1] * b->damping);
  float wd = (float)(b->weights[2] * b->damping);
  float inv, ma, mt, s, f;
  double len;
  vec3 v;

  v.x = (float)((float)(wc * pc->vel.x + (float)((float)((float)(k * n.x) + nd * pa->vel.x) + wb * pb->vel.x)) +
                wd * pd->vel.x);
  v.y = (float)(pd->vel.y * wd + (float)(pc->vel.y * wc + (float)(pb->vel.y * wb + (float)(pa->vel.y * nd + (float)(k * n.y)))));
  v.z = (float)(wd * pd->vel.z + (float)(wc * pc->vel.z + (float)(wb * pb->vel.z + (float)(nd * pa->vel.z + (float)(k * n.z)))));
  /* No test for a short vector here, unlike types 2 and 4. */
  len = x87_sqrt((double)(float)(v.y * v.y + v.x * v.x + v.z * v.z));
  inv = (float)(1.0 / (float)len);
  v.x = inv * v.x;
  v.y = (float)(v.y * inv);
  v.z = (float)(inv * v.z);
  ma = sr_particles[b->particle_a].mass;
  mt = triangle_mass(b);
  s = (float)((float)len / (mt + ma));
  if (!(0.0 < (float)(n.z * v.z + (n.x * v.x + v.y * n.y))))
    return;
  calculatefriction(&v, &b->normal, b->friction);
  f = s * mt;
  pa->dv.x = (float)(pa->dv.x + f * v.x);
  pa->dv.y = (float)(v.y * f + pa->dv.y);
  pa->dv.z = (float)(pa->dv.z + v.z * f);
  push_triangle(b, -s * ma, &v);
}

/* Type 4: a bone of a human against a triangle of an object. */
static void contact_bone_triangle(const Bond *b)
{
  const BondParticle *pb = &sr_bondparticles[b->particle_b];
  const BondParticle *pc = &sr_bondparticles[b->particle_c];
  const BondParticle *pd = &sr_bondparticles[b->particle_d];
  const vec3 n = b->normal;
  float k = (float)(b->rest_length * b->stiffness), nd = -b->damping;
  float wb = (float)(b->weights[0] * b->damping), wc = (float)(b->weights[1] * b->damping);
  float wd = (float)(b->weights[2] * b->damping);
  float len, inv, mb, mt, f;
  vec3 v;

  v.x = (float)((float)((float)(wb * pb->vel.x + (float)(k * n.x + nd * BOND_HUMAN_VEL(b->owner, b->bone_a).x)) + wc * pc->vel.x) +
                wd * pd->vel.x);
  v.y = (float)(pd->vel.y * wd + (float)(pc->vel.y * wc + (float)(pb->vel.y * wb + (float)(BOND_HUMAN_VEL(b->owner, b->bone_a).y * nd + (float)(n.y * k)))));
  v.z = (float)(wd * pd->vel.z + (float)(wc * pc->vel.z + (float)(wb * pb->vel.z + (float)(nd * BOND_HUMAN_VEL(b->owner, b->bone_a).z + (float)(k * n.z)))));
  len = (float)x87_sqrt((double)(float)(v.y * v.y + v.x * v.x + v.z * v.z));
  if (len <= 1.52587890625E-5) {
    v.z = 0.0f;
    v.x = 0.0f;
    v.y = 0.0f;
  } else {
    inv = (float)(1.0 / len);
    v.x = inv * v.x;
    v.y = (float)(v.y * inv);
    v.z = inv * v.z;
  }
  mb = sr_humans[b->owner].bones[b->bone_a].mass;
  mt = triangle_mass(b);
  len = (float)(len / (mt + mb));
  if (!(0.0 < (float)(n.z * v.z + (n.x * v.x + v.y * n.y))))
    return;
  calculatefriction(&v, &b->normal, b->friction);
  f = len * mt;
  BOND_HUMAN_DV(b->owner, b->bone_a).x = (float)(BOND_HUMAN_DV(b->owner, b->bone_a).x + f * v.x);
  BOND_HUMAN_DV(b->owner, b->bone_a).y = (float)(v.y * f + BOND_HUMAN_DV(b->owner, b->bone_a).y);
  BOND_HUMAN_DV(b->owner, b->bone_a).z = (float)(BOND_HUMAN_DV(b->owner, b->bone_a).z + v.z * f);
  push_triangle(b, -len * mb, &v);
}

/* Type 5: a wheel of an object on the ground. The ground moves against the wheel at
 * spin * radius along roll_dir. The push goes into the hub, the friction into Wheel.slip,
 * and the part of the push along roll_dir slows the spin. */
static void contact_wheel(const Bond *b)
{
  Wheel *w = &sr_objects[b->owner].wheels[b->bone_a];
  const vec3 n = b->normal, rd = b->roll_dir;
  float k = (float)(b->rest_length * b->stiffness), nd = -b->damping;
  float roll = (float)(w->spin * w->radius * b->damping);
  float proj;
  vec3 v;

  v.x = (float)((float)(k * n.x + nd * w->vel.x) + roll * rd.x);
  v.y = (float)(roll * rd.y + (float)(w->vel.y * nd + (float)(n.y * k)));
  v.z = (float)(roll * rd.z + (float)(nd * w->vel.z + (float)(k * n.z)));
  if (!(0.0 < (float)(n.z * v.z + (n.x * v.x + v.y * n.y))))
    return;
  w->slip = (float)(calculatefriction(&v, &b->normal, b->friction) + (double)w->slip);
  w->dv.x = (float)(v.x + w->dv.x);
  w->dv.y = (float)(v.y + w->dv.y);
  w->dv.z = (float)(v.z + w->dv.z);
  proj = (float)((float)(v.z * rd.z + (rd.x * v.x + rd.y * v.y)) * w->radius);
  w->dspin = (float)(w->dspin - proj);
}

/* Type 7: a bone of a human against a fixed surface. The hit point is arm from the bone, so
 * its velocity has the turn of the bone in it, and the push also turns the bone. */
static void contact_bone_level(const Bond *b)
{
  const vec3 n = b->normal, r = b->arm;
  float k = (float)(b->rest_length * b->stiffness), nd = -b->damping;
  vec3 p, v, t;

  p.x = (float)((float)(BOND_HUMAN_W(b->owner, b->bone_a).z * r.y - r.z * BOND_HUMAN_W(b->owner, b->bone_a).y) + BOND_HUMAN_VEL(b->owner, b->bone_a).x);
  p.y = (float)(BOND_HUMAN_VEL(b->owner, b->bone_a).y + (float)(r.z * BOND_HUMAN_W(b->owner, b->bone_a).x - r.x * BOND_HUMAN_W(b->owner, b->bone_a).z));
  p.z = (float)(BOND_HUMAN_VEL(b->owner, b->bone_a).z + (float)(r.x * BOND_HUMAN_W(b->owner, b->bone_a).y - BOND_HUMAN_W(b->owner, b->bone_a).x * r.y));
  v.x = (float)(k * n.x + nd * p.x);
  v.y = (float)(nd * p.y + (float)(k * n.y));
  v.z = (float)(nd * p.z + (float)(k * n.z));
  if (!(0.0 < (float)(n.z * v.z + (n.x * v.x + v.y * n.y))))
    return;
  calculatefriction(&v, &b->normal, b->friction);
  BOND_HUMAN_DV(b->owner, b->bone_a).x = (float)(v.x + BOND_HUMAN_DV(b->owner, b->bone_a).x);
  BOND_HUMAN_DV(b->owner, b->bone_a).y = (float)(v.y + BOND_HUMAN_DV(b->owner, b->bone_a).y);
  BOND_HUMAN_DV(b->owner, b->bone_a).z = (float)(v.z + BOND_HUMAN_DV(b->owner, b->bone_a).z);
  t.x = (float)(r.z * v.y - v.z * r.y);
  t.y = (float)(v.z * r.x - r.z * v.x);
  t.z = (float)(r.y * v.x - v.y * r.x);
  BOND_HUMAN_DW(b->owner, b->bone_a).x = (float)(t.x + BOND_HUMAN_DW(b->owner, b->bone_a).x);
  BOND_HUMAN_DW(b->owner, b->bone_a).y = (float)(BOND_HUMAN_DW(b->owner, b->bone_a).y + t.y);
  BOND_HUMAN_DW(b->owner, b->bone_a).z = (float)(BOND_HUMAN_DW(b->owner, b->bone_a).z + t.z);
}

/* Type 9: the turn that a muscle holds between bones b and c of human a. The change of the
 * angular velocity removes the turn error (dir) and a part of the difference of the two turns.
 * Each bone gets its share. */
static void bond_bodyrotbody(const BondOpt *o)
{
  float d = o->damping, nd = -o->damping, ns;
  vec3 v;

  v.x = (float)((float)(nd * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->b).x + o->dir.x) + d * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->c).x);
  v.y = (float)(BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->c).y * d + (float)(BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->b).y * nd + o->dir.y));
  v.z = (float)(d * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->c).z + (float)(nd * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->b).z + o->dir.z));
  BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).x = (float)(BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).x + o->share_a * v.x);
  BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).y = (float)(v.y * o->share_a + BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).y);
  BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).z = (float)(BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).z + v.z * o->share_a);
  ns = -o->share_b;
  BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).x = (float)(BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).x + ns * v.x);
  BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).y = (float)(v.y * ns + BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).y);
  BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).z = (float)(v.z * ns + BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).z);
}

/* Type 10: a bone of a human holds an item at a point. The velocity of the hold point on the
 * bone (arm_a) and on the item (arm_b) must be equal. The change goes to the bone with the share
 * weights[0] and to the item with weights[1], and each also turns around its point. */
static void bond_bodyitem(const Bond *b, const BondOpt *o)
{
  Item *it = &sr_items[b->target];
  const vec3 ra = o->arm_a, rb = o->arm_b;
  float d = b->damping, nd = -b->damping;
  float sb = b->weights[0], si = b->weights[1], ns = -sb;
  vec3 v, t;

  v.x = (float)(nd * (float)(it->vel.x + (float)(rb.y * it->angvel.z - rb.z * it->angvel.y)) +
                (float)(d * (float)((float)(ra.y * BOND_HUMAN_W(b->owner, b->bone_a).z - ra.z * BOND_HUMAN_W(b->owner, b->bone_a).y) + BOND_HUMAN_VEL(b->owner, b->bone_a).x) + o->dir.x));
  v.y = (float)(nd * (float)(it->vel.y + (float)(it->angvel.x * rb.z - rb.x * it->angvel.z)) +
                (float)(d * (float)(BOND_HUMAN_VEL(b->owner, b->bone_a).y + (float)(BOND_HUMAN_W(b->owner, b->bone_a).x * ra.z - ra.x * BOND_HUMAN_W(b->owner, b->bone_a).z)) + o->dir.y));
  v.z = (float)(nd * (float)(it->vel.z + (float)(it->angvel.y * rb.x - rb.y * it->angvel.x)) +
                (float)(d * (float)(BOND_HUMAN_VEL(b->owner, b->bone_a).z + (float)(ra.x * BOND_HUMAN_W(b->owner, b->bone_a).y - BOND_HUMAN_W(b->owner, b->bone_a).x * ra.y)) + o->dir.z));
  BOND_HUMAN_DV(b->owner, b->bone_a).x = (float)(BOND_HUMAN_DV(b->owner, b->bone_a).x + v.x * ns);
  BOND_HUMAN_DV(b->owner, b->bone_a).y = (float)(BOND_HUMAN_DV(b->owner, b->bone_a).y + v.y * ns);
  BOND_HUMAN_DV(b->owner, b->bone_a).z = (float)(BOND_HUMAN_DV(b->owner, b->bone_a).z + v.z * ns);
  BOND_HUMAN_DW(b->owner, b->bone_a).x = (float)(ns * (float)(v.y * ra.z - ra.y * v.z) + BOND_HUMAN_DW(b->owner, b->bone_a).x);
  BOND_HUMAN_DW(b->owner, b->bone_a).y = (float)(ns * (float)(ra.x * v.z - v.x * ra.z) + BOND_HUMAN_DW(b->owner, b->bone_a).y);
  BOND_HUMAN_DW(b->owner, b->bone_a).z = (float)(ns * (float)(v.x * ra.y - v.y * ra.x) + BOND_HUMAN_DW(b->owner, b->bone_a).z);
  it->dv.x = (float)(it->dv.x + si * v.x);
  it->dv.y = (float)(v.y * si + it->dv.y);
  it->dv.z = (float)(v.z * si + it->dv.z);
  t.x = (float)(v.y * rb.z - rb.y * v.z);
  t.y = (float)(rb.x * v.z - v.x * rb.z);
  t.z = (float)(rb.y * v.x - rb.x * v.y);
  it->dw.x = (float)(t.x * si + it->dw.x);
  it->dw.y = (float)(t.y * si + it->dw.y);
  it->dw.z = (float)(si * t.z + it->dw.z);
}

/* Type 11: a human holds an item in the hand, and the item turns with the bone. As type 9,
 * between the bone and the item. arm_a.x holds the gain: the smallest inertia of the item. */
static void bond_itemrot(const Bond *b, const BondOpt *o)
{
  Item *it = &sr_items[b->target];
  float d = o->damping, nd = -o->damping, g = o->arm_a.x, ns;
  vec3 v;

  v.x = (float)(g * (float)((float)(nd * BOND_HUMAN_W(b->owner, b->bone_a).x + o->dir.x) + d * it->angvel.x));
  v.y = (float)(g * (float)(it->angvel.y * d + (float)(BOND_HUMAN_W(b->owner, b->bone_a).y * nd + o->dir.y)));
  v.z = (float)(g * (float)(d * it->angvel.z + (float)(nd * BOND_HUMAN_W(b->owner, b->bone_a).z + o->dir.z)));
  BOND_HUMAN_DW(b->owner, b->bone_a).x = (float)(BOND_HUMAN_DW(b->owner, b->bone_a).x + v.x * o->share_a);
  BOND_HUMAN_DW(b->owner, b->bone_a).y = (float)(BOND_HUMAN_DW(b->owner, b->bone_a).y + v.y * o->share_a);
  BOND_HUMAN_DW(b->owner, b->bone_a).z = (float)(BOND_HUMAN_DW(b->owner, b->bone_a).z + v.z * o->share_a);
  ns = -o->share_b;
  it->dw.x = (float)(it->dw.x + ns * v.x);
  it->dw.y = (float)(v.y * ns + it->dw.y);
  it->dw.z = (float)(ns * v.z + it->dw.z);
}

/* The unit vector of v, or zero when v is very short. Returns the length. */
static float unit_or_zero(vec3 *v)
{
  float len = (float)x87_sqrt((double)(float)(v->z * v->z + (v->x * v->x + v->y * v->y)));
  float inv;

  if (len <= 1.52587890625E-5) {
    v->z = 0.0f;
    v->x = 0.0f;
    v->y = 0.0f;
  } else {
    inv = (float)(1.0 / len);
    v->x = inv * v->x;
    v->y = (float)(inv * v->y);
    v->z = inv * v->z;
  }
  return len;
}

/* Type 13: a bone of one human against a bone of another human. Each side gets its share of
 * the change from the bone masses. */
static void contact_bone_bone(const Bond *b)
{
  float mx = sr_humans[b->owner].bones[b->bone_a].mass, my = sr_humans[b->target].bones[b->bone_b].mass;
  const vec3 n = b->normal;
  float k = (float)(b->rest_length * b->stiffness), d = b->damping, nd = -b->damping;
  float s, f;
  vec3 v;

  v.x = (float)(d * BOND_HUMAN_VEL(b->target, b->bone_b).x + (float)(nd * BOND_HUMAN_VEL(b->owner, b->bone_a).x + k * n.x));
  v.y = (float)(BOND_HUMAN_VEL(b->target, b->bone_b).y * d + (float)(BOND_HUMAN_VEL(b->owner, b->bone_a).y * nd + (float)(k * n.y)));
  v.z = (float)(d * BOND_HUMAN_VEL(b->target, b->bone_b).z + (float)(nd * BOND_HUMAN_VEL(b->owner, b->bone_a).z + (float)(k * n.z)));
  s = (float)(unit_or_zero(&v) / (mx + my));
  if (!(0.0 < (float)(n.z * v.z + (v.x * n.x + v.y * n.y))))
    return;
  calculatefriction(&v, &b->normal, b->friction);
  f = (float)(s * my);
  BOND_HUMAN_DV(b->owner, b->bone_a).x = (float)(BOND_HUMAN_DV(b->owner, b->bone_a).x + f * v.x);
  BOND_HUMAN_DV(b->owner, b->bone_a).y = (float)(BOND_HUMAN_DV(b->owner, b->bone_a).y + v.y * f);
  BOND_HUMAN_DV(b->owner, b->bone_a).z = (float)(BOND_HUMAN_DV(b->owner, b->bone_a).z + v.z * f);
  f = (float)(-s * mx);
  BOND_HUMAN_DV(b->target, b->bone_b).x = (float)(BOND_HUMAN_DV(b->target, b->bone_b).x + f * v.x);
  BOND_HUMAN_DV(b->target, b->bone_b).y = (float)(BOND_HUMAN_DV(b->target, b->bone_b).y + f * v.y);
  BOND_HUMAN_DV(b->target, b->bone_b).z = (float)(f * v.z + BOND_HUMAN_DV(b->target, b->bone_b).z);
}

/* Type 14: a bone of a human against an item. The hit point is local_a on the bone and
 * local_b on the item, so the push also turns both. */
static void contact_bone_item(const Bond *b)
{
  Item *it = &sr_items[b->target];
  float mx = sr_humans[b->owner].bones[b->bone_a].mass;
  const vec3 n = b->normal, ra = b->local_a, rb = b->local_b;
  float k = (float)(b->rest_length * b->stiffness), d = b->damping, nd = -b->damping;
  float s, f;
  vec3 p, v, a, t;

  p.x = (float)(it->vel.x + (float)(rb.y * it->angvel.z - rb.z * it->angvel.y));
  p.y = (float)(it->vel.y + (float)(rb.z * it->angvel.x - rb.x * it->angvel.z));
  p.z = (float)(it->vel.z + (float)(rb.x * it->angvel.y - it->angvel.x * rb.y));
  v.x = (float)(d * p.x + (float)(nd * (float)(BOND_HUMAN_VEL(b->owner, b->bone_a).x + (float)(ra.y * BOND_HUMAN_W(b->owner, b->bone_a).z - ra.z * BOND_HUMAN_W(b->owner, b->bone_a).y)) + k * n.x));
  v.y = (float)(d * p.y +
                (float)(nd * (float)(BOND_HUMAN_VEL(b->owner, b->bone_a).y + (float)(BOND_HUMAN_W(b->owner, b->bone_a).x * ra.z - ra.x * BOND_HUMAN_W(b->owner, b->bone_a).z)) + (float)(k * n.y)));
  v.z = (float)(d * p.z +
                (float)(nd * (float)(BOND_HUMAN_VEL(b->owner, b->bone_a).z + (float)(BOND_HUMAN_W(b->owner, b->bone_a).y * ra.x - ra.y * BOND_HUMAN_W(b->owner, b->bone_a).x)) + (float)(k * n.z)));
  s = (float)(unit_or_zero(&v) / (mx + it->mass));
  if (!(0.0 < (float)(n.z * v.z + (v.x * n.x + v.y * n.y))))
    return;
  calculatefriction(&v, &b->normal, b->friction);
  f = (float)(it->mass * s);
  a.x = f * v.x;
  a.y = (float)(f * v.y);
  a.z = (float)(f * v.z);
  BOND_HUMAN_DV(b->owner, b->bone_a).x = (float)(a.x + BOND_HUMAN_DV(b->owner, b->bone_a).x);
  BOND_HUMAN_DV(b->owner, b->bone_a).y = (float)(a.y + BOND_HUMAN_DV(b->owner, b->bone_a).y);
  BOND_HUMAN_DV(b->owner, b->bone_a).z = (float)(a.z + BOND_HUMAN_DV(b->owner, b->bone_a).z);
  BOND_HUMAN_DW(b->owner, b->bone_a).x = (float)((float)(ra.z * a.y - ra.y * a.z) + BOND_HUMAN_DW(b->owner, b->bone_a).x);
  BOND_HUMAN_DW(b->owner, b->bone_a).y = (float)(BOND_HUMAN_DW(b->owner, b->bone_a).y + (float)(ra.x * a.z - a.x * ra.z));
  BOND_HUMAN_DW(b->owner, b->bone_a).z = (float)(BOND_HUMAN_DW(b->owner, b->bone_a).z + (float)(ra.y * a.x - ra.x * a.y));
  f = (float)(-s * mx);
  a.x = f * v.x;
  a.y = (float)(f * v.y);
  a.z = (float)(f * v.z);
  it->dv.x = (float)(a.x + it->dv.x);
  it->dv.y = (float)(a.y + it->dv.y);
  it->dv.z = (float)(a.z + it->dv.z);
  t.x = (float)(rb.z * a.y - rb.y * a.z);
  t.y = (float)(rb.x * a.z - a.x * rb.z);
  t.z = (float)(rb.y * a.x - rb.x * a.y);
  it->dw.x = (float)(t.x + it->dw.x);
  it->dw.y = (float)(it->dw.y + t.y);
  it->dw.z = (float)(it->dw.z + t.z);
}

/* Type 15: a wheel body of an object against a fixed surface, at the point local_a. */
static void contact_wheelbody(const Bond *b)
{
  WheelBody *wb = &sr_objects[b->owner].wheel_bodies[b->bone_a];
  const vec3 n = b->normal, r = b->local_a;
  float k = (float)(b->rest_length * b->stiffness), nd = -b->damping;
  vec3 p, v, t;

  p.x = (float)(wb->vel.x + (float)(r.y * wb->w.z - r.z * wb->w.y));
  p.y = (float)(wb->vel.y + (float)(r.z * wb->w.x - r.x * wb->w.z));
  p.z = (float)(wb->vel.z + (float)(r.x * wb->w.y - r.y * wb->w.x));
  v.x = (float)(nd * p.x + (float)(k * n.x));
  v.y = (float)(nd * p.y + (float)(k * n.y));
  v.z = (float)(nd * p.z + (float)(k * n.z));
  if (!(0.0 < (float)(n.z * v.z + (n.x * v.x + v.y * n.y))))
    return;
  calculatefriction(&v, &b->normal, b->friction);
  wb->dv.x = (float)(v.x + wb->dv.x);
  wb->dv.y = (float)(v.y + wb->dv.y);
  wb->dv.z = (float)(v.z + wb->dv.z);
  t.x = (float)(r.z * v.y - r.y * v.z);
  t.y = (float)(v.z * r.x - r.z * v.x);
  t.z = (float)(r.y * v.x - v.y * r.x);
  wb->dw.x = (float)(wb->dw.x + t.x);
  wb->dw.y = (float)(wb->dw.y + t.y);
  wb->dw.z = (float)(wb->dw.z + t.z);
}

#if BOND_BONE_SPRING
enum { BOND_TYPE_BONE_SPRING = 16 };

/* Type 16: a spring between bones of two humans. */
static void bond_bone_spring(const Bond *b, const BondOpt *o)
{
  const Bone *a = &sr_humans[b->owner].bones[b->bone_a];
  const Bone *c = &sr_humans[b->target].bones[b->bone_b];
  vec3 dir, relative;
  float length, impulse, inv, share_a, share_b, change;

  dir.x = (float)(c->pos.x - a->pos.x);
  dir.y = (float)(c->pos.y - a->pos.y);
  dir.z = (float)(c->pos.z - a->pos.z);
  length = unit_or_zero(&dir);
  impulse = (float)((float)(length - b->rest_length) * b->stiffness);
  relative.x = (float)(BOND_HUMAN_VEL(b->target, b->bone_b).x - BOND_HUMAN_VEL(b->owner, b->bone_a).x);
  relative.y = (float)(BOND_HUMAN_VEL(b->target, b->bone_b).y - BOND_HUMAN_VEL(b->owner, b->bone_a).y);
  relative.z = (float)(BOND_HUMAN_VEL(b->target, b->bone_b).z - BOND_HUMAN_VEL(b->owner, b->bone_a).z);
  impulse = (float)(dotproduct(&relative, &dir) * o->damping + impulse);
  inv = (float)(1.0 / (a->mass + c->mass));
  share_a = (float)(c->mass * inv);
  share_b = (float)(a->mass * inv);
  change = (float)(impulse * share_a);
  BOND_HUMAN_DV(b->owner, b->bone_a).x = (float)(change * dir.x + BOND_HUMAN_DV(b->owner, b->bone_a).x);
  BOND_HUMAN_DV(b->owner, b->bone_a).y = (float)(change * dir.y + BOND_HUMAN_DV(b->owner, b->bone_a).y);
  BOND_HUMAN_DV(b->owner, b->bone_a).z = (float)(change * dir.z + BOND_HUMAN_DV(b->owner, b->bone_a).z);
  change = (float)(-impulse * share_b);
  BOND_HUMAN_DV(b->target, b->bone_b).x = (float)(change * dir.x + BOND_HUMAN_DV(b->target, b->bone_b).x);
  BOND_HUMAN_DV(b->target, b->bone_b).y = (float)(change * dir.y + BOND_HUMAN_DV(b->target, b->bone_b).y);
  BOND_HUMAN_DV(b->target, b->bone_b).z = (float)(change * dir.z + BOND_HUMAN_DV(b->target, b->bone_b).z);
}
#endif

/* The end of each pass: each particle takes its velocity change. */
static void apply_particles(void)
{
  BondParticle *p = &sr_bondparticles[0];
  int i, n = sr_numofparticles;

  for (i = 0; i < n; i++, p++) {
    p->vel.x = (float)(p->dv.x + p->vel.x);
    p->vel.y = (float)(p->dv.y + p->vel.y);
    p->vel.z = (float)(p->dv.z + p->vel.z);
  }
}

/* bondtest: each bone in the mask of an active human takes its velocity changes. The angular
 * velocity w comes from the angular momentum: its part along each axis of the bone, times the
 * inverse inertia k of that axis. */
static void apply_bones(void)
{
  float f;
  int h, i;

  for (h = 0; h < 512; h++) {
    if (!BOND_HUMAN_ACTIVE(h))
      continue;
    for (i = 0; i < 15; i++) {
      if (!BOND_HUMAN_SOLVES(h, i))
        continue;
      BOND_HUMAN_VEL(h, i).x = (float)(BOND_HUMAN_DV(h, i).x + BOND_HUMAN_VEL(h, i).x);
      BOND_HUMAN_VEL(h, i).y = (float)(BOND_HUMAN_DV(h, i).y + BOND_HUMAN_VEL(h, i).y);
      BOND_HUMAN_VEL(h, i).z = (float)(BOND_HUMAN_DV(h, i).z + BOND_HUMAN_VEL(h, i).z);
      BOND_HUMAN_MOMENTUM(h, i).x = (float)(BOND_HUMAN_DW(h, i).x + BOND_HUMAN_MOMENTUM(h, i).x);
      BOND_HUMAN_MOMENTUM(h, i).y = (float)(BOND_HUMAN_DW(h, i).y + BOND_HUMAN_MOMENTUM(h, i).y);
      BOND_HUMAN_MOMENTUM(h, i).z = (float)(BOND_HUMAN_DW(h, i).z + BOND_HUMAN_MOMENTUM(h, i).z);
      f = (float)((float)(BOND_HUMAN_MOMENTUM(h, i).z * BOND_HUMAN_AXIS(h, i, 0).z +
                          (BOND_HUMAN_MOMENTUM(h, i).y * BOND_HUMAN_AXIS(h, i, 0).y + BOND_HUMAN_MOMENTUM(h, i).x * BOND_HUMAN_AXIS(h, i, 0).x)) * BOND_HUMAN_INERTIA(h, i, 0));
      BOND_HUMAN_W(h, i).x = f * BOND_HUMAN_AXIS(h, i, 0).x;
      BOND_HUMAN_W(h, i).y = (float)(f * BOND_HUMAN_AXIS(h, i, 0).y);
      BOND_HUMAN_W(h, i).z = (float)(f * BOND_HUMAN_AXIS(h, i, 0).z);
      f = (float)((float)(BOND_HUMAN_MOMENTUM(h, i).z * BOND_HUMAN_AXIS(h, i, 1).z +
                          (BOND_HUMAN_MOMENTUM(h, i).y * BOND_HUMAN_AXIS(h, i, 1).y + BOND_HUMAN_MOMENTUM(h, i).x * BOND_HUMAN_AXIS(h, i, 1).x)) * BOND_HUMAN_INERTIA(h, i, 1));
      BOND_HUMAN_W(h, i).x = (float)(BOND_HUMAN_W(h, i).x + f * BOND_HUMAN_AXIS(h, i, 1).x);
      BOND_HUMAN_W(h, i).y = (float)(f * BOND_HUMAN_AXIS(h, i, 1).y + BOND_HUMAN_W(h, i).y);
      BOND_HUMAN_W(h, i).z = (float)(f * BOND_HUMAN_AXIS(h, i, 1).z + BOND_HUMAN_W(h, i).z);
      f = (float)((float)(BOND_HUMAN_MOMENTUM(h, i).z * BOND_HUMAN_AXIS(h, i, 2).z +
                          (BOND_HUMAN_MOMENTUM(h, i).y * BOND_HUMAN_AXIS(h, i, 2).y + BOND_HUMAN_MOMENTUM(h, i).x * BOND_HUMAN_AXIS(h, i, 2).x)) * BOND_HUMAN_INERTIA(h, i, 2));
      BOND_HUMAN_W(h, i).x = (float)(f * BOND_HUMAN_AXIS(h, i, 2).x + BOND_HUMAN_W(h, i).x);
      BOND_HUMAN_W(h, i).y = (float)(f * BOND_HUMAN_AXIS(h, i, 2).y + BOND_HUMAN_W(h, i).y);
      BOND_HUMAN_W(h, i).z = (float)(f * BOND_HUMAN_AXIS(h, i, 2).z + BOND_HUMAN_W(h, i).z);
    }
  }
}

/* Each active item with physics takes its velocity changes. angvel comes from ang_momentum:
 * its part along each row of rot, times inv_inertia. */
static void apply_items(void)
{
  Item *it;
  const vec3 *r;
  float f;

  for (it = &sr_items[0]; it < &sr_items[1024]; it++) {
    if (it->active == 0 || it->has_physics == 0)
      continue;
    r = it->rot;
    it->vel.x = (float)(it->dv.x + it->vel.x);
    it->vel.y = (float)(it->dv.y + it->vel.y);
    it->vel.z = (float)(it->dv.z + it->vel.z);
    it->ang_momentum.x = (float)(it->dw.x + it->ang_momentum.x);
    it->ang_momentum.y = (float)(it->dw.y + it->ang_momentum.y);
    it->ang_momentum.z = (float)(it->dw.z + it->ang_momentum.z);
    it->angvel.x = 0.0f;
    it->angvel.y = 0.0f;
    it->angvel.z = 0.0f;
    f = (float)((float)(it->ang_momentum.z * r[0].z + (it->ang_momentum.y * r[0].y + it->ang_momentum.x * r[0].x)) *
                it->inv_inertia.x);
    it->angvel.x = (float)(it->angvel.x + f * r[0].x);
    it->angvel.y = (float)(f * r[0].y + it->angvel.y);
    it->angvel.z = (float)(f * r[0].z + it->angvel.z);
    f = (float)((float)(it->ang_momentum.z * r[1].z + (it->ang_momentum.y * r[1].y + r[1].x * it->ang_momentum.x)) *
                it->inv_inertia.y);
    it->angvel.x = (float)(it->angvel.x + f * r[1].x);
    it->angvel.y = (float)(f * r[1].y + it->angvel.y);
    it->angvel.z = (float)(f * r[1].z + it->angvel.z);
    f = (float)((float)(it->ang_momentum.z * r[2].z + (it->ang_momentum.y * r[2].y + it->ang_momentum.x * r[2].x)) *
                it->inv_inertia.z);
    it->angvel.x = (float)(f * r[2].x + it->angvel.x);
    it->angvel.y = (float)(f * r[2].y + it->angvel.y);
    it->angvel.z = (float)(f * r[2].z + it->angvel.z);
  }
}

/* Each wheel body of an active object takes its velocity changes, as the items. */
static void apply_wheelbodies(void)
{
  Object *o;
  WheelBody *wb;
  const vec3 *r;
  float f;
  int i;

  for (o = &sr_objects[0]; o < &sr_objects[1024]; o++) {
    if (o->active == 0)
      continue;
    for (i = 0; i < o->num_wheel_bodies; i++) {
      wb = &o->wheel_bodies[i];
      r = wb->rot;
      wb->vel.x = (float)(wb->dv.x + wb->vel.x);
      wb->vel.y = (float)(wb->dv.y + wb->vel.y);
      wb->vel.z = (float)(wb->dv.z + wb->vel.z);
      wb->ang_momentum.x = (float)(wb->dw.x + wb->ang_momentum.x);
      wb->ang_momentum.y = (float)(wb->dw.y + wb->ang_momentum.y);
      wb->ang_momentum.z = (float)(wb->dw.z + wb->ang_momentum.z);
      wb->w.x = 0.0f;
      wb->w.y = 0.0f;
      wb->w.z = 0.0f;
      f = (float)((float)(r[0].z * wb->ang_momentum.z + (wb->ang_momentum.y * r[0].y + wb->ang_momentum.x * r[0].x)) *
                  wb->inv_inertia.x);
      wb->w.x = (float)(wb->w.x + f * r[0].x);
      wb->w.y = (float)(r[0].y * f + wb->w.y);
      wb->w.z = (float)(f * r[0].z + wb->w.z);
      f = (float)((float)(r[1].z * wb->ang_momentum.z + (wb->ang_momentum.y * r[1].y + wb->ang_momentum.x * r[1].x)) *
                  wb->inv_inertia.y);
      wb->w.x = (float)(wb->w.x + f * r[1].x);
      wb->w.y = (float)(r[1].y * f + wb->w.y);
      wb->w.z = (float)(f * r[1].z + wb->w.z);
      f = (float)((float)(r[2].z * wb->ang_momentum.z + (r[2].x * wb->ang_momentum.x + wb->ang_momentum.y * r[2].y)) *
                  wb->inv_inertia.z);
      wb->w.x = (float)(wb->w.x + f * r[2].x);
      wb->w.y = (float)(r[2].y * f + wb->w.y);
      wb->w.z = (float)(f * r[2].z + wb->w.z);
    }
  }
}

/* After the last pass: the solver velocities go back to the particles and to the bones of
 * each active human with physics. */
static void copy_back(void)
{
  Human *h;
  int i, j, n = sr_numofparticles;

  for (i = 0; i < n; i++)
    sr_particles[i].vel = sr_bondparticles[i].vel;
  for (i = 0, h = &sr_humans[0]; i < 512; i++, h++) {
    if (h->active == 0 || h->has_physics == 0)
      continue;
    for (j = 0; j < 15; j++) {
      h->bones[j].vel = BOND_HUMAN_VEL(i, j);
      h->bones[j].ang_momentum = BOND_HUMAN_MOMENTUM(i, j);
      h->bones[j].w = BOND_HUMAN_W(i, j);
    }
  }
}

void bondsimulation2(void)
{
  int i, pass, type;
  unsigned mode;

  sr_timer_name(bondsim, "bondsim");
  sr_timer_name(bondtest, "bondtest");
  sr_timer_name(bondsetup, "bondsetup");
  sr_timer_name(bondsetupcopy, "bondsetupcopy");
  sr_timer_name(bondsetuphuman, "bondsetuphuman");
  sr_timer_name(bondloopcopy, "bondloopcopy");
  sr_timer_name(bondhuman, "bondhuman");
  sr_timer_name(bondwheelsim, "bondwheelsim");
  sr_timer_name(bondpartpart, "bondpartpart");
  sr_timer_name(bondbodybody, "bondbodybody");
  sr_timer_name(bondbodyrotbody, "bondbodyrotbody");
  sr_timer_name(bondwheelcollevel, "bondwheelcollevel");
  sr_timer_name(bondbodyitem, "bondbodyitem");
  sr_timer_name(bondbodylevel, "bondbodylevel");
  sr_timer_start(bondsim);
  sr_timer_start(bondsetup);
  for (i = 0; i < sr_numofbonds; i++) {
    Bond *b = &sr_bonds[i];
    BondOpt *o = &sr_bondopt[i];
    if (b->mode >= 0x100)
      continue;
    if (b->type == 0)
      setup_partpart(b, o);
    if (b->type == 10)
      setup_bodyitem(b, o);
    if (b->type == 8)
      setup_bodybody(b, o);
    if (b->type == 9)
      setup_bodyrotbody(b, o);
  }
  sr_timer_stop(bondsetup);
  setup_copy();
  for (pass = 0; pass < 32; pass++) {
    loop_clear(pass);
    loop_balance();
    sr_timer_start(bondwheelsim);
    objectwheelsimulation2();
    sr_timer_stop(bondwheelsim);
    for (i = 0; i < sr_numofbonds; i++) {
      Bond *b = &sr_bonds[i];
      BondOpt *o = &sr_bondopt[i];
      /* A bond takes part in the passes whose number has all the bits of its mode. */
      mode = BOND_MODE(b, o);
      if (((unsigned)pass & mode) != mode)
        continue;
      type = b->type;
      if (type == 0) {
        sr_timer_start(bondpartpart);
        bond_partpart(i);
        sr_timer_stop(bondpartpart);
      }
      if (type == 1)
        contact_level(b);
      if (type == 2)
        contact_particle(b);
      if (type == 3)
        contact_triangle(b);
      if (type == 4)
        contact_bone_triangle(b);
      if (type == 5) {
        sr_timer_start(bondwheelcollevel);
        contact_wheel(b);
        sr_timer_stop(bondwheelcollevel);
      }
      if (type == 7) {
        sr_timer_start(bondbodylevel);
        contact_bone_level(b);
        sr_timer_stop(bondbodylevel);
      }
      if (type == 8) {
        sr_timer_start(bondbodybody);
        bond_bodybody(i);
        sr_timer_stop(bondbodybody);
      }
      if (type == 9) {
        sr_timer_start(bondbodyrotbody);
        bond_bodyrotbody(o);
        sr_timer_stop(bondbodyrotbody);
      }
      if (type == 10) {
        sr_timer_start(bondbodyitem);
        bond_bodyitem(b, o);
        sr_timer_stop(bondbodyitem);
      }
      if (type == 11)
        bond_itemrot(b, o);
      if (type == 13)
        contact_bone_bone(b);
      if (type == 14)
        contact_bone_item(b);
      if (type == 15)
        contact_wheelbody(b);
#if BOND_BONE_SPRING
      if (type == BOND_TYPE_BONE_SPRING)
        bond_bone_spring(b, o);
#endif
    }
    sr_timer_start(bondwheelsim);
    objectenginesimulation2();
    sr_timer_stop(bondwheelsim);
    sr_timer_start(bondloopcopy);
    apply_particles();
    sr_timer_start(bondtest);
    apply_bones();
    sr_timer_stop(bondtest);
    apply_items();
    apply_wheelbodies();
    sr_timer_stop(bondloopcopy);
  }
  sr_timer_start(bondsetupcopy);
  copy_back();
  sr_timer_stop(bondsetupcopy);
  sr_timer_stop(bondsim);
  /* The contact bonds of this tick end here: the list goes back to the static bonds. */
  g_numofdynamicbonds = sr_numofbonds;
  sr_numofbonds = g_numofstaticbonds;
}
