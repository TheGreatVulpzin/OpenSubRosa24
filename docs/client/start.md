# Start of the client

`setupgame` and `setupclient` (`src/client/setup/identifiers.c`, `client_setup.c`) prepare everything that the game draws and plays. `setupgame` gives a number to each model, texture, sound and material. `setupclient` builds the thing behind each number.

## Identifiers

Each list is a record with named fields. A field holds the number of one thing, or an array of numbers.

- `ModelList` (485 numbers): `vehicle[32]`, `item[32]`, `wheel[32]`, `human[256]`, `bullet[32]`, `bullet_flat[32]`, `city_object[32]`, `sunglasses`, `head`, `foot`, `hair`, `sprite[32]` and `rotor`. The renderer reads `item` with the item type, `wheel` with the wheel type of an object and `human` with the human number.
- `TextureList` (848 numbers): the 256 material textures, the human textures, the paint colors, the street signs and lamps, the panels, the cash bills, the landscape textures and the pictures that come from files. Seven slots have no name yet.
- `SoundList` (47 numbers): the gun shots, the engines, the tires, eight ricochets, the crashes, the bullet hits, the glass, the phone, ten digit tones and the gear. Ten slots have no name yet.
- The material list holds 44 numbers for the preset materials, then one for each picture of the folder `data/texture`.

The numbers go out in a fixed order, so a number always means the same thing. `assign_asset_ids` (`src/game/asset_ids.c`) hands out the model, texture and sound numbers, and the server calls the same function. The client then numbers the materials.

## Setup of the client

The calls run in a fixed order.

1. The sphere mesh and the landscape sector lists.
2. `setupgame`.
3. The models: bullets, items, wheels, one human, ammunition shapes, city objects, head, hair, foot and one sprite.
4. The generated textures: one for each material, then the plain colors, the street signs, the panels and the landscape detail.
5. The textures from files: the desert, the table of random vectors, the twitter and microphone pictures, seven cash bills and one picture for each file of the folder.
6. The logo texture: 32 by 16 pixels. A pixel is white where the bitmap is not zero.
7. The frame textures.
8. The sounds: 30 files and 11 generated tones.

## Settings keys

`src/client/setup/option_keys.c` reads and writes one key of the settings file. A read searches the key in the text and sets the value. A write puts the key and the value on one line, with a carriage return and a line feed. The file is the shared file handle `open_file`. The server uses the same worker (`option_int` and `option_string` in `src/game/options.c`) and only reads.

## Quirks

- A model that a builder makes from an item gets the item type as the second argument. The cash builder makes the Magnum, the Uzi and the 9mm, and the bill builder makes the two cash items.
- Material numbers from 256 up have no entry in `TextureList.material`. The folder can add more files than that, and the code then writes and reads past the array.

## Tickets and folders

`ticketreturned` keeps an authentication ticket of up to 1024 bytes. `steam_getauthticket` gives the ticket and its size, or null when none is kept. `initplayticket` sets the play ticket to 0, 0, -1, 0. `getpaths` keeps the start folder and then finds the documents folder of the user. It makes the folder `Sub Rosa` there when it is missing, writes its path into the settings path, and goes back to the start folder. `loadaccountsserver` reads `user/server.srk` without checks on the lengths and keeps the file handle in the shared open file global.

## Records of players and messages

`player_addaction_menu`, `player_addaction_chat` and `player_addaction_item` add an action to the ring of 64 actions of a player. A chat action keeps up to 63 characters, with no end mark. `addplayerwaypoint` adds a waypoint and `logic_splitteamcash` shares an amount between the active players of a team. `scenario_isseekmission` tells if a team has an active mission of kind 4.

`createmessage` adds a chat message to the ring of 256 messages. Kinds 0 and 4 last 600 ticks and the other kinds last 480. `newspaper_addline` stores 64 bytes in a line of the newspaper. `checkfilespec` matches a name against a spec with the wildcard `*`. `building_iswindow` tells if a wall of the floor plan is a window. `addwindowatlas` finds or adds a cell of the window list. The list holds 63 cells.
