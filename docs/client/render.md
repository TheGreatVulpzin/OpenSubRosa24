# Rendering

Files: `src/client/render/draw2d.c`, `text.c`, `indicators.c`, `gl_state.c`, `programs.c`, `sprites.c`, `sprites_draw.c`, `effects.c`, `frame_models.c`, `sector_model.c`, `interior_model.c`, `sector_shadow.c`, `screen.c`, `depth.c`, `debug_display.c`, `frame.c`.

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
- `loadsource(path)` (`src/client/render/source.c`) reads a text file into a new block with a zero at its end. The caller frees the block. It adds the path and a line break to the shader log before it opens the file, so the log also lists a file that is missing. The bytes go through `source_buffer` (0x20000 bytes) first: a longer file writes past it. It returns NULL when the file does not open.
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

## Map overlay, player list and head up display

- `drawmap(player)` in `src/client/render/map.c` draws the map texture on a square of 512 pixels with one pixel for each 4 units of the city. It adds the checkpoints of a driving game (green until the viewed player reaches them), a circle for the base of each team (a smaller one inside it when the team has players), the name of each road and the pointer of the player. The picture follows the human of the player and stops at the edge of the city. The key at index 11 of the keyboard table shows twice the area. The number -1 has no human: the function then reads the record before the first human and draws no pointer.
- `drawplayerinfo` in `src/client/render/player_info.c` draws the list of players: one column for each of the three teams and one for the players without a team. Each column has the name of the team, the price of its stock (with the change in percent during a game) and its players in the order of their worth. The worth of a player is the money plus the stocks at the price now. The function stores the worth in the player record. The sort itself is a game function that reads `sort_keys` and writes `sort_order`. The last column has an empty label.
- `drawhud(player)` in `src/client/render/hud.c` draws the display of the viewed player: the map, the help for a phone in the right hand, the labels for the stance, the health, the walk mode and the voice, the items of the two hands (the hand that the left shift key selects is white and larger), the items of the five slots of the pack, the meters of a vehicle and the four aim marks of a gun. A player with no human sees only the map.
- `drawdisplay` in `src/client/render/display.c` draws the rest of the display. With the map page open it draws the stock price of the five teams, and in a game of type 3 or 4 the missions of the team of the viewed player: a text for each kind of mission, the value, the expected rise of the stocks and the time when the mission ends. It also draws the clock of a game of type 4, the timers of a game of type 3, the player list (at the end of a round, in the menu 2 of the wait state, or while the list key is down), the money and the stocks of the viewed player and the chat lines. A chat line fades with its time.

## Interior passes

`src/client/render/sectors.c` draws the interiors of the buildings of one block sector. A sector has four slots. A slot that is in use names an interior mesh. A mesh has vertices of 64 bytes (position, normal, texture coordinates and color), triangles as three indices and parts: each part is a run of triangles with one material.

- `render_sector_interior(sector, mode)` draws the parts whose material has no flag, with the texture of the material. The mode 0 also sets the normal, texture coordinate and color arrays.
- `render_sector_alpha(sector, mode)` draws the parts whose material has the flag (the windows) in the ambient color of the light.
- `render_sector_interior_shadow2(sector)` builds the shadow volume of the triangles that face away from the light: the triangle, the same triangle pushed 16 units away from the light, and a wall for each edge that the light faces. It draws the list as triangles.

A sector is a `BlockSector` record. Its `interior_models` field names the mesh of each slot and `interior_used` tells if the slot is in use. The `block_sectors` array holds the 8192 records.

## Sector list

`src/client/render/sectorlist.c` chooses the sectors of the city to draw and draws their outer blocks.

- `clearsectorlist(mode)` clears the runs of the last frame. A sector within 32 units of the camera is near. A game function marks the interiors in use for the near sectors, and `area_interior_buildsectormodel` builds their meshes. The near sectors with an interior go into a list that is sorted from the nearest. With a mode other than 0, a sector that is outside the view cone and not near is left out. Each remaining sector adds its runs of triangles to the list of its material. A near sector leaves out the materials with windows.
- `render_sectorlist(mode)` draws the runs of each material. The mode 0 draws lit with the shadow map on texture unit 1, the mode 1 draws only the shape for the depth pass and the mode 2 binds the texture of the material only. The vertices are in a buffer when the buffer extension is on. The runs of a material go out in one call when the multi draw extension is on.

A sector is a `BlockSector` record. `sector_entries` holds the sector and the distance of each sector with an interior before the sort, and `sector_list` holds the sectors after it.

## View, texture upload and small vectors

`src/client/render/view.c` sets the view of a pass. Each function ends with the model view matrix active. The three that set a projection also clear the projection, texture and model view matrices.

- `setuptextdisplay` prepares the 2D layer: texture and blending on, depth test and depth writes off, a viewport of the whole window and an orthographic projection with the coordinates of the font (the size in `Font.resolution_x` and `resolution_y`, from the top left corner).
- `setupperspectiveviewport(x, y, width, height, near, far, scale)` sets a viewport and a scissor box for a rectangle in the units of the font, and a perspective projection. The window size scales the rectangle. The scale multiplies the width of the near plane. The height follows the ratio of the rectangle.
- `setuporthoviewport` does the same with an orthographic box of half width, half height and half depth.
- `setupviewpoint(position, rotation)` multiplies the model view matrix by the rotation of the camera (three rows) and moves it by the opposite of the position. A null pointer leaves that part out.
- `setupobjectrender(position, rotation)` moves to the position, then multiplies by the rotation of the object. A null pointer leaves that part out.

`src/client/textures/upload.c` has `setuptexture(texture)`: it binds the texture and gives OpenGL the wrap and filter parameters and the pixels of each level. Each level is half the size of the level before it.

`src/client/math/small_vectors.c` has `copyvector` and `copyvector2d` (copy the 32 bit words, so a value that is not a number keeps its bits), `zerovector` and `flabs` (the absolute value of a float).


## Particles and glass

`src/client/render/sprites.c` moves particles and glass under gravity. The pool holds 16384 records of 148 bytes. Each record has a kind, a life, a position, a velocity and three orientation rows. A glass triangle uses those rows as corners and has one velocity for each corner. Three springs hold its edges near their rest lengths.

- `spritesimulation` moves sparks, stains and glass. Sparks bounce at the level. Stains stop at a hit. A glass shard bounces on a wall and stops on an upward facing surface. A shard that stops clears its life even when the list of landed vertices is full.
- `createwindowsprites` finds the last window with a matching face and projects its four corners through the object's orientation. It starts 81 shards on a nine by nine grid, with random spin and speed from the hit and the object.
- `rendersprites` sorts the particles by depth and draws particles, glass faces, glass edges and short light traces. It also draws the separate lists of landed particles and landed glass.
- `createspritemodel` builds a small box for kind 1 and an empty model for other kinds. Every triangle gets zero texture coordinates.

## Tire strips and muzzle flashes

`src/client/render/effects.c` adds a tire strip when wheel slip exceeds 1/32. Each strip has four positions and four opacity values. The ring has 16384 slots and holds at most 16383 strips. It replaces its oldest strip when full. The wheel saves its position and slip each tick, including ticks with no new strip. The downward level tests adjust each corner's height.

`tiremark_render` draws the ring from its oldest strip. `rendermuzzleflashes` draws sphere level 0 at each active gun with a positive countdown. It stretches vertex 1 less than the other vertices along the barrel.

## Checkpoint strips and item selection

`rendercheckpoints` draws both faces of a strip along one side of each checkpoint box. The strip is 1.5 units high. A checkpoint with an invalid side uses the preceding checkpoint's corners. The client reads the strips at byte 0x25978 of the game record. In this pass, `round_goal` gives the strip count. A nonpositive `round_ticks` draws only the first strip, in red. A positive value draws all strips.

`selectnearitem` selects a free item near the viewed human. It uses a reach segment of 1.5 units from the head. The nearest item within half a unit of the segment becomes `selected_item`. The selected hand must be empty. Without a player or human in view, the selection does not change.

## Frame models

`src/client/render/frame_models.c` draws the sorted frame list. Each `FrameEntry` has a model, flags, four texture indices (the draw uses the first two), a position, three orientation rows, a scale and a color. `frame_addmodel` adds an entry with a scale and a color of 1. A full list (4096 entries) ignores the model. `frame_model_order` selects the entries to draw. Flag 2 hides an entry. Mode 0 binds its color and lightmap textures, mode 1 draws depth, and mode 2 supplies texture coordinates and normals without texture binds. A new lightmap texture binds only when the color texture changes. The world debug view suppresses this pass.

`src/client/render/frame_list.c` (`buildframelist`) fills the frame list each frame: the body, head, hair, sunglasses and feet of each human, each vehicle with its wheels, the rotor of a helicopter, the bullets, the items (a stack of bills gets one model for each bill), the street objects (an elevator gets four door parts) and the lamps and traffic lights in the interiors of the visible sectors. It also fills the window list of the vehicles. Flag 1 casts a shadow, flag 2 hides the entry: the head of the human that the local player views gets flag 3. The list is sorted by the first texture with `csradixsort`. Each call gives `frame_addmodel` two texture numbers and 0 for the last two words.

`src/client/render/frame_textures.c` (`setupframetextures`) makes the five frame textures (color, shadow map, sky, auxiliary and a spare one) at the render size, with no pixels in memory and edge clamping. The sky texture uses no filter. With frame buffers it attaches the color texture and a depth and stencil buffer of the render size to the current frame buffer. It also sets the eight blur weights: cos(i pi / 17) for i = 0 to 7, divided by 0.875 times the sum of the 15 taps. The harness has no OpenGL driver for the frame buffer calls, so this function waits for a test in the game window (`adopted_C10_pending.txt`).

`buildhumanmodel` transforms human model corners through the bones. References below 16 use the corner position. Other references select paired part offsets and scales from the human color vectors. The scale changes local x and y only. The function fills face normals and all three vertex normals after the positions.

`frame_rendermessages` disables culling and depth testing, then enables both states.

## Sector meshes

`src/client/render/sector_model.c` and `interior_model.c` build the meshes of a block sector in the temporary mesh (`tempmesh`) and copy them into the sector or into an interior record.

`area_buildsectormodel(sector)` builds the outer mesh:

- The blocks go in the order y, z, x. A normal block adds a quad for each visible face (bits 0 to 5 of the block word). The faces 1 to 4 take the color of their side from the building part. A preset texture gives the layer in `uv.z`. A sign texture gives the cell of the sign.
- A special block adds the mesh of its block type. A special block that fills more than one cell goes in once: `tempmesh.specials` keeps the places that are in the mesh.
- `tempmesh_optimize` joins the quads. Each quad becomes two triangles with the normal of its first vertex. The vertices that no triangle uses go away: the last vertex moves into the place.
- The sector gets the vertices (`SectorVertex`, 64 bytes), the triangles, the material of each triangle and its normal. Each triangle also gets the triangle on the other side of each edge (`neighbors`) and the normal of that triangle (`edge_normals`), for the shadow volumes.

`area_interior_buildsectormodel(sector, slot)` builds the interior of floors `slot * 2` and `slot * 2 + 1`:

- The result is the interior record, or -1. A slot with nothing to show gets -2 and is not built again. A slot that already has a record keeps it.
- Only special interior blocks add a mesh. Bits 16 to 25 of the block word hide the first faces of the block. A face that the block can hide gets the flagged material. Slots 8 to 15 select the outer textures of the block. Faces with slot 9 to 12 take the color of their side.
- Each city object of the two floors adds a fixture: a lamp or a traffic light with its place and its turn.
- `tempmesh_optimize2` joins the quads. The two triangles of a quad are neighbors on the diagonal. The other neighbors come only from the solid materials (no flags), and only from later triangles.
- A new mesh takes the next record of a ring of 4096 (`interior.count`). The sector that used that record loses it.
- The triangles of the record are in parts, one part for each material, in material order. The neighbor of an edge keeps its index from before the sort.

`area_submitsectormodel(sector)` copies the outer mesh into the sector list: one run of triangles for each material. A run fills one block of 16 vertex and 16 triangle places (`sector_block`), and a new block starts when the vertices or the triangles pass 15. Each corner of a block is one vertex of the block (`tempmesh.block_verts`). With the buffer extension, each block goes to the vertex buffer of the sectors.

## Sector shadow volumes

`src/client/render/sector_shadow.c` builds closed shadow volumes from the sector triangles that face away from the light. Each volume has two caps and up to three sides. The six current corners also occupy a separate array. The extrusion reaches the base of the sector's vertical block range. A light y component from zero through 0.125 gives zero extrusion. A negative y component leaves the vertex list unchanged and leaves the build timer active. A NaN y component clears the list and completes the pass without faces.

The volumes use the outer mesh of `BlockSector` (`num_tris`, `tris`, `tri_normals`, `edge_normals`). The shadow list has room for 262144 positions. The buffer extension uses a three megabyte allocation and uploads the active part of the list. Without that extension, the draw pass reads the list directly. `render_areasectorshadow` draws triangles and reports the vertex count as `shadowverts`.

## Display modes and screenshots

`src/client/render/screen.c` collects display modes and saves the back buffer. `listvideomodes` appends width, height and pixel depth records. It reads only the first 512 SDL modes. A matching width replaces the stored height. A nonzero new height makes that entry a match and preserves its pixel depth. Every call also appends 1024 by 640 by 32, even when that mode is already present.

`screenshot` accepts dimensions up to 1280 by 960. It reads RGBA pixels, writes an 18 byte TGA header, then writes BGR pixels. The filename starts at `scsht000.tga`. Existing files advance the three digits through 999. At index 1000, the first digit becomes a colon. The file handle uses `open_file`, including the value after close.

## Depth and occlusion

`src/client/render/depth.c` draws the opaque depth pass, then the sector and model shadow volumes. Frame entry flag 1 selects a model shadow, including an entry hidden by flag 2. The world debug view suppresses frame models and their shadows. Active shell and debris models also contribute depth.

The stencil mask produces the red channel of the shadow map. Ambient occlusion produces its green channel. Two blur passes use separate horizontal and vertical offsets. Each offset is 1.125 divided by the corresponding render dimension. The shader also receives two sets of four projection parameters.

Framebuffer support selects color attachments. Without that support, the pass copies the viewport to its image textures. The shadow debug key selects a colored volume view and returns before the occlusion and blur passes.

## Diagnostic pages

`src/client/render/debug_display.c` counts active nonbot players even when the diagnostic display is off. A positive display mode draws frame rate, frame time, four switches and voice channel states.

- Mode 1 shows named integer and float values.
- Mode 2 shows sixteen counters and active player names.
- Mode 3 shows the render timers from index 64.
- Mode 4 shows the first sixteen timer totals.
- Mode 5 shows round or connection state, packet sizes, event counts and player count.

The first two numbers use two decimal places. Text rows use fixed advances of 16 or 20 units.

## Frame pass

`src/client/render/frame.c` draws one frame. The order of the passes:

1. Reset the thirteen pass timers. Set the viewport size from the render size, the near plane (0.0625), the far plane (16384) and the view angle (`view_zoom`).
2. Turn the two side directions of the view cone with the camera. Set `world_position` to the camera position times 0.0001 plus 637.1 on y.
3. Choose the sectors (`clearsectorlist`), build the frame list (`buildframelist`) and set the landscape detail (`setupframelandscapelod`).
4. Bind the frame framebuffer when the option allows it. Draw the depth pass (`renderframedepth`). The polygon debug view clears the buffers in place of the depth pass.
5. The shadow debug key stops the frame here. The shadow volumes stay on the screen.
6. Clear the color, point light 0 at the sun and get the sun and sky colors. The two sun angles of the game record give the light direction.
7. Draw the opaque geometry: sectors, landscape, interiors with the shadow map on unit 1, frame models, shells and debris pieces, sprite models and tracks. The polygon debug view draws them as lines and stops after them.
8. Draw the sky (`renderframeatmosphere`), then the alpha pass under the `renderalpha` timer: the sun sphere, tire marks, sector reflections, checkpoint strips, particles, car windows, muzzle flashes and the wire model of the selected item.
9. In a race, draw a sphere at each checkpoint. A checkpoint that the viewed player has passed is grey.
10. Draw the floor buttons of each elevator panel. The current floor is white.
11. The world debug view adds bone boxes, team area boxes, item corners, track triangles, bond lines, debug lines and spheres, wheel cylinders and bullet paths.
12. A name key draws the name of each player within 32 units above the head.

Car windows sort from far to near. The sort key is 16777216 minus 256 times the distance. Each window has a normal, a color of half the ambient light and an alpha of 0.25 for a window that faces the camera, 0.1875 for the other side.

From 0x48 on, the client game record has its fields 4 bytes before the server fields. `ClientGameView` gives the client offsets of the sun angles, the team areas and the race checkpoints.

## The list of particles

`src/client/render/sprite_pool.c` keeps the list of particles. The list holds 16384 particles. `createsprite` returns -1 when it is full. A mark (kind 0) needs a normal in `vel`: the function builds a frame around the normal and moves the mark 1/64 off the surface. Stains, sparks and traces get their life, gravity and drag from their kind: life 65535, 60 and 3 ticks, and drag 0.9975, 0.975 and 0.875. `deletesprite` moves the last particle into the place of the removed one. `spritetimetolive` counts the life of every particle down by one. A life of 65535 does not change, and a particle with a life below 1 is removed.

`raycast_addsector` marks the sector of a block as near the camera and marks one interior slot as in use. Each slot covers two block layers. `reloadopengl` builds the shader programs and the frame textures again and then each texture that has a width. `glstatediffuse` and `glstateambient` set the same OpenGL state and differ in the stencil test: equal to 0, or not equal to 0.

## Rays, areas and sector meshes

`render_raycast2` casts 80 rows of 128 rays from the camera. Each ray takes 48 steps of 1/256 of a block. At each block it marks the sector as near the camera and marks the interior slots that the step reaches. A ray stops at once in a solid block that has no interior.

`area_getblocklink` gives the block word at a place and follows a link to the larger block. `area_getblockmaterial` gives the material of the last face of a block, or -1. `area_sectorvertequal` tells if two vertices of a sector mesh are closer than 1e-4 on each axis. `area_resetsector` makes the models of a used sector again, empties its four interior slots and clears its changed flag.

The sector meshes use a table of 1024 words. Each bit marks a group of 16 places of the mesh buffer. `area_setsectormesh` sets a bit, `area_getnextsectormesh` finds a free group and `area_removesectormodel` frees the meshes of a sector.

The block builders add flat surfaces to the temporary mesh. `area_block_addfloor` and `area_block_addceil` take the height as the 32 bits of a float. `area_block_addfloorceil` adds the floor at 1/16 and the ceiling at 7/8. `area_block_insertquad` opens two places in the list of quads. `createwindowsprites_block` breaks a window face of a block into four triangle pieces. `settwosidedstencil` sets the stencil of the back and front faces, through the EXT extension or else the ATI extension.
