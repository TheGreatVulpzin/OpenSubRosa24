/* Garage door blocks of the three teams. */
#include "building_actions.h"
#include "area.h"
#include "logic.h"

/* Add or remove the two garage doors of a team and mark their sectors. */
void building_addgaragedoor(int team, int remove)
{
    const DoorBlock *doors = (const DoorBlock *)TEAM_FIELD(team0_doors, team);
    int index;
    for (index = 0; index < 2; ++index) {
        const DoorBlock *door = &doors[index];
        uint32_t object = remove ? 0 : ((uint32_t)door->turn << 10) | 9u;
        area_interior_setobject(0, door->x, door->y, door->z, object);
        area_marksector(0, door->x, door->y, door->z);
    }
}
