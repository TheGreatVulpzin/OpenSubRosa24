/* The bond solver: each pass changes the velocities of two bodies so that their bond holds.
 *
 * bondsimulation2 copies the velocities into the work arrays (BondParticle, BondHuman), and
 * prepares one BondOpt record for each bond. Each function here reads one record and adds
 * the changes to dv (and dw).
 */
#include "solver.h"
#include "bond_storage.h"

#include "sr_math.h"
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
    const vec3 ra = o->arm_a, rb = o->arm_b;
    float k = o->damping, nk = -o->damping;
    float ex, ey, ez, sa, sb;

    /* The difference of the two point velocities, damped, plus the position error. */
    ex = (float)(nk * (float)(BOND_HUMAN_VEL((uint16_t)o->a, (uint16_t)o->c).x + (float)(rb.y * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->c).z - rb.z * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->c).y)) +
                 (float)(o->dir.x + k * (float)(BOND_HUMAN_VEL((uint16_t)o->a, (uint16_t)o->b).x + (float)(ra.y * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->b).z - ra.z * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->b).y))));
    ey = (float)((float)(BOND_HUMAN_VEL((uint16_t)o->a, (uint16_t)o->c).y + (float)(BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->c).x * rb.z - rb.x * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->c).z)) * nk +
                 (float)((float)(BOND_HUMAN_VEL((uint16_t)o->a, (uint16_t)o->b).y + (float)(ra.z * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->b).x - ra.x * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->b).z)) * k + o->dir.y));
    ez = (float)(nk * (float)(BOND_HUMAN_VEL((uint16_t)o->a, (uint16_t)o->c).z + (float)(BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->c).y * rb.x - rb.y * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->c).x)) +
                 (float)(k * (float)(BOND_HUMAN_VEL((uint16_t)o->a, (uint16_t)o->b).z + (float)(ra.x * BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->b).y - BOND_HUMAN_W((uint16_t)o->a, (uint16_t)o->b).x * ra.y)) + o->dir.z));

    sb = o->share_b;
    sa = -o->share_a;
    BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->b).x = (float)(BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->b).x + ex * sa);
    BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->b).y = (float)(BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->b).y + ey * sa);
    BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->b).z = (float)(BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->b).z + ez * sa);
    BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).x = (float)(sa * (float)(ey * ra.z - ez * ra.y) + BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).x);
    BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).y = (float)(sa * (float)(ez * ra.x - ra.z * ex) + BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).y);
    BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).z = (float)(BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->b).z + (float)(ex * ra.y - ey * ra.x) * sa);
    BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->c).x = (float)(BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->c).x + sb * ex);
    BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->c).y = (float)(ey * sb + BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->c).y);
    BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->c).z = (float)(ez * sb + BOND_HUMAN_DV((uint16_t)o->a, (uint16_t)o->c).z);
    BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).x = (float)((float)(ey * rb.z - rb.y * ez) * sb + BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).x);
    BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).y = (float)((float)(rb.x * ez - rb.z * ex) * sb + BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).y);
    BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).z = (float)(sb * (float)(rb.y * ex - ey * rb.x) + BOND_HUMAN_DW((uint16_t)o->a, (uint16_t)o->c).z);
}

float calculatefriction(vec3 *v, const vec3 *n, float friction)
{
    float vx = v->x, vy = v->y, vz = v->z, d, ex, ey, ez, len, inv, limit, lost = 0.0f;
    double along;

    d = (float)(vz * n->z + (vy * n->y + vx * n->x));
    v->x = d * n->x;
    v->y = (float)(d * n->y);
    d = (float)(d * n->z);
    v->z = d;
    ex = (float)(vx - v->x);
    ey = (float)(vy - v->y);
    ez = (float)(vz - d);
    len = (float)x87_sqrt((double)(float)(ez * ez + (ex * ex + ey * ey)));
    if (len <= 1.52587890625E-5)
        return 0.0f;
    inv = (float)(1.0 / len);
    along = x87_sqrt((double)(float)(v->y * v->y + v->x * v->x + d * d));
    limit = (float)((float)along * friction);
    if (limit < len) {
        lost = (float)((float)((len - limit) * (float)along) * 60.0 * 8.0);
        len = limit;
    }
    v->x = (float)(inv * ex * len + v->x);
    v->y = (float)((float)(ey * inv) * len + v->y);
    v->z = (float)(len * (float)(inv * ez) + v->z);
    return lost;
}
