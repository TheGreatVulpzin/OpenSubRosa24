/* Small edits of the model in work and of the temporary mesh: remove a quad, add a vertex that
 * is not there yet, and compare two mesh vertices.
 */
#include "textures/mesh_edit.h"

#include <math.h>

#include "client.h"
#include "sr_runtime.h"
#include "textures/models.h"
#include "vectors.h"

#define g_temp cl_data_modeltemp
#define g_tempmesh sr_data_tempmesh
#define g_cubequad sr_data_cubequad /* the 4 corners of each of the 6 faces of a cube */

#define MESH_SAME_DISTANCE 9.999999747378752E-5f /* 1e-4 as a float */

/* Remove one quad of the model in work. The last quad takes its place. An index outside the
 * list changes nothing. */
void __cdecl model_deletequad(int quad)
{
    if (quad > -1 && quad < g_temp.num_quads) {
        g_temp.num_quads--;
        g_temp.quads[quad] = g_temp.quads[g_temp.num_quads];
        g_temp.quad_texcoords[quad] = g_temp.quad_texcoords[g_temp.num_quads];
    }
}

/* Add a vertex to the model in work and return its index. A vertex whose three coordinates are
 * all closer than tolerance to the new place is used again. */
int __cdecl model_addvertex(const vec3 *place, float tolerance)
{
    return modeltemp_addvertex(place, tolerance);
}

/* Remove one quad of the temporary mesh. The last quad takes its place, with its slot and its
 * normal. An index outside the list changes nothing. */
void __cdecl tempmesh_deletequad(int quad)
{
    if (quad > -1 && quad < g_tempmesh.num_quads) {
        g_tempmesh.num_quads--;
        g_tempmesh.quads[quad] = g_tempmesh.quads[g_tempmesh.num_quads];
        g_tempmesh.quad_slots[quad] = g_tempmesh.quad_slots[g_tempmesh.num_quads];
        g_tempmesh.quad_normals[quad] = g_tempmesh.quad_normals[g_tempmesh.num_quads];
    }
}

/* Tell if two vertices of the temporary mesh are the same: place, normal and color closer than
 * 1e-4 on each axis, and an equal third texture value. NaN gives no. */
int __cdecl tempmesh_vertnormalequal(int a, int b)
{
    const vec3 *va = &g_tempmesh.verts[a], *vb = &g_tempmesh.verts[b];
    const vec3 *na = &g_tempmesh.normals[a], *nb = &g_tempmesh.normals[b];
    const vec3 *ca = &g_tempmesh.colors[a], *cb = &g_tempmesh.colors[b];

    return (float)fabsf((float)(va->x - vb->x)) < MESH_SAME_DISTANCE &&
           (float)fabsf((float)(va->y - vb->y)) < MESH_SAME_DISTANCE &&
           (float)fabsf((float)(va->z - vb->z)) < MESH_SAME_DISTANCE &&
           (float)fabsf((float)(na->x - nb->x)) < MESH_SAME_DISTANCE &&
           (float)fabsf((float)(na->y - nb->y)) < MESH_SAME_DISTANCE &&
           (float)fabsf((float)(na->z - nb->z)) < MESH_SAME_DISTANCE &&
           (float)fabsf((float)(ca->x - cb->x)) < MESH_SAME_DISTANCE &&
           (float)fabsf((float)(ca->y - cb->y)) < MESH_SAME_DISTANCE &&
           (float)fabsf((float)(ca->z - cb->z)) < MESH_SAME_DISTANCE &&
           g_tempmesh.uvs[b].z == g_tempmesh.uvs[a].z;
}

/* Blend two values: a at t = 0 and b at t = 1. */
float __cdecl lerp(float a, float b, float t)
{
    return (float)(b * t + (1.0 - t) * a);
}

/* Add a box with 8 vertices and 6 quads to the model in work. The sizes are the full lengths of
 * the sides. The vertex i is at the minus side of x when its low two bits are 0 or 3, at the
 * minus side of y when i is below 4, and at the minus side of z when its low two bits are 0 or 1. */
void __cdecl addboxtomodelquad(const vec3 *center, float size_x, float size_y, float size_z)
{
    int first = g_temp.num_verts;
    int i, face, k;

    for (i = 0; i < 8; i++) {
        vec3 corner;

        corner.x = (i & 3) == 0 || (i & 3) == 3 ? -size_x : size_x;
        corner.x = (float)(corner.x * 0.5);
        corner.y = i < 4 ? -size_y : size_y;
        corner.y = (float)(corner.y * 0.5);
        corner.z = (i & 3) == 0 || (i & 3) == 1 ? -size_z : size_z;
        corner.z = (float)(corner.z * 0.5);
        addvectors(&corner, &corner, center);
        g_temp.verts[g_temp.num_verts] = corner;
        g_temp.num_verts++;
    }
    for (face = 0; face < 6; face++) {
        for (k = 0; k < 4; k++)
            g_temp.quads[g_temp.num_quads].verts[k] = first + g_cubequad[face * 4 + k];
        g_temp.num_quads++;
    }
}
