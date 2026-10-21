/* The vehicles: the engine and the helicopter.
 *
 * objectenginesimulation2 runs once for each solver pass, 32 times for each tick. It drives
 * the wheels of cars (spin) and the wheel bodies of type 1 cars (angular velocity).
 * objectsimulation_heli turns the rotor, pushes the particles with the air drag and the lift,
 * and turns the body with the drive input. objectsimulation_car reads the pedals and sets
 * the gear, the brake and the steering. object_rotorsimulation is a spring model of the rotor.
 *
 * Float rules: a value that the code stores in a float rounds to a float. A value inside one
 * expression stays a double. The sums keep their order, so the results are bit exact.
 */
#include <math.h>
#include <stdint.h>

#include "events.h"
#include "objects.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

#define PI 3.14159265359

/* Game globals. */
#define sr_soundlist (*SR_AT(SoundList, 0x916d700))

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

/* The ratio of the gears -1 to 4. */
static float gear_ratio_of(int gear)
{
  switch (gear) {
  case -1:
    return -10.5f;
  case 1:
    return 10.84999942779541f;
  case 2:
    return 7.700000286102295f;
  case 3:
    return 5.25f;
  case 4:
    return 3.5f;
  default:
    return 0.0f;
  }
}

/* The gear from the pedals (drive_input[2]) and the shifter (drive_input[0]). */
static void car_pick_gear(Object *o)
{
  const float *in = o->drive_input;

  o->clutch = 1.0f;
  o->gear_auto = 0;
  if (o->drive_buttons & 2)
    o->clutch = 0.0f;
  if (-0.5 <= in[2]) {
    if (in[2] <= 0.5) {
      o->gear = 0;
      o->clutch = 0.0f;
    } else if (0.5 <= in[0]) {
      o->gear = 1.5 <= in[0] ? -1 : 4;
    } else {
      o->gear = 2;
    }
  } else if (0.5 <= in[0]) {
    if (1.5 <= in[0])
      o->gear_auto = 1;
    else
      o->gear = 3;
  } else {
    o->gear = 1;
  }
}

/* The angles of the front wheels from the steering input. The outer wheel turns less
 * (Ackermann steering). */
static void car_steer(Object *o)
{
  float x = (float)(o->drive_input[1] * 0.75);
  float a = fabsf(x);
  float t;

  if (!(a > 0.0))
    t = 0.0f;
  else
    t = (float)(1.570796326795 -
                x87_atan2((float)(x87_tan(1.570796326795 - a) * o->wheelbase + o->axle_width), o->wheelbase));
  if (!(x > 0.0)) {
    o->wheels[0].steer = -t;
    t = -a;
  } else {
    o->wheels[0].steer = a;
  }
  o->wheels[1].steer = t;
}

/* Type 1 cars: the weights of the two steered wheel bodies. */
static void car_steer_bodies(Object *o)
{
  int k;

  for (k = 0; k < 2; k++) {
    WheelBody *wb = &o->wheel_bodies[k];
    float g = k == 0 ? -o->wheels[0].steer : o->wheels[1].steer;
    float cx = (float)((float)(x87_cos(g) * 0.25) + 0.0);
    float cy = (float)((float)(x87_sin(g) * 0.25) + 0.4375);
    double h = (2.0 - cx) * 0.5;
    double p = cy / 1.75;
    double q = (1.75 - cy) / 1.75;
    double c = cx * 0.5;

    wb->steer_weights[0] = (float)(p * h);
    wb->steer_weights[1] = (float)(h * q);
    wb->steer_weights[2] = (float)(p * c);
    wb->steer_weights[3] = (float)(q * c);
    wb->steer_side = k == 0 ? 0.125f : -0.125f;
  }
}

/* The car: the pedals, the gear, the brake and the steering of the driver. */
void objectsimulation_car(int obj)
{
  Object *o = &sr_objects[obj];
  float *in = o->drive_input;
  uint32_t buttons = o->drive_buttons;
  int auto_gear;
  int h;

  if (in[3] <= 0.0)
    o->throttle = o->throttle <= 0.125 ? 0.0f : (float)(o->throttle - 0.125);
  else
    o->throttle = fabsf(in[3]);
  car_pick_gear(o);
  auto_gear = o->gear_auto;
  if (auto_gear) {
    if (o->gear < 1)
      o->gear = 1;
    if (o->gear > 1 && o->engine_rpm < 4.363323129986111)
      o->gear--;
    if (o->gear < 4 && 8.726646259972222 < o->engine_rpm)
      o->gear++;
  }
  if (o->gear < -1)
    o->gear = -1;
  if (o->gear > 4)
    o->gear = 4;
  o->brake = 0.0f;
  if (buttons == 0xffffffffu) {
    o->gear = 0;
    o->brake = 1.0f;
    o->engine_rpm = (float)(o->engine_rpm * 0.9375);
  }
  if (!auto_gear && o->gear != o->gear_prev)
    createevent_sound(sr_soundlist.gear, &o->pos, o->gear == 0 ? 0.75f : 1.0f, o->gear == 0 ? 1.0f : 0.75f);
  o->gear_ratio = gear_ratio_of(o->gear);
  if (in[3] < 0.0)
    o->brake = -in[3];
  if (o->drive_buttons & 4)
    o->brake = 2.0f;
  o->gear_ratio_inv = o->gear_ratio == 0.0 ? 0.0f : (float)(1.0 / o->gear_ratio);
  car_steer(o);
  if (o->type == 1)
    car_steer_bodies(o);
  if (o->health < 1 || o->key_away)
    o->engine_rpm = 0.0f;
  o->rpm_net = (int)(((o->engine_rpm * 60.0) * 60.0) / 6.28318530718);
  if (o->rpm_net < 100)
    o->rpm_net = 100;
  if (o->rpm_net > 0x1fff)
    o->rpm_net = 0x1fff;
  for (h = 0; h < 512; h++) {
    const Human *hu = &sr_humans[h];
    if (hu->active && hu->vehicle_id == obj && hu->vehicle_seat == 0)
      break;
  }
  if (h == 512)
    o->rpm_net = 0;
  o->gear_prev = o->gear;
  if (!(in[3] <= 0.0))
    in[3] = 0.0f;
}

/* Move the six points of the rotor with their velocity and the gravity. */
static void rotor_move(Object *o)
{
  int k;

  for (k = 0; k < 6; k++) {
    RotorPoint *r = &o->rotor[k];

    r->pos.x = (float)(r->vel.x * 0.03125 + r->pos.x);
    r->pos.y = (float)(r->vel.y * 0.03125 + r->pos.y);
    r->pos.z = (float)(r->vel.z * 0.03125 + r->pos.z);
    r->vel.y = (float)(r->vel.y - 8.506944868713617E-5);
    r->vel_prev = r->vel;
  }
}

/* Subtract two points: out = a - b, each component rounded to a float. */
static void sub3(vec3 *out, const vec3 *a, const vec3 *b)
{
  out->x = (float)(a->x - b->x);
  out->y = (float)(a->y - b->y);
  out->z = (float)(a->z - b->z);
}

/* Add scale * dir to the velocity of a point. */
static void add_force(vec3 *vel, float scale, const vec3 *dir)
{
  vel->x = (float)(scale * dir->x + vel->x);
  vel->y = (float)(scale * dir->y + vel->y);
  vel->z = (float)(scale * dir->z + vel->z);
}

/* The lift of one blade: 8 strips along the blade, each with two surfaces. */
static void rotor_blade(Object *o, int blade)
{
  const float *in = o->drive_input;
  RotorPoint *hub = &o->rotor[4];
  const RotorPoint *tail = &o->rotor[5];
  RotorPoint *bl = &o->rotor[blade];
  vec3 d, e, n, r, q, u;
  float c0, c2, ang;
  double sa, ca;
  int j, i;

  sub3(&d, &bl->pos, &hub->pos);
  sub3(&e, &hub->pos, &tail->pos);
  n.x = (float)(d.y * e.z - d.z * e.y);
  n.y = (float)(e.x * d.z - d.x * e.z);
  n.z = (float)(e.y * d.x - d.y * e.x);
  object_normalize(&n, &n);
  bl->vel.x = (float)(bl->vel.x + n.x * 0.0011111111380159855);
  bl->vel.y = (float)(bl->vel.y + n.y * 0.0011111111380159855);
  bl->vel.z = (float)(bl->vel.z + n.z * 0.0011111111380159855);
  sub3(&r, &bl->pos, &hub->pos);
  object_normalize(&r, &r);
  c2 = (float)(r.z * o->rot[2].z + (r.x * o->rot[2].x + r.y * o->rot[2].y));
  c0 = (float)((r.x * o->rot[0].x + r.y * o->rot[0].y) + r.z * o->rot[0].z);
  ang = (float)x87_atan2(-c0, c2);
  sub3(&e, &hub->pos, &tail->pos);
  q.x = (float)(e.y * r.z - e.z * r.y);
  q.y = (float)(r.x * e.z - e.x * r.z);
  q.z = (float)(r.y * e.x - e.y * r.x);
  object_normalize(&q, &q);
  u.x = (float)(r.y * q.z - r.z * q.y);
  u.y = (float)(q.x * r.z - r.x * q.z);
  u.z = (float)(q.y * r.x - r.y * q.x);
  object_normalize(&u, &u);
  /* The sine and the cosine stay doubles. */
  sa = x87_sin(ang);
  ca = x87_cos(ang);
  for (j = 0; j < 8; j++) {
    float f = (float)((float)j * 0.125 + 0.0625);
    double g = 1.0 - f;
    vec3 m, mh;
    float s2;

    m.x = (float)(hub->vel_prev.x * g + (float)(f * bl->vel_prev.x));
    m.y = (float)(hub->vel_prev.y * g + (float)(f * bl->vel_prev.y));
    m.z = (float)(hub->vel_prev.z * g + (float)(f * bl->vel_prev.z));
    s2 = object_normalize(&mh, &m);
    s2 = (float)(s2 * s2);
    for (i = 0; i < 2; i++) {
      vec3 a = u;
      float w, L, F, G;
      double k, X;

      if (i == 0)
        rotatevector(&a, &r, 1.5707963705062866f);
      w = (float)(x87_atan2(in[2] * ca, 1.0) * 0.25 +
                  (float)((float)((in[3] * 0.0625) * 3.14159265359 + 0.04908738521234375) -
                          x87_atan2(in[0] * sa, 1.0) * 0.25));
      rotatevector(&a, &r, w);
      k = i == 0 ? 0.015625 : 0.15625;
      X = (-k * (float)((mh.x * a.x + mh.y * a.y) + a.z * mh.z)) * s2;
      L = (float)(X + X);
      F = (float)(((L * f) * 0.03125) * 0.03125);
      add_force(&bl->vel, F, &a);
      G = (float)(((L * g) * 0.015625) * 0.03125);
      add_force(&hub->vel, G, &a);
    }
  }
}

/* The hub and the tail pull on the particles 20 and 21 of the object. */
static void rotor_mount(Object *o)
{
  int k;

  for (k = 4; k < 6; k++) {
    int idx = o->particles[20 + k - 4];
    const Particle *q = &sr_particles[idx];
    BondParticle *bq = &sr_bondparticles[idx];
    RotorPoint *r = &o->rotor[k];
    vec3 d;
    float inv, a, bm;

    d.x = (float)((float)((float)((float)(r->pos.x - q->pos.x) * 0.03125) - bq->vel.x * 0.125) + r->vel_prev.x * 0.125);
    d.y = (float)((float)((float)((float)(r->pos.y - q->pos.y) * 0.03125) - bq->vel.y * 0.125) + r->vel_prev.y * 0.125);
    d.z = (float)((float)((float)((float)(r->pos.z - q->pos.z) * 0.03125) - bq->vel.z * 0.125) + r->vel_prev.z * 0.125);
    inv = (float)(1.0 / (q->mass + 64.0));
    a = (float)(inv * 64.0);
    bm = (float)(inv * q->mass);
    bq->dv.x = (float)(d.x * a + bq->dv.x);
    bq->dv.y = (float)(d.y * a + bq->dv.y);
    bq->dv.z = (float)(d.z * a + bq->dv.z);
    add_force(&r->vel, -bm, &d);
  }
}

/* The rest length of the spring between the points a and b of the rotor. */
static double rotor_rest(int a, int b)
{
  if (a < 2) {
    if (b < 2)
      return 8.0;
    if (b < 4)
      return x87_sqrt(17.0);
    return b == 4 ? 4.0 : x87_sqrt(17.0);
  }
  if (a < 4) {
    if (b < 4)
      return 2.0;
    return b == 4 ? 1.0 : x87_sqrt(2.0);
  }
  return 1.0;
}

/* The springs between the six points keep the shape of the rotor. */
static void rotor_springs(Object *o)
{
  int a, b;

  for (a = 0; a < 6; a++) {
    for (b = a + 1; b < 6; b++) {
      RotorPoint *pa = &o->rotor[a];
      RotorPoint *pb = &o->rotor[b];
      vec3 d, dir;
      float s, inv, ext, F, ma, mb, iv, fa, fb;

      sub3(&d, &pb->pos, &pa->pos);
      s = (float)x87_sqrt((float)((d.y * d.y + d.x * d.x) + d.z * d.z));
      inv = (float)(1.0 / s);
      dir.x = (float)(inv * d.x);
      dir.y = (float)(inv * d.y);
      dir.z = (float)(inv * d.z);
      ext = (float)(s - rotor_rest(a, b));
      F = (float)((float)((float)(ext * 0.125) -
                          (float)((pa->vel_prev.z * dir.z) + (pa->vel_prev.x * dir.x + dir.y * pa->vel_prev.y)) * 0.125) +
                  0.125 * (float)((pb->vel_prev.x * dir.x + pb->vel_prev.y * dir.y) + pb->vel_prev.z * dir.z));
      ma = a < 4 ? 32.0f : 64.0f;
      mb = b < 4 ? 32.0f : 64.0f;
      iv = (float)(1.0 / (ma + mb));
      fa = (float)(F * (float)(mb * iv));
      add_force(&pa->vel, fa, &dir);
      fb = (float)(-F * (float)(ma * iv));
      add_force(&pb->vel, fb, &dir);
    }
  }
}

/* The rotor of the helicopter as six points with springs. Nothing calls it. */
void object_rotorsimulation(int obj)
{
  Object *o = &sr_objects[obj];

  rotor_move(o);
  rotor_blade(o, 0);
  rotor_blade(o, 1);
  rotor_mount(o);
  rotor_springs(o);
}
