# Buildings and city objects

## Records

`Building` holds three grids of 64 floors, 64 rows and 64 columns.
Each `BuildingCell` occupies 0x48 bytes. The file grid starts at `cells`.
`build_cells` holds the block and material ids for this server.
`rot_cells` keeps the cells before a turn.
Each grid has 16 `BuildingArea` boxes for special rooms and safe zones.

The dimensions are `size_x`, `size_z` and `floors`.
The working dimensions are `build_size_x`, `build_size_z` and `build_floors`.
`materials` and `blocks` each hold 256 names of 64 bytes.

`BuildingType.interior_blocks` caches block ids for interior codes.
Its 256 `BuildPieceCopy` entries omit the index from `BuildPiece`.
The 64 `BuildingPart` entries hold colors.
`BuildPiece.flags` selects the left frame, right frame, sill and lintel with bits 1, 2, 4 and 8.
`DoorBlock.turn` stores a turn from 0 through 3.

## City objects

`createcityobject` uses the first inactive slot among 1024 city objects.
It sets the type, position and orientation. Other fields keep their values.
A full pool returns -1.

`city_createelevator` selects the last building whose bounds contain the position.
The lower bounds are inclusive. The upper bounds are exclusive. A NaN coordinate fails the test.
An existing elevator keeps its object id.
A new elevator uses the center of `elevator_block` and receives the building id and floor count.
The city building records have a stride of 0x9dc bytes.

`cityobject_addbox` adds eight vertices and six faces to a type.
Each coordinate rounds once after the scale and translation.
Face grids keep their values.
`setupcityobjecttypes` appends the boxes for types 1, 4, 5, 8 and 9.
Type 2 has eight fixed vertices and two faces.
Its face indices stay literal even when the vertex count starts above zero.
Type 8 scales only its first four vertices by 0.75.

`setupbuildingtypes` sets the first color of every part to white.
Part 0 also receives trim, dark and team colors.
It defines four opening types: `test`, `test2`, `glassonly` and `parking`.
It copies all eight pieces of each type, including the slots that keep their values.
The 32 copied pieces receive consecutive indices.

## Newspaper

`newspaper_load` reads at most 1013 lines from `readme.txt`.
It appends a title and the ten accounts with the most money.
The ranking uses unsigned radix keys equal to `0x8fffffff - money` with integer wrap.
The text buffers are shared game state.
The title and ten accounts fit in the 1024-line array.

## Building files

`building_load` accepts `.sbb` versions 6 through 10.
The loader accepts dimensions from 0 through 63 and name counts from 0 through 256.
Invalid dimensions and name counts use the missing-file path. Name tables keep their previous values on rejection.
The file stores cells in floor, row and column order. Rows and columns include the boundary cells.
Version 7 adds the interior bit 18. Version 8 keeps the cell color.
Version 9 adds the 16 area boxes. Version 10 adds the guard direction and prize flag.
A missing file still prepares the working floor plan. A bad version or a header-only read returns before that preparation.
After a complete read, the last row and column lose their outer and inner blocks.

`building_save` scans the whole cell grid. Occupied cells can increase the dimensions, which never decrease here.
It writes version 10 and keeps every cell field, including `spare`.
Building and block paths use `snprintf`. Names cannot contain separators or two consecutive dots.
The complete path must fit in 64 bytes.

`building_setupbuild` maps file names to server material and block ids.
The last matching name wins. Unknown names map to zero.
Special block words add the turn to the mapped block id.
Outer textures keep their high byte. Inner textures lose their high byte.
The destination `spare` field keeps its value.

`building_setupfloorplannewer` copies the working cells to `FloorplanCell` records.
It sets the ceiling bit from the cell above or the top floor.
For plain outer blocks with empty interiors or doors, adjacent walls flip texture bit 15.
The wall test accepts values 1 through 8 and 12.
Neighbor indices can cross a row or floor boundary.

`building_rotatefloorplan` turns the working grid by 90 degrees.
It copies the source cells, clears the destination grid and swaps the working dimensions.
A special block moves by `1 - AreaBlock.size_z` cells and changes its turn within the configured id range.
The first block at each destination wins.
Textures and walls turn with the grid. Guard directions advance from 4 to 1.
Area boxes exchange their horizontal dimensions and keep their height.

## Placement in an area

`building_buildfloorplan` removes occupied outer cells, creates the floor slab, and places the outer blocks and their colors.
It then places the interior blocks. An inner word of `0xffff` builds or finds an interior from its wall code.
The code keeps overlapping file bits. It also adds corners from adjacent walls and doors.
Neighbor reads can cross the grid boundary into the preceding row, floor or bytes before the grid.
An existing interior block prevents replacement by a generated block.

Team types 0 through 4 set two door cells, base bounds and safe bounds.
They append eight car spots after two orientation turns.
The position rounds to float after each offset and each advance to the next spot.
Guard spots stop at 64 entries. The last prize cell sets the prize position.
The nearest intersection supplies two street road ids. The first equal distance wins.
With no intersection, the function still reads road ids from entry 0.

Types 10 through 15 add a city building record. Other types keep the current record count.

| Type | Building kind | Extra state | Return value |
| --- | --- | --- | --- |
| 10 | 3 | None | z + size_z |
| 11 | 2 | Car spawn and empty shop | build_size_z - 6 + z |
| 12 | 5 | Empty shop | Record byte offset |
| 13 | 4 | Two shop rows when the server is active | Shop count address when active, else record byte offset |
| 14 | 7 | Empty shop | Record byte offset |
| 15 | 6 | Bounds from area 0, shop count stays | Area 0 far z |

Each building record has a stride of 0x9dc bytes.
The return values preserve the bits that callers receive, including the address for type 13.

## Checks

`tools/lift/synth_city_objects.py --run` tests allocation, elevator bounds, NaN coordinates,
a full object pool, appended meshes, fractional box dimensions and newspaper overflow.

`tools/lift/synth_buildings.py --run` tests all five file versions, missing and bad files,
header-only reads, name maps, doors, wide blocks, turns and guard directions.
The save case compares both output files with the expected bytes.
Placement cases cover every type with the server active and inactive, all four door turns,
the guard limit, empty intersection lists, wall codes, corner neighbors and existing interior blocks.
