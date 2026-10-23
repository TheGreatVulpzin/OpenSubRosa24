/* The helpers that the type setups use: append a record to a type, find the weights of a
 * point inside a quad, and center a type on its mass.
 *
 * Every append writes at the count and adds 1. There is no bound check. A setup that runs
 * twice appends the records twice (except the bonds, which are never repeated).
 */
#include "object_types.h"

#include <math.h>

#include "sr_math.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define g_pointtemp SR_AT(vec3, 0x1c497c0) /* the points of the type that is being weighted */
#define g_xaxis (*SR_AT(vec3, 0x60cd4))    /* (1 0 0) */
#define g_zaxis (*SR_AT(vec3, 0x60cec))    /* (0 0 1) */

static __forceinline vec3 point_delta(const vec3 *point, const vec3 *base)
{
  vec3 delta;
  delta.x = (float)(point->x - base->x);
  delta.y = (float)(point->y - base->y);
  delta.z = (float)(point->z - base->z);
  return delta;
}

/* The normal points from the second edge toward the first edge. */
static __forceinline vec3 point_plane_normal(const vec3 *j, const vec3 *m)
{
  vec3 normal;
  float length, inv;
  normal.x = (float)(j->z * m->y - j->y * m->z);
  normal.y = (float)(j->x * m->z - m->x * j->z);
  normal.z = (float)(m->x * j->y - j->x * m->y);
  length = (float)x87_sqrt((float)(normal.z * normal.z
      + (normal.x * normal.x + normal.y * normal.y)));
  if (length == 0.0f) {
    normal.x = normal.y = normal.z = 0.0f;
  } else {
    inv = (float)(1.0 / length);
    normal.x = (float)(inv * normal.x);
    normal.y = (float)(normal.y * inv);
    normal.z = (float)(inv * normal.z);
  }
  return normal;
}

/* Each nonflat set of four points contributes to the point weights. */
void calculatepointscale(float *out, int n, const vec3 *points, const vec3 *light)
{
  int a, b, c, d, k, i, j, m;
  int count = 0;
  float total;
  for (a = 0; a < n; a++) {
    for (b = a + 1; b < n; b++) {
      vec3 e = point_delta(&points[a], &points[b]);
      for (c = b + 1; c < n; c++) {
        vec3 edge = point_delta(&points[c], &points[b]);
        for (d = c + 1; d < n; d++) {
          vec3 other = point_delta(&points[d], &points[b]);
          vec3 normal = point_plane_normal(&edge, &other);
          float dot = (float)(normal.z * e.z + (normal.x * e.x + e.y * normal.y));
          if (dot < 0.0f) {
            normal.x = -normal.x;
            normal.y = -normal.y;
            normal.z = -normal.z;
          }
          dot = (float)(normal.z * e.z + (normal.y * e.y + e.x * normal.x));
          if (0.01f <= fabs(dot))
            count++;
        }
      }
    }
  }
  for (k = 0; k < n; k++) {
    out[k] = 0.0f;
    for (i = 0; i < n; i++) {
      for (j = i + 1; j < n; j++) {
        for (m = j + 1; m < n; m++) {
          const vec3 *base = &points[i];
          vec3 normal, e, edge, other;
          float dot, numerator, denominator;
          if (k == i || k == j || k == m)
            continue;
          edge = point_delta(&points[j], base);
          other = point_delta(&points[m], base);
          normal = point_plane_normal(&edge, &other);
          e = point_delta(&points[k], base);
          dot = (float)(normal.z * e.z + (normal.x * e.x + e.y * normal.y));
          if (dot < 0.0f) {
            normal.x = -normal.x;
            normal.y = -normal.y;
            normal.z = -normal.z;
          }
          dot = (float)(normal.z * e.z + (normal.x * e.x + normal.y * e.y));
          if (0.01f <= fabs(dot)) {
            numerator = (float)(normal.z * (float)(light->z - base->z)
                + (normal.x * (float)(light->x - base->x)
                + normal.y * (float)(light->y - base->y)));
            denominator = (float)(e.y * normal.y + e.x * normal.x + e.z * normal.z);
            out[k] = (float)((float)(numerator / denominator) + out[k]);
          }
        }
      }
    }
  }
  total = (float)count;
  /* A flat point set divides by zero. */
  for (k = 0; k < n; k++)
    out[k] = (float)(out[k] / total);
}

/* Append a particle. The first 4 bytes of the record stay as they are. */
void objecttype_addparticle(int type, float x, float y, float z, float mass)
{
  ObjectType *t = &sr_objecttypes[type];
  TypeParticle *p = &t->particles[t->num_particles];

  p->pos.x = x;
  p->pos.y = y;
  p->pos.z = z;
  p->mass = mass;
  t->num_particles++;
}

/* Append a bond unless the pair exists in one of the two orders. The result is the byte
 * offset of the type record. A pair of one particle with itself is a normal pair. */
int objecttype_addbond(int type, int subtype, int particle_a, int particle_b)
{
  ObjectType *t = &sr_objecttypes[type];
  TypeBond *b;
  int k;

  for (k = 0; k < t->num_bonds; k++) {
    b = &t->bonds[k];
    if ((b->particle_a == particle_a && b->particle_b == particle_b) ||
        (b->particle_b == particle_a && b->particle_a == particle_b))
      return type * (int)sizeof(ObjectType);
  }
  b = &t->bonds[t->num_bonds];
  b->subtype = subtype;
  b->particle_a = particle_a;
  b->particle_b = particle_b;
  t->num_bonds++;
  return type * (int)sizeof(ObjectType);
}

/* Append a face with no links. The caller sets the links, the link weights and the window flag. */
void objecttype_addface(int type, int corners, int v0, int v1, int v2, int v3, float extra, float armor)
{
  ObjectType *t = &sr_objecttypes[type];
  TypeFace *f = &t->faces[t->num_faces];
  int k;

  f->corners = corners;
  f->verts[0] = v0;
  f->verts[1] = v1;
  f->verts[2] = v2;
  f->verts[3] = v3;
  for (k = 0; k < 4; k++)
    f->links[k] = -1;
  f->extra = extra;
  f->armor = armor;
  t->num_faces++;
}

/* Append a wheel. The wheel sits on the line between the two particles, at the weight of the
 * second one, and ride_height higher. */
void objecttype_addwheel(int type, int popped, int particle_a, int particle_b, const float *weights, float radius,
                         float mass, float inv_inertia, float ride_height, float spring, float damper,
                         float vertical)
{
  ObjectType *t = &sr_objecttypes[type];
  TypeWheel *w = &t->wheels[t->num_wheels];
  const vec3 *pa = &t->particles[particle_a].pos;
  const vec3 *pb = &t->particles[particle_b].pos;
  vec3 d;

  w->popped = popped;
  w->particle_a = particle_a;
  w->particle_b = particle_b;
  w->weight_a = weights[0];
  w->weight_b = weights[1];
  w->radius = radius;
  w->mass = mass;
  w->inv_inertia = inv_inertia;
  w->ride_height = ride_height;
  w->suspension[0] = spring;
  w->suspension[1] = damper;
  w->suspension[2] = vertical;
  d.x = (float)(pb->x - pa->x);
  d.y = (float)(pb->y - pa->y);
  d.z = (float)(pb->z - pa->z);
  w->pos.x = (float)(d.x * weights[1] + pa->x);
  w->pos.y = (float)(d.y * weights[1] + pa->y);
  w->pos.z = (float)(d.z * weights[1] + pa->z);
  w->pos.y = (float)(ride_height + w->pos.y);
  t->num_wheels++;
}

/* The weights of the point q in the quad p[0..3]. Corner i gets a sum over the edges j to k
 * that do not touch it: the distance of q to the plane of the edge, divided by the distance of
 * the corner. The loop order is fixed because the float sums depend on it. */
void objecttype_calcquadweights(float weights[4], const vec3 *q, const vec3 p[4])
{
  vec3 n, e, c, d, s;
  float len, inv, num, den;
  int i, j, k;

  weights[0] = 0.0f;
  weights[1] = 0.0f;
  weights[2] = 0.0f;
  weights[3] = 0.0f;
  generatetrinormal(&n, &p[0], &p[1], &p[2]);
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      if (j == i)
        continue;
      for (k = j + 1; k < 4; k++) {
        if (k == i)
          continue;
        e.x = (float)(p[k].x - p[j].x);
        e.y = (float)(p[k].y - p[j].y);
        e.z = (float)(p[k].z - p[j].z);
        c.x = (float)(e.y * n.z - e.z * n.y);
        c.y = (float)(n.x * e.z - e.x * n.z);
        c.z = (float)(e.x * n.y - n.x * e.y);
        len = (float)x87_sqrt((float)((c.x * c.x + c.y * c.y) + c.z * c.z));
        if (len == 0.0f) {
          c.x = 0.0f;
          c.y = 0.0f;
          c.z = 0.0f;
        } else {
          inv = (float)(1.0 / len);
          c.x = (float)(c.x * inv);
          c.y = (float)(c.y * inv);
          c.z = (float)(c.z * inv);
        }
        d.x = (float)(p[i].x - p[j].x);
        d.y = (float)(p[i].y - p[j].y);
        d.z = (float)(p[i].z - p[j].z);
        if ((float)((c.x * d.x + c.y * d.y) + c.z * d.z) < 0.0f) {
          c.x = -c.x;
          c.y = -c.y;
          c.z = -c.z;
        }
        s.x = (float)(q->x - p[j].x);
        s.y = (float)(q->y - p[j].y);
        s.z = (float)(q->z - p[j].z);
        num = (float)((c.x * s.x + c.y * s.y) + c.z * s.z);
        den = (float)((d.y * c.y + d.x * c.x) + d.z * c.z);
        weights[i] = (float)((float)(num / den) + weights[i]);
      }
    }
  }
  for (k = 0; k < 4; k++)
    weights[k] = (float)(weights[k] * 0.25);
}

/* Make the masses of a type a share of 1, center the wheels on the particles, find the radius
 * of the frame, and make the two weight tables that scale the frame points. */
void setupobjecttypeweight(int type)
{
  ObjectType *t = &sr_objecttypes[type];
  int n = t->num_particles;
  int m = t->num_frame_particles;
  float sum = 0.0f;
  vec3 s, c, d, light;
  float inv, len, f;
  int k;

  for (k = 0; k < n; k++)
    sum = (float)(t->particles[k].mass + sum);
  for (k = 0; k < n; k++)
    t->particles[k].mass = (float)(t->particles[k].mass / sum);

  s.x = s.y = s.z = 0.0f;
  for (k = 0; k < n; k++) {
    s.x = (float)(t->particles[k].pos.x + s.x);
    s.y = (float)(t->particles[k].pos.y + s.y);
    s.z = (float)(t->particles[k].pos.z + s.z);
  }
  inv = (float)(1.0 / (float)n);
  c.x = (float)(inv * s.x);
  c.y = (float)(inv * s.y);
  c.z = (float)(inv * s.z);
  for (k = 0; k < t->num_wheels; k++) {
    t->wheels[k].pos.x = (float)(t->wheels[k].pos.x - c.x);
    t->wheels[k].pos.y = (float)(t->wheels[k].pos.y - c.y);
    t->wheels[k].pos.z = (float)(t->wheels[k].pos.z - c.z);
  }

  /* The center of the frame particles. With no frame particle the center is not a number. */
  s.x = s.y = s.z = 0.0f;
  for (k = 0; k < m; k++) {
    s.x = (float)(t->particles[k].pos.x + s.x);
    s.y = (float)(t->particles[k].pos.y + s.y);
    s.z = (float)(t->particles[k].pos.z + s.z);
  }
  inv = (float)(1.0 / (float)m);
  c.x = (float)(inv * s.x);
  c.y = (float)(inv * s.y);
  c.z = (float)(inv * s.z);

  t->radius = 0.0f;
  for (k = 0; k < n; k++) {
    d.x = (float)(c.x - t->particles[k].pos.x);
    d.y = (float)(c.y - t->particles[k].pos.y);
    d.z = (float)(c.z - t->particles[k].pos.z);
    len = (float)x87_sqrt((float)((d.x * d.x + d.y * d.y) + d.z * d.z));
    if (t->radius < len)
      t->radius = len;
  }
  t->radius = (float)(t->radius * 1.125);

  for (k = 0; k < m; k++)
    g_pointtemp[k] = t->particles[k].pos;

  f = (float)(t->radius * 0.25);
  light.x = (float)(c.x + f * g_zaxis.x);
  light.y = (float)(g_zaxis.y * f + c.y);
  light.z = (float)(f * g_zaxis.z + c.z);
  calculatepointscale(t->frame_weight_z, m, g_pointtemp, &light);

  f = (float)(t->radius * 0.25);
  light.x = (float)(c.x + f * g_xaxis.x);
  light.y = (float)(g_xaxis.y * f + c.y);
  light.z = (float)(f * g_xaxis.z + c.z);
  calculatepointscale(t->frame_weight_x, m, g_pointtemp, &light);
}
