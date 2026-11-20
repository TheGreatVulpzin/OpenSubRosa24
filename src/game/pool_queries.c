/* Queries and removals for the game pools.
 * Active flags mark occupied slots in the fixed pools.
 * Bullets use a packed pool and move the last entry into a removed slot.
 */
#include "pool_queries.h"

#include "sr_runtime.h"
#include "events.h"

enum { CITY_OBJECT_LIMIT = 1024 };

/* Return the first free city object, or minus one when the pool is full. */
int getnextcityobject(void)
{
    int index;
    for (index = 0; index < CITY_OBJECT_LIMIT; ++index)
        if (sr_data_cityobject[index].active == 0)
            return index;
    return -1;
}

/* Return the first free human, or minus one when the pool is full. */
int getnexthuman(void)
{
    int index;
    for (index = 0; index < SR_MAX_HUMAN; ++index)
        if (sr_humans[index].active == 0)
            return index;
    return -1;
}

/* Return the first free item, or minus one when the pool is full. */
int getnextitem(void)
{
    int index;
    for (index = 0; index < SR_MAX_ITEM; ++index)
        if (sr_items[index].active == 0)
            return index;
    return -1;
}

/* Return the first free object, or minus one when the pool is full. */
int getnextobject(void)
{
    int index;
    for (index = 0; index < SR_MAX_OBJECT; ++index)
        if (sr_objects[index].active == 0)
            return index;
    return -1;
}

/* Return the first free player, or minus one when the pool is full. */
int getnextplayer(void)
{
    int index;
    for (index = 0; index < SR_MAX_PLAYER; ++index)
        if (sr_players[index].active == 0)
            return index;
    return -1;
}

/* Release a city object. An index outside the pool has no effect. */
void deletecityobject(int index)
{
    if ((unsigned)index < CITY_OBJECT_LIMIT)
        sr_data_cityobject[index].active = 0;
}

/* Remove a bullet. The last bullet fills the gap. */
void deletebullet(int index)
{
    if (index < 0 || index >= sr_numofbullets)
        return;
    --sr_numofbullets;
    if (index != sr_numofbullets)
        sr_bullets[index] = sr_bullets[sr_numofbullets];
}

/* Remove an object and release the humans and particles that refer to it. */
void deleteobject(int object)
{
    Object *body;
    int index;
    if ((unsigned)object >= SR_MAX_OBJECT)
        return;
    body = &sr_objects[object];
    for (index = 0; index < body->num_particles; ++index)
        sr_particles[body->particles[index]].time_to_live = 0;
    for (index = 0; index < SR_MAX_HUMAN; ++index)
        if (sr_humans[index].active && sr_humans[index].vehicle_id == object)
            sr_humans[index].vehicle_id = -1;
    body->active = 0;
    createevent_updateobject(object, 3, 0, &body->pos, NULL);
}

