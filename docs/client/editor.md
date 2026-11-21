# Building editor

Files: `src/client/editor/building.c`, `building_loop.c`, `menus.c`. Header: `building.h`, `building_loop.h`, `menus.h`.

The client opens the building editor only when the debug flag `debugit` is on. The client never sets the flag. For a test, set it with `subvulpes.ini` `[client] debugit=1` (the wsock32 proxy) or `SR_DEBUGIT=1` (the standalone client). Then the main menu shows **Editor**. It opens `buildingedit_menu` to choose a building, and F1 in the game opens `building_editorloop`.

Before the first frame, the level builder makes the window texture and allocates the sector vertex buffer. It then submits the sector meshes and makes the track model. The building mode runs this final setup before it returns.

A building has 64 floors of 64 by 64 cells (`BuildingCell`). The state of the editor is `cl_data_building_editor` (`BuildingEditor`), and its view is `cl_data_building_view` (`EditorView`).

## The loop

`building_editorloop` runs until Exit or Escape:

1. At the start, it finds the game material of each material name and the game block of each block name of the building. A left button that is down at the start counts only after a release (`building_click_wait`).
2. Each frame makes the items (Exit, the file name, the wall tools) and the picker tiles. The block mode has 13 texture tiles (6 outer slots, 7 inner slots) and one block tile for a special block.
3. It draws the floor in work from above, a preview of the block of each block tile, the texts and the tiles.
4. A, D, W and S move the camera in steps of 16 ms. Shift makes the steps 4 times longer.
5. At the end, it saves the building, removes its blocks from the world, builds it again at 36, 6, 36 and frees all interior meshes.

## Mouse in the view and the tiles

`building_checkeditor` runs each frame before the drawing:

- The wheel over a tile steps its value, with a wrap from `start` to `count - 1`. With shift, a texture tile of the outer sides (1 to 4) or of the inner sides (7 to 10) gives its value to the other tiles of the group.
- In the view, it finds the cell under the mouse (`cursor_x`, `cursor_z`). J sets the size of the building to that cell plus 1.
- The wall mode takes the cell edges too. With no button down, the wall goes along z when the mouse is nearer a line of whole x, else along x.
- The area mode sets the start of the chosen area (`area`) with a left click and its size while the button stays down.
- The wheel in the view changes the zoom from 2 to 256. Wheel steps out of the tiles and the view are lost.

## Keys

| Key | Action |
| --- | --- |
| F1, F2, F3, F4 | Mode: blocks, walls (edges), areas, markers (objects) |
| F5, F6 | The texture picker and the block picker |
| 1, 2, 3 (block mode) | The kind of a new block: normal, interior special, outer special |
| Q | Turn a new special block by 90 degrees |
| Y | Switch the floor of new blocks |
| Minus, equals | The block piece (0 to 3) |
| Brackets | The colors of the four sides (0 to 15) |
| H | The height of the building: the floor in work plus 1 |
| F, R | One floor down or up. With shift, the new floor gets a copy of the floor before |
| G | Take the blocks, the textures and the colors of the cell under the mouse |

## Mouse

- Block mode: a left click adds a block, a right click removes it. Shift and the left button drag a box. With a box, E adds blocks, T sets the texture slots, U sets the colors and Delete removes the blocks.
- Wall mode: a left click sets the wall of the chosen direction to the tool, a right click clears it.
- Marker mode: a left click counts the guard direction from 1 to 4, a right click clears it. With 1 held, the click changes the prize.

## Tests

No recorded call reaches the editor. The callees `buildingedit_render`, `buildingedit_addblock`, `buildingedit_deleteblock` and `building_save` have their own gate results. A test in the game window compares the loop with the flag on.
