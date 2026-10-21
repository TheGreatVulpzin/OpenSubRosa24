/* createobject and createvehicle: make an object from an object type.
 *
 * An object copies the lists of its type: one particle for each type particle (placed in the
 * frame of the object), one bond for each type bond, the faces, the frame weights, the wheels
 * and the seats. A car of type 1 also gets a solid body for each wheel. A helicopter gets the
 * six points of its rotor.
 *
 * Float rules: a value that the code stores in a float rounds to a float. A value inside one
 * expression stays a double. The sums keep their order, so the results are bit exact.
 */
#include <string.h>

#include "events.h"
#include "object_types.h"
#include "objects.h"
#include "particles.h"
#include "sr_runtime.h"

/* A solid body for wheel k of a type 1 car. The partner is the other wheel of the axle. */
static void make_wheel_body(Object *o, int k, const vec3 rot[3])
{
  const Wheel *ow = &o->wheels[k];
  const Wheel *m = &o->wheels[k ^ 1];
  WheelBody *b = &o->wheel_bodies[k];
  float sum, r, i0, i1;
  int j;

  b->pos = ow->pos;
  b->rot[0] = rot[0];
  b->rot[1] = rot[1];
  b->rot[2] = rot[2];
  b->mass = ow->mass;
  b->radius = ow->radius;
  b->half_width = 0.125f;

  /* The hub anchor: the four particles of the two wheels of the axle. */
  b->particles[0] = ow->particle_a;
  b->particles[1] = ow->particle_b;
  b->particles[2] = m->particle_a;
  b->particles[3] = m->particle_b;
  b->weights[0] = ow->weight_a;
  b->weights[1] = ow->weight_b;
  b->weights[2] = (float)(m->weight_a * 0.0);
  b->weights[3] = (float)(m->weight_b * 0.0);
  sum = (float)(b->weights[3] + (float)((float)(b->weights[0] + b->weights[1]) + b->weights[2]));
  for (j = 0; j < 4; j++)
    b->weights[j] = (float)(b->weights[j] / sum);
  b->side = (k & 1) ? 0.125f : -0.125f;

  /* The steering anchor: it leans toward the wheel itself. */
  b->steer_particles[0] = ow->particle_a;
  b->steer_particles[1] = ow->particle_b;
  b->steer_particles[2] = m->particle_a;
  b->steer_particles[3] = m->particle_b;
  b->steer_weights[0] = (float)(ow->weight_a * 0.875);
  b->steer_weights[1] = (float)(0.875 * ow->weight_b);
  b->steer_weights[2] = (float)(m->weight_a * 0.125);
  b->steer_weights[3] = (float)(0.125 * m->weight_b);
  sum = (float)(b->steer_weights[3] +
                (float)((float)(b->steer_weights[0] + b->steer_weights[1]) + b->steer_weights[2]));
  for (j = 0; j < 4; j++)
    b->steer_weights[j] = (float)(b->steer_weights[j] / sum);
  b->steer_side = (k & 1) ? -0.125f : 0.125f;

  /* The inertia of a disc with the radius of half the tire. */
  r = (float)(ow->radius * 0.5);
  i0 = (float)(r * r * 0.5);
  i1 = (float)((r * 3.0 * r + 0.0625) / 12.0);
  b->axle_inv_inertia = i0;
  b->axle_inv_inertia_y = i1;
  b->axle_inv_inertia_z = i1;
  b->inv_inertia.x = (float)(1.0 / i0);
  b->inv_inertia.y = (float)(1.0 / i1);
  b->inv_inertia.z = (float)(1.0 / i1);
}

/* The six points of a rotor: the two tips along the z axis, two along the x axis, the hub and
 * the point below it. */
static void make_rotor(Object *o, const vec3 *pos, const vec3 rot[3])
{
  RotorPoint *r = o->rotor;

  r[0].pos.x = (float)(pos->x - rot[2].x * 4.0);
  r[0].pos.y = (float)(pos->y - rot[2].y * 4.0);
  r[0].pos.z = (float)(pos->z - rot[2].z * 4.0);
  r[1].pos.x = (float)(rot[2].x * 4.0 + pos->x);
  r[1].pos.y = (float)(rot[2].y * 4.0 + pos->y);
  r[1].pos.z = (float)(rot[2].z * 4.0 + pos->z);
  r[2].pos.x = (float)(pos->x - rot[0].x);
  r[2].pos.y = (float)(pos->y - rot[0].y);
  r[2].pos.z = (float)(pos->z - rot[0].z);
  r[3].pos.x = (float)(pos->x + rot[0].x);
  r[3].pos.y = (float)(pos->y + rot[0].y);
  r[3].pos.z = (float)(pos->z + rot[0].z);
  r[4].pos = *pos;
  r[5].pos.x = (float)(pos->x - rot[1].x);
  r[5].pos.y = (float)(pos->y - rot[1].y);
  r[5].pos.z = (float)(pos->z - rot[1].z);
}

/* Make an object of the given type. The result is the object, or -1 when all 1024 are in use.
 * vel is the velocity of every particle, or NULL for 0. */
int createobject(int type, const vec3 *pos, const vec3 *vel, const vec3 rot[3], int color)
{
  const ObjectType *t = &sr_objecttypes[type];
  Object *o;
  int object, k;

  for (object = 0; object < 1024; object++)
    if (sr_objects[object].active == 0)
      break;
  if (object == 1024)
    return -1;
  o = &sr_objects[object];
  memset(o, 0, sizeof *o);
  o->active = 1;
  o->type = type;
  o->health = 100;
  o->time_to_live = 0xffff;
  o->color = color;
  o->driver = -1;
  o->state = t->state;
  o->pos = *pos;
  o->pos_prev = *pos;
  o->rot[0] = rot[0];
  o->rot[1] = rot[1];
  o->rot[2] = rot[2];
  o->num_frame_particles = t->num_frame_particles;
  o->engine_power = (float)(t->engine_power * 213.0);
  o->engine_inertia = 32.0f;
  o->engine_max = 199.99998474121094f;
  o->engine_scale = 0.005000000353902578f;

  /* One particle for each type particle, turned by rot and moved to pos. */
  for (k = 0; k < t->num_particles; k++) {
    const vec3 *p = &t->particles[k].pos;
    vec3 w, a;

    w.x = (float)((p->y * rot[1].x + p->x * rot[0].x) + p->z * rot[2].x);
    w.y = (float)((p->x * rot[0].y + p->y * rot[1].y) + p->z * rot[2].y);
    w.z = (float)((p->y * rot[1].z + p->x * rot[0].z) + p->z * rot[2].z);
    a.x = (float)(pos->x + w.x);
    a.y = (float)(w.y + pos->y);
    a.z = (float)(w.z + pos->z);
    o->particles[o->num_particles] = createparticle(0, &a, vel, (float)(t->particles[k].mass * t->mass), object);
    o->num_particles++;
  }
  for (k = 0; k < t->num_bonds; k++)
    createbond(t->bonds[k].subtype, o->particles[t->bonds[k].particle_a], o->particles[t->bonds[k].particle_b], -1);

  /* The faces. The type face has one more float (extra) than the object face has room for:
   * armor and the window flag move 4 bytes down. */
  o->num_faces = t->num_faces;
  for (k = 0; k < t->num_faces; k++) {
    const TypeFace *tf = &t->faces[k];
    ObjectFace *f = &o->faces[k];

    f->corners = tf->corners;
    memcpy(f->verts, tf->verts, sizeof f->verts);
    memcpy(f->links, tf->links, sizeof f->links);
    memcpy(f->link_weights, tf->link_weights, sizeof f->link_weights);
    f->armor = tf->armor;
    f->is_window = tf->is_window;
    f->health = 8;
  }
  for (k = 0; k < o->num_particles; k++) {
    o->frame_weight_x[k] = t->frame_weight_x[k];
    o->frame_weight_z[k] = t->frame_weight_z[k];
  }

  o->axle_width = t->axle_width;
  o->wheelbase = t->wheelbase;
  o->num_wheels = t->num_wheels;
  for (k = 0; k < t->num_wheels; k++) {
    const TypeWheel *tw = &t->wheels[k];
    Wheel *w = &o->wheels[k];
    const vec3 *pa = &sr_particles[o->particles[tw->particle_a]].pos;
    const vec3 *pb = &sr_particles[o->particles[tw->particle_b]].pos;
    float w1 = tw->weight_b;

    w->pos.x = (float)(pa->x + w1 * (float)(pb->x - pa->x));
    w->pos.y = (float)((float)(pb->y - pa->y) * w1 + pa->y);
    w->pos.z = (float)(w1 * (float)(pb->z - pa->z) + pa->z);
    w->popped = tw->popped;
    w->deflated = tw->deflated;
    w->health = 8;
    w->particle_a = tw->particle_a;
    w->particle_b = tw->particle_b;
    w->weight_a = tw->weight_a;
    w->weight_b = tw->weight_b;
    w->mass = tw->mass;
    w->radius = tw->radius;
    w->inv_inertia = tw->inv_inertia;
    w->ride_height = tw->ride_height;
    w->spring = tw->suspension[0];
    w->damper = tw->suspension[1];
    w->vertical = tw->suspension[2];
  }

  if (sr_objecttypes[type].num_wheels >= 1 && type == 1) {
    int n = sr_objecttypes[1].num_wheels;

    o->num_wheel_bodies = n;
    o->num_wheels = 0;
    for (k = 0; k < n; k++)
      make_wheel_body(o, k, rot);
  } else if (type == 11) {
    make_rotor(o, pos, rot);
  }

  /* The two rear wheels drive. */
  o->num_driven = 2;
  o->driven_wheels[0] = 2;
  o->driven_wheels[1] = 3;
  o->num_seats = t->num_seats;
  for (k = 0; k < t->num_seats; k++)
    o->seats[k] = t->seats[k];
  o->linked_object = -1;
  for (k = 0; k < 7; k++)
    o->unused_5be8[k] = -1;
  createevent_createobject(object);
  return object;
}

/* Make a train: the locomotive (type 12) and two bogies (type 13) tied to it. Only type 0 is
 * known. Another type gives -1. rot is changed to the frame of the locomotive. */
int createvehicle(int type, const vec3 *pos, vec3 rot[3])
{
  Object *loco;
  Object *bogie;
  vec3 at;
  int l, b1, b2, p, j;

  if (type != 0)
    return -1;
  at = *pos;
  l = createobject(12, &at, NULL, rot, 0);
  loco = &sr_objects[l];
  loco->track_target = 0x4a;
  rot[0] = loco->rot[0];
  rot[1] = loco->rot[1];
  rot[2] = loco->rot[2];

  /* The front bogie, behind the locomotive. */
  at.x = (float)((float)(loco->pos.x - rot[2].x * 5.5) + rot[1].x * 0.5);
  at.y = (float)(rot[1].y * 0.5 + (float)(loco->pos.y - rot[2].y * 5.5));
  at.z = (float)(rot[1].z * 0.5 + (float)(loco->pos.z - rot[2].z * 5.5));
  b1 = createobject(13, &at, NULL, rot, 0);
  bogie = &sr_objects[b1];
  bogie->linked_object = l;
  for (p = 0; p < 12; p++)
    if (p < 4 || (p >= 6 && p < 10))
      for (j = 4; j < 6; j++)
        createbond(0, loco->particles[p], bogie->particles[j], -1);

  rot[0] = loco->rot[0];
  rot[1] = loco->rot[1];
  rot[2] = loco->rot[2];
  at.x = (float)((float)(loco->pos.x + rot[2].x * 5.5) + rot[1].x * 0.5);
  at.y = (float)(rot[1].y * 0.5 + (float)(rot[2].y * 5.5 + loco->pos.y));
  at.z = (float)(rot[1].z * 0.5 + (float)(rot[2].z * 5.5 + loco->pos.z));
  b2 = createobject(13, &at, NULL, rot, 0);
  bogie = &sr_objects[b2];
  bogie->linked_object = l;
  for (p = 2; p < 12; p++)
    if (p < 6 || p > 7)
      for (j = 4; j < 6; j++)
        createbond(0, loco->particles[p], bogie->particles[j], -1);
  return l;
}
