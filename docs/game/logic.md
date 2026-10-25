# Players, economy and game logic

`logicsimulation` runs the player menus, counts team members and selects the game mode.
It also updates the traffic lights. A nearby train makes all four lights of intersection 4 red.
Player, team and loop indices use integers.

## Game flow

Round mode uses three states:

| State | Behavior |
| --- | --- |
| 1 | The players select teams and buy equipment. Ready players can shorten the wait to 1860 ticks. At 1800 ticks, the game creates missions, traffic, team cars and phones. At zero, it opens the doors. |
| 2 | The round timer runs. Without money items and active missions, it can shorten to 60 ticks. It can also shorten when no human player remains. At zero, the game starts state 3. |
| 3 | The end timer runs. At zero, the game saves player inventories and calls `resetgame`. |

World mode changes state 1 to state 2, creates 32 traffic bots and sets the clock to 1800000.
The clock adds 10 each tick. At 3888000, the game values the player assets, saves accounts and resets.
Every 18000 clock units, it updates the shops, saves accounts and adds five minutes of play time.
Every 36000 units, it reduces account bans. Every 54000 units, it resets purchase limits and creates missions.
The newspaper count stays at eight while the world runs.

`resetgame` clears bodies, items, events, voices, player actions and active missions.
It clears bot players, creates the level and sets the random seed.
A server reset also places two trains.
Server test modes 0, 1 and 2 also create test players and equipment.
`setupgame` assigns 485 model identifiers, 848 texture identifiers and 47 sound identifiers.
It initializes 256 material records, even when the material count has another value.

## Players and accounts

Each player has a human identifier, team, money, stocks, menu and a ring of 64 actions.
`playersimulation` transfers the input to the human and packs the inventory for the client.
Only hand slots 0 and 1 contribute gun flags and ammunition to the packed state.
`createplayer` uses the first free player slot. `deleteplayer` removes team and human links.

The account database stores server accounts separately from client accounts.
Client files start with version 1, then a count and four integers per account.
The server save includes the account key and account records.
`player_saveinventory` stores item types, ammunition and vehicle keys.
`player_loadinventory` creates the items again and links them to the human.

## Economy

Each company has a share count, price, opening price, integer price and mission stake.
Buying and selling stocks changes both the company count and the player's holding.
The world reset starts each company with 100 shares at a price of 100.
Round mode keeps the integer price as its opening price.

The unused `economysimulation` function pays ready players and ranks them by assets.
It gives each player 150 plus half the team cash. Managers can receive another stock bonus.
It limits prices to at least 10 and splits at 500 or more.
Cash and mission rewards can also trigger a split, with their own comparison forms.
A split multiplies holdings by five and scales the price by the stored float value near 0.2.
Mission stakes remain separate from player money.

Death tax takes five percent of money above 10000 and reduces the stock holding.
Team damage takes money and stocks. In world mode, it also damages the player's human.

## Missions

Each team has 16 mission slots. A scenario can link missions across several teams.

| Kind | Role |
| --- | --- |
| 0 | Acquisition from guards in a building |
| 1 | Acquisition from a vehicle chase |
| 2 | Giver in a three-way exchange |
| 3, 4 | Other roles in a three-way exchange |
| 5, 6 | Transaction roles |

`createscenario_world` selects teams that need missions and shuffles their identifiers.
It chooses an acquisition, a three-way exchange or a transaction.
The world clock sets each mission's expiry. Expiry clears every mission of its scenario.
Active mission items keep their lifetime at 65535 when their lifetime is positive.
Completion pays the reward, adjusts stock prices and stakes, and clears the linked missions.

## Menus

Menu actions use kind 0. The action's menu identifier must match the player's current menu.
Kind 1 sends chat. Kind 2 selects money bills or enters phone digits.
The action ring advances with `(tail + 1) & 63`.

| Menu | Values |
| --- | --- |
| 0 | No menu action |
| 1 | 1 spawns a human in world mode |
| 2 | 1 to 3 select teams, 4 selects team 7, 5 marks the player ready |
| 3 | 1 to 5 open menus 4, 5, 7, 11 and 8 |
| 4 | 1 to 4 buy item types 10, 6, 0 and 2. Value 16 returns to menu 3. |
| 5 | 1 to 4 buy item types 11, 7, 1 and 3. Value 16 returns to menu 3. |
| 6 | A row buys a team car and key. Value 16 returns to menu 3. |
| 7 | 1 buys type 12. Value 2 buys type 13. |
| 8 | Odd values 1, 3, 5 and 7 buy 1, 10, 100 and 5000 shares. The following even values sell them. |
| 9 | 1 opens shop menu 10 |
| 10 | A row buys a suit, car or gun from the selected building |
| 11 | 1 to 6 withdraw bills from the team stake. Value 7 deposits money or completes a mission. Value 16 returns. |
| 12 | 1 to 6 withdraw bills from player money. Value 7 deposits money. Value 16 returns. |

Weapon purchases stop at ten per player. Small bill withdrawals stop at forty.
Car purchases stop at three per player. The periodic reset reduces this count by one when it is positive.
The bank uses the bill table for values 1 to 6.
Phone input accepts digits 0 to 9 only while the phone can dial and its cooldown is zero.

## Bot controls

`ai_assessthreats` scans humans within 128 units and tests the view direction and level geometry.
It stores at most 16 threats. Visible threats gain one level, up to 16.
Each call scales every level by 0.96875 and stores it as a float before the 0.875 comparison.
A level below 0.875 is removed. A NaN level stays in the list.
Removal copies the last entry into the gap and tests that entry again.
Entry zero stays the default target, even when the threat count is zero.
Other targets must have a level above eight and a shorter distance.

`playerai` reloads equipment, follows foot waypoints or calls `ai_playercar` for the driver.
Foot combat requires a threat level above 12. Passengers use a level above eight.
Foot aim uses the wrapped angular difference. Passenger aim uses a direct subtraction.
Each update applies one sixteenth of the difference.
The pitch calculation keeps the horizontal square root at double precision until `atan2` returns.
Friendly fire checks use 128 units on foot and 64 units in a vehicle.

## Record layouts

| Record | Size | Fields |
| --- | --- | --- |
| `Mission` | 0x40 | +00 active, +04 scenario, +08 kind, +0c item, +10 partner, +14 unknown, +18 padding, +20 item_type, +24 cash, +28 expire, +2c site, +30 stake, +34 stake_min, +38 own_growth, +3c other_growth |
| `Stock` | 0x14 | +00 shares, +04 price, +08 open_price, +0c price_int, +10 stake |
| `SavedItem` | 0x0c | +00 type, +04 a, +08 b. Guns use magazine presence and ammunition. Keys use vehicle type and color. |
| `SavedSlot` | 0x64 | +00 count, +04 eight `SavedItem` entries |
| `Threat` | 0x18 | +00 human, +04 position, +10 level, +14 timer |
| `MaterialRec` | 0x58 | +00 four float parameters, +10 flags, +14 a 68-byte name |

`MaterialList` holds material identifiers, preset rows and identifiers for files.
The shared street names at +0x28c to +0x298 match the level system.
`AiWaypoint.pos` occupies its first 12 bytes. Driving also uses road, direction, lane and turn fields.

## Preserved quirks

These rules keep the state bytes of defined behavior.
Undefined stack values use fixed defaults and can differ, as the entries below explain.

- A player who leaves a team writes a stale integer to `Human.team`. It can be 16, a market cap or 5.
  The player still receives team 7. The loop that lets team 7 join a base has no early exit.
- A zero share count can make a cash fraction NaN. The comparisons keep NaN.
  Its integer conversion adds `INT_MIN` to the player's money.
- Threat decay compares the rounded float. A product just below 0.875 can round to 0.875 and remain.
  Driving uses a strict distance comparison. A checkpoint with a NaN distance stays unreached.
- The manager phone angle reads team record 1024, at RVA 0x953b5c8.
- The menu reopen flag has no initial value in the game. The C function starts it at zero.
  A menu 10 choice sets it, and later choices in the same call keep it.
- Passenger seats 4 and above can use an undefined aim value. The C function uses zero.
- Guards with no spots, a chase with no roads and some other early returns have no defined return value.
  Tests can report a return difference for guards with no spots.
- A full mission list still uses slot 16. A full player list can leave a scenario spawn with index -1.
- Reset test modes keep their item order, random call order, float stores and event order.
  Material name copies leave bytes after the terminating zero unchanged.

`instrumentation/lift/synth_logic.py` tests the edge cases and retains per-case logs under `build/`.
Run it with both a world recording and a round recording.

## Measured performance

The step 6 bench on 2026-10-22 measured `logicsimulation` at 1.00 for 193 calls.
It measured `logicsimulation_world` at 0.99 for 121 calls.
Three helpers remained above 1.05 in all three final runs:

| Function | Calls | Ratios |
| --- | ---: | --- |
| `logic_player` | 168 | 1.10, 1.14, 1.14 |
| `createevent_updateplayer_finance` | 38 | 1.12, 1.13, 1.14 |
| `lineintersectnormalblock` | 130 | 1.16, 1.13, 1.07 |

These results remain a known performance limit of group 6.

## Functions

| Function | RVA | File |
| --- | --- | --- |
| `account_nameexists` | `0x1010` | `account_db.c` |
| `saveaccountsserver` | `0x1080` | `account_db.c` |
| `saveaccountsclient` | `0x11d0` | `account_db.c` |
| `loadaccountsclient` | `0x12f0` | `account_db.c` |
| `createaccount` | `0xded0` | `account_db.c` |
| `economysimulation` | `0x53a0` | `economy.c` |
| `economy_carmarket` | `0x5700` | `economy.c` |
| `economy_gunmarket` | `0x5870` | `economy.c` |
| `playerbuystock` | `0x5b40` | `economy.c` |
| `playersellstock` | `0x5be0` | `economy.c` |
| `player_calcassets` | `0x5f20` | `economy.c` |
| `logic_addteamcash` | `0x8cf0` | `economy.c` |
| `logic_teammissioncomplete` | `0x8fe0` | `economy.c` |
| `scenario_getnumofmissions` | `0xaa80` | `economy.c` |
| `scenario_setmissioninactive` | `0xab20` | `economy.c` |
| `playerdeathtax` | `0x16960` | `economy.c` |
| `playerteamkilltax` | `0x16a20` | `economy.c` |
| `setupgame` | `0x502a0` | `game_setup.c` |
| `resetgame` | `0x590f0` | `game_setup.c` |
| `logic_suits_buy` | `0x93e0` | `logic.c` |
| `logic_player` | `0x94a0` | `logic.c` |
| `logic_farbslist` | `0x1d2c0` | `logic.c` |
| `logicsimulation_driving` | `0x1d3b0` | `logic.c` |
| `logic_setteammanager` | `0x1d4d0` | `logic.c` |
| `logic_gunstore_buy` | `0x1d7a0` | `logic.c` |
| `logic_createcar` | `0x4e060` | `logic.c` |
| `logic_cardealer_buy` | `0x4e0d0` | `logic.c` |
| `logic_playeractions` | `0x55700` | `logic.c` |
| `logicsimulation` | `0x5c580` | `logic.c` |
| `logicsimulation_round` | `0x5ae70` | `logic_sim.c` |
| `logicsimulation_world` | `0x5b9b0` | `logic_sim.c` |
| `createmission_threeway` | `0x28230` | `missions.c` |
| `createmission_transaction` | `0x286b0` | `missions.c` |
| `createmission_acquisition` | `0x58ef0` | `missions.c` |
| `createscenario_world` | `0x5aa60` | `missions.c` |
| `player_saveinventory` | `0x5fe0` | `player.c` |
| `deleteplayer` | `0xa1b0` | `player.c` |
| `playersimulation` | `0xa2a0` | `player.c` |
| `createplayer` | `0x28020` | `player.c` |
| `player_loadinventory` | `0x500e0` | `player.c` |
| `ai_assessthreats` | `0x3d500` | `player_ai.c` |
| `playerai` | `0x4f720` | `player_ai.c` |
| `scenario_armhuman` | `0x28190` | `scenario.c` |
| `scenario_createcar` | `0x33350` | `scenario.c` |
| `scenario_createguards` | `0x58320` | `scenario.c` |
| `scenario_createtraffic2` | `0x585b0` | `scenario.c` |
| `scenario_createchase` | `0x587f0` | `scenario.c` |
