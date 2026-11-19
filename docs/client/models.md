# Models

Files: `src/client/textures/models.c`, `sphere.c`. Header: `models.h`, `sphere.h`.

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
| `createheadmodel3` (`head_models.c`) | The simple head: eight points, each triangle split into four along Hermite curves, and two eye quads. |

## The human model

Each vertex has a bone (`cl_data_bonevert[i].bone`) and a place (`pos`). A vertex with bone 0 follows no bone. The torso builder makes such vertices, and `createhumanmodel` copies their place from the model and lowers it by 0.125. A copy of this record for every triangle corner is in `cl_data_cornervert`.

The torso builder adds 10 vertices, 5 quads and 6 triangles around a center. It does not set the bone of its vertices. `createhumanmodel` reads the bone record of every vertex, so a torso vertex has the bone that its record holds in memory. That value is 0 until a function sets it.

After the model is built, `createhumanmodel` copies it to the 255 models that follow it.

## Edits of the model in work

`src/client/textures/mesh_edit.c` holds the small edits. `model_deletequad` removes a quad of the model in work: the last quad takes its place, with its texture corners. `tempmesh_deletequad` does the same in the temporary mesh and moves the slot and the normal of the quad too. An index outside the list changes nothing.

`model_addvertex` returns the index of a vertex that is closer than the tolerance on all three axes, or adds a new vertex. It does the same as `modeltemp_addvertex`. `tempmesh_vertnormalequal` tells if two vertices of the temporary mesh have the same place, normal and color (each axis closer than 1e-4) and the same third texture value. `addboxtomodelquad` adds 8 vertices and 6 quads. The sizes are the full lengths of the sides. `lerp` mixes two values: `b * t + (1 - t) * a`.

`model_createcash` builds the cash bundle: a box with the given half sizes and fixed texture corners on triangles 0 and 1, and on triangles 10 and 11. All other triangles get zero texture corners.
