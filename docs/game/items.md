# Items and bullets

Items are the things that a human can hold, carry or find: guns, cash, bandages, keys, doors, newspapers and briefcases. Bullets are the shots of the guns. Files: `src/game/items.c`, `item_types.c`, `item_logic.c`, `item_physics.c`, `human_items.c`, `bullets.c` and `bullet_sim.c`. The code covers the item types, the life of an item, its place, its motion, its use by a human, the bills of a cash item and the flight of the bullets.

## Records

| Record | Key fields | Use |
| --- | --- | --- |
| `Item` (array `sr_items`, 1024 records, 0x1ac bytes) | `active`, `type`, `mass`, `time_to_live` | One item. `active` is 0 for a free record. |
| | `parent_human`, `parent_item`, `parent_slot` | The place of the item. -1 means no parent. |
| | `num_children`, `children[8]` | The items inside this item. |
| | `pos`, `pos_prev`, `vel`, `rot[3]`, `ang_momentum`, `angvel`, `inertia`, `inv_inertia` | The rigid body state. |
| | `bills_mask`, `last_bill`, `num_bills` | The bills of a cash item. |
| | `ammo`, `call_state`, `info`, `linked_item`, `vehicle_id` | Gun and phone state. A key item stores its vehicle in `vehicle_id`. |
| | `ends[2]`, `end_pos[2]` | The two ends of a door. |
| | `at_rest`, `contact_vel`, `contact_angvel`, `countdown` | Free physics state. `at_rest` 1 skips the physics. |
| | `flags`, `flags_prev`, `cooldown`, `trigger_ticks`, `ring_ticks` | The use bits of the tick, the gun timers and the phone ring timer. |
| `ItemType` (array `sr_itemtypes`) | `price`, `mass`, `is_gun`, `one_handed`, `fire_delay`, `bullet_type`, `client_param`, `ammo`, `bullet_speed`, `spread`, `name`, `muzzle`, `grip`, `recoil`, `shape[4]`, `size`, `inv_inertia`, `fits_in[64]` | The data of an item type. `fits_in[parent type]` tells if the type can go inside a parent type. |
| `InventorySlot` (inside `Human.inventory`) | `count`, `items[8]` | One slot of a human. A slot holds up to 8 item indices. |
| `Bullet` (array `sr_bullets`, 16384 records) | `type`, `ttl`, `owner`, `mass`, `gravity`, `pos`, `pos_prev`, `vel`, `side`, `up`, `back` | One bullet in flight. |
| `BulletType` (array `g_bullettypes`, 4 records) | `mass`, `drag`, `caliber`, `length` | The cartridge data. `ItemType.bullet_type` selects one. |

## The places of an item

An item has one place at a time:

- In the inventory of a human: `parent_human` and `parent_slot` are set. The slot lists the item.
- Inside another item: `parent_item` is set. `parent_slot` is the index in `children[]`.
- Free in the world: all parents are -1.

Every change of place makes an item event (`createevent_updateitem`). See [network.md](network.md).

### Slot rules (`linkitem`)

- Slots 0 and 1 hold one item.
- Slot 2 holds two guns and nothing else.
- Slots 3 and up hold two items that are not guns.
- A new item that goes into a slot first leaves its old parent item and its old human.

### Container rules (`linkitemtoitem`)

- A parent holds up to 8 children.
- The `fits_in` table of the child type must allow the parent type.
- Cash items can go in up to 5 times, for example into an open briefcase.
- Other types go in only alone.

## Cash bills

A cash item holds up to 10 bills. Each bill uses 3 bits of `bills_mask`. Bill 0 is in the low bits. `last_bill` is the index of the last bill, or -1 for no bill.

The value of a bill type comes from a game table: 1, 5, 10, 20, 50, 100, 1000 and 0.

- `item_billvalue` adds the value of all bills.
- `item_addbill` puts a bill at an index and moves the higher bills up. It returns 0 when the item holds 10 bills.
- `item_removebill` takes a bill out and moves the higher bills down. When the last bill goes, the item gets `time_to_live` 0 and so it ends at the next tick.

## Flow

### createitem

1. Find the first free record. Return -1 when all 1024 are in use.
2. Clear the record. Set `active`, `type`, the mass and the inertia from the item type.
3. Set `time_to_live` to 0xffff (the item stays) and all parents to -1.
4. Copy the position (to `pos` and `pos_prev`), the velocity and the orientation.
5. For a door, compute the two end positions. The ends are half a unit back on the x axis. They are one unit down and one unit up on the y axis.
6. Make the item event.

### itemtimetolive (each tick)

For each active item:

1. If `time_to_live` is below 0xffff, subtract 1.
2. If it is still 1 or more, go to the next item.
3. A newspaper lowers the newspaper count of the game.
4. The item leaves its human and its parent item. Its children leave it.
5. A key item sets the `time_to_live` of its vehicle to 0. The vehicle goes with its key.
6. Set `active` to 0 and make the item event.

### Bullets

1. `setupbullettypes` copies the four bullet types to the game table. The types are 7.62x39, 5.56x45, 9x19 and .44 Magnum.
2. `createbullet` adds a bullet at the end of the list. It returns -1 when 16384 bullets exist.
3. The bullet gets `ttl` 600, a mass from its type and a gravity of 9.8 / 3600 per tick.
4. The bullet gets a frame: `back` is the opposite of the velocity. `side` is `back` crossed with an axis. `up` is `side` crossed with `back`. All three are unit vectors.
5. `bullettimetolive` lowers `ttl` by 1 for each bullet. A bullet with `ttl` 0 leaves the list.

The list is packed. The last bullet takes the place of a bullet that ends.

### setupitemtypes

Fills the table of the 27 item types. The guns are AK-47, M-16, MP5, Uzi, Magnum and 9mm. Each gun has a bullet type, a fire delay, a bullet speed and a spread of 0.0333. The recoil is the bullet mass times the bullet speed times 0.25 (0.5 for the Magnum and the 9mm). The Uzi copies the recoil of the MP5. A magazine names its gun in `fits_in`. At the end, every type gets the inverse inertia of a box of its size. The function reads the bullet masses, so `setupbullettypes` runs first.

### itemlogic (each tick)

For each active item, in this order:

1. Life. A loose item (no parent, not a phone, not cash) and an item inside a loose gun lose 1 tick, up to 18000. An item with children gets 18000. In game type 3 the life is 0xffff. Any other item with a life goes back to 0xffff.
2. A gun runs `itemweaponsimulation`.
3. A key sets `key_away` of its vehicle to 1. A human that sits in that vehicle and holds the key sets it back to 0.
4. A phone runs its state machine (see below).
5. The use key (press) turns a briefcase into an open briefcase and back.
6. The release of the use key on a bandage heals the nearest human within 2 units that bleeds or has less than 10 health. The bandage loses 1 use.
7. `flags_prev` takes `flags`, and `flags` goes to 0.

### Phone states

| State | Meaning | Change |
| --- | --- | --- |
| 0 | Idle | The use key starts the dial (state 1). |
| 1 | Dial | The use key goes back to 0. A number above 999 looks for an idle phone with that `phone_id`. A match sets both phones to state 2. |
| 2 | Ring | The ring sound plays every 128 ticks. After 0x2ff ticks both phones go to 0. The use key on the called phone answers. |
| 3 | Talk | The use key ends the call. A call whose partner is gone ends by itself. |

### itemweaponsimulation

1. `cooldown` counts down. A gun with no human does nothing.
2. Outside game state 2 only the top player of a team can shoot.
3. `trigger_ticks` counts the ticks of the use key. The shot happens when it equals 2. The Magnum needs 6. The Magnum and the 9mm shoot once for each press.
4. A magazine in the gun loads the chamber when `ammo` is 0.
5. The shot gives the bullet the gun velocity, the speed of the type and a random spread. The gun gets the opposite momentum. The bullet and the event are made. The human gets a random recoil. `cooldown` takes `fire_delay`.

### itemsimulation (each tick)

1. An item below y = -32 gets `time_to_live` 0. `countdown` counts down.
2. A door (type 25) runs `item_physicssimulation`. A loose item, or an item in a hand, that is not at rest runs the free physics. Any other item gets `at_rest` 0.
3. Free physics: move, turn by `angvel`, gravity, a sweep of the center against the level (bounce with a loss), then the 8 corners of the box. A corner under the level pushes the item out and turns it. With more than 3 corners down and almost no speed, the item rests.
4. An item inside another item copies the position and the axes of the parent. In a briefcase, cash items lie side by side. Other parents hold the item 0.0625 units back.

### item_physicssimulation (door)

A door slows down, turns, falls, and hangs at two ends (`end_pos`) with a spring and a damper. A bone of a human (not the hands) that touches the door gets a contact bond of type 14. The list of bonds has no size test here.

### human_itemsimulation (each tick, each human)

Slots 0 and 1 are the hands and slots 2 to 6 are the pockets. Button 0x10 chooses the hand that the keys use.

1. The reload timer counts down.
2. With an empty hand, the use key (press) casts a line of 2 units from the head. It can set the floor of an elevator panel, call an elevator, or open a base door for a player of the team that stands in the base.
3. With an empty hand: button 0x1000 takes a magazine out of the gun in the other hand, a pocket key takes the item of that pocket, and the release of the use key takes the nearest loose item on the line of sight (within 0.5 units). A taken door also sets the hand target.
4. With an item in the hand: the use key and the load key set bits of `flags`. A held door sets the hand target. Button 0x20 drops the item. Button 0x1000 loads the magazine into the gun of the other hand. A pocket key moves the item to the pocket. A briefcase does not go to a pocket.
5. Button 0x2000 swaps the hands.
6. Every item in the slots 0 to 6 gets `at_rest` 0 and goes to the position of its slot. A gun in the inventory sets `protect_timer` to 0.

### bulletsimulation (each tick)

1. The bullet loses speed to the air (drag of its type) and moves.
2. The server finds the nearest hit on a human (not the hands of the owner, not an immune player) or on an object.
3. The level wins when it is nearer. A block may break. The bullet ends.
4. A human hit does damage from the speed times the mass. The head multiplies it by 2, 3 or 4. The team kill tax (a shot at a human of the same team) works only in game type 3, the round mode. Game type 4 taxes a shot at a protected human of team 7 instead. Other game types have no tax. The human may bleed, gets a stun time and gets a push on the bone. The bullet ends.
5. An object hit on a wheel lowers its health. A flat wheel gets a smaller radius. A window face loses health and breaks at 0. Any other face damages the object. The face particles get a push. The bullet ends.

## Functions

| Function | RVA | File | Purpose |
| --- | --- | --- | --- |
| `item_billvalue` | 0x7b90 | items.c | Value of all bills of a cash item. |
| `item_addbill` | 0x7c40 | items.c | Add a bill at an index. |
| `item_removebill` | 0x7ce0 | items.c | Remove a bill at an index. |
| `linkitemtoitem` | 0x7d60 | items.c | Put an item inside another item. |
| `item_addhumanlink` | 0x7e00 | items.c | Add an item to an inventory slot. |
| `item_removehumanlink` | 0x7e50 | items.c | Remove an item from an inventory slot. |
| `item_removeitemlink` | 0x7f80 | items.c | Remove a child from its parent item. |
| `createitem` | 0x1b070 | items.c | Make a new item. |
| `linkitem` | 0x1b870 | items.c | Move an item to an item, a slot or the world. |
| `itemtimetolive` | 0x30ab0 | items.c | Count down the life of the items and delete old ones. |
| `setupbullettypes` | 0x4a10 | bullets.c | Fill the table of the four bullet types. |
| `createbullet` | 0x143b0 | bullets.c | Add a bullet to the list. |
| `bullettimetolive` | 0x14590 | bullets.c | Count down the life of the bullets. |
| `setupitemtypes` | 0x6b30 | item_types.c | Fill the table of the 27 item types. |
| `itemweaponsimulation` | 0x30ba0 | item_logic.c | Shoot a held gun. |
| `itemlogic` | 0x4c8c0 | item_logic.c | Life, keys, phones, briefcases and bandages. |
| `item_physicssimulation` | 0x4cee0 | item_physics.c | Hang a door at its two ends. |
| `itemsimulation` | 0x542a0 | item_physics.c | Move the items. |
| `human_itemsimulation` | 0x4bd60 | human_items.c | The use of the hands and the pockets. |
| `bulletsimulation` | 0x48ec0 | bullet_sim.c | Move the bullets and apply the hits. |

## Known odd behavior

- A slot removal takes out every copy of an item index in the slot. The last item of the slot takes the place of the removed item.
- `item_removebill` with one bill left sets `time_to_live` to 0 and keeps the bill. The item is deleted later.
- `bullettimetolive` tests the moved bullet again. A bullet that moves into a place loses one more tick of life.
- `ItemType.client_param` is copied to the item field `unk_e4`. The values are 90 for the rifles and the sub machine guns, 18 for the Magnum and 32 for the 9mm.
- A door end is computed from the global axes `(1 0 0)` and `(0 1 0)`, not from the orientation of the door.
- `human_itemsimulation` uses player 0 for the team test of a human with no player.
- The elevator floor that a human sets reads the building of city object 0, not of the new elevator.
- `itemweaponsimulation` adds `0.0` times the axis to the muzzle position. The sum keeps the sign of a zero.
- A broken window sends its event again on each hit, and an object hit always ends the bullet.
- `item_physicssimulation` can write past the end of the bond list. The game has no size test there.
- A phone call sets the `linked_item` of the caller only when the call goes out. The called phone gets its link at the answer.
- The arm of a door end rounds `ends.x * rot[0]` to a float before the next term, in all 3 components. The assembly does this. The lifted C did not round the x component. No test sees it, because `ends.x` is -0.5 and the product is exact.
