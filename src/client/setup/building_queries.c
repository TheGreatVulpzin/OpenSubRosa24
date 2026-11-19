/* Small questions about the floor plan and the material names of the buildings. */
#include "setup/building_queries.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"

#define FLOOR_CELLS 4096
#define ROW_CELLS 64
#define MATERIAL_NAMES 0x100
#define MATERIAL_NAME_SIZE 64
#define WALL_DOOR 9

/* Tell if the wall value of a cell of the floor plan is a window: 1 to 8, or the door value 9
 * when door is not 0. side 0 reads the wall of the row and another side reads the wall of the column. */
int __cdecl building_iswindow(int column, int floor, int row, int side, int door)
{
    const FloorplanCell *cell = &sr_data_floorplan[floor * FLOOR_CELLS + row * ROW_CELLS + column];
    int wall = side == 0 ? cell->wall_row : cell->wall_col;

    if (0 < wall && wall < WALL_DOOR)
        return 1;
    if (door != 0 && wall == WALL_DOOR)
        return 1;
    return 0;
}

/* Find the number of the material of each material name of the building file. A name that is not in
 * the list of the game gets 0. When two materials have the name, the last one wins. */
void __cdecl building_reloadtextures(void)
{
    int name, material;

    for (name = 0; name < MATERIAL_NAMES; name++)
        sr_data_building.material_textures[name] = 0;
    for (name = 0; name < sr_data_building.num_materials; name++)
        for (material = 0; material < sr_data_materiallist.num_materials; material++)
            if (strcmp(&sr_data_building.materials[name * MATERIAL_NAME_SIZE], cl_data_material_records[material].name) == 0)
                sr_data_building.material_textures[name] = material;
}
