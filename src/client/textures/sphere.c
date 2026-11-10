/* The sphere meshes.
 *
 * The client keeps 8 meshes of a unit sphere. Level 0 is an octahedron. Each next level splits
 * every triangle of the level before into 4 and moves the new points onto the sphere. The
 * renderer picks a level by the size of the sphere on the screen.
 */
#include "textures/sphere.h"

#include <string.h>

#include "sr_os.h"
#include "sr_runtime.h"
#include "vectors.h"

#define g_sphere cl_data_sphere

#define SPHERE_LEVELS 8
#define OCTAHEDRON_VERTS 6
#define OCTAHEDRON_TRIS 8

/* The point on the unit sphere between two points. */
static void sphere_midpoint(vec3 *out, const vec3 *a, const vec3 *b)
{
    vec3 middle = *a;
    addvectors(&middle, &middle, b);
    normalizevector(&middle, &middle);
    *out = middle;
}

static void add_tri(SphereLevel *level, int a, int b, int c)
{
    MeshTri *tri = &level->tris[level->num_tris];
    tri->verts[0] = a;
    tri->verts[1] = b;
    tri->verts[2] = c;
    level->num_tris++;
}

/* Make the next level from the level before. It keeps the old vertices and adds one vertex
 * for each edge of each triangle, so two triangles that share an edge get two equal vertices. */
static void split_level(SphereLevel *next, const SphereLevel *before)
{
    int i;

    os_free(next->verts);
    next->verts = os_malloc((size_t)(before->num_verts + before->num_tris * 3) * sizeof(vec3));
    os_free(next->tris);
    next->tris = os_malloc((size_t)before->num_tris * 4 * sizeof(MeshTri));
    next->num_verts = before->num_verts;
    memcpy(next->verts, before->verts, (size_t)before->num_verts * sizeof(vec3));
    next->num_tris = 0;
    for (i = 0; i < before->num_tris; i++) {
        const int *corner = before->tris[i].verts;
        int first = next->num_verts;
        sphere_midpoint(&next->verts[first], &before->verts[corner[0]], &before->verts[corner[1]]);
        sphere_midpoint(&next->verts[first + 1], &before->verts[corner[1]], &before->verts[corner[2]]);
        sphere_midpoint(&next->verts[first + 2], &before->verts[corner[2]], &before->verts[corner[0]]);
        next->num_verts += 3;
        add_tri(next, corner[0], first, first + 2);
        add_tri(next, first + 2, first + 1, corner[2]);
        add_tri(next, first + 2, first, first + 1);
        add_tri(next, first, corner[1], first + 1);
    }
}

/* Make the 8 sphere meshes. */
void __cdecl setupspheremodel(void)
{
    static const vec3 octahedron_verts[OCTAHEDRON_VERTS] = {
        {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
    };
    static const MeshTri octahedron_tris[OCTAHEDRON_TRIS] = {
        {{0, 1, 2}}, {{0, 2, 3}}, {{0, 3, 4}}, {{0, 4, 1}}, {{2, 1, 5}}, {{3, 2, 5}}, {{4, 3, 5}}, {{1, 4, 5}},
    };
    SphereLevel *base = &g_sphere[0];
    int level;

    base->num_verts = OCTAHEDRON_VERTS;
    base->num_tris = OCTAHEDRON_TRIS;
    os_free(base->verts);
    base->verts = os_malloc(sizeof octahedron_verts);
    os_free(base->tris);
    base->tris = os_malloc(sizeof octahedron_tris);
    memcpy(base->verts, octahedron_verts, sizeof octahedron_verts);
    memcpy(base->tris, octahedron_tris, sizeof octahedron_tris);
    for (level = 1; level < SPHERE_LEVELS; level++)
        split_level(&g_sphere[level], &g_sphere[level - 1]);
}
