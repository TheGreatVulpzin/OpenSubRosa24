# Collision

The collision code tests lines, spheres and wheel circles against the world. The world has the landscape, 4 areas of blocks (buildings and streets), the city objects, the track pieces, the objects (vehicles), the humans and the items. Each test keeps the nearest hit and returns 1 on a hit. Files: `src/game/collide.c`, `collide_area.c`, `area.c` and `rigid.c`. The basic tests of lines, spheres and triangles are in [math.md](math.md). The physics code that uses the hits is in [physics.md](physics.md).

## Records

| Record | Key fields | Use |
| --- | --- | --- |
| `Intersect` (global `g_intersect`) | `pos[3]`, `normal[3]` | The hit point and the hit normal. |
| | `fraction`, `distance` | The part of the line before the hit (1.0 for no hit). The distance of a sphere hit (65536.0 for no hit). |
| | `surface_vel` | The velocity of the hit surface. |
| | `face`, `tri_particles[3]`, `tri_weights[3]` | The face of an object, and the 3 particles of the hit triangle with the weights of the hit point. A hit on a human puts the bone in `tri_particles[0]`. |
| | `block_area`, `block[3]`, `block_word`, `block_face`, `spec_face` | The hit block of an area: its area, its cell, its word and its face. |
| | `material`, `area_material` | The material of a wheel hit. |
| | `index` | The city object of a hit, or -1. |
| | `face_u`, `face_v` | The hit point on a face with a grid. |
| `Area` (array `g_areas`, 4 records) | `active`, `origin`, `axes[3]`, `size_x/y/z`, `block_size`, `box_min`, `box_max`, `chunks[512]` | A grid of blocks with its own frame. |
| `AreaBlock` (global `g_blocktypes`) | `verts`, `faces`, `num_faces`, `tris`, `num_tris`, `num_hideable` | The mesh of a special block type. |
| `CityObjectRec` (array `g_cityobjects`, 1024 records) | `active`, `type`, `pos`, `vel`, `rot[3]` | A placed city object. |
| `CityObjectTypeRec` | `radius`, `verts[256]`, `faces[256]`, `block_offset` | The mesh of a city object type. |
| landscape (globals) | `g_land_origin`, `g_land_height` | A grid of 2049 by 2049 heights, 8 units apart. Each cell is two triangles. |

## How the areas store blocks

An area is a grid of chunks. A chunk holds 8 by 8 by 8 sectors. A sector holds 8 by 8 by 8 blocks. Only the used chunks and sectors have a record. The area has 512 chunks and the game has 0x2000 sectors.

- `area_blocksector` finds the sector of a block and the index of the block in it. It returns -1 for a block outside the area.
- `area_getblock` and `area_interior_getblock` return the word of an outer block and of an interior block. They return 0 for no block.
- `area_getblocktexture` and `area_interior_getblocktexture` return the texture of a face, or -1.
- `area_interior_getblocklink` follows a link to the block it points to.
- `area_getnextchunk` and `area_getnextsector` return the first free chunk or sector, or -1.

### The block word

| Bits | Meaning |
| --- | --- |
| 29 to 31 | The kind: 0x40000000 normal (a cube), 0x80000000 special (a mesh), 0xc0000000 link. |
| 0 to 11 | Normal block: for face f (0 to 5), bits f and f + 6. A face counts when both are set. |
| 0 to 15 | Special block: the block type. |
| 16 to 25 | Special block: the number of first faces that are hidden. |
| link | The link points to the block at x - dx, y - dy, z - dz. The bytes of the word give dx, dz and dy. |

## Test flow

### Level tests (collide.c)

Each level test runs the landscape first. Then it runs the 4 areas, then the city objects.

1. `sphereintersectlevel` and `circleintersectlevel` set `distance` to 65536.0. They keep the hit with the smallest distance.
2. `lineintersectlevel` sets `fraction` to 1.0. It keeps the hit with the smallest fraction. It can include the city objects. Their velocity goes into `surface_vel`.
3. `lineintersectall` tests the areas and the city objects. It does not test the landscape. `index` gets the city object of the hit.

### Areas (collide_area.c)

Each area test moves the point or the line into the frame of the area. Then it tests the blocks. A hit goes back to the world frame.

1. `lineintersectarea` walks the cells from the start of the line to its end (at most 0x4000 cells). It stops at the first hit. For each cell it tests the city object, then the outer block, then the interior block.
2. `sphereintersectarea` and `circleintersectarea` skip an area when the sphere is outside the box of the area. For each block, the kind of the word selects the test (`area_block_test`):
   - Normal: `sphereintersectnormalblock` or `circleintersectnormalblock`.
   - Special: `sphereintersectspecblock` or `circleintersectspecblock`.
   - Link: the test goes to the special block that the link points to.
3. A sphere against a quad (`sphere_quad`) tests both triangles and then the 4 edges. The test skips a quad when the center is not in front of it.
4. The wheel circle test keeps the axis in the world frame. The hit gets the texture of its face in `area_material`.

### Objects, humans and items (collide.c)

- `lineintersectobject` tests the faces of an object from both sides. A face with no health left does not count. With wheels, it also tests the wheels. `surface_vel` gets the velocity of the hit point.
- `sphereintersectobject` tests a sphere against the faces. A quad is two triangles. An object of type 12 skips the faces with no health left.
- `lineintersecthuman` tests a sphere around each of the 15 bones. The sphere gets larger by the radius `r`. The nearest hit goes into the record, with the bone in `tri_particles[0]`.
- `item_sphereintersect` tests a sphere against the box of an item.
- `lineintersectcityobject`, `lineintersectcityobjecttype` and `sphereintersectcityobjecttype` test the mesh of a city object type. The mesh is turned and placed at the object position.

### Landscape and track

- `lineintersectlandscape` walks over the cells that the line crosses. At most 0x4000 cells. It tests two triangles in each cell.
- `sphereintersectlandscape` and `circleintersectlandscape` test the cells under the sphere or the wheel.
- `lineintersecttrack` tests the ground triangles of a track piece. `lineintersecttrackrail` tests the rail triangles (triangle 14 on). A line test needs the start to be within 64 units of the piece center on each axis.

### Places

`pointinsidebase`, `pointinsidebasesafe` and `pointinsidebuilding` test if a position is in a base, in its safe zone or in a building box.

## Rigid body helper

`getobjectrotation2` finds the angular velocity of an object around a center. For each axis it adds, for all particles, the velocity along (axis x r). Here r is the position of the particle from the center. The sum is scaled by 1/128.

## Functions

| Function | RVA | File | Purpose |
| --- | --- | --- | --- |
| `sphereintersectlevel` | 0x4d750 | collide.c | Sphere against the level. |
| `lineintersectlevel` | 0x31630 | collide.c | Line against the level. |
| `lineintersectall` | 0x31480 | collide.c | Line against the areas and city objects. |
| `circleintersectlevel` | 0x4d630 | collide.c | Wheel circle against the level. |
| `sphereintersectobject` | 0x32bb0 | collide.c | Sphere against the faces of an object. |
| `lineintersectobject` | 0x257a0 | collide.c | Line against the faces of an object. |
| `sphereintersectlandscape` | 0x1cdf0 | collide.c | Sphere against the landscape. |
| `lineintersectlandscape` | 0x1c1d0 | collide.c | Line against the landscape. |
| `circleintersectlandscape` | 0x1c8f0 | collide.c | Wheel circle against the landscape. |
| `lineintersecttrackrail` | 0x26db0 | collide.c | Line against the rails of a track piece. |
| `lineintersecttrack` | 0x26bc0 | collide.c | Line against the ground of a track piece. |
| `sphereintersectcityobjecttype` | 0x2dfc0 | collide.c | Sphere against a city object mesh. |
| `lineintersectcityobject` | 0x146a0 | collide.c | Line against a city object. |
| `lineintersectcityobjecttype` | 0x14dd0 | collide.c | Line against a city object mesh in a block. |
| `pointinsidebase` | 0x89c0 | collide.c | Position in a team base. |
| `pointinsidebasesafe` | 0x8b00 | collide.c | Position in a safe zone. |
| `pointinsidebuilding` | 0x8a60 | collide.c | Position in a building. |
| `lineintersecthuman` | 0x2f130 | collide.c | Line against the bones of a human. |
| `item_sphereintersect` | 0x30f00 | collide.c | Sphere against the box of an item. |
| `lineintersectnormalblock` | 0x12450 | collide_area.c | Line against a cube block. |
| `lineintersectspecblock` | 0x126b0 | collide_area.c | Line against a mesh block. |
| `lineintersectarea` | 0x2c0c0 | collide_area.c | Line against an area. |
| `sphereintersectnormalblock` | 0x2cda0 | collide_area.c | Sphere against a cube block. |
| `sphereintersectspecblock` | 0x2d2d0 | collide_area.c | Sphere against a mesh block. |
| `sphereintersectarea` | 0x40aa0 | collide_area.c | Sphere against an area. |
| `circleintersectnormalblock` | 0x2d7f0 | collide_area.c | Wheel circle against a cube block. |
| `circleintersectspecblock` | 0x2daf0 | collide_area.c | Wheel circle against a mesh block. |
| `circleintersectarea` | 0x41550 | collide_area.c | Wheel circle against an area. |
| `area_getblock` | 0x1850 | area.c | Outer block word. |
| `area_getblocktexture` | 0x1bc0 | area.c | Texture of an outer block face. |
| `area_interior_getblock` | 0x3030 | area.c | Interior block word. |
| `area_interior_getblocktexture` | 0x3290 | area.c | Texture of an interior block face. |
| `area_interior_getblocklink` | 0x3130 | area.c | Interior block word after a link. |
| `area_getnextchunk` | 0x1e20 | area.c | First free chunk. |
| `area_getnextsector` | 0x1cd0 | area.c | First free sector. |
| `getobjectrotation2` | 0x223d0 | rigid.c | Angular velocity of an object. |

The other functions in these files are `static` helpers. `area_blocksector` is a helper that the three `area_*` getters share.

## Known odd behavior

- A city object in a block stops the sphere test at once. Its hit stays in the frame of the area, and `Intersect.block[0]` gets -1.
- The landscape line walk always uses a step of 0 for y.
- A triangle face with a grid takes the fourth corner of the quad before it in the same call. It takes 0 before any quad.
- A quad tests its second triangle with the normal of the first.
- An edge hit of a quad changes the normal for the tests after it.
- The interior blocks of the circle test have no normal blocks.
- A face of a grid has a hit point in 8 steps along the first edge and 4 steps along the second edge.
- In a cube block, the first triangle that the line hits stops the test.
- The hit values are copied bit for bit. A float load would change a NaN.
