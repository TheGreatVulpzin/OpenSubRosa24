/* The wheels of the objects: suspension, wheel bodies, and the contacts with the ground
 * and the rails.
 *
 * objectwheelsimulation2 runs once for each solver pass, 32 times for each tick. A wheel is
 * a hub that hangs from two particles of the object on a spring. In the first pass the code
 * computes the shares of the masses and the spring offset and keeps them in the wheel record.
 * The other passes read them again. A wheel body (type 1 cars) is a solid wheel with an
 * angular velocity.
 *
 * Float rules: a value that the code stores in a float rounds to a float. A value inside one
 * expression stays a double. The sums keep their order, so the results are bit exact.
 */
#include <stdint.h>

#include "collide.h"
#include "objects.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define g_bondpass (*SR_AT(int, 0x203dd0))        /* the number of the solver pass */
#define g_intersect (*SR_AT(Intersect, SR_RVA_INTERSECT))
#define g_track SR_AT(TrackPiece, 0x2597efc0)
#define g_numoftracks (*SR_AT(int, 0x90dc520))
/* The friction of a level material. The table has no name yet. */
#define g_material_friction(m) (*SR_AT(float, 0x16d50030u + (unsigned)(m) * 0x58u))

/* dst = f * v + dst, each component rounded to a float. */
static void add_scaled(vec3 *dst, float f, const vec3 *v)
{
  dst->x = (float)(f * v->x + dst->x);
  dst->y = (float)(f * v->y + dst->y);
  dst->z = (float)(f * v->z + dst->z);
}

/* The dot product with the rounding order of the wheel code. */
static float dot3(const vec3 *a, const vec3 *b)
{
  return (float)((a->x * b->x + a->y * b->y) + a->z * b->z);
}

/* The angular velocity of the object from the 8 corner particles. */
void getobjectrotation3(vec3 *out, int obj)
{
  static const int corner[8] = {2, 3, 5, 4, 10, 11, 13, 12};
  const Object *o = &sr_objects[obj];
  vec3 c = {0.0f, 0.0f, 0.0f};
  float w[3] = {0.0f, 0.0f, 0.0f};
  float sum[3] = {0.0f, 0.0f, 0.0f};
  vec3 o3;
  int j, r;

  for (j = 0; j < 8; j++) {
    const Particle *p = &sr_particles[o->particles[corner[j]]];
    c.x = (float)(p->pos.x + c.x);
    c.y = (float)(p->pos.y + c.y);
    c.z = (float)(p->pos.z + c.z);
  }
  c.x = (float)(c.x * 0.125);
  c.y = (float)(c.y * 0.125);
  c.z = (float)(c.z * 0.125);
  for (j = 0; j < 8; j++) {
    const Particle *p = &sr_particles[o->particles[corner[j]]];

    for (r = 0; r < 3; r++) {
      const vec3 *n = &o->rot[r];
      vec3 v, q;
      float d, nd, s;

      v.x = (float)(p->pos.x - c.x);
      v.y = (float)(p->pos.y - c.y);
      v.z = (float)(p->pos.z - c.z);
      d = (float)((v.y * n->y + v.x * n->x) + v.z * n->z);
      nd = -d;
      v.x = (float)(v.x + n->x * nd);
      v.y = (float)(v.y + n->y * nd);
      v.z = (float)(v.z + n->z * nd);
      s = object_normalize(&v, &v);
      q.x = (float)(v.z * p->vel.y - v.y * p->vel.z);
      q.y = (float)(v.x * p->vel.z - v.z * p->vel.x);
      q.z = (float)(v.y * p->vel.x - v.x * p->vel.y);
      w[r] = (float)((float)((n->y * q.y + n->x * q.x) + n->z * q.z) + w[r]);
      sum[r] = (float)(sum[r] + s);
    }
  }
  for (r = 0; r < 3; r++)
    w[r] = (float)(w[r] / sum[r]);
  o3.x = w[0];
  o3.y = w[1];
  o3.z = w[2];
  out->x = (float)(o->rot[0].x * o3.x);
  out->y = (float)(o->rot[0].y * o3.x);
  out->z = (float)(o->rot[0].z * o3.x);
  out->x = (float)(o->rot[1].x * o3.y + out->x);
  out->y = (float)(o->rot[1].y * o3.y + out->y);
  out->z = (float)(o->rot[1].z * o3.y + out->z);
  out->x = (float)(o->rot[2].x * o3.z + out->x);
  out->y = (float)(o->rot[2].y * o3.z + out->y);
  out->z = (float)(o->rot[2].z * o3.z + out->z);
}

/* The spring and the damper of each wheel: they push the hub and the two particles. */
static void wheel_springs(Object *o)
{
  const vec3 *r1 = &o->rot[1];
  int k;

  for (k = 0; k < o->num_wheels; k++) {
    Wheel *w = &o->wheels[k];
    int ia = o->particles[w->particle_a];
    int ib = o->particles[w->particle_b];
    const Particle *pa = &sr_particles[ia];
    const Particle *pb = &sr_particles[ib];
    BondParticle *ba = &sr_bondparticles[ia];
    BondParticle *bb = &sr_bondparticles[ib];
    float sw, sb, c;
    vec3 off, v, s, u, f;
    float dt, du;

    if (g_bondpass == 0) {
      vec3 t;
      float m = (float)(pa->mass * w->weight_a + pb->mass * w->weight_b);
      float tot = (float)(w->mass + m);

      sw = (float)(m / tot);
      sb = (float)(w->mass / tot);
      w->share_wheel = sw;
      w->share_body = sb;
      t.x = (float)(pb->pos.x - pa->pos.x);
      t.y = (float)(pb->pos.y - pa->pos.y);
      t.z = (float)(pb->pos.z - pa->pos.z);
      t.x = (float)(t.x * w->weight_b + pa->pos.x);
      t.y = (float)(t.y * w->weight_b + pa->pos.y);
      t.z = (float)(t.z * w->weight_b + pa->pos.z);
      t.x = (float)(t.x - w->pos.x);
      t.y = (float)(t.y - w->pos.y);
      t.z = (float)(t.z - w->pos.z);
      t.x = (float)(r1->x * w->ride_height + t.x);
      t.y = (float)(r1->y * w->ride_height + t.y);
      t.z = (float)(r1->z * w->ride_height + t.z);
      off.x = (float)(w->spring * t.x);
      off.y = (float)(w->spring * t.y);
      off.z = (float)(w->spring * t.z);
      w->spring_offset = off;
    } else {
      sw = w->share_wheel;
      sb = w->share_body;
      off = w->spring_offset;
    }
    v.x = (float)(w->vel.x * -w->damper);
    v.y = (float)(w->vel.y * -w->damper);
    v.z = (float)(w->vel.z * -w->damper);
    c = (float)(w->damper * w->weight_a);
    add_scaled(&v, c, &ba->vel);
    c = (float)(w->damper * w->weight_b);
    add_scaled(&v, c, &bb->vel);
    s.x = (float)(v.x + off.x);
    s.y = (float)(v.y + off.y);
    s.z = (float)(v.z + off.z);
    dt = dot3(&s, r1);
    add_scaled(&s, -dt, r1);
    add_scaled(&w->dv, sw, &s);
    c = (float)(-sb * w->weight_a);
    add_scaled(&ba->dv, c, &s);
    c = (float)(-sb * w->weight_b);
    add_scaled(&bb->dv, c, &s);

    c = (float)(w->vertical * 0.375);
    u.x = (float)((float)(w->vertical * v.x) + (float)(c * off.x));
    u.y = (float)((float)(w->vertical * v.y) + (float)(c * off.y));
    u.z = (float)((float)(w->vertical * v.z) + (float)(c * off.z));
    du = dot3(&u, r1);
    f.x = (float)(du * r1->x);
    f.y = (float)(du * r1->y);
    f.z = (float)(du * r1->z);
    add_scaled(&w->dv, sw, &f);
    c = (float)(-sb * w->weight_a);
    add_scaled(&ba->dv, c, &f);
    c = (float)(-sb * w->weight_b);
    add_scaled(&bb->dv, c, &f);
  }
}

/* One wheel body: the angular push, the hub spring and the damping of its four particles. */
static void wheel_body(Object *o, int obj, int k)
{
  WheelBody *wb = &o->wheel_bodies[k];
  const vec3 *r1 = &o->rot[1];
  int idx[4];
  const Particle *pp[4];
  BondParticle *bp[4];
  vec3 ax = o->rot[0];
  vec3 wp, t, r, rp, hub, v, hb, vb, f;
  float d, nd, d2, nd2, e, g, tr, h, d3, n3, g3, d4, n4, g4;
  float mab, m, tot, s1, s2, ns2;
  int n;

  if (k == 0)
    rotatevector(&ax, r1, o->wheels[0].steer);
  if (k == 1)
    rotatevector(&ax, r1, o->wheels[1].steer);
  d = (float)((ax.y * wb->w.y + wb->w.x * ax.x) + wb->w.z * ax.z);
  nd = -d;
  wp.x = (float)(ax.x * nd + wb->w.x);
  wp.y = (float)(ax.y * nd + wb->w.y);
  wp.z = (float)(ax.z * nd + wb->w.z);
  t.x = (float)(ax.y * wb->rot[0].z - ax.z * wb->rot[0].y);
  t.y = (float)(wb->rot[0].x * ax.z - wb->rot[0].z * ax.x);
  t.z = (float)(wb->rot[0].y * ax.x - wb->rot[0].x * ax.y);
  t.x = (float)((float)(t.x * 0.25) - wp.x * 0.5);
  t.y = (float)((float)(t.y * 0.25) - wp.y * 0.5);
  t.z = (float)((float)(t.z * 0.25) - wp.z * 0.5);
  getobjectrotation3(&r, obj);
  d2 = (float)((r.x * ax.x + ax.y * r.y) + ax.z * r.z);
  nd2 = -d2;
  rp.x = (float)(r.x + ax.x * nd2);
  rp.y = (float)(r.y + ax.y * nd2);
  rp.z = (float)(r.z + ax.z * nd2);
  t.x = (float)(rp.x * 0.5 + t.x);
  t.y = (float)(rp.y * 0.5 + t.y);
  t.z = (float)(rp.z * 0.5 + t.z);
  g = wb->axle_inv_inertia;
  e = (float)(t.x * g + 0.0);
  wb->dw.x = (float)(e * 0.5 + wb->dw.x);
  e = (float)(t.y * g + 0.0);
  wb->dw.y = (float)(e * 0.5 + wb->dw.y);
  e = (float)(t.z * g + 0.0);
  wb->dw.z = (float)(e * 0.5 + wb->dw.z);

  for (n = 0; n < 4; n++) {
    idx[n] = o->particles[wb->particles[n]];
    pp[n] = &sr_particles[idx[n]];
    bp[n] = &sr_bondparticles[idx[n]];
  }
  hub.x = (float)(pp[0]->pos.x * wb->weights[0]);
  hub.y = (float)(pp[0]->pos.y * wb->weights[0]);
  hub.z = (float)(pp[0]->pos.z * wb->weights[0]);
  for (n = 1; n < 4; n++) {
    hub.x = (float)(wb->weights[n] * pp[n]->pos.x + hub.x);
    hub.y = (float)(wb->weights[n] * pp[n]->pos.y + hub.y);
    hub.z = (float)(wb->weights[n] * pp[n]->pos.z + hub.z);
  }
  hub.x = (float)(hub.x - r1->x * 0.125);
  hub.y = (float)(hub.y - r1->y * 0.125);
  hub.z = (float)(hub.z - r1->z * 0.125);
  hub.x = (float)(hub.x - wb->pos.x);
  hub.y = (float)(hub.y - wb->pos.y);
  hub.z = (float)(hub.z - wb->pos.z);
  tr = (float)((r1->x * hub.x + hub.y * r1->y) + hub.z * r1->z);
  o->wheels[k].travel = -tr;
  hub.x = (float)(hub.x * 0.25);
  hub.y = (float)(hub.y * 0.25);
  hub.z = (float)(hub.z * 0.25);

  v.x = (float)(wb->vel.x + 0.0);
  v.y = (float)(wb->vel.y + 0.0);
  v.z = (float)(wb->vel.z + 0.0);
  v.x = (float)(v.x * -0.5);
  v.y = (float)(v.y * -0.5);
  v.z = (float)(v.z * -0.5);
  for (n = 0; n < 4; n++) {
    h = (float)(wb->weights[n] * 0.5);
    v.x = (float)(bp[n]->vel.x * h + v.x);
    v.y = (float)(bp[n]->vel.y * h + v.y);
    v.z = (float)(bp[n]->vel.z * h + v.z);
  }
  d3 = (float)((r1->x * hub.x + hub.y * r1->y) + hub.z * r1->z);
  n3 = -d3;
  hb.x = (float)(hub.x + n3 * r1->x);
  hb.y = (float)(hub.y + n3 * r1->y);
  hb.z = (float)(hub.z + n3 * r1->z);
  g3 = (float)((0.125 * d3) * 0.375);
  hb.x = (float)(r1->x * g3 + hb.x);
  hb.y = (float)(r1->y * g3 + hb.y);
  hb.z = (float)(r1->z * g3 + hb.z);
  d4 = (float)((v.x * r1->x + v.y * r1->y) + v.z * r1->z);
  n4 = -d4;
  vb.x = (float)(v.x + n4 * r1->x);
  vb.y = (float)(v.y + n4 * r1->y);
  vb.z = (float)(v.z + n4 * r1->z);
  g4 = (float)((0.375 * d4) * 0.5);
  vb.x = (float)(r1->x * g4 + vb.x);
  vb.y = (float)(r1->y * g4 + vb.y);
  vb.z = (float)(r1->z * g4 + vb.z);
  f.x = (float)(vb.x + hb.x);
  f.y = (float)(vb.y + hb.y);
  f.z = (float)(vb.z + hb.z);

  mab = (float)(pp[0]->mass * wb->weights[0] + pp[1]->mass * wb->weights[1]);
  m = (float)((pp[2]->mass * wb->weights[2] + pp[3]->mass * wb->weights[3]) + mab);
  tot = (float)(wb->mass + m);
  s1 = (float)(m / tot);
  s2 = (float)(wb->mass / tot);
  add_scaled(&wb->dv, s1, &f);
  ns2 = -s2;
  for (n = 0; n < 4; n++) {
    float c = (float)(wb->weights[n] * ns2);
    add_scaled(&bp[n]->dv, c, &f);
  }
}

/* The wheels of every object, once for each solver pass. */
void objectwheelsimulation2(void)
{
  int i, k;

  for (i = 0; i < 1024; i++) {
    Object *o = &sr_objects[i];

    if (!o->active)
      continue;
    o->engine_accel = 0.0f;
    for (k = 0; k < o->num_wheels; k++) {
      Wheel *w = &o->wheels[k];
      w->dspin = 0.0f;
      w->vel = w->dv;
      w->slip = 0.0f;
    }
    wheel_springs(o);
    for (k = 0; k < o->num_wheel_bodies; k++)
      wheel_body(o, i, k);
  }
}

/* A wheel touches the ground: a bond of type 5 with the ground normal and the rolling direction. */
void objectwheelcollision(int obj, int wheel)
{
  Object *o = &sr_objects[obj];
  Wheel *w = &o->wheels[wheel];
  vec3 c = w->pos;
  int pa = o->particles[w->particle_a];
  int pb = o->particles[w->particle_b];
  const float *ip = g_intersect.pos;
  const float *n = g_intersect.normal;
  Bond *b;
  vec3 d, a, c1;

  if (!circleintersectlevel(&c.x, &o->rot[0].x, w->radius))
    return;
  d.x = (float)(c.x - ip[0]);
  d.y = (float)(c.y - ip[1]);
  d.z = (float)(c.z - ip[2]);
  b = &sr_bonds[sr_numofbonds];
  b->type = 5;
  b->particle_a = pa;
  b->particle_b = pb;
  b->weights[0] = w->weight_a;
  b->weights[1] = w->weight_b;
  b->rest_length = (float)(w->radius - (float)((n[1] * d.y + n[0] * d.x) + n[2] * d.z));
  b->stiffness = 0.0625f;
  b->damping = 0.25f;
  b->mode = 0;
  b->normal.x = n[0];
  b->normal.y = n[1];
  b->normal.z = n[2];
  b->friction = w->popped ? 0.30000001192092896f : g_material_friction(g_intersect.material);
  b->owner = obj;
  b->bone_a = wheel;
  a = o->rot[2];
  rotatevector(&a, &o->rot[1], w->steer);
  c1.x = (float)(n[1] * a.z - n[2] * a.y);
  c1.y = (float)(n[2] * a.x - a.z * n[0]);
  c1.z = (float)(a.y * n[0] - a.x * n[1]);
  b->roll_dir.x = (float)(n[1] * c1.z - n[2] * c1.y);
  b->roll_dir.y = (float)(n[2] * c1.x - c1.z * n[0]);
  b->roll_dir.z = (float)(n[0] * c1.y - n[1] * c1.x);
  normalizevector(&b->roll_dir, &b->roll_dir);
  sr_numofbonds++;
}

/* A wheel body touches the ground at the left or right edge of the tire. */
void objectwheelcollision2(int obj, int body)
{
  Object *o = &sr_objects[obj];
  WheelBody *wb = &o->wheel_bodies[body];
  const float *ip = g_intersect.pos;
  const float *n = g_intersect.normal;
  int side;

  for (side = 0; side < 2; side++) {
    float h = side == 0 ? -wb->half_width : wb->half_width;
    vec3 p, d, c;
    Bond *b;
    float r;

    p.x = (float)(wb->pos.x + h * wb->rot[0].x);
    p.y = (float)(wb->rot[0].y * h + wb->pos.y);
    p.z = (float)(h * wb->rot[0].z + wb->pos.z);
    if (!circleintersectlevel(&p.x, &wb->rot[0].x, wb->radius))
      continue;
    d.x = (float)(ip[0] - p.x);
    d.y = (float)(ip[1] - p.y);
    d.z = (float)(ip[2] - p.z);
    b = &sr_bonds[sr_numofbonds];
    b->type = 15;
    b->owner = obj;
    b->bone_a = body;
    normalizevector(&d, &d);
    r = wb->radius;
    c.x = (float)(p.x + r * d.x);
    c.y = (float)(d.y * r + p.y);
    c.z = (float)(r * d.z + p.z);
    b->rest_length = (float)((n[1] * (float)(ip[1] - c.y) + n[0] * (float)(ip[0] - c.x)) + n[2] * (float)(ip[2] - c.z));
    b->stiffness = 0.03125f;
    b->damping = 0.0625f;
    b->friction = g_material_friction(g_intersect.material);
    b->normal.x = n[0];
    b->normal.y = n[1];
    b->normal.z = n[2];
    b->local_a.x = (float)(ip[0] - wb->pos.x);
    b->local_a.y = (float)(ip[1] - wb->pos.y);
    b->local_a.z = (float)(ip[2] - wb->pos.z);
    sr_numofbonds++;
  }
}

/* A contact of a point of a train car with the track: a bond that holds it on the rails. */
static void track_bond(int particle, float stiffness, float damping)
{
  const float *ip = g_intersect.pos;
  const float *n = g_intersect.normal;
  const Particle *p = &sr_particles[particle];
  vec3 d;
  float rest;

  d.x = (float)(ip[0] - p->pos.x);
  d.y = (float)(ip[1] - p->pos.y);
  d.z = (float)(ip[2] - p->pos.z);
  rest = (float)((n[1] * d.y + n[0] * d.x) + n[2] * d.z);
  contact_bond(1, particle, n, rest, stiffness, damping);
  sr_bonds[sr_numofbonds - 1].surface_vel.x = 0.0f;
  sr_bonds[sr_numofbonds - 1].surface_vel.y = 0.0f;
  sr_bonds[sr_numofbonds - 1].surface_vel.z = 0.0f;
  sr_bonds[sr_numofbonds - 1].friction = 0.0f;
}

/* A train car: the first four particles touch the track pieces near the car. */
void objecttrainsimulation(int obj)
{
  Object *o = &sr_objects[obj];
  int t, k;

  for (t = 0; t < g_numoftracks; t++) {
    const TrackPiece *tp = &g_track[t];

    if (!(tp->box_min.x <= o->box_max.x && tp->box_min.z <= o->box_max.z && o->box_min.x <= tp->box_max.x &&
          o->box_min.z <= tp->box_max.z))
      continue;
    for (k = 0; k < 4; k++) {
      int p = o->particles[k];

      if (lineintersecttrack(&o->pos.x, &sr_particles[p].pos.x, t)) {
        track_bond(p, 0.015625f, 0.25f);
        o->track_last = t;
        if (o->linked_object != -1)
          sr_objects[o->linked_object].track_last = t;
      }
      if (lineintersecttrackrail(&o->pos.x, &sr_particles[p].pos.x, t))
        track_bond(p, 0.00390625f, 0.125f);
    }
  }
}
