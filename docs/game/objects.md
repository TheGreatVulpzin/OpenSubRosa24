# Objects and vehicles

An object is a vehicle: a car, a helicopter, a locomotive or a train car. It is a cloud of particles with a frame (`rot`), wheels and a mesh of faces. Files: `src/game/objects.c`, `vehicles.c`, `wheels.c` and `objects.h`. This page covers the simulation of the objects (part A of group 4). The creation of objects and the object types (`createobject`, `setupobjecttypes_*`) are part B.

## Records

| Record | Key fields | Use |
| --- | --- | --- |
| `Object` (array `sr_objects`, 1024 records, 0x8230 bytes) | `active`, `type`, `state`, `health`, `time_to_live`, `key_away` | One object. `state` is 1 for a car, 2 for a helicopter, 3 for a train car. A locomotive has type 12 and state 0. |
| | `pos`, `pos_prev`, `vel`, `vel_prev`, `rot[3]`, `box_min`, `box_max` | Made by `objectsimulation` from the particles each tick. |
| | `num_particles`, `particles[64]` | Indexes into `sr_particles`. The first `num_frame_particles` give the frame. |
| | `frame_weight_x[64]`, `frame_weight_z[64]` | The weights of the particles in `rot[0]` and `rot[2]`. |
| | `verts[256]` | The vertices of the mesh: the particles, then points made of four particles. |
| | `drive_input[4]`, `drive_buttons` | The input of the driver (`humansimulation` copies it). |
| | `engine_rpm`, `engine_accel`, `throttle`, `engine_power`, `engine_inertia`, `engine_scale`, `clutch`, `brake` | The engine. |
| | `gear`, `gear_prev`, `gear_auto`, `gear_ratio`, `gear_ratio_inv`, `rpm_net` | The gearbox. `rpm_net` is the value that the packets send. |
| | `num_driven`, `driven_wheels[16]`, `differential` | The wheels that the engine drives. |
| | `num_wheels`, `wheels[32]` | Wheels on springs. |
| | `num_wheel_bodies`, `wheel_bodies[16]` | Solid wheels of type 1 cars. |
| | `rotor[6]` | The six points of a helicopter rotor. |
| | `track_target`, `stop_ticks`, `track_last`, `linked_object` | Train state. |
| | `axle_width`, `wheelbase` | The steering geometry. |
| | `crash_noise` | The sum of the impact noise. |
| `Wheel` (0x98 bytes) | `particle_a`, `particle_b`, `weight_a`, `weight_b` | The hub hangs between two particles of the object. |
| | `pos`, `vel`, `dv`, `radius`, `mass`, `inv_inertia` | The hub. `vel` is the hub velocity at the start of the pass. `dv` is the change of the pass. |
| | `ride_height`, `spring`, `damper`, `vertical` | The suspension. |
| | `share_wheel`, `share_body`, `spring_offset` | Made in solver pass 0 and read in the other passes. |
| | `spin`, `dspin`, `angle`, `steer`, `slip`, `travel`, `popped` | Rolling state. `travel` is the offset of the hub along `rot[1]`. |
| `WheelBody` (0xe8 bytes) | `pos`, `vel`, `dv`, `rot[3]`, `ang_momentum`, `w`, `dw`, `inv_inertia` | A solid wheel with an angular velocity. |
| | `particles[4]`, `weights[4]`, `mass`, `radius`, `half_width`, `axle_inv_inertia` | The hub points and the tire. |
| `RotorPoint` (0x24 bytes) | `pos`, `vel`, `vel_prev` | One point of the rotor model. |
| `TrackPiece` (0x578 bytes) | `box_min`, `box_max`, `tris` | A piece of the rails with its box. |

## The tick

`objectsimulation` (0x56710) runs once for each tick. It has three loops over the 1024 objects.

Loop 1, for each active object (`object_update`):

1. `object_gather`: sums the particles into `pos` and `vel`, makes the box (the wheels widen it by their radius) and the impact. The impact is the mean of the change of the particle speed since the last tick.
2. `object_frame`: makes `rot[0]` and `rot[2]` from the weighted offsets of the first particles to their center. Then it makes `rot[1]` by a cross product and orthogonalizes `rot[0]`.
3. An object that falls below y = -32 loses all health.
4. `object_vertices`: copies the particles to `verts` and builds the extra points of the type (`ObjectType.quads`).
5. `object_impact` (not for type 13): an impact above 0.0833 calls `applydamageobject`. The impact noise adds up in `crash_noise` and plays the crash sound above 2.
6. `object_drag` (not for helicopters): the air drag along the three axes of the frame.

Loop 2, for each active object (`object_move`):

1. State 1 runs `objectsimulation_car`. State 2 runs `objectsimulation_heli`.
2. Type 12 runs `locomotive`: it sets `g_train_near` near the crossing, opens the door face while the train moves, pulls the particles toward the station track piece and stops for 0x4b0 ticks at the station.
3. Wheel bodies move and turn, then `objectwheelcollision2` makes their bonds.
4. Wheels move with `dv`, get the gravity, and `objectwheelcollision` makes their bonds.
5. State 3 runs `objecttrainsimulation`.
6. Other states test each particle against the level (`level_contacts`) and make contact bonds. If no particle hits the level on the line from `pos_prev`, `pos_prev` becomes `pos`.

Loop 3 (`object_contacts`): each particle of an object (not state 3) is tested against the faces of each other object whose box overlaps. A hit makes a bond of type 3. An object does not touch its linked object. A type 9 object does not touch a type 10 object.

## Contact bonds

`contact_bond` (in `objects.h`) writes a bond of type 1 for a particle: the stretch is at most 0.25, the mode is 0. The caller sets the friction and the surface velocity.

| Source | Bond type | Stiffness | Damping | Friction |
| --- | --- | --- | --- | --- |
| level, line from the center | 1 | 0.125 | 0.25 | 0.6 |
| level, line from the last center, or a broken block | 1 | 0.0078125 (broken block) | 0.03125 (broken block) | 0.6 |
| track piece | 1 | 0.015625 | 0.25 | 0 |
| rail | 1 | 0.00390625 | 0.125 | 0 |
| wheel | 5 | 0.0625 | 0.25 | the material friction, 0.3 for a popped tire |
| wheel body | 15 | 0.03125 | 0.0625 | the material friction |
| another object | 3 | 0.0625 | 0.25 | 0.4 |

A particle that touches the level deep (its line from the center crosses the level) tests a second line from half a unit above. It takes the nearer normal. If the block breaks (`area_breakblock`), the bond is soft.

## The vehicle code

- `objectsimulation_car` (0x272b0): the throttle follows the pedal. The shifter and the pedals choose the gear (automatic gears when `drive_input[0]` is 1.5 or more and `drive_input[2]` is below -0.5). A gear change plays the gear sound. The steering angles use Ackermann geometry from `axle_width` and `wheelbase`. `rpm_net` is the rpm in units of the packets, at least 100, and 0 when nobody drives.
- `objectenginesimulation2` (0x324c0): runs 32 times for each tick. The engine adds `getenginetorque2` and removes the drag of the rpm. For type 1 cars the torque turns the wheel bodies. For other cars it turns the spins of the driven wheels through a differential. The brake acts on the spin of each wheel.
- `getenginetorque2` (0x23fa0): the torque curve of the rpm. It is 0 above 7000 rpm. Type 7 has another curve.
- `objectwheelsimulation2` (0x22f90): runs 32 times for each tick. A wheel hub is a spring and a damper between two particles. Solver pass 0 stores the mass shares and the spring offset in the wheel. The other passes read them. A wheel body gets an angular push and a spring to its four hub particles. `getobjectrotation3` (0x225e0) gives the angular velocity of the object from its eight corner particles.
- `objectsimulation_heli` (0x24f60): the rotor speed follows the driver (`rpm_net` rises by 10 to 2000). With a driver and health, the 20 first particles get air drag, the particles 21 and up get the blade lift, and `addobjectrotation` (0x21e00) turns all the particles around `pos` with the stick input.
- `object_rotorsimulation` (0x240e0): a spring model of six rotor points. Nothing calls it.
- `objecttrainsimulation` (0x26f90): the four first particles of a train car test the track pieces whose box overlaps the car. A hit makes a bond and stores the track piece in `track_last` of the car and its linked car.
- `applydamageobject` (0x22e20): lowers the health. At no health two particles get a random push along `rot[1]` and an update event goes out. A locomotive takes no damage.
- `objecttimetolive` (0x32340): counts `time_to_live` down. At 0 the particles lose their life, the humans in the object leave it, the object becomes free and an update event goes out.

## Odd behavior that the code keeps

- The first loop sums the impact with the rounding of a float at each step. A change of the order changes the damage by a bit.
- `objectwheelcollision` tests the circle with the axle `rot[0]`, not the steered axle. The steered axle only gives the rolling direction.
- `travel` of a wheel body goes into `wheels[k]`, the wheel with the same index.
- The wheel bodies exist only for type 1 cars. No recording has one. `instrumentation/lift/synth_vehicles.py` builds the situation.
- `object_rotorsimulation` has no caller. It is dead code in the game too.
- The helicopter forces need a driver and health above 0. A helicopter without them only turns its rotor.
- `g_train_near` is set by a locomotive that moves toward the crossing within 160 units. `logicsimulation` reads it.
- The exact test: the harness compares floats with a tolerance of 4 ulps. Put `verify_float_ulps=0` under `[hooks]` in `build/hook/subvulpes.ini` for a bit exact compare.

## Speed

`objectwheelsimulation2` and `objectenginesimulation2` run 32 times for each tick. The loops are flat: they use pointers to the object, the wheel and the particle. Bench (`instrumentation/lift/bench.py`, 3 runs, 2026-10-21): `objectsimulation`, `objectwheelsimulation2` and `objectenginesimulation2` are within the noise of the old code (ratio 0.93 to 1.09). `objecttrainsimulation` is 1.07 to 1.19 slower on 7 calls (a train exists only in some recordings). `objectwheelcollision` moves between 1.04 and 1.38 on 4 calls.

## Object types and creation (group 4 part B)

Files: `object_types.c` (the append helpers, `objecttype_calcquadweights`, `setupobjecttypeweight`), `vehicle_types.c` (the three setups), `object_create.c` (`createobject`, `createvehicle`), `ai_car.c` (the bot driver).

**Types.** `ObjectType` (128 records of 0x828c bytes) holds the lists of a vehicle: `particles` (TypeParticle), `bonds` (TypeBond), `faces` (TypeFace), `quads` (TypeQuad, train only), `wheels` (TypeWheel), `seats`. Every `objecttype_add*` helper writes at the count and adds 1. No setup resets a count. A second run of a setup appends all records again, except the bonds (`objecttype_addbond` skips a pair that exists). The recordings take the snapshot after `setupgame`, so the gate always tests this second run. `instrumentation/lift/synth_types.py` (`fresh_*` cases) clears the types and tests the first run.

**Types in use.** 0 Town Car, 1 Town Car 2 (wheel bodies, a hitch particle 20), 2 Metro, 3 Limo, 4 Turbo, 5 Turbo S, 6 Beamer, 7 Van, 8 Minivan, 9 Truck (hitch particle 20), 10 Trailer (hitch particle 16, state 0, no engine), 11 Helicopter, 12 Train, 13 Bogie. A car is a grid of 16 points (vans, truck and trailer: 12) in 3 (2) box segments, a roof, a cabin box with window faces, and 4 wheels (the truck has 6). The setup order is 0, 1, 2, 3, 4, 5, 7, 8, 6, 9, 10. `setupobjecttypeweight` writes the global `pointtemp`, so the order is part of the result.

**setupobjecttypeweight.** It makes the masses a share of 1, moves the wheels to the center of the particles, sets `radius` (1.125 times the largest distance from the frame center) and makes the two frame weight tables with `calculatepointscale`.

**createobject.** It takes the first free slot (no free slot gives -1). It copies the type: one particle for each type particle (turned by `rot`, moved to `pos`), one bond for each type bond, the faces (the object face has armor and the window flag 4 bytes lower than the type face, and `health` 8), the frame weights, the wheels and the seats. A type 1 car gets one `WheelBody` for each wheel. A type 11 helicopter gets the six `rotor` points. The create event (type 3) goes out only on a server.

**createvehicle.** Only type 0 (the train): a locomotive (type 12, `track_target` 0x4a) and two bogies (type 13, `linked_object` = the locomotive) tied to it with bonds. `rot` is overwritten with the frame of the locomotive. Another type gives -1.

**AI driving.** `ai_playercar(player, object)` follows `Player.waypoints` (road, dir, lane, turn). It steers toward a point on the lane line, takes the speed from the distance to the next crossing (`CityIntersection.red` stops it) and to the car ahead, changes lane (chase bots only), and backs out when `stuck_ticks` reaches 0xb4. The result goes to `input_axes`. `ai_car_nextwaypoint` picks the side to leave a crossing by (toward `ai_dest_x`, `ai_dest_z`, with random picks) and consumes `os_rand` calls in a fixed order.

Odd behavior that the code keeps:

- `objecttype_addbond` returns the byte offset of the type record (`type * 0x828c`), not a bond index.
- The bogie has bonds of a particle with itself (6 of 21).
- The Metro roof has armor 0.
- `WheelBody.axle_inv_inertia` is written with `r * r * 0.5`, which is an inertia, not an inverse. The name stays.
- The `* 0.0` in the wheel body weights stays: it turns a weight into 0 and -0.
- The helicopter centroid sums over `num_frame_particles`, which is set to `num_particles` at that point. A second run of the setup therefore includes the points of the first run.
- Train points (quads) share the index space with the particles: a quad index starts at `num_particles`.
- `ai_playercar` steering clamps leave a NaN as it is. The lane change reads the current waypoint, the turn reads the next one.
- `ai_car_nextwaypoint` tries at most 512 random sides. After that it uses the last side with no test, even if it has no road.
