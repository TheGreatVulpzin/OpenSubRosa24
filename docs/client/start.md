# Start of the client

`setupgame` and `setupclient` (`src/client/setup/identifiers.c`, `client_setup.c`) prepare everything that the game draws and plays. `setupgame` gives a number to each model, texture, sound and material. `setupclient` builds the thing behind each number.

## Identifiers

Each list is a record with named fields. A field holds the number of one thing, or an array of numbers.

- `ModelList` (485 numbers): `bullet[32]`, `item[32]`, `wheel[32]`, `human[256]`, `ammo[32]`, `ammo_b[32]`, `city_object[32]`, `sunglasses`, `head`, `foot`, `hair`, `sprite[32]` and `rotor`. The renderer reads `item` with the item type, `wheel` with the wheel type of an object and `human` with the human number.
- `TextureList` (848 numbers): the 256 material textures, the human textures, the paint colors, the street signs and lamps, the panels, the cash bills, the landscape textures and the pictures that come from files. Seven slots have no name yet.
- `SoundList` (47 numbers): the gun shots, the engines, the tires, eight ricochets, the crashes, the bullet hits, the glass, the phone, ten digit tones and the gear. Ten slots have no name yet.
- The material list holds 44 numbers for the preset materials, then one for each picture of the folder `data/texture`.

The numbers go out in a fixed order, so a number always means the same thing.

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

## Quirks

- A model that a builder makes from an item gets the item type as the second argument. The cash builder makes the Magnum, the Uzi and the 9mm, and the bill builder makes the two cash items.
- Material numbers from 256 up have no entry in `TextureList.material`. The folder can add more files than that, and the code then writes and reads past the array.
