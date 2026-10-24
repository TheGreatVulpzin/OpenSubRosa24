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

`newspaper_load` reads at most 1022 lines from `readme.txt`.
It appends a title and the ten accounts with the most money.
The ranking uses unsigned radix keys equal to `0x8fffffff - money` with integer wrap.
The text buffers are shared game state.
A full input file lets the ranking extend beyond the 1024-line array.
This behavior preserves the bytes that the game writes.

## Checks

`tools/lift/synth_city_objects.py --run` tests allocation, elevator bounds, NaN coordinates,
a full object pool, appended meshes, fractional box dimensions and newspaper overflow.
