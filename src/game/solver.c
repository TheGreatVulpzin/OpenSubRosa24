/* The bond solver: each pass changes the velocities of two bodies so that their bond holds.
 *
 * bondsimulation2 copies the velocities into the work arrays (BondParticle, BondHuman), and
 * prepares one BondOpt record for each bond. Each function here reads one record and adds
 * the changes to dv (and dw). The float math keeps the order of the 024d.
 */
#include "solver.h"

#include "sr_runtime.h"

/* A bond between two particles: the change acts along the bond, in the ratio of the masses. */
void bond_partpart(int i)
{
    const BondOpt *o = &sr_bondopt[i];
    BondParticle *pa = &sr_bondparticles[(uint16_t)o->a];
    BondParticle *pb = &sr_bondparticles[(uint16_t)o->b];
    float nx = o->dir.x, ny = o->dir.y, nz = o->dir.z;
    float impulse, da, db;

    /* The speed along the bond, damped, plus the stretch. */
    impulse = (float)((float)(ny * (float)(pb->vel.y - pa->vel.y) + nx * (float)(pb->vel.x - pa->vel.x) +
                              nz * (float)(pb->vel.z - pa->vel.z)) *
                      o->damping + o->bias);
    da = impulse * o->share_a;
    pa->dv.x = (float)(da * nx + pa->dv.x);
    pa->dv.y = (float)(ny * da + pa->dv.y);
    pa->dv.z = (float)(da * nz + pa->dv.z);
    db = (float)(-impulse * o->share_b);
    pb->dv.x = (float)(pb->dv.x + db * nx);
    pb->dv.y = (float)(pb->dv.y + db * ny);
    pb->dv.z = (float)(nz * db + pb->dv.z);
}

/* A joint between two bones of a human: the velocity of each point is vel + w x arm. */
void bond_bodybody(int i)
{
    const BondOpt *o = &sr_bondopt[i];
    BondHuman *h = &sr_bondhumans[(uint16_t)o->a];
    BondBone *a = &h->bones[(uint16_t)o->b];
    BondBone *b = &h->bones[(uint16_t)o->c];
    const vec3 ra = o->arm_a, rb = o->arm_b;
    float k = o->damping, nk = -o->damping;
    float ex, ey, ez, sa, sb;

    /* The difference of the two point velocities, damped, plus the position error. */
    ex = (float)(nk * (float)(b->vel.x + (float)(rb.y * b->w.z - rb.z * b->w.y)) +
                 (float)(o->dir.x + k * (float)(a->vel.x + (float)(ra.y * a->w.z - ra.z * a->w.y))));
    ey = (float)((float)(b->vel.y + (float)(b->w.x * rb.z - rb.x * b->w.z)) * nk +
                 (float)((float)(a->vel.y + (float)(ra.z * a->w.x - ra.x * a->w.z)) * k + o->dir.y));
    ez = (float)(nk * (float)(b->vel.z + (float)(b->w.y * rb.x - rb.y * b->w.x)) +
                 (float)(k * (float)(a->vel.z + (float)(ra.x * a->w.y - a->w.x * ra.y)) + o->dir.z));

    sb = o->share_b;
    sa = -o->share_a;
    a->dv.x = (float)(a->dv.x + ex * sa);
    a->dv.y = (float)(a->dv.y + ey * sa);
    a->dv.z = (float)(a->dv.z + ez * sa);
    a->dw.x = (float)(sa * (float)(ey * ra.z - ez * ra.y) + a->dw.x);
    a->dw.y = (float)(sa * (float)(ez * ra.x - ra.z * ex) + a->dw.y);
    a->dw.z = (float)(a->dw.z + (float)(ex * ra.y - ey * ra.x) * sa);
    b->dv.x = (float)(b->dv.x + sb * ex);
    b->dv.y = (float)(ey * sb + b->dv.y);
    b->dv.z = (float)(ez * sb + b->dv.z);
    b->dw.x = (float)((float)(ey * rb.z - rb.y * ez) * sb + b->dw.x);
    b->dw.y = (float)((float)(rb.x * ez - rb.z * ex) * sb + b->dw.y);
    b->dw.z = (float)(sb * (float)(rb.y * ex - ey * rb.x) + b->dw.z);
}
