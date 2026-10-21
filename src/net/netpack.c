/* The events and the objects that the server writes into the state packet of a connection.
 *
 * packevent writes one event. packobjectpacket picks the humans, objects, items and city
 * objects that a player sees, and writes them. Each connection has a score for every human and
 * every object: it grows each tick by how near the thing is, and a thing that is sent starts
 * again at 0. So near things go out often and far things go out rarely.
 *
 * The positions and vectors are copied as raw bytes, so no float load changes their bits.
 */
#include <math.h>
#include <string.h>

#include "packet.h"
#include "server.h"
#include "sr_math.h"
#include "util.h"
#include "vectors.h"

#define sr_cityobjects SR_AT(CityObjectRec, SR_RVA_CITYOBJECT)
#define EVENT_FILLER 0x12 /* the type of a placeholder: the client skips it */
#define EVENT_FRESH_TICKS 600
#define MAX_SENT_BEHIND 0x17 /* a client further behind than this gets no objects */
#define NUM_NET_HUMANS 128
#define NUM_NET_OBJECTS 128
#define NUM_NET_CITYOBJECTS 128
#define BEST_HUMANS 8

static void filler(void)
{
    uint32_t f = EVENT_FILLER;
    pbitwrite(&f, 6);
}

/* Write the text of the own message of a player: the text before the chat filter. Returns 1
 * when the text goes out. */
static int own_text(const Event *ev, const Connection *c)
{
    const Player *p;
    uint32_t n, i;
    if (c->player_id == -1)
        return 0;
    p = &sr_players[c->player_id];
    if (p->orig_text[0] == '\0' || p->human_id != ev->attach_id)
        return 0;
    pbitwrite((const uint32_t *)&ev->type, 6);
    pbitwrite((const uint32_t *)&ev->tick, 28);
    n = (uint32_t)strlen(p->orig_text);
    pbitwrite(&n, 6);
    pbitwrite((const uint32_t *)&ev->attach_type, 4);
    pbitwritenego((const uint32_t *)&ev->attach_id, 10);
    pbitwrite((const uint32_t *)&ev->range_mode, 4);
    for (i = 0; i < (n & 0x3f); i++) {
        uint32_t ch = (uint32_t)((signed char)p->orig_text[i]) & 0x7f;
        pbitwrite(&ch, 7);
    }
    return 1;
}

/* Is a message event too far to be heard? A message of a human or an item reaches 32 units
 * divided by the distance factor. Returns 1 when the client does not get the event. */
static int out_of_range(const Event *ev, const Connection *c)
{
    vec3 pos;
    float d, s;
    if (ev->attach_type == 1)
        pos = sr_humans[ev->attach_id].bones[2].pos;
    else
        pos = sr_items[ev->attach_id].pos;
    d = vectordistance(&pos, &c->listener_pos);
    s = (float)(32.0 / (d * 0.125));
    if (ev->range_mode == 0)
        s = (float)(s * 0.25);
    if (ev->range_mode == 2)
        s = (float)(s + s);
    if (ev->attach_type == 2)
        s = (float)(0.25 * s);
    return s < 4.0f;
}

static void put_raw(const void *p)
{
    cs_pwrite(p, 4, 1);
}

static void put_bits(const int *field, uint32_t bits)
{
    pbitwrite((const uint32_t *)field, bits);
}

/* The characters of a message, 7 bits each. */
static void put_text(const char *text, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        uint32_t ch = (uint8_t)text[i] & 0x7f;
        pbitwrite(&ch, 7);
    }
}

/* Write the fields of an event after its type and tick. */
static void write_event_body(const Event *ev)
{
    switch (ev->type) {
    case 0:
        put_bits(&ev->param, 4);
        put_bits(&ev->attach_type, 10);
        cs_pwrite(&ev->pos, 4, 3);
        cs_pwrite(&ev->vec, 4, 3);
        break;
    case 1:
        put_bits(&ev->param, 4);
        put_bits(&ev->attach_type, 6);
        cs_pwrite(&ev->pos, 4, 3);
        cs_pwrite(&ev->vec, 4, 3);
        break;
    case 2:
        put_bits(&ev->param, 6);
        put_bits(&ev->attach_type, 4);
        put_bits(&ev->attach_id, 10);
        put_bits(&ev->range_mode, 4);
        put_text(ev->message, ev->param & 0x3f);
        break;
    case 3:
        put_bits(&ev->param, 10);
        put_bits(&ev->attach_type, 8);
        put_bits(&ev->attach_id, 4);
        break;
    case 4:
        put_bits(&ev->param, 10);
        put_bits(&ev->attach_type, 4);
        put_bits(&ev->attach_id, 10);
        cs_pwrite(&ev->pos, 4, 3);
        cs_pwrite(&ev->vec, 4, 3);
        break;
    case 5:
        put_bits(&ev->param, 20);
        put_bits(&ev->attach_type, 10);
        put_bits(&ev->attach_id, 10);
        put_bits(&ev->range_mode, 4);
        break;
    case 6:
        put_bits(&ev->param, 10);
        put_bits(&ev->attach_type, 3);
        put_bits(&ev->attach_id, 16);
        put_bits(&ev->range_mode, 2);
        break;
    case 7:
        put_bits(&ev->param, 16);
        put_bits(&ev->attach_type, 10);
        put_raw(&ev->attach_id);
        put_bits(&ev->range_mode, 16);
        put_text(ev->message, 31);
        break;
    case 8:
        put_bits(&ev->param, 8);
        put_raw(&ev->attach_type);
        put_raw(&ev->attach_id);
        break;
    case 9:
        put_bits(&ev->param, 8);
        cs_pwrite(&ev->pos, 4, 3);
        put_raw(&ev->sound_a);
        put_raw(&ev->sound_b);
        break;
    case 10:
        put_bits(&ev->param, 8);
        put_bits(&ev->attach_type, 1);
        break;
    case 11:
    case 12:
        put_raw(&ev->param);
        put_raw(&ev->attach_type);
        put_raw(&ev->attach_id);
        put_raw(&ev->range_mode);
        break;
    case 13:
    case 15:
        put_raw(&ev->param);
        put_raw(&ev->attach_type);
        put_raw(&ev->attach_id);
        break;
    case 14:
        put_bits(&ev->param, 8);
        put_bits(&ev->attach_type, 8);
        put_bits(&ev->attach_id, 8);
        put_bits(&ev->range_mode, 20);
        break;
    case 16:
        put_raw(&ev->param);
        put_raw(&ev->attach_type);
        put_raw(&ev->attach_id);
        put_bits(&ev->range_mode, 10);
        cs_pwrite(&ev->pos, 4, 3);
        cs_pwrite(&ev->vec, 4, 3);
        break;
    case 17:
        put_bits(&ev->param, 10);
        put_bits(&ev->attach_type, 1);
        put_bits(&ev->attach_id, 8);
        break;
    default:
        break;
    }
}

/* Write event number index of the ring for a connection. An old message goes out as a
 * placeholder, and an admin message goes only to an admin. A message of a human or an item
 * that is too far away becomes a placeholder too. */
void packevent(int index, int conn)
{
    const Event *ev = &sr_events[index];
    const Connection *c = &sr_connections[conn];
    int type = ev->type;
    if ((uint32_t)(sr_game.ticks - ev->tick) > EVENT_FRESH_TICKS &&
        ((type == 2 && ev->attach_type != 3) || type == 9)) {
        filler();
        return;
    }
    if (type == 2 && ev->attach_type == 4 && c->is_admin == 0) {
        filler();
        return;
    }
    if (c->is_admin == 0 && type == 2 && (ev->attach_type == 1 || ev->attach_type == 2) && ev->attach_id != -1) {
        if (ev->attach_type == 1 && own_text(ev, c))
            return;
        if (out_of_range(ev, c)) {
            filler();
            return;
        }
    }
    pbitwrite((const uint32_t *)&ev->type, 6);
    pbitwrite((const uint32_t *)&ev->tick, 28);
    write_event_body(ev);
}

/* The squared length of the vector from a to b is the sum of three squares; the distance is its
 * square root. */
static float net_distance(const vec3 *ref, const vec3 *p)
{
    float dx = (float)(ref->x - p->x), dy = (float)(ref->y - p->y), dz = (float)(ref->z - p->z);
    float sq = (float)(dz * dz + (dx * dx + dy * dy));
    return (float)x87_sqrt(sq);
}

/* How much a thing at distance s adds to its score each tick: 1 far away up to 256 close. */
static int priority_step(float s)
{
    float q = (float)((float)x87_pow(s, 1.5) * 0.00390625);
    int v = (int)(long long)((1.0 - q) * 256.0);
    return v < 1 ? 1 : v;
}

/* Does an item go into the packet? An item that sits on the floor and does not move goes out
 * only when the client is within 32 units, or on every 32nd packet. Both object packets use
 * this test. */
int item_is_sent(const Item *it, int index, uint32_t sequence, const vec3 *ref)
{
    uint32_t x = sequence ^ (uint32_t)index;
    float d, speed;
    if ((x & 3) != 0) {
        d = net_distance(ref, &it->pos);
        if (32.0f <= d)
            return 0;
    }
    if ((x & 0x1f) != 0) {
        d = net_distance(ref, &it->pos);
        if (!(d < 256.0f))
            return 0;
        if (it->parent_human == -1) {
            speed = (float)x87_sqrt((float)(it->vel.z * it->vel.z + (it->vel.x * it->vel.x + it->vel.y * it->vel.y)));
            if (!(speed > 0.008333333767950535)) {
                speed = (float)x87_sqrt((float)(it->angvel.z * it->angvel.z +
                                               (it->angvel.x * it->angvel.x + it->angvel.y * it->angvel.y)));
                if (!(speed > 0.008333333767950535))
                    return 0;
            }
        }
    }
    return 1;
}

/* Sort the entries of the radix record. After the sort, radix.order[i] for i = count - 1 down
 * to 0 is the entry with the highest score first. */
static void sort_entries(void)
{
    csradixsort();
}

/* Add the scores of all humans, pick the best 8 and give each picked human a score of 0 again.
 * The own human adds 0x400, so it is always among the best. */
static void pick_humans(Connection *c, const vec3 *ref, int own)
{
    ObjectPacket *op = &sr_objectpacket;
    Radix *radix = &sr_radix;
    int h, n, i;
    for (h = 0; h < NUM_NET_HUMANS; h++) {
        const Human *hm = &sr_humans[h];
        float s;
        int v;
        if (!hm->active)
            continue;
        s = net_distance(ref, &hm->pos);
        if (hm->vehicle_id != -1)
            s = (float)(s + s);
        v = priority_step(s);
        if (h == own)
            v = 0x400;
        c->human_priority[h].score += v;
    }
    radix->count = 0;
    for (h = 0; h < NUM_NET_HUMANS; h++) {
        if (sr_humans[h].active) {
            radix->keys[radix->count] = c->human_priority[h].score;
            radix->values[radix->count] = h;
            radix->count++;
        }
    }
    sort_entries();
    n = radix->count;
    op->num_humans = 0;
    for (i = n - 1; i >= 0; i--) {
        h = radix->values[radix->order[i]];
        if (i >= n - BEST_HUMANS) {
            c->human_priority[h].score = 0;
            op->humans[op->num_humans].id = h;
            op->humans[op->num_humans].mode = 0;
            op->num_humans++;
        }
    }
}

/* The same for the objects. An object that is not in the world (state 3) is left out. The
 * 2 best get mode 0 (full), the next 6 mode 1 (a coarser turn), the next 8 mode 2 (only the
 * place). */
static void pick_objects(Connection *c, const vec3 *ref)
{
    ObjectPacket *op = &sr_objectpacket;
    Radix *radix = &sr_radix;
    int o, n, i;
    for (o = 0; o < NUM_NET_OBJECTS; o++) {
        const Object *ob = &sr_objects[o];
        float s;
        int v;
        if (!ob->active || ob->state == 3)
            continue;
        s = net_distance(ref, &ob->pos);
        v = priority_step(s);
        if (s < 4.0f)
            v = 0x400;
        if (ob->type == 12 && s < 12.0f)
            v = 0x400;
        c->obj_priority[o].score += v;
    }
    radix->count = 0;
    for (o = 0; o < NUM_NET_OBJECTS; o++) {
        if (sr_objects[o].active && sr_objects[o].state != 3) {
            radix->keys[radix->count] = c->obj_priority[o].score;
            radix->values[radix->count] = o;
            radix->count++;
        }
    }
    sort_entries();
    n = radix->count;
    op->num_objects = 0;
    for (i = n - 1; i >= 0; i--) {
        NetEntry *e = &op->objects[op->num_objects];
        o = radix->values[radix->order[i]];
        if (i >= n - 2) {
            c->obj_priority[o].score = 0;
            e->id = o;
            e->mode = 0;
            op->num_objects++;
        } else if (i >= n - 8) {
            c->obj_priority[o].score = 0;
            e->id = o;
            e->mode = 1;
            op->num_objects++;
        } else if (i >= n - 16) {
            e->id = o;
            e->mode = 2;
            op->num_objects++;
        }
    }
}

/* The items to send. The list has 256 places but the count can pass 255: the extra entries
 * go into the next fields of the record, and the count of city objects is set after them. */
static void pick_items(const Connection *c, const vec3 *ref)
{
    ObjectPacket *op = &sr_objectpacket;
    int *list = op->items;
    int i;
    op->num_items = 0;
    for (i = 0; i < SR_MAX_ITEM; i++) {
        const Item *it = &sr_items[i];
        if (!it->active || it->parent_item != -1 || (it->parent_human != -1 && it->parent_slot >= 2))
            continue;
        list[op->num_items] = i;
        if (item_is_sent(it, i, c->sequence, ref))
            op->num_items++;
    }
}

static void pick_cityobjects(void)
{
    ObjectPacket *op = &sr_objectpacket;
    int i;
    op->num_cityobjects = 0;
    for (i = 0; i < NUM_NET_CITYOBJECTS; i++) {
        if (sr_cityobjects[i].active != 0)
            op->cityobjects[op->num_cityobjects++] = i;
    }
}

/* One of the 15 bones of a human: the first bone has a position and a turn, the others only
 * the 3 angles to their parent. */
static void write_bone(const Human *hm, int b)
{
    const Bone *bone = &hm->bones[b];
    if (b == 0) {
        packposition(&bone->pos);
        packorientation(&bone->rot[1], 31);
        packorientation(&bone->rot[2], 31);
    } else {
        packangle(&bone->joint_angles.x, 16);
        packangle(&bone->joint_angles.y, 16);
        packangle(&bone->joint_angles.z, 16);
    }
}

static void write_humans(const ObjectPacket *op)
{
    int i, b;
    pbitwrite((const uint32_t *)&op->num_humans, 8);
    for (i = 0; i < op->num_humans; i++) {
        int id = op->humans[i].id, mode = op->humans[i].mode;
        const Human *hm = &sr_humans[id];
        uint32_t hp;
        pbitwrite((const uint32_t *)&id, 10);
        pbitwrite((const uint32_t *)&mode, 1);
        if (mode != 0)
            continue;
        pbitwrite((const uint32_t *)&hm->vehicle_id, 8);
        pbitwrite((const uint32_t *)&hm->vehicle_seat, 4);
        hp = (uint32_t)(hm->health + 10);
        if ((int)hp < 0)
            hp = 0;
        pbitwrite(&hp, 7);
        pbitwrite((const uint32_t *)&hm->bleeding, 1);
        for (b = 0; b < 15; b++) {
            if (hm->vehicle_id == -1 || b < 9)
                write_bone(hm, b);
        }
    }
}

static void write_objects(const ObjectPacket *op)
{
    int i, k;
    pbitwrite((const uint32_t *)&op->num_objects, 8);
    for (i = 0; i < op->num_objects; i++) {
        int id = op->objects[i].id, mode = op->objects[i].mode;
        const Object *ob = &sr_objects[id];
        pbitwrite((const uint32_t *)&id, 10);
        pbitwrite((const uint32_t *)&mode, 2);
        packposition(&ob->pos);
        if (mode != 0 && mode != 1)
            continue;
        packorientation(&ob->rot[1], mode == 0 ? 31 : 21);
        packorientation(&ob->rot[2], mode == 0 ? 31 : 21);
        for (k = 0; k < 4; k++)
            packangle(&ob->wheels[k].angle, 8);
        packangle(&ob->wheels[0].steer, 8);
        packangle(&ob->wheels[1].steer, 8);
        for (k = 0; k < 4; k++)
            packscalar(&ob->wheels[k].travel, 1.0f, 6);
        for (k = 0; k < 4; k++)
            packscalar(&ob->wheels[k].slip, 1.0f, 6);
        packscalar(&ob->vel.x, 1.0f, 8);
        packscalar(&ob->vel.y, 1.0f, 8);
        packscalar(&ob->vel.z, 1.0f, 8);
        pbitwrite((const uint32_t *)&ob->rpm_net, 13);
    }
}

static void write_items(const ObjectPacket *op)
{
    int i;
    pbitwrite((const uint32_t *)&op->num_items, 8);
    for (i = 0; i < op->num_items; i++) {
        int id = op->items[i];
        const Item *it = &sr_items[id];
        uint32_t cash = it->type == 0x10;
        pbitwrite((const uint32_t *)&id, 10);
        packposition(&it->pos);
        packorientation(&it->rot[1], 31);
        packorientation(&it->rot[2], 31);
        pbitwrite(&cash, 1);
        if (cash) {
            pbitwrite((const uint32_t *)&it->last_bill, 4);
            pbitwrite((const uint32_t *)&it->num_bills, 4);
            pbitwrite(&it->bills_mask, 30);
        }
    }
}

static void write_cityobjects(const ObjectPacket *op)
{
    int i;
    pbitwrite((const uint32_t *)&op->num_cityobjects, 8);
    for (i = 0; i < op->num_cityobjects; i++) {
        int id = op->cityobjects[i];
        const CityObjectRec *rec = &sr_cityobjects[id];
        pbitwrite((const uint32_t *)&id, 10);
        packposition(&rec->pos);
        packorientation(&rec->rot[1], 31);
        packorientation(&rec->rot[2], 31);
        pbitwrite((const uint32_t *)&rec->num_floors, 7);
        pbitwrite((const uint32_t *)&rec->floor, 7);
        packscalar(&rec->net_scalar, 1.0f, 6);
    }
}

/* Choose and write the things that the player of a connection sees. The reference point is
 * the human of the player, or the camera when the player has no living human. */
void packobjectpacket(int conn, int player)
{
    Connection *c = &sr_connections[conn];
    ObjectPacket *op = &sr_objectpacket;
    vec3 ref;
    int own = -1, behind;
    if (player == -1 || sr_players[player].human_id == -1 || sr_humans[sr_players[player].human_id].health < 1)
        ref = c->camera_pos;
    else
        ref = sr_humans[sr_players[player].human_id].pos;
    if (player != -1)
        own = sr_players[player].human_id;
    pick_humans(c, &ref, own);
    pick_objects(c, &ref);
    pick_items(c, &ref);
    pick_cityobjects();
    behind = (int)sr_numofevents - c->event_ack;
    if ((int)sr_numofevents < c->event_ack)
        behind += 0x10000;
    if (behind > MAX_SENT_BEHIND) {
        op->num_items = 0;
        op->num_objects = 0;
        op->num_humans = 0;
    }
    write_humans(op);
    write_objects(op);
    write_items(op);
    write_cityobjects(op);
    c->sequence++;
}

/* Choose the humans and objects with one list: all of them are candidates with their scores,
 * and the best 8 go out, all in full. Items and city objects are
 * chosen as in packobjectpacket. The score falls off with the distance to the power 1.75. */
void packobjectpacket2(int conn, int player)
{
    Connection *c = &sr_connections[conn];
    ObjectPacket *op = &sr_objectpacket;
    Radix *radix = &sr_radix;
    vec3 ref;
    int own = -1, own_vehicle = -1, h, o, i, n, behind;
    if (player == -1 || sr_players[player].human_id == -1 || sr_humans[sr_players[player].human_id].health < 1)
        ref = c->camera_pos;
    else
        ref = sr_humans[sr_players[player].human_id].pos;
    if (player != -1 && sr_players[player].human_id != -1) {
        own = sr_players[player].human_id;
        if (sr_humans[own].vehicle_id != -1)
            own_vehicle = sr_humans[own].vehicle_id;
    }
    op->num_candidates = 0;
    for (h = 0; h < NUM_NET_HUMANS; h++) {
        const Human *hm = &sr_humans[h];
        float s, q;
        int v;
        NetCandidate *cand;
        if (!hm->active)
            continue;
        s = net_distance(&ref, &hm->pos);
        q = (float)((float)x87_pow(s, 1.75) * 0.00390625);
        v = (int)(long long)((1.0 - q) * 256.0);
        if (v < 1)
            v = 1;
        if (h == own)
            v = 0x400;
        c->human_priority[h].score += v;
        cand = &op->candidates[op->num_candidates++];
        cand->kind = 0;
        cand->id = h;
        cand->score = c->human_priority[h].score;
    }
    for (o = 0; o < NUM_NET_OBJECTS; o++) {
        const Object *ob = &sr_objects[o];
        float s, q;
        int v;
        NetCandidate *cand;
        if (!ob->active || ob->state == 3)
            continue;
        s = net_distance(&ref, &ob->pos);
        s = (float)(s * 0.00390625);
        q = (float)((float)x87_pow(s, 1.75) * 0.00390625);
        v = (int)(long long)((1.0 - q) * 256.0);
        if (v < 1)
            v = 1;
        if (o == own_vehicle)
            v = 0x400;
        c->obj_priority[o].score += v;
        cand = &op->candidates[op->num_candidates++];
        cand->kind = 1;
        cand->id = o;
        cand->score = c->obj_priority[o].score;
    }
    radix->count = 0;
    for (i = 0; i < op->num_candidates; i++) {
        radix->keys[radix->count] = op->candidates[i].score;
        radix->values[radix->count] = i;
        radix->count++;
    }
    sort_entries();
    n = radix->count;
    op->num_humans = 0;
    op->num_objects = 0;
    for (i = n - 1; i >= 0; i--) {
        const NetCandidate *cand = &op->candidates[radix->values[radix->order[i]]];
        if (i < n - BEST_HUMANS)
            continue;
        if (cand->kind == 0) {
            c->human_priority[cand->id].score = 0;
            op->humans[op->num_humans].id = cand->id;
            op->humans[op->num_humans].mode = 0;
            op->num_humans++;
        }
        if (cand->kind == 1) {
            c->obj_priority[cand->id].score = 0;
            op->objects[op->num_objects].id = cand->id;
            op->objects[op->num_objects].mode = 0;
            op->num_objects++;
        }
    }
    pick_items(c, &ref);
    pick_cityobjects();
    behind = (int)sr_numofevents - c->event_ack;
    if ((int)sr_numofevents < c->event_ack)
        behind += 0x10000;
    if (behind > 0x1f) {
        op->num_items = 0;
        op->num_objects = 0;
        op->num_humans = 0;
    }
    write_humans(op);
    write_objects(op);
    write_items(op);
    write_cityobjects(op);
    c->sequence++;
}
