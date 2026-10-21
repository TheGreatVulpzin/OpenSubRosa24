/* The simulation of objects (cars, helicopters, trains): once for each tick.
 *
 * Loop 1 makes the box, the center, the speed and the frame (rot) of each object from its
 * particles and adds the air drag. Loop 2 runs the vehicle code, moves the wheels and the
 * wheel bodies, and makes the contacts of the particles with the level as bonds. Loop 3 makes
 * the contacts between the particles of one object and the faces of another object.
 *
 * Float rules: a value that the code stores in a float rounds to a float. A value inside one
 * expression stays a double. The sums keep their order, so the results are bit exact.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "collide.h"
#include "events.h"
#include "objects.h"
#include "profiler.h"
#include "sr_math.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define g_intersect (*SR_AT(Intersect, SR_RVA_INTERSECT))
#define g_track SR_AT(TrackPiece, 0x2597efc0)
#define g_yaxis (*SR_AT(vec3, 0x60ce0))
#define g_train_near (*SR_AT(int, 0x1952d668)) /* 1 when a locomotive moves toward the crossing */
#define sr_objecttypes SR_AT(ObjectType, 0xac51bc0)
#define sr_soundlist (*SR_AT(SoundList, 0x916d700))
#define sr_city (*SR_AT(City, 0xe299220))

/* Game functions of other files. */
int __cdecl lf_2c020(uint32_t area, uint32_t x, uint32_t y, uint32_t z, uint32_t word, int spec_face, vec3 *pos,
                     vec3 *vel);
#define area_breakblock lf_2c020

#define GRAVITY 0.0027222223579883575

/* An object is hurt. At no health, two particles get a random kick along the up axis. */
void applydamageobject(int obj, int damage)
{
  Object *o = &sr_objects[obj];
  int i;

  if (o->type == 12 || o->health <= 0)
    return;
  o->health -= damage;
  if (o->health >= 1)
    return;
  for (i = 6; i < 8; i++) {
    Particle *p = &sr_particles[o->particles[i]];
    float f = (float)((((((float)(os_rand() & 0x7f) * 0.25) * 0.25) * 9.800000190734863) / 60.0) + 0.02041666768491268);
    vec3 a;

    a.x = (float)(f * o->rot[1].x);
    a.y = (float)(f * o->rot[1].y);
    a.z = (float)(f * o->rot[1].z);
    p->vel.x = (float)(a.x + p->vel.x);
    p->vel.y = (float)(a.y + p->vel.y);
    p->vel.z = (float)(a.z + p->vel.z);
  }
  createevent_updateobject(obj, 2, 0, &o->pos, &o->vel);
}

/* Count down the life of each object. At the end the object goes away. */
void objecttimetolive(void)
{
  int i, k, h;

  for (i = 0; i < 1024; i++) {
    Object *o = &sr_objects[i];

    if (!o->active)
      continue;
    if (o->time_to_live < 0xffff)
      o->time_to_live--;
    if (o->time_to_live >= 1)
      continue;
    for (k = 0; k < o->num_particles; k++)
      sr_particles[o->particles[k]].time_to_live = 0;
    for (h = 0; h < 512; h++) {
      if (sr_humans[h].active && sr_humans[h].vehicle_id == i)
        sr_humans[h].vehicle_id = -1;
    }
    o->active = 0;
    createevent_updateobject(i, 3, 0, &o->pos, NULL);
  }
}

/* Grow the box of the object so that it holds the particle. */
static void box_add(Object *o, const Particle *p)
{
  if (p->pos.x < o->box_min.x)
    o->box_min.x = p->pos.x;
  if (o->box_max.x < p->pos.x)
    o->box_max.x = p->pos.x;
  if (p->pos.y < o->box_min.y)
    o->box_min.y = p->pos.y;
  if (o->box_max.y < p->pos.y)
    o->box_max.y = p->pos.y;
  if (p->pos.z < o->box_min.z)
    o->box_min.z = p->pos.z;
  if (o->box_max.z < p->pos.z)
    o->box_max.z = p->pos.z;
}

/* The center, the mean velocity, the box and the impact of the particles. Returns the impact:
 * the mean change of the speed of the particles since the last tick. */
static float object_gather(Object *o)
{
  float imp = 0.0f;
  float inv;
  int k;

  o->vel_prev = o->vel;
  o->pos.x = 0.0f;
  o->pos.y = 0.0f;
  o->pos.z = 0.0f;
  o->vel.x = 0.0f;
  o->vel.y = 0.0f;
  o->vel.z = 0.0f;
  o->box_min.x = o->box_min.y = o->box_min.z = 65536.0f;
  o->box_max.x = o->box_max.y = o->box_max.z = -65536.0f;
  for (k = 0; k < o->num_particles; k++) {
    Particle *p = &sr_particles[o->particles[k]];
    vec3 d;

    d.x = (float)(p->vel_last.x - p->vel.x);
    d.y = (float)(p->vel_last.y - p->vel.y);
    d.z = (float)(p->vel_last.z - p->vel.z);
    imp = (float)((float)x87_sqrt((float)((d.y * d.y + d.x * d.x) + d.z * d.z)) + imp);
    p->vel_last = p->vel;
    o->pos.x = (float)(o->pos.x + p->pos.x);
    o->pos.y = (float)(o->pos.y + p->pos.y);
    o->pos.z = (float)(o->pos.z + p->pos.z);
    o->vel.x = (float)(p->vel.x + o->vel.x);
    o->vel.y = (float)(p->vel.y + o->vel.y);
    o->vel.z = (float)(p->vel.z + o->vel.z);
    box_add(o, p);
  }
  for (k = 0; k < o->num_wheels; k++) {
    const Wheel *w = &o->wheels[k];
    double lo = w->pos.x - w->radius;
    double hi = w->radius + w->pos.x;

    if (lo < o->box_min.x)
      o->box_min.x = (float)lo;
    if (o->box_max.x < hi)
      o->box_max.x = (float)hi;
    lo = w->pos.y - w->radius;
    hi = w->radius + w->pos.y;
    if (lo < o->box_min.y)
      o->box_min.y = (float)lo;
    if (o->box_max.y < hi)
      o->box_max.y = (float)hi;
    lo = w->pos.z - w->radius;
    hi = w->radius + w->pos.z;
    if (lo < o->box_min.z)
      o->box_min.z = (float)lo;
    if (o->box_max.z < hi)
      o->box_max.z = (float)hi;
  }
  inv = (float)(1.0 / (float)o->num_particles);
  o->pos.x = (float)(o->pos.x * inv);
  o->pos.y = (float)(o->pos.y * inv);
  o->pos.z = (float)(o->pos.z * inv);
  o->vel.x = (float)(o->vel.x * inv);
  o->vel.y = (float)(o->vel.y * inv);
  o->vel.z = (float)(o->vel.z * inv);
  return (float)(imp / o->num_particles);
}

/* The orientation of the object (rot) from the first particles and their weights. */
static void object_frame(Object *o)
{
  int m = o->num_frame_particles;
  vec3 c = {0.0f, 0.0f, 0.0f};
  float inv;
  int k;

  for (k = 0; k < m; k++) {
    const Particle *p = &sr_particles[o->particles[k]];
    c.x = (float)(p->pos.x + c.x);
    c.y = (float)(p->pos.y + c.y);
    c.z = (float)(p->pos.z + c.z);
  }
  inv = (float)(1.0 / (float)m);
  c.x = (float)(inv * c.x);
  c.y = (float)(inv * c.y);
  c.z = (float)(inv * c.z);
  o->rot[0].x = o->rot[0].y = o->rot[0].z = 0.0f;
  o->rot[2].x = o->rot[2].y = o->rot[2].z = 0.0f;
  for (k = 0; k < m; k++) {
    const Particle *p = &sr_particles[o->particles[k]];
    vec3 d;

    d.x = (float)(p->pos.x - c.x);
    d.y = (float)(p->pos.y - c.y);
    d.z = (float)(p->pos.z - c.z);
    o->rot[0].x = (float)(d.x * o->frame_weight_x[k] + o->rot[0].x);
    o->rot[0].y = (float)(d.y * o->frame_weight_x[k] + o->rot[0].y);
    o->rot[0].z = (float)(d.z * o->frame_weight_x[k] + o->rot[0].z);
    o->rot[2].x = (float)(d.x * o->frame_weight_z[k] + o->rot[2].x);
    o->rot[2].y = (float)(d.y * o->frame_weight_z[k] + o->rot[2].y);
    o->rot[2].z = (float)(d.z * o->frame_weight_z[k] + o->rot[2].z);
  }
  object_normalize(&o->rot[0], &o->rot[0]);
  object_normalize(&o->rot[2], &o->rot[2]);
  o->rot[1].x = (float)(o->rot[0].z * o->rot[2].y - o->rot[0].y * o->rot[2].z);
  o->rot[1].y = (float)(o->rot[2].z * o->rot[0].x - o->rot[2].x * o->rot[0].z);
  o->rot[1].z = (float)(o->rot[0].y * o->rot[2].x - o->rot[0].x * o->rot[2].y);
  object_normalize(&o->rot[1], &o->rot[1]);
  o->rot[0].x = (float)(o->rot[2].z * o->rot[1].y - o->rot[1].z * o->rot[2].y);
  o->rot[0].y = (float)(o->rot[1].z * o->rot[2].x - o->rot[2].z * o->rot[1].x);
  o->rot[0].z = (float)(o->rot[2].y * o->rot[1].x - o->rot[2].x * o->rot[1].y);
  object_normalize(&o->rot[0], &o->rot[0]);
}

/* The vertices of the mesh: the particles, then the points that are sums of four particles. */
static void object_vertices(Object *o)
{
  const TypeQuad *q = sr_objecttypes[o->type].quads;
  int n = o->num_particles;
  int k;

  for (k = 0; k < n; k++) {
    o->verts[k].pos = sr_particles[o->particles[k]].pos;
    o->verts[k].particle = k;
  }
  for (k = n; k < sr_objecttypes[o->type].num_quads; k++) {
    const Particle *p0 = &sr_particles[o->particles[q[k].particles[0]]];
    const Particle *p1 = &sr_particles[o->particles[q[k].particles[1]]];
    const Particle *p2 = &sr_particles[o->particles[q[k].particles[2]]];
    const Particle *p3 = &sr_particles[o->particles[q[k].particles[3]]];
    const float *w = q[k].weights;
    vec3 v;

    v.x = (float)(w[0] * p0->pos.x);
    v.y = (float)(w[0] * p0->pos.y);
    v.z = (float)(w[0] * p0->pos.z);
    v.x = (float)(p1->pos.x * w[1] + v.x);
    v.y = (float)(p1->pos.y * w[1] + v.y);
    v.z = (float)(p1->pos.z * w[1] + v.z);
    v.x = (float)(p2->pos.x * w[2] + v.x);
    v.y = (float)(p2->pos.y * w[2] + v.y);
    v.z = (float)(p2->pos.z * w[2] + v.z);
    v.x = (float)(p3->pos.x * w[3] + v.x);
    v.y = (float)(p3->pos.y * w[3] + v.y);
    v.z = (float)(p3->pos.z * w[3] + v.z);
    o->verts[k].pos = v;
    o->verts[k].particle = q[k].particles[0];
  }
}

/* A hard impact hurts the object. Impact noise adds up and plays a sound above a limit. */
static void object_impact(Object *o, int obj, float imp)
{
  if (o->type == 13)
    return;
  if (0.0833333358168602 < imp)
    applydamageobject(obj, (int)(imp * 60.0 + 10.0));
  if (imp <= 0.03333333507180214) {
    if (0.5 < o->crash_noise)
      createevent_sound(sr_soundlist.crash, &o->pos, (float)(o->crash_noise * 0.5), 1.0f);
    o->crash_noise = 0.0f;
  } else {
    float x = (float)(((imp * 60.0) * 0.125) + o->crash_noise);

    o->crash_noise = x;
    if (2.0 < x) {
      createevent_sound(sr_soundlist.crash, &o->pos, (float)(0.5 * x), 1.0f);
      o->crash_noise = (float)(x - 2.0);
    }
  }
}

/* Air drag on each particle along the three axes of the object (not for helicopters). */
static void object_drag(Object *o)
{
  const vec3 *r0 = &o->rot[0];
  const vec3 *r1 = &o->rot[1];
  const vec3 *r2 = &o->rot[2];
  int k;

  for (k = 0; k < o->num_particles; k++) {
    Particle *p = &sr_particles[o->particles[k]];
    vec3 nv;
    float s, s2, f;

    s = (float)x87_sqrt((float)((p->vel.y * p->vel.y + p->vel.x * p->vel.x) + p->vel.z * p->vel.z));
    s2 = (float)(s * s);
    if (s == 0.0) {
      nv.x = nv.y = nv.z = 0.0f;
    } else {
      float inv = (float)(1.0 / s);
      nv.x = (float)(inv * p->vel.x);
      nv.y = (float)(inv * p->vel.y);
      nv.z = (float)(inv * p->vel.z);
    }
    f = (float)((float)((r0->y * nv.y + r0->x * nv.x) + nv.z * r0->z) * ((((4.0 * s2) * 0.5) * 0.75) / -1000.0));
    p->vel.x = (float)(r0->x * f + p->vel.x);
    p->vel.y = (float)(r0->y * f + p->vel.y);
    p->vel.z = (float)(r0->z * f + p->vel.z);
    f = (float)((float)((nv.x * r1->x + nv.y * r1->y) + nv.z * r1->z) * (((((s2 + s2) * 4.0) * 0.5) * 0.75) / -1000.0));
    p->vel.x = (float)(f * r1->x + p->vel.x);
    p->vel.y = (float)(f * r1->y + p->vel.y);
    p->vel.z = (float)(f * r1->z + p->vel.z);
    f = (float)((float)((nv.y * r2->y + nv.x * r2->x) + nv.z * r2->z) * ((((s2 + s2) * 0.25) * 0.75) / -1000.0));
    p->vel.x = (float)(f * r2->x + p->vel.x);
    p->vel.y = (float)(f * r2->y + p->vel.y);
    p->vel.z = (float)(f * r2->z + p->vel.z);
  }
}

/* Loop 1: the state of one object from its particles. */
static void object_update(Object *o, int obj)
{
  float imp = object_gather(o);

  object_frame(o);
  if (o->pos.y < -32.0)
    o->health = 0;
  object_vertices(o);
  object_impact(o, obj, imp);
  if (o->state != 2)
    object_drag(o);
}

/* The locomotive: the door face follows the speed, the engine pulls toward the station track
 * piece, and it stops at the station for 0x4b0 ticks. */
static void locomotive(Object *o, int obj)
{
  const vec3 *r2 = &o->rot[2];
  float dx = (float)(sr_city.intersections[4].pos.x - o->pos.x);
  float dz = (float)(sr_city.intersections[4].pos.z - o->pos.z);
  float thr = 0.4f;
  float fw, sp, t;
  int f, tp, k;
  vec3 d;
  ObjectFace *face;

  if (0.0 < (float)((o->vel.y * 0.0 + o->vel.x * dx) + dz * o->vel.z)) {
    if ((float)x87_sqrt((float)((dx * dx + 0.0) + dz * dz)) < 160.0)
      g_train_near = 1;
  }
  fw = (float)((o->vel.y * r2->y + r2->x * o->vel.x) + r2->z * o->vel.z);
  o->rpm_net = (int)((fabs(fw) * 120.0) * 60.0);
  sp = -fw;
  f = sr_objecttypes[12].door_face;
  face = &o->faces[f];
  if (sp >= 0.01666666753590107) {
    if (face->health < 8) {
      face->health = 8;
      createevent_updateobject(obj, 5, f, &o->pos, &o->vel);
    }
  } else if (face->health > 0) {
    face->health = 0;
    createevent_updateobject(obj, 4, f, &o->pos, &o->vel);
  }
  tp = o->track_target;
  d.x = (float)(g_track[tp].pos.x - o->pos.x);
  d.y = (float)(g_track[tp].pos.y - o->pos.y);
  d.z = (float)(g_track[tp].pos.z - o->pos.z);
  if ((float)x87_sqrt((float)((d.y * d.y + d.x * d.x) + d.z * d.z)) < 128.0 && abs(o->track_last - tp) < 8) {
    int stay = 0;

    thr = 0.0f;
    if (sp < 0.01666666753590107) {
      if (++o->stop_ticks < 0x4b0)
        stay = 1;
      else
        o->track_target = tp != 0x4a ? 0x4a : 0x1c;
    }
    if (!stay)
      o->stop_ticks = 0;
  }
  t = (float)((float)(thr - sp) * 8.0);
  if (!(-1.0 <= t))
    t = -1.0f;
  else if (1.0 < t)
    t = 1.0f;
  if (o->num_particles > 0) {
    float pull = (float)((8.0 * -t) / 3600.0);

    for (k = 0; k < o->num_particles; k++) {
      Particle *p = &sr_particles[o->particles[k]];
      p->vel.x = (float)(p->vel.x + r2->x * pull);
      p->vel.y = (float)(p->vel.y + r2->y * pull);
      p->vel.z = (float)(p->vel.z + r2->z * pull);
    }
  }
}

/* The angular velocity of each wheel body from its angular momentum, and its contacts. */
static void wheel_bodies_move(Object *o, int obj)
{
  int k;

  for (k = 0; k < o->num_wheel_bodies; k++) {
    WheelBody *wb = &o->wheel_bodies[k];
    float f, s;

    wb->pos.x = (float)(wb->vel.x + wb->pos.x);
    wb->pos.y = (float)(wb->pos.y + wb->vel.y);
    wb->pos.z = (float)(wb->vel.z + wb->pos.z);
    wb->vel.y = (float)(wb->vel.y - GRAVITY);
    wb->w.x = 0.0f;
    wb->w.y = 0.0f;
    wb->w.z = 0.0f;
    f = (float)((float)((wb->ang_momentum.x * wb->rot[0].x + wb->ang_momentum.y * wb->rot[0].y) +
                        wb->ang_momentum.z * wb->rot[0].z) *
                wb->inv_inertia.x);
    wb->w.x = (float)(f * wb->rot[0].x + wb->w.x);
    wb->w.y = (float)(f * wb->rot[0].y + wb->w.y);
    wb->w.z = (float)(f * wb->rot[0].z + wb->w.z);
    f = (float)((float)((wb->rot[1].x * wb->ang_momentum.x + wb->ang_momentum.y * wb->rot[1].y) +
                        wb->ang_momentum.z * wb->rot[1].z) *
                wb->inv_inertia.y);
    wb->w.x = (float)(f * wb->rot[1].x + wb->w.x);
    wb->w.y = (float)(f * wb->rot[1].y + wb->w.y);
    wb->w.z = (float)(f * wb->rot[1].z + wb->w.z);
    f = (float)((float)((wb->ang_momentum.x * wb->rot[2].x + wb->ang_momentum.y * wb->rot[2].y) +
                        wb->ang_momentum.z * wb->rot[2].z) *
                wb->inv_inertia.z);
    wb->w.x = (float)(f * wb->rot[2].x + wb->w.x);
    wb->w.y = (float)(f * wb->rot[2].y + wb->w.y);
    wb->w.z = (float)(f * wb->rot[2].z + wb->w.z);
    s = (float)x87_sqrt((float)((wb->w.x * wb->w.x + wb->w.y * wb->w.y) + wb->w.z * wb->w.z));
    if (1.52587890625E-5 < s) {
      float iv = (float)(1.0 / s);
      vec3 a;

      a.x = (float)(iv * wb->w.x);
      a.y = (float)(iv * wb->w.y);
      a.z = (float)(iv * wb->w.z);
      rotateorientation(wb->rot, &a, s);
    }
    objectwheelcollision2(obj, k);
  }
}

/* The hub of each wheel moves with its velocity, then the wheel touches the level. */
static void wheels_move(Object *o, int obj)
{
  const vec3 *r1 = &o->rot[1];
  int k;

  for (k = 0; k < o->num_wheels; k++) {
    Wheel *w = &o->wheels[k];
    const vec3 *pa = &sr_particles[o->particles[w->particle_a]].pos;
    const vec3 *pb = &sr_particles[o->particles[w->particle_b]].pos;
    vec3 e, base;

    w->pos.x = (float)(w->dv.x + w->pos.x);
    w->pos.y = (float)(w->dv.y + w->pos.y);
    w->pos.z = (float)(w->dv.z + w->pos.z);
    w->dv.y = (float)(w->dv.y - GRAVITY);
    base.x = (float)(w->weight_b * (float)(pb->x - pa->x) + pa->x);
    base.y = (float)(w->weight_b * (float)(pb->y - pa->y) + pa->y);
    base.z = (float)(w->weight_b * (float)(pb->z - pa->z) + pa->z);
    e.x = (float)(w->pos.x - (float)(w->ride_height * r1->x + base.x));
    e.y = (float)(w->pos.y - (float)(w->ride_height * r1->y + base.y));
    e.z = (float)(w->pos.z - (float)(w->ride_height * r1->z + base.z));
    w->travel = (float)((r1->y * e.y + e.x * r1->x) + e.z * r1->z);
    w->angle = (float)(w->spin + w->angle);
    objectwheelcollision(obj, k);
  }
}

/* Loop 2, contacts of the particles with the level. Returns 1 when a particle hit the level
 * on the line from the last place of the object. */
static int level_contacts(Object *o, int obj)
{
  const float *ip = g_intersect.pos;
  const float *n = g_intersect.normal;
  int hit = 0;
  int k;

  for (k = 0; k < o->num_particles; k++) {
    int p = o->particles[k];
    Particle *pt = &sr_particles[p];
    vec3 d;
    float s;
    Bond *b;

    d.x = (float)(pt->pos.x - o->pos.x);
    d.y = (float)(pt->pos.y - o->pos.y);
    d.z = (float)(pt->pos.z - o->pos.z);
    s = (float)x87_sqrt((float)((d.x * d.x + d.y * d.y) + d.z * d.z));
    if (64.0 <= s || !lineintersectlevel(&o->pos.x, &pt->pos.x, 0)) {
      if (lineintersectlevel(&o->pos_prev.x, &pt->pos.x, 0)) {
        d.x = (float)(ip[0] - pt->pos.x);
        d.y = (float)(ip[1] - pt->pos.y);
        d.z = (float)(ip[2] - pt->pos.z);
        contact_bond(1, p, n, (float)((n[1] * d.y + n[0] * d.x) + n[2] * d.z), 0.125f, 0.25f);
        b = &sr_bonds[sr_numofbonds - 1];
        b->surface_vel.x = b->surface_vel.y = b->surface_vel.z = 0.0f;
        b->friction = 0.6f;
        hit = 1;
      }
    } else {
      int broke = 0;
      float depth, d2;
      float nr[3];
      vec3 q;

      if (g_intersect.block[0] != -1)
        broke = area_breakblock(g_intersect.block_area, g_intersect.block[0], g_intersect.block[1],
                                g_intersect.block[2], g_intersect.block_word, g_intersect.spec_face, &pt->pos,
                                &pt->vel);
      d.x = (float)(ip[0] - pt->pos.x);
      d.y = (float)(ip[1] - pt->pos.y);
      d.z = (float)(ip[2] - pt->pos.z);
      depth = (float)((d.x * n[0] + n[1] * d.y) + d.z * n[2]);
      nr[0] = n[0];
      nr[1] = n[1];
      nr[2] = n[2];
      q.x = (float)(pt->pos.x + g_yaxis.x * 0.5);
      q.y = (float)(g_yaxis.y * 0.5 + pt->pos.y);
      q.z = (float)(g_yaxis.z * 0.5 + pt->pos.z);
      if (lineintersectlevel(&q.x, &pt->pos.x, 0)) {
        d.x = (float)(ip[0] - pt->pos.x);
        d.y = (float)(ip[1] - pt->pos.y);
        d.z = (float)(ip[2] - pt->pos.z);
        d2 = (float)((d.x * n[0] + n[1] * d.y) + d.z * n[2]);
        if (d2 < depth) {
          nr[0] = n[0];
          nr[1] = n[1];
          nr[2] = n[2];
          depth = d2;
        }
      }
      contact_bond(1, p, nr, depth, broke ? 0.0078125f : 0.125f, broke ? 0.03125f : 0.25f);
      b = &sr_bonds[sr_numofbonds - 1];
      b->surface_vel.x = b->surface_vel.y = b->surface_vel.z = 0.0f;
      b->friction = 0.6f;
      if (o->state == 2 && 20 < k)
        applydamageobject(obj, 100);
    }
  }
  return hit;
}

/* Loop 2: the vehicle code, the wheels and the contacts of one object. */
static void object_move(Object *o, int obj)
{
  int hit = 0;

  if (o->state == 1)
    objectsimulation_car(obj);
  if (o->state == 2)
    objectsimulation_heli(obj);
  if (o->type == 12)
    locomotive(o, obj);
  wheel_bodies_move(o, obj);
  wheels_move(o, obj);
  if (o->state == 3)
    objecttrainsimulation(obj);
  if (o->state != 3)
    hit = level_contacts(o, obj);
  if (!hit)
    o->pos_prev = o->pos;
}

/* Loop 3: the particles of object a against the faces of object b. */
static void object_contacts(Object *a, int ia)
{
  int j, k;

  for (j = 0; j < 1024; j++) {
    Object *b = &sr_objects[j];

    if (j == ia || !b->active || b->state == 3 || a->linked_object == j || b->linked_object == ia)
      continue;
    if (!(b->box_min.x <= a->box_max.x && b->box_min.y <= a->box_max.y && b->box_min.z <= a->box_max.z &&
          a->box_min.x <= b->box_max.x && a->box_min.y <= b->box_max.y && a->box_min.z <= b->box_max.z))
      continue;
    if (a->type == 9 && b->type == 10)
      continue;
    for (k = 0; k < a->num_particles; k++) {
      int p = a->particles[k];
      Particle *pt = &sr_particles[p];

      if (lineintersectobject(j, &a->pos.x, &pt->pos.x, 0)) {
        const float *ip = g_intersect.pos;
        const float *n = g_intersect.normal;
        Bond *bd = &sr_bonds[sr_numofbonds];
        vec3 d;

        d.x = (float)(ip[0] - pt->pos.x);
        d.y = (float)(ip[1] - pt->pos.y);
        d.z = (float)(ip[2] - pt->pos.z);
        bd->type = 3;
        bd->particle_a = p;
        bd->particle_b = g_intersect.tri_particles[0];
        bd->particle_c = g_intersect.tri_particles[1];
        bd->particle_d = g_intersect.tri_particles[2];
        bd->weights[0] = g_intersect.tri_weights[0];
        bd->weights[1] = g_intersect.tri_weights[1];
        bd->weights[2] = g_intersect.tri_weights[2];
        bd->rest_length = (float)((d.x * n[0] + n[1] * d.y) + d.z * n[2]);
        bd->stiffness = 0.0625f;
        bd->damping = 0.25f;
        bd->mode = 0;
        bd->normal.x = n[0];
        bd->normal.y = n[1];
        bd->normal.z = n[2];
        bd->friction = 0.4f;
        sr_numofbonds++;
      }
    }
  }
}

/* The simulation of every object for one tick. */
void objectsimulation(void)
{
  int i;

  nametimer(g_timerlist.objectsim, "objectsim");
  starttimer(g_timerlist.objectsim);
  g_train_near = 0;
  for (i = 0; i < 1024; i++) {
    if (sr_objects[i].active)
      object_update(&sr_objects[i], i);
  }
  for (i = 0; i < 1024; i++) {
    if (sr_objects[i].active)
      object_move(&sr_objects[i], i);
  }
  for (i = 0; i < 1024; i++) {
    if (sr_objects[i].active && sr_objects[i].state != 3)
      object_contacts(&sr_objects[i], i);
  }
  stoptimer(g_timerlist.objectsim);
}
