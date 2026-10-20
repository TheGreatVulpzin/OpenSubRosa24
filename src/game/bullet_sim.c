/* bulletsimulation: the flight of the bullets and what they hit.
 *
 * A bullet loses speed to the air, then moves. The server tests the move against the humans, the
 * objects and the level. The nearest hit of a human or an object wins, and the level wins when
 * it is nearer than that hit. The first hit ends the bullet (ttl 0).
 */
#include "bullets.h"

#include <math.h>
#include <string.h>

#include "collide.h"
#include "events.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game globals. */
#define g_intersect (*SR_AT(Intersect, SR_RVA_INTERSECT))
#define g_object SR_AT(Object, SR_RVA_OBJECT)

/* Game functions with no clean version yet: the calls go to the lifted C. */
void __cdecl lf_16a20(int player, int tax);
void __cdecl lf_22e20(int object, int damage);
int __cdecl lf_2c020(uint32_t area, uint32_t x, uint32_t y, uint32_t z, uint32_t word, int spec_face, vec3 *pos,
                     vec3 *vel);
#define playerteamkilltax lf_16a20
#define applydamageobject lf_22e20
#define area_breakblock lf_2c020

#define GAMETYPE_TEAMS 3
#define GAMETYPE_BASES 4
#define TEAM_NEUTRAL 7
#define BONE_HEAD 2
#define BONE_LEFT_HAND 5
#define BONE_RIGHT_HAND 8

/* The result of the tests of one bullet. */
typedef struct Hit {
    vec3 pos, normal;
    int human;  /* -1: no human */
    int object; /* -1: no object */
    int part;   /* the bone of a human, or the wheel of an object */
    int face;   /* the face of an object. -1: a wheel */
    int particle[3];
    float weight[3];
} Hit;

/* The speed of a bullet. */
static float bullet_speed(const Bullet *b)
{
    return (float)sqrt((float)(b->vel.z * b->vel.z + (b->vel.x * b->vel.x + b->vel.y * b->vel.y)));
}

/* The damage of a bullet to an object: speed times mass, with a factor for the target. */
static int bullet_damage(const Bullet *b, double factor)
{
    int dmg = (int)(bullet_speed(b) * factor * b->mass * 60.0 - 2.0);

    return dmg < 1 ? 0 : dmg;
}

/* The damage that a bullet does to a human. The head takes more. */
static void bullet_human_hit(Bullet *b, Hit *hit)
{
    Human *hu = &sr_humans[hit->human];
    Bone *bone = &hu->bones[hit->part];
    int dmg = (int)(bullet_speed(b) * 5.5 * b->mass * 60.0 - 2.0);
    double bm;
    float k;
    vec3 r, q;

    if (dmg < 1)
        dmg = 0;
    else
        createevent_bullethit(0, 1, &hit->pos, &hit->normal);
    if (hit->part == BONE_HEAD) {
        if (dmg < 25)
            dmg = dmg > 9 ? dmg * 3 : dmg * 2;
        else
            dmg = dmg * 4;
    }
    /* A team kill costs a tax. Game type 4 taxes a hit on a protected human of team 7. */
    if (sr_game.gametype == GAMETYPE_BASES) {
        if (b->owner != -1 && hu->player_id != -1 && b->owner != hu->player_id &&
            sr_players[hu->player_id].team == TEAM_NEUTRAL && hu->protect_timer > 0)
            playerteamkilltax(b->owner, dmg);
    } else if (sr_game.gametype == GAMETYPE_TEAMS) {
        if (b->owner != -1 && hu->player_id != -1 && b->owner != hu->player_id &&
            sr_players[b->owner].team == sr_players[hu->player_id].team)
            playerteamkilltax(b->owner, dmg);
    }
    hu->health -= dmg;
    if (dmg > 0 && (os_rand() & 0x1f) < dmg)
        hu->bleeding = 1;
    hu->stun_timer = dmg * 3;
    if (b->type == 3)
        hu->stun_timer = dmg * 6;
    /* A bone takes the share of the momentum that its mass gives. The sum stays in a register. */
    bm = bone->mass * 0.125;
    k = (float)(b->mass / (bm + b->mass));
    if (hu->health < 1 && hu->health + dmg > 0) {
        k = (float)(b->mass / (bm * 0.25 + b->mass));
        hu->bones[1].vel.y = (float)(((double)(os_rand() & 7) + 4.0) / 60.0 + hu->bones[1].vel.y);
    }
    bone->vel.x = (float)(k * b->vel.x + bone->vel.x);
    bone->vel.y = (float)(k * b->vel.y + bone->vel.y);
    bone->vel.z = (float)(k * b->vel.z + bone->vel.z);
    r.x = (float)(hit->pos.x - bone->pos.x);
    r.y = (float)(hit->pos.y - bone->pos.y);
    r.z = (float)(hit->pos.z - bone->pos.z);
    q.x = (float)(r.z * b->vel.y - r.y * b->vel.z);
    q.y = (float)(r.x * b->vel.z - b->vel.x * r.z);
    q.z = (float)(r.y * b->vel.x - b->vel.y * r.x);
    b->ttl = 0;
    bone->ang_momentum.x = (float)(q.x * k + bone->ang_momentum.x);
    bone->ang_momentum.y = (float)(q.y * k + bone->ang_momentum.y);
    bone->ang_momentum.z = (float)(k * q.z + bone->ang_momentum.z);
}

/* The particles of a hit face get the momentum of the bullet along the normal. */
static void bullet_push_face(const Bullet *b, const Hit *hit)
{
    float d = (float)((hit->normal.y * b->vel.y + hit->normal.x * b->vel.x) + hit->normal.z * b->vel.z);
    vec3 j;
    int k;

    j.x = (float)(d * hit->normal.x);
    j.y = (float)(d * hit->normal.y);
    j.z = (float)(hit->normal.z * d);
    for (k = 0; k < 3; k++) {
        Particle *p = &sr_particles[hit->particle[k]];
        float g = (float)((b->mass * hit->weight[k]) / p->mass);

        p->vel.x = (float)(p->vel.x + j.x * g);
        p->vel.y = (float)(j.y * g + p->vel.y);
        p->vel.z = (float)(p->vel.z + j.z * g);
    }
}

/* The damage that a bullet does to an object: a wheel loses air, a window breaks, any other
 * face damages the object. */
static void bullet_object_hit(Bullet *b, Hit *hit)
{
    Object *obj = &g_object[hit->object];

    if (hit->face == -1) {
        Wheel *w = &obj->wheels[hit->part];

        if (w->health > 0) {
            w->health -= bullet_damage(b, 1.5);
            if (w->health < 1) {
                w->radius = 0.25f;
                w->popped = 1;
                w->deflated = 1;
                createevent_updateobject(hit->object, 1, hit->part, &hit->pos, &b->vel);
            }
        }
    } else {
        ObjectFace *f = &obj->faces[hit->face];
        int dmg = bullet_damage(b, 1.5);

        if (f->is_window == 1) {
            if (f->armor < 8.0f)
                f->health = (int)((double)f->health - (8.0 - f->armor));
            if (f->health < 1) {
                createevent_updateobject(hit->object, 0, hit->face, &hit->pos, &b->vel);
                f->health = 0;
            }
        } else {
            createevent_bullethit(0, 2, &hit->pos, &hit->normal);
            applydamageobject(hit->object, dmg);
        }
        bullet_push_face(b, hit);
    }
    b->ttl = 0;
}

/* The nearest hit of the move of a bullet on a human or on an object. A bone of the hand of the
 * owner is not a target, and a human of a player with "immune" is not a target. Returns the
 * fraction of the move. */
static float bullet_find_hit(Bullet *b, Hit *hit)
{
    float best = 1.0f;
    int h, o;

    for (h = 0; h < 0x200; h++) {
        int pid = sr_humans[h].player_id;

        if (sr_humans[h].active != 0 && lineintersecthuman(h, &b->pos_prev.x, &b->pos.x, 0.0f) &&
            g_intersect.fraction < best &&
            (pid != b->owner ||
             (g_intersect.tri_particles[0] != BONE_LEFT_HAND && g_intersect.tri_particles[0] != BONE_RIGHT_HAND)) &&
            (pid == -1 || sr_players[pid].godmode == 0)) {
            memcpy(&hit->pos, g_intersect.pos, sizeof hit->pos);
            memcpy(&hit->normal, g_intersect.normal, sizeof hit->normal);
            hit->part = g_intersect.tri_particles[0];
            best = g_intersect.fraction;
            hit->human = h;
        }
    }
    for (o = 0; o < 0x400; o++) {
        if (g_object[o].active != 0 && lineintersectobject(o, &b->pos_prev.x, &b->pos.x, 1) &&
            g_intersect.fraction < best) {
            memcpy(&hit->pos, g_intersect.pos, sizeof hit->pos);
            memcpy(&hit->normal, g_intersect.normal, sizeof hit->normal);
            hit->face = g_intersect.face;
            hit->human = -1;
            memcpy(hit->particle, g_intersect.tri_particles, sizeof hit->particle);
            hit->part = g_intersect.tri_particles[0];
            memcpy(hit->weight, g_intersect.tri_weights, sizeof hit->weight);
            best = g_intersect.fraction;
            hit->object = o;
        }
    }
    return best;
}

void bulletsimulation(void)
{
    int i;

    for (i = 0; i < sr_numofbullets; i++) {
        Bullet *b = &sr_bullets[i];
        float s, f, best = 1.0f;
        vec3 n;
        Hit hit;

        b->pos_prev = b->pos;
        b->vel.y = (float)(b->vel.y - b->gravity);
        s = bullet_speed(b);
        f = (float)(-s * s * g_bullettypes[b->type].drag);
        normalizevector(&n, &b->vel);
        hit.human = -1;
        hit.object = -1;
        b->vel.x = (float)(b->vel.x + f * n.x);
        b->vel.y = (float)(n.y * f + b->vel.y);
        b->vel.z = (float)(f * n.z + b->vel.z);
        b->pos.x = (float)(b->pos.x + b->vel.x);
        b->pos.y = (float)(b->pos.y + b->vel.y);
        b->pos.z = (float)(b->pos.z + b->vel.z);
        if (sr_server)
            best = bullet_find_hit(b, &hit);
        if (lineintersectlevel(&b->pos_prev.x, &b->pos.x, 1) && g_intersect.fraction < best) {
            hit.human = -1;
            hit.object = -1;
            memcpy(&hit.pos, g_intersect.pos, sizeof hit.pos);
            memcpy(&hit.normal, g_intersect.normal, sizeof hit.normal);
            if (sr_server && area_breakblock(g_intersect.block_area, g_intersect.block[0], g_intersect.block[1],
                                             g_intersect.block[2], g_intersect.block_word, g_intersect.spec_face,
                                             &hit.pos, &b->vel) == 0)
                createevent_bullethit(0, 0, &hit.pos, &hit.normal);
            b->pos = hit.pos;
            b->ttl = 0;
        }
        if (sr_server) {
            if (hit.human != -1)
                bullet_human_hit(b, &hit);
            if (hit.object != -1)
                bullet_object_hit(b, &hit);
        }
    }
}
