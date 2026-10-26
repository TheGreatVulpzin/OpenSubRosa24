/* The receive side of the client: the same wire format as the server writes (netpack.c and
 * server.c), read back into the game records. The dedicated server never calls these
 * functions. A read that fails leaves its target as it was.
 *
 * The positions and vectors are copied as raw bytes, so no float load changes their bits.
 */
#include <string.h>

#include "events.h"
#include "packet.h"
#include "server.h"
#include "sr_math.h"
#include "vectors.h"

#define sr_body sr_data_body
#define sr_cityobjects sr_data_cityobject
#define sr_areasector sr_data_area_sector
#define sr_debug sr_data_debug
#define sr_voice_channels sr_data_voice
#define sr_view sr_data_view /* the player whose state the packet carries */
#define NUM_CHANNELS 8
#define TIMEOUT_TICKS 0x4af
#define MAX_RING_FRAME 64

/* Read bits as a number where all bits set mean -1. */
static void read_or_none(int *dst, uint32_t *tmp, uint32_t bits)
{
    pbitread(tmp, bits);
    *dst = *tmp == (1u << (bits & 31)) - 1 ? -1 : (int)*tmp;
}

/* Read the 4-byte fields of an event one by one: each read moves to the next byte. */
static void read_raw(int *field)
{
    cs_pread(field, 4, 1);
}

static void read_vec(vec3 *v)
{
    cs_pread(v, 4, 3);
}

/* Read the fields of an event after its type and tick. tmp and tmp2 keep their value when a
 * read fails: tmp starts as the event number. */
static void read_event_body(Event *ev, uint32_t *tmp, uint32_t *tmp2)
{
    int i;
    switch (ev->type) {
    case 0:
        pbitread((uint32_t *)&ev->param, 4);
        pbitread((uint32_t *)&ev->attach_type, 10);
        read_vec(&ev->pos);
        read_vec(&ev->vec);
        break;
    case 1:
        pbitread((uint32_t *)&ev->param, 4);
        pbitread((uint32_t *)&ev->attach_type, 6);
        read_vec(&ev->pos);
        read_vec(&ev->vec);
        break;
    case 2:
        pbitread((uint32_t *)&ev->param, 6);
        pbitread((uint32_t *)&ev->attach_type, 4);
        pbitreadnego((uint32_t *)&ev->attach_id, 10);
        pbitread((uint32_t *)&ev->range_mode, 4);
        for (i = 0; i < ev->param; i++) {
            pbitread(tmp, 7);
            ev->message[i] = (char)*tmp;
        }
        ev->message[ev->param] = '\0';
        break;
    case 3:
        pbitread((uint32_t *)&ev->param, 10);
        pbitread((uint32_t *)&ev->attach_type, 8);
        read_or_none(&ev->attach_id, tmp2, 4);
        break;
    case 4:
        pbitread((uint32_t *)&ev->param, 10);
        pbitread((uint32_t *)&ev->attach_type, 4);
        pbitread((uint32_t *)&ev->attach_id, 10);
        read_vec(&ev->pos);
        read_vec(&ev->vec);
        break;
    case 5:
        pbitread((uint32_t *)&ev->param, 20);
        read_or_none(&ev->attach_type, tmp2, 10);
        read_or_none(&ev->attach_id, tmp2, 10);
        pbitread((uint32_t *)&ev->range_mode, 4);
        break;
    case 6:
        pbitread((uint32_t *)&ev->param, 10);
        pbitread((uint32_t *)&ev->attach_type, 3);
        pbitread((uint32_t *)&ev->attach_id, 16);
        pbitread((uint32_t *)&ev->range_mode, 2);
        break;
    case 7:
        pbitread((uint32_t *)&ev->param, 16);
        read_or_none(&ev->attach_type, tmp2, 10);
        read_raw(&ev->attach_id);
        pbitread((uint32_t *)&ev->range_mode, 16);
        for (i = 0; i < 31; i++) {
            pbitread(tmp, 7);
            ev->message[i] = (char)*tmp;
        }
        ev->message[31] = '\0';
        break;
    case 8:
        pbitread((uint32_t *)&ev->param, 8);
        read_raw(&ev->attach_type);
        read_raw(&ev->attach_id);
        break;
    case 9:
        pbitread((uint32_t *)&ev->param, 8);
        read_vec(&ev->pos);
        cs_pread(&ev->sound_a, 4, 1);
        cs_pread(&ev->sound_b, 4, 1);
        break;
    case 10:
        pbitread((uint32_t *)&ev->param, 8);
        pbitread((uint32_t *)&ev->attach_type, 1);
        break;
    case 11:
    case 12:
        read_raw(&ev->param);
        read_raw(&ev->attach_type);
        read_raw(&ev->attach_id);
        read_raw(&ev->range_mode);
        break;
    case 13:
    case 15:
        read_raw(&ev->param);
        read_raw(&ev->attach_type);
        read_raw(&ev->attach_id);
        break;
    case 14:
        pbitread((uint32_t *)&ev->param, 8);
        pbitread((uint32_t *)&ev->attach_type, 8);
        pbitread((uint32_t *)&ev->attach_id, 8);
        pbitread((uint32_t *)&ev->range_mode, 20);
        break;
    case 16:
        read_raw(&ev->param);
        read_raw(&ev->attach_type);
        read_raw(&ev->attach_id);
        read_or_none(&ev->range_mode, tmp, 10);
        read_vec(&ev->pos);
        read_vec(&ev->vec);
        break;
    case 17:
        pbitread((uint32_t *)&ev->param, 10);
        pbitread((uint32_t *)&ev->attach_type, 1);
        read_or_none(&ev->attach_id, tmp, 8);
        break;
    default:
        break;
    }
}

/* Read event number index of the ring. The type 0x12 is a placeholder: it has no fields. */
void unpackevent(int index)
{
    Event *ev = &sr_events[index];
    uint32_t tmp = (uint32_t)index, tmp2 = 0;
    pbitread((uint32_t *)&ev->type, 6);
    if (ev->type == 0x12)
        return;
    pbitread((uint32_t *)&ev->tick, 28);
    read_event_body(ev, &tmp, &tmp2);
}

/* The third row of an orientation is the cross product of the other two, as a unit vector. */
static void finish_orientation(vec3 rot[3])
{
    rot[0].x = (float)(rot[2].z * rot[1].y - rot[2].y * rot[1].z);
    rot[0].y = (float)(rot[1].z * rot[2].x - rot[1].x * rot[2].z);
    rot[0].z = (float)(rot[2].y * rot[1].x - rot[2].x * rot[1].y);
    normalizevector(&rot[0], &rot[0]);
}

/* Read a position and two rows of an orientation (the third row follows from them). */
static void read_pose(vec3 *pos, vec3 rot[3], uint32_t bits)
{
    cs_pread(&pos->x, 4, 1);
    cs_pread(&pos->y, 4, 1);
    cs_pread(&pos->z, 4, 1);
    unpackorientation(&rot[1], bits);
    unpackorientation(&rot[2], bits);
    finish_orientation(rot);
}

/* The place of a bone from its parent: the joint of the bone in the frame of the parent. */
static void place_bone(Human *hm, int b)
{
    Bone *bone = &hm->bones[b];
    const Bone *par = &hm->bones[sr_body[b].parent];
    const vec3 *joint = &sr_body[b].joint;
    vec3 d;
    bone->pos.x = (float)(joint->x * par->rot[0].x + par->pos.x);
    bone->pos.y = (float)(par->rot[0].y * joint->x + par->pos.y);
    bone->pos.z = (float)(joint->x * par->rot[0].z + par->pos.z);
    bone->pos.x = (float)(joint->y * par->rot[1].x + bone->pos.x);
    bone->pos.y = (float)(par->rot[1].y * joint->y + bone->pos.y);
    bone->pos.z = (float)(joint->y * par->rot[1].z + bone->pos.z);
    bone->pos.x = (float)(joint->z * par->rot[2].x + bone->pos.x);
    bone->pos.y = (float)(par->rot[2].y * joint->z + bone->pos.y);
    bone->pos.z = (float)(joint->z * par->rot[2].z + bone->pos.z);
    memcpy(bone->rot, par->rot, sizeof bone->rot);
    rotateorientation(bone->rot, &bone->rot[0], bone->joint_angles.x);
    rotateorientation(bone->rot, &bone->rot[1], bone->joint_angles.y);
    rotateorientation(bone->rot, &bone->rot[2], bone->joint_angles.z);
    d.x = (float)(sr_body[b].pos.x - joint->x);
    d.y = (float)(sr_body[b].pos.y - joint->y);
    d.z = (float)(sr_body[b].pos.z - joint->z);
    bone->pos.x = (float)(bone->pos.x + d.x * bone->rot[0].x);
    bone->pos.y = (float)(bone->rot[0].y * d.x + bone->pos.y);
    bone->pos.z = (float)(d.x * bone->rot[0].z + bone->pos.z);
    bone->pos.x = (float)(d.y * bone->rot[1].x + bone->pos.x);
    bone->pos.y = (float)(bone->rot[1].y * d.y + bone->pos.y);
    bone->pos.z = (float)(d.y * bone->rot[1].z + bone->pos.z);
    bone->pos.x = (float)(bone->pos.x + d.z * bone->rot[2].x);
    bone->pos.y = (float)(bone->rot[2].y * d.z + bone->pos.y);
    bone->pos.z = (float)(d.z * bone->rot[2].z + bone->pos.z);
}

static void read_humans(ObjectPacket *op, uint32_t *id, uint32_t *kind)
{
    uint32_t tmp = 0;
    int i, b;
    pbitread((uint32_t *)&op->num_humans, 8);
    for (i = 0; i < op->num_humans; i++) {
        Human *hm;
        pbitread(id, 10);
        pbitread(kind, 1);
        if (*kind != 0)
            continue;
        hm = &sr_humans[*id];
        read_or_none(&hm->vehicle_id, &tmp, 8);
        pbitread((uint32_t *)&hm->vehicle_seat, 4);
        pbitread((uint32_t *)&hm->health, 7);
        hm->health -= 10;
        pbitread((uint32_t *)&hm->bleeding, 1);
        hm->net_tick_prev = hm->net_tick;
        hm->net_pos_prev = hm->net_pos;
        hm->net_tick = sr_game.ticks;
        for (b = 0; b < 15; b++) {
            Bone *bone = &hm->bones[b];
            if (hm->vehicle_id != -1 && b >= 9)
                continue;
            if (b == 0) {
                read_pose(&bone->pos, bone->rot, 31);
                hm->net_pos = bone->pos;
            } else {
                unpackangle(&bone->joint_angles.x, 16);
                unpackangle(&bone->joint_angles.y, 16);
                unpackangle(&bone->joint_angles.z, 16);
                place_bone(hm, b);
            }
        }
    }
}

static void read_objects(ObjectPacket *op, uint32_t *id, uint32_t *mode)
{
    uint32_t raw = 0;
    int i, k;
    pbitread((uint32_t *)&op->num_objects, 8);
    for (i = 0; i < op->num_objects; i++) {
        Object *ob;
        pbitread(id, 10);
        pbitread(mode, 2);
        ob = &sr_objects[*id];
        ob->net_tick_prev = ob->net_tick;
        ob->net_pos_prev = ob->net_pos;
        ob->net_tick = sr_game.ticks;
        cs_pread(&ob->pos.x, 4, 1);
        cs_pread(&ob->pos.y, 4, 1);
        cs_pread(&ob->pos.z, 4, 1);
        ob->net_pos = ob->pos;
        if (*mode != 0 && *mode != 1)
            continue;
        unpackorientation(&ob->rot[1], *mode == 0 ? 31 : 21);
        unpackorientation(&ob->rot[2], *mode == 0 ? 31 : 21);
        finish_orientation(ob->rot);
        for (k = 0; k < 4; k++)
            unpackangle(&ob->wheels[k].angle, 8);
        unpackangle(&ob->wheels[0].steer, 8);
        unpackangle(&ob->wheels[1].steer, 8);
        for (k = 0; k < 4; k++) {
            pbitread(&raw, 6);
            ob->wheels[k].travel = unpackscalar(raw, 6);
        }
        for (k = 0; k < 4; k++) {
            pbitread(&raw, 6);
            ob->wheels[k].slip = unpackscalar(raw, 6);
        }
        pbitread(&raw, 8);
        ob->vel.x = unpackscalar(raw, 8);
        pbitread(&raw, 8);
        ob->vel.y = unpackscalar(raw, 8);
        pbitread(&raw, 8);
        ob->vel.z = unpackscalar(raw, 8);
        pbitread((uint32_t *)&ob->rpm_net, 13);
    }
}

static void read_items(ObjectPacket *op)
{
    uint32_t id = 0, cash = 0;
    int i;
    pbitread((uint32_t *)&op->num_items, 8);
    for (i = 0; i < op->num_items; i++) {
        Item *it;
        pbitread(&id, 10);
        it = &sr_items[id];
        read_pose(&it->pos, it->rot, 31);
        pbitread(&cash, 1);
        if (cash != 0) {
            pbitread((uint32_t *)&it->last_bill, 4);
            pbitread((uint32_t *)&it->num_bills, 4);
            pbitread(&it->bills_mask, 30);
        }
    }
}

static void read_cityobjects(ObjectPacket *op)
{
    uint32_t id = 0, raw = 0;
    int i;
    pbitread((uint32_t *)&op->num_cityobjects, 8);
    for (i = 0; i < op->num_cityobjects; i++) {
        CityObjectRec *rec;
        pbitread(&id, 10);
        rec = &sr_cityobjects[id];
        rec->active = 1;
        rec->type = 2;
        read_pose(&rec->pos, rec->rot, 31);
        pbitread((uint32_t *)&rec->num_floors, 7);
        pbitread((uint32_t *)&rec->floor, 7);
        pbitread(&raw, 6);
        rec->net_scalar = unpackscalar(raw, 6);
    }
}

/* Read the humans, objects, items and city objects of a state packet. */
void unpackobjectpacket(void)
{
    ObjectPacket *op = &sr_objectpacket;
    uint32_t id = 0, kind = 0;
    read_humans(op, &id, &kind);
    read_objects(op, &id, &kind);
    read_items(op);
    read_cityobjects(op);
}

/* The server sends the inventory of a human as a list in each slot. The client rebuilds the
 * lists from the items: each item that a human holds goes in its slot. */
static void rebuild_inventories(void)
{
    int h, i;
    for (h = 0; h < SR_MAX_HUMAN; h++) {
        if (sr_humans[h].active != 0) {
            int s;
            for (s = 0; s < 7; s++)
                sr_humans[h].inventory[s].count = 0;
        }
    }
    for (i = 0; i < SR_MAX_ITEM; i++) {
        const Item *it = &sr_items[i];
        if (it->active != 0 && it->parent_human != -1) {
            InventorySlot *slot = &sr_humans[it->parent_human].inventory[it->parent_slot];
            slot->items[slot->count] = i;
            slot->count++;
        }
    }
}

/* The type 6 packet: a new game. */
static void read_new_game(void)
{
    sr_areasector.client_timeout = 0;
    cs_pread(&sr_areasector.client_game_id, 4, 1);
    pbitread((uint32_t *)&sr_game.gametype, 4);
    pbitread((uint32_t *)&sr_game.round_mode, 4);
    unpackangle(&sr_game.start_angle_a, 16);
    unpackangle(&sr_game.start_angle_b, 16);
}

/* The rows of the shop that the player looks at (menu 10). */
static void read_shop(int building)
{
    int *shop = (int *)((uint8_t *)&sr_city.shop_count + building * 0x9dc);
    int k;
    pbitread((uint32_t *)&shop[0], 8);
    for (k = 0; k < shop[0]; k++) {
        pbitread((uint32_t *)&shop[1 + 3 * k], 8);
        pbitread((uint32_t *)&shop[2 + 3 * k], 24);
        pbitread((uint32_t *)&shop[3 + 3 * k], 4);
    }
}

/* The part of the state packet after the tick: the game, the player and the objects. */
static void read_state(int bytes)
{
    int view, k, j;
    uint32_t tmp = 0, count = 0, index = 0;
    vec3 vel = { 0.0f, 0.0f, 0.0f };
    Player *pl;
    pbitread((uint32_t *)&sr_game.state, 4);
    if (sr_game.state == 1) {
        for (k = 0; k < 32; k++)
            pbitread((uint32_t *)&sr_players[k].ready, 1);
    }
    pbitread((uint32_t *)&sr_game.timer, 16);
    pbitread((uint32_t *)&sr_game_world.clock, 30);
    pbitread((uint32_t *)&sr_game.team0_members, 6);
    pbitread((uint32_t *)&sr_game.team1_members, 6);
    pbitread((uint32_t *)&sr_game.team2_members, 6);
    pbitread((uint32_t *)&sr_view, 8);
    view = sr_view;
    pl = &sr_players[view];
    read_or_none(&pl->human_id, &tmp, 10);
    unpackangle(&pl->look_yaw, 16);
    unpackangle(&pl->look_pitch, 16);
    cs_pread(&vel.x, 4, 1);
    cs_pread(&vel.y, 4, 1);
    cs_pread(&vel.z, 4, 1);
    if (pl->human_id != -1) {
        Human *hm = &sr_humans[pl->human_id];
        hm->axis = pl->look_yaw;
        hm->pitch = pl->look_pitch;
        hm->bones[2].vel = vel;
    }
    pbitread((uint32_t *)&pl->menu, 8);
    pbitread((uint32_t *)&pl->menu_building, 16);
    if (pl->menu == 10)
        read_shop(pl->menu_building);
    cs_pread(&pl->money, 4, 1);
    pbitread((uint32_t *)&pl->action_tail, 8);
    pbitread((uint32_t *)&pl->hud_health, 8);
    pbitread((uint32_t *)&pl->hud_value6, 6);
    pbitread((uint32_t *)&pl->hud_value8, 8);
    for (k = 0; k < 7; k++) {
        int *slot = &pl->hud_inventory[k].count;
        pbitread((uint32_t *)slot, 4);
        for (j = 1; j <= slot[0]; j++)
            pbitread((uint32_t *)&slot[j], 16);
    }
    unpackobjectpacket();
    sr_debug.net_flag = 0;
    pbitread((uint32_t *)&sr_areasector.client_event_total, 16);
    pbitread(&count, 6);
    pbitread(&index, 16);
    for (k = 0; k < (int)count; k++) {
        unpackevent((int)index);
        if (sr_numofevents == index)
            sr_numofevents = (sr_numofevents + 1) & 0xffff;
        index = (index + 1) & 0xffff;
    }
    rebuild_inventories();
    sr_areasector.client_connected = 1;
    sr_serverinfo.last_packet_size = bytes;
    if (sr_serverinfo.max_packet_size < bytes)
        sr_serverinfo.max_packet_size = bytes;
}

/* The type 8 packet: the voice frames of the speakers. Each channel has a ring of 64 frames. */
static void read_voice(void)
{
    uint32_t tmp = 0, frame = 0;
    int c, k;
    for (c = 0; c < NUM_CHANNELS; c++) {
        VoiceChannel *ch = &sr_voice_channels[c];
        pbitread((uint32_t *)&ch->active, 1);
        if (ch->active == 0)
            continue;
        read_or_none(&ch->human, &tmp, 8);
        read_or_none(&ch->item, &tmp, 8);
        for (k = 0; k < 4; k++) {
            pbitread(&frame, 6);
            pbitread((uint32_t *)&ch->size[frame], 8);
            pbitread((uint32_t *)&ch->mode[frame], 2);
            cs_pread(ch->data[frame], ch->size[frame], 1);
        }
        ch->head = (int)((frame + 1) & (MAX_RING_FRAME - 1));
    }
}

/* Each tick of the client: handle the packets from the server. After 0x4b0 ticks without a
 * packet the client leaves the game. */
void clientrecv(void)
{
    uint8_t type = 0xff; /* the type byte of the last packet: a short packet keeps the old one */
    uint32_t gid = 0;
    int n;
    sr_areasector.client_timeout++;
    if (sr_areasector.client_timeout > TIMEOUT_TICKS)
        sr_game.link_state = 3;
    for (n = recvpacket(); n > 0; n = recvpacket()) {
        cs_pread(&type, 1, 1);
        if (type == 6) {
            read_new_game();
        } else if (type == 5) {
            sr_areasector.client_timeout = 0;
            cs_pread(&gid, 4, 1);
            if ((int)gid == sr_game.round_number) {
                cs_pread(&gid, 4, 1);
                if (sr_game.ticks <= (int)gid || sr_game.ticks - (int)gid > 99) {
                    sr_game.ticks = (int)gid;
                    read_state(n);
                }
            }
        } else if (type == 8) {
            read_voice();
        }
    }
}
