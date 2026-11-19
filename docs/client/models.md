# Models

Files: `src/client/textures/models.c`, `model_parts.c`, `head_models.c`, `tempmesh.c`, `sphere.c`. Header: `models.h`, `model_parts.h`, `head_models.h`, `tempmesh.h`, `sphere.h`.

## The model in work

`cl_data_modeltemp` (`ModelTemp`, 0x270014 bytes) holds one model while a builder fills it: vertices, triangles with a texture coordinate for each corner and a shading mode, quads (faces that a builder can pull out of the surface) and lines. A builder clears the counts, adds parts, and calls `setupmodel` (normals and render vertices) and `copymodel` to store the result as a game model (`cl_data_model[1024]`).

Shading modes of a triangle: -2 every corner uses the normal of the triangle, -1 each corner uses the normal of its vertex.

| Function | Use |
| --- | --- |
| `modeltempvertsequal`, `modeltemp_addvertex` | Two vertices are the same point when each axis is closer than a tolerance. `modeltemp_addvertex` adds a vertex only when there is no equal one. |
| `copymodel` | Gives the game model its own copy of each array. |
| `addboxtomodel` | A box: 8 vertices and 12 flat triangles. |
| `model_settritexcoord`, `model_setquadtexcoord` | One texture coordinate for the corners of a triangle or a quad. |
| `model_extrudequad`, `model_extrudequad2`, `model_extrudequadscale` | Pull a quad out of the surface by an offset. They add 4 vertices and 4 side quads. |
| `createmodel_cityobject`, `_type`, `_elevator` | The models of the city objects (elevator, door, computer, traffic light). |
| `setupspheremodel` | 8 sphere meshes. Level 0 is an octahedron. Each level splits every triangle of the level before into 4. |
| `createhairmodel` | The hair: an octahedron of radius 0.1275 with each triangle split into 4. The points at the front and below the middle are stretched. |
| `createhumanmodel` | The human: a torso that grows by four extrusions, then ten boxes (the head and the limbs), then every quad as two triangles. |
| `createheadmodel2` (`head_models.c`) | A head of eight cubic Bezier triangles. The control points are the edit vertices. Each triangle splits two times into four. A mark (patch corners and weights) finds each point of one area of the face. Hand placed triangles and two eye extrusions replace that area. Shift changes the weights of two points. No code of the client calls it. |
| `model_extrudeeye2` (`model_parts.c`) | The eye of the Bezier head: as `model_extrudeeye`, with a ring 1.5 times as far from the middle. A ring vertex gets 0.875 times its offset as its texture coordinate. Only the middle vertex follows the face bone. |
| `model_extrudequadface` (`model_parts.c`) | A face on a quad of the head: three rows of 4 vertices (left eye, right eye, mouth), 12 flat triangles and 6 quads around them. The mouth goes 1/32 into the head. Each eye quad gets `model_extrudeeye`. `head_face_quads` keeps the quads of the eyes and the mouth. |
| `createmodel_cityobject_desk` (`models.c`) | The desk: a top and two legs, each a box, all with texture coordinate 0, 0. |
| `createheadmodel3` (`head_models.c`) | The simple head: eight points, each triangle split into four along Hermite curves, and two eye quads. |

## The human model

Each vertex has a bone (`cl_data_bonevert[i].bone`) and a place (`pos`). A vertex with bone 0 follows no bone. The torso builder makes such vertices, and `createhumanmodel` copies their place from the model and lowers it by 0.125. A copy of this record for every triangle corner is in `cl_data_cornervert`.

The torso builder adds 10 vertices, 5 quads and 6 triangles around a center. It does not set the bone of its vertices. `createhumanmodel` reads the bone record of every vertex, so a torso vertex has the bone that its record holds in memory. That value is 0 until a function sets it.

After the model is built, `createhumanmodel` copies it to the 255 models that follow it.

## Edits of the model in work

`src/client/textures/mesh_edit.c` holds the small edits. `model_deletequad` removes a quad of the model in work: the last quad takes its place, with its texture corners. `tempmesh_deletequad` does the same in the temporary mesh and moves the slot and the normal of the quad too. An index outside the list changes nothing.

`model_addvertex` returns the index of a vertex that is closer than the tolerance on all three axes, or adds a new vertex. It does the same as `modeltemp_addvertex`. `tempmesh_vertnormalequal` tells if two vertices of the temporary mesh have the same place, normal and color (each axis closer than 1e-4) and the same third texture value. `addboxtomodelquad` adds 8 vertices and 6 quads. The sizes are the full lengths of the sides. `lerp` mixes two values: `b * t + (1 - t) * a`.

`model_createcash` builds the cash bundle: a box with the given half sizes and fixed texture corners on triangles 0 and 1, and on triangles 10 and 11. All other triangles get zero texture corners.

## Quad joins of the temporary mesh

`src/client/textures/tempmesh.c` joins the quads of the temporary mesh (`tempmesh`) before the sector meshes split them into triangles.

- Two quads join when a side of one has the same two corners as a side of the other (same key, place and normal), they have the same material, their normals differ by less than 0.0001 and the sides past the shared side go on in one line (cosine at least 0.99975). The first quad takes the far corners of the second. The texture coordinates of the new corners keep the scale of the first quad. The second quad goes away.
- `tempmesh_optimize` (outer mesh) joins along side 1 to 2 until a scan finds no join, then along side 2 to 3. It never joins a quad with the sign material.
- `tempmesh_optimize2` (interior mesh) does the same, with a done mark for each quad (`tempmesh.quad_done`). A quad with a window material starts done. A quad that finds no partner in a scan gets done. A scan runs again only after a join with an earlier quad. The scan does not compare a quad with itself, so a quad can join itself. The timer 3 ("optimize2") counts the joins.
