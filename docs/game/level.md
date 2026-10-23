# Level construction

The level combines a height map, a track, an area of blocks, city streets and building floorplans.
`createlevel` builds these systems in a fixed order. The editor supplies intersections, streets, tiles and building entries.

## Records

| Record | Size | Main fields |
| --- | --- | --- |
| TrackPiece | 0x578 | type, pos, num_verts, verts, num_tris, tris, center, across, box_min, box_max |
| CityIntersection | 0x68 | x/y/z, pos, roads[4], extent[4], light, light_max |
| CityRoad | 0x4c | ends[2], kind, width[2], dir, side, name[32] |
| CityBlock | 0x30 | x/z, size_x, height, size_z, inner_x, inner_z |
| LevelIntersection | 0x0c | x/y/z |
| LevelStreet | 0x34 | a/b, axis, lanes_a, lanes_b, name[32] |
| LevelBuilding | 0x54 | name[64], x/y/z, turns, unsaved[4] |
| MeshQuad / MeshTri | 0x10 / 0x0c | Four or three vertex indices |

`LevelEditor.tiles` holds 1024 by 1024 bytes. The low four bits select a material rule.
The high four bits select a block kind. The active tile loop covers x and z from 1 through 510.

`TempMesh` keeps separate arrays for positions, normals, tangents, UVs, quads and triangles.
Each UV occupies a vec3 slot, but surface construction writes only x and y.
The quad and triangle slot arrays keep texture and side flags.

`Building.build_size_x`, `build_size_z` and `build_floors` hold the dimensions prepared by `building_setupbuild`.
They describe x size, z size and floor count. There is no y-size field in this triple.

## Noise and terrain

`lerpnoise2d`, `cerpnoise2d` and `serpnoise2d` use the same integer hash at four grid corners.
They use linear, cosine and smooth-step interpolation. Coordinates truncate toward zero.
The mask applies to the next x and y indices. The current indices stay unmasked.
The integer hash converts directly to double for scaling. The unit result then rounds to float.

`createlandscape` writes 2049 by 2049 height samples, with a row stride of 2049.
It sets the x and z origin to -4096 and leaves the y origin unchanged.
Each grid step covers eight world units.

Each cell uses five `serpnoise2d` calls. Their coordinate factors, in call order, are 0.5, 1, 0.25, 0.125 and 0.0625.
A radial falloff uses the distance from grid point (640, 640). It clamps to the range 0 through 3.
The outer height term and the next term use the same float square of the falloff.

The blend band covers indices 497 through 783 on both axes.
It blends toward height 23.75. The inner square covers indices 513 through 767.
Its height is 16, with height 20 on its border.

## Curves and surfaces

`tesselatebezier` and `tesselatebezierderivative` support degrees 1, 2 and 3.
For another degree, the curve returns `1 - t`. The derivative returns the degree's bit pattern as a float.

`addbeziersurface` takes a 4 by 4 array of vec3 control points and a matching array of vec2 UVs.
It evaluates v first, then u. It appends vertices and quads at the current mesh counts.
The normal uses the v derivative crossed with the u derivative. The tangent follows the u derivative.
Both vectors normalize to positive zero when their length is zero.

`addbeziersurfacetexture` shares this construction. It takes UV x and y from the vertex's x and z coordinates.
It ignores the UV argument and keeps the UV curve buffer unchanged.
Both functions leave the third component of every output UV unchanged.

Zero steps still produce one row or column. Negative steps suppress that row or column loop.
The v curve buffers can still change when only the u loop is suppressed.
There is no mesh capacity check.

## Track

`createtrack` takes four corners. One edge joins corners 0 and 3. The other joins corners 1 and 2.
The control points use horizontal distances and normalized side vectors.
`rise_start` raises the near controls. `drop_end` lowers the far controls.

Each piece stores 32 vertices and 42 triangles. Its triangle array has space for 48 triangles.
The eight centers at offset 0x498 are the means of the paired edge vertices.
The eight across vectors at offset 0x4f8 point from the second edge toward the first edge.
The box starts with bounds of positive and negative 16777216, then uses strict comparisons over all stored vertices.

| Type | Rail behavior |
| --- | --- |
| 0 | Raise each edge by 0.5 |
| 1 | Move outward by two units and lower by 0.5 |
| Other | Repeat corner 2 for every rail vertex |

A piece with center x at least 2048 flattens a 7 by 7 patch of terrain.
The patch height is the piece's center y minus 0.25. Row and column 2048 are excluded from these writes.
Terrain coordinate conversion truncates toward zero and rejects negative indices with unsigned bounds checks.

`buildsomeshit` creates 145 pieces. It keeps the mean angle in double precision before sine and cosine.
Each trigonometric result and each point component rounds to float.
Earlier rounding of the mean angle changes the track layout.
Piece 74 sets the spawn position. Spawn sizes are 16, 4 and 16.

## City

`city_addintersection` appends a record. Its world position is `(x * 4, (y + 1) * 4, z * 4)`.
Road slots start at -1. Extents, light and light_max start at zero. The light timer stays unchanged.

`addcitystreet` appends a road, updates the connected road slots and widens the intersection extents.
Axis selection compares signed coordinate differences. A road toward negative x can therefore select kind 1, the z kind.
The road direction is normalized. Its side vector is `(-dir.z, 0, dir.x)`.
The name copies through its terminating zero without a length limit.

The road slots and extents use this order: positive x, positive z, negative x, negative z.
Roads along x widen the positive-z extent by width 1 and the negative-z extent by width 0.
Roads along z widen the positive-x extent by width 0 and the negative-x extent by width 1.

`buildcityinterection` fills the rectangle defined by the four extents, then creates its traffic lights.
A road id of -1 suppresses the corresponding light. Light placement uses extents 0 and 1 for all four corners.

`buildcitystreet` builds rows between the two intersections. A height change uses six ramp rows per block of height.
Rising ramps raise y before the first row of each group. Falling ramps lower y after the sixth row.
Material identifiers use their low 16 bits.

The caps overwrite the first and last street rows. They keep material slot 0 from the last row.
For a street with zero or negative length, all cap materials start at 3.
This gives a defined value to slots that have no row value.
Constructors do not clamp counts, widths or coordinates.

## Level order

1. Seed the random generator, clear level marks, generate terrain and create the track.
2. Create area 0 at the zero origin, with dimensions 64 by 8 by 64 and scale 4.
3. Reset city counts and car-spot counts for all eight teams. Add editor intersections and streets unless gametype is 1.
4. Fill ground in y/z/x order. Y covers 0 through 5. The z band from 136 through 151 stays open.
5. Build streets, intersections, the fixed platform and editor tiles. Gametype 1 builds a single floorplan and checks gametype again before returning.
6. Load editor buildings, apply positive turns, prepare floorplans, select building kinds and place them at y = 6.

Editor intersection y values are ignored in favor of 5. Editor building y values are ignored in favor of 6.
Positive building turns cause that many rotations. Zero or negative turns cause no rotation.
The team angle still uses the full signed turn count times pi/2, rounded to float.

| Tile kind | Upper block |
| --- | --- |
| 0 or 7 | None |
| 1 | Edge block selected from the four neighboring tile kinds |
| 2 through 5 | One of four slab turns |
| 6 | Normal block |
| 8 through 15 | Fence or fence-end turn |

Every nonzero tile first creates ground at y = 5. Low material bits 0 select all-zero materials.
Low bits 1 select material 3. Other low values select `MaterialList.tile_material`, truncated to 16 bits.
After an upper block is placed, bit 0x20 is cleared in the lower block if it is a normal block.

| Building name | Kind |
| --- | --- |
| base1 through base5 | 0 through 4 |
| dealership2 | 11 |
| gunshop | 14 |
| hospital0 | 15 |
| aoffice1 | 10 |
| conveniencestore1 | 13 |
| Other | 9 |

The base-name comparisons precede the team angle store. The shop-name comparisons follow that store.

## Implementation

- [level.c](../../src/game/level.c): terrain, track and level order.
- [city.c](../../src/game/city.c): intersections and streets.
- [mesh.c](../../src/game/mesh.c): curves and surfaces.
- [random.c](../../src/common/random.c): the noise functions.
- [object_types.c](../../src/game/object_types.c): point weights used by object type setup.
