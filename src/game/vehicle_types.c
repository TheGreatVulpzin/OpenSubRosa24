/* The object types of the vehicles: 11 cars and trucks, the helicopter, the train and its bogie.
 *
 * Each setup runs once at the start of the game. A type is a list of particles, a list of bonds
 * between them, a list of faces for the hits, and a list of wheels. The particles of a car sit
 * on a grid: 3 or 2 segments of 8 corners (a box), a cabin box above the middle, and some
 * extra points. Every list only grows: a record goes at the count and the count adds 1. The
 * counts are never set to 0, so a second run of a setup appends all the records again (the
 * bonds excepted). createobject copies the lists into an object.
 *
 * The weight of a particle is a share of the mass of the type: setupobjecttypeweight divides
 * each mass by the sum.
 */
#include <string.h>

#include "object_types.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Add the bonds between every pair of the corners c[0..n-1]. */
static void box_bonds(int type, int subtype, const int *c, int n)
{
  int a, b;

  for (a = 0; a < n - 1; a++)
    for (b = a + 1; b < n; b++)
      objecttype_addbond(type, subtype, c[a], c[b]);
}

/* The 8 corners of segment s of a grid of 4 columns of 2 points (the car grid has 16 points). */
static void grid_corners16(int s, int c[8])
{
  c[0] = 2 * s;
  c[1] = 2 * s + 1;
  c[2] = 2 * s + 3;
  c[3] = 2 * s + 2;
  c[4] = 2 * s + 8;
  c[5] = 2 * s + 9;
  c[6] = 2 * s + 11;
  c[7] = 2 * s + 10;
}

/* The same for the grid of 3 columns (the van, truck and trailer grid has 12 points). */
static void grid_corners12(int s, int c[8])
{
  c[0] = 2 * s;
  c[1] = 2 * s + 1;
  c[2] = 2 * s + 3;
  c[3] = 2 * s + 2;
  c[4] = 2 * s + 6;
  c[5] = 2 * s + 7;
  c[6] = 2 * s + 9;
  c[7] = 2 * s + 8;
}

/* Which faces a segment has besides the two that every segment has: the front (c5 c4 c0 c1),
 * the side (c6 c5 c1 c2), the back (c7 c6 c2 c3) and the top (c4 c5 c6 c7). */
enum { SEG_FRONT = 1, SEG_SIDE = 2, SEG_BACK = 4, SEG_TOP = 8 };

static void segment_faces(int type, const int *c, int parts)
{
  objecttype_addface(type, 4, c[0], c[3], c[2], c[1], 1.0f, 1.0f);
  objecttype_addface(type, 4, c[4], c[7], c[3], c[0], 1.0f, 1.0f);
  if (parts & SEG_FRONT)
    objecttype_addface(type, 4, c[5], c[4], c[0], c[1], 1.0f, 1.0f);
  if (parts & SEG_SIDE)
    objecttype_addface(type, 4, c[6], c[5], c[1], c[2], 1.0f, 1.0f);
  if (parts & SEG_BACK)
    objecttype_addface(type, 4, c[7], c[6], c[2], c[3], 1.0f, 1.0f);
  if (parts & SEG_TOP)
    objecttype_addface(type, 4, c[4], c[5], c[6], c[7], 1.0f, 1.0f);
}

/* A face of the cabin. The face gets the window flag, and up to 4 links: link[k] is a particle
 * of the neighbor face on the side k and its weight. */
typedef struct {
  signed char v[4];
  float armor;
  signed char window;
  signed char nlinks;
  struct {
    signed char side;
    signed char particle;
    float weight;
  } link[4];
} CabinFace;

/* The cabin of the Town Car, Town Car 2, Beamer and helicopter. */
static const CabinFace cabin_a[] = {
  { { 16, 18, 12, 10 }, 0.0f, 1, 2, { { 1, 16, 0.5f }, { 2, 10, 0.4375f } } },
  { { 16, 18, 12, 10 }, 0.0f, 1, 2, { { 0, 18, 0.5f }, { 3, 12, 0.5625f } } },
  { { 17, 16, 10, 11 }, 6.0f, 1, 0 },
  { { 19, 17, 11, 13 }, 0.0f, 1, 2, { { 0, 17, 0.5f }, { 3, 11, 0.4375f } } },
  { { 19, 17, 11, 13 }, 0.0f, 1, 2, { { 1, 19, 0.5f }, { 2, 13, 0.5625f } } },
  { { 18, 19, 13, 12 }, 0.0f, 1, 0 },
  { { 16, 17, 19, 18 }, 1.0f, 0, 0 },
};

/* The cabin of the Metro. The roof has no armor. */
static const CabinFace cabin_b[] = {
  { { 16, 18, 12, 10 }, 0.0f, 1, 0 },
  { { 17, 16, 10, 11 }, 4.0f, 1, 0 },
  { { 19, 17, 11, 13 }, 0.0f, 1, 0 },
  { { 18, 19, 13, 12 }, 0.0f, 1, 0 },
  { { 16, 17, 19, 18 }, 0.0f, 0, 0 },
};

/* The cabin of the Limo. */
static const CabinFace cabin_c[] = {
  { { 16, 18, 12, 10 }, 0.0f, 1, 2, { { 1, 16, 0.75f }, { 2, 10, 0.625f } } },
  { { 16, 18, 12, 10 }, 6.0f, 1, 4, { { 0, 18, 0.25f }, { 1, 16, 0.25f }, { 2, 10, 0.25f }, { 3, 12, 0.375f } } },
  { { 16, 18, 12, 10 }, 0.0f, 1, 2, { { 0, 18, 0.75f }, { 3, 12, 0.75f } } },
  { { 17, 16, 10, 11 }, 6.0f, 1, 0 },
  { { 19, 17, 11, 13 }, 0.0f, 1, 2, { { 0, 17, 0.75f }, { 3, 11, 0.625f } } },
  { { 19, 17, 11, 13 }, 6.0f, 1, 4, { { 0, 17, 0.25f }, { 1, 19, 0.25f }, { 2, 13, 0.375f }, { 3, 11, 0.25f } } },
  { { 19, 17, 11, 13 }, 0.0f, 1, 2, { { 1, 19, 0.75f }, { 2, 13, 0.75f } } },
  { { 18, 19, 13, 12 }, 0.0f, 1, 0 },
  { { 16, 17, 19, 18 }, 1.0f, 0, 0 },
};

/* The cabin of the Turbo and the Turbo S. */
static const CabinFace cabin_d[] = {
  { { 16, 18, 12, 10 }, 0.0f, 1, 0 },
  { { 17, 16, 10, 11 }, 6.0f, 1, 0 },
  { { 19, 17, 11, 13 }, 0.0f, 1, 0 },
  { { 18, 19, 13, 12 }, 0.0f, 1, 0 },
  { { 16, 17, 19, 18 }, 1.0f, 0, 0 },
};

/* The cabin of the vans. The truck uses it with all the particles 4 higher. */
static const CabinFace cabin_e[] = {
  { { 12, 14, 10, 8 }, 0.0f, 1, 2, { { 1, 12, 0.7f }, { 2, 8, 0.625f } } },
  { { 12, 14, 10, 8 }, 0.0f, 1, 4, { { 0, 14, 0.3f }, { 1, 12, 0.25f }, { 2, 8, 0.225f }, { 3, 10, 0.375f } } },
  { { 12, 14, 10, 8 }, 0.0f, 1, 2, { { 0, 14, 0.75f }, { 3, 10, 0.775f } } },
  { { 13, 12, 8, 9 }, 6.0f, 1, 0 },
  { { 15, 13, 9, 11 }, 0.0f, 1, 2, { { 0, 13, 0.7f }, { 3, 9, 0.625f } } },
  { { 15, 13, 9, 11 }, 0.0f, 1, 4, { { 0, 13, 0.25f }, { 1, 15, 0.3f }, { 2, 11, 0.375f }, { 3, 9, 0.225f } } },
  { { 15, 13, 9, 11 }, 0.0f, 1, 2, { { 1, 15, 0.75f }, { 2, 11, 0.775f } } },
  { { 14, 15, 11, 10 }, 0.0f, 1, 0 },
  { { 12, 13, 15, 14 }, 1.0f, 0, 0 },
};

/* Add the faces of a cabin. Every particle number adds `shift`. */
static void cabin_faces(int type, const CabinFace *rows, int n, int shift)
{
  ObjectType *t = &sr_objecttypes[type];
  int r, k;

  for (r = 0; r < n; r++) {
    const CabinFace *row = &rows[r];
    TypeFace *f;

    objecttype_addface(type, 4, row->v[0] + shift, row->v[1] + shift, row->v[2] + shift, row->v[3] + shift, 1.0f,
                       row->armor);
    f = &t->faces[t->num_faces - 1];
    if (row->window)
      f->is_window = 1;
    for (k = 0; k < row->nlinks; k++) {
      f->links[row->link[k].side] = row->link[k].particle + shift;
      f->link_weights[row->link[k].side] = row->link[k].weight;
    }
  }
}

/* A wheel on the line between two particles: the weights of the two, and the third
 * suspension value (the first two are 0.25 and 0.5 for all). */
typedef struct {
  int a, b;
  float w0, w1;
  float s2;
} WheelRow;

static void add_wheels(int type, const WheelRow *rows, int n, float mass, float inv_inertia, float ride_height)
{
  int k;

  for (k = 0; k < n; k++) {
    float w[2];

    w[0] = rows[k].w0;
    w[1] = rows[k].w1;
    objecttype_addwheel(type, 0, rows[k].a, rows[k].b, w, 0.375f, mass, inv_inertia, ride_height, 0.25f, 0.5f,
                        rows[k].s2);
  }
}

#define INERTIA_CAR 14.222222328186035f
#define INERTIA_VAN 11.377778053283691f

/* The seats of a vehicle: rows of two places at x = -0.375 and 0.375. */
static void add_seats(int type, float wheelbase, float axle_width, float y, const float *z, int rows)
{
  ObjectType *t = &sr_objecttypes[type];
  int r;

  t->wheelbase = wheelbase;
  t->num_seats = 2 * rows;
  t->axle_width = axle_width;
  for (r = 0; r < rows; r++) {
    t->seats[2 * r].x = -0.375f;
    t->seats[2 * r].y = y;
    t->seats[2 * r].z = z[r];
    t->seats[2 * r + 1].x = 0.375f;
    t->seats[2 * r + 1].y = y;
    t->seats[2 * r + 1].z = z[r];
  }
}

/* The scalars of a type. */
static void set_header(int type, const char *name, int state, int price, float mass, float power)
{
  ObjectType *t = &sr_objecttypes[type];

  t->state = state;
  strcpy(t->name, name);
  t->price = price;
  t->mass = mass;
  t->engine_power = power;
}

/* The particles of a car with a grid of 16 points. The points of one column share the x, the
 * two rows of 8 share the height. `mod` changes some points: 1 the Turbo, 2 the Turbo S,
 * 3 the Beamer. */
static void grid16_particles(int type, float x, float y, const float *zs, int mod)
{
  int i;

  for (i = 0; i < 16; i++) {
    float px = (i & 1) ? x : -x;
    float py = (i < 8) ? -y : y;
    float pz = zs[(i >> 1) & 3];

    if ((mod == 1 || mod == 2) && (i == 8 || i == 9)) {
      py = (float)(py * 0.875);
      pz = (float)(pz * 0.9375);
    }
    if (mod == 3 && (i == 8 || i == 9))
      py = (float)(py * 0.75);
    if (mod == 1 && i > 13)
      py = (float)(py * 0.625);
    if (mod == 2 && i > 13)
      py = (float)(py * 0.75);
    objecttype_addparticle(type, px, py, pz, (i < 4) ? 2.0f : 1.0f);
  }
}

/* The roof points: two at the front and two at the back. */
static void roof_particles(int type, float x, float y, float z0, float z1, float mass)
{
  int i;

  for (i = 0; i < 4; i++)
    objecttype_addparticle(type, (i & 1) ? x : -x, y, (i < 2) ? z0 : z1, mass);
}

/* One car of the 16 point grid: the Town Car, Town Car 2, Metro, Limo, Turbo, Turbo S, Beamer. */
typedef struct {
  const char *name;
  int price;
  float mass, power;
  float x, y, zs[4];
  int mod;
  float roof[5]; /* x, y, z0, z1, mass */
  const CabinFace *cabin;
  int ncabin;
  const WheelRow *wheels;
  float wheel_mass, wheel_inertia, wheel_ride;
  float wheelbase, seat_y, seat_z[2];
  int seat_rows;
} Car16;

static const WheelRow wheels_front_light[] = { { 0, 2, 0.25f, 0.75f, 0.25f }, { 1, 3, 0.25f, 0.75f, 0.25f },
                                               { 2, 4, 0.125f, 0.875f, 0.25f }, { 3, 5, 0.125f, 0.875f, 0.25f } };
static const WheelRow wheels_beamer[] = { { 0, 2, 0.375f, 0.625f, 0.25f }, { 1, 3, 0.375f, 0.625f, 0.25f },
                                          { 2, 4, 0.125f, 0.875f, 0.25f }, { 3, 5, 0.125f, 0.875f, 0.25f } };
static const WheelRow wheels_turbo[] = { { 0, 2, 0.25f, 0.75f, 0.5f }, { 1, 3, 0.25f, 0.75f, 0.5f },
                                         { 2, 4, 0.25f, 0.75f, 0.5f }, { 3, 5, 0.25f, 0.75f, 0.5f } };

static const Car16 car16_town = {
  "Town Car", 1000, 1000.0f, 1.0f, 1.0f, 0.4375f, { -3.0625f, -1.3125f, 1.75f, 2.625f }, 0,
  { 0.875f, 1.09375f, -0.4375f, 1.3125f, 0.5f }, cabin_a, 7, wheels_front_light, 12.0f, INERTIA_CAR, -0.125f,
  3.1171875f, -0.375f, { -0.25f, 1.0f }, 2
};
static const Car16 car16_town2 = {
  "Town Car 2", 1000, 1000.0f, 1.0f, 1.0f, 0.4375f, { -3.0625f, -1.3125f, 1.75f, 2.625f }, 0,
  { 0.875f, 1.09375f, -0.4375f, 1.3125f, 0.5f }, cabin_a, 7, wheels_front_light, 24.0f, INERTIA_CAR, -0.125f,
  3.1171875f, -0.375f, { -0.25f, 1.0f }, 2
};
static const Car16 car16_metro = {
  "Metro", 100, 600.0f, 0.4375f, 1.0f, 0.4375f, { -2.40625f, -1.3125f, 1.3125f, 2.1875f }, 0,
  { 0.875f, 1.09375f, -0.4375f, 0.875f, 0.5f }, cabin_b, 5, wheels_front_light, 12.0f, INERTIA_CAR, -0.125f,
  2.5703125f, -0.375f, { 0.0f }, 1
};
static const Car16 car16_limo = {
  "Limo", 100, 1500.0f, 1.0f, 1.0f, 0.4375f, { -3.9375f, -2.1875f, 2.625f, 3.5f }, 0,
  { 0.875f, 1.09375f, -1.3125f, 2.1875f, 0.5f }, cabin_c, 9, wheels_front_light, 12.0f, INERTIA_CAR, -0.125f,
  4.6484375f, -0.375f, { -1.25f, 2.0f }, 2
};
static const Car16 car16_turbo = {
  "Turbo", 20000, 800.0f, 1.25f, 1.0f, 0.3828125f, { -2.40625f, -0.875f, 1.75f, 2.1875f }, 1,
  { 0.75f, 0.95703125f, -0.21875f, 0.875f, 0.5f }, cabin_d, 5, wheels_turbo, 12.0f, INERTIA_VAN, 0.0f,
  2.3515625f, -0.5f, { 0.125f }, 1
};
static const Car16 car16_turbo_s = {
  "Turbo S", 40000, 750.0f, 1.75f, 1.0f, 0.3828125f, { -2.40625f, -0.875f, 1.75f, 2.1875f }, 2,
  { 0.75f, 0.95703125f, -0.21875f, 0.875f, 0.375f }, cabin_d, 5, wheels_turbo, 12.0f, INERTIA_VAN, 0.0f,
  2.3515625f, -0.375f, { 0.125f }, 1
};
static const Car16 car16_beamer = {
  "Beamer", 10000, 900.0f, 1.25f, 1.0f, 0.39374998211860657f, { -2.5f, -1.375f, 1.25f, 2.0f }, 3,
  { 0.875f, 0.9843749403953552f, -0.75f, 0.75f, 0.375f }, cabin_a, 7, wheels_beamer, 12.0f, INERTIA_CAR, -0.125f,
  2.578125f, -0.5f, { -0.25f, 0.625f }, 2
};

static void setup_car16(int type, const Car16 *c)
{
  ObjectType *t = &sr_objecttypes[type];
  static const int cabin_box[8] = { 10, 11, 13, 12, 16, 17, 19, 18 };
  int corners[8];
  int s, k;

  set_header(type, c->name, 1, c->price, c->mass, c->power);
  grid16_particles(type, c->x, c->y, c->zs, c->mod);
  roof_particles(type, c->roof[0], c->roof[1], c->roof[2], c->roof[3], c->roof[4]);
  t->num_frame_particles = 16;
  if (type == 1)
    objecttype_addparticle(type, 0.0f, 0.0f, -2.1875f, 4.0f);

  for (s = 0; s < 3; s++) {
    grid_corners16(s, corners);
    box_bonds(type, 0, corners, 8);
    if (type == 1 && s == 0)
      for (k = 0; k < 8; k++)
        objecttype_addbond(type, 0, corners[k], 20);
    segment_faces(type, corners, (s == 0 ? SEG_FRONT | SEG_SIDE | SEG_TOP : s == 1 ? SEG_SIDE : SEG_SIDE | SEG_BACK | SEG_TOP));
  }
  box_bonds(type, 0, cabin_box, 8);
  cabin_faces(type, c->cabin, c->ncabin, 0);
  add_wheels(type, c->wheels, 4, c->wheel_mass, c->wheel_inertia, c->wheel_ride);
  add_seats(type, c->wheelbase, 1.0f, c->seat_y, c->seat_z, c->seat_rows);
  setupobjecttypeweight(type);
}

/* The particles of a van: 12 grid points, then the roof. */
static void setup_van(int type, const char *name, int price, float mass, float power, float x, float y,
                      const float *zs, float roof_x, float roof_y, float roof_z0, float roof_z1, float wheelbase,
                      float axle_width, const float *seat_z)
{
  ObjectType *t = &sr_objecttypes[type];
  static const int cabin_box[8] = { 8, 9, 11, 10, 12, 13, 15, 14 };
  static const WheelRow wheels_van[] = { { 0, 2, 0.25f, 0.75f, 0.5f }, { 1, 3, 0.25f, 0.75f, 0.5f },
                                         { 2, 4, 0.25f, 0.75f, 0.5f }, { 3, 5, 0.25f, 0.75f, 0.5f } };
  static const WheelRow wheels_mini[] = { { 0, 2, 0.0f, 1.0f, 0.375f }, { 1, 3, 0.0f, 1.0f, 0.375f },
                                          { 2, 4, 0.25f, 0.75f, 0.25f }, { 3, 5, 0.25f, 0.75f, 0.25f } };
  int corners[8];
  int i, s;

  set_header(type, name, 1, price, mass, power);
  for (i = 0; i < 12; i++) {
    float py = (i < 6) ? -y : y;

    if (i == 6 || i == 7)
      py = (float)(py * 0.5);
    objecttype_addparticle(type, (i & 1) ? x : -x, py, zs[(i >> 1) % 3], (i < 4) ? 2.0f : 1.0f);
  }
  roof_particles(type, roof_x, roof_y, roof_z0, roof_z1, 0.5f);
  t->num_frame_particles = 12;

  for (s = 0; s < 2; s++) {
    grid_corners12(s, corners);
    box_bonds(type, 0, corners, 8);
    segment_faces(type, corners, s == 0 ? SEG_FRONT | SEG_SIDE | SEG_TOP : SEG_SIDE | SEG_BACK);
  }
  box_bonds(type, 0, cabin_box, 8);
  cabin_faces(type, cabin_e, 9, 0);
  if (type == 7)
    add_wheels(type, wheels_van, 4, 20.0f, INERTIA_VAN, -0.125f);
  else
    add_wheels(type, wheels_mini, 4, 12.0f, INERTIA_VAN, -0.125f);
  add_seats(type, wheelbase, axle_width, (type == 7) ? -0.25f : -0.375f, seat_z, 3);
  setupobjecttypeweight(type);
}

static void setup_truck(void)
{
  ObjectType *t = &sr_objecttypes[9];
  static const float zs[3] = { -2.0f, 0.0f, 2.5f };
  static const float seat_z[1] = { -0.875f };
  static const int top_box[8] = { 6, 7, 9, 8, 12, 13, 15, 14 };
  static const int pipe_box[4] = { 4, 5, 18, 19 };
  static const int cabin_box[8] = { 12, 13, 15, 14, 16, 17, 19, 18 };
  static const WheelRow wheels[] = { { 0, 2, 0.75f, 0.25f, 0.5f }, { 1, 3, 0.75f, 0.25f, 0.5f },
                                     { 2, 4, 0.25f, 0.75f, 0.5f }, { 3, 5, 0.25f, 0.75f, 0.5f },
                                     { 2, 4, 0.5f, 0.5f, 0.5f },   { 3, 5, 0.5f, 0.5f, 0.5f } };
  int corners[8];
  int i, s, k;

  set_header(9, "Truck", 1, 1000, 9000.0f, 1.0f);
  for (i = 0; i < 12; i++)
    objecttype_addparticle(9, (i & 1) ? 1.125f : -1.125f, (i < 6) ? -0.5f : -0.125f, zs[(i >> 1) % 3],
                           (i < 4) ? 4.0f : (i < 6) ? 2.0f : 1.0f);
  for (i = 0; i < 8; i++)
    objecttype_addparticle(9, (i & 1) ? 1.125f : -1.125f, (i < 4) ? 1.25f : 1.875f, ((i & 3) < 2) ? -2.0f : 0.0f,
                           0.5f);
  t->num_frame_particles = 20;
  objecttype_addparticle(9, 0.0f, 0.125f, 1.5f, 2.0f);

  for (s = 0; s < 2; s++) {
    grid_corners12(s, corners);
    box_bonds(9, 0, corners, 8);
    if (s == 1)
      for (k = 0; k < 8; k++)
        objecttype_addbond(9, 0, corners[k], 20);
    segment_faces(9, corners, s == 0 ? SEG_FRONT | SEG_SIDE : SEG_SIDE | SEG_BACK | SEG_TOP);
  }
  box_bonds(9, 0, top_box, 8);
  objecttype_addface(9, 4, 12, 14, 8, 6, 1.0f, 1.0f);
  objecttype_addface(9, 4, 13, 12, 6, 7, 1.0f, 1.0f);
  objecttype_addface(9, 4, 15, 13, 7, 9, 1.0f, 1.0f);
  objecttype_addface(9, 4, 14, 15, 9, 8, 1.0f, 1.0f);
  box_bonds(9, 0, pipe_box, 4);
  box_bonds(9, 0, cabin_box, 8);
  cabin_faces(9, cabin_e, 9, 4);
  add_wheels(9, wheels, 6, 80.0f, INERTIA_VAN, -0.125f);
  add_seats(9, 3.375f, 1.125f, 0.125f, seat_z, 1);
  setupobjecttypeweight(9);
}

static void setup_trailer(void)
{
  ObjectType *t = &sr_objecttypes[10];
  static const float zs[3] = { -7.25f, 0.0f, 8.0f };
  static const int box_a[8] = { 12, 13, 15, 14, 2, 3, 5, 4 };
  static const int box_b[8] = { 12, 13, 15, 14, 8, 9, 11, 10 };
  static const WheelRow wheels[] = { { 12, 14, 0.75f, 0.25f, 0.5f }, { 13, 15, 0.75f, 0.25f, 0.5f },
                                     { 12, 14, 0.25f, 0.75f, 0.5f }, { 13, 15, 0.25f, 0.75f, 0.5f } };
  int corners[8];
  int i, s, k;

  /* The trailer has no engine and its state is 0. */
  t->state = 0;
  strcpy(t->name, "Trailer");
  t->price = 1000;
  t->mass = 9000.0f;
  for (i = 0; i < 12; i++)
    objecttype_addparticle(10, (i & 1) ? 1.125f : -1.125f, (i < 2) ? 0.0f : (i < 6) ? -0.125f : 3.0f,
                           zs[(i >> 1) % 3], (i < 4) ? 2.0f : 1.0f);
  for (i = 0; i < 4; i++)
    objecttype_addparticle(10, (i & 1) ? 1.125f : -1.125f, -0.5f, ((i & 3) < 2) ? 6.0f : 8.0f, 1.0f);
  t->num_frame_particles = 16;
  objecttype_addparticle(10, 0.0f, 0.125f, -7.0f, 2.0f);

  for (s = 0; s < 2; s++) {
    grid_corners12(s, corners);
    box_bonds(10, 0, corners, 8);
    if (s == 0)
      for (k = 0; k < 8; k++)
        objecttype_addbond(10, 0, corners[k], 16);
    segment_faces(10, corners, s == 0 ? SEG_FRONT | SEG_SIDE | SEG_TOP : SEG_SIDE | SEG_BACK | SEG_TOP);
  }
  box_bonds(10, 0, box_a, 8);
  box_bonds(10, 0, box_b, 8);
  add_wheels(10, wheels, 4, 80.0f, INERTIA_VAN, -0.125f);
  t->wheelbase = 11.4375f;
  t->num_seats = 0;
  t->axle_width = 1.125f;
  setupobjecttypeweight(10);
}

/* Set up the object types 0 to 10: the cars, the trucks and the trailer. */
void setupobjecttypes_cars(void)
{
  static const float seats_van[3] = { -0.5f, 0.875f, 2.25f };
  static const float seats_mini[3] = { -0.4375f, 0.765625f, 1.96875f };
  static const float zs_van[3] = { -2.75f, -1.75f, 2.5f };
  static const float zs_mini[3] = { -2.5f, -1.875f, 2.25f };

  setup_car16(0, &car16_town);
  setup_car16(1, &car16_town2);
  setup_car16(2, &car16_metro);
  setup_car16(3, &car16_limo);
  setup_car16(4, &car16_turbo);
  setup_car16(5, &car16_turbo_s);
  setup_van(7, "Van", 3000, 2100.0f, 1.0f, 1.125f, 0.625f, zs_van, 0.984375f, 1.375f, -1.25f, 2.375f, 3.4375f,
            1.125f, seats_van);
  setup_van(8, "Minivan", 100, 1200.0f, 0.5f, 1.0f, 0.44999998807907104f, zs_mini, 0.875f, 1.149999976158142f,
            -1.125f, 2.125f, 3.09375f, 1.0f, seats_mini);
  setup_car16(6, &car16_beamer);
  setup_truck();
  setup_trailer();
}

/* Set up the object type 11, the helicopter. The frame is the 16 grid points and the 4 roof
 * points. Five more points sit around the center of mass: the hub and four rotor tips. */
void setupobjecttypes_helicopters(void)
{
  ObjectType *t = &sr_objecttypes[11];
  static const float zs[4] = { -2.1875f, -1.3125f, 1.75f, 4.375f };
  static const int cabin_box[8] = { 10, 11, 13, 12, 16, 17, 19, 18 };
  float sx = 0.0f, sz = 0.0f, sm = 0.0f;
  float inv, cx, cz;
  int corners[8];
  int i, j, k, s, a, b;
  static const int cabin_corners[4] = { 10, 11, 13, 12 };

  t->mass = 1000.0f;
  t->engine_power = 1.0f;
  strcpy(t->name, "Helicopter");
  t->state = 2;
  t->price = 1000000;
  for (i = 0; i < 16; i++) {
    float x = (i & 1) ? 0.875f : -0.875f;
    float y = (i < 8) ? -0.4375f : 0.4375f;

    k = (i >> 1) & 3;
    if (k == 0 || k == 3) {
      x = (float)(x * 0.5);
      y = (float)(y * 0.5);
    }
    objecttype_addparticle(11, x, y, zs[k], (i < 4) ? 1.5f : 1.0f);
  }
  objecttype_addparticle(11, -0.765625f, 1.3125f, -0.4375f, 0.5f);
  objecttype_addparticle(11, 0.765625f, 1.3125f, -0.4375f, 0.5f);
  objecttype_addparticle(11, -0.765625f, 1.3125f, 1.3125f, 0.5f);
  objecttype_addparticle(11, 0.765625f, 1.3125f, 1.3125f, 0.5f);
  t->num_frame_particles = t->num_particles;

  /* The center of mass of the frame, in the xz plane. */
  for (k = 0; k < t->num_frame_particles; k++) {
    float m = t->particles[k].mass;

    sx = (float)(m * t->particles[k].pos.x + sx);
    sz = (float)(m * t->particles[k].pos.z + sz);
    sm = (float)(m + sm);
  }
  inv = (float)(1.0 / sm);
  cx = (float)(inv * sx);
  cz = (float)(inv * sz);
  objecttype_addparticle(11, cx, 1.75f, cz, 1.0f);
  objecttype_addparticle(11, (float)(cx - 4.0), 1.75f, cz, 1.0f);
  objecttype_addparticle(11, cx, 1.75f, (float)(cz - 4.0), 1.0f);
  objecttype_addparticle(11, (float)(cx + 4.0), 1.75f, cz, 1.0f);
  objecttype_addparticle(11, cx, 1.75f, (float)(cz + 4.0), 1.0f);

  /* The five points are tied to each other and to the cabin, with bond subtype 1 for the cabin. */
  for (a = 0; a < 5; a++)
    for (b = a + 1; b < 5; b++)
      objecttype_addbond(11, 0, 20 + a, 20 + b);
  for (i = 0; i < 4; i++)
    for (j = 0; j < 5; j++)
      objecttype_addbond(11, 1, cabin_corners[i], 20 + j);
  for (s = 0; s < 3; s++) {
    grid_corners16(s, corners);
    box_bonds(11, 1, corners, 8);
    segment_faces(11, corners, (s == 0 ? SEG_FRONT | SEG_SIDE | SEG_TOP : s == 1 ? SEG_SIDE : SEG_SIDE | SEG_BACK | SEG_TOP));
  }
  box_bonds(11, 1, cabin_box, 8);
  cabin_faces(11, cabin_a, 7, 0);
  {
    static const float seat_z[2] = { -1.0f, 0.5f };

    t->num_seats = 4;
    for (i = 0; i < 2; i++) {
      t->seats[2 * i].x = -0.375f;
      t->seats[2 * i].y = -0.625f;
      t->seats[2 * i].z = seat_z[i];
      t->seats[2 * i + 1].x = 0.375f;
      t->seats[2 * i + 1].y = -0.625f;
      t->seats[2 * i + 1].z = seat_z[i];
    }
  }
  setupobjecttypeweight(11);
}

/* A face of the train with armor 1. */
static void train_face(int a, int b, int c, int d)
{
  objecttype_addface(12, 4, a, b, c, d, 1.0f, 1.0f);
}

/* A point made of 4 particles. The point is the base particle moved by (0, dy, dz). The result
 * is the index of the point: the points go after the particles in one index space. */
static int train_quad(const int *p, int base, double dy, double dz)
{
  ObjectType *t = &sr_objecttypes[12];
  TypeQuad *q = &t->quads[t->num_quads];
  vec3 pts[4];
  vec3 at;
  int idx = t->num_quads;
  int k;

  for (k = 0; k < 4; k++)
    pts[k] = t->particles[p[k]].pos;
  at.x = t->particles[base].pos.x;
  at.y = (float)(t->particles[base].pos.y + dy);
  at.z = (float)(t->particles[base].pos.z + dz);
  objecttype_calcquadweights(q->weights, &at, pts);
  for (k = 0; k < 4; k++)
    q->particles[k] = p[k];
  t->num_quads++;
  return idx;
}

/* A window in a wall of the train. The wall is the box side (p, q, r, s) with the hole w[0..3]:
 * 8 faces around the hole and one face of armor 256 in it (the window). */
static void train_window(int p, int q, int r, int s, const int *w, int first)
{
  ObjectType *t = &sr_objecttypes[12];

  train_face(p, q, w[1], w[0]);
  train_face(p, w[0], w[1], q);
  train_face(w[1], q, r, w[2]);
  train_face(w[1], w[2], r, q);
  train_face(p, w[0], w[3], s);
  train_face(p, s, w[3], w[0]);
  train_face(w[3], w[2], r, s);
  train_face(w[3], s, r, w[2]);
  objecttype_addface(12, 4, w[0], w[3], w[2], w[1], 1.0f, 256.0f);
  t->faces[t->num_faces - 1].is_window = 1;
  if (first)
    t->door_face = t->num_faces - 1;
}

/* Set up the object types 12 (the locomotive) and 13 (a bogie). */
void setupobjectypes_train(void)
{
  ObjectType *t = &sr_objecttypes[12];
  static const float zs[3] = { -7.25f, 0.0f, 8.0f };
  static const int pa[4] = { 6, 8, 2, 0 };
  static const int pb[4] = { 11, 9, 3, 5 };
  static const int pc[4] = { 8, 10, 4, 2 };
  int qa[4], qb[4], qc[4];
  int corners[8];
  int i, s, a, b;

  t->mass = 32000.0f;
  strcpy(t->name, "Train");
  t->price = 1000;
  t->state = 0;
  for (i = 0; i < 12; i++)
    objecttype_addparticle(12, (i & 1) ? 1.125f : -1.125f, (i < 6) ? 0.0f : 3.0f, zs[(i >> 1) % 3],
                           (i < 4) ? 2.0f : 1.0f);
  t->num_frame_particles = 12;
  t->num_quads = t->num_particles;
  qa[0] = train_quad(pa, 0, 2.5, 2.0);
  qa[1] = train_quad(pa, 0, 2.5, 3.5);
  qa[2] = train_quad(pa, 0, 0.125, 3.5);
  qa[3] = train_quad(pa, 0, 0.125, 2.0);
  qb[0] = train_quad(pb, 5, 2.5, -1.0);
  qb[1] = train_quad(pb, 5, 2.5, -7.0);
  qb[2] = train_quad(pb, 5, 1.5, -7.0);
  qb[3] = train_quad(pb, 5, 1.5, -1.0);
  qc[0] = train_quad(pc, 2, 2.5, 1.0);
  qc[1] = train_quad(pc, 2, 2.5, 7.0);
  qc[2] = train_quad(pc, 2, 1.5, 7.0);
  qc[3] = train_quad(pc, 2, 1.5, 1.0);
  for (s = 0; s < 2; s++) {
    const int *c = corners;

    grid_corners12(s, corners);
    box_bonds(12, 0, corners, 8);
    train_face(c[0], c[3], c[2], c[1]);
    train_face(c[0], c[1], c[2], c[3]);
    if (s == 0) {
      train_window(c[4], c[7], c[3], c[0], qa, 1);
      train_face(c[5], c[4], c[0], c[1]);
      train_face(c[5], c[1], c[0], c[4]);
      train_face(c[6], c[5], c[1], c[2]);
      train_face(c[6], c[2], c[1], c[5]);
    } else {
      train_window(c[4], c[7], c[3], c[0], qc, 0);
      train_window(c[6], c[5], c[1], c[2], qb, 0);
      train_face(c[7], c[6], c[2], c[3]);
      train_face(c[7], c[3], c[2], c[6]);
    }
    train_face(c[4], c[5], c[6], c[7]);
    train_face(c[4], c[7], c[6], c[5]);
  }
  t->num_seats = 0;
  setupobjecttypeweight(12);

  /* The bogie. The pairs of a point with itself are bonds too. */
  t = &sr_objecttypes[13];
  t->mass = 8000.0f;
  t->state = 3;
  objecttype_addparticle(13, -1.0f, -1.0f, -1.5f, 1.0f);
  objecttype_addparticle(13, 1.0f, -1.0f, -1.5f, 1.0f);
  objecttype_addparticle(13, 1.0f, -1.0f, 1.5f, 1.0f);
  objecttype_addparticle(13, -1.0f, -1.0f, 1.5f, 1.0f);
  objecttype_addparticle(13, 0.0f, 0.0f, 0.0f, 1.0f);
  objecttype_addparticle(13, 0.0f, 2.0f, 0.0f, 1.0f);
  for (a = 0; a < 6; a++)
    for (b = a; b < 6; b++)
      objecttype_addbond(13, 0, a, b);
}
