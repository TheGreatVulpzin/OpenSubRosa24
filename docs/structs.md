# Structs (Phase 1)

Generated headers:

- [`src/include/sr_types.h`](../src/include/sr_types.h): `Player`, `Human`, `Item`, `Object`, `Connection`, `Account`, `vec3`, `Bone`, `InventorySlot`.
- [`src/include/sr_globals_024d.h`](../src/include/sr_globals_024d.h): RVAs of the global variables in the 024d.

Do not edit the headers by hand. The flow is:

1. Decompile the whole i386 Mach-O: `instrumentation/ghidra/decompile.sh subrosa build/decomp/mac_i386_all.c`.
2. Collect the accessed offsets: `python -I tools/re/struct_fields.py build/decomp/mac_i386_all.c --out analysis/structs/mac --struct player=0x2408 ...`.
3. Name the fields in [`analysis/structs/names.csv`](../analysis/structs/names.csv). Each name needs evidence.
4. Generate the headers: `python -I tools/re/gen_types.py`.
5. Compile `instrumentation/tests/check_types.c` with MSVC x86. The `static_assert`s check every size and every named offset.

## Why the Mach-O i386

The Mach-O is 32-bit like the 024d PE. The record sizes are equal in both: `player` `0x2408`, `human` `0x2b64`, `object` `0x8230`, `item` `0x1ac`, `connection` `0xf170`. The x64 ELF has the same sizes for these structs, because they have no pointers.

## Validation in the 024d

The 024d functions `createplayer` (`0x28020`), `createhuman` (`0x50ae0`) and `createitem` (`0x1b070`) write the same constants at the same offsets as the Mac:

| Struct | Offset | Value written |
| --- | --- | --- |
| player | `+0x22f4..+0x2320` | 4 color vectors |
| player | `+0x23b4`, `+0x23b8`, `+0x23bc` | `rand() % 3`, `& 7`, `& 3` |
| human | `+0x24` | `0xe10` (3600) |
| human | `+0xc0` | `0x3c0` |
| item | `+0x10`, `+0x118..+0x120`, `+0x17c..+0x19c` | same as the Mac |

## Differences between versions

| Field | 0.24b (Mac) | 024d |
| --- | --- | --- |
| `human+0x34` at creation | `((rand() & 3) + 2) * 3600` | `((rand() & 3) + 3) * 3600` |
| `Game` | tick at `+0x34` | tick at `+0x38`: 0.24c inserted a 4-byte field at the start |

## Game

The global `game` is a single struct of `0x25d64` bytes. The layout comes from the 0.24c server (ELF), and the 024d uses the same offsets in the same functions. The Mac 0.24b is not a valid reference for `Game` (see the table above). In the 024d, the global `item` starts right after it, at `game + 0x25d80`.

## Status (Phase 1 criteria)

| Criterion | Status |
| --- | --- |
| 1. Server structs in the header, with the right size | `Player`, `Human`, `Item`, `Object`, `Connection`, `ItemType`, `ObjectType`, `Bond`, `Event`, `Bullet`, `Body`, `Account`, `Game`. |
| 2. Sub-structs | `Bone` (position, rotation, mass), `InventorySlot`, `vec3`. |
| 3. Name coverage ≥ 80% | **80.0%** of the server accesses (`tools/re/struct_coverage.py`). |
| 4. 024d globals | All the arrays above, plus `numofbonds`, `numofbullets`, `numofevents`, `ticks`. Confirmed in `analysis/024d/manual_globals.csv`. |
| 5. Validation | Compiles with MSVC x86 at `/W4 /WX`. 170 `static_assert`s. Offsets checked in the 024d. |

**Phase 1 is complete.**

Capacities (size of the global in the 0.24c ELF ÷ record size): player 256, human 512, item 1024, object 1024, connection 256, bond 16384, bullet 16384, event 65536 (circular buffer), particle 4096, objecttype 128, itemtype 64, body 32, account 65536.

## Protocol findings

- Player input packet (`serverrecv`): name (32 bytes) and `account_id` on join. Then, every frame: 8 floats (`+0x7c..+0x98`), buttons (`+0x9c`), `input_mode` (8 bits), `hud_mode` (4 bits), `num_actions` (8 bits).
- Events: the client acknowledges the last event it received (`connection.event_ack`, 16 bits). The server resends from `event_ack` up to `numofevents`, at most 31 per packet.
- Team phones: number = `1111 × team` (`item.phone_id`).

## Next steps

- Name the `Bone` fields (15 per body) with `human_buildmuscles` and `bondsimulation2`.
- Name `Object` (vehicles) with `createobject` and `objectsimulation_car`.
