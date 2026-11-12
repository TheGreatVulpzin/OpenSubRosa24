# Rendering

Files: `src/client/render/draw2d.c`, `text.c`, `indicators.c`, `gl_state.c`, `programs.c`.

## The 2D layer

The screen is a canvas of 1024 by 640 units. A shape has a place and a size. The flags tell how the place relates to the shape:

| Bit | Meaning |
| --- | --- |
| 0 | the shape is centered on x |
| 1 | the right edge is on x |
| 2 | the shape is centered on y |
| 3 | the bottom edge is on y |

- `drawbitmap(texture, x, y, width, height, flags, red, green, blue, alpha)` draws a rectangle with a texture. A texture of -1 draws a plain rectangle.
- `drawbitmaptexcoord` is the same call with four pairs of texture coordinates.
- `drawlinebox` draws the outline of a rectangle with four smooth lines.
- `drawmousecursor(x, y, size, color)` draws the cursor texture of the font in a square of 3 times the size around the point.
- `drawcircle` draws 64 smooth line segments. `drawcirclefill` draws 64 triangles around the center.
- `drawplayerindicator` draws a filled ring whose width changes along the circle. The ring is widest at the start and the end and narrowest in the middle. The last argument is the angle where it starts.
- `drawcircleindicator` draws the compass: a dark disc, a black outline of 64 segments and a grey needle at the angle of the last argument.

## Text

`drawtext(text, x, y, size, flags, red, green, blue, alpha, ...)` writes a text with the letters of the font atlas. The size is the height of a letter and the width of a letter comes from the font. The flags add to the table above:

| Bit | Meaning |
| --- | --- |
| 4 | every letter uses the one fixed glyph of the font |
| 5 | the text has a black shadow one unit down and to the right |
| 6 | the text is plain: the codes below do nothing |
| 7 | the letters are flipped |
| 8 | the text runs from top to bottom |

In a text that is not plain, a slash and a letter ask for the next extra argument:

| Code | Argument | Text |
| --- | --- | --- |
| `//` | none | a slash |
| `/c` | a letter | the letter (nothing for 0) |
| `/i` | a number | the number |
| `/u` | an unsigned number | the number |
| `/x` | an unsigned number | hexadecimal with capitals |
| `/s` | a string | the string |
| `/f` | a double | the number with three decimals |
| `/I` | a number | the number in groups of three digits with commas |
| `/b` | an unsigned number | the 32 bits, in groups of eight with commas |

Any other letter after a slash is dropped with the slash. A text holds at most 1000 letters.

## OpenGL states

- `glstateshadowtwoside`: no color, no depth write, no culling. The stencil counts down on the back faces and up on the front faces. With the extension for two sided stencil it sets both faces in one pass. With the ATI extension it uses the separate operations. With neither, only the base state is set.
- `glstatenormal`: colors on, depth write on, culling on, no stencil.
- `enabletwosidedstencil` and `disabletwosidedstencil` turn the two sided stencil test on and off when the extension exists.

## Shader programs

The renderer has ten programs, each with a vertex file and a fragment file in `shader/`: sector, sector window, reflect, model, shadow, depth, occlusion, atmosphere, blur and landscape.

- `compileprogramobject(index, vertex source, fragment source)` compiles each shader, attaches it and links the program. Each step adds its info log to one text. The result is 1 once the program was linked.
- `setupprogramobjects` reads the files, builds the ten programs and sets their uniform variables: the size of one pixel of the screen, the atmosphere constants and the numbers of the texture units. It writes the text of the logs to `shaderoutput.txt` when the text is not empty.
- The program record holds the places of the uniform variables. The place of a variable that the setup does not look up is 0.

## Models

`src/client/render/models_draw.c` draws the models with vertex arrays. A model keeps its vertices in one float array. A vertex has 3 floats of position, 2 of texture coordinates and 3 of normal, plus extra floats that the stride skips (`vertex_floats * 4` bytes). Every call draws at the place that `setupobjectrender` sets from the placement and the scale.

- `rendermodel(model, placement, scale, mode, textures, color)`: mode 0 draws a lit model. It uses the color texture, the light map texture and the shadow map texture on three texture units and sets the light. Mode 2 uses the color texture only. Another mode draws the shape only.
- `rendermodelwire` draws the shape in a color with half alpha. It then sets both faces to fill.
- `rendermodelnormals` draws the normal of each vertex as a red line and the tangent as a green line, half a unit long.
- `rendersphere(placement, scale, level)` draws one level of the sphere mesh. Each corner has the normal of its vertex.
- `rendertrackmodel(mode)` draws the buffer of the track as quads. Mode 0 uses the model program and the first panel texture.

## Landscape, sky and shadow

- `renderlandscape(mode)` draws each built sector of the landscape. Mode 0 uses the landscape program with the normal map, the detail map and the desert color map. The program gets the light direction from the second uniform slot of its record. The setup fills the first slot, so the second slot is 0.
- `renderframeatmosphere(scale)` draws one quad over the whole screen with the atmosphere program. The program turns each corner into a direction with the rotation of the camera. Depth test and culling are off while it runs.
- `rendermodelshadow(model, placement, rotation)` builds the shadow volume of a model. It turns the light into the axes of the model, pushes the corners of each triangle that faces away from the light 512 units along the light and closes the shape with a side quad on each open edge and a cap at each end. The triangles go to one list that the stencil pass draws.
- `boxinsidefrustum(smallest, largest)` tells if a box is at least partly in the view cone. It tests the eight corners against five planes.

## Map, help and debug shapes

- `rendermap` draws the city from above into the map texture of 512 by 512 pixels. The camera hangs at (1152, 0, 1152) and looks straight down. The map uses a depth only pass of the sector list and copies the picture into the texture.
- `drawhelp` writes the help lines at the left of the screen for the viewed player. The first line always shows. A gun in the right hand and a magazine in the left hand give the lines about loading and unloading. A briefcase gives the lines about opening it and moving items in or out.
- `debug_renderboundingbox(low, high)` draws the twelve edges of a box between two corners in white. `debug_renderbox(placement, rotation, x, y, z)` draws the edges of a box at an object in red.
- `pointinsidefrustum(point)` tells if a point is in the view cone. It is 1 when the point is on the near side of the plane across the view and on the inner side of the four side planes.

## Flying pieces

`src/client/render/sprite_models.c` holds the pieces that fly and land: shell casings (kind 0) and bits of debris (kind 1). A piece has a place, a speed, an orientation of three rows, a spin, a gravity and a drag. The pool has 16384 places.

- `createsprite_model(kind, place, speed, orientation)` starts a piece and gives it a random spin about a mix of two rows of the orientation. The speed may be 0. The result is the number of the piece, or -1 when the pool is full.
- `sprite_modeltimetolive` counts down the life of each piece and frees the ones with no life left. `deletesprite_model(index)` frees a piece: the last piece takes its place.
- `sprite_model_simulation` moves every active piece for one tick. The piece falls, loses speed to drag and turns with its spin. A move that crosses the level makes the piece bounce: the sound of a shell plays when the hit is strong, the speed along the surface falls, and a shell turns flat on the ground. A piece on flat ground that moves slower than 1 in 120 stops. Its corners go into a list of vertices (up to 261632 for shells and 65000 for debris) and the piece is no longer active.
- `renderspritemodels(mode)` draws the two lists. Mode 0 uses the light and the color, light map and shadow map textures.

