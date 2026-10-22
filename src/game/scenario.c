/* People and vehicles for missions and street traffic.
 * A full player list leaves index -1. The spawn paths still use that index.
 */
#include "scenario.h"

#include <stdio.h>
#include <string.h>

#include "ai_car.h"
#include "events.h"
#include "humans.h"
#include "items.h"
#include "logic.h"
#include "objects.h"
#include "player.h"
#include "sr_os.h"
#include "vectors.h"

#define g_yaxis (*SR_AT(vec3, 0x60ce0))
#define BUILDING_FIELD(field, building) \
    ((void *)((uint8_t *)&g_city.field + (building) * 0x9dc))

int scenario_armhuman(int human, int type, int mags)
{
    Human *h;
    int gun, mag, result, k;
    if (human == -1)
        return 0;
    h = &sr_humans[human];
    gun = createitem(type, &h->bones[0].pos, NULL, h->bones[0].rot);
    linkitem(gun, UINT32_MAX, human, 0);
    mag = createitem(type + 1, &h->bones[0].pos, NULL, h->bones[0].rot);
    result = linkitem(mag, UINT32_MAX, human, 1);
    for (k = 1; k < mags; ++k) {
        mag = createitem(type + 1, &h->bones[0].pos, NULL, h->bones[0].rot);
        result = linkitem(mag, UINT32_MAX, human, ((k - 1) >> 1) + 3);
    }
    return result;
}

int scenario_createcar(int type, int color, int road, int side, int lane, float dist)
{
    CityRoad *rd = &g_city.roads[road];
    CityIntersection *isec = &g_city.intersections[rd->ends[side != 0]];
    float shift;
    vec3 base, dir, pos, rot[3];
    if (side == 0) {
        shift = (float)((double)lane * 4.0 + 2.0);
        dir.x = rd->dir.x;
        dir.y = rd->dir.y;
        dir.z = rd->dir.z;
    } else {
        shift = (float)(-2.0 - (double)lane * 4.0);
        dir.x = -rd->dir.x;
        dir.y = -rd->dir.y;
        dir.z = -rd->dir.z;
    }
    base.x = (float)((double)shift * rd->side.x + isec->pos.x);
    base.y = (float)((double)rd->side.y * shift + isec->pos.y);
    base.z = (float)((double)shift * rd->side.z + isec->pos.z);
    resetorientation(rot);
    if (rd->kind == 0)
        rotateorientation(rot, &rot[1], -1.5707963705062866f);
    if (side == 0)
        rotateorientation(rot, &rot[1], 3.1415927410125732f);
    pos.x = (float)((double)dist * dir.x + base.x);
    pos.y = (float)((double)(float)((double)dir.y * dist + base.y) + 1.5);
    pos.z = (float)((double)dist * dir.z + base.z);
    return createobject(type, &pos, NULL, rot, color);
}

int scenario_createguards(int building, int item_type, int expire)
{
    int spots = *(int *)BUILDING_FIELD(num_guard_spots, building);
    GuardSpot *list = (GuardSpot *)BUILDING_FIELD(guard_spots, building);
    int r1, r2, r3, n, k, item;
    vec3 rot[3];
    if (spots <= 0)
        return 0;
    memset(g_guard_used, 0, 64 * sizeof(int));
    r1 = os_rand();
    r2 = os_rand();
    r3 = os_rand();
    n = (r3 & 1) + 2 + (r1 & 1) + (r2 & 1);
    if (n > spots)
        n = spots;
    for (k = 0; k < n; ++k) {
        int spot = os_rand() % spots;
        int tries = 0, player, human, type;
        float angle = 0;
        vec3 pos;
        Player *p;
        while (g_guard_used[spot] && tries < 0x200) {
            spot = os_rand() % spots;
            ++tries;
        }
        g_guard_used[spot] = 1;
        player = createplayer();
        os_rand();
        p = &sr_players[player];
        pos = list[spot].pos;
        p->team = 6;
        sprintf(p->name, "");
        p->is_bot = 1;
        p->despawn_time = expire;
        createevent_updateplayer(player);
        if (list[spot].dir == 1)
            angle = 1.5707963705062866f;
        else if (list[spot].dir == 2)
            angle = 3.1415927410125732f;
        else if (list[spot].dir == 3)
            angle = -1.5707963705062866f;
        resetorientation(rot);
        rotateorientation(rot, &g_yaxis, angle);
        human = createhuman(&pos.x, &rot[0].x, player);
        p->yaw = angle;
        p->human_id = human;
        type = (os_rand() & 1) == 0 ? 6 : 10;
        scenario_armhuman(human, type, 3);
    }
    resetorientation(rot);
    item = createitem(item_type, (vec3 *)BUILDING_FIELD(prize_pos, building), NULL, rot);
    g_scenario.items[0] = item;
    g_scenario.site = building;
    return item;
}

void scenario_createtraffic2(int n)
{
    int k;
    if (g_city.num_roads == 0)
        return;
    for (k = 0; k < n; ++k) {
        int player = createplayer();
        Player *p = &sr_players[player];
        int ra, road, rb, r, type, color, car, human, j;
        float dist;
        p->team = 6;
        sprintf(p->name, "");
        p->is_bot = 1;
        createevent_updateplayer(player);
        ra = os_rand();
        road = os_rand() % g_city.num_roads;
        rb = os_rand();
        dist = (float)((double)(k & 3) * 8.0 + 16.0);
        r = os_rand() & 15;
        type = r < 2 ? 7 : r < 6 ? 2 : r < 12 ? 0 : r < 14 ? 8 : (r < 15) * 4 + 3;
        color = os_rand() % 6;
        car = scenario_createcar(type, color, road, ra & 1, rb & 1, dist);
        human = createhuman(&sr_objects[car].pos.x, &sr_objects[car].rot[0].x, player);
        p->human_id = human;
        sr_humans[human].vehicle_id = car;
        sr_humans[human].vehicle_seat = 0;
        p->ai_waypoint = 0;
        p->num_path = 0;
        j = g_city.num_intersections < 1 ? 0 : os_rand() % g_city.num_intersections;
        p->ai_dest_x = g_city.intersections[j].x;
        p->ai_dest_z = g_city.intersections[j].z;
        p->waypoints[0].road = road;
        p->waypoints[0].dir = ra & 1;
        p->waypoints[0].lane = rb & 1;
        p->num_path = 1;
        ai_car_nextwaypoint(player);
    }
}

static int chase_crew(int car, int seat, int expire)
{
    int player = createplayer();
    Player *p = &sr_players[player];
    int human, type;
    p->team = 6;
    sprintf(p->name, "");
    p->is_bot = 1;
    p->despawn_time = expire;
    createevent_updateplayer(player);
    human = createhuman(&sr_objects[car].pos.x, &sr_objects[car].rot[0].x, player);
    p->human_id = human;
    sr_humans[human].vehicle_id = car;
    sr_humans[human].vehicle_seat = seat;
    type = (os_rand() & 1) == 0 ? 6 : 10;
    return scenario_armhuman(human, type, 4);
}

int scenario_createchase(int item_type, int expire)
{
    int player, ra, road, car, human, key, briefcase, r;
    Player *p;
    vec3 pos = {62, 10, 60}, rot[3];
    if (g_city.num_roads == 0)
        return 0;
    player = createplayer();
    p = &sr_players[player];
    p->team = 6;
    sprintf(p->name, "");
    p->is_bot = 1;
    p->despawn_time = expire;
    createevent_updateplayer(player);
    ra = os_rand();
    road = os_rand() % g_city.num_roads;
    car = scenario_createcar(3, 0, road, ra & 1, 1, 32.0f);
    human = createhuman(&sr_objects[car].pos.x, &sr_objects[car].rot[0].x, player);
    p->human_id = human;
    sr_humans[human].vehicle_id = car;
    sr_humans[human].vehicle_seat = 0;
    resetorientation(rot);
    key = createitem(24, &pos, NULL, rot);
    sr_items[key].vehicle_id = car;
    linkitem(key, UINT32_MAX, p->human_id, 4);
    briefcase = createitem(13, &pos, NULL, rot);
    sr_items[briefcase].type = 14;
    g_scenario.items[0] = createitem(item_type, &pos, NULL, rot);
    g_scenario.site = 255;
    linkitem(briefcase, g_scenario.items[0], -1, 0);
    sr_items[briefcase].type = 13;
    linkitem(briefcase, UINT32_MAX, p->human_id, 3);
    p->team = 6;
    p->ai_chase = 1;
    p->ai_waypoint = 0;
    p->num_path = 0;
    p->ai_dest_x = (os_rand() % 6 + 1) * 32;
    p->ai_dest_z = (os_rand() % 6 + 1) * 32;
    p->waypoints[0].road = road;
    p->waypoints[0].dir = ra & 1;
    p->waypoints[0].lane = 1;
    p->num_path = 1;
    ai_car_nextwaypoint(player);
    ai_car_nextwaypoint(player);
    chase_crew(car, 1, expire);
    if ((os_rand() & 1) == 0)
        chase_crew(car, 2, expire);
    r = os_rand();
    if ((r & 1) == 0)
        return chase_crew(car, 3, expire);
    return r;
}
