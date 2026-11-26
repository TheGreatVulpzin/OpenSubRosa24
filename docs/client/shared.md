# Shared code in the client

The client runs functions of the shared game code (`src/game`, `src/common`, `src/net`). The server runs the same functions. A client function calls the shared version when its name is in an adoption list. Before a name goes in the list, the test must show the same result as the client.

## Where the client and the server differ

The server and the client compile the same source file for the shared code. When the two programs must run different code, the difference has one name in `src/include/sr_variant.h`. The first part of that file is the server, the second part the client. A shared file uses only the name: no shared file has its own `#ifndef` for a client hook. To add a difference, add the name to both parts with a comment that says what differs.

The data of each program is separate. The server data is in `src/server/storage.c`. The client data is in `src/client/storage.c` (from `analysis/client_win/*.csv`). A client name that is a field of a bigger record is a field in `analysis/structs/names.csv`, not a macro with an offset.

## The pieces

- `analysis/client_win/adopted*.txt`: one list for each group. A line has the function name and its source file. `adopted_pending.txt` holds the candidates that do not pass yet.
- `analysis/client_win/storage.csv`: the client address of each data object that shared code reads as `sr_data_<name>`. `tools/re/gen_client_storage.py` writes `instrumentation/client/runtime/storage_adapter.h` from it. An object with no address gets a private dummy, and a function that reaches a dummy does not work. `python -I tools/re/check_adopted.py --file <list>` finds those functions.
- `analysis/client_win/params.csv`: the parameters that are pointers. A pointer typed as a number makes the test skip the call.
- `src/include/sr_runtime.h`: the macros that the client adapter replaces (see [records](records.md)).
- Synthetic calls: `instrumentation/client/synth_pure.py` builds calls for the functions that no recorded session calls. `bash instrumentation/client/step.sh --synth <names>` runs them.

## When two versions differ

One shared function is better than two copies. The client may differ a little from the old client when the change touches none of these:

1. The network protocol (the bits of a packet).
2. The format of a file (settings, saves, levels).
3. The sync between a client and a server.

- An address, a size or an offset differs: use a macro or a client type. The function is shared.
- A formula or an error path differs, and none of the three points is touched: use the shared function. The test lists a known difference.
- A difference touches one of the three points: write a separate function in `src/client/`. Example: `pbitread` reads 32 bits at any bit position, and the server version loses bits. Example: `loadsaveoptions`, because the client has no `gametype=` key.

Known differences in the test:

- `playerteamkilltax`: the client takes a smaller share of the money.
- The plane, sphere, triangle, barycentric and rotation functions: NaN and infinite values.
- `calculatefriction` and `objecttype_calcquadweights`: the last bit of a float.
- The two Bezier functions: a degree outside 1 to 3.

## Adopted functions

Each function gives the same result as the client on its synthetic calls (20 to 90 for each) or on its recorded calls.

- `src/common/files.c`: `listfiles`, `removeflieextentions`
- `src/common/intersect.c`: `calculatebarycentric`, `circleintersecttriangle`, `lineintersectplane`, `lineintersectplanetwosided`, `lineintersectsphere`, `lineintersecttriangle`, `lineintersecttriangletwosided`, `pointdistanceline`, `sphereintersectline`, `sphereintersecttriangle`, `sphereintersecttrianglefull`
- `src/common/parser.c`: `checkstring`, `cs_getline`, `findstring`, `getstring`, `loadtextfile`
- `src/common/random.c`: `cerpnoise2d`, `genrand_int32`, `getrandomvector3d`, `init_by_array`, `init_genrand`, `lerpnoise2d`, `serpnoise2d`
- `src/common/system.c`: `debug_resetlogfile`, `getcpufrequency`, `gettimeofday`, `setcpuaffinity`
- `src/common/util.c`: `csradixsort`, `sprintf_comma`
- `src/common/vectors.c`: `addvectors`, `angulardistance`, `dotproduct`, `generatetrinormal`, `geteulerangles2`, `multiply3x3matrices`, `multiplyquaternion`, `normalizevector`, `orientationtoquaternion`, `projectorientation`, `projectvector`, `resetorientation`, `rotateorientation`, `rotatevector`, `scaleaddvectors`, `scalevector`, `subtractvectors`, `vectordistance`, `vectorlength`
- `src/game/account_db.c`: `account_nameexists`, `createaccount`, `saveaccountsclient`
- `src/game/ai_car.c`: `ai_car_nextwaypoint`, `ai_playercar`
- `src/game/area.c`: `area_createblock`, `area_createchunk`, `area_createsector`, `area_deleteblock`, `area_getblock`, `area_getnextchunk`, `area_getnextsector`, `area_interior_setobject`, `area_marksector`, `area_setblock`, `area_setblockcolor`
- `src/game/block_mesh.c`: `area_block_addbasicsurface`, `area_block_addbeam`, `area_block_addbox`, `area_block_addcorner`, `area_block_adddoor`, `area_block_addoutdoor`, `area_block_addwindow`, `area_block_addwindowhorizontal`, `area_block_addwindowvertical`, `area_block_createbuildinginterior`, `area_block_rotatesurface`, `area_copyrotateblock`, `area_createinteriorblocks`, `area_setupsblock`, `loadblock`
- `src/game/building_types.c`: `setupbuildingtypes`
- `src/game/bullet_sim.c`: `bulletsimulation`
- `src/game/bullets.c`: `createbullet`, `setupbullettypes`
- `src/game/city.c`: `buildcityinterection`, `buildcitystreet`
- `src/game/city_objects.c`: `city_createelevator`, `cityobject_addbox`, `setupcityobjecttypes`
- `src/game/collide.c`: `item_sphereintersect`, `lineintersectall`, `lineintersectcityobject`, `lineintersectcityobjecttype`, `lineintersecthuman`, `lineintersectlandscape`, `lineintersectlevel`, `lineintersectobject`, `lineintersecttrack`, `lineintersecttrackrail`, `pointinsidebase`, `pointinsidebasesafe`, `sphereintersectlandscape`, `sphereintersectlevel`
- `src/game/collide_area.c`: `lineintersectarea`, `lineintersectnormalblock`, `lineintersectspecblock`
- `src/game/economy.c`: `economy_carmarket`, `economy_gunmarket`, `economysimulation`, `player_calcassets`, `playerbuystock`, `playerdeathtax`, `playerteamkilltax`, `scenario_getnumofmissions`
- `src/game/human_arms.c`: `human_calcarmtorque`, `human_calculatearmangles`
- `src/game/human_body.c`: `createjointbond`, `human_buildmuscles`, `human_createmuscleball`, `setupbody`
- `src/game/human_pose.c`: `human_calculatelegangles`, `human_collisionhuman`
- `src/game/item_logic.c`: `itemlogic`, `itemweaponsimulation`
- `src/game/item_physics.c`: `item_physicssimulation`, `itemsimulation`
- `src/game/item_types.c`: `setupitemtypes`
- `src/game/items.c`: `createitem`, `item_removehumanlink`, `item_removeitemlink`, `linkitem`
- `src/game/level.c`: `buildsomeshit`, `createlandscape`, `createtrack`
- `src/game/logic.c`: `logic_gunstore_buy`, `logic_player`, `logic_suits_buy`
- `src/game/mesh.c`: `tesselatebezier`, `tesselatebezierderivative`
- `src/game/object_types.c`: `calculatepointscale`, `objecttype_addbond`, `objecttype_addface`, `objecttype_addparticle`, `objecttype_addwheel`, `objecttype_calcquadweights`, `setupobjecttypeweight`
- `src/game/objects.c`: `applydamageobject`
- `src/game/particles.c`: `createbond`, `createparticle`
- `src/game/player.c`: `createplayer`, `deleteplayer`, `player_saveinventory`, `playersimulation`
- `src/game/player_ai.c`: `playerai`
- `src/game/profiler.c`: `stoptimer`
- `src/game/scenario.c`: `scenario_armhuman`
- `src/game/solver.c`: `calculatefriction`
- `src/game/vehicle_types.c`: `setupobjecttypes_cars`, `setupobjecttypes_helicopters`, `setupobjectypes_train`
- `src/game/vehicles.c`: `addobjectrotation`, `getenginetorque2`, `object_rotorsimulation`, `objectenginesimulation2`, `objectsimulation_car`, `objectsimulation_heli`
- `src/game/wheels.c`: `objecttrainsimulation`, `objectwheelsimulation2`
- `src/net/events.c`: `createevent_bullethit`, `createevent_updateblock_quad`, `createevent_updateitem`, `createevent_updateiteminfo`, `createevent_updateobject`, `createevent_updateplayer`, `createevent_updateplayer_finance`, `eventsimulation`
- `src/net/master.c`: `getmasterserverip`
- `src/net/packet.c`: `cs_pwrite`, `pbitwrite`, `pbitwritenego`, `recvpacket`, `sendpacket`
- `src/net/server.c`: `calculateplayervoice`, `servercycle`

## Functions that are separate

| Function | Why |
| --- | --- |
| `pbitread`, `pbitreadnego` | The client reads 32 bits at any bit position and resets nothing on a short packet. |
| `loadsaveoptions` | The client option file has no `gametype=` key. |
| `createhuman`, `createarea` | The results differ. The cause is not found. |
| `building_load`, `building_buildfloorplan` | The client builds a building with no floor plan array. |
| `systemtimer` | The result depends on the clock. |
| `bullettimetolive` | The client function counts the time of the pieces of sprites, not of bullets. |

## Tests

- `python -I instrumentation/lift/gate.py --target client_win --changed --quick` runs the adopted functions whose shared code changed.
- A full test before a merge: `python -I instrumentation/lift/gate.py --target client_win --jobs 1`.
- The test uses 2.3 GB of memory for each run. Start one run for each 6 GB of free memory.
