/* The passes that draw the interiors of the buildings of a block sector: the opaque parts, the
 * parts with windows and the shadow volumes. */
#include "render/sectors.h"

#include "client.h"
#include "setup/timers.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"

#define g_timerlist cl_data_timerlist
#define g_texturelist cl_data_texturelist
#define g_textures cl_data_texture
#define g_materials cl_data_material_records
#define g_interiors ((const InteriorMesh *)&sr_data_interior)
#define g_light_ambient cl_data_light_ambient
#define g_block_sectors cl_data_block_sectors

GL_IMPORT(glColorPointer, (int size, uint32_t type, int stride, const void *pointer), (size, type, stride, pointer))
GL_IMPORT(glNormalPointer, (uint32_t type, int stride, const void *pointer), (type, stride, pointer))
GL_IMPORT(glTexCoordPointer, (int size, uint32_t type, int stride, const void *pointer), (size, type, stride, pointer))
GL_IMPORT(glColor3f, (float red, float green, float blue), (red, green, blue))
GL_IMPORT(glColor3fv, (const float *color), (color))
GL_IMPORT(glDrawArrays, (uint32_t mode, int first, int count), (mode, first, count))

enum {
    SLOTS_PER_SECTOR = 4,
    VERTEX_SIZE = 0x40,
    NORMAL_OFFSET = 0x0c,
    TEXCOORD_OFFSET = 0x24,
    COLOR_OFFSET = 0x2c,
    TRIANGLE_INDEX_SIZE = 12
};

/* The number of the interior in a slot of a sector, or a negative number when the slot has no
 * interior to draw. */
static int sector_interior(int sector, int slot)
{
    const BlockSector *block = &g_block_sectors[sector];

    return block->interior_used[slot] != 0 && -1 < block->interior_models[slot] ? block->interior_models[slot] : -1;
}

/* Point the vertex arrays at the vertices of an interior. The mode 0 also needs the normals and
 * the texture coordinates. */
static void set_arrays(const InteriorMesh *mesh, int mode, int with_colors)
{
    glVertexPointer(3, GL_FLOAT, VERTEX_SIZE, mesh->vertices);
    if (mode != 0)
        return;
    glNormalPointer(GL_FLOAT, VERTEX_SIZE, (const char *)mesh->vertices + NORMAL_OFFSET);
    glTexCoordPointer(2, GL_FLOAT, VERTEX_SIZE, (const char *)mesh->vertices + TEXCOORD_OFFSET);
    if (with_colors)
        glColorPointer(3, GL_FLOAT, VERTEX_SIZE, (const char *)mesh->vertices + COLOR_OFFSET);
}

/* Draw the parts of an interior whose material has the flag set or clear. */
static void draw_parts(const InteriorMesh *mesh, int windows)
{
    int part;

    for (part = 0; part < mesh->num_parts; ++part) {
        int material = mesh->parts[part].material;

        if ((g_materials[material].flags != 0) != windows)
            continue;
        glBindTexture(GL_TEXTURE_2D, g_textures[g_texturelist.material[material]].gl_name);
        glDrawElements(GL_TRIANGLES, mesh->parts[part].num_triangles * 3, GL_UNSIGNED_INT,
                       (const char *)mesh->indices + mesh->parts[part].first_triangle * TRIANGLE_INDEX_SIZE);
    }
}

/* Draw the parts of the interiors of a sector that have no window. The mode 0 sets the normals,
 * the texture coordinates and the colors. */
void __cdecl render_sector_interior(int sector, int mode)
{
    int slot;

    starttimer(g_timerlist.renderinterior);
    for (slot = 0; slot < SLOTS_PER_SECTOR; ++slot) {
        int interior = sector_interior(sector, slot);

        if (interior < 0)
            continue;
        set_arrays(&g_interiors[interior], mode, 1);
        draw_parts(&g_interiors[interior], 0);
    }
    stoptimer(g_timerlist.renderinterior);
}

/* Draw the parts of the interiors of a sector that have windows, in the ambient color of the light. */
void __cdecl render_sector_alpha(int sector, int mode)
{
    int slot;

    starttimer(g_timerlist.renderinteriorwindows);
    glColor3f(1.0f, 1.0f, 1.0f);
    for (slot = 0; slot < SLOTS_PER_SECTOR; ++slot) {
        int interior = sector_interior(sector, slot);

        if (interior < 0)
            continue;
        glColor3fv(g_light_ambient);
        set_arrays(&g_interiors[interior], mode, 0);
        draw_parts(&g_interiors[interior], 1);
    }
    stoptimer(g_timerlist.renderinteriorwindows);
}

/* The dot product of two vectors, returned as a double. */
#define dotproduct_wide CL_FN(0x50b7f0, double(__cdecl *)(const float *a, const float *b))

#define SHADOW_PUSH_LENGTH (-16.0f)

/* Add a point to the list of the shadow volume. */
static void push_point(const vec3 *point)
{
    cl_data_shadow_list[cl_data_shadow_count] = *point;
    ++cl_data_shadow_count;
}

/* Add a triangle to the list of the shadow volume. */
static void push_triangle(const vec3 *a, const vec3 *b, const vec3 *c)
{
    push_point(a);
    push_point(b);
    push_point(c);
}

/* Build the shadow volume of the triangle of an interior that faces away from the light: the
 * triangle, the same triangle pushed away from the light and a side wall for each edge that
 * the light faces. */
static void add_shadow_triangle(const InteriorMesh *mesh, int triangle, const float *light)
{
    vec3 *corner = cl_data_interior_corners;
    const int *index = (const int *)mesh->indices + triangle * 3;
    const vec3 *vertices = (const vec3 *)mesh->vertices;
    int k;

    for (k = 0; k < 3; ++k)
        corner[k] = *(const vec3 *)((const char *)vertices + index[k] * VERTEX_SIZE);
    for (k = 0; k < 3; ++k)
        scaleaddvectors(&corner[k + 3], &corner[k], (const vec3 *)light, SHADOW_PUSH_LENGTH);
    if (0.0 <= dotproduct_wide(&mesh->edge_normals[triangle * 3].x, light)) {
        push_triangle(&corner[1], &corner[0], &corner[3]);
        push_triangle(&corner[1], &corner[3], &corner[4]);
    }
    if (0.0 <= dotproduct_wide(&mesh->edge_normals[triangle * 3 + 1].x, light)) {
        push_triangle(&corner[2], &corner[1], &corner[4]);
        push_triangle(&corner[2], &corner[4], &corner[5]);
    }
    if (0.0 <= dotproduct_wide(&mesh->edge_normals[triangle * 3 + 2].x, light)) {
        push_triangle(&corner[0], &corner[2], &corner[5]);
        push_triangle(&corner[0], &corner[5], &corner[3]);
    }
    push_triangle(&corner[0], &corner[1], &corner[2]);
    push_triangle(&corner[5], &corner[4], &corner[3]);
}

/* Draw the shadow volumes of the interiors of a sector as triangles. */
void __cdecl render_sector_interior_shadow2(int sector)
{
    const float *light = cl_data_light_position;
    int slot;
    int triangle;

    starttimer(g_timerlist.renderinteriorshadow);
    cl_data_shadow_count = 0;
    for (slot = 0; slot < SLOTS_PER_SECTOR; ++slot) {
        int interior = sector_interior(sector, slot);
        const InteriorMesh *mesh;

        if (interior < 0)
            continue;
        mesh = &g_interiors[interior];
        for (triangle = 0; triangle < mesh->num_triangles; ++triangle) {
            if (g_materials[mesh->triangle_material[triangle]].flags != 0)
                continue;
            if (!(dotproduct_wide(&mesh->triangle_plane[triangle].x, light) < 0.0))
                continue;
            add_shadow_triangle(mesh, triangle, light);
        }
    }
    glVertexPointer(3, GL_FLOAT, sizeof(vec3), cl_data_shadow_list);
    glDrawArrays(GL_TRIANGLES, 0, cl_data_shadow_count);
    stoptimer(g_timerlist.renderinteriorshadow);
}
