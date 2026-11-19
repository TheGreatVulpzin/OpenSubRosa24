/* The list of window cells: a texture and a window type that the level builder has used. */
#include "textures/window_atlas.h"

#include "client.h"
#include "sr_runtime.h"

#define g_windowatlas cl_data_texture_windowatlas
#define ATLAS_LAST_CELL 0x3e

/* Find the cell of a material and a window type and give its number. A cell that is not in the list is
 * added at the end of the list, and the list holds 63 cells: a full list reuses its last cell. */
int __cdecl addwindowatlas(int material, int window)
{
    int cell = 0;

    for (;;) {
        if (cell > ATLAS_LAST_CELL || g_windowatlas.num_cells <= cell) {
            g_windowatlas.cells[cell].material = material;
            g_windowatlas.cells[cell].window = window;
            g_windowatlas.num_cells = cell + 1;
            return cell;
        }
        if (g_windowatlas.cells[cell].material == material && g_windowatlas.cells[cell].window == window)
            return cell;
        cell++;
    }
}
