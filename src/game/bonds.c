/* bondsimulation2: the bond solver of the physics, once for each tick.
 *
 * Each section has a profiler timer with its name. bondsetup prepares one BondOpt record for
 * each bond (setup_partpart and the others). bondsetupcopy copies the velocities into the work
 * arrays (setup_copy). The loop then does a number of solver passes over all the bonds, and the
 * end copies the velocities back. The parts from the loop on are not yet clean C.
 */
#include "lift_support.h"
#include "lift_os.h"

/* The type of the bond that pfVar33 points into (pfVar33 = &bond.normal.y, bond + 0x78).
 * An int load: as a float, the type is a denormal on the x87, and each test is slow. */
#define BOND_KIND(p) (*(int *)((char *)(p) - 0x78))

#define g_human (*(Human (*)[512])SR_VA(0x19cbf80))
#define g_sysinfo (*(SysInfo *)SR_VA(0x956d2e0))
#define DAT_00460d18 (*(uint *)SR_VA(0x460d18))
#define DAT_00603dd0 (*(int * *)SR_VA(0x603dd0))
#define g_numofstaticbonds (*(int *)SR_VA(0x14022d24))
#define g_numofdynamicbonds (*(int *)SR_VA(0x20a5a30))
#define g_yaxis (*(float (*)[3])SR_VA(0x460ce0))
#define g_zaxis (*(float (*)[3])SR_VA(0x460cec))
#define g_timer (*(Timer (*)[256])SR_VA(0x18043a0))
#define g_object (*(Object (*)[1024])SR_VA(0x5992600))
#define DAT_00461dd4 (*(undefined4 *)SR_VA(0x461dd4))
#define DAT_00461de0 (*(undefined4 *)SR_VA(0x461de0))
#define DAT_00461de4 (*(undefined4 *)SR_VA(0x461de4))
#define DAT_00461de8 (*(undefined4 *)SR_VA(0x461de8))
#define g_item (*(Item (*)[1024])SR_VA(0x95022c0))
#define DAT_00461df0 (*(undefined4 *)SR_VA(0x461df0))
#define DAT_00461df4 (*(undefined4 *)SR_VA(0x461df4))
#define DAT_00461df8 (*(undefined4 *)SR_VA(0x461df8))
#define DAT_00461e04 (*(undefined4 *)SR_VA(0x461e04))
#define g_particle (*(Particle (*)[16384])SR_VA(0xb7e7200))
#define DAT_00461e10 (*(undefined4 *)SR_VA(0x461e10))
#define DAT_00461e14 (*(undefined4 *)SR_VA(0x461e14))
#define DAT_00461e18 (*(undefined4 *)SR_VA(0x461e18))
#define DAT_00461e20 (*(undefined4 *)SR_VA(0x461e20))
#define DAT_00461e24 (*(undefined4 *)SR_VA(0x461e24))
#define DAT_00461e28 (*(undefined4 *)SR_VA(0x461e28))
#define DAT_00583dd0 (*(undefined4 *)SR_VA(0x583dd0))
#define DAT_00583dd4 (*(undefined4 *)SR_VA(0x583dd4))
#define DAT_00583dd8 (*(undefined4 *)SR_VA(0x583dd8))
#define DAT_00583de0 (*(undefined4 *)SR_VA(0x583de0))
#define DAT_00583de4 (*(undefined4 *)SR_VA(0x583de4))
#define g_bondopt (*(BondOpt *)SR_VA(0xfcf7b80))
#define g_timerlist (*(TimerList *)SR_VA(0x12e81c60))
#define DAT_00583de8 (*(undefined4 *)SR_VA(0x583de8))
#define g_numofbonds (*(int *)SR_VA(0x17579a28))
#define g_bond (*(Bond (*)[16384])SR_VA(0x17579a40))
#define g_area_sector (*(AreaSector *)SR_VA(0x1994d700))
#define geteulerangles2 ((void (__cdecl *)(float *, float *))SR_FN(0x409600))
#define stoptimer ((void (__cdecl *)(int))SR_FN(0x40b3f0))
#define rotatevector ((void (__cdecl *)(float *, float *, float))SR_FN(0x40bc50))
#define rotateorientation ((void (__cdecl *)(float *, float *, float))SR_FN(0x40bdb0))
#define bond_partpart ((void (__cdecl *)(int))SR_FN(0x412f80))
#define bond_bodybody ((void (__cdecl *)(int))SR_FN(0x4130d0))
#define multiply3x3matrices ((void (__cdecl *)(float *, float *, float *))SR_FN(0x41d9e0))
#define objectwheelsimulation2 ((void (__cdecl *)(void))SR_FN(0x422f90))
#define calculatefriction ((float10 (__cdecl *)(float *, float *, float))SR_FN(0x427e80))
#define objectenginesimulation2 ((void (__cdecl *)(void))SR_FN(0x4324c0))
#define __security_check_cookie(x) ((void)0)
#define _CIsqrt x87_sqrt
#define auStack_e4 (*(undefined1 (*)[16])(lf_frame + 0x1c))
#define local_d4 (*(float *)(lf_frame + 0x2c))
#define local_d0 (*(float *)(lf_frame + 0x30))
#define local_cc (*(float *)(lf_frame + 0x34))
#define local_c8 (*(float *)(lf_frame + 0x38))
#define local_c4 (*(float *)(lf_frame + 0x3c))
#define local_c0 (*(int *)(lf_frame + 0x40))
#define local_bc (*(int *)(lf_frame + 0x44))
#define local_b8 (*(float *)(lf_frame + 0x48))
#define local_b4 (*(float *)(lf_frame + 0x4c))
#define local_b0 (*(float *)(lf_frame + 0x50))
#define local_ac (*(float *)(lf_frame + 0x54))
#define local_a8 (*(float *)(lf_frame + 0x58))
#define local_a4 (*(float *)(lf_frame + 0x5c))
#define local_a0 (*(int * *)(lf_frame + 0x60))
#define local_9c (*(float * *)(lf_frame + 0x64))
#define local_98 (*(float *)(lf_frame + 0x68))
#define local_94 (*(float *)(lf_frame + 0x6c))
#define local_90 (*(float *)(lf_frame + 0x70))
#define local_8c (*(float *)(lf_frame + 0x74))
#define local_88 (*(float * *)(lf_frame + 0x78))
#define local_84 (*(int * *)(lf_frame + 0x7c))
#define local_80 (*(float * *)(lf_frame + 0x80))
#define local_7c (*(float *)(lf_frame + 0x84))
#define local_78 (*(float *)(lf_frame + 0x88))
#define local_74 (*(float *)(lf_frame + 0x8c))
#define local_70 (*(float *)(lf_frame + 0x90))
#define local_6c (*(float *)(lf_frame + 0x94))
#define local_68 (*(float *)(lf_frame + 0x98))
#define local_64 (*(float *)(lf_frame + 0x9c))
#define local_60 (*(int * *)(lf_frame + 0xa0))
#define local_5c (*(float *)(lf_frame + 0xa4))
#define local_58 (*(float *)(lf_frame + 0xa8))
#define local_54 (*(float *)(lf_frame + 0xac))
#define local_50 (*(float * *)(lf_frame + 0xb0))
#define local_4c (*(float *)(lf_frame + 0xb4))
#define local_48 (*(float *)(lf_frame + 0xb8))
#define local_44 (*(float *)(lf_frame + 0xbc))
#define local_40 (*(float *)(lf_frame + 0xc0))
#define local_3c (*(float *)(lf_frame + 0xc4))
#define local_38 (*(float *)(lf_frame + 0xc8))
#define local_34 (*(float *)(lf_frame + 0xcc))
#define local_30 (*(float *)(lf_frame + 0xd0))
#define local_2c (*(float *)(lf_frame + 0xd4))
#define local_28 (*(float *)(lf_frame + 0xd8))
#define local_24 (*(float *)(lf_frame + 0xdc))
#define local_20 (*(float *)(lf_frame + 0xe0))
#define local_1c (*(float *)(lf_frame + 0xe4))
#define local_18 (*(float *)(lf_frame + 0xe8))
#define local_14 (*(float *)(lf_frame + 0xec))
#define local_10 (*(float *)(lf_frame + 0xf0))
#define local_c (*(float *)(lf_frame + 0xf4))
#define local_8 (*(float *)(lf_frame + 0xf8))
#define local_4 (*(uint *)(lf_frame + 0xfc))
/* Give a profiler timer its name and clear its total. */
static void nametimer(int timer, const char *name)
{
  Timer *t = &g_timer[timer];
  char *d = t->name;
  while ((*d++ = *name++) != '\0')
    ;
  t->total_lo = 0;
  t->total_hi = 0;
  t->ms = 0;
}

static void starttimer(int timer)
{
  ulonglong now = rdtsc();
  g_timer[timer].start_lo = (int)now;
  g_timer[timer].start_hi = (int)(now >> 0x20);
}

/* bondsetup, type 0: two particles. The direction from a to b, the stretch, and the share of
 * each side from the masses. */
static void setup_partpart(const Bond *b, BondOpt *o)
{
  const Particle *pa = &g_particle[b->particle_a];
  const Particle *pb = &g_particle[b->particle_b];
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
  const Bone *bone = &g_human[b->owner].bones[b->bone_a];
  const Item *it = &g_item[b->target];
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
  const Bone *ba = &g_human[b->owner].bones[b->bone_a];
  const Bone *bb = &g_human[b->owner].bones[b->target];
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
 * angles get a dead zone. The share of each side also has the strength of the muscle. */
static void setup_bodyrotbody(const Bond *b, BondOpt *o)
{
  Human *h = &g_human[b->owner];
  int ka = b->bone_a, kb = b->target;
  const Bone *ba = &h->bones[ka];
  Bone *bb = &h->bones[kb];
  float m[9], e[3], r[9];
  float k, e0, e1, lo, hi, inv, sa, sb;
  int row, col;

  starttimer(g_timerlist.bondsetuphuman);
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
  multiply3x3matrices(m, m, (float *)bb->target_rot);
  k = b->damping;
  o->damping = k;
  /* For bones 3 to 8 of a living human, the direction of the last tick stays. */
  if (kb < 3 || 8 < kb || h->health < 1) {
    geteulerangles2(e, m);
    e0 = e[0];
    e1 = e[1];
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
      if (lo <= e[0]) {
        if (e[0] <= hi)
          e[0] = 0.0f;
        else
          e[0] = (float)(e[0] - hi);
      } else {
        e[0] = (float)(e[0] - lo);
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
      if (lo <= e[1]) {
        if (e[1] <= hi)
          e[1] = 0.0f;
        else
          e[1] = e[1] - hi;
      } else {
        e[1] = e[1] - lo;
      }
      e[0] = e[0] * 0.125f;
      e[1] = (float)(e[1] * 0.125);
      e[2] = (float)(e[2] * 0.125);
      o->damping = (float)(b->damping * 0.5);
    }
    if (e[0] < -0.7853981852531433)
      e[0] = -0.7853981852531433f;
    if (0.7853981852531433 < e[0])
      e[0] = 0.7853981852531433f;
    if (e[1] < -0.7853981852531433)
      e[1] = -0.7853981852531433f;
    if (0.7853981852531433 < e[1])
      e[1] = 0.7853981852531433f;
    if (e[2] < -0.7853981852531433)
      e[2] = -0.7853981852531433f;
    if (0.7853981852531433 < e[2])
      e[2] = 0.7853981852531433f;
    /* Each angle turns around one axis of bone a. The axis points into r, the matrix that
     * rotateorientation turns, so each turn uses the axis that the turn before it moved. */
    r[0] = ba->rot[0].x;
    r[1] = ba->rot[0].y;
    r[2] = ba->rot[0].z;
    r[3] = ba->rot[1].x;
    r[4] = ba->rot[1].y;
    r[5] = ba->rot[1].z;
    r[6] = ba->rot[2].x;
    r[7] = ba->rot[2].y;
    r[8] = ba->rot[2].z;
    o->dir.x = (float)(r[6] * e[0]);
    o->dir.y = (float)(r[7] * e[0]);
    o->dir.z = (float)(e[0] * r[8]);
    rotateorientation(r, &r[6], e0);
    o->dir.x = (float)(o->dir.x + e[1] * r[0]);
    o->dir.y = (float)(e[1] * r[1] + o->dir.y);
    o->dir.z = (float)(e[1] * r[2] + o->dir.z);
    rotateorientation(r, r, e1);
    o->dir.x = (float)(e[2] * r[3] + o->dir.x);
    o->dir.y = (float)(e[2] * r[4] + o->dir.y);
    o->dir.z = (float)(e[2] * r[5] + o->dir.z);
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
  /* arm_a.x holds the strength of the muscle for this bond type. */
  o->arm_a.x = bb->strength;
  if (bb->strength < bb->strength_min)
    o->arm_a.x = bb->strength_min;
  if (o->arm_a.x < bb->strength_min2)
    o->arm_a.x = bb->strength_min2;
  o->share_a = o->arm_a.x * sa;
  o->share_b = (float)(sb * o->arm_a.x);
  stoptimer(g_timerlist.bondsetuphuman);
}

#define g_bondhuman (*(BondHuman (*)[512])SR_VA(0x461dd0))
#define g_bondparticle (*(BondParticle (*)[16384])SR_VA(0x583dd0))
#define g_bondopts (*(BondOpt (*)[16384])SR_VA(0xfcf7b80))

/* bondsetupcopy: the solver works on copies. For each human with a body: the velocity, the
 * angular velocity and the axes of its 15 bones, and a mask of the bones that take part. For
 * each particle: the velocity. For each bond: the mode. */
static void setup_copy(void)
{
  int h, k, i, n;

  starttimer(g_timerlist.bondsetupcopy);
  for (h = 0; h < 512; h++) {
    const Human *hu = &g_human[h];
    BondHuman *bh = &g_bondhuman[h];
    bh->active = 0;
    if (hu->active == 0 || hu->has_physics == 0)
      continue;
    bh->active = 1;
    bh->mask = 0;
    for (k = 0; k < 15; k++) {
      const Bone *bone = &hu->bones[k];
      BondBone *w = &bh->bones[k];
      if (0.0 < bone->unk_cc)
        bh->mask |= 1 << k;
      w->vel = bone->vel;
      w->w = bone->w;
      w->ang_momentum = bone->ang_momentum;
      w->axes[0].dir = bone->rot[0];
      w->axes[0].k = bone->axis_k[0];
      w->axes[1].dir = bone->rot[1];
      w->axes[1].k = bone->axis_k[1];
      w->axes[2].dir = bone->rot[2];
      w->axes[2].k = bone->axis_k[2];
    }
  }
  n = g_area_sector.num_particles;
  for (i = 0; i < n; i++)
    g_bondparticle[i].vel = g_particle[i].vel;
  n = g_numofbonds;
  for (i = 0; i < n; i++)
    g_bondopts[i].mode = (short)g_bond[i].mode;
  stoptimer(g_timerlist.bondsetupcopy);
}

#define g_bondpass (*(int *)SR_VA(0x603dd0))

/* bondloopcopy: the start of each solver pass. The pass number goes into a global (the wheels
 * use it), and each velocity change starts at 0: the particles, the items with physics, the
 * wheel bodies of the objects, and the bones of the humans in the solver. */
static void loop_clear(int pass)
{
  int i, k, n;

  starttimer(g_timerlist.bondloopcopy);
  g_bondpass = pass;
  n = g_area_sector.num_particles;
  for (i = 0; i < n; i++) {
    g_bondparticle[i].dv.x = 0.0f;
    g_bondparticle[i].dv.y = 0.0f;
    g_bondparticle[i].dv.z = 0.0f;
  }
  for (i = 0; i < 1024; i++) {
    Item *it = &g_item[i];
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
    Object *o = &g_object[i];
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
    BondHuman *bh = &g_bondhuman[i];
    if (bh->active == 0)
      continue;
    for (k = 0; k < 15; k++) {
      BondBone *b = &bh->bones[k];
      if ((bh->mask & 1 << k) == 0)
        continue;
      b->dv.x = 0.0f;
      b->dv.y = 0.0f;
      b->dv.z = 0.0f;
      b->dw.x = 0.0f;
      b->dw.y = 0.0f;
      b->dw.z = 0.0f;
    }
  }
  stoptimer(g_timerlist.bondloopcopy);
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
  const vec3 up = *(const vec3 *)g_yaxis;
  int h;

  starttimer(g_timerlist.bondhuman);
  for (h = 0; h < 512; h++) {
    const Human *hu = &g_human[h];
    const vec3 *r1 = &hu->bones[0].rot[1], *r2 = &hu->bones[0].rot[2];
    BondBone *w = &g_bondhuman[h].bones[0];
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
    face = *(const vec3 *)g_zaxis;
    rotatevector(&face.x, g_yaxis, hu->axis);
    t.x = 0.0f;
    t.y = (float)((float)(r2->x * face.z - r2->z * face.x) * 0.125);
    t.z = 0.0f;
    s = (float)(r1->y + 0.30000001192092896);
    if (0.0 <= s) {  /* this form sends a NaN to 0 */
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
    a.x = (float)(a.x - w->w.x * 0.5);
    a.y = (float)(a.y - w->w.y * 0.5);
    a.z = (float)(a.z - w->w.z * 0.5);
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
    w->ang_momentum.x = (float)(w->ang_momentum.x + a.x);
    w->ang_momentum.y = (float)(a.y + w->ang_momentum.y);
    w->ang_momentum.z = (float)(a.z + w->ang_momentum.z);
  }
  stoptimer(g_timerlist.bondhuman);
}

/* The contact bonds hold a point against a hit surface. The velocity change pushes the point
 * out along the normal (the stretch rest_length * stiffness, plus the damped speed against the
 * surface), and calculatefriction removes a part of the sideways speed. A contact only pushes:
 * when the change points into the surface, nothing happens. */

/* Type 1: a particle against a fixed surface that moves with surface_vel. */
static void contact_level(const Bond *b)
{
  BondParticle *pa = &g_bondparticle[b->particle_a];
  const vec3 n = b->normal;
  float k = (float)(b->rest_length * b->stiffness), d = b->damping, nd = -b->damping;
  vec3 v;

  v.x = (float)((float)(nd * pa->vel.x + (float)(k * n.x)) + d * b->surface_vel.x);
  v.y = (float)(b->surface_vel.y * d + (float)(pa->vel.y * nd + (float)(k * n.y)));
  v.z = (float)(d * b->surface_vel.z + (float)(nd * pa->vel.z + (float)(k * n.z)));
  if (!(0.0 < (float)(n.z * v.z + (n.x * v.x + v.y * n.y))))
    return;
  calculatefriction(&v.x, (float *)&b->normal, b->friction);
  pa->dv.x = (float)(pa->dv.x + v.x);
  pa->dv.y = (float)(pa->dv.y + v.y);
  pa->dv.z = (float)(pa->dv.z + v.z);
}

/* Type 2: particle a against particle b. Each side gets its share of the change from the
 * masses. */
static void contact_particle(const Bond *b)
{
  BondParticle *pa = &g_bondparticle[b->particle_a], *pb = &g_bondparticle[b->particle_b];
  const Particle *ma = &g_particle[b->particle_a], *mb = &g_particle[b->particle_b];
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
  calculatefriction(&v.x, (float *)&b->normal, b->friction);
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
  BondParticle *pb = &g_bondparticle[b->particle_b];
  BondParticle *pc = &g_bondparticle[b->particle_c];
  BondParticle *pd = &g_bondparticle[b->particle_d];
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
  return (float)(g_particle[b->particle_d].mass * b->weights[2] +
                 (g_particle[b->particle_c].mass * b->weights[1] + g_particle[b->particle_b].mass * b->weights[0]));
}

/* Type 3: particle a against a triangle of another object (particles b, c and d). */
static void contact_triangle(const Bond *b)
{
  BondParticle *pa = &g_bondparticle[b->particle_a];
  const BondParticle *pb = &g_bondparticle[b->particle_b];
  const BondParticle *pc = &g_bondparticle[b->particle_c];
  const BondParticle *pd = &g_bondparticle[b->particle_d];
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
  ma = g_particle[b->particle_a].mass;
  mt = triangle_mass(b);
  s = (float)((float)len / (mt + ma));
  if (!(0.0 < (float)(n.z * v.z + (n.x * v.x + v.y * n.y))))
    return;
  calculatefriction(&v.x, (float *)&b->normal, b->friction);
  f = s * mt;
  pa->dv.x = (float)(pa->dv.x + f * v.x);
  pa->dv.y = (float)(v.y * f + pa->dv.y);
  pa->dv.z = (float)(pa->dv.z + v.z * f);
  push_triangle(b, -s * ma, &v);
}

/* Type 4: a bone of a human against a triangle of an object. */
static void contact_bone_triangle(const Bond *b)
{
  BondBone *x = &g_bondhuman[b->owner].bones[b->bone_a];
  const BondParticle *pb = &g_bondparticle[b->particle_b];
  const BondParticle *pc = &g_bondparticle[b->particle_c];
  const BondParticle *pd = &g_bondparticle[b->particle_d];
  const vec3 n = b->normal;
  float k = (float)(b->rest_length * b->stiffness), nd = -b->damping;
  float wb = (float)(b->weights[0] * b->damping), wc = (float)(b->weights[1] * b->damping);
  float wd = (float)(b->weights[2] * b->damping);
  float len, inv, mb, mt, f;
  vec3 v;

  v.x = (float)((float)((float)(wb * pb->vel.x + (float)(k * n.x + nd * x->vel.x)) + wc * pc->vel.x) +
                wd * pd->vel.x);
  v.y = (float)(pd->vel.y * wd + (float)(pc->vel.y * wc + (float)(pb->vel.y * wb + (float)(x->vel.y * nd + (float)(n.y * k)))));
  v.z = (float)(wd * pd->vel.z + (float)(wc * pc->vel.z + (float)(wb * pb->vel.z + (float)(nd * x->vel.z + (float)(k * n.z)))));
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
  mb = g_human[b->owner].bones[b->bone_a].mass;
  mt = triangle_mass(b);
  len = (float)(len / (mt + mb));
  if (!(0.0 < (float)(n.z * v.z + (n.x * v.x + v.y * n.y))))
    return;
  calculatefriction(&v.x, (float *)&b->normal, b->friction);
  f = len * mt;
  x->dv.x = (float)(x->dv.x + f * v.x);
  x->dv.y = (float)(v.y * f + x->dv.y);
  x->dv.z = (float)(x->dv.z + v.z * f);
  push_triangle(b, -len * mb, &v);
}

/* Type 5: a wheel of an object on the ground. The ground moves against the wheel at
 * spin * radius along roll_dir. The push goes into the hub, the friction into Wheel.slip,
 * and the part of the push along roll_dir slows the spin. */
static void contact_wheel(const Bond *b)
{
  Wheel *w = &g_object[b->owner].wheels[b->bone_a];
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
  w->slip = (float)(calculatefriction(&v.x, (float *)&b->normal, b->friction) + (float10)w->slip);
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
  BondBone *x = &g_bondhuman[b->owner].bones[b->bone_a];
  const vec3 n = b->normal, r = b->arm;
  float k = (float)(b->rest_length * b->stiffness), nd = -b->damping;
  vec3 p, v, t;

  p.x = (float)((float)(x->w.z * r.y - r.z * x->w.y) + x->vel.x);
  p.y = (float)(x->vel.y + (float)(r.z * x->w.x - r.x * x->w.z));
  p.z = (float)(x->vel.z + (float)(r.x * x->w.y - x->w.x * r.y));
  v.x = (float)(k * n.x + nd * p.x);
  v.y = (float)(nd * p.y + (float)(k * n.y));
  v.z = (float)(nd * p.z + (float)(k * n.z));
  if (!(0.0 < (float)(n.z * v.z + (n.x * v.x + v.y * n.y))))
    return;
  calculatefriction(&v.x, (float *)&b->normal, b->friction);
  x->dv.x = (float)(v.x + x->dv.x);
  x->dv.y = (float)(v.y + x->dv.y);
  x->dv.z = (float)(v.z + x->dv.z);
  t.x = (float)(r.z * v.y - v.z * r.y);
  t.y = (float)(v.z * r.x - r.z * v.x);
  t.z = (float)(r.y * v.x - v.y * r.x);
  x->dw.x = (float)(t.x + x->dw.x);
  x->dw.y = (float)(x->dw.y + t.y);
  x->dw.z = (float)(x->dw.z + t.z);
}

/* Type 9: the turn that a muscle holds between bones b and c of human a. The change of the
 * angular velocity removes the turn error (dir) and a part of the difference of the two turns.
 * Each bone gets its share. */
static void bond_bodyrotbody(const BondOpt *o)
{
  BondHuman *h = &g_bondhuman[(ushort)o->a];
  BondBone *x = &h->bones[(ushort)o->b], *y = &h->bones[(ushort)o->c];
  float d = o->damping, nd = -o->damping, ns;
  vec3 v;

  v.x = (float)((float)(nd * x->w.x + o->dir.x) + d * y->w.x);
  v.y = (float)(y->w.y * d + (float)(x->w.y * nd + o->dir.y));
  v.z = (float)(d * y->w.z + (float)(nd * x->w.z + o->dir.z));
  x->dw.x = (float)(x->dw.x + o->share_a * v.x);
  x->dw.y = (float)(v.y * o->share_a + x->dw.y);
  x->dw.z = (float)(x->dw.z + v.z * o->share_a);
  ns = -o->share_b;
  y->dw.x = (float)(y->dw.x + ns * v.x);
  y->dw.y = (float)(v.y * ns + y->dw.y);
  y->dw.z = (float)(v.z * ns + y->dw.z);
}

/* Type 10: a bone of a human holds an item at a point. The velocity of the hold point on the
 * bone (arm_a) and on the item (arm_b) must be equal. The change goes to the bone with the share
 * weights[0] and to the item with weights[1], and each also turns around its point. */
static void bond_bodyitem(const Bond *b, const BondOpt *o)
{
  BondBone *x = &g_bondhuman[b->owner].bones[b->bone_a];
  Item *it = &g_item[b->target];
  const vec3 ra = o->arm_a, rb = o->arm_b;
  float d = b->damping, nd = -b->damping;
  float sb = b->weights[0], si = b->weights[1], ns = -sb;
  vec3 v, t;

  v.x = (float)(nd * (float)(it->vel.x + (float)(rb.y * it->angvel.z - rb.z * it->angvel.y)) +
                (float)(d * (float)((float)(ra.y * x->w.z - ra.z * x->w.y) + x->vel.x) + o->dir.x));
  v.y = (float)(nd * (float)(it->vel.y + (float)(it->angvel.x * rb.z - rb.x * it->angvel.z)) +
                (float)(d * (float)(x->vel.y + (float)(x->w.x * ra.z - ra.x * x->w.z)) + o->dir.y));
  v.z = (float)(nd * (float)(it->vel.z + (float)(it->angvel.y * rb.x - rb.y * it->angvel.x)) +
                (float)(d * (float)(x->vel.z + (float)(ra.x * x->w.y - x->w.x * ra.y)) + o->dir.z));
  x->dv.x = (float)(x->dv.x + v.x * ns);
  x->dv.y = (float)(x->dv.y + v.y * ns);
  x->dv.z = (float)(x->dv.z + v.z * ns);
  x->dw.x = (float)(ns * (float)(v.y * ra.z - ra.y * v.z) + x->dw.x);
  x->dw.y = (float)(ns * (float)(ra.x * v.z - v.x * ra.z) + x->dw.y);
  x->dw.z = (float)(ns * (float)(v.x * ra.y - v.y * ra.x) + x->dw.z);
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
  BondBone *x = &g_bondhuman[b->owner].bones[b->bone_a];
  Item *it = &g_item[b->target];
  float d = o->damping, nd = -o->damping, g = o->arm_a.x, ns;
  vec3 v;

  v.x = (float)(g * (float)((float)(nd * x->w.x + o->dir.x) + d * it->angvel.x));
  v.y = (float)(g * (float)(it->angvel.y * d + (float)(x->w.y * nd + o->dir.y)));
  v.z = (float)(g * (float)(d * it->angvel.z + (float)(nd * x->w.z + o->dir.z)));
  x->dw.x = (float)(x->dw.x + v.x * o->share_a);
  x->dw.y = (float)(x->dw.y + v.y * o->share_a);
  x->dw.z = (float)(x->dw.z + v.z * o->share_a);
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
  BondBone *x = &g_bondhuman[b->owner].bones[b->bone_a];
  BondBone *y = &g_bondhuman[b->target].bones[b->bone_b];
  float mx = g_human[b->owner].bones[b->bone_a].mass, my = g_human[b->target].bones[b->bone_b].mass;
  const vec3 n = b->normal;
  float k = (float)(b->rest_length * b->stiffness), d = b->damping, nd = -b->damping;
  float s, f;
  vec3 v;

  v.x = (float)(d * y->vel.x + (float)(nd * x->vel.x + k * n.x));
  v.y = (float)(y->vel.y * d + (float)(x->vel.y * nd + (float)(k * n.y)));
  v.z = (float)(d * y->vel.z + (float)(nd * x->vel.z + (float)(k * n.z)));
  s = (float)(unit_or_zero(&v) / (mx + my));
  if (!(0.0 < (float)(n.z * v.z + (v.x * n.x + v.y * n.y))))
    return;
  calculatefriction(&v.x, (float *)&b->normal, b->friction);
  f = (float)(s * my);
  x->dv.x = (float)(x->dv.x + f * v.x);
  x->dv.y = (float)(x->dv.y + v.y * f);
  x->dv.z = (float)(x->dv.z + v.z * f);
  f = (float)(-s * mx);
  y->dv.x = (float)(y->dv.x + f * v.x);
  y->dv.y = (float)(y->dv.y + f * v.y);
  y->dv.z = (float)(f * v.z + y->dv.z);
}

/* Type 14: a bone of a human against an item. The hit point is local_a on the bone and
 * local_b on the item, so the push also turns both. */
static void contact_bone_item(const Bond *b)
{
  BondBone *x = &g_bondhuman[b->owner].bones[b->bone_a];
  Item *it = &g_item[b->target];
  float mx = g_human[b->owner].bones[b->bone_a].mass;
  const vec3 n = b->normal, ra = b->local_a, rb = b->local_b;
  float k = (float)(b->rest_length * b->stiffness), d = b->damping, nd = -b->damping;
  float s, f;
  vec3 p, v, a, t;

  p.x = (float)(it->vel.x + (float)(rb.y * it->angvel.z - rb.z * it->angvel.y));
  p.y = (float)(it->vel.y + (float)(rb.z * it->angvel.x - rb.x * it->angvel.z));
  p.z = (float)(it->vel.z + (float)(rb.x * it->angvel.y - it->angvel.x * rb.y));
  v.x = (float)(d * p.x + (float)(nd * (float)(x->vel.x + (float)(ra.y * x->w.z - ra.z * x->w.y)) + k * n.x));
  v.y = (float)(d * p.y +
                (float)(nd * (float)(x->vel.y + (float)(x->w.x * ra.z - ra.x * x->w.z)) + (float)(k * n.y)));
  v.z = (float)(d * p.z +
                (float)(nd * (float)(x->vel.z + (float)(x->w.y * ra.x - ra.y * x->w.x)) + (float)(k * n.z)));
  s = (float)(unit_or_zero(&v) / (mx + it->mass));
  if (!(0.0 < (float)(n.z * v.z + (v.x * n.x + v.y * n.y))))
    return;
  calculatefriction(&v.x, (float *)&b->normal, b->friction);
  f = (float)(it->mass * s);
  a.x = f * v.x;
  a.y = (float)(f * v.y);
  a.z = (float)(f * v.z);
  x->dv.x = (float)(a.x + x->dv.x);
  x->dv.y = (float)(a.y + x->dv.y);
  x->dv.z = (float)(a.z + x->dv.z);
  x->dw.x = (float)((float)(ra.z * a.y - ra.y * a.z) + x->dw.x);
  x->dw.y = (float)(x->dw.y + (float)(ra.x * a.z - a.x * ra.z));
  x->dw.z = (float)(x->dw.z + (float)(ra.y * a.x - ra.x * a.y));
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
  WheelBody *wb = &g_object[b->owner].wheel_bodies[b->bone_a];
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
  calculatefriction(&v.x, (float *)&b->normal, b->friction);
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

/* The end of each pass: each particle takes its velocity change. */
static void apply_particles(void)
{
  BondParticle *p = &g_bondparticle[0];
  int i, n = g_area_sector.num_particles;

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
  BondHuman *h;
  BondBone *x;
  const BondAxis *a;
  uint bit;
  float f;
  int i;

  for (h = &g_bondhuman[0]; h < &g_bondhuman[512]; h++) {
    if (h->active == 0)
      continue;
    for (i = 0, bit = 1; i < 15; i++, bit <<= 1) {
      if (((uint)h->mask & bit) == 0)
        continue;
      x = &h->bones[i];
      a = x->axes;
      x->vel.x = (float)(x->dv.x + x->vel.x);
      x->vel.y = (float)(x->dv.y + x->vel.y);
      x->vel.z = (float)(x->dv.z + x->vel.z);
      x->ang_momentum.x = (float)(x->dw.x + x->ang_momentum.x);
      x->ang_momentum.y = (float)(x->dw.y + x->ang_momentum.y);
      x->ang_momentum.z = (float)(x->dw.z + x->ang_momentum.z);
      f = (float)((float)(x->ang_momentum.z * a[0].dir.z +
                          (x->ang_momentum.y * a[0].dir.y + x->ang_momentum.x * a[0].dir.x)) * a[0].k);
      x->w.x = f * a[0].dir.x;
      x->w.y = (float)(f * a[0].dir.y);
      x->w.z = (float)(f * a[0].dir.z);
      f = (float)((float)(x->ang_momentum.z * a[1].dir.z +
                          (x->ang_momentum.y * a[1].dir.y + x->ang_momentum.x * a[1].dir.x)) * a[1].k);
      x->w.x = (float)(x->w.x + f * a[1].dir.x);
      x->w.y = (float)(f * a[1].dir.y + x->w.y);
      x->w.z = (float)(f * a[1].dir.z + x->w.z);
      f = (float)((float)(x->ang_momentum.z * a[2].dir.z +
                          (x->ang_momentum.y * a[2].dir.y + x->ang_momentum.x * a[2].dir.x)) * a[2].k);
      x->w.x = (float)(f * a[2].dir.x + x->w.x);
      x->w.y = (float)(f * a[2].dir.y + x->w.y);
      x->w.z = (float)(f * a[2].dir.z + x->w.z);
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

  for (it = &g_item[0]; it < &g_item[1024]; it++) {
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

  for (o = &g_object[0]; o < &g_object[1024]; o++) {
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
  int i, j, n = g_area_sector.num_particles;

  for (i = 0; i < n; i++)
    g_particle[i].vel = g_bondparticle[i].vel;
  for (i = 0, h = &g_human[0]; i < 512; i++, h++) {
    if (h->active == 0 || h->has_physics == 0)
      continue;
    for (j = 0; j < 15; j++) {
      const BondBone *x = &g_bondhuman[i].bones[j];
      h->bones[j].vel = x->vel;
      h->bones[j].ang_momentum = x->ang_momentum;
      h->bones[j].w = x->w;
    }
  }
}

void __cdecl lf_41de0(void)

{
  __declspec(align(16)) uchar lf_frame[0x100];
  vec3 *pvVar1;
  lf_m128 auVar2;
  lf_m128 auVar3;
  lf_m128 auVar4;
  float fVar5;
  int kind_copy;
  float fVar6;
  float fVar7;
  float fVar8;
  float fVar9;
  float fVar10;
  float fVar11;
  char cVar12;
  undefined8 uVar13;
  float *pfVar14;
  char *pcVar15;
  int iVar16;
  int iVar17;
  int *piVar18;
  undefined4 *puVar19;
  uint *puVar20;
  float *pfVar21;
  int *piVar22;
  int iVar23;
  short *psVar24;
  uint uVar25;
  int *piVar26;
  uint *puVar27;
  uint uVar28;
  int iVar29;
  int iVar30;
  uint *puVar31;
  uint uVar32;
  float *pfVar33;
  int *piVar34;
  float10 fVar35;
  lf_m128 auVar36;
  lf_m128 auVar37;
  lf_m128 auVar38;
  double dVar39;






















































  
  local_4 = DAT_00460d18 ^ (uint)auStack_e4;
  nametimer(g_timerlist.bondsim, "bondsim");
  nametimer(g_timerlist.bondtest, "bondtest");
  nametimer(g_timerlist.bondsetup, "bondsetup");
  nametimer(g_timerlist.bondsetupcopy, "bondsetupcopy");
  nametimer(g_timerlist.bondsetuphuman, "bondsetuphuman");
  nametimer(g_timerlist.bondloopcopy, "bondloopcopy");
  nametimer(g_timerlist.bondhuman, "bondhuman");
  nametimer(g_timerlist.bondwheelsim, "bondwheelsim");
  nametimer(g_timerlist.bondpartpart, "bondpartpart");
  nametimer(g_timerlist.bondbodybody, "bondbodybody");
  nametimer(g_timerlist.bondbodyrotbody, "bondbodyrotbody");
  nametimer(g_timerlist.bondwheelcollevel, "bondwheelcollevel");
  nametimer(g_timerlist.bondbodyitem, "bondbodyitem");
  nametimer(g_timerlist.bondbodylevel, "bondbodylevel");
  starttimer(g_timerlist.bondsim);
  starttimer(g_timerlist.bondsetup);
  local_c0 = 0;
  if (0 < g_numofbonds) {
    local_a0 = &g_bond[0].particle_a;
    pfVar21 = &g_bondopt.dir.y;
    do {
      piVar18 = local_a0;
      if (local_a0[-2] < 0x100) {
        if (((Bond *)(local_a0 + -5))->type == 0) {
          setup_partpart((Bond *)(local_a0 + -5), (BondOpt *)(pfVar21 + -3));
        }
        if (((Bond *)(piVar18 + -5))->type == 10) {
          setup_bodyitem((Bond *)(piVar18 + -5), (BondOpt *)(pfVar21 + -3));
        }
        if (((Bond *)(piVar18 + -5))->type == 8) {
          setup_bodybody((Bond *)(piVar18 + -5), (BondOpt *)(pfVar21 + -3));
        }
        if (((Bond *)(piVar18 + -5))->type == 9) {
          setup_bodyrotbody((Bond *)(piVar18 + -5), (BondOpt *)(pfVar21 + -3));
        }
      }
      local_c0 = local_c0 + 1;
      local_a0 = piVar18 + 0x3b;
      pfVar21 = pfVar21 + 0x10;
    } while (local_c0 < g_numofbonds);
  }
  stoptimer(g_timerlist.bondsetup);
  setup_copy();
  local_60 = (int *)0x0;
  do {
    piVar18 = local_60;
    loop_clear((int)local_60);
    loop_balance();
    starttimer(g_timerlist.bondwheelsim);
    objectwheelsimulation2();
    stoptimer(g_timerlist.bondwheelsim);
    local_c0 = 0;
    if (0 < g_numofbonds) {
      pfVar33 = &g_bond[0].normal.y;
      pfVar21 = &g_bondopt.arm_a.z;
      do {
        if (((uint)local_60 & (uint)(ushort)*(short *)((int)pfVar21 + -0x1a)) ==
            (uint)(ushort)*(short *)((int)pfVar21 + -0x1a)) {
          if (BOND_KIND(pfVar33) == 0x0U) {
            starttimer(g_timerlist.bondpartpart);
            bond_partpart(local_c0);
            stoptimer(g_timerlist.bondpartpart);
          }
          if (BOND_KIND(pfVar33) == 0x1U)
            contact_level((Bond *)((char *)pfVar33 - 0x78));
          if (BOND_KIND(pfVar33) == 0x2U)
            contact_particle((Bond *)((char *)pfVar33 - 0x78));
          if (BOND_KIND(pfVar33) == 0x3U)
            contact_triangle((Bond *)((char *)pfVar33 - 0x78));
          if (BOND_KIND(pfVar33) == 0x4U)
            contact_bone_triangle((Bond *)((char *)pfVar33 - 0x78));
          if (BOND_KIND(pfVar33) == 0x5U) {
            starttimer(g_timerlist.bondwheelcollevel);
            contact_wheel((Bond *)((char *)pfVar33 - 0x78));
            stoptimer(g_timerlist.bondwheelcollevel);
          }
          if (BOND_KIND(pfVar33) == 0x7U) {
            starttimer(g_timerlist.bondbodylevel);
            contact_bone_level((Bond *)((char *)pfVar33 - 0x78));
            stoptimer(g_timerlist.bondbodylevel);
          }
          if (BOND_KIND(pfVar33) == 0x8U) {
            starttimer(g_timerlist.bondbodybody);
            bond_bodybody(local_c0);
            stoptimer(g_timerlist.bondbodybody);
          }
          if (BOND_KIND(pfVar33) == 0x9U) {
            starttimer(g_timerlist.bondbodyrotbody);
            bond_bodyrotbody((BondOpt *)(pfVar21 - 8));
            stoptimer(g_timerlist.bondbodyrotbody);
          }
          if (BOND_KIND(pfVar33) == 0xaU) {
            starttimer(g_timerlist.bondbodyitem);
            bond_bodyitem((Bond *)((char *)pfVar33 - 0x78), (BondOpt *)(pfVar21 - 8));
            stoptimer(g_timerlist.bondbodyitem);
          }
          /* One read of the type for the next two tests. An int: as a float it is a denormal
           * on the x87. */
          kind_copy = BOND_KIND(pfVar33);
          if (kind_copy == 0xb)
            bond_itemrot((Bond *)((char *)pfVar33 - 0x78), (BondOpt *)(pfVar21 - 8));
          if (kind_copy == 0xd)
            contact_bone_bone((Bond *)((char *)pfVar33 - 0x78));
          if (BOND_KIND(pfVar33) == 0xeU)
            contact_bone_item((Bond *)((char *)pfVar33 - 0x78));
          if (BOND_KIND(pfVar33) == 0xfU)
            contact_wheelbody((Bond *)((char *)pfVar33 - 0x78));
        }
        local_c0 = local_c0 + 1;
        pfVar21 = pfVar21 + 0x10;
        pfVar33 = pfVar33 + 0x3b;
        piVar18 = local_60;
      } while (local_c0 < g_numofbonds);
    }
    starttimer(g_timerlist.bondwheelsim);
    objectenginesimulation2();
    stoptimer(g_timerlist.bondwheelsim);
    starttimer(g_timerlist.bondloopcopy);
    apply_particles();
    starttimer(g_timerlist.bondtest);
    apply_bones();
    stoptimer(g_timerlist.bondtest);
    apply_items();
    apply_wheelbodies();
    stoptimer(g_timerlist.bondloopcopy);
    local_60 = (int *)((int)piVar18 + 1);
  } while ((int)local_60 < 0x20);
  starttimer(g_timerlist.bondsetupcopy);
  copy_back();
  stoptimer(g_timerlist.bondsetupcopy);
  stoptimer(g_timerlist.bondsim);
  /* The contact bonds of this tick end here: the list goes back to the static bonds. */
  g_numofdynamicbonds = g_numofbonds;
  g_numofbonds = g_numofstaticbonds;
  return;
}
#undef g_human
#undef g_sysinfo
#undef DAT_00460d18
#undef DAT_00603dd0
#undef g_numofstaticbonds
#undef g_numofdynamicbonds
#undef g_yaxis
#undef g_zaxis
#undef g_timer
#undef g_object
#undef DAT_00461dd4
#undef DAT_00461de0
#undef DAT_00461de4
#undef DAT_00461de8
#undef g_item
#undef DAT_00461df0
#undef DAT_00461df4
#undef DAT_00461df8
#undef DAT_00461e04
#undef g_particle
#undef DAT_00461e10
#undef DAT_00461e14
#undef DAT_00461e18
#undef DAT_00461e20
#undef DAT_00461e24
#undef DAT_00461e28
#undef DAT_00583dd0
#undef DAT_00583dd4
#undef DAT_00583dd8
#undef DAT_00583de0
#undef DAT_00583de4
#undef g_bondopt
#undef g_timerlist
#undef DAT_00583de8
#undef g_numofbonds
#undef g_bond
#undef g_area_sector
#undef geteulerangles2
#undef stoptimer
#undef rotatevector
#undef rotateorientation
#undef bond_partpart
#undef bond_bodybody
#undef multiply3x3matrices
#undef objectwheelsimulation2
#undef calculatefriction
#undef objectenginesimulation2
#undef __security_check_cookie
#undef _CIsqrt
#undef auStack_e4
#undef local_d4
#undef local_d0
#undef local_cc
#undef local_c8
#undef local_c4
#undef local_c0
#undef local_bc
#undef local_b8
#undef local_b4
#undef local_b0
#undef local_ac
#undef local_a8
#undef local_a4
#undef local_a0
#undef local_9c
#undef local_98
#undef local_94
#undef local_90
#undef local_8c
#undef local_88
#undef local_84
#undef local_80
#undef local_7c
#undef local_78
#undef local_74
#undef local_70
#undef local_6c
#undef local_68
#undef local_64
#undef local_60
#undef local_5c
#undef local_58
#undef local_54
#undef local_50
#undef local_4c
#undef local_48
#undef local_44
#undef local_40
#undef local_3c
#undef local_38
#undef local_34
#undef local_30
#undef local_2c
#undef local_28
#undef local_24
#undef local_20
#undef local_1c
#undef local_18
#undef local_14
#undef local_10
#undef local_c
#undef local_8
#undef local_4

