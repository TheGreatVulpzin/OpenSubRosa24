/* The mesh and the textures of the landscape.
 *
 * The landscape is a grid of 2049 x 2049 heights, 8 units apart. The renderer splits it into
 * 32 x 32 sectors of 65 x 65 vertices. Each sector has its vertices in an OpenGL buffer. The
 * triangles come from index lists that all sectors share: one list for each level of detail,
 * for each level of the neighbor sector and for each of the 4 sides. The start of a game calls
 * landscape_buildsectorlods and landscape_buildmesh. The frame calls landscape_rendersector.
 */
#include "textures/landscape.h"

#include <string.h>

#include "client.h"
#include "random.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/noise.h"
#include "textures/opengl.h"
#include "textures/textures.h"
#include "vectors.h"

#define g_landscape sr_data_landscape
#define g_render cl_data_landrender
#define g_sector_vbo cl_data_landsector_vbo
#define g_vbo cl_data_vbo
#define g_sector_lod cl_data_landsector_lod
#define g_textures cl_data_texture
#define g_texturelist cl_data_texturelist
#define g_image cl_data_image
#define glBindBufferARB ((GlBindBufferFn)cl_data_gl_bind_buffer)
#define glBufferDataARB ((GlBufferDataFn)cl_data_gl_buffer_data)

#define GRID_CELLS 2048               /* cells of the landscape on each axis */
#define GRID_ROW (GRID_CELLS + 1)     /* heights in one row */
#define CELL_SIZE 8.0                 /* world units between two heights */
#define SECTORS_PER_SIDE 32
#define NUM_SECTORS (SECTORS_PER_SIDE * SECTORS_PER_SIDE)
#define SECTOR_CELLS 64               /* cells of a sector on each axis */
#define SECTOR_ROW (SECTOR_CELLS + 1) /* vertices in one row of a sector */
#define SECTOR_CENTER (SECTOR_CELLS / 2)
#define SECTOR_VERTS_BYTES (SECTOR_ROW * SECTOR_ROW * (int)sizeof(vec3))

#define NUM_LEVELS 7     /* levels of detail that have index lists: a step of 1 to 64 cells */
#define MAX_LEVELS 16    /* room in the table of index lists */
#define NUM_SIDES 4
#define BUILT_SIDE 2     /* the side that the lists are made for. The others are turns of it */

#define NORMAL_TEXTURE_SIZE 0x800
#define DETAIL_TEXTURE_SIZE 0x400

/* Game functions of other files. */
#define setuptexture CL_FN(0x50ac40, void(__cdecl *)(int texture))

/* The index list of a level, an edge level and a side. */
static LandIndexList *index_list(int level, int edge_level, int side)
{
    return &g_render.lists[(level * MAX_LEVELS + edge_level) * NUM_SIDES + side];
}

/* The index of a vertex of a sector. */
static int sector_vertex(int row, int column)
{
    return row * SECTOR_ROW + column;
}

static void add_temp_tri(int a, int b, int c)
{
    MeshTri *tri = &g_render.temp_tris[g_render.num_temp_tris];
    tri->verts[0] = a;
    tri->verts[1] = b;
    tri->verts[2] = c;
    g_render.num_temp_tris++;
}

/* Store a copy of the list in work as the index list of a level, an edge level and a side. */
static void store_temp_list(int level, int edge_level, int side)
{
    LandIndexList *list = index_list(level, edge_level, side);
    int bytes = g_render.num_temp_tris * (int)sizeof(MeshTri);
    list->num_tris = g_render.num_temp_tris;
    list->tris = os_malloc((size_t)bytes);
    memcpy(list->tris, g_render.temp_tris, (size_t)bytes);
}

/* Fill the list in work with one quarter of a sector: the triangle from the center to one
 * side. The inner triangles have a step of 1 << level cells. The last row meets the neighbor
 * sector with a step of 1 << edge_level cells. */
static void build_quarter(int level, int edge_level)
{
    int step = 1 << level;
    int edge_step = 1 << edge_level;
    int rows = 0;
    int row, middle, column;

    g_render.num_temp_tris = 0;
    for (row = SECTOR_CENTER; row + step < SECTOR_CELLS; row += step) {
        for (middle = SECTOR_CENTER - step * rows; middle <= step * rows + SECTOR_CENTER; middle += step * 2) {
            for (column = middle - step; column < middle + step; column += step) {
                add_temp_tri(sector_vertex(row, middle), sector_vertex(row + step, column + step),
                             sector_vertex(row + step, column));
                add_temp_tri(sector_vertex(row + step, column), sector_vertex(row + step, column + step),
                             sector_vertex(row + step * 2, middle));
            }
        }
        rows++;
    }
    if (step > SECTOR_CENTER)
        step = SECTOR_CENTER;
    for (middle = SECTOR_CENTER - step * rows; middle <= step * rows + SECTOR_CENTER; middle += step * 2) {
        for (column = middle - step; column < middle + step; column += edge_step) {
            add_temp_tri(sector_vertex(row, middle), sector_vertex(row + step, column + edge_step),
                         sector_vertex(row + step, column));
        }
    }
}

/* Turn each vertex of the list in work from the built side to another side. */
static void turn_temp_list(int side)
{
    int *index = g_render.temp_tris[0].verts;
    int count = g_render.num_temp_tris * 3;
    int i;

    for (i = 0; i < count; i++) {
        int column = (int)((uint32_t)index[i] % SECTOR_ROW);
        int row = (int)((uint32_t)index[i] / SECTOR_ROW);
        if (side == 0)
            index[i] = sector_vertex(SECTOR_CELLS - row, SECTOR_CELLS - column);
        else if (side == 1)
            index[i] = sector_vertex(SECTOR_CELLS - column, row);
        else
            index[i] = sector_vertex(column, SECTOR_CELLS - row);
    }
}

/* Make the index lists of the sectors. The edge level is never above the level. The old
 * lists stay allocated. */
void __cdecl landscape_buildsectorlods(void)
{
    int side, level, edge_level;

    for (edge_level = 0; edge_level < NUM_LEVELS; edge_level++) {
        for (level = edge_level; level < NUM_LEVELS; level++) {
            build_quarter(level, edge_level);
            store_temp_list(level, edge_level, BUILT_SIDE);
        }
    }
    for (side = 0; side < NUM_SIDES; side++) {
        if (side == BUILT_SIDE)
            continue;
        for (edge_level = 0; edge_level < NUM_LEVELS; edge_level++) {
            for (level = edge_level; level < NUM_LEVELS; level++) {
                const LandIndexList *built = index_list(level, edge_level, BUILT_SIDE);
                g_render.num_temp_tris = built->num_tris;
                memcpy(g_render.temp_tris, built->tris, (size_t)built->num_tris * sizeof(MeshTri));
                turn_temp_list(side);
                store_temp_list(level, edge_level, side);
            }
        }
    }
}

/* Fill the vertices of a sector from the heights and send them to its OpenGL buffer. A vertex
 * past the last height uses the last height. */
void __cdecl landscape_buildsectormesh(int sector)
{
    const LandSector *s = &g_render.sectors[sector];
    int row, column;

    for (row = 0; row < SECTOR_ROW; row++) {
        for (column = 0; column < SECTOR_ROW; column++) {
            vec3 *vert = &s->verts[sector_vertex(row, column)];
            int grid_x = s->x * SECTOR_CELLS + column;
            int grid_z = s->z * SECTOR_CELLS + row;
            if (grid_x > GRID_CELLS)
                grid_x = GRID_CELLS;
            if (grid_z > GRID_CELLS)
                grid_z = GRID_CELLS;
            vert->x = (float)(column * CELL_SIZE + (s->x * 64.0 * CELL_SIZE + g_landscape.origin.x));
            vert->z = (float)(row * CELL_SIZE + (s->z * 64.0 * CELL_SIZE + g_landscape.origin.z));
            vert->y = g_landscape.heights[grid_z * GRID_ROW + grid_x];
        }
    }
    glBindBufferARB(GL_ARRAY_BUFFER, g_vbo[g_sector_vbo[sector]].gl_name);
    glBufferDataARB(GL_ARRAY_BUFFER, SECTOR_VERTS_BYTES, s->verts, GL_STATIC_DRAW);
    glBindBufferARB(GL_ARRAY_BUFFER, 0);
}

/* Give each sector its place and new memory for its vertices, fill all the sectors and make
 * the texture of the normals. */
void __cdecl landscape_buildmesh(void)
{
    int x, z, sector;

    for (z = 0; z < SECTORS_PER_SIDE; z++) {
        for (x = 0; x < SECTORS_PER_SIDE; x++) {
            LandSector *s = &g_render.sectors[z * SECTORS_PER_SIDE + x];
            s->x = x;
            s->y = 0;
            s->z = z;
            os_free(s->verts);
            s->verts = os_malloc(SECTOR_VERTS_BYTES);
        }
    }
    for (sector = 0; sector < NUM_SECTORS; sector++)
        landscape_buildsectormesh(sector);
    landscape_generatenormaltexture();
}

/* Set the height of the grid point at pos. A point outside the grid, or on its last row or
 * column, changes nothing. */
void __cdecl landscape_setheightf(const vec3 *pos, float height)
{
    int x = (int)(long long)((pos->x - g_landscape.origin.x) / CELL_SIZE);
    int z = (int)(long long)((pos->z - g_landscape.origin.z) / CELL_SIZE);
    if (x >= 0 && x < GRID_CELLS && z >= 0 && z < GRID_CELLS)
        g_landscape.heights[z * GRID_ROW + x] = height;
}

/* Draw a sector: one index list for each side, at the level of the sector in this frame. */
void __cdecl landscape_rendersector(int sector)
{
    const LandSectorLod *lod = &g_sector_lod[sector];
    int level = lod->level;
    int side;

    glBindBufferARB(GL_ARRAY_BUFFER, g_vbo[g_sector_vbo[sector]].gl_name);
    glVertexPointer(3, GL_FLOAT, (int)sizeof(vec3), 0);
    for (side = 0; side < NUM_SIDES; side++) {
        const LandIndexList *list = index_list(level, lod->edge_level[side], side);
        glDrawElements(GL_TRIANGLES, list->num_tris * 3, GL_UNSIGNED_INT, list->tris);
    }
}

/* Make the texture that holds the normal of each grid point: x in red, y in green and z,
 * turned around, in blue. The slope comes from the two heights next to the point. A point on
 * the border has no slope on that axis. Row 0 of the grid is the last row of the texture. */
void __cdecl landscape_generatenormaltexture(void)
{
    int texture = g_texturelist.landscape_normal;
    Texture *t = &g_textures[texture];
    const float *heights = g_landscape.heights;
    int x, z;

    t->width = NORMAL_TEXTURE_SIZE;
    t->height = NORMAL_TEXTURE_SIZE;
    t->num_levels = 1;
    t->format = GL_RGBA;
    t->internal_format = GL_RGBA;
    t->spare = 0;
    t->wrap_s = GL_CLAMP_TO_EDGE;
    t->wrap_t = GL_CLAMP_TO_EDGE;
    t->mag_filter = GL_LINEAR;
    t->min_filter = GL_LINEAR_MIPMAP_LINEAR;
    os_free(t->levels[0]);
    t->levels[0] = os_malloc((size_t)(t->width * t->height * 4));
    for (z = 0; z < t->height; z++) {
        uint32_t *out = &t->levels[0][(t->height - z - 1) * t->width];
        for (x = 0; x < t->width; x++) {
            vec3 normal = {0.0f, 1.0f, 0.0f};
            int red, green, blue;
            if (x > 0 && x < GRID_CELLS)
                normal.x = (float)((heights[z * GRID_ROW + x - 1] - heights[z * GRID_ROW + x + 1]) * 0.5);
            if (z > 0 && z < GRID_CELLS)
                normal.z = (float)((heights[(z + 1) * GRID_ROW + x] - heights[(z - 1) * GRID_ROW + x]) * 0.5);
            normalizevector(&normal, &normal);
            red = (int)(long long)(normal.x * 127.0 + 128.0);
            green = (int)(long long)(normal.y * 127.0 + 128.0);
            blue = (int)(long long)(128.0 - normal.z * 127.0);
            out[x] = (uint32_t)green * 0x100u + (uint32_t)blue * 0x10000u + 0xff000000u + (uint32_t)red;
        }
    }
    generatemipmap(texture);
    setuptexture(texture);
}

/* Make the detail texture of the landscape: a gray noise for each pixel, and in alpha the part
 * of a smooth noise that is above 0.25. */
void __cdecl landscape_generatedetail(int mipmap, int wrap_s, int wrap_t, int mag_filter, int min_filter)
{
    int texture = g_texturelist.landscape_detail;
    int x, y;

    g_image.width = DETAIL_TEXTURE_SIZE;
    g_image.height = DETAIL_TEXTURE_SIZE;
    for (y = 0; y < g_image.height; y++) {
        for (x = 0; x < g_image.width; x++) {
            int gray = (int)(long long)(noise2d(x, y) * 16.0 + 192.0);
            float patch = cerpnoise2d((float)x, (float)y, DETAIL_TEXTURE_SIZE - 1);
            int alpha = 0;
            if (patch > 0.25)
                alpha = (int)(long long)((patch - 0.25) * 128.0);
            g_image.pixels[y * g_image.width + x] = pack_pixel(gray, gray, gray, alpha);
        }
    }
    texture_from_image(texture, wrap_s, wrap_t, mag_filter, min_filter);
    if (mipmap != 0)
        generatemipmap(texture);
    setuptexture(texture);
}
