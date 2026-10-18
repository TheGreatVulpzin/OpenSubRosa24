/* bondsimulation2: the bond solver of the physics, once for each tick.
 *
 * Each section has a profiler timer with its name. bondsetup prepares one BondOpt record for
 * each bond (setup_partpart and the others). bondsetupcopy copies the velocities into the work
 * arrays (setup_copy). The loop then does a number of solver passes over all the bonds, and the
 * end copies the velocities back. The parts from the loop on are not yet clean C.
 */
#include "lift_support.h"
#include "lift_os.h"

/* The type of the bond that pfVar33 points into (pfVar33 = &bond.friction_dir.y, bond + 0x78).
 * An int load: the lifted C read it as a float, a denormal on the x87 for each test. */
#define BOND_KIND(p) (*(int *)((char *)(p) - 0x78))

#define g_human (*(Human (*)[512])SR_VA(0x19cbf80))
#define g_sysinfo (*(SysInfo *)SR_VA(0x956d2e0))
#define DAT_00460d18 (*(uint *)SR_VA(0x460d18))
#define DAT_00603dd0 (*(int * *)SR_VA(0x603dd0))
#define DAT_019cbf68 (*(int *)SR_VA(0x19cbf68))
#define DAT_14022d24 (*(undefined4 *)SR_VA(0x14022d24))
#define DAT_020a5a30 (*(undefined *)SR_VA(0x20a5a30))
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
#define g_particle (*(Particle (*)[4096])SR_VA(0xb7e7200))
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
#define _DAT_020a5a30 (*(int *)SR_VA(0x20a5a30))
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
  b->mass_ratio[0] = inv * m_item;
  b->mass_ratio[1] = (float)(m_bone * inv);
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
     * rotateorientation turns, as in the 024d. */
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
      w->torque = bone->torque;
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

/* bondhuman: a human who stands upright on the ground keeps the balance. Bone 0 gets a turn
 * that brings it back to the vertical (its length is at most a limit from the stance and the
 * body strength), a
 * turn toward the direction that the human faces (human.axis), and less of the turn it has
 * now. The sum, with a limit, goes into BondBone.torque. */
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
    w->torque.x = (float)(w->torque.x + a.x);
    w->torque.y = (float)(a.y + w->torque.y);
    w->torque.z = (float)(a.z + w->torque.z);
  }
  stoptimer(g_timerlist.bondhuman);
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
    iVar30 = g_timerlist.bondwheelsim;
    local_50 = (float *)&g_timer[g_timerlist.bondwheelsim].start_lo;
    uVar13 = rdtsc();
    LF_SET_BITS(*local_50, (int)uVar13);
    g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
    objectwheelsimulation2();
    stoptimer(g_timerlist.bondwheelsim);
    local_c0 = 0;
    if (0 < g_numofbonds) {
      pfVar33 = &g_bond[0].friction_dir.y;
      pfVar21 = &g_bondopt.arm_a.z;
      do {
        iVar30 = g_timerlist.bondpartpart;
        if (((uint)local_60 & (uint)(ushort)*(short *)((int)pfVar21 + -0x1a)) ==
            (uint)(ushort)*(short *)((int)pfVar21 + -0x1a)) {
          if (BOND_KIND(pfVar33) == 0x0U) {
            local_50 = (float *)&g_timer[g_timerlist.bondpartpart].start_lo;
            uVar13 = rdtsc();
            LF_SET_BITS(*local_50, (int)uVar13);
            g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
            bond_partpart(local_c0);
            stoptimer(g_timerlist.bondpartpart);
          }
          if (BOND_KIND(pfVar33) == 0x1U) {
            LF_COPY(fVar6, pfVar33[-0x19]);
            fVar7 = (float)(pfVar33[-6] * pfVar33[-3]);
            pvVar1 = (vec3 *)(pfVar33 + -1);
            fVar8 = -pfVar33[-2];
            LF_COPY(fVar5, pfVar33[-2]);
            local_5c = (float)((float)(fVar8 * LF_BITS_F((&DAT_00583dd0)[((int)LF_UINT(fVar6)) * 8]) + (float)(fVar7 * pvVar1->x)) +
                       fVar5 * pfVar33[0x11]);
            local_58 = (float)(pfVar33[0x12] * fVar5 +
                       ((float)(LF_BITS_F((&DAT_00583dd4)[((int)LF_UINT(fVar6)) * 8]) * fVar8 + (float)(fVar7 * *pfVar33))));
            local_54 = (float)(fVar5 * pfVar33[0x13] +
                       ((float)(fVar8 * LF_BITS_F((&DAT_00583dd8)[((int)LF_UINT(fVar6)) * 8]) + (float)(fVar7 * pfVar33[1]))));
            local_50 = ((float *)LF_BITS_U(((float)(pfVar33[1] * local_54 + (pvVar1->x * local_5c + local_58 * *pfVar33)))))
            ;
            if (0.0 < LF_BITS_F(local_50)) {
              calculatefriction(&local_5c,(float *)pvVar1,pfVar33[2]);
              *(float *)&((&DAT_00583de0)[((int)LF_UINT(fVar6)) * 8]) = (float)(LF_BITS_F((&DAT_00583de0)[((int)LF_UINT(fVar6)) * 8]) + local_5c);
              *(float *)&((&DAT_00583de4)[((int)LF_UINT(fVar6)) * 8]) = (float)(LF_BITS_F((&DAT_00583de4)[((int)LF_UINT(fVar6)) * 8]) + local_58);
              *(float *)&((&DAT_00583de8)[((int)LF_UINT(fVar6)) * 8]) = (float)(LF_BITS_F((&DAT_00583de8)[((int)LF_UINT(fVar6)) * 8]) + local_54);
            }
          }
          if (BOND_KIND(pfVar33) == 0x2U) {
            LF_COPY(fVar6, pfVar33[-0x19]);
            fVar7 = (float)(pfVar33[-6] * pfVar33[-3]);
            local_84 = ((int *)LF_UINT(pfVar33[-0x18]));
            local_9c = (float *)((int)local_84 * 0x20);
            fVar8 = -pfVar33[-2];
            LF_COPY(fVar5, pfVar33[-2]);
            local_7c = (float)(fVar5 * local_9c[(0x160f74 + LF_RELOC / 4)] +
                       ((float)(fVar8 * LF_BITS_F((&DAT_00583dd0)[((int)LF_UINT(fVar6)) * 8]) +
                       (float)(fVar7 * ((vec3 *)(pfVar33 + -1))->x))));
            local_78 = (float)(local_9c[(0x160f75 + LF_RELOC / 4)] * fVar5 +
                       ((float)(LF_BITS_F((&DAT_00583dd4)[((int)LF_UINT(fVar6)) * 8]) * fVar8 + (float)(fVar7 * *pfVar33))));
            local_74 = (float)(fVar5 * local_9c[(0x160f76 + LF_RELOC / 4)] +
                       ((float)(fVar8 * LF_BITS_F((&DAT_00583dd8)[((int)LF_UINT(fVar6)) * 8]) + (float)(fVar7 * pfVar33[1]))));
            local_50 = ((float *)LF_BITS_U(((float)(local_74 * local_74 + (local_7c * local_7c + local_78 * local_78)))));
            local_bc = ((int)LF_UINT(fVar6));
            dVar39 = _CIsqrt((double)LF_BITS_F(local_50));
            local_70 = (float)dVar39;
            if (local_70 <= 1.52587890625E-5) {
              local_74 = 0.0;
              local_7c = 0.0;
              local_78 = 0.0;
            }
            else {
              fVar5 = (float)(1.0 / local_70);
              local_7c = fVar5 * local_7c;
              local_78 = (float)(fVar5 * local_78);
              local_74 = fVar5 * local_74;
            }
            local_88 = &g_particle[0].mass + (int)local_84 * 0x3c;
            local_70 = (float)(local_70 /
                       ((&g_particle[0].mass)[(int)local_84 * 0x3c] +
                       (&g_particle[0].mass)[local_bc * 0x3c]));
            local_80 = &g_particle[0].mass + local_bc * 0x3c;
            local_50 = ((float *)LF_BITS_U(((float)(pfVar33[1] * local_74 +
                                (local_7c * ((vec3 *)(pfVar33 + -1))->x + local_78 * *pfVar33)))));
            if (0.0 < LF_BITS_F(local_50)) {
              calculatefriction(&local_7c,(float *)(pfVar33 + -1),pfVar33[2]);
              fVar5 = (float)(local_70 * *local_88);
              *(float *)&((&DAT_00583de0)[((int)LF_UINT(fVar6)) * 8]) =
                   (float)(LF_BITS_F((&DAT_00583de0)[((int)LF_UINT(fVar6)) * 8]) + fVar5 * local_7c);
              *(float *)&((&DAT_00583de4)[((int)LF_UINT(fVar6)) * 8]) =
                   (float)(LF_BITS_F((&DAT_00583de4)[((int)LF_UINT(fVar6)) * 8]) + local_78 * fVar5);
              *(float *)&((&DAT_00583de8)[((int)LF_UINT(fVar6)) * 8]) =
                   (float)(LF_BITS_F((&DAT_00583de8)[((int)LF_UINT(fVar6)) * 8]) + local_74 * fVar5);
              local_50 = ((float *)LF_BITS_U(((float)(-local_70 * *local_80))));
              local_9c[(0x160f78 + LF_RELOC / 4)] = (float)(local_9c[(0x160f78 + LF_RELOC / 4)] + LF_BITS_F(local_50) * local_7c);
              local_9c[(0x160f79 + LF_RELOC / 4)] = (float)(local_9c[(0x160f79 + LF_RELOC / 4)] + LF_BITS_F(local_50) * local_78);
              local_9c[(0x160f7a + LF_RELOC / 4)] = (float)(LF_BITS_F(local_50) * local_74 + local_9c[(0x160f7a + LF_RELOC / 4)]);
            }
          }
          if (BOND_KIND(pfVar33) == 0x3U) {
            LF_COPY(fVar5, pfVar33[-0x19]);
            fVar6 = (float)(pfVar33[-6] * pfVar33[-3]);
            local_84 = ((int *)LF_UINT(pfVar33[-0x18]));
            local_9c = (float *)((int)local_84 * 0x20);
            local_80 = ((float *)LF_UINT(pfVar33[-0x17]));
            local_a0 = ((int *)LF_UINT(pfVar33[-0x16]));
            fVar7 = -pfVar33[-2];
            fVar8 = (float)(pfVar33[-0xb] * pfVar33[-2]);
            local_88 = (float *)((int)local_80 * 0x20);
            fVar9 = (float)(pfVar33[-10] * pfVar33[-2]);
            fVar10 = (float)(pfVar33[-9] * pfVar33[-2]);
            local_50 = (float *)((int)local_a0 * 0x20);
            local_5c = (float)((float)(fVar9 * local_88[(0x160f74 + LF_RELOC / 4)] +
                       ((float)((float)((float)(fVar6 * ((vec3 *)(pfVar33 + -1))->x) +
                       fVar7 * LF_BITS_F((&DAT_00583dd0)[((int)LF_UINT(fVar5)) * 8])) + fVar8 * local_9c[(0x160f74 + LF_RELOC / 4)]))) +
                       fVar10 * local_50[(0x160f74 + LF_RELOC / 4)]);
            local_58 = (float)(local_50[(0x160f75 + LF_RELOC / 4)] * fVar10 +
                       ((float)(local_88[(0x160f75 + LF_RELOC / 4)] * fVar9 +
                       ((float)(local_9c[(0x160f75 + LF_RELOC / 4)] * fVar8 +
                       ((float)(LF_BITS_F((&DAT_00583dd4)[((int)LF_UINT(fVar5)) * 8]) * fVar7 + (float)(fVar6 * *pfVar33))))))));
            local_54 = (float)(fVar10 * local_50[(0x160f76 + LF_RELOC / 4)] +
                       ((float)(fVar9 * local_88[(0x160f76 + LF_RELOC / 4)] +
                       ((float)(fVar8 * local_9c[(0x160f76 + LF_RELOC / 4)] +
                       ((float)(fVar7 * LF_BITS_F((&DAT_00583dd8)[((int)LF_UINT(fVar5)) * 8]) + (float)(fVar6 * pfVar33[1]))))))));
            local_98 = (float)(local_58 * local_58 + local_5c * local_5c + local_54 * local_54);
            local_bc = ((int)LF_UINT(fVar5));
            dVar39 = _CIsqrt((double)local_98);
            fVar6 = (float)(1.0 / (float)dVar39);
            local_5c = fVar6 * local_5c;
            local_58 = (float)(local_58 * fVar6);
            local_54 = (float)(fVar6 * local_54);
            local_98 = (&g_particle[0].mass)[local_bc * 0x3c];
            local_84 = ((int *)LF_BITS_U(((float)((&g_particle[0].mass)[(int)local_a0 * 0x3c] * pfVar33[-9] +
                              ((&g_particle[0].mass)[(int)local_80 * 0x3c] * pfVar33[-10] +
                              (&g_particle[0].mass)[(int)local_84 * 0x3c] * pfVar33[-0xb])))));
            local_70 = (float)((float)dVar39 / (LF_BITS_F(local_84) + local_98));
            local_80 = ((float *)LF_BITS_U(((float)(pfVar33[1] * local_54 +
                                (((vec3 *)(pfVar33 + -1))->x * local_5c + local_58 * *pfVar33)))));
            if (0.0 < LF_BITS_F(local_80)) {
              calculatefriction(&local_5c,(float *)(pfVar33 + -1),pfVar33[2]);
              fVar6 = local_70 * LF_BITS_F(local_84);
              *(float *)&((&DAT_00583de0)[((int)LF_UINT(fVar5)) * 8]) =
                   (float)(LF_BITS_F((&DAT_00583de0)[((int)LF_UINT(fVar5)) * 8]) + fVar6 * local_5c);
              *(float *)&((&DAT_00583de4)[((int)LF_UINT(fVar5)) * 8]) =
                   (float)(local_58 * fVar6 + LF_BITS_F((&DAT_00583de4)[((int)LF_UINT(fVar5)) * 8]));
              *(float *)&((&DAT_00583de8)[((int)LF_UINT(fVar5)) * 8]) =
                   (float)(LF_BITS_F((&DAT_00583de8)[((int)LF_UINT(fVar5)) * 8]) + local_54 * fVar6);
              fVar5 = -local_70 * local_98;
              local_80 = ((float *)LF_BITS_U(((float)(pfVar33[-0xb] * fVar5))));
              local_9c[(0x160f78 + LF_RELOC / 4)] = (float)(local_9c[(0x160f78 + LF_RELOC / 4)] + LF_BITS_F(local_80) * local_5c);
              local_9c[(0x160f79 + LF_RELOC / 4)] = (float)(local_58 * LF_BITS_F(local_80) + local_9c[(0x160f79 + LF_RELOC / 4)]);
              local_9c[(0x160f7a + LF_RELOC / 4)] = (float)(LF_BITS_F(local_80) * local_54 + local_9c[(0x160f7a + LF_RELOC / 4)]);
              local_9c = ((float *)LF_BITS_U(((float)(pfVar33[-10] * fVar5))));
              local_88[(0x160f78 + LF_RELOC / 4)] = (float)(local_88[(0x160f78 + LF_RELOC / 4)] + LF_BITS_F(local_9c) * local_5c);
              local_88[(0x160f79 + LF_RELOC / 4)] = (float)(local_58 * LF_BITS_F(local_9c) + local_88[(0x160f79 + LF_RELOC / 4)]);
              local_88[(0x160f7a + LF_RELOC / 4)] = (float)(LF_BITS_F(local_9c) * local_54 + local_88[(0x160f7a + LF_RELOC / 4)]);
              local_88 = ((float *)LF_BITS_U(((float)(fVar5 * pfVar33[-9]))));
              local_50[(0x160f78 + LF_RELOC / 4)] = (float)(local_50[(0x160f78 + LF_RELOC / 4)] + LF_BITS_F(local_88) * local_5c);
              local_50[(0x160f79 + LF_RELOC / 4)] = (float)(local_50[(0x160f79 + LF_RELOC / 4)] + LF_BITS_F(local_88) * local_58);
              local_50[(0x160f7a + LF_RELOC / 4)] = (float)(LF_BITS_F(local_88) * local_54 + local_50[(0x160f7a + LF_RELOC / 4)]);
            }
          }
          if (BOND_KIND(pfVar33) == 0x4U) {
            local_98 = pfVar33[6];
            fVar6 = (float)(pfVar33[-6] * pfVar33[-3]);
            local_bc = ((int)LF_UINT(pfVar33[7]));
            local_9c = (float *)(((int)LF_UINT(local_98)) * 0x810 + local_bc * 0x80);
            LF_COPY(fVar5, pfVar33[-0x18]);
            local_80 = ((float *)LF_UINT(pfVar33[-0x17]));
            local_a0 = ((int *)LF_UINT(pfVar33[-0x16]));
            local_88 = (float *)((int)local_80 * 0x20);
            fVar7 = -pfVar33[-2];
            fVar8 = (float)(pfVar33[-0xb] * pfVar33[-2]);
            fVar9 = (float)(pfVar33[-10] * pfVar33[-2]);
            local_50 = (float *)((int)local_a0 * 0x20);
            fVar10 = (float)(pfVar33[-9] * pfVar33[-2]);
            local_5c = (float)((float)((float)(fVar8 * LF_BITS_F((&DAT_00583dd0)[((int)LF_UINT(fVar5)) * 8]) +
                       ((float)(fVar6 * ((vec3 *)(pfVar33 + -1))->x + fVar7 * local_9c[(0x118778 + LF_RELOC / 4)]))) +
                       fVar9 * local_88[(0x160f74 + LF_RELOC / 4)]) + fVar10 * local_50[(0x160f74 + LF_RELOC / 4)]);
            local_58 = (float)(local_50[(0x160f75 + LF_RELOC / 4)] * fVar10 +
                       ((float)(local_88[(0x160f75 + LF_RELOC / 4)] * fVar9 +
                       ((float)(LF_BITS_F((&DAT_00583dd4)[((int)LF_UINT(fVar5)) * 8]) * fVar8 +
                       ((float)(local_9c[(0x118779 + LF_RELOC / 4)] * fVar7 + (float)(*pfVar33 * fVar6))))))));
            local_54 = (float)(fVar10 * local_50[(0x160f76 + LF_RELOC / 4)] +
                       ((float)(fVar9 * local_88[(0x160f76 + LF_RELOC / 4)] +
                       ((float)(fVar8 * LF_BITS_F((&DAT_00583dd8)[((int)LF_UINT(fVar5)) * 8]) +
                       ((float)(fVar7 * local_9c[(0x11877a + LF_RELOC / 4)] + (float)(fVar6 * pfVar33[1]))))))));
            local_70 = (float)(local_58 * local_58 + local_5c * local_5c + local_54 * local_54);
            local_84 = ((int *)LF_UINT(fVar5));
            dVar39 = _CIsqrt((double)local_70);
            local_70 = (float)dVar39;
            if (local_70 <= 1.52587890625E-5) {
              local_54 = 0.0;
              local_5c = 0.0;
              local_58 = 0.0;
            }
            else {
              fVar6 = (float)(1.0 / local_70);
              local_5c = fVar6 * local_5c;
              local_58 = (float)(local_58 * fVar6);
              local_54 = fVar6 * local_54;
            }
            local_98 = *(float *)(((int)LF_UINT(local_98)) * 0x2b64 + ((int)&g_human[0].lf_raw[0x154]) + local_bc * 0xe0);
            local_84 = ((int *)LF_BITS_U(((float)((&g_particle[0].mass)[(int)local_a0 * 0x3c] * pfVar33[-9] +
                              ((&g_particle[0].mass)[(int)local_80 * 0x3c] * pfVar33[-10] +
                              (&g_particle[0].mass)[(int)local_84 * 0x3c] * pfVar33[-0xb])))));
            local_70 = (float)(local_70 / (LF_BITS_F(local_84) + local_98));
            local_80 = ((float *)LF_BITS_U(((float)(pfVar33[1] * local_54 +
                                (((vec3 *)(pfVar33 + -1))->x * local_5c + local_58 * *pfVar33)))));
            if (0.0 < LF_BITS_F(local_80)) {
              calculatefriction(&local_5c,(float *)(pfVar33 + -1),pfVar33[2]);
              local_80 = ((float *)LF_BITS_U((local_70 * LF_BITS_F(local_84))));
              local_9c[(0x11877c + LF_RELOC / 4)] = (float)(local_9c[(0x11877c + LF_RELOC / 4)] + LF_BITS_F(local_80) * local_5c);
              local_9c[(0x11877d + LF_RELOC / 4)] = (float)(local_58 * LF_BITS_F(local_80) + local_9c[(0x11877d + LF_RELOC / 4)]);
              local_9c[(0x11877e + LF_RELOC / 4)] = (float)(local_9c[(0x11877e + LF_RELOC / 4)] + local_54 * LF_BITS_F(local_80));
              fVar6 = -local_70 * local_98;
              fVar7 = (float)(pfVar33[-0xb] * fVar6);
              *(float *)&((&DAT_00583de0)[((int)LF_UINT(fVar5)) * 8]) =
                   (float)(LF_BITS_F((&DAT_00583de0)[((int)LF_UINT(fVar5)) * 8]) + fVar7 * local_5c);
              *(float *)&((&DAT_00583de4)[((int)LF_UINT(fVar5)) * 8]) =
                   (float)(local_58 * fVar7 + LF_BITS_F((&DAT_00583de4)[((int)LF_UINT(fVar5)) * 8]));
              *(float *)&((&DAT_00583de8)[((int)LF_UINT(fVar5)) * 8]) =
                   (float)(fVar7 * local_54 + LF_BITS_F((&DAT_00583de8)[((int)LF_UINT(fVar5)) * 8]));
              local_9c = ((float *)LF_BITS_U(((float)(pfVar33[-10] * fVar6))));
              local_88[(0x160f78 + LF_RELOC / 4)] = (float)(local_88[(0x160f78 + LF_RELOC / 4)] + LF_BITS_F(local_9c) * local_5c);
              local_88[(0x160f79 + LF_RELOC / 4)] = (float)(local_58 * LF_BITS_F(local_9c) + local_88[(0x160f79 + LF_RELOC / 4)]);
              local_88[(0x160f7a + LF_RELOC / 4)] = (float)(LF_BITS_F(local_9c) * local_54 + local_88[(0x160f7a + LF_RELOC / 4)]);
              local_88 = ((float *)LF_BITS_U(((float)(fVar6 * pfVar33[-9]))));
              local_50[(0x160f78 + LF_RELOC / 4)] = (float)(local_50[(0x160f78 + LF_RELOC / 4)] + LF_BITS_F(local_88) * local_5c);
              local_50[(0x160f79 + LF_RELOC / 4)] = (float)(local_50[(0x160f79 + LF_RELOC / 4)] + LF_BITS_F(local_88) * local_58);
              local_50[(0x160f7a + LF_RELOC / 4)] = (float)(LF_BITS_F(local_88) * local_54 + local_50[(0x160f7a + LF_RELOC / 4)]);
            }
          }
          iVar30 = g_timerlist.bondwheelcollevel;
          if (BOND_KIND(pfVar33) == 0x5U) {
            /* Type 5: a wheel of an object on the ground. The object and wheel indexes are
             * ints: as floats they are denormals, and each x87 use took about 9000 cycles
             * for each bond. The float math is the lifted one, in the same order. */
            Bond *bond = (Bond *)((char *)pfVar33 - 0x78);
            int obj = bond->owner;     /* type 5: object index */
            int wheel = bond->bone_a;  /* type 5: wheel index (0x98 bytes each, at object+0x5f0c) */
            float *w = (float *)(&g_object[obj].lf_raw[0x5f0c] + wheel * 0x98);
            float k, neg, load, proj;
            uVar13 = rdtsc();
            g_timer[g_timerlist.bondwheelcollevel].start_lo = (int)uVar13;
            g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
            k = (float)(bond->rest_length * bond->stiffness);
            neg = -bond->damping;
            load = (float)(w[3] * w[0] * bond->damping);
            local_5c = (float)((float)(k * bond->friction_dir.x + neg * w[9]) + load * pfVar33[0xe]);
            local_58 = (float)(load * pfVar33[0xf] + ((float)(w[10] * neg + (float)(bond->friction_dir.y * k))));
            local_54 = (float)(load * pfVar33[0x10] + ((float)(neg * w[11] + (float)(k * bond->friction_dir.z))));
            proj = (float)(bond->friction_dir.z * local_54 +
                           (bond->friction_dir.x * local_5c + local_58 * bond->friction_dir.y));
            local_50 = (float *)LF_UINT(proj);
            if (0.0 < proj) {
              fVar35 = (float10)calculatefriction(&local_5c, &bond->friction_dir.x, pfVar33[2]);
              w[20] = (float)(fVar35 + (float10)w[20]);
              w[12] = (float)(local_5c + w[12]);
              w[13] = (float)(local_58 + w[13]);
              w[14] = (float)(local_54 + w[14]);
              proj = (float)(((float)(local_54 * pfVar33[0x10] + (pfVar33[0xe] * local_5c + pfVar33[0xf] * local_58))) *
                             w[0]);
              local_50 = (float *)LF_UINT(proj);
              w[2] = (float)(w[2] - proj);
            }
            stoptimer(g_timerlist.bondwheelcollevel);
          }
          iVar30 = g_timerlist.bondbodylevel;
          if (BOND_KIND(pfVar33) == 0x7U) {
            uVar13 = rdtsc();
            g_timer[g_timerlist.bondbodylevel].start_lo = (int)uVar13;
            g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
            LF_COPY(fVar5, pfVar33[6]);
            LF_COPY(fVar6, pfVar33[7]);
            fVar7 = (float)(pfVar33[-6] * pfVar33[-3]);
            local_6c = (float)(((float)(LF_BITS_F((&DAT_00461e18)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) *
                        pfVar33[-0x13] -
                       pfVar33[-0x12] *
                       LF_BITS_F((&DAT_00461e14)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]))) +
                       LF_BITS_F((&DAT_00461de0)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]));
            local_68 = (float)(LF_BITS_F((&DAT_00461de4)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) +
                       ((float)(pfVar33[-0x12] *
                        LF_BITS_F((&DAT_00461e10)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) -
                       pfVar33[-0x14] *
                       LF_BITS_F((&DAT_00461e18)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]))));
            local_64 = (float)(LF_BITS_F((&DAT_00461de8)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) +
                       ((float)(pfVar33[-0x14] *
                        LF_BITS_F((&DAT_00461e14)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) -
                       LF_BITS_F((&DAT_00461e10)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) *
                       pfVar33[-0x13])));
            fVar8 = -pfVar33[-2];
            local_5c = (float)(fVar7 * ((vec3 *)(pfVar33 + -1))->x + fVar8 * local_6c);
            local_58 = (float)(fVar8 * local_68 + (float)(fVar7 * *pfVar33));
            local_54 = (float)(fVar8 * local_64 + (float)(fVar7 * pfVar33[1]));
            local_50 = ((float *)LF_BITS_U(((float)(pfVar33[1] * local_54 +
                                (((vec3 *)(pfVar33 + -1))->x * local_5c + local_58 * *pfVar33)))));
            if (0.0 < LF_BITS_F(local_50)) {
              calculatefriction(&local_5c,pfVar33 + -1,pfVar33[2]);
              *(float *)&((&DAT_00461df0)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) =
                   (float)(local_5c + LF_BITS_F((&DAT_00461df0)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]));
              *(float *)&((&DAT_00461df4)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) =
                   (float)(local_58 + LF_BITS_F((&DAT_00461df4)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]));
              *(float *)&((&DAT_00461df8)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) =
                   (float)(local_54 + LF_BITS_F((&DAT_00461df8)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]));
              local_6c = (float)(pfVar33[-0x12] * local_58 - local_54 * pfVar33[-0x13]);
              local_68 = (float)(local_54 * pfVar33[-0x14] - pfVar33[-0x12] * local_5c);
              local_64 = (float)(pfVar33[-0x13] * local_5c - local_58 * pfVar33[-0x14]);
              *(float *)&((&DAT_00461e20)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) =
                   (float)(local_6c + LF_BITS_F((&DAT_00461e20)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]));
              *(float *)&((&DAT_00461e24)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) =
                   (float)(LF_BITS_F((&DAT_00461e24)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) + local_68);
              *(float *)&((&DAT_00461e28)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) =
                   (float)(LF_BITS_F((&DAT_00461e28)[((int)LF_UINT(fVar6)) * 0x20 + ((int)LF_UINT(fVar5)) * 0x204]) + local_64);
            }
            stoptimer(g_timerlist.bondbodylevel);
          }
          iVar30 = g_timerlist.bondbodybody;
          if (BOND_KIND(pfVar33) == 0x8U) {
            local_50 = (float *)&g_timer[g_timerlist.bondbodybody].start_lo;
            uVar13 = rdtsc();
            LF_SET_BITS(*local_50, (int)uVar13);
            g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
            bond_bodybody(local_c0);
            stoptimer(g_timerlist.bondbodybody);
          }
          iVar30 = g_timerlist.bondbodyrotbody;
          if (BOND_KIND(pfVar33) == 0x9U) {
            uVar13 = rdtsc();
            g_timer[g_timerlist.bondbodyrotbody].start_lo = (int)uVar13;
            g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
            uVar25 = (uint)(ushort)((BondOpt *)(pfVar21 + -8))->a;
            fVar6 = -pfVar21[7];
            uVar32 = (uint)(ushort)*(short *)((int)pfVar21 + -0x1e);
            uVar28 = (uint)*(ushort *)(pfVar21 + -7);
            LF_COPY(fVar5, pfVar21[7]);
            local_5c = (float)((float)(fVar6 * LF_BITS_F((&DAT_00461e10)[uVar25 * 0x204 + uVar32 * 0x20]) + pfVar21[-6])
                       + fVar5 * LF_BITS_F((&DAT_00461e10)[uVar28 * 0x20 + uVar25 * 0x204]));
            local_58 = (float)(LF_BITS_F((&DAT_00461e14)[uVar28 * 0x20 + uVar25 * 0x204]) * fVar5 +
                       ((float)(LF_BITS_F((&DAT_00461e14)[uVar25 * 0x204 + uVar32 * 0x20]) * fVar6 + pfVar21[-5])));
            local_54 = (float)(fVar5 * LF_BITS_F((&DAT_00461e18)[uVar28 * 0x20 + uVar25 * 0x204]) +
                       ((float)(fVar6 * LF_BITS_F((&DAT_00461e18)[uVar25 * 0x204 + uVar32 * 0x20]) + pfVar21[-4])));
            LF_COPY(fVar5, pfVar21[5]);
            *(float *)&((&DAT_00461e20)[uVar25 * 0x204 + uVar32 * 0x20]) =
                 (float)(LF_BITS_F((&DAT_00461e20)[uVar25 * 0x204 + uVar32 * 0x20]) + fVar5 * local_5c);
            *(float *)&((&DAT_00461e24)[uVar25 * 0x204 + uVar32 * 0x20]) =
                 (float)(local_58 * fVar5 + LF_BITS_F((&DAT_00461e24)[uVar25 * 0x204 + uVar32 * 0x20]));
            *(float *)&((&DAT_00461e28)[uVar25 * 0x204 + uVar32 * 0x20]) =
                 (float)(LF_BITS_F((&DAT_00461e28)[uVar25 * 0x204 + uVar32 * 0x20]) + local_54 * fVar5);
            local_50 = ((float *)LF_BITS_U(-pfVar21[6]));
            *(float *)&((&DAT_00461e20)[uVar28 * 0x20 + uVar25 * 0x204]) =
                 (float)(LF_BITS_F((&DAT_00461e20)[uVar28 * 0x20 + uVar25 * 0x204]) + LF_BITS_F(local_50) * local_5c)
            ;
            *(float *)&((&DAT_00461e24)[uVar28 * 0x20 + uVar25 * 0x204]) =
                 (float)(local_58 * LF_BITS_F(local_50) + LF_BITS_F((&DAT_00461e24)[uVar28 * 0x20 + uVar25 * 0x204]))
            ;
            *(float *)&((&DAT_00461e28)[uVar28 * 0x20 + uVar25 * 0x204]) =
                 (float)(local_54 * LF_BITS_F(local_50) + LF_BITS_F((&DAT_00461e28)[uVar28 * 0x20 + uVar25 * 0x204]))
            ;
            stoptimer(g_timerlist.bondbodyrotbody);
          }
          iVar30 = g_timerlist.bondbodyitem;
          if (BOND_KIND(pfVar33) == 0xaU) {
            uVar13 = rdtsc();
            g_timer[g_timerlist.bondbodyitem].start_lo = (int)uVar13;
            g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
            LF_COPY(fVar6, pfVar33[6]);
            local_94 = pfVar21[-2];
            LF_COPY(fVar7, pfVar33[7]);
            local_90 = pfVar21[-1];
            local_8c = *pfVar21;
            local_cc = pfVar21[1];
            local_c8 = pfVar21[2];
            local_c4 = pfVar21[3];
            LF_COPY(fVar8, pfVar33[8]);
            LF_COPY(fVar5, pfVar33[-2]);
            fVar9 = -pfVar33[-2];
            local_5c = (float)(fVar9 * ((float)(g_item[((int)LF_UINT(fVar8))].vel.x +
                               ((float)(local_c8 * g_item[((int)LF_UINT(fVar8))].torque.z -
                               local_c4 * g_item[((int)LF_UINT(fVar8))].torque.y)))) +
                       ((float)(fVar5 * ((float)(((float)(local_90 *
                                 LF_BITS_F((&DAT_00461e18)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) -
                                local_8c *
                                LF_BITS_F((&DAT_00461e14)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]))) +
                               LF_BITS_F((&DAT_00461de0)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]))) +
                       pfVar21[-6])));
            local_58 = (float)(fVar9 * ((float)(g_item[((int)LF_UINT(fVar8))].vel.y +
                               ((float)(g_item[((int)LF_UINT(fVar8))].torque.x * local_c4 -
                               local_cc * g_item[((int)LF_UINT(fVar8))].torque.z)))) +
                       ((float)(fVar5 * ((float)(LF_BITS_F((&DAT_00461de4)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) +
                               ((float)(LF_BITS_F((&DAT_00461e10)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) *
                                local_8c -
                               local_94 *
                               LF_BITS_F((&DAT_00461e18)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]))))) +
                       pfVar21[-5])));
            local_54 = (float)(fVar9 * ((float)(g_item[((int)LF_UINT(fVar8))].vel.z +
                               ((float)(g_item[((int)LF_UINT(fVar8))].torque.y * local_cc -
                               local_c8 * g_item[((int)LF_UINT(fVar8))].torque.x)))) +
                       ((float)(fVar5 * ((float)(LF_BITS_F((&DAT_00461de8)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) +
                               ((float)(local_94 *
                                LF_BITS_F((&DAT_00461e14)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) -
                               LF_BITS_F((&DAT_00461e10)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) *
                               local_90)))) + pfVar21[-4])));
            local_d4 = pfVar33[-0xb];
            local_d0 = pfVar33[-10];
            local_88 = ((float *)LF_BITS_U(-local_d4));
            *(float *)&((&DAT_00461df0)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) =
                 (float)(LF_BITS_F((&DAT_00461df0)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) +
                 local_5c * LF_BITS_F(local_88));
            *(float *)&((&DAT_00461df4)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) =
                 (float)(LF_BITS_F((&DAT_00461df4)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) +
                 local_58 * LF_BITS_F(local_88));
            *(float *)&((&DAT_00461df8)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) =
                 (float)(LF_BITS_F((&DAT_00461df8)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) +
                 local_54 * LF_BITS_F(local_88));
            *(float *)&((&DAT_00461e20)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) =
                 (float)(LF_BITS_F(local_88) * ((float)(local_58 * local_8c - local_90 * local_54)) +
                 LF_BITS_F((&DAT_00461e20)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]));
            *(float *)&((&DAT_00461e24)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) =
                 (float)(LF_BITS_F(local_88) * ((float)(local_94 * local_54 - local_5c * local_8c)) +
                 LF_BITS_F((&DAT_00461e24)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]));
            *(float *)&((&DAT_00461e28)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) =
                 (float)(LF_BITS_F(local_88) * ((float)(local_5c * local_90 - local_58 * local_94)) +
                 LF_BITS_F((&DAT_00461e28)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]));
            g_item[((int)LF_UINT(fVar8))].dv.x =
                 (float)(g_item[((int)LF_UINT(fVar8))].dv.x + local_d0 * local_5c);
            g_item[((int)LF_UINT(fVar8))].dv.y =
                 (float)(local_58 * local_d0 + g_item[((int)LF_UINT(fVar8))].dv.y);
            g_item[((int)LF_UINT(fVar8))].dv.z =
                 (float)(local_54 * local_d0 + g_item[((int)LF_UINT(fVar8))].dv.z);
            local_6c = (float)(local_58 * local_c4 - local_c8 * local_54);
            local_68 = (float)(local_cc * local_54 - local_5c * local_c4);
            local_64 = (float)(local_c8 * local_5c - local_cc * local_58);
            g_item[((int)LF_UINT(fVar8))].dw.x =
                 (float)(local_6c * local_d0 + g_item[((int)LF_UINT(fVar8))].dw.x);
            g_item[((int)LF_UINT(fVar8))].dw.y =
                 (float)(local_68 * local_d0 + g_item[((int)LF_UINT(fVar8))].dw.y);
            g_item[((int)LF_UINT(fVar8))].dw.z =
                 (float)(local_d0 * local_64 + g_item[((int)LF_UINT(fVar8))].dw.z);
            local_50 = local_88;
            stoptimer(g_timerlist.bondbodyitem);
          }
          /* One copy of the type for the next two tests, as the original (a block can change
           * the type). An int: as a float it is a denormal on the x87. */
          kind_copy = BOND_KIND(pfVar33);
          if (kind_copy == 0xb) {
            LF_COPY(fVar7, pfVar33[6]);
            fVar10 = -pfVar21[7];
            LF_COPY(fVar8, pfVar33[7]);
            LF_COPY(fVar9, pfVar33[8]);
            LF_COPY(fVar6, pfVar21[7]);
            local_54 = pfVar21[-2];
            local_5c = (float)(local_54 *
                       ((float)((float)(fVar10 * LF_BITS_F((&DAT_00461e10)[((int)LF_UINT(fVar8)) * 0x20 + ((int)LF_UINT(fVar7)) * 0x204]) +
                        pfVar21[-6]) + fVar6 * g_item[((int)LF_UINT(fVar9))].torque.x)));
            local_58 = (float)(local_54 *
                       ((float)(g_item[((int)LF_UINT(fVar9))].torque.y * fVar6 +
                       ((float)(LF_BITS_F((&DAT_00461e14)[((int)LF_UINT(fVar8)) * 0x20 + ((int)LF_UINT(fVar7)) * 0x204]) * fVar10 +
                       pfVar21[-5])))));
            local_54 = (float)(local_54 *
                       ((float)(fVar6 * g_item[((int)LF_UINT(fVar9))].torque.z +
                       ((float)(fVar10 * LF_BITS_F((&DAT_00461e18)[((int)LF_UINT(fVar8)) * 0x20 + ((int)LF_UINT(fVar7)) * 0x204]) +
                       pfVar21[-4])))));
            LF_COPY(fVar6, pfVar21[5]);
            *(float *)&((&DAT_00461e20)[((int)LF_UINT(fVar8)) * 0x20 + ((int)LF_UINT(fVar7)) * 0x204]) =
                 (float)(LF_BITS_F((&DAT_00461e20)[((int)LF_UINT(fVar8)) * 0x20 + ((int)LF_UINT(fVar7)) * 0x204]) + local_5c * fVar6);
            *(float *)&((&DAT_00461e24)[((int)LF_UINT(fVar8)) * 0x20 + ((int)LF_UINT(fVar7)) * 0x204]) =
                 (float)(LF_BITS_F((&DAT_00461e24)[((int)LF_UINT(fVar8)) * 0x20 + ((int)LF_UINT(fVar7)) * 0x204]) + local_58 * fVar6);
            *(float *)&((&DAT_00461e28)[((int)LF_UINT(fVar8)) * 0x20 + ((int)LF_UINT(fVar7)) * 0x204]) =
                 (float)(LF_BITS_F((&DAT_00461e28)[((int)LF_UINT(fVar8)) * 0x20 + ((int)LF_UINT(fVar7)) * 0x204]) + local_54 * fVar6);
            local_50 = ((float *)LF_BITS_U(-pfVar21[6]));
            g_item[((int)LF_UINT(fVar9))].dw.x =
                 (float)(g_item[((int)LF_UINT(fVar9))].dw.x + LF_BITS_F(local_50) * local_5c);
            g_item[((int)LF_UINT(fVar9))].dw.y =
                 (float)(local_58 * LF_BITS_F(local_50) + g_item[((int)LF_UINT(fVar9))].dw.y);
            g_item[((int)LF_UINT(fVar9))].dw.z =
                 (float)(LF_BITS_F(local_50) * local_54 + g_item[((int)LF_UINT(fVar9))].dw.z);
          }
          if (kind_copy == 0xd) {
            LF_COPY(fVar6, pfVar33[6]);
            fVar8 = (float)(pfVar33[-6] * pfVar33[-3]);
            LF_COPY(fVar7, pfVar33[7]);
            local_84 = ((int *)LF_UINT(pfVar33[9]));
            local_88 = ((float *)LF_UINT(pfVar33[8]));
            fVar9 = -pfVar33[-2];
            local_80 = (float *)((int)local_88 * 0x810 + (int)local_84 * 0x80);
            LF_COPY(fVar5, pfVar33[-2]);
            local_7c = (float)(fVar5 * local_80[(0x118778 + LF_RELOC / 4)] +
                       ((float)(fVar9 * LF_BITS_F((&DAT_00461de0)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) +
                       fVar8 * ((vec3 *)(pfVar33 + -1))->x)));
            local_78 = (float)(local_80[(0x118779 + LF_RELOC / 4)] * fVar5 +
                       ((float)(LF_BITS_F((&DAT_00461de4)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) * fVar9 +
                       (float)(fVar8 * *pfVar33))));
            local_74 = (float)(fVar5 * local_80[(0x11877a + LF_RELOC / 4)] +
                       ((float)(fVar9 * LF_BITS_F((&DAT_00461de8)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) +
                       (float)(fVar8 * pfVar33[1]))));
            local_50 = ((float *)LF_BITS_U(((float)(local_74 * local_74 + (local_7c * local_7c + local_78 * local_78)))));
            local_bc = ((int)LF_UINT(fVar7));
            local_98 = fVar6;
            dVar39 = _CIsqrt((double)LF_BITS_F(local_50));
            local_70 = (float)dVar39;
            if (local_70 <= 1.52587890625E-5) {
              local_74 = 0.0;
              local_7c = 0.0;
              local_78 = 0.0;
            }
            else {
              fVar5 = (float)(1.0 / local_70);
              local_7c = fVar5 * local_7c;
              local_78 = (float)(fVar5 * local_78);
              local_74 = fVar5 * local_74;
            }
            local_50 = (float *)(((int)LF_UINT(local_98)) * 0x2b64 + ((int)&g_human[0].lf_raw[0x154]) + local_bc * 0xe0);
            local_70 = (float)(local_70 /
                       (*(float *)(((int)LF_UINT(local_98)) * 0x2b64 + ((int)&g_human[0].lf_raw[0x154]) + local_bc * 0xe0) +
                       *(float *)((int)local_88 * 0x2b64 + ((int)&g_human[0].lf_raw[0x154]) + (int)local_84 * 0xe0)));
            local_9c = (float *)((int)local_88 * 0x2b64 + ((int)&g_human[0].lf_raw[0x154]) + (int)local_84 * 0xe0);
            local_88 = ((float *)LF_BITS_U(((float)(pfVar33[1] * local_74 +
                                (local_7c * ((vec3 *)(pfVar33 + -1))->x + local_78 * *pfVar33)))));
            if (0.0 < LF_BITS_F(local_88)) {
              calculatefriction(&local_7c,(float *)(pfVar33 + -1),pfVar33[2]);
              local_88 = ((float *)LF_BITS_U(((float)(local_70 * *local_9c))));
              *(float *)&((&DAT_00461df0)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) =
                   (float)(LF_BITS_F((&DAT_00461df0)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) +
                   LF_BITS_F(local_88) * local_7c);
              *(float *)&((&DAT_00461df4)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) =
                   (float)(LF_BITS_F((&DAT_00461df4)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) +
                   local_78 * LF_BITS_F(local_88));
              *(float *)&((&DAT_00461df8)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) =
                   (float)(LF_BITS_F((&DAT_00461df8)[((int)LF_UINT(fVar7)) * 0x20 + ((int)LF_UINT(fVar6)) * 0x204]) +
                   local_74 * LF_BITS_F(local_88));
              local_50 = ((float *)LF_BITS_U(((float)(-local_70 * *local_50))));
              local_80[(0x11877c + LF_RELOC / 4)] = (float)(local_80[(0x11877c + LF_RELOC / 4)] + LF_BITS_F(local_50) * local_7c);
              local_80[(0x11877d + LF_RELOC / 4)] = (float)(local_80[(0x11877d + LF_RELOC / 4)] + LF_BITS_F(local_50) * local_78);
              local_80[(0x11877e + LF_RELOC / 4)] = (float)(LF_BITS_F(local_50) * local_74 + local_80[(0x11877e + LF_RELOC / 4)]);
            }
          }
          if (BOND_KIND(pfVar33) == 0xeU) {
            local_94 = pfVar33[-0x11];
            local_98 = pfVar33[6];
            local_bc = ((int)LF_UINT(pfVar33[7]));
            local_90 = pfVar33[-0x10];
            local_8c = pfVar33[-0xf];
            local_cc = pfVar33[-0xe];
            local_c8 = pfVar33[-0xd];
            local_c4 = pfVar33[-0xc];
            fVar7 = (float)(pfVar33[-6] * pfVar33[-3]);
            local_9c = (float *)(((int)LF_UINT(local_98)) * 0x810 + local_bc * 0x80);
            LF_COPY(fVar6, pfVar33[8]);
            fVar8 = -pfVar33[-2];
            local_6c = (float)(g_item[((int)LF_UINT(fVar6))].vel.x +
                       ((float)(local_c8 * g_item[((int)LF_UINT(fVar6))].torque.z -
                       local_c4 * g_item[((int)LF_UINT(fVar6))].torque.y)));
            local_68 = (float)(g_item[((int)LF_UINT(fVar6))].vel.y +
                       ((float)(local_c4 * g_item[((int)LF_UINT(fVar6))].torque.x -
                       local_cc * g_item[((int)LF_UINT(fVar6))].torque.z)));
            local_64 = (float)(g_item[((int)LF_UINT(fVar6))].vel.z +
                       ((float)(local_cc * g_item[((int)LF_UINT(fVar6))].torque.y -
                       g_item[((int)LF_UINT(fVar6))].torque.x * local_c8)));
            LF_COPY(fVar5, pfVar33[-2]);
            local_7c = (float)(fVar5 * local_6c +
                       ((float)(fVar8 * ((float)(local_9c[(0x118778 + LF_RELOC / 4)] +
                               ((float)(local_90 * local_9c[(0x118786 + LF_RELOC / 4)] - local_8c * local_9c[(0x118785 + LF_RELOC / 4)])))) +
                       fVar7 * ((vec3 *)(pfVar33 + -1))->x)));
            local_78 = (float)(fVar5 * local_68 +
                       ((float)(fVar8 * ((float)(local_9c[(0x118779 + LF_RELOC / 4)] +
                               ((float)(local_9c[(0x118784 + LF_RELOC / 4)] * local_8c - local_94 * local_9c[(0x118786 + LF_RELOC / 4)])))) +
                       (float)(fVar7 * *pfVar33))));
            local_74 = (float)(fVar5 * local_64 +
                       ((float)(fVar8 * ((float)(local_9c[(0x11877a + LF_RELOC / 4)] +
                               ((float)(local_9c[(0x118785 + LF_RELOC / 4)] * local_94 - local_90 * local_9c[(0x118784 + LF_RELOC / 4)])))) +
                       (float)(fVar7 * pfVar33[1]))));
            local_50 = ((float *)LF_BITS_U(((float)(local_74 * local_74 + (local_7c * local_7c + local_78 * local_78)))));
            dVar39 = _CIsqrt((double)LF_BITS_F(local_50));
            local_70 = (float)dVar39;
            if (local_70 <= 1.52587890625E-5) {
              local_74 = 0.0;
              local_7c = 0.0;
              local_78 = 0.0;
            }
            else {
              fVar5 = (float)(1.0 / local_70);
              local_7c = fVar5 * local_7c;
              local_78 = (float)(fVar5 * local_78);
              local_74 = fVar5 * local_74;
            }
            local_70 = (float)(local_70 /
                       (*(float *)(((int)LF_UINT(local_98)) * 0x2b64 + ((int)&g_human[0].lf_raw[0x154]) + local_bc * 0xe0) +
                       g_item[((int)LF_UINT(fVar6))].mass));
            local_50 = (float *)(((int)LF_UINT(local_98)) * 0x2b64 + ((int)&g_human[0].lf_raw[0x154]) + local_bc * 0xe0);
            local_88 = ((float *)LF_BITS_U(((float)(pfVar33[1] * local_74 +
                                (local_7c * ((vec3 *)(pfVar33 + -1))->x + local_78 * *pfVar33)))));
            if (0.0 < LF_BITS_F(local_88)) {
              calculatefriction(&local_7c,(float *)(pfVar33 + -1),pfVar33[2]);
              local_88 = ((float *)LF_BITS_U(((float)(g_item[((int)LF_UINT(fVar6))].mass * local_70))));
              fVar5 = LF_BITS_F(local_88) * local_7c;
              fVar8 = (float)(LF_BITS_F(local_88) * local_78);
              fVar7 = (float)(LF_BITS_F(local_88) * local_74);
              local_9c[(0x11877c + LF_RELOC / 4)] = (float)(fVar5 + local_9c[(0x11877c + LF_RELOC / 4)]);
              local_9c[(0x11877d + LF_RELOC / 4)] = (float)(fVar8 + local_9c[(0x11877d + LF_RELOC / 4)]);
              local_9c[(0x11877e + LF_RELOC / 4)] = (float)(fVar7 + local_9c[(0x11877e + LF_RELOC / 4)]);
              local_9c[(0x118788 + LF_RELOC / 4)] = (float)(((float)(local_8c * fVar8 - local_90 * fVar7)) + local_9c[(0x118788 + LF_RELOC / 4)]);
              local_9c[(0x118789 + LF_RELOC / 4)] = (float)(local_9c[(0x118789 + LF_RELOC / 4)] + ((float)(local_94 * fVar7 - fVar5 * local_8c)));
              local_9c[(0x11878a + LF_RELOC / 4)] = (float)(local_9c[(0x11878a + LF_RELOC / 4)] + ((float)(local_90 * fVar5 - local_94 * fVar8)));
              local_50 = ((float *)LF_BITS_U(((float)(-local_70 * *local_50))));
              local_5c = LF_BITS_F(local_50) * local_7c;
              local_58 = (float)(LF_BITS_F(local_50) * local_78);
              local_54 = (float)(LF_BITS_F(local_50) * local_74);
              g_item[((int)LF_UINT(fVar6))].dv.x = (float)(local_5c + g_item[((int)LF_UINT(fVar6))].dv.x);
              g_item[((int)LF_UINT(fVar6))].dv.y =
                   (float)(local_58 + g_item[((int)LF_UINT(fVar6))].dv.y);
              g_item[((int)LF_UINT(fVar6))].dv.z =
                   (float)(local_54 + g_item[((int)LF_UINT(fVar6))].dv.z);
              local_6c = (float)(local_c4 * local_58 - local_c8 * local_54);
              local_68 = (float)(local_cc * local_54 - local_5c * local_c4);
              local_64 = (float)(local_c8 * local_5c - local_cc * local_58);
              g_item[((int)LF_UINT(fVar6))].dw.x = (float)(local_6c + g_item[((int)LF_UINT(fVar6))].dw.x);
              g_item[((int)LF_UINT(fVar6))].dw.y =
                   (float)(g_item[((int)LF_UINT(fVar6))].dw.y + local_68);
              g_item[((int)LF_UINT(fVar6))].dw.z =
                   (float)(g_item[((int)LF_UINT(fVar6))].dw.z + local_64);
            }
          }
          if (BOND_KIND(pfVar33) == 0xfU) {
            LF_COPY(fVar5, pfVar33[6]);
            fVar6 = (float)(pfVar33[-6] * pfVar33[-3]);
            iVar30 = ((int)LF_UINT(pfVar33[7])) * 0xe8;
            local_6c = (float)(*(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x71fc] + iVar30) +
                       ((float)(pfVar33[-0x10] * *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x724c] + iVar30) -
                       pfVar33[-0xf] * *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7248] + iVar30))));
            local_68 = (float)(*(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7200] + iVar30) +
                       ((float)(pfVar33[-0xf] * *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7244] + iVar30) -
                       pfVar33[-0x11] * *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x724c] + iVar30))));
            local_64 = (float)(*(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7204] + iVar30) +
                       ((float)(pfVar33[-0x11] * *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7248] + iVar30) -
                       pfVar33[-0x10] * *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7244] + iVar30))));
            fVar7 = -pfVar33[-2];
            local_5c = (float)(fVar7 * local_6c + (float)(fVar6 * ((vec3 *)(pfVar33 + -1))->x));
            local_58 = (float)(fVar7 * local_68 + (float)(fVar6 * *pfVar33));
            local_54 = (float)(fVar7 * local_64 + (float)(fVar6 * pfVar33[1]));
            local_50 = ((float *)LF_BITS_U(((float)(pfVar33[1] * local_54 +
                                (((vec3 *)(pfVar33 + -1))->x * local_5c + local_58 * *pfVar33)))));
            if (0.0 < LF_BITS_F(local_50)) {
              calculatefriction(&local_5c,pfVar33 + -1,pfVar33[2]);
              *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7208] + iVar30) =
                   (float)(local_5c + *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7208] + iVar30));
              *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x720c] + iVar30) =
                   (float)(local_58 + *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x720c] + iVar30));
              *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7210] + iVar30) =
                   (float)(local_54 + *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7210] + iVar30));
              local_6c = (float)(pfVar33[-0xf] * local_58 - pfVar33[-0x10] * local_54);
              local_68 = (float)(local_54 * pfVar33[-0x11] - pfVar33[-0xf] * local_5c);
              local_64 = (float)(pfVar33[-0x10] * local_5c - local_58 * pfVar33[-0x11]);
              *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7250] + iVar30) =
                   (float)(*(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7250] + iVar30) + local_6c);
              *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7254] + iVar30) =
                   (float)(*(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7254] + iVar30) + local_68);
              *(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7258] + iVar30) =
                   (float)(*(float *)(&g_object[((int)LF_UINT(fVar5))].lf_raw[0x7258] + iVar30) + local_64);
            }
          }
        }
        local_c0 = local_c0 + 1;
        pfVar21 = pfVar21 + 0x10;
        pfVar33 = pfVar33 + 0x3b;
        piVar18 = local_60;
      } while (local_c0 < g_numofbonds);
    }
    iVar30 = g_timerlist.bondwheelsim;
    local_60 = &g_timer[g_timerlist.bondwheelsim].start_lo;
    uVar13 = rdtsc();
    *local_60 = (int)uVar13;
    g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
    objectenginesimulation2();
    stoptimer(g_timerlist.bondwheelsim);
    iVar30 = g_timerlist.bondloopcopy;
    uVar13 = rdtsc();
    g_timer[g_timerlist.bondloopcopy].start_lo = (int)uVar13;
    g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
    iVar29 = g_area_sector.num_particles;
    iVar30 = 0;
    if (3 < g_area_sector.num_particles) {
      iVar16 = (g_area_sector.num_particles - 4U >> 2) + 1;
      iVar30 = iVar16 * 4;
      pfVar21 = (float *)&DAT_00583dd4;
      do {
        iVar16 = iVar16 + -1;
        pfVar21[-1] = (float)(pfVar21[3] + pfVar21[-1]);
        *pfVar21 = (float)(pfVar21[4] + *pfVar21);
        pfVar21[1] = (float)(pfVar21[5] + pfVar21[1]);
        pfVar21[7] = (float)(pfVar21[0xb] + pfVar21[7]);
        pfVar21[8] = (float)(pfVar21[0xc] + pfVar21[8]);
        pfVar21[9] = (float)(pfVar21[0xd] + pfVar21[9]);
        pfVar21[0xf] = (float)(pfVar21[0x13] + pfVar21[0xf]);
        pfVar21[0x10] = (float)(pfVar21[0x14] + pfVar21[0x10]);
        pfVar21[0x11] = (float)(pfVar21[0x15] + pfVar21[0x11]);
        pfVar21[0x17] = (float)(pfVar21[0x1b] + pfVar21[0x17]);
        pfVar21[0x18] = (float)(pfVar21[0x1c] + pfVar21[0x18]);
        pfVar21[0x19] = (float)(pfVar21[0x1d] + pfVar21[0x19]);
        pfVar21 = pfVar21 + 0x20;
      } while (iVar16 != 0);
    }
    if (iVar30 < iVar29) {
      iVar29 = iVar29 - iVar30;
      pfVar21 = (float *)(&DAT_00583dd4 + iVar30 * 8);
      do {
        iVar29 = iVar29 + -1;
        pfVar21[-1] = (float)(pfVar21[3] + pfVar21[-1]);
        *pfVar21 = (float)(pfVar21[4] + *pfVar21);
        pfVar21[1] = (float)(pfVar21[5] + pfVar21[1]);
        pfVar21 = pfVar21 + 8;
      } while (iVar29 != 0);
    }
    iVar30 = g_timerlist.bondtest;
    local_60 = &g_timer[g_timerlist.bondtest].start_lo;
    uVar13 = rdtsc();
    *local_60 = (int)uVar13;
    g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
    puVar31 = &DAT_00461dd4;
    if (DAT_019cbf68 == 0) {
      do {
        if (puVar31[-1] != 0) {
          uVar32 = 1;
          pfVar21 = (float *)(puVar31 + 4);
          iVar30 = 0xf;
          do {
            if ((*puVar31 & uVar32) != 0) {
              pfVar21[-1] = (float)(pfVar21[3] + pfVar21[-1]);
              *pfVar21 = (float)(pfVar21[4] + *pfVar21);
              pfVar21[1] = (float)(pfVar21[5] + pfVar21[1]);
              pfVar21[7] = (float)(pfVar21[0xf] + pfVar21[7]);
              pfVar21[8] = (float)(pfVar21[0x10] + pfVar21[8]);
              pfVar21[9] = (float)(pfVar21[0x11] + pfVar21[9]);
              fVar5 = (float)(((float)(pfVar21[9] * pfVar21[0x15] +
                      (pfVar21[8] * pfVar21[0x14] + pfVar21[7] * pfVar21[0x13]))) * pfVar21[0x16]);
              pfVar21[0xb] = fVar5 * pfVar21[0x13];
              pfVar21[0xc] = (float)(fVar5 * pfVar21[0x14]);
              pfVar21[0xd] = (float)(fVar5 * pfVar21[0x15]);
              fVar5 = (float)(((float)(pfVar21[9] * pfVar21[0x19] +
                      (pfVar21[8] * pfVar21[0x18] + pfVar21[7] * pfVar21[0x17]))) * pfVar21[0x1a]);
              pfVar21[0xb] = (float)(pfVar21[0xb] + fVar5 * pfVar21[0x17]);
              pfVar21[0xc] = (float)(fVar5 * pfVar21[0x18] + pfVar21[0xc]);
              pfVar21[0xd] = (float)(fVar5 * pfVar21[0x19] + pfVar21[0xd]);
              local_60 = ((int *)LF_BITS_U(((float)(((float)(pfVar21[9] * pfVar21[0x1d] +
                                 (pfVar21[8] * pfVar21[0x1c] + pfVar21[7] * pfVar21[0x1b]))) *
                                pfVar21[0x1e]))));
              pfVar21[0xb] = (float)(LF_BITS_F(local_60) * pfVar21[0x1b] + pfVar21[0xb]);
              pfVar21[0xc] = (float)(LF_BITS_F(local_60) * pfVar21[0x1c] + pfVar21[0xc]);
              pfVar21[0xd] = (float)(LF_BITS_F(local_60) * pfVar21[0x1d] + pfVar21[0xd]);
            }
            pfVar21 = pfVar21 + 0x20;
            uVar32 = uVar32 << 1 | (uint)((int)uVar32 < 0);
            iVar30 = iVar30 + -1;
          } while (iVar30 != 0);
        }
        puVar31 = puVar31 + 0x204;
      } while ((int)puVar31 < ((int)SR_VA(0x563dd4)));
    }
    else {
      do {
        if (puVar31[-1] != 0) {
          uVar32 = 1;
          pfVar21 = (float *)(puVar31 + 3);
          iVar30 = 0xf;
          do {
            if ((*puVar31 & uVar32) != 0) {
              *pfVar21 = (float)(*pfVar21 + pfVar21[4]);
              pfVar21[1] = (float)(pfVar21[1] + pfVar21[5]);
              pfVar21[2] = (float)(pfVar21[2] + pfVar21[6]);
              pfVar21[3] = (float)(pfVar21[3] + pfVar21[7]);
              pfVar21[8] = (float)(pfVar21[0x10] + pfVar21[8]);
              pfVar21[9] = (float)(pfVar21[0x11] + pfVar21[9]);
              pfVar21[10] = (float)(pfVar21[0x12] + pfVar21[10]);
              pfVar21[0xb] = (float)(pfVar21[0x13] + pfVar21[0xb]);
              auVar2 = *(lf_m128 *)(pfVar21 + 0x14);
              fVar5 = (*(float *)((uchar *)&auVar2 + 12));
              auVar36 = *(lf_m128 *)(pfVar21 + 8);
              auVar37 = lf_dpps(auVar36,auVar2,0x7f);
              auVar3 = *(lf_m128 *)(pfVar21 + 0x18);
              fVar6 = (*(float *)((uchar *)&auVar3 + 12));
              auVar38 = lf_dpps(auVar36,auVar3,0x7f);
              auVar4 = *(lf_m128 *)(pfVar21 + 0x1c);
              fVar7 = (*(float *)((uchar *)&auVar4 + 12));
              auVar36 = lf_dpps(auVar36,auVar4,0x7f);
              pfVar21[0xc] = (float)((float)((float)(fVar7 * (*(float *)((uchar *)&auVar36 + 0))) * (*(float *)((uchar *)&auVar4 + 0))) +
                             ((float)((float)((float)(fVar6 * (*(float *)((uchar *)&auVar38 + 0))) * (*(float *)((uchar *)&auVar3 + 0))) +
                             (float)((float)(fVar5 * (*(float *)((uchar *)&auVar37 + 0))) * (*(float *)((uchar *)&auVar2 + 0))))));
              pfVar21[0xd] = (float)((float)((float)(fVar7 * (*(float *)((uchar *)&auVar36 + 4))) * (*(float *)((uchar *)&auVar4 + 4))) +
                             ((float)((float)((float)(fVar6 * (*(float *)((uchar *)&auVar38 + 4))) * (*(float *)((uchar *)&auVar3 + 4))) +
                             (float)((float)(fVar5 * (*(float *)((uchar *)&auVar37 + 4))) * (*(float *)((uchar *)&auVar2 + 4))))));
              pfVar21[0xe] = (float)((float)((float)(fVar7 * (*(float *)((uchar *)&auVar36 + 8))) * (*(float *)((uchar *)&auVar4 + 8))) +
                             ((float)((float)((float)(fVar6 * (*(float *)((uchar *)&auVar38 + 8))) * (*(float *)((uchar *)&auVar3 + 8))) +
                             (float)((float)(fVar5 * (*(float *)((uchar *)&auVar37 + 8))) * (*(float *)((uchar *)&auVar2 + 8))))));
              pfVar21[0xf] = (float)((float)((float)(fVar7 * (*(float *)((uchar *)&auVar36 + 12))) * fVar7) +
                             ((float)((float)((float)(fVar6 * (*(float *)((uchar *)&auVar38 + 12))) * fVar6) + (float)((float)(fVar5 * (*(float *)((uchar *)&auVar37 + 12))) * fVar5))));
            }
            pfVar21 = pfVar21 + 0x20;
            uVar32 = uVar32 << 1 | (uint)((int)uVar32 < 0);
            iVar30 = iVar30 + -1;
          } while (iVar30 != 0);
        }
        puVar31 = puVar31 + 0x204;
      } while ((int)puVar31 < ((int)SR_VA(0x563dd4)));
    }
    stoptimer(g_timerlist.bondtest);
    pfVar21 = &g_item[0].vel.y;
    do {
      if ((LF_UINT(pfVar21[-0x1c]) != 0x0U) && (LF_UINT(pfVar21[-0x1b]) != 0x0U)) {
        ((vec3 *)(pfVar21 + -1))->x = (float)(pfVar21[2] + ((vec3 *)(pfVar21 + -1))->x);
        *pfVar21 = (float)(pfVar21[3] + *pfVar21);
        pfVar21[1] = (float)(pfVar21[4] + pfVar21[1]);
        pfVar21[0xe] = (float)(pfVar21[0x17] + pfVar21[0xe]);
        pfVar21[0xf] = (float)(pfVar21[0x18] + pfVar21[0xf]);
        pfVar21[0x10] = (float)(pfVar21[0x19] + pfVar21[0x10]);
        pfVar21[0x11] = 0.0;
        pfVar21[0x12] = 0.0;
        pfVar21[0x13] = 0.0;
        fVar5 = (float)(((float)(pfVar21[0x10] * pfVar21[7] + (pfVar21[0xf] * pfVar21[6] + pfVar21[0xe] * pfVar21[5])))
                * pfVar21[0x2d]);
        pfVar21[0x11] = (float)(pfVar21[0x11] + fVar5 * pfVar21[5]);
        pfVar21[0x12] = (float)(fVar5 * pfVar21[6] + pfVar21[0x12]);
        pfVar21[0x13] = (float)(fVar5 * pfVar21[7] + pfVar21[0x13]);
        fVar5 = (float)(((float)(pfVar21[0x10] * pfVar21[10] + (pfVar21[0xf] * pfVar21[9] + pfVar21[8] * pfVar21[0xe]))
                ) * pfVar21[0x2e]);
        pfVar21[0x11] = (float)(pfVar21[0x11] + fVar5 * pfVar21[8]);
        pfVar21[0x12] = (float)(fVar5 * pfVar21[9] + pfVar21[0x12]);
        pfVar21[0x13] = (float)(fVar5 * pfVar21[10] + pfVar21[0x13]);
        local_60 = ((int *)LF_BITS_U(((float)(((float)(pfVar21[0x10] * pfVar21[0xd] +
                           (pfVar21[0xf] * pfVar21[0xc] + pfVar21[0xe] * pfVar21[0xb]))) *
                          pfVar21[0x2f]))));
        pfVar21[0x11] = (float)(LF_BITS_F(local_60) * pfVar21[0xb] + pfVar21[0x11]);
        pfVar21[0x12] = (float)(LF_BITS_F(local_60) * pfVar21[0xc] + pfVar21[0x12]);
        pfVar21[0x13] = (float)(LF_BITS_F(local_60) * pfVar21[0xd] + pfVar21[0x13]);
      }
      pfVar21 = pfVar21 + 0x6b;
    } while ((int)pfVar21 < ((int)&g_sysinfo.lf_raw[0x50]));
    piVar34 = &g_object[0].num_wheel_bodies;
    do {
      if ((((Object *)(piVar34 + -0x1c7b))->active != 0) && (iVar30 = 0, 0 < *piVar34)) {
        pfVar21 = (float *)(piVar34 + 5);
        do {
          pfVar21[-1] = (float)(pfVar21[2] + pfVar21[-1]);
          *pfVar21 = (float)(pfVar21[3] + *pfVar21);
          pfVar21[1] = (float)(pfVar21[4] + pfVar21[1]);
          pfVar21[0xe] = (float)(pfVar21[0x14] + pfVar21[0xe]);
          pfVar21[0xf] = (float)(pfVar21[0x15] + pfVar21[0xf]);
          pfVar21[0x10] = (float)(pfVar21[0x16] + pfVar21[0x10]);
          pfVar21[0x11] = 0.0;
          pfVar21[0x12] = 0.0;
          pfVar21[0x13] = 0.0;
          fVar5 = (float)(((float)(pfVar21[7] * pfVar21[0x10] +
                  (pfVar21[0xf] * pfVar21[6] + pfVar21[0xe] * pfVar21[5]))) * pfVar21[0x1a]);
          pfVar21[0x11] = (float)(pfVar21[0x11] + fVar5 * pfVar21[5]);
          pfVar21[0x12] = (float)(pfVar21[6] * fVar5 + pfVar21[0x12]);
          pfVar21[0x13] = (float)(fVar5 * pfVar21[7] + pfVar21[0x13]);
          fVar5 = (float)(((float)(pfVar21[10] * pfVar21[0x10] +
                  (pfVar21[0xf] * pfVar21[9] + pfVar21[0xe] * pfVar21[8]))) * pfVar21[0x1b]);
          pfVar21[0x11] = (float)(pfVar21[0x11] + fVar5 * pfVar21[8]);
          pfVar21[0x12] = (float)(pfVar21[9] * fVar5 + pfVar21[0x12]);
          pfVar21[0x13] = (float)(fVar5 * pfVar21[10] + pfVar21[0x13]);
          iVar30 = iVar30 + 1;
          local_60 = ((int *)LF_BITS_U(((float)(((float)(pfVar21[0xd] * pfVar21[0x10] +
                             (pfVar21[0xb] * pfVar21[0xe] + pfVar21[0xf] * pfVar21[0xc]))) *
                            pfVar21[0x1c]))));
          pfVar21[0x11] = (float)(pfVar21[0x11] + LF_BITS_F(local_60) * pfVar21[0xb]);
          pfVar21[0x12] = (float)(pfVar21[0xc] * LF_BITS_F(local_60) + pfVar21[0x12]);
          pfVar21[0x13] = (float)(LF_BITS_F(local_60) * pfVar21[0xd] + pfVar21[0x13]);
          pfVar21 = pfVar21 + 0x3a;
        } while (iVar30 < *piVar34);
      }
      piVar34 = piVar34 + 0x208c;
    } while ((int)piVar34 < ((int)SR_VA(0x7a257ec)));
    stoptimer(g_timerlist.bondloopcopy);
    iVar30 = g_timerlist.bondsetupcopy;
    local_60 = (int *)((int)piVar18 + 1);
  } while ((int)local_60 < 0x20);
  local_60 = &g_timer[g_timerlist.bondsetupcopy].start_lo;
  uVar13 = rdtsc();
  *local_60 = (int)uVar13;
  g_timer[iVar30].start_hi = (int)((ulonglong)uVar13 >> 0x20);
  iVar29 = g_area_sector.num_particles;
  iVar30 = 0;
  if (3 < g_area_sector.num_particles) {
    iVar16 = (g_area_sector.num_particles - 4U >> 2) + 1;
    iVar30 = iVar16 * 4;
    pfVar21 = (float *)&DAT_00583dd8;
    pfVar33 = &g_particle[0].vel.z;
    do {
      pfVar33[-2] = pfVar21[-2];
      iVar16 = iVar16 + -1;
      pfVar33[-1] = pfVar21[-1];
      *pfVar33 = *pfVar21;
      pfVar33[0x3a] = pfVar21[6];
      pfVar33[0x3b] = pfVar21[7];
      pfVar33[0x3c] = pfVar21[8];
      pfVar33[0x76] = pfVar21[0xe];
      pfVar33[0x77] = pfVar21[0xf];
      pfVar33[0x78] = pfVar21[0x10];
      pfVar33[0xb2] = pfVar21[0x16];
      pfVar33[0xb3] = pfVar21[0x17];
      pfVar33[0xb4] = pfVar21[0x18];
      pfVar21 = pfVar21 + 0x20;
      pfVar33 = pfVar33 + 0xf0;
    } while (iVar16 != 0);
  }
  if (iVar30 < iVar29) {
    iVar29 = iVar29 - iVar30;
    pfVar21 = &g_particle[0].vel.z + iVar30 * 0x3c;
    pfVar33 = (float *)(&DAT_00583dd8 + iVar30 * 8);
    do {
      pfVar21[-2] = pfVar33[-2];
      iVar29 = iVar29 + -1;
      pfVar21[-1] = pfVar33[-1];
      *pfVar21 = *pfVar33;
      pfVar21 = pfVar21 + 0x3c;
      pfVar33 = pfVar33 + 8;
    } while (iVar29 != 0);
  }
  piVar34 = &DAT_00461de8;
  piVar18 = &g_human[0].has_physics;
  do {
    if ((((Human *)(piVar18 + -1))->active != 0) && (*piVar18 != 0)) {
      iVar30 = 3;
      piVar22 = piVar34;
      piVar26 = piVar18 + 0x39;
      do {
        piVar26[-2] = piVar22[-2];
        piVar26[-1] = piVar22[-1];
        *piVar26 = *piVar22;
        piVar26[0xd] = piVar22[6];
        piVar26[0xe] = piVar22[7];
        piVar26[0xf] = piVar22[8];
        piVar26[0x10] = piVar22[10];
        piVar26[0x11] = piVar22[0xb];
        piVar26[0x12] = piVar22[0xc];
        piVar26[0x36] = piVar22[0x1e];
        piVar26[0x37] = piVar22[0x1f];
        piVar26[0x38] = piVar22[0x20];
        piVar26[0x45] = piVar22[0x26];
        piVar26[0x46] = piVar22[0x27];
        piVar26[0x47] = piVar22[0x28];
        piVar26[0x48] = piVar22[0x2a];
        piVar26[0x49] = piVar22[0x2b];
        piVar26[0x4a] = piVar22[0x2c];
        piVar26[0x6e] = piVar22[0x3e];
        piVar26[0x6f] = piVar22[0x3f];
        piVar26[0x70] = piVar22[0x40];
        piVar26[0x7d] = piVar22[0x46];
        piVar26[0x7e] = piVar22[0x47];
        piVar26[0x7f] = piVar22[0x48];
        piVar26[0x80] = piVar22[0x4a];
        piVar26[0x81] = piVar22[0x4b];
        piVar26[0x82] = piVar22[0x4c];
        piVar26[0xa6] = piVar22[0x5e];
        piVar26[0xa7] = piVar22[0x5f];
        piVar26[0xa8] = piVar22[0x60];
        piVar26[0xb5] = piVar22[0x66];
        piVar26[0xb6] = piVar22[0x67];
        piVar26[0xb7] = piVar22[0x68];
        piVar26[0xb8] = piVar22[0x6a];
        piVar26[0xb9] = piVar22[0x6b];
        piVar26[0xba] = piVar22[0x6c];
        piVar26[0xde] = piVar22[0x7e];
        piVar26[0xdf] = piVar22[0x7f];
        piVar26[0xe0] = piVar22[0x80];
        piVar26[0xed] = piVar22[0x86];
        piVar26[0xee] = piVar22[0x87];
        iVar30 = iVar30 + -1;
        piVar26[0xef] = piVar22[0x88];
        piVar26[0xf0] = piVar22[0x8a];
        piVar26[0xf1] = piVar22[0x8b];
        piVar26[0xf2] = piVar22[0x8c];
        piVar22 = piVar22 + 0xa0;
        piVar26 = piVar26 + 0x118;
      } while (iVar30 != 0);
    }
    piVar18 = piVar18 + 0xad9;
    piVar34 = piVar34 + 0x204;
  } while ((int)piVar18 < ((int)SR_VA(0x1f38784)));
  stoptimer(g_timerlist.bondsetupcopy);
  stoptimer(g_timerlist.bondsim);
  _DAT_020a5a30 = g_numofbonds;
  g_numofbonds = DAT_14022d24;
  return;
}
#undef g_human
#undef g_sysinfo
#undef DAT_00460d18
#undef DAT_00603dd0
#undef DAT_019cbf68
#undef DAT_14022d24
#undef DAT_020a5a30
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
#undef _DAT_020a5a30
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

