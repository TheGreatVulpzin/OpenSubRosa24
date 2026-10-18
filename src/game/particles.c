/* Particles and bonds: the points with a mass that make up people, vehicles and objects,
 * and the links between them.
 *
 * The pools are packed: when a particle or a bond is removed, the last one moves into its
 * slot, and each list that has the old index of the moved one gets the new index.
 */
#include "particles.h"

#include <string.h>

#include "sr_runtime.h"
#include "vectors.h"

#define NO_LIMIT 0x10000 /* time_to_live of a particle or a bond that stays */
#define GRAVITY 0.0027222223579883575f
#define MAX_BONDS_OF_PARTICLE 31
#define MAX_BONDS_OF_OBJECT 127

enum { BOND_JOINT = 8, BOND_MUSCLE = 9 };

/* A bond of type 0 keeps its list of objects where the other types keep their owner. */
static int *bond_objects(Bond *b)
{
    return &b->owner;
}

/* The particle indices of a bond start at particle_a. */
static int *bond_particles(Bond *b)
{
    return &b->particle_a;
}

/* Remove each copy of value: the last entry goes into its place. */
static void remove_index(int *list, int *count, int value)
{
    int k = 0;
    while (k < *count) {
        if (list[k] == value) {
            (*count)--;
            list[k] = list[*count];
        } else {
            k++;
        }
    }
}

static void rename_index(int *list, const int *count, int from, int to)
{
    int k;
    for (k = 0; k < *count; k++)
        if (list[k] == from)
            list[k] = to;
}

int createparticle(int type, const vec3 *pos, const vec3 *vel, float mass, int object)
{
    int n = sr_numofparticles;
    Particle *p;
    if (n >= SR_MAX_PARTICLE)
        return -1;
    p = &sr_particles[n];
    memset(p, 0, sizeof *p);
    p->type = type;
    p->time_to_live = NO_LIMIT;
    p->pos = *pos;
    p->pos_prev = *pos;
    if (vel)
        p->vel = *vel;
    p->mass = mass;
    p->gravity = GRAVITY;
    p->unk_0010 = 1.0f;
    if (object != -1)
        p->objects[p->num_objects++] = object;
    sr_numofparticles = n + 1;
    return n;
}

void particlesimulation(void)
{
    Particle *p = sr_particles;
    int n = sr_numofparticles;
    int i;
    for (i = 0; i < n; i++, p++) {
        p->pos_prev = p->pos;
        p->pos.x = p->pos.x + p->vel.x;
        p->pos.y = p->pos.y + p->vel.y;
        p->pos.z = p->pos.z + p->vel.z;
        p->vel_prev = p->vel;
        p->vel.y = p->vel.y - p->gravity;
    }
}

/* A particle at the end of its life loses its bonds, and the last particle takes its slot.
 * The loop then looks at the moved particle again, so it loses one more tick, as in the
 * 024d. */
void particletimetolive(void)
{
    Particle *particles = sr_particles;
    int i = 0;
    while (i < sr_numofparticles) {
        Particle *p = &particles[i];
        Human *h;
        int k, last;
        if (p->time_to_live < NO_LIMIT)
            p->time_to_live--;
        if (p->time_to_live >= 1) {
            i++;
            continue;
        }
        for (k = p->num_bonds - 1; k >= 0; k--)
            deletebond(p->bonds[k]);
        last = --sr_numofparticles;
        if (i == last)
            return;
        *p = particles[last];
        for (k = 0; k < p->num_bonds; k++) {
            Bond *b = &sr_bonds[p->bonds[k]];
            rename_index(bond_particles(b), &b->num_particles, last, i);
        }
        for (k = 0; k < p->num_objects; k++) {
            Object *o = &sr_objects[p->objects[k]];
            rename_index(o->particles, &o->num_particles, last, i);
        }
        for (h = sr_humans; h < sr_humans + SR_MAX_HUMAN; h++)
            if (h->active != 0 && h->unk_0010 == last)
                h->unk_0010 = i;
    }
}

int createbond(int subtype, int particle_a, int particle_b, int object)
{
    int n = sr_numofbonds;
    Particle *pa, *pb;
    Bond *b;
    vec3 d;
    int k;
    if (n >= SR_MAX_BOND)
        return -1;
    if (particle_a == particle_b || particle_a < 0 || particle_a >= sr_numofparticles ||
        particle_b < 0 || particle_b >= sr_numofparticles)
        return -1;
    pa = &sr_particles[particle_a];
    pb = &sr_particles[particle_b];
    if (pa->num_bonds >= MAX_BONDS_OF_PARTICLE || pb->num_bonds >= MAX_BONDS_OF_PARTICLE)
        return -1;
    if (object != -1 && sr_objects[object].num_bonds >= MAX_BONDS_OF_OBJECT)
        return -1;
    /* No second bond between the same two particles. */
    for (k = 0; k < pa->num_bonds; k++) {
        const Bond *o = &sr_bonds[pa->bonds[k]];
        if ((o->particle_a == particle_a && o->particle_b == particle_b) ||
            (o->particle_b == particle_a && o->particle_a == particle_b))
            return -1;
    }
    for (k = 0; k < pb->num_bonds; k++) {
        const Bond *o = &sr_bonds[pb->bonds[k]];
        if ((o->particle_a == particle_a && o->particle_b == particle_b) ||
            (o->particle_b == particle_a && o->particle_a == particle_b))
            return -1;
    }
    b = &sr_bonds[n];
    memset(b, 0, sizeof *b);
    b->type = 0;
    b->subtype = subtype;
    b->time_to_live = NO_LIMIT;
    b->num_particles = 2;
    b->particle_a = particle_a;
    b->particle_b = particle_b;
    b->mode = subtype == 1 ? 0 : 3;
    d.x = (float)(pb->pos.x - pa->pos.x);
    d.y = (float)(pb->pos.y - pa->pos.y);
    d.z = (float)(pb->pos.z - pa->pos.z);
    b->rest_length = vectorlength(&d);
    b->stiffness = 0.25f;
    b->unk_70 = 0.25f;
    pa->bonds[pa->num_bonds++] = n;
    pb->bonds[pb->num_bonds++] = n;
    if (object != -1) {
        Object *o = &sr_objects[object];
        bond_objects(b)[b->num_objects++] = object;
        o->bonds[o->num_bonds++] = n;
    }
    sr_numofbonds = n + 1;
    return n;
}

/* Remove bond i from the lists of its particles and objects, and move the last bond into
 * its slot. */
static void remove_bond(int i)
{
    Bond *bonds = sr_bonds;
    Bond *b = &bonds[i];
    int k, last;
    for (k = 0; k < b->num_particles; k++) {
        int p = bond_particles(b)[k];
        if (p != -1)
            remove_index(sr_particles[p].bonds, &sr_particles[p].num_bonds, i);
    }
    for (k = 0; k < b->num_objects; k++) {
        Object *o = &sr_objects[bond_objects(b)[k]];
        remove_index(o->bonds, &o->num_bonds, i);
    }
    last = --sr_numofbonds;
    if (i == last)
        return;
    *b = bonds[last];
    for (k = 0; k < b->num_particles; k++) {
        int p = bond_particles(b)[k];
        if (p != -1)
            rename_index(sr_particles[p].bonds, &sr_particles[p].num_bonds, last, i);
    }
    for (k = 0; k < b->num_objects; k++) {
        Object *o = &sr_objects[bond_objects(b)[k]];
        rename_index(o->bonds, &o->num_bonds, last, i);
    }
    if (b->type == BOND_JOINT) {
        Human *h = &sr_humans[b->owner];
        for (k = 0; k < h->num_joints; k++)
            if (h->joints[k].bond == last)
                h->joints[k].bond = i;
    }
    if (b->type == BOND_MUSCLE) {
        Human *h = &sr_humans[b->owner];
        if (h->bones[b->target].bond == last)
            h->bones[b->target].bond = i;
        for (k = 0; k < h->num_muscles; k++)
            if (h->muscles[k].bond == last)
                h->muscles[k].bond = i;
    }
}

void deletebond(int bond)
{
    if (bond >= 0 && bond < sr_numofbonds)
        remove_bond(bond);
}

/* As with the particles, the moved bond loses one more tick. */
void bondtimetolive(void)
{
    Bond *b = sr_bonds;
    int n = sr_numofbonds;
    int i = 0;
    while (i < n) {
        if (b->time_to_live < NO_LIMIT)
            b->time_to_live--;
        if (b->time_to_live >= 1) {
            i++;
            b++;
            continue;
        }
        remove_bond(i);
        n = sr_numofbonds;
    }
}
