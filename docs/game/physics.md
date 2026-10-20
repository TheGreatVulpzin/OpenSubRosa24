# Physics: particles, bonds and the solver

The bodies of the game are points with a mass (particles). Bonds link them. People, vehicles and objects are sets of particles, bones and rigid bodies that bonds hold together. Each tick, a solver changes the velocities until the bonds hold. Files: `src/game/particles.c`, `solver.c` and `bonds.c`.

## Records

| Record | Key fields | Use |
| --- | --- | --- |
| `Particle` (array `sr_particles`, 16384 records) | `type`, `time_to_live`, `mass`, `gravity`, `pos`, `pos_prev`, `vel`, `vel_prev` | One point. |
| | `num_bonds`, `bonds[31]` | The bonds of the particle. |
| | `num_objects`, `objects[6]` | The objects that own the particle. |
| `Bond` (array `sr_bonds`, 16384 records) | `type`, `subtype`, `time_to_live`, `mode` | One link. `type` selects the solver function. `mode` selects the passes. |
| | `num_particles`, `particle_a` to `particle_d`, `weights[3]` | The particles of the bond. Types 3 and 4 use a triangle of three particles. |
| | `rest_length`, `stiffness`, `damping` | The spring of a type 0 bond. |
| | `normal`, `friction`, `surface_vel` | The contact data of the bond types 1 to 4 and 7. |
| | `owner`, `target`, `bone_a`, `bone_b`, `arm`, `local_a`, `local_b` | The humans, bones and items of the body bonds. |
| | `roll_dir` | The rolling direction of a wheel bond (type 5). |
| `BondOpt` (array `sr_bondopt`) | `a`, `b`, `c`, `mode`, `dir`, `bias`, `arm_a`, `arm_b`, `share_a`, `share_b`, `damping` | The values that the setup step computes once for each bond. |
| `BondParticle`, `BondBone`, `BondHuman` | `vel`, `dv`, `w`, `dw`, `ang_momentum`, `axes` | The work copies of the velocities. |

The pools are packed. A removed particle or bond is replaced by the last one. Each list that holds the old index of the moved record gets the new index.

## Particle flow

### createparticle

1. Return -1 when 16384 particles exist.
2. Clear the record. Set the type, the position, the velocity and the mass.
3. Set `time_to_live` to 0x10000 (no limit) and the gravity to 0.0027222 per tick.
4. If an object is given, add it to the list of objects.

### particlesimulation (each tick)

For each particle:

1. Copy `pos` to `pos_prev`.
2. Add `vel` to `pos`.
3. Copy `vel` to `vel_prev`.
4. Subtract the gravity from `vel.y`.

### particletimetolive (each tick)

1. Subtract 1 from `time_to_live` if it is below 0x10000.
2. If the value is 1 or more, go to the next particle.
3. Delete all bonds of the particle.
4. Move the last particle into the slot.
5. Rename the moved particle in its bonds, in its objects and in the humans that name it.

## Bond flow

### createbond

`createbond` makes a type 0 bond (a spring between two particles). It returns -1 when:

- 16384 bonds exist.
- The two particles are the same or out of range.
- A particle has 31 bonds already, or the object has 127.
- A bond between the same two particles exists already.

Otherwise it sets the rest length to the distance of the two particles. It sets the stiffness and the damping to 0.25. It sets `mode` to 3, or to 0 for subtype 1.

### deletebond and bondtimetolive

`deletebond` removes the bond from the lists of its particles and objects. It moves the last bond into the slot. A joint bond (type 8) renames the bond index in the joints of its human. A muscle bond (type 9) renames it in the bones and the muscles. `bondtimetolive` counts down `time_to_live` and removes the bonds that reach 0.

## The solver: bondsimulation2

The solver runs once each tick. It has 5 steps:

1. **Setup.** For each bond with a mode below 0x100, it computes the `BondOpt` record. Type 0 (particle pair) gets the direction, the stretch and the mass shares. Type 8 (joint), type 9 (muscle) and type 10 (item in a hand) get the lever arms and the position error.
2. **Copy.** `setup_copy` copies the velocities of the particles, the bones of each active human, the items and the wheel bodies into the work arrays.
3. **Passes.** The solver runs 32 passes. Each pass does these steps:
   1. Clear all `dv` and `dw` values (`loop_clear`). Store the pass number for the wheels.
   2. Hold the balance of each upright human (`loop_balance`).
   3. Run the wheel simulation.
   4. Run each bond that takes part in the pass. A bond takes part when the pass number has all the bits of the mode of the bond. Mode 3 runs in passes 3, 7, 11 and so on. Mode 0 runs in every pass.
   5. Run the engine simulation.
   6. Add the changes to the velocities of the particles, the bones, the items and the wheel bodies.
4. **Copy back.** The work velocities go back to the particles and the bones.
5. **Restore.** The count of bonds goes back to the number of static bonds. The contact bonds of the tick end here.

### Bond types

| Type | Function | Meaning |
| --- | --- | --- |
| 0 | `bond_partpart` | A spring between two particles. |
| 1 | `contact_level` | A particle against a fixed surface. |
| 2 | `contact_particle` | A particle against a particle. |
| 3 | `contact_triangle` | A particle against a triangle of another object. |
| 4 | `contact_bone_triangle` | A bone of a human against a triangle of an object. |
| 5 | `contact_wheel` | A wheel on the ground. |
| 7 | `contact_bone_level` | A bone against a fixed surface. |
| 8 | `bond_bodybody` | A joint between two bones of a human. |
| 9 | `bond_bodyrotbody` | A muscle: it holds the turn between two bones. |
| 10 | `bond_bodyitem` | A bone holds an item at a point. |
| 11 | `bond_itemrot` | The item in a hand turns with the bone. |
| 13 | `contact_bone_bone` | A bone of one human against a bone of another human. |
| 14 | `contact_bone_item` | A bone against an item. |
| 15 | `contact_wheelbody` | A wheel body against a fixed surface. |

### How a bond changes velocities

Each function finds the relative velocity at the bond. It then adds a change to `dv` of each side. The change of each side uses a share of the impulse, from the masses. A body bond also adds a change to `dw`, with the cross product of the lever arm. A contact bond keeps the friction. `calculatefriction` splits a velocity into the part along the normal and the part across it. It limits the part across to the normal speed times the friction. It returns the speed that friction removed, scaled by 60 times 8.

## Functions

| Function | RVA | File | Purpose |
| --- | --- | --- | --- |
| `createparticle` | 0x278c0 | particles.c | Add a particle. |
| `particlesimulation` | 0x27ba0 | particles.c | Move all particles one tick. |
| `particletimetolive` | 0x279b0 | particles.c | Delete particles at the end of their life. |
| `createbond` | 0x129b0 | particles.c | Add a spring bond. |
| `deletebond` | 0x33a0 | particles.c | Remove a bond. |
| `bondtimetolive` | 0x12c90 | particles.c | Delete bonds at the end of their life. |
| `bond_partpart` | 0x12f80 | solver.c | Solve a particle pair bond. |
| `bond_bodybody` | 0x130d0 | solver.c | Solve a joint between two bones. |
| `calculatefriction` | 0x27e80 | solver.c | Limit the sliding speed with friction. |
| `bondsimulation2` | 0x41de0 | bonds.c | The solver for one tick. |

The type functions of the table above (`contact_*`, `bond_bodyrotbody`, `bond_bodyitem`, `bond_itemrot`) and the helpers of the steps (`setup_*`, `loop_clear`, `loop_balance`, `apply_*`, `copy_back`) are `static` in `bonds.c`. They were part of `bondsimulation2` in the game.

## Known odd behavior

- A particle or a bond that moves into a freed slot gets the life test again. It loses one more tick.
- A bond of type 0 keeps its object list in the field `owner`. The other types keep their owner there. A helper (`bond_objects`) names this use.
- The setup step skips the bonds with a mode of 0x100 or more.
- `loop_balance` adds three turns to bone 0 of an upright human: to the vertical, to the stance and to the body strength. This holds the person up on the ground.
- A small integer in a float field is a denormal on the x87. It makes the code slow.
- `contact_wheel` writes the friction into `Wheel.slip`. The wheel code reads it in the next pass.
- The wheel and engine steps still run as lifted C (`objectwheelsimulation2`, `objectenginesimulation2`).
