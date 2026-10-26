/* Threat memory and the controls of bot players. */
#include "logic.h"

#include <math.h>
#include <string.h>

#include "ai_car.h"
#include "collide.h"
#include "random.h"
#include "sr_math.h"
#include "sr_os.h"
#include "vectors.h"

#define g_yaxis sr_data_yaxis
#define g_intersect sr_data_intersect

static float threat_distance(const vec3 *a, const vec3 *b)
{
    vec3 d;
    float sum;
    d.x = (float)((double)a->x - b->x);
    d.y = (float)((double)a->y - b->y);
    d.z = (float)((double)a->z - b->z);
    sum = (float)((double)d.z * d.z + ((double)d.x * d.x + (double)d.y * d.y));
    return (float)x87_sqrt(sum);
}

void ai_assessthreats(int player, int human)
{
    Player *p;
    Human *me;
    int k, j;
    float best;
    if (player == -1 || human == -1)
        return;
    p = &sr_players[player];
    me = &sr_humans[human];
    for (k = 0; k < 512; ++k) {
        Human *enemy = &sr_humans[k];
        int index = -1, bone;
        vec3 *target;
        vec3 d;
        float len, sum, dot;
        if (!enemy->active || enemy->health <= 0 || enemy->player_id == -1 ||
            sr_players[enemy->player_id].team == p->team ||
            !(vectordistance(&enemy->bones[2].pos, &me->bones[2].pos) < 128.0f))
            continue;
        for (j = 0; j < p->num_threats; ++j)
            if (p->threats[j].human == k)
                index = j;
        bone = sr_game.ticks & 15;
        if (bone > 14)
            bone = 2;
        target = &enemy->bones[bone].pos;
        d.x = (float)((double)target->x - me->bones[2].pos.x);
        d.y = (float)((double)target->y - me->bones[2].pos.y);
        d.z = (float)((double)target->z - me->bones[2].pos.z);
        sum = (float)((double)d.z * d.z + ((double)d.x * d.x + (double)d.y * d.y));
        len = (float)x87_sqrt(sum);
        dot = (float)(((double)me->bones[2].rot[2].y * d.y +
                       (double)me->bones[2].rot[2].x * d.x) +
                      (double)me->bones[2].rot[2].z * d.z);
        if (dot < 0 && !lineintersectlevel(&me->bones[2].pos.x, &target->x, 1)) {
            Threat *threat;
            float range;
            vec3 v;
            if (index == -1) {
                if (p->num_threats >= 16)
                    continue;
                index = p->num_threats++;
                threat = &p->threats[index];
                threat->human = k;
                threat->pos = *target;
                threat->level = 0;
                threat->timer = 600;
            }
            threat = &p->threats[index];
            threat->level = (float)((double)threat->level + 1.0);
            if (threat->level > 16.0f)
                threat->level = 16.0f;
            scalevector(&threat->pos, &threat->pos, 0.875f);
            range = (float)((16.0 - threat->level) * 0.125 * 0.25 + (double)len * 0.25 * 0.125);
            getrandomvector3d(&v.x, range, 0);
            v.x = (float)((double)v.x + target->x);
            v.y = (float)((double)v.y + target->y);
            v.z = (float)((double)v.z + target->z);
            scaleaddvectors(&v, &v, &enemy->bones[0].vel, 4.0f);
            scaleaddvectors(&threat->pos, &threat->pos, &v, 0.125f);
        }
    }
    j = 0;
    while (j < p->num_threats) {
        float level = (float)((double)p->threats[j].level * 0.96875);
        p->threats[j].level = level;
        if (level < 0.875f) {
            --p->num_threats;
            memcpy(&p->threats[j], &p->threats[p->num_threats], sizeof(Threat));
        } else {
            ++j;
        }
    }
    /* Entry zero remains the default when the threat list is empty. */
    p->ai_target = 0;
    best = threat_distance(&me->pos, &sr_humans[p->threats[0].human].pos);
    for (j = 1; j < p->num_threats; ++j)
        if (p->threats[j].level > 8.0f) {
            float distance = threat_distance(&me->pos, &sr_humans[p->threats[j].human].pos);
            if (distance < best) {
                p->ai_target = j;
                best = distance;
            }
        }
}

static int friend_in_line(int player, int human, vec3 *start, vec3 *end, float range)
{
    int n;
    for (n = 0; n < 512; ++n) {
        Human *other = &sr_humans[n];
        if (n != human && other->active && other->player_id != -1 &&
            sr_players[other->player_id].team == sr_players[player].team &&
            vectordistance(&other->bones[2].pos, &sr_humans[human].bones[2].pos) < range &&
            lineintersecthuman(n, &start->x, &end->x, 0.25f))
            return 1;
    }
    return 0;
}

static void reload_bot(Player *p, Human *h)
{
    int gun;
    Item *g;
    if (h->inventory[0].count <= 0)
        return;
    gun = h->inventory[0].items[0];
    g = &sr_items[gun];
    if (g->ammo != 0)
        return;
    if (g->num_children == 0) {
        if (h->inventory[1].count < 1) {
            if ((os_rand() & 1) == 0) {
                int slot;
                for (slot = 3; slot < 7; ++slot)
                    if (h->inventory[slot].count > 0 &&
                        sr_itemtypes[sr_items[h->inventory[slot].items[0]].type].fits_in[g->type])
                        p->input_buttons |= (0x80u << (slot - 3)) | 0x10u;
            }
        } else if ((os_rand() & 31) == 0) {
            p->input_buttons |= sr_items[h->inventory[1].items[0]].ammo < 1 ? 0x30u : 0x1010u;
        }
    } else if ((os_rand() & 31) == 0) {
        p->input_buttons |= sr_items[g->children[0]].ammo > 0 ? 0x1000u : 0x1010u;
    }
}

void playerai(int player)
{
    Player *p = &sr_players[player];
    Human *h;
    int human = p->human_id, j;
    float aim, pitch_aim = 0;
    for (j = 0; j < 4; ++j)
        p->input_axes[j] = 0;
    p->input_buttons = 0;
    if (p->ai_wait > 0) {
        --p->ai_wait;
        return;
    }
    if (human == -1)
        return;
    h = &sr_humans[human];
    reload_bot(p, h);
    if (h->vehicle_id == -1) {
        vec3 rot[3];
        ai_assessthreats(player, human);
        p->input_mode = 1;
        resetorientation(rot);
        rotateorientation(rot, &g_yaxis, h->axis);
        aim = p->yaw;
        if (h->bones[0].rot[1].y < 0.8999999761581421f) {
            p->input_buttons |= 8;
            return;
        }
        if (p->ai_waypoint < p->num_path) {
            vec3 d;
            const vec3 *point = &p->waypoints[p->ai_waypoint].pos;
            float distance;
            p->input_axes[3] = 1;
            d.x = (float)((double)point->x - h->pos.x);
            d.y = 0;
            d.z = (float)((double)point->z - h->pos.z);
            distance = vectorlength(&d);
            normalizevector(&d, &d);
            aim = (float)x87_atan2(d.x, -d.z);
            if (distance < 4.0f)
                ++p->ai_waypoint;
        }
        if (p->num_threats > 0) {
            Threat *target = &p->threats[p->ai_target];
            if (target->level < 2.0f && (os_rand() & 31) == 0) {
                p->num_path = 1;
                p->ai_waypoint = 0;
                p->waypoints[0].pos = target->pos;
            }
        }
        if (p->num_threats > 0 && p->threats[p->ai_target].level > 12.0f) {
            vec3 d;
            float distance;
            subtractvectors(&d, &p->threats[p->ai_target].pos, &h->bones[6].pos);
            d.y = (float)((double)d.y - 0.5);
            distance = vectorlength(&d);
            aim = (float)x87_atan2(d.x, -d.z);
            pitch_aim = (float)x87_atan2(-d.y, x87_sqrt((double)d.x * d.x + (double)d.z * d.z));
            if (distance < 32.0f)
                p->input_axes[3] = 0;
            if (distance < 64.0f && fabs((double)angulardistance(aim, p->yaw)) < 0.125 &&
                fabs((double)angulardistance(pitch_aim, p->input_pitch)) < 0.125) {
                vec3 end;
                scaleaddvectors(&end, &h->bones[8].pos, &h->bones[8].rot[1], -64.0f);
                if (!friend_in_line(player, human, &h->bones[8].pos, &end, 128.0f) && (os_rand() & 3) == 0)
                    p->input_buttons |= 1;
            }
        }
        p->yaw = (float)((double)angulardistance(aim, p->yaw) * 0.125 * 0.5 + p->yaw);
        p->input_pitch = (float)((double)angulardistance(pitch_aim, p->input_pitch) * 0.125 * 0.5 + p->input_pitch);
    } else {
        int car = h->vehicle_id;
        Object *object = &sr_objects[car];
        p->input_mode = 2;
        if (object->health < 1)
            ++p->ai_wreck_ticks;
        else
            p->ai_wreck_ticks = 0;
        if (p->ai_wreck_ticks > 0xb3)
            p->input_buttons |= 0x800;
        if (h->vehicle_seat == 0) {
            ai_playercar(player, car);
            return;
        }
        ai_assessthreats(player, human);
        aim = 0;
        if (h->vehicle_seat == 1)
            aim = 0.7853981852531433f;
        else if (h->vehicle_seat == 2)
            aim = -2.356194496154785f;
        else if (h->vehicle_seat == 3)
            aim = 2.356194496154785f;
        if (p->num_threats > 0 && p->threats[p->ai_target].level > 8.0f) {
            vec3 d, normal;
            float distance;
            int gun, mag;
            subtractvectors(&d, &p->threats[p->ai_target].pos, &h->bones[6].pos);
            d.y = (float)((double)d.y - 0.25);
            distance = vectorlength(&d);
            normalizevector(&normal, &d);
            d.x = dotproduct(&normal, &object->rot[0]);
            d.y = dotproduct(&normal, &object->rot[1]);
            d.z = dotproduct(&normal, &object->rot[2]);
            aim = (float)x87_atan2(d.x, -d.z);
            pitch_aim = (float)x87_atan2(-d.y, x87_sqrt((double)d.x * d.x + (double)d.z * d.z));
            gun = h->inventory[0].items[0];
            mag = sr_items[gun].children[0];
            if (distance < 64.0f && fabs((double)angulardistance(aim, p->yaw)) < 0.125 &&
                fabs((double)angulardistance(pitch_aim, p->input_pitch)) < 0.125) {
                vec3 end;
                scaleaddvectors(&end, &sr_items[mag].pos, &sr_items[mag].rot[2], -64.0f);
                if (!friend_in_line(player, human, &sr_items[mag].pos, &end, 64.0f) &&
                    (!lineintersectobject(car, &sr_items[mag].pos.x, &end.x, 0) ||
                     object->faces[g_intersect.face].is_window == 1))
                    p->input_buttons |= 1;
            }
        }
        p->yaw = (float)((double)p->yaw + ((double)aim - p->yaw) * 0.125 * 0.5);
        p->input_pitch = (float)(((double)pitch_aim - p->input_pitch) * 0.125 * 0.5 + p->input_pitch);
    }
}
