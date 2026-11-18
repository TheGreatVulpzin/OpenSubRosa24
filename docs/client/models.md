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
| `addhalfboxtomodel` | Eight vertices and both windings of each triangle, except the selected open face. |
| `addwaisttomodel` | Ten vertices, five quads and six lower triangles. Existing parts remain in the model. |
| `createfootmodel`, `createrotormodel` | Boxes whose centers are behind or below the origin. |
| `createsunglassmodel` | Two tapered lenses with zero texture coordinates. |
| `createcellphonemodel`, `createkeymodel` | A phone with a sloped body and antenna, or a key with a shaft and circular handle. |
| `createheadmodel` | Thirty-eight control points with a lowered jaw, scaled by 0.15 before the surface is built. |
| `tesselatebeziertriangle` | Ten cubic terms, with a float store after each term of each coordinate. The output can overlap the control points. |
| `sethumanmodelvert`, `sethumanmodelvertface` | Set the bone or face reference and position. A face adds sixteen to its index and selects mode two. Padding remains intact. |

## The human model

Each vertex has a bone (`cl_data_bonevert[i].bone`) and a place (`pos`). A vertex with bone 0 follows no bone. The torso builder makes such vertices, and `createhumanmodel` copies their place from the model and lowers it by 0.125. A copy of this record for every triangle corner is in `cl_data_cornervert`.

The torso builder adds 10 vertices, 5 quads and 6 triangles around a center. It does not set the bone of its vertices. `createhumanmodel` reads the bone record of every vertex, so a torso vertex has the bone that its record holds in memory. That value is 0 until a function sets it.

After the model is built, `createhumanmodel` copies it to the 255 models that follow it.

## CSM files

`savemodelcsm` and `loadmodelcsm` first try to enter the `model` directory. They return to the parent only if that change succeeded. A CSM file has a four-byte signature, version one, the vertex count, the render vertex width and the triangle count. Six arrays follow: positions, vertex normals, vertex tangents, triangle indices, triangle normals and triangle neighbors. Each array uses four-byte words and three words per vertex or triangle.

A missing file, another signature or another version makes `loadmodelcsm` return zero. A complete file is copied through the model in work and returns one. The quad and line counts remain as they were. `savemodelcsm` has no result and closes a file after the transfers.
