# File formats (Sub Rosa 0.24)

Readers in [`tools/formats/subrosa_formats.py`](../tools/formats/subrosa_formats.py).

Source: decompilation of the Linux client 0.24b (`subrosa.x64`, with symbols). Functions: `saveaccountsserver`, `loadaccountsserver`, `saveaccountsclient`, `loadblock`, `building_save`, `building_load`, `leveleditor_save`, `leveleditor_load`, `savemodelcsm`, `loadmodelcsm`, `createaccount`.

General rules:

- Little-endian. 32-bit integers unless noted.
- The game reads through the pointer `freadptr`. On Mac PowerPC it points to `freadswap` (swaps the byte order). On x86 it is a plain `fread`.
- The first integer of each file is the version. The game rejects versions outside the range.

Test: all 0.24b files pass and are read to the last byte (29 `.sbb`, 30 `.sbl`, `city.sbc`, `ak47.csm`, 2 `server.srk`). The `server.srk` writer produces files identical to the originals.

```bash
python -I tools/formats/subrosa_formats.py original/client/data/city.sbc
```

## `user/server.srk`: server accounts

| Field | Type | Description |
| --- | --- | --- |
| version | int | 1 to 3. The game writes 3. |
| key | uint | Global `account`. Random number (`genrand_int32`) generated when the file does not exist. |
| n | int | Number of accounts (maximum `0xffff`). |
| accounts | n × 60 bytes | See below. |

Account in the file (60 bytes). In memory the record is `0x4c` bytes. The fields `+0x28` and `+0x40..+0x4b` do not go to the file.

| Memory offset | Type | Field | Description |
| --- | --- | --- | --- |
| `+0x00` | uint | id[0] | Copied from `player+0x24`. Identifies the player. |
| `+0x04` | uint | id[1] | Copied from `player+0x28`. |
| `+0x08` | char[32] | name | The game writes all 32 bytes, with garbage after the `\0`. A repeated name gets `(n)`, cut at 24 characters. |
| `+0x2c` | int | money | New account: 1000. Can go negative (debt). |
| `+0x30` | int | unknown | Not read in version 1. |
| `+0x34` | int | look A | Creation: `rand() % 3`. |
| `+0x38` | int | look B | Creation: `rand() & 7`. |
| `+0x3c` | int | look C | Creation: `rand() & 3`. |

Version 1 does not have the last 4 fields. In version 2, if A, B and C are 0, the game rolls again.

## `client.srk`: accounts on the client

| Field | Type |
| --- | --- |
| version | int (1) |
| n | int |
| records | n × 4 int |

Likely: each record holds a server key and the player `id` on that server. Needs confirmation.

## `data/block/*.sbl`: block (building piece)

| Field | Type | Description |
| --- | --- | --- |
| version | int | 1 or 2. |
| header | 10 int | `blocktemp+0x40..+0x64`. The 10th is the number of surfaces. |
| surfaces | see below | |
| n_boxes | int | Version 2 only. |
| boxes | n × (8 vec3 + 6 int + 1 int) | Version 2 only. Record of `0x7c` bytes. 8 corners, 6 values per face, 1 flag. Likely: collision. |

Surface (record of `0x15c` bytes in memory):

1. 6 int, read in the order `+8, +0, +0xc, +4, +0x150, +0x154`.
2. 16 points (4×4 grid). Each point: position (3 float) + UV (2 float). It is a bicubic Bézier patch (`addbeziersurface`).

## `data/building/*.sbb`: building

| Field | Type | Description |
| --- | --- | --- |
| version | int | 6 to 10. The game writes 10. |
| name | char[64] | |
| size_x | int | Columns. Default 24. |
| size_z | int | Rows. Default 24. |
| floors | int | Default 4. |
| n_textures | int | |
| textures | n × char[64] | E.g. `concrete`, `greycarpet`, `ceiling0`. |
| n_blocks | int | |
| blocks | n × char[64] | Names of `.sbl` files. E.g. `astairwell`, `rooftop`. |
| cells | see below | Grid with border: floor `0..floors-1`, row `0..size_z`, column `0..size_x`. |
| extras | 16 × 6 int | Version ≥ 9 only. |

In memory the grid is `[64 floors][64 rows][64 columns]` of `0x48` bytes, at `building+0x4c`.

Cell in the file (read order):

| Offset | Type | Note |
| --- | --- | --- |
| `+0x00`, `+0x04` | int, int | Cell is occupied when one of them is not 0. They use high bits as flags. |
| `+0x10` | int | Version ≥ 7 only. |
| `+0x08`, `+0x0c`, `+0x14` | int ×3 | Likely: texture indices (floor, ceiling). |
| `+0x18..+0x36` | 16 × int16 | Likely: pieces/textures per side. |
| `+0x38`, `+0x3c` | int ×2 | `+0x3c` is zeroed in version < 8. |
| `+0x40`, `+0x44` | int ×2 | Version ≥ 10 only. |

## `data/city.sbc`: city

Single file, ~1 MB. The game only accepts version 6. Data goes to the global `level_editor`.

| Field | Type | Description |
| --- | --- | --- |
| version | int | 6. |
| n_intersections | int | |
| intersections | n × 3 int | `(x, y, z)` in tile units. E.g. `(160, 0, 288)`. |
| n_streets | int | |
| streets | n × (5 int + char[32]) | The first 2 int are the intersections at the ends. Name: `Main`, `6th Street`... |
| tiles | 1024 × 1024 bytes | Tile map. Values seen: 0, 16, 17, 18 and multiples of 16 up to 176. |
| n_buildings | int | |
| buildings | n × (char[64] + 4 int) | Name of the `.sbb` + `(x, y, z, rotation 0-3)`. E.g. `gunshop` at `(147, 6, 261, 2)`. |

The `city.sbc` of 0.24b has 54 intersections, 84 streets and 39 buildings.

## `*.csm`: 3D model

| Field | Type | Description |
| --- | --- | --- |
| signature | 4 bytes | `CSMo` |
| version | int | 1 |
| n_vertices | int | |
| unknown | int | `model+4`. Value 8 in `ak47.csm`. |
| n_triangles | int | |
| positions | n_vertices × vec3 | |
| vector B | n_vertices × vec3 | In `ak47` it starts with (1, 0, 0). Likely: tangent. |
| vector C | n_vertices × vec3 | In `ak47` it starts with (0, 1, 0). Likely: bitangent. |
| indices | n_triangles × 3 int | Vertices of each triangle. |
| normals | n_triangles × vec3 | Face normal (unit vectors). |
| C indices | n_triangles × 3 int | Likely: UV indices. |

## Open items

- Name the `.sbb` cell fields with the code of `building_setupbuild`.
- Name the 10 header integers of `.sbl` and the 5 street integers of `.sbc`.
- Confirm `client.srk` with the code of `loadaccountsclient`.
- Find out the account field `+0x30`.
