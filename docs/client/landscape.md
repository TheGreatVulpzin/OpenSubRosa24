# Landscape

File: `src/client/textures/landscape.c`. Header: `landscape.h`.

The landscape is a grid of 2049 x 2049 heights, 8 units apart (`sr_data_landscape`). The renderer splits it into 32 x 32 sectors of 65 x 65 vertices. Each sector has its vertices in an OpenGL buffer (`cl_data_vbo`, `cl_data_landsector_vbo`). The triangles come from index lists that all sectors share: one list for each level of detail, for each level of the neighbor sector and for each of the 4 sides. The level of each sector in a frame is in `cl_data_landsector_lod`.

The start of a game calls `landscape_buildsectorlods` and `landscape_buildmesh`. Each frame calls `landscape_rendersector`.

| Function | Use |
| --- | --- |
| `landscape_setheightf` | Sets the height of the grid point at a position. A point outside the grid, or on its last row or column, changes nothing. |
| `landscape_buildsectorlods` | Makes the index lists. The edge level is never above the level. The old lists stay allocated. |
| `landscape_buildsectormesh` | Fills the vertices of one sector from the heights and sends them to its OpenGL buffer. A vertex past the last height uses the last height. |
| `landscape_buildmesh` | Gives each sector its place and new memory for its vertices, fills all the sectors and makes the texture of the normals. |
| `landscape_rendersector` | Draws one sector: one index list for each side, at the level of the sector in this frame. |
| `landscape_generatenormaltexture` | The texture of the normal of each grid point: x in red, y in green and z, turned around, in blue. Row 0 of the grid is the last row of the texture. |
| `landscape_generatedetail` | The detail texture: a gray noise for each pixel, and in alpha the part of a smooth noise that is above 0.25. |

The functions that call OpenGL extension functions use the pointers that the extension loader fills (`cl_data_gl_bind_buffer`, `cl_data_gl_buffer_data`).
