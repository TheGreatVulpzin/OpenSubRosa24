# Items and bullets

Items are the things that a human can hold, carry or find: guns, cash, bandages, keys, doors, newspapers and briefcases. Bullets are the shots of the guns. Files: `src/game/items.c` and `src/game/bullets.c`. The code covers the life of an item, its place, the bills of a cash item and the bullet list. The code that uses a gun (`itemlogic`, `itemweaponsimulation`) is not rewritten yet.

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
| `ItemType` (array `sr_itemtypes`) | `price`, `mass`, `is_gun`, `one_handed`, `fire_delay`, `bullet_type`, `ammo`, `bullet_speed`, `name`, `inv_inertia`, `fits_in[64]` | The data of an item type. `fits_in[parent type]` tells if the type can go inside a parent type. |
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

## Known odd behavior

- A slot removal takes out every copy of an item index in the slot. The last item of the slot takes the place of the removed item.
- `item_removebill` with one bill left sets `time_to_live` to 0 and keeps the bill. The item is deleted later.
- `bullettimetolive` tests the moved bullet again. A bullet that moves into a place loses one more tick of life.
- `ItemType.unk_0018` is copied to the item field `unk_e4`. The values are 90 for the rifles and the sub machine guns, 18 for the Magnum and 32 for the 9mm.
- A door end is computed from the global axes `(1 0 0)` and `(0 1 0)`, not from the orientation of the door.
