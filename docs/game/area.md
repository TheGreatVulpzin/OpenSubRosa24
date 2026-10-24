# Block areas

An area divides space into chunks of 64 cells on each axis. Each chunk contains 512 sectors.
A sector contains 512 cells, arranged as eight cells on each axis.
The index within both grids is `x + 8 * z + 64 * y`, with each coordinate masked to three bits.

There are four areas, 512 chunk records per area and 8192 shared sector records.
Each area's `chunk_index` maps its grid coordinates to chunk records. A value of -1 marks an empty position.
Each chunk maps its sector positions in the same way.

| Record | Size | Fields |
| --- | --- | --- |
| AreaChunk | 0x804 | used, sectors |
| BlockSector | 0x6068 | used, blocks, textures, colors, interior, interior_textures, cityobjects, interior_models, area, pos, dirty, num_meshes, unused_584c |
| AreaBlock | 0x84 | name, num_verts, verts, normals, tangents, uvs, num_faces, faces, face_slots, num_tris, tris, tri_slots, size_x, size_y, size_z, num_hideable, open_sides, is_door |

## Cell words

The high three bits give the kind of a cell word.

| Kind | Value | Meaning |
| --- | --- | --- |
| Normal | 0x40000000 | A cube with eight texture values |
| Special | 0x80000000 | A mesh whose id occupies the low 16 bits |
| Link | 0xc0000000 | A cell occupied by another cell's mesh |

A link stores its offsets in bits 0 through 7 for x, 8 through 15 for z and 16 through 23 for y.
Subtract these offsets to reach the owning cell.
Normal words use bits 0 through 5 for visible faces, in the order y-, z+, x-, z-, x+, y+.
Bits 6 through 13 indicate textures whose bit 15 is clear.

## Allocation and setters

`area_createchunk` takes the first free chunk. It clears all 512 sector indexes to -1.
`area_createsector` takes the first free sector. It clears outer words, interior words and city objects.
It sets all four `interior_models` to -1 and increments `Area.num_sectors`.
Textures, colors and interior textures keep their previous values when a sector is reused.

`area_setblock`, `area_setblockcolor` and `area_createblock` create missing chunks and sectors.
The interior setters require an existing sector.
`area_marksector` sets the sector's `dirty` field to 1.
`area_interior_setobject` leaves that flag unchanged, so its caller marks the sector when needed.

Each interior model covers a pair of cell layers. Its index is `(y >> 1) & 3`.
The interior setters release an existing model by setting its sector and its model index to -1.
`area_interior_createblock` does this for every word kind, including words that create no block.

## Creation, deletion and breakage

`area_createblock` hides shared faces between normal neighbors. The order is y-, y+, z+, z-, x-, x+.
At y = 0, it also hides the bottom face without a neighbor.
Special blocks create links in y/z/x order over their declared dimensions.
Each link setter can allocate a missing chunk or sector.
Unsupported nonzero kinds allocate the containing chunk and sector, then leave the cell unchanged.
A zero word does not allocate either record.

`area_interior_createblock` stores only the low byte of each 16-bit texture value.
Its links use the interior setter, which requires an existing sector.
`area_deleteblock` exposes the matching faces of normal neighbors, then clears the outer and interior words.
It keeps the interior model indexes. This differs from the interior setters and is required for exact behavior.

`area_breakblock` accepts a special word when the signed face index is below `num_hideable`.
It emits an update event, then sets the pair of hidden-face bits selected by `face & 0x1e`.
Odd and even face indexes therefore select the same pair.
Only x has a coordinate check before the event. The interior setter checks all coordinates.

## Area reset

`createarea` stores the origin, identity axes, dimensions, cell scale and inverse scale.
It frees the previous chunk index, allocates its replacement and fills the index with -1.
It clears the chunk use flags and the sector use flags for that area.
`num_sectors` keeps its value, and `unused_0064` becomes zero.

The inverse scale rounds to float at its store.
Each maximum bound uses double products and an addition, then rounds once to float at its store.
The bounds span 64 cells per chunk.

## Block and city files

`loadblock` reads `data/block/<name>.sbl`. It accepts versions 1 and 2.
Both versions store the dimensions, six side markers and surface records.
Each surface stores its two subdivision counts, two curve degrees, slot, automatic-side flag and sixteen point/UV pairs.
The first word of each `BlockSurface` stays unchanged.
Version 2 also stores boxes. Version 1 sets `num_boxes` to zero.

`leveleditor_load` and `leveleditor_save` use version 6 of `data/city.sbc`.
The file holds intersections, streets, the 1024 by 1024 tile grid and building entries, in that order.
Each building entry saves its name, position and turn count. Its `unsaved` field stays unchanged.
The file counts have no bounds checks. A mismatched version closes the file without loading its records.
Both loaders and the city writer store the file handle in `Parser.file` and keep its value after close.

## Mesh storage and turns

`area_setupsblock` first projects UV pairs for quads that have a known side.

| Side | UV pair |
| --- | --- |
| 0x10000 | z, y |
| 0x20000 | 1 - z, y |
| 0x30000 | x, z |
| 0x40000 | x, 1 - z |
| 0x50000 | 1 - x, y |
| 0x60000 | x, y |

Quads run in index order. When quads share a vertex, the last quad supplies its UV pair.
Each subtraction rounds once to float at the store.

The mesh buffers are replaced in this order: vertices, normals, tangents, UV pairs, faces, face slots, triangles and triangle slots.
Each replacement frees its previous pointer before it allocates the new buffer, even for a null pointer or a zero size.
Vertex data uses bit copies. UV pairs use eight bytes per vertex, while the temporary mesh uses a twelve-byte stride.
Face slots keep only their low sixteen bits. Triangle slots keep all bits.

`area_block_rotatesurface` turns all sixteen control points about y, including the inner points.
Each turn maps x to `1 - z` and z to the previous x.
Its side cycle is 0x60000, 0x10000, 0x50000, 0x20000, then 0x60000 again.

`area_copyrotateblock` swaps the x and z dimensions and rotates the temporary vertices, normals and tangents.
Horizontal quads cycle their four indexes. Vertical quads turn their side and cycle texture slots 1 through 4.
The function changes the temporary mesh and the target dimensions. It does not copy the mesh buffers into the block record.

## Boxes and beams

`area_block_addbox` creates all six cube faces, with four separate vertices per face.
Each component rounds the scaled cube coordinate to float, then rounds the sum with the position to float.
Normals and tangents come from the cube tables.

`area_block_addbeam` creates only the faces selected by its mask.
It writes four surface corners, rotates the surface and adds a bilinear surface for each selected face.
Its scaled coordinate and position sum stay in double until the final store to float.
The bottom face uses texture entry 5. The top face uses entry 0.

## Interior surfaces

`BuildBlockTypeRec` holds a name and eight `BuildPiece` records, with a total size of 0x1a8 bytes.
There are sixteen block types. Each piece stores kind, x, width, y, height, flags and index.
Flag bits 1, 2, 4 and 8 select the left frame, right frame, sill and lintel.
The complete table also has a byte view of 0x1a80 bytes.

The surface builders set only the four corners of the control grid.
Inner points keep their previous values and turn with each call to `area_block_rotatesurface`.
Each face restores its corners before the turn. The order of these turns matters even for bilinear surfaces.

Windows create two faces before any frame surfaces. Each window adds two to `num_hideable`.
`area_block_addwindow` returns the address of that counter, encoded as an integer.
Doors add six frame faces. Corners add two faces.
`area_block_addoutdoor` uses fixed post widths and ignores its second parameter.

Window endpoints round after x + width and y + height.
Frame endpoints round after width + x and height + y.
Door and outer-frame subtraction results round before they supply another builder.

## Interior cache

`BuildingType.interior_blocks` maps each low 22-bit code to a block id, with -1 for a missing entry.
The four low nibbles select wall pieces. Bits 16 and 17 select floor and ceiling.
Bits 18 through 21 select one of the sixteen block types.

| Wall code | Geometry |
| --- | --- |
| 1 through 8 | A piece from the selected block type |
| 9 | A plain wall face |
| 10 | A door frame |
| 11 | A corner |
| 12 | An outer frame |
| Other values | No wall geometry |

`area_block_createbuildinginterior` returns an existing cache entry before it checks the block limit.
A missing entry takes the next block id, unless the counter is above 65535. A full table returns zero.
The builder adds glass first, then floor, ceiling and the four wall frames.
`open_sides` records floor, ceiling and walls whose code is 9. Every generated interior has dimensions 1 by 1 by 1.

A nonzero scratch parameter skips the cache and uses block id 65535.
Its geometry stays in `TempMesh`. The block's mesh buffers keep their previous values.
After UV projection, every quad with a nonzero side value keeps only its low sixteen slot bits.
Quads without a side value keep their slot word.
