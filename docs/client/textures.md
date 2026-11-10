# Textures and noise

Files: `src/client/textures/textures.c`, `noise.c`. Header: `textures.h`, `noise.h`.

The game makes most of its textures when it starts. A generator fills the shared image, copies it into a texture record, and sends the texture to OpenGL with `setuptexture`. No texture comes from a file, except the landscape and the images of the menus.

## Records

- `Image` (`cl_data_image`): `width`, `height` and room for 4096 x 4096 pixels. Every generator writes the size first, then the pixels.
- `Texture` (`cl_data_texture[4096]`, 0x70 bytes each): size of level 0, `num_levels`, format, wrap and filter modes, the OpenGL name and the pixels of each level (`levels[16]`).
- `TextureList` (`cl_data_texturelist`): the number of each texture. `material[256]` has the texture of each material. `human_textures[512]` has the texture of each human. `human[4]`, `paint[8]`, `street[10]` and the other fields have the small plain textures.
- `MaterialList` (`sr_data_materiallist`): the number of the special materials (tiles, streets, the carpet, the sign) and `presets[]`, the four groups of wall materials.

A pixel is one 32 bit word: red in the low byte, then green, blue and alpha. On a big endian machine (`cl_data_bigendian` not zero) the bytes have the other order. `pack_pixel` makes the word.

## Generators

| Function | Texture |
| --- | --- |
| `generatemipmap` | Makes the smaller levels of a texture. Each pixel is the mean of 4 pixels of the level above. Only RGB and RGBA textures get levels. |
| `generatetexture` | One of the small plain textures of 128 x 128: signs, lamps, panels, discs, paints and the four human parts. A number that is in no list gives faint magenta. |
| `generatetexture_randomvec` | 64 x 8 random unit vectors, x y z in red green blue. |
| `generatetexture_carpet` | Light diamonds on a dark red ground. A sine of four noise layers changes the brightness. |
| `generatetexture_billboard`, `generatetexture_newspaper` | Drawn with the text and bitmap functions on a page of 512 x 512 units. OpenGL copies the screen into the texture. The newspaper draws the page 4 times, each time moved by half a page. |
| `generatematerialtexture` | A material texture of 128 x 128: walls, floors, streets, ceilings and signs. The sign and the carpet call their own generator. |
| `generatematerialwindowtexture` | The window texture of 2048 x 2048: 16 x 4 cells of 128 x 128. A cell is the second level of a material texture with the lit rectangle of a window drawn over it. |
| `generatehumantexture` | The texture of one human, from its team, skin, hair, eye and suit values. |

### Material textures

`generatematerialtexture` starts with magenta. A wall group (0 to 3 of `presets`) then gives a ground of four noise layers, with joints (group 2) or mortar (group 1) that darken the color. After that the checks for single materials run in a fixed order, and a later check replaces an earlier one. The streets (a range of four materials) and the materials 3 to 9 use the noise of the grid point (`noise2d`) and add a grey line near one edge.

A line of joints can darken a pixel more than once: each kind of joint that the pixel is on halves the color again.

### Window texture

The window texture reads the second level of the source textures, so a source texture needs `levels[1]`. A source with no second level leaves the color of the pixel before. A window with `clear` not zero is drawn as grey glass with alpha 32. Any other window gets alpha 255.

### Human texture

The game keeps the five look values of the last picture of each human (`cl_data_texture_human`). `generatehumantexture` returns at once when none of them changed. The pixel memory of the texture is made once and then reused. A skin or hair value outside its table gives black.

The left half is the suit, with a badge and ties in the team color. The right half is skin (top), hair (middle) and an eye (bottom).

## Float to int conversion

`cl_ftoi` (`src/client/client.h`) converts a float to an int with a 32 bit result. A value that is NaN, infinite, or outside the range of an int gives 0x80000000. The texture generators use it for every pixel value and index.

## Noise

- `noise1d(n)`, `noise2d(x, y)`, `noise3d(x, y, z)`: a fixed value from -1 to 1 for an integer or a grid point.
- `serp`, `cerp`: blend two values with a smooth weight (a sine and a cosine curve).
- `lerpnoise2d`, `serpnoise2d`, `cerpnoise2d` (in `src/common/random.c`): the noise at a point between grid points. These functions convert the coordinate to an int with a 64 bit result. A coordinate that is NaN, infinite or outside the range of an int picks another grid cell than `cl_ftoi` would. The texture generators never pass such a value.
- `hermiteinterpolation`, `engine_bezierinterpolation`: a point on a curve.
