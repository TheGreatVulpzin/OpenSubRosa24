/* The objects of a state packet, as the client reads them.
 *
 * cl_unpackobjectpacket reads the humans, the vehicles, the items and the city objects of a
 * state packet and then runs the client simulation. It keeps the place and the tick of the
 * state before for the smoothing. The third row of each orientation is the cross product of
 * the third and the second row, made unit length.
 */
#include "setup/unpack_objects.h"

#include <stdint.h>

#include "client.h"
#include "../../net/packet.h"
#include "setup/interpolation.h"
#include "sr_runtime.h"
#include "vectors.h"

/* Game functions of other files. */
#define unpackorientation CL_FN_NAMED(unpackorientation, 0x468bd0, void(__cdecl *)(vec3 *out, int bits))
#define unpackangle CL_FN_NAMED(unpackangle, 0x468ec0, void(__cdecl *)(float *out, uint32_t bits))
#define unpackposition CL_FN_NAMED(unpackposition, 0x468f80, void(__cdecl *)(vec3 *out))
#define unpackscalar CL_FN_NAMED(unpackscalar, 0x469110, void(__cdecl *)(float *out, float scale, uint32_t bits))
#define crossproduct CL_FN_NAMED(crossproduct, 0x50b830, void(__cdecl *)(vec3 *out, const vec3 *a, const vec3 *b))

#define BONES 15
#define FIRST_LEG_BONE 9 /* a human in a vehicle sends only bones 0 to 8 */
#define HEALTH_BIAS 10

/* The bit widths of the fields of the object packet. */
#define COUNT_BITS 8
#define ID_BITS 10
#define HUMAN_KIND_BITS 1
#define VEHICLE_BITS 8
#define SEAT_BITS 4
#define HEALTH_BITS 7
#define BLEEDING_BITS 1
#define ROT_BITS 31          /* an orientation row */
#define ROT_SHORT_BITS 21    /* an orientation row of a vehicle in mode 1 */
#define ANGLE_BITS 16        /* a joint angle */
#define OBJECT_MODE_BITS 2
#define WHEEL_ANGLE_BITS 8
#define WHEEL_SCALAR_BITS 6
#define VELOCITY_BITS 8
#define RPM_BITS 13
#define CASH_FLAG_BITS 1
#define LAST_BILL_BITS 4
#define NUM_BILLS_BITS 4
#define BILLS_MASK_BITS 30
#define FLOOR_BITS 7
#define CITY_SCALAR_BITS 6
#define NUM_WHEELS 4
#define CITYOBJECT_ELEVATOR 2

/* Read rows 1 and 2 of an orientation. Row 0 is their cross product. */
static void read_orientation(vec3 rot[3], int bits)
{
    unpackorientation(&rot[1], bits);
    unpackorientation(&rot[2], bits);
    crossproduct(&rot[0], &rot[2], &rot[1]);
    normalizevector(&rot[0], &rot[0]);
}

/* A bone hangs from its parent: the joint of the body goes into the frame of the parent, the
 * bone turns by its joint angles, and the bone moves from the joint to its place. */
static void place_bone(Human *hm, int b)
{
    Bone *bone = &hm->bones[b];
    const Body *body = &sr_data_body[b];
    const Bone *parent = &hm->bones[body->parent];
    vec3 offset;

    scaleaddvectors(&bone->pos, &parent->pos, &parent->rot[0], body->joint.x);
    scaleaddvectors(&bone->pos, &bone->pos, &parent->rot[1], body->joint.y);
    scaleaddvectors(&bone->pos, &bone->pos, &parent->rot[2], body->joint.z);
    bone->rot[0] = parent->rot[0];
    bone->rot[1] = parent->rot[1];
    bone->rot[2] = parent->rot[2];
    rotateorientation(bone->rot, &bone->rot[0], bone->joint_angles.x);
    rotateorientation(bone->rot, &bone->rot[1], bone->joint_angles.y);
    rotateorientation(bone->rot, &bone->rot[2], bone->joint_angles.z);
    subtractvectors(&offset, &body->pos, &body->joint);
    scaleaddvectors(&bone->pos, &bone->pos, &bone->rot[0], offset.x);
    scaleaddvectors(&bone->pos, &bone->pos, &bone->rot[1], offset.y);
    scaleaddvectors(&bone->pos, &bone->pos, &bone->rot[2], offset.z);
}

/* The humans: the vehicle, the seat, the health, the bleeding flag and the bones. Bone 0 has
 * a place and an orientation, the other bones have joint angles. A record with the kind bit set
 * carries nothing more. */
static void read_humans(ObjectPacket *op)
{
    int i, b;

    pbitread((uint32_t *)&op->num_humans, COUNT_BITS);
    for (i = 0; i < op->num_humans; i++) {
        uint32_t id = 0, kind = 0;
        Human *hm;

        pbitread(&id, ID_BITS);
        pbitread(&kind, HUMAN_KIND_BITS);
        if (kind != 0)
            continue;
        hm = &sr_data_human[id];
        pbitreadnego((uint32_t *)&hm->vehicle_id, VEHICLE_BITS);
        pbitread((uint32_t *)&hm->vehicle_seat, SEAT_BITS);
        pbitread((uint32_t *)&hm->health, HEALTH_BITS);
        hm->health -= HEALTH_BIAS;
        pbitread((uint32_t *)&hm->bleeding, BLEEDING_BITS);
        hm->net_tick_prev = hm->net_tick;
        hm->net_pos_prev = hm->net_pos;
        hm->net_tick = sr_data_game.ticks;
        for (b = 0; b < BONES; b++) {
            Bone *bone = &hm->bones[b];

            if (hm->vehicle_id != -1 && b >= FIRST_LEG_BONE)
                continue;
            if (b == 0) {
                unpackposition(&bone->pos);
                hm->net_pos = bone->pos;
                read_orientation(bone->rot, ROT_BITS);
            } else {
                unpackangle(&bone->joint_angles.x, ANGLE_BITS);
                unpackangle(&bone->joint_angles.y, ANGLE_BITS);
                unpackangle(&bone->joint_angles.z, ANGLE_BITS);
                place_bone(hm, b);
            }
        }
    }
}

/* The vehicles. Mode 0 and 1 carry the orientation (31 or 21 bits), the wheels, the speed and
 * the engine; other modes carry only the place. */
static void read_objects(ObjectPacket *op)
{
    int i, k;

    pbitread((uint32_t *)&op->num_objects, COUNT_BITS);
    for (i = 0; i < op->num_objects; i++) {
        uint32_t id = 0, mode = 0;
        Object *ob;

        pbitread(&id, ID_BITS);
        pbitread(&mode, OBJECT_MODE_BITS);
        ob = &sr_data_object[id];
        ob->net_tick_prev = ob->net_tick;
        ob->net_pos_prev = ob->net_pos;
        ob->net_tick = sr_data_game.ticks;
        unpackposition(&ob->pos);
        ob->net_pos = ob->pos;
        if (mode != 0 && mode != 1)
            continue;
        read_orientation(ob->rot, mode == 0 ? ROT_BITS : ROT_SHORT_BITS);
        for (k = 0; k < NUM_WHEELS; k++)
            unpackangle(&ob->wheels[k].angle, WHEEL_ANGLE_BITS);
        unpackangle(&ob->wheels[0].steer, WHEEL_ANGLE_BITS);
        unpackangle(&ob->wheels[1].steer, WHEEL_ANGLE_BITS);
        for (k = 0; k < NUM_WHEELS; k++)
            unpackscalar(&ob->wheels[k].travel, 1.0f, WHEEL_SCALAR_BITS);
        for (k = 0; k < NUM_WHEELS; k++)
            unpackscalar(&ob->wheels[k].slip, 1.0f, WHEEL_SCALAR_BITS);
        unpackscalar(&ob->vel.x, 1.0f, VELOCITY_BITS);
        unpackscalar(&ob->vel.y, 1.0f, VELOCITY_BITS);
        unpackscalar(&ob->vel.z, 1.0f, VELOCITY_BITS);
        pbitread((uint32_t *)&ob->rpm_net, RPM_BITS);
    }
}

/* The items: the place, the orientation and, for a stack of bills, the bills. */
static void read_items(ObjectPacket *op)
{
    int i;

    pbitread((uint32_t *)&op->num_items, COUNT_BITS);
    for (i = 0; i < op->num_items; i++) {
        uint32_t id = 0, cash = 0;
        Item *it;

        pbitread(&id, ID_BITS);
        it = &sr_data_item[id];
        unpackposition(&it->pos);
        read_orientation(it->rot, ROT_BITS);
        pbitread(&cash, CASH_FLAG_BITS);
        if (cash != 0) {
            pbitread((uint32_t *)&it->last_bill, LAST_BILL_BITS);
            pbitread((uint32_t *)&it->num_bills, NUM_BILLS_BITS);
            pbitread(&it->bills_mask, BILLS_MASK_BITS);
        }
    }
}

/* The city objects (the elevators): each one gets active and type 2. */
static void read_cityobjects(ObjectPacket *op)
{
    int i;

    pbitread((uint32_t *)&op->num_cityobjects, COUNT_BITS);
    for (i = 0; i < op->num_cityobjects; i++) {
        uint32_t id = 0;
        CityObjectRec *rec;

        pbitread(&id, ID_BITS);
        rec = &sr_data_cityobject[id];
        rec->active = 1;
        rec->type = CITYOBJECT_ELEVATOR;
        unpackposition(&rec->pos);
        read_orientation(rec->rot, ROT_BITS);
        pbitread((uint32_t *)&rec->num_floors, FLOOR_BITS);
        pbitread((uint32_t *)&rec->floor, FLOOR_BITS);
        unpackscalar(&rec->net_scalar, 1.0f, CITY_SCALAR_BITS);
    }
}

/* Read the objects of a state packet, then run the client simulation. */
void __cdecl cl_unpackobjectpacket(void)
{
    ObjectPacket *op = &sr_data_objectpacket;

    read_humans(op);
    read_objects(op);
    read_items(op);
    read_cityobjects(op);
    client_simulation();
}
