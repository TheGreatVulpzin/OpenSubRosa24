/* The vehicles: the engine and the helicopter.
 *
 * objectenginesimulation2 runs once for each solver pass, 32 times for each tick. It drives
 * the wheels of cars (spin) and the wheel bodies of type 1 cars (angular velocity).
 * objectsimulation_heli turns the rotor, pushes the particles with the air drag and the lift,
 * and turns the body with the drive input.
 *
 * Float rules: a value that the code stores in a float rounds to a float. A value inside one
 * expression stays a double. The sums keep their order, so the results are bit exact.
 */
#include <math.h>
#include <stdint.h>

#include "objects.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

#define PI 3.14159265359

/* The torque of the engine for the rpm of the object. It is 0 above the rpm limit. */
float getenginetorque2(int obj)
{
  const Object *o = &sr_objects[obj];
  float x = (float)((((o->engine_rpm * 60.0) * 60.0) / 6.28318530718) / 7000.0);
  float t, y, u;

  if (x > 1.0)
    return 0.0f;
  if (x < 0.0)
    x = 0.0f;
  t = (float)(1.0 - x);
  y = (float)(((((t * 0.75) * t) * x + ((0.0 * t) * t) * t) + ((t * 2.25) * x) * x) + x * (x * x));
  u = (float)(1.0 - y);
  if (o->type == 7)
    return (float)((u * u + (u * 2.5) * y) + (y * 0.625) * y);
  return (float)((((0.75 * u) * u) + (u + u) * y) + (0.625 * y) * y);
}

/* Add the velocity of a turn around the point center to each particle of the object. */
void addobjectrotation(int obj, const vec3 *center, const vec3 *axis, float k)
{
  const Object *o = &sr_objects[obj];
  int i;

  for (i = 0; i < o->num_particles; i++) {
    Particle *p = &sr_particles[o->particles[i]];
    vec3 d;

    d.x = (float)(p->pos.x - center->x);
    d.y = (float)(p->pos.y - center->y);
    d.z = (float)(p->pos.z - center->z);
    p->vel.x = (float)((float)(axis->z * d.y - axis->y * d.z) * k + p->vel.x);
    p->vel.y = (float)((float)(d.z * axis->x - axis->z * d.x) * k + p->vel.y);
    p->vel.z = (float)((float)(d.x * axis->y - d.y * axis->x) * k + p->vel.z);
  }
}

/* The acceleration of the engine: the torque minus the drag of the rpm. */
static float engine_accel(int obj, const Object *o)
{
  float torque = getenginetorque2(obj);
  float power = o->engine_power;
  float accel = o->engine_accel;

  return (float)((float)(((((power / 3600.0) * (torque * o->throttle)) / (o->engine_scale * o->engine_inertia)) * 0.03125) +
                         accel) -
                 (1.125 - o->throttle) * ((((o->engine_rpm * 0.125) * 0.125) * 0.125) * 0.25));
}

/* The brake force of wheel k. The first two wheels stop braking above a brake value of 1. */
static float wheel_brake(float brake, int k)
{
  if (k >= 2)
    return (float)(brake / 60.0);
  return !(brake <= 1.0) ? 0.0f : (float)(brake / 60.0);
}

/* Type 1 cars: the wheel bodies get the brake and the drive as angular velocity. */
static void engine_bodies(int obj, Object *o)
{
  float sum = 0.0f;
  int k;

  for (k = 0; k < o->num_wheel_bodies; k++) {
    WheelBody *wb = &o->wheel_bodies[k];
    float s = (float)((wb->w.y * wb->rot[0].y + wb->w.x * wb->rot[0].x) + wb->w.z * wb->rot[0].z);
    float t4 = (float)(wheel_brake(o->brake, k) * 4.0);
    float l, f;

    if (s < -t4)
      l = t4;
    else if (s > t4)
      l = -t4;
    else
      l = -s;
    f = (float)((float)(l * 0.5) * wb->axle_inv_inertia);
    wb->dw.x = (float)(f * wb->rot[0].x + wb->dw.x);
    wb->dw.y = (float)(f * wb->rot[0].y + wb->dw.y);
    wb->dw.z = (float)(f * wb->rot[0].z + wb->dw.z);
  }
  o->engine_accel = engine_accel(obj, o);
  o->differential = 0.0f;
  o->engine_rpm = (float)(o->engine_rpm + o->engine_accel);
  o->engine_accel = 0.0f;
  if (o->gear_ratio != 0.0) {
    for (k = 0; k < o->num_driven; k++) {
      WheelBody *wb = &o->wheel_bodies[o->driven_wheels[k]];
      float f3 = (float)(fabs(o->gear_ratio_inv) * wb->mass);
      float f4 = (float)((o->engine_scale * o->engine_inertia) * fabs(o->gear_ratio));
      float f5 = (float)(1.0 / (f3 + f4));
      float q3 = (float)(f5 * f3);
      float q4 = (float)(f5 * f4);
      float dd = (float)((wb->w.y * o->rot[0].y + wb->w.x * o->rot[0].x) + wb->w.z * o->rot[0].z);
      float f2 = (float)((float)((float)(o->engine_rpm - dd * o->gear_ratio) * o->clutch) * 0.015625);
      float f = (float)((q4 * f2) * o->gear_ratio_inv);

      wb->dw.x = (float)(f * o->rot[0].x + wb->dw.x);
      wb->dw.y = (float)(f * o->rot[0].y + wb->dw.y);
      wb->dw.z = (float)(f * o->rot[0].z + wb->dw.z);
      sum = (float)(sum - q3 * f2);
    }
  }
  o->engine_rpm = (float)(o->engine_rpm + sum);
}

/* Other cars: the wheels get the brake, the differential and the drive as spin. */
static void engine_wheels(int obj, Object *o)
{
  float sum = 0.0f;
  float d;
  double step;
  Wheel *a, *b;
  int k;

  for (k = 0; k < o->num_wheels; k++) {
    Wheel *w = &o->wheels[k];
    w->spin = (float)(w->inv_inertia * w->dspin + w->spin);
  }
  for (k = 0; k < o->num_wheels; k++) {
    Wheel *w = &o->wheels[k];
    float t = wheel_brake(o->brake, k);

    if (w->spin < -t)
      w->spin = (float)(t + w->spin);
    else if (w->spin > t)
      w->spin = (float)(w->spin - t);
    else
      w->spin = 0.0f;
  }
  o->engine_accel = engine_accel(obj, o);
  a = &o->wheels[o->driven_wheels[0]];
  b = &o->wheels[o->driven_wheels[1]];
  d = (float)(o->differential - (a->spin - b->spin));
  o->differential = (float)(o->differential - 0.9846153855323792 * d);
  step = d * 0.015384615398943424;
  a->spin = (float)(a->spin + step);
  b->spin = (float)(b->spin - step);
  o->engine_rpm = (float)(o->engine_rpm + o->engine_accel);
  o->engine_accel = 0.0f;
  if (o->gear_ratio != 0.0) {
    for (k = 0; k < o->num_driven; k++) {
      Wheel *w = &o->wheels[o->driven_wheels[k]];
      float f3 = (float)((w->mass / w->inv_inertia) * fabs(o->gear_ratio_inv));
      float f4 = (float)((o->engine_scale * o->engine_inertia) * fabs(o->gear_ratio));
      float f5 = (float)(1.0 / (f3 + f4));
      float q3 = (float)(f5 * f3);
      float q4 = (float)(f5 * f4);
      float l = (float)(o->engine_rpm - o->gear_ratio * w->spin);
      float f2;

      if (k == 0)
        l = (float)((o->differential * 0.5) * o->gear_ratio + l);
      if (k == 1)
        l = (float)(l - (o->differential * 0.5) * o->gear_ratio);
      f2 = (float)((float)(o->clutch * l) * 0.125);
      w->spin = (float)(w->spin + (q4 * f2) * o->gear_ratio_inv);
      sum = (float)(sum - q3 * f2);
    }
  }
  o->engine_rpm = (float)(o->engine_rpm + sum);
}

/* The engine of every vehicle, once for each solver pass. */
void objectenginesimulation2(void)
{
  int i;

  for (i = 0; i < 1024; i++) {
    Object *o = &sr_objects[i];

    if (!o->active)
      continue;
    if (o->type == 1)
      engine_bodies(i, o);
    else
      engine_wheels(i, o);
  }
}

/* The lift of the helicopter: the angle of the blades for the particle k (21 and on). */
static float blade_angle(const Object *o, int k)
{
  const float *in = o->drive_input;
  float base = (float)((((in[3] * 0.125) * 0.25) * PI) + 0.14726215563703127);

  switch (k) {
  case 21:
    return (float)((PI * ((0.125 * (in[2] * 0.125)) * 0.25)) + base);
  case 22:
    return (float)(base - PI * (0.125 * (in[0] * 0.125)));
  case 23:
    return (float)(base - PI * (0.25 * (0.125 * (in[2] * 0.125))));
  case 24:
    return (float)((PI * (0.125 * (in[0] * 0.125))) + base);
  default:
    return base;
  }
}

/* Add f * axis to the velocity of a particle. */
static void push(Particle *p, float f, const vec3 *axis)
{
  p->vel.x = (float)(f * axis->x + p->vel.x);
  p->vel.y = (float)(f * axis->y + p->vel.y);
  p->vel.z = (float)(f * axis->z + p->vel.z);
}

/* The air drag along the three axes of the object, for the 20 first particles. */
static void heli_drag(Object *o)
{
  int k;

  for (k = 0; k < 20; k++) {
    Particle *p = &sr_particles[o->particles[k]];
    const vec3 *r0 = &o->rot[0];
    const vec3 *r1 = &o->rot[1];
    const vec3 *r2 = &o->rot[2];
    vec3 n;
    float s, s2, f;

    p->vel_prev = p->vel;
    s = (float)x87_sqrt((float)((p->vel_prev.y * p->vel_prev.y + p->vel_prev.x * p->vel_prev.x) + p->vel_prev.z * p->vel_prev.z));
    s2 = (float)(s * s);
    normalizevector(&n, &p->vel_prev);
    f = (float)((((4.0 * s2) * 0.5) / -1000.0) * (float)((n.y * r0->y + n.x * r0->x) + n.z * r0->z));
    push(p, f, r0);
    f = (float)((float)((n.y * r1->y + r1->x * n.x) + n.z * r1->z) * ((((s2 + s2) * 4.0) * 0.5) / -1000.0));
    push(p, f, r1);
    f = (float)((float)(n.z * r2->z + (n.x * r2->x + n.y * r2->y)) * (((s2 + s2) * 0.375) / -1000.0));
    push(p, f, r2);
  }
}

/* The blades and the tail: the lift along the up axis and the drag of the particle k. */
static void heli_blades(Object *o)
{
  int k;

  for (k = 21; k < o->num_particles; k++) {
    Particle *p = &sr_particles[o->particles[k]];
    const vec3 *r0 = &o->rot[0];
    const vec3 *r1 = &o->rot[1];
    const vec3 *r2 = &o->rot[2];
    float angle = blade_angle(o, k);
    float g = (float)((x87_sin(angle) * -160.0) / 60.0);
    vec3 v, n;
    float s, s2, f;

    v.x = (float)(r1->x * g + p->vel.x);
    v.y = (float)(r1->y * g + p->vel.y);
    v.z = (float)(r1->z * g + p->vel.z);
    s = object_normalize(&n, &v);
    s2 = (float)(s * s);
    f = (float)((float)((n.y * r1->y + r1->x * n.x) + n.z * r1->z) * (((s2 * 4.0) * 0.5) * -0.03125));
    push(p, f, r1);
    p->vel_prev = p->vel;
    s = object_normalize(&n, &p->vel_prev);
    s2 = (float)(s * s);
    f = (float)((((4.0 * s2) * 0.5) * -0.00390625) * (float)((n.y * r0->y + n.x * r0->x) + n.z * r0->z));
    push(p, f, r0);
    f = (float)((float)(n.z * r2->z + (n.x * r2->x + n.y * r2->y)) * (-0.00390625 * ((s2 + s2) * 0.375)));
    push(p, f, r2);
  }
}

/* The helicopter: the rotor speed, the forces and the turn. The forces need a driver. */
void objectsimulation_heli(int obj)
{
  Object *o = &sr_objects[obj];
  const float *in = o->drive_input;
  int driver = 0;
  int h;
  float f, a0;

  for (h = 0; h < 512; h++) {
    const Human *hu = &sr_humans[h];
    if (hu->active && hu->vehicle_id == obj && hu->vehicle_seat == 0)
      driver = 1;
  }
  if (o->health < 1 || o->key_away) {
    driver = 0;
    o->rpm_net = o->rpm_net < 10 ? 0 : o->rpm_net - 10;
  } else if (driver) {
    o->rpm_net = o->rpm_net < 0x7c7 ? o->rpm_net + 10 : 2000;
  } else {
    o->rpm_net = o->rpm_net < 10 ? 0 : o->rpm_net - 10;
  }
  f = (float)o->rpm_net;
  a0 = (float)(((((f * 2.0) * PI) / 60.0) / 60.0) + o->wheels[0].angle);
  o->wheels[0].angle = a0;
  o->wheels[1].angle = (float)(a0 - ((((2.0 * (f * 0.125)) * PI) / 60.0) / 60.0));
  if (o->wheels[0].angle > PI)
    o->wheels[0].angle = (float)(o->wheels[0].angle - 6.28318530718);
  else if (o->wheels[0].angle < -PI)
    o->wheels[0].angle = (float)(o->wheels[0].angle + 6.28318530718);
  if (!driver || o->health <= 0)
    return;

  heli_drag(o);
  if (o->num_particles > 21)
    heli_blades(o);
  addobjectrotation(obj, &o->pos, &o->rot[1], (float)(in[1] * 2.7777778450399637E-4));
}
