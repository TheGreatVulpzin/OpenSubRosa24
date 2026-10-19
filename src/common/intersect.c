/* Line and sphere tests against planes, triangles and edges. Each test returns 1 on a hit,
 * and writes the hit point, the normal and the distance or fraction. */
#include "intersect.h"

#include <math.h>

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

/* The turn of the edge a -> b against the hit point, along n: 0 or more on the inner side. */
static float edge_side(const vec3 *hit, const vec3 *n, const vec3 *a, const vec3 *b)
{
    return (float)(((float)(((float)(hit->x - a->x)) * ((float)(b->y - a->y)) -
                                   ((float)(b->x - a->x)) * ((float)(hit->y - a->y)))) * n->z +
                          (((float)(((float)(b->z - a->z)) * (hit->y - a->y) -
                                    (b->y - a->y) * ((float)(hit->z - a->z)))) * n->x +
                           ((float)((b->x - a->x) * (hit->z - a->z) - (hit->x - a->x) * (b->z - a->z))) * n->y));
}

static int inside_edge(const vec3 *hit, const vec3 *n, const vec3 *a, const vec3 *b)
{
    return 0.0 <= edge_side(hit, n, a, b);
}

/* The line from -> to against the triangle a b c with the normal n, from the front only. */
int lineintersecttriangle(vec3 *hit, const vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *a,
                          const vec3 *b, const vec3 *c)
{
    return lineintersectplane(hit, n, t, from, to, a) != 0 && inside_edge(hit, n, a, b) && inside_edge(hit, n, b, c) &&
           inside_edge(hit, n, c, a);
}

/* As lineintersecttriangle. two_sided: the line may cross the plane from both sides. */
int lineintersecttriangletwosided(vec3 *hit, const vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *a,
                                  const vec3 *b, const vec3 *c, int two_sided)
{
    int crosses;

    if (two_sided == 0)
        crosses = lineintersectplane(hit, n, t, from, to, a);
    else
        crosses = lineintersectplanetwosided(hit, n, t, from, to, a);
    if (crosses == 0)
        return 0;
    if (edge_side(hit, n, a, b) < 0.0 || edge_side(hit, n, b, c) < 0.0)
        return 0;
    return 0.0 <= edge_side(hit, n, c, a);
}

/* The line from -> to against the sphere (center c, radius r): *t gets the fraction of the line
 * at the first hit, and n the unit vector from the center to the hit. */
int lineintersectsphere(vec3 *hit, vec3 *n, float *t, const vec3 *from, const vec3 *to, const vec3 *c, float r)
{
    vec3 d;
    float yy, xx, zz, qx, qy, qz, along, qq, miss, rr, f;
    double back, len;

    d.x = (float)(to->x - from->x);
    d.y = (float)(to->y - from->y);
    d.z = (float)(to->z - from->z);
    yy = d.y * d.y;
    xx = d.x * d.x;
    zz = d.z * d.z;
    qx = (float)(c->x - from->x);
    qy = (float)(c->y - from->y);
    qz = (float)(c->z - from->z);
    normalizevector(&d, &d);
    along = (float)(qz * d.z + (qy * d.y + qx * d.x));
    *t = along;
    if (!(0.0 <= along))
        return 0;
    qq = (float)(qy * qy + qx * qx + qz * qz);
    miss = (float)(qq - along * along);
    rr = r * r;
    if (rr < miss || qq < rr)
        return 0;
    back = x87_sqrt((double)(rr - miss));
    len = x87_sqrt((double)(float)(zz + (yy + xx)));
    f = (float)((along - (float)back) / (float)len);
    *t = f;
    if (!(f <= 1.0))
        return 0;
    d.x = (float)(to->x - from->x);
    d.y = (float)(to->y - from->y);
    d.z = (float)(to->z - from->z);
    scaleaddvectors(hit, from, &d, *t);
    n->x = (float)(hit->x - c->x);
    n->y = (float)(hit->y - c->y);
    n->z = (float)(hit->z - c->z);
    normalizevector(n, n);
    return 1;
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

/* The side of the hit point against the edge p -> q of a triangle with the normal n:
 * 0 or more on the inner side. */
static float circle_edge_side(const vec3 *hit, const vec3 *n, const vec3 *p, const vec3 *q)
{
    vec3 e, h, s;

    e.x = (float)(q->x - p->x);
    e.y = (float)(q->y - p->y);
    e.z = (float)(q->z - p->z);
    h.x = (float)(hit->x - p->x);
    h.y = (float)(hit->y - p->y);
    h.z = (float)(hit->z - p->z);
    crossproduct(&s, &e, &h);
    return (float)(n->z * s.z + (n->x * s.x + n->y * s.y));
}

/* An edge p -> q against the plane of the wheel: a nearer hit moves hit, n (from the hit to the
 * center) and *dist. */
static void circle_edge(vec3 *hit, vec3 *n, float *dist, const vec3 *pos, const vec3 *rot, const vec3 *p, const vec3 *q)
{
    vec3 e, d;
    float t, len;

    if (lineintersectplanetwosided(&e, rot, &t, p, q, pos) == 0)
        return;
    d.x = (float)(pos->x - e.x);
    d.y = (float)(pos->y - e.y);
    d.z = (float)(pos->z - e.z);
    len = (float)x87_sqrt((double)(float)(d.z * d.z + (d.x * d.x + d.y * d.y)));
    if (!(len < *dist))
        return;
    *hit = e;
    n->x = (float)(pos->x - e.x);
    n->y = (float)(pos->y - e.y);
    n->z = (float)(pos->z - e.z);
    normalizevector(n, n);
    *dist = len;
}

/* A wheel (a circle of radius r at pos, in the plane with the normal rot) against the triangle
 * a b c with the normal n. First the point of the triangle plane nearest to the center, on the
 * line where the two planes meet, then the 3 edges against the plane of the wheel. n gets the
 * vector from an edge hit to the center, and *dist the distance (r when there is no hit). */
int circleintersecttriangle(vec3 *hit, vec3 *n, float *dist, const vec3 *pos, const vec3 *rot, float r, const vec3 *a,
                            const vec3 *b, const vec3 *c)
{
    vec3 cr, q, u, p, w;
    float lsq, pd, ad, s, len;

    *dist = r;
    cr.x = (float)(n->y * rot->z - n->z * rot->y);
    cr.y = (float)(rot->x * n->z - rot->z * n->x);
    cr.z = (float)(rot->y * n->x - rot->x * n->y);
    lsq = (float)(cr.y * cr.y + cr.x * cr.x + cr.z * cr.z);
    if (1.52587890625E-5 <= lsq) {
        pd = (float)(pos->z * rot->z + (rot->y * pos->y + pos->x * rot->x));
        ad = (float)(n->z * a->z + (a->y * n->y + a->x * n->x));
        q.x = ad * rot->x;
        q.y = (float)(rot->y * ad);
        q.z = (float)(ad * rot->z);
        u.x = (float)(pd * n->x - q.x);
        u.y = (float)((float)(n->y * pd) - q.y);
        u.z = (float)((float)(pd * n->z) - q.z);
        p.x = (float)(((float)(u.z * cr.y - cr.z * u.y)) / lsq);
        p.y = (float)(((float)(u.x * cr.z - cr.x * u.z)) / lsq);
        p.z = (float)(((float)(u.y * cr.x - cr.y * u.x)) / lsq);
        w.x = (float)(pos->x - p.x);
        w.y = (float)(pos->y - p.y);
        w.z = (float)(pos->z - p.z);
        normalizevector(&cr, &cr);
        s = (float)(cr.z * w.z + (cr.y * w.y + cr.x * w.x));
        hit->x = (float)(p.x + s * cr.x);
        hit->y = (float)(p.y + s * cr.y);
        hit->z = (float)(p.z + cr.z * s);
        w.x = (float)(pos->x - hit->x);
        w.y = (float)(pos->y - hit->y);
        w.z = (float)(pos->z - hit->z);
        len = (float)x87_sqrt((double)(float)(w.z * w.z + (w.x * w.x + w.y * w.y)));
        if (len < r && 0.0 <= circle_edge_side(hit, n, a, b) && 0.0 <= circle_edge_side(hit, n, b, c) &&
            0.0 <= circle_edge_side(hit, n, c, a))
            *dist = len;
    }
    circle_edge(hit, n, dist, pos, rot, a, b);
    circle_edge(hit, n, dist, pos, rot, b, c);
    circle_edge(hit, n, dist, pos, rot, c, a);
    return r <= *dist ? 0 : 1;
}

/* The sphere (center, r) against point p. On a hit, normal gets the unit vector from p to the
 * center, hit gets the center and *dist gets the depth. It returns 0 also on a hit. */
int sphereintersectpoint(vec3 *hit, vec3 *normal, float *dist, const vec3 *center, float r, const vec3 *p)
{
    float dx = (float)(center->x - p->x), dy = (float)(center->y - p->y), dz = (float)(center->z - p->z);
    float len = (float)x87_sqrt((double)(float)(dz * dz + (dy * dy + dx * dx)));
    float inv;

    if (len < r) {
        inv = (float)(1.0 / len);
        normal->x = inv * dx;
        normal->y = (float)(dy * inv);
        normal->z = (float)(inv * dz);
        *hit = *center;
        *dist = (float)(r - len);
    }
    return 0;
}

/* The distance from point p to the segment a -> b. */
float pointdistanceline(const vec3 *a, const vec3 *b, const vec3 *p)
{
    vec3 d, q, foot;
    float f;

    d.x = (float)(b->x - a->x);
    d.y = (float)(b->y - a->y);
    d.z = (float)(b->z - a->z);
    q.x = (float)(p->x - b->x);
    q.y = (float)(p->y - b->y);
    q.z = (float)(p->z - b->z);
    f = (float)(d.y * q.y + d.x * q.x + d.z * q.z);
    if (0.0 <= f) {
        d.x = (float)(b->x - p->x);
        d.y = (float)(b->y - p->y);
        d.z = (float)(b->z - p->z);
        return (float)x87_sqrt((double)(float)(d.z * d.z + (d.x * d.x + d.y * d.y)));
    }
    q.x = (float)(p->x - a->x);
    q.y = (float)(p->y - a->y);
    q.z = (float)(p->z - a->z);
    if ((float)(d.z * q.z + (q.y * d.y + q.x * d.x)) <= 0.0)
        return vectordistance(p, a);
    normalizevector(&d, &d);
    f = -(float)(d.y * q.y + d.x * q.x + d.z * q.z);
    foot.x = (float)(p->x + f * d.x);
    foot.y = (float)(p->y + f * d.y);
    foot.z = (float)(d.z * f + p->z);
    return vectordistance(&foot, a);
}

/* The 2D line a -> b against the segment c -> d, from the front side of the segment only. n gets
 * the unit normal of the segment, *t the part of a -> b before the hit and hit the hit point. */
int lineintersectline2d(float hit[2], float n[2], float *t, const float a[2], const float b[2], const float c[2],
                        const float d[2])
{
    float ny, bx, ax, by, ay, s, y;

    n[0] = (float)(c[1] - d[1]);
    ny = (float)(d[0] - c[0]);
    n[1] = ny;
    if (!(0.0 <= (float)(ny * (float)(a[1] - c[1]) + n[0] * (float)(a[0] - c[0])) &&
          (float)(n[0] * (float)(b[0] - c[0]) + (float)(b[1] - c[1]) * ny) <= 0.0))
        return 0;
    normalizevector2d(n, n);
    *t = (float)(n[1] * (float)(c[1] - a[1]) + n[0] * (float)(c[0] - a[0]));
    bx = b[0];
    ax = a[0];
    by = b[1];
    ay = a[1];
    s = (float)(*t / (float)((float)(bx - ax) * n[0] + (float)(by - ay) * n[1]));
    *t = s;
    hit[0] = (float)(a[0] + s * (bx - ax));
    y = (float)((by - ay) * s + a[1]);
    hit[1] = y;
    return 0.0 <= (float)((float)(d[1] - c[1]) * (float)(y - c[1]) + (float)(d[0] - c[0]) * (float)(hit[0] - c[0])) &&
                   0.0 <= (float)((float)(c[1] - d[1]) * (float)(hit[1] - d[1]) +
                                  (float)(c[0] - d[0]) * (float)(hit[0] - d[0]))
               ? 1
               : 0;
}

/* The 2D circle (p, r) against the segment a -> b. n gets the unit normal of the segment, hit
 * the nearest point of the segment and *t the signed distance over r. */
int sphereintersectline2d(float hit[2], float n[2], float *t, const float p[2], const float a[2], const float b[2],
                          float r)
{
    float d;

    if (!(0.0 <= (float)((float)(b[1] - a[1]) * (float)(p[1] - a[1]) + (float)(b[0] - a[0]) * (float)(p[0] - a[0])) &&
          0.0 <= (float)((float)(a[1] - b[1]) * (float)(p[1] - b[1]) + (float)(a[0] - b[0]) * (float)(p[0] - b[0]))))
        return 0;
    n[0] = (float)(a[1] - b[1]);
    n[1] = (float)(b[0] - a[0]);
    normalizevector2d(n, n);
    d = (float)((float)(p[1] - a[1]) * n[1] + n[0] * (float)(p[0] - a[0]));
    if (!(fabs(d) <= r))
        return 0;
    hit[0] = (float)(p[0] + -d * n[0]);
    hit[1] = (float)(-d * n[1] + p[1]);
    *t = (float)(d / r);
    return 1;
}

/* The 2D segment a -> b against the circle (c, r). n gets the unit direction of the segment, hit
 * the first point inside the circle and *t its distance from a. */
int lineintersectcircle2d(float hit[2], float n[2], float *t, const float a[2], const float b[2], const float c[2],
                          float r)
{
    double len, back;
    float dy, inv, ux, uy, along, px, py, ex, ey, dd, k;

    n[0] = (float)(b[0] - a[0]);
    dy = (float)(b[1] - a[1]);
    n[1] = dy;
    len = x87_sqrt((double)(float)(n[0] * n[0] + dy * dy));
    inv = (float)(1.0 / (float)len);
    ux = inv * n[0];
    n[0] = ux;
    uy = n[1] * inv;
    n[1] = uy;
    along = (float)(ux * (float)(c[0] - a[0]) + uy * (float)(c[1] - a[1]));
    if (along < 0.0)
        return 0;
    if (!(along <= (float)len))
        return 0;
    px = (float)(a[0] + along * ux);
    py = (float)(uy * along + a[1]);
    ex = (float)(px - c[0]);
    ey = (float)(py - c[1]);
    dd = (float)x87_sqrt((double)(float)(ex * ex + ey * ey));
    if (!(dd <= r))
        return 0;
    back = x87_sqrt((double)(r * r - dd * dd));
    k = -(float)back;
    hit[0] = (float)(px + k * n[0]);
    hit[1] = (float)(k * n[1] + py);
    *t = (float)(along - (float)back);
    return 1;
}
