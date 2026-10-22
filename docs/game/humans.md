# Humans

A human is a body of 15 bones. The bones are rigid bodies. Joints and muscles (bonds) hold them together. A human can walk on the ground, sit in a vehicle, hold items and die. A player controls one human. A human with no player is a bystander. Files: `src/game/humans.c`, `human_body.c`, `human_pose.c` and `human_arms.c`. The bond solver that moves the bones is in [physics.md](physics.md).

## Records

| Record | Key fields | Use |
| --- | --- | --- |
| `Human` (array `sr_humans`, 512 records) | `active`, `has_physics`, `player_id`, `account` | One person. `has_physics` is 0 while the human sits in a vehicle with no item. |
| | `health`, `bleeding`, `body_strength`, `stun_timer`, `protect_timer`, `despawn_timer` | Health and timers. |
| | `vehicle_id`, `vehicle_seat`, `last_vehicle`, `last_vehicle_timer`, `exit_timer` | Vehicle state. |
| | `input_axes[4]`, `input_buttons`, `input_buttons_prev`, `view_axis`, `view_pitch`, `move_mode` | The input of the player. |
| | `stance`, `walk_phase`, `on_ground`, `axis`, `pitch` | Standing and walking state. |
| | `bones[15]` | The bones. |
| | `muscles[128]`, `num_muscles` | The muscle bonds (stride 0x30). |
| | `joints[32]`, `num_joints` | The joint bonds (stride 0xc). |
| | `inventory[8]` | The inventory slots. See [items.md](items.md). |
| | `colors[4]` | The colors of the clothes. |
| `Bone` (0xe0 bytes) | `pos`, `pos_prev`, `vel`, `rot[3]`, `ang_momentum`, `w`, `radius`, `half_size`, `mass`, `inertia`, `inv_inertia` | The rigid body of a bone. |
| | `target_rot[3]`, `joint_angles`, `bond`, `in_solver` | The pose that the muscle holds. `in_solver` is 1 when the bone takes part in the solver. |
| `Body` (global `g_body`) | parent bone and offset of each bone | The bone tree. |

### The bones

| Bone | Part |
| --- | --- |
| 0 | Hip. The root of the tree. |
| 1 | Chest. |
| 2 | Head. |
| 3, 4, 5 | Arm on the negative x side: shoulder, elbow and hand. Bone 5 is the left hand. |
| 6, 7, 8 | Arm on the positive x side: shoulder, elbow and hand. Bone 8 is the right hand. |
| 9, 10, 11 | Leg on the negative x side: thigh, lower leg and foot. |
| 12, 13, 14 | Leg on the positive x side: thigh, lower leg and foot. |

The bone tree (`body_table` in `human_body.c`) gives the parent of each bone. The hip, the chest and the head hang in a chain. The legs hang from the hip. The arms hang from the chest.

## Flow of one tick

`humansimulation` runs once each tick. First it sets the drive buttons of all 1024 objects to 4. Then it does these steps for each active human:

1. `human_begin`: lower the timers and set the stance, the view angles and the strength. Move the bones one step. Return 1 when an item is in slot 0 or 1.
2. If the human is on foot, run `human_onfoot`. Otherwise run `human_invehicle`.
3. `human_calculatelegangles`: set the angles that the leg muscles hold.
4. `human_calculatearmangles`: set the angles of the chest, the head and the arms. Set the bonds of the items in the hands.
5. `human_finish`: the end of the tick.

### Step 1: human_begin

- The stance moves toward 1 by 1/64 per tick. It moves toward 0.625 while the crouch button (8) is down.
- `human_strength` sets the strength of the muscles. Below 25 health, the strength drops and wobbles. Below 10 it drops more. The wobble phase differs for each human.
- `human_movebones` moves each bone. A dead bone loses speed (factor 0.99609375). The bone moves by `vel` and falls by gravity. It turns by `w`. Bone 0 does not go through the level between two ticks.

### Step 2: on foot

1. A living human runs `human_stand`. It finds the floor under the feet (`human_findfloor`). It pushes bone 0 up to the stance height. The push is at most 1/18 per tick. Button 4 makes the human jump.
2. `human_levelcontacts` adds a type 7 contact bond for each bone that touches the level. Bone 0 has radius 0.25. The hands and feet have 0.0625. The other bones have 0.125. A fast hit of bone 0 costs 120 health.
3. A standing human runs `human_walk`. Bone 0 moves toward the floor velocity plus the input direction. The speed is 0.15. It is 0.05 and 0.025 in move modes 1 and 2. Crouching is slower.
4. `human_objectcontacts` adds a type 4 contact bond for each bone that touches a triangle of an object. The bonds against the vehicle that the human just left start soft. A fast hit costs 120 health. When it kills, the hit sound plays.
5. `human_entervehicle`: button 0x800 puts the human in the nearest free seat closer than 1.5.

The floor search uses rays. First it looks at the objects (a ray along the up axis). Then it looks at the level: one ray down from bone 0, else 4 rays around it, else one short ray down. A ray finds a floor when the normal points up more than 0.7.

### Step 2: in a vehicle

1. Bone 0 sits at the seat of the vehicle and turns with it.
2. The driver (seat 0) gives the inputs to the vehicle.
3. The other bones hang from their parent bone. The view turns the head and the arms.
4. Button 0x800 held for 30 ticks gets the human out. A fast vehicle needs more ticks.
5. Each bone moves out of the walls of the level.

### Step 5: human_finish

1. Set the bond modes (`human_bond_modes`). In a vehicle, the bonds of the bones that hold no item leave the solver (mode 0x100). A muscle is weaker below 25 health and while the stun timer runs.
2. A bone 0 below y = -32 sets the health to -100.
3. A dead human drops the first item of the first 7 slots. A living human runs the item simulation.
4. Bleeding: a bleeding or dead human loses health once each 256 ticks.
5. A human on foot makes contact bonds with the other humans (`human_collisionhuman`).
6. Despawn: the timer is 3600 while a player has the human. A dead human counts down. At 2700 it lets its player go (`human_releaseplayer`). A human with no player counts down. At 0, `deletehuman` ends the human.
7. A human with a player heals 1 health each 32 ticks in a building of type 6. It stops bleeding.
8. Save the previous bone positions and compute the joint angles of each bone (`human_jointangles`).

## Creation and deletion

### createhuman

1. Find a free record. Return -1 when all 512 are in use.
2. Set the health to 100 and the despawn timer to 3600. Set the vehicle to -1.
3. Copy the colors of the player. A human with no player gets random colors.
4. Set the 15 bones: position, orientation, radius, size, mass and inertia. The data comes from the bone tree (`setupbody`).
5. Add the joints and the muscles (`human_buildmuscles`). Each one is a joint bond (type 8) and a muscle bond (type 9). The muscle turns the bone toward `target_rot`.

### deletehuman

1. The player loses the human.
2. The items leave the slots.
3. The bonds of the human stop.
4. The clients get an event.

## Legs and arms

The muscles hold angles. The leg and arm functions set those angles each tick.

- A dead human keeps the angle of each leg bone, between -pi/4 and pi/4. The knees do not bend back.
- A living human swings the legs with the walk phase. The knees bend with the stance.
- `human_collisionhuman` makes a type 13 bond for each pair of bones that are nearer than 0.375. It does so for a later human on foot. The lower legs and the feet do not touch other humans.
- `human_calculatearmangles` sets the chest and head angles from the view. It sets the bonds of the item in each hand. The right hand (bone 8) holds the item of slot 0. The left hand (bone 5) holds the item of slot 1, or a two-handed gun of slot 0. A hand with a door stops the arm code.
- `human_calcarmtorque` moves each hand a step toward its target in the frame of the chest. It sets the muscles of the shoulder, the elbow and the wrist so that the arm turns there.
- `setitempositionhuman` puts an item on the chest bone, except for slots 0 and 1. The item takes the turn of the chest and the velocity of the hip.

## Functions

| Function | RVA | File | Purpose |
| --- | --- | --- | --- |
| `humansimulation` | 0x51060 | humans.c | One tick for all humans. |
| `createhuman` | 0x50ae0 | human_body.c | Make a human. |
| `deletehuman` | 0x18720 | human_body.c | End a human. |
| `createjointbond` | 0x19ee0 | human_body.c | Add a joint bond between two bones. |
| `human_createmuscleball` | 0x306d0 | human_body.c | Add a joint and a muscle between two bones. |
| `human_buildmuscles` | 0x4c720 | human_body.c | Add all joints and muscles of a human. |
| `setitempositionhuman` | 0x1b6e0 | human_body.c | Put an item on the chest bone. |
| `setupbody` | 0x19610 | human_body.c | Fill the bone tree and the inertia of the bones. |
| `human_calculatelegangles` | 0x19060 | human_pose.c | Set the angles of the leg muscles. |
| `human_collisionhuman` | 0x19380 | human_pose.c | Add contact bonds between humans. |
| `human_calculatearmangles` | 0x4a440 | human_arms.c | Set the angles of the chest, head and arms. |
| `human_calcarmtorque` | 0x2f420 | human_arms.c | Move the hands toward their targets. |

The other functions of these files are `static`. They are parts of `humansimulation` in the game.

| Helper | File | Purpose |
| --- | --- | --- |
| `human_begin` | humans.c | Start of the tick: timers, stance, strength, bone step. |
| `human_strength` | humans.c | Muscle strength of a hurt human. |
| `human_movebones` | humans.c | One step of each bone. |
| `human_onfoot` | humans.c | The tick of a human on foot. |
| `human_findfloor`, `human_floorray`, `human_addfloor`, `human_feet`, `human_hip` | humans.c | Find the floor with rays. |
| `human_stand` | humans.c | Hold the hip at the stance height. |
| `human_walk` | humans.c | Walk on the floor. |
| `human_levelcontacts` | humans.c | Contact bonds against the level. |
| `human_objectcontacts` | humans.c | Contact bonds against objects. |
| `human_entervehicle` | humans.c | Take a seat. |
| `human_invehicle` | humans.c | The tick of a sitting human. |
| `human_finish` | humans.c | End of the tick. |
| `human_bond_modes` | humans.c | Set the modes of joints and muscles. |
| `human_dropitems` | humans.c | A dead human drops items. |
| `human_bleed` | humans.c | Health loss of a bleeding human. |
| `human_releaseplayer` | humans.c | Death tax and release of the player. |
| `human_despawn` | humans.c | Despawn timer. |
| `human_heal` | humans.c | Healing in a building. |
| `human_jointangles` | humans.c | Angles of each bone against its parent. |
| `hold_right`, `hold_left` | human_arms.c | Bonds that hold an item in a hand. |
| `hand_item_type`, `hand_target` | human_arms.c | The item and the target of a hand. |
| `turn_toward` | human_arms.c | The turn that brings a bone to a target. |

## Known odd behavior

- A dead human without a player costs its account 5 percent of the money above 10000.
- In game type 3, a dead human counts down by at least 4 each tick. In game type 4, a human without a player goes at once in a healing building.
- The arm code reads `arm_axis` and `arm_pitch`. `playersimulation` writes them from `Player.input_extra[0]` and `[1]`. This copy uses input mode 1 on foot, or mode 2 or 3 in a vehicle.
- `spawn_pos`, `spawn_pos2` and `body_param` are written when the human is made. No other code reads them.
- The wobble of the strength uses the human index and the tick. The result differs for each human.
- A NaN in a quaternion angle becomes -1 (`clamp_w`). This avoids a NaN in the arm angles.
- Each float rounding in the code follows the game. A change of a cast changes the result.
