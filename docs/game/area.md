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
