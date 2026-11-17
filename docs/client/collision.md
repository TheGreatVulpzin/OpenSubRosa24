# Math, collision, areas and block meshes

These functions are shared with the server (see [shared code](shared.md)). No recorded session calls most of them. The synthetic calls of `instrumentation/client/synth_pure.py` test them. Each test case builds arguments from kinds of values.

## Vectors and random numbers

- Same on all synthetic calls: `angulardistance`, `circleintersecttriangle`, `geteulerangles2`, `lineintersectplanetwosided`, `orientationtoquaternion`, `pointdistanceline`, `projectvector`, `sphereintersectline`, `sphereintersecttriangle`, `sphereintersecttrianglefull`, `genrand_int32`, `init_by_array`, `init_genrand`, `getrandomvector3d`, `sprintf_comma`, `multiplyquaternion`, `multiply3x3matrices`, `projectorientation`, `rotateorientation`, `resetorientation`.
- Known differences: `lineintersectplane`, `lineintersectsphere`, `calculatebarycentric`, `rotatevector`, `lineintersecttriangle` and `lineintersecttriangletwosided` differ for NaN and infinite values only. With finite values all 90 calls are the same. `calculatefriction` differs when the tangent length is near 1.5e-5. `objecttype_calcquadweights` differs on 1 call in 90 by 1e-5 of a weight. The Bezier functions differ for a degree outside 1 to 3. No caller passes such a degree.
- `SYNTH_FINITE=1` keeps NaN, infinity and huge values out of the arguments.

## Text parser

`findstring`, `checkstring`, `getstring` and `cs_getline` (`src/common/parser.c`) read a settings text. The test puts a text in the parser record and calls each function from a set cursor. The keys include a missing key, an empty key and a key that is cut short. The parser reads from the global `open_file` in the client (`sr_parser_file`).

## Collision

Sixteen functions of `src/game/collide.c` and `collide_area.c` are shared: `sphereintersectlandscape`, `lineintersectlandscape`, `sphereintersectlevel`, `lineintersectlevel`, `lineintersectall`, `pointinsidebase`, `pointinsidebasesafe`, `lineintersectobject`, `lineintersecttrack`, `lineintersecttrackrail`, `item_sphereintersect`, `lineintersectcityobject`, `lineintersectcityobjecttype`, `lineintersectarea`, `lineintersectnormalblock` and `lineintersectspecblock`. `lineintersecthuman` is also shared.

The test uses these kinds of arguments: a point of the world (x and z from 0 to 2048, y from 0 to 80, every fifth call on the street grid), an end point within 40 units of the last point, a radius, a team, and an index of an object, a human or an item.

## Areas

Ten functions of `src/game/area.c` are shared: `area_getblock`, `area_marksector`, `area_setblock`, `area_setblockcolor`, `area_deleteblock`, `area_interior_setobject`, `area_getnextchunk`, `area_getnextsector`, `area_createchunk` and `area_createsector`. `area_createblock` is also shared. The test kinds are an area (0 to 3, or out of range), a block coordinate (edges of sectors and chunks, a random value, or -1) and a block word. Memory changes are part of the result.

## Block meshes

Ten functions of `src/game/block_mesh.c` build the surfaces of a block: `area_block_rotatesurface`, `area_copyrotateblock`, `area_block_addbasicsurface`, `area_block_addbeam`, `area_block_addwindow`, `area_block_adddoor`, `area_block_addoutdoor`, `area_block_addcorner`, `area_block_addwindowvertical` and `area_block_addwindowhorizontal`. `area_createinteriorblocks`, `area_block_addbox` and `loadblock` are also shared.

The Bezier code writes three scratch arrays (`bezier_curve`, `bezier_curve_uv`, `bezier_curve_dv`). They need client addresses (see [records](records.md)). Without them the shared code writes a private object and the client memory does not change.

## Level and city

`createlandscape`, `createtrack`, `buildsomeshit`, `buildcitystreet`, `buildcityinterection`, `city_createelevator` and `cityobject_addbox` build the level and the city. Their synthetic calls give the same result as the client. The test lists `buildsomeshit` as a known difference in the last bits of a float.
