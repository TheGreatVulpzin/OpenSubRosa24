/* Line and sphere tests against planes, triangles and edges. See intersect.c. */
#ifndef SR_INTERSECT_H
#define SR_INTERSECT_H

#include "sr_types.h"

int lineintersectplane(vec3 *hit, const vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *p);
int lineintersectplanetwosided(vec3 *hit, const vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *p);
int lineintersecttriangle(vec3 *hit, const vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *a,
                          const vec3 *b, const vec3 *c);
int lineintersecttriangletwosided(vec3 *hit, const vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *a,
                                  const vec3 *b, const vec3 *c, int two_sided);
int lineintersectsphere(vec3 *hit, vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *c, float r);
void calculatebarycentric(vec3 *w, const vec3 *p, const vec3 *a, const vec3 *b, const vec3 *c);
int sphereintersectline(vec3 *hit, vec3 *n, float *t, const vec3 *c, float r, const vec3 *p0, const vec3 *p1);
int sphereintersecttriangle(vec3 *hit, const vec3 *n, float *dist, const vec3 *center, float r, const vec3 *a,
                            const vec3 *b, const vec3 *c);
int sphereintersecttrianglefull(vec3 *hit, vec3 *n, float *dist, const vec3 *center, float r, const vec3 *a,
                                const vec3 *b, const vec3 *c);

#endif /* SR_INTERSECT_H */
