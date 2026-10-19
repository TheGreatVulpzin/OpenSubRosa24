/* Line and sphere tests against planes, triangles and edges. Each test returns 1 on a hit,
 * and writes the hit point, the normal and the distance or fraction. */
#include "intersect.h"

#include "sr_math.h"
#include "vectors.h"

/* The line from -> to against the plane of p with the normal n, from the front only: from must
 * be on the back side or on the plane, and to on the front side. *t gets the fraction. */
int lineintersectplane(vec3 *hit, const vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *p)
{
    float d, denom;

    if (!(0.0 <= (float)(((float)(p->z - to->z)) * n->z +
                         (((float)(p->y - to->y)) * n->y + n->x * ((float)(p->x - to->x))))))
        return 0;
    d = (float)(((float)(p->z - from->z)) * n->z + (((float)(p->y - from->y)) * n->y + n->x * ((float)(p->x - from->x))));
    if (0.0 < d)
        return 0;
    *t = d;
    denom = (float)(((float)(to->z - from->z)) * n->z + (((float)(to->y - from->y)) * n->y + ((float)(to->x - from->x)) * n->x));
    if (denom == 0.0)
        return 0;
    denom = (float)(*t / denom);
    *t = denom;
    hit->x = (float)(from->x + denom * (to->x - from->x));
    hit->y = (float)(from->y + denom * (to->y - from->y));
    hit->z = (float)((to->z - from->z) * denom + from->z);
    return 1;
}

/* As lineintersectplane, from both sides: the ends must be on different sides of the plane. */
int lineintersectplanetwosided(vec3 *hit, const vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *p)
{
    float dto, dfrom, denom;

    dto = (float)(((float)(p->z - to->z)) * n->z + (((float)(p->y - to->y)) * n->y + n->x * ((float)(p->x - to->x))));
    dfrom = (float)(((float)(p->z - from->z)) * n->z + (((float)(p->y - from->y)) * n->y + n->x * ((float)(p->x - from->x))));
    if (0.0 < dfrom && 0.0 < dto)
        return 0;
    if (dfrom < 0.0 && dto < 0.0)
        return 0;
    *t = dfrom;
    denom = (float)(((float)(to->z - from->z)) * n->z + (((float)(to->y - from->y)) * n->y + ((float)(to->x - from->x)) * n->x));
    if (denom == 0.0)
        return 0;
    denom = (float)(*t / denom);
    *t = denom;
    hit->x = (float)(from->x + denom * (to->x - from->x));
    hit->y = (float)(from->y + denom * (to->y - from->y));
    hit->z = (float)((to->z - from->z) * denom + from->z);
    return 1;
}

/* 0 <= the turn of the edge a -> b against the hit point, along n: the hit is on the inner side. */
static int inside_edge(const vec3 *hit, const vec3 *n, const vec3 *a, const vec3 *b)
{
    return 0.0 <= (float)(((float)(((float)(hit->x - a->x)) * ((float)(b->y - a->y)) -
                                   ((float)(b->x - a->x)) * ((float)(hit->y - a->y)))) * n->z +
                          (((float)(((float)(b->z - a->z)) * (hit->y - a->y) -
                                    (b->y - a->y) * ((float)(hit->z - a->z)))) * n->x +
                           ((float)((b->x - a->x) * (hit->z - a->z) - (hit->x - a->x) * (b->z - a->z))) * n->y));
}

/* The line from -> to against the triangle a b c with the normal n, from the front only. */
int lineintersecttriangle(vec3 *hit, const vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *a,
                          const vec3 *b, const vec3 *c)
{
    return lineintersectplane(hit, n, t, from, to, a) != 0 && inside_edge(hit, n, a, b) && inside_edge(hit, n, b, c) &&
           inside_edge(hit, n, c, a);
}

/* The weights of p on the triangle a b c: for each corner, the distance of p from the opposite
 * edge, against the distance of the corner. */
void calculatebarycentric(vec3 *w, const vec3 *p, const vec3 *a, const vec3 *b, const vec3 *c)
{
    vec3 nrm, e;

    generatetrinormal(&nrm, a, b, c);
    e.x = (float)(((float)(c->y - b->y)) * nrm.z - ((float)(c->z - b->z)) * nrm.y);
    e.y = (float)(nrm.x * (c->z - b->z) - ((float)(c->x - b->x)) * nrm.z);
    e.z = (float)(nrm.y * (c->x - b->x) - (c->y - b->y) * nrm.x);
    normalizevector(&e, &e);
    w->x = (float)(((float)(e.z * ((float)(p->z - b->z)) + (e.y * ((float)(p->y - b->y)) + e.x * ((float)(p->x - b->x))))) /
                   ((float)(((float)(a->x - b->x)) * e.x + ((float)(a->y - b->y)) * e.y + ((float)(a->z - b->z)) * e.z)));
    e.x = (float)(((float)(a->y - c->y)) * nrm.z - ((float)(a->z - c->z)) * nrm.y);
    e.y = (float)(nrm.x * (a->z - c->z) - ((float)(a->x - c->x)) * nrm.z);
    e.z = (float)(nrm.y * (a->x - c->x) - (a->y - c->y) * nrm.x);
    normalizevector(&e, &e);
    w->y = (float)(((float)(e.z * ((float)(p->z - c->z)) + (e.y * ((float)(p->y - c->y)) + e.x * ((float)(p->x - c->x))))) /
                   ((float)(((float)(b->x - c->x)) * e.x + ((float)(b->y - c->y)) * e.y + ((float)(b->z - c->z)) * e.z)));
    e.x = (float)(((float)(b->y - a->y)) * nrm.z - ((float)(b->z - a->z)) * nrm.y);
    e.y = (float)(nrm.x * (b->z - a->z) - ((float)(b->x - a->x)) * nrm.z);
    e.z = (float)(nrm.y * (b->x - a->x) - (b->y - a->y) * nrm.x);
    normalizevector(&e, &e);
    w->z = (float)(((float)(e.z * ((float)(p->z - a->z)) + (e.y * ((float)(p->y - a->y)) + e.x * ((float)(p->x - a->x))))) /
                   ((float)(((float)(c->x - a->x)) * e.x + ((float)(c->y - a->y)) * e.y + ((float)(c->z - a->z)) * e.z)));
}

/* The sphere (center c, radius r) against the edge p0 -> p1: the nearest point of the edge must
 * lie between the ends and closer than r. n gets the unit vector from the hit to the center,
 * and *t the distance. */
int sphereintersectline(vec3 *hit, vec3 *n, float *t, const vec3 *c, float r, const vec3 *p0, const vec3 *p1)
{
    vec3 d, q;
    float yy, xx, zz, along, len;

    d.x = (float)(p1->x - p0->x);
    d.y = (float)(p1->y - p0->y);
    d.z = (float)(p1->z - p0->z);
    yy = d.y * d.y;
    xx = d.x * d.x;
    zz = d.z * d.z;
    q.x = (float)(c->x - p0->x);
    q.y = (float)(c->y - p0->y);
    q.z = (float)(c->z - p0->z);
    normalizevector(&d, &d);
    along = (float)(q.z * d.z + (q.y * d.y + q.x * d.x));
    if (!(0.0 < along) || !(along < (float)x87_sqrt((double)(float)(zz + (yy + xx)))))
        return 0;
    hit->x = (float)(p0->x + along * d.x);
    hit->y = (float)(along * d.y + p0->y);
    q.z = (float)(along * d.z + p0->z);
    hit->z = q.z;
    q.x = (float)(c->x - hit->x);
    q.y = (float)(c->y - hit->y);
    q.z = (float)(c->z - q.z);
    len = (float)x87_sqrt((double)(float)(q.y * q.y + q.x * q.x + q.z * q.z));
    if (!(len < r))
        return 0;
    normalizevector(n, &q);
    *t = len;
    return 1;
}

/* The sphere (center c, radius r) against the face of the triangle a b c with the normal n:
 * the center must be in front of the plane, closer than r. *dist gets the distance to the hit. */
int sphereintersecttriangle(vec3 *hit, const vec3 *n, float *dist, const vec3 *center, float r, const vec3 *a,
                            const vec3 *b, const vec3 *c)
{
    vec3 to;
    float h, t;

    h = (float)(((float)(center->z - a->z)) * n->z + (((float)(center->y - a->y)) * n->y + ((float)(center->x - a->x)) * n->x));
    if (!(h <= r) || !(0.0 <= h))
        return 0;
    t = -r;
    to.x = (float)(t * n->x + center->x);
    to.y = (float)(t * n->y + center->y);
    to.z = (float)(t * n->z + center->z);
    if (lineintersecttriangle(hit, n, &t, center, &to, a, b, c) == 0)
        return 0;
    *dist = vectordistance(center, hit);
    return 1;
}

/* As sphereintersecttriangle, then against the 3 edges: the nearest hit wins. n gets the normal
 * of the face or the vector from the edge to the center. */
int sphereintersecttrianglefull(vec3 *hit, vec3 *n, float *dist, const vec3 *center, float r, const vec3 *a,
                                const vec3 *b, const vec3 *c)
{
    vec3 to, h, m;
    float d, t;

    *dist = 65536.0f;
    d = (float)(n->z * ((float)(center->z - a->z)) + (((float)(center->x - a->x)) * n->x + n->y * ((float)(center->y - a->y))));
    if (r < d)
        return 0;
    if (d < 0.0)
        return 0;
    t = -r;
    to.x = (float)(t * n->x + center->x);
    to.y = (float)(n->y * t + center->y);
    to.z = (float)(t * n->z + center->z);
    m = *n;
    if (lineintersecttriangle(&h, &m, &t, center, &to, a, b, c) != 0) {
        *hit = h;
        *n = m;
        t = vectordistance(center, &h);
        *dist = t;
        if (t != 65536.0)
            return 65536.0 <= *dist ? 0 : 1;
    }
    if (sphereintersectline(&h, &m, &t, center, r, a, b) != 0 && t < *dist) {
        *hit = h;
        *n = m;
        *dist = t;
    }
    if (sphereintersectline(&h, &m, &t, center, r, b, c) != 0 && t < *dist) {
        *hit = h;
        *n = m;
        *dist = t;
    }
    if (sphereintersectline(&h, &m, &t, center, r, c, a) != 0 && t < *dist) {
        *hit = h;
        *n = m;
        *dist = t;
    }
    return 65536.0 <= *dist ? 0 : 1;
}
