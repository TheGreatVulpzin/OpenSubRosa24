/* The interior mesh of two floors of a sector.
 *
 * area_interior_buildsectormodel collects the special interior blocks of two floors in the
 * temporary mesh and adds a fixture for each city object of these floors. It then joins the
 * quads, splits them into triangles, removes the vertices that no triangle uses, finds the
 * triangle on the other side of each edge and copies the result into the next free interior
 * record. The triangles of the record are sorted by material.
 */
#include "render/interior_model.h"

#include <stdint.h>
#include <string.h>

#include "client.h"
#include "debug/debug.h"
#include "setup/timers.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "vectors.h"

#define g_tempmesh sr_data_tempmesh
#define g_area sr_data_area
#define g_area_block sr_data_area_block
#define g_buildingtype sr_data_buildingtype
#define g_materiallist sr_data_materiallist
#define g_materials cl_data_material_records
#define g_block_sectors cl_data_block_sectors
#define g_cityobjecttype sr_data_cityobjecttype
#define g_interior_store sr_data_interior
#define g_interiors ((InteriorMesh *)&sr_data_interior)
#define g_timers cl_data_timerlist
#define g_debug sr_data_debug
#define g_interior_model_count cl_data_interior_model_count
#define g_yaxis sr_data_yaxis

/* Game functions of other files. */
#define tempmesh_optimize2 CL_FN(0x44cd90, void(__cdecl *)(void))
#define tempmesh_vertnormalequal CL_FN(0x50b290, int(__cdecl *)(int a, int b))

#define NUM_SECTORS 0x2000
#define MAX_MODELS 0x1000
#define NO_INTERIOR (-2)
#define SECTOR_SIDE 8
#define BLOCK_KIND_MASK 0xe0000000u
#define BLOCK_SPECIAL 0x80000000u
#define BLOCK_TYPE_MASK 0xffff
#define FACE_TEXTURES 8
#define OUTER_SLOT 8      /* slots 8 to 15 select the outer face textures of the block */
#define NUM_MATERIALS 256
#define SORT_TIMER 4

/* The place of a block in the two floors and its index in the block lists. */
typedef struct FloorBlock {
    int x, y, z;
    int index;
} FloorBlock;

/* x, y and z of the vertex without the fraction in bits 0, 8 and 16. The normal gives bits 24 to
 * 29: 2 bits for each axis, the axis value plus 1. */
static int interior_key(const vec3 *v, const vec3 *normal)
{
    uint32_t key = (uint32_t)cl_ftoi(v->x);

    key |= (uint32_t)cl_ftoi(v->y) << 8;
    key |= (uint32_t)cl_ftoi(v->z) << 16;
    key |= (uint32_t)cl_ftoi(normal->x + 1.0) << 24;
    key |= (uint32_t)cl_ftoi(normal->y + 1.0) << 26;
    key |= (uint32_t)cl_ftoi(normal->z + 1.0) << 28;
    return (int)key;
}

/* Add the mesh of a special interior block. Bits 16 to 25 of the block word hide the first faces
 * of the block. A face that the block can hide gets the flagged material. Faces with slot 9 to 12
 * take the color of that side. A loose triangle with an outer slot gets material 0. */
static void add_block_mesh(const BlockSector *s, uint32_t word, const FloorBlock *b,
                           const vec3 *offset, const Area *area)
{
    TempMesh *tm = &g_tempmesh;
    const AreaBlock *block = &g_area_block[word & BLOCK_TYPE_MASK];
    const vec3 *verts = (const vec3 *)(uintptr_t)block->verts;
    const vec3 *normals = (const vec3 *)(uintptr_t)block->normals;
    const vec3 *tangents = (const vec3 *)(uintptr_t)block->tangents;
    const float *uvs = (const float *)(uintptr_t)block->uvs;
    const int (*faces)[4] = (const int (*)[4])(uintptr_t)block->faces;
    const int *face_slots = (const int *)(uintptr_t)block->face_slots;
    const int (*tris)[3] = (const int (*)[3])(uintptr_t)block->tris;
    const int *tri_slots = (const int *)(uintptr_t)block->tri_slots;
    uint32_t hidden = word >> 16 & 0x3ff;
    uint8_t textures[FACE_TEXTURES];
    float size = area->block_size;
    int first = tm->num_verts, i, k;

    for (i = 0; i < FACE_TEXTURES; i++)
        textures[i] = s->interior_textures[b->index][i];
    for (i = 0; i < block->num_verts; i++) {
        int n = tm->num_verts;
        vec3 *v = &tm->verts[n];

        v->x = (float)((float)b->x * size + verts[i].x * size);
        v->y = (float)((float)b->y * size + verts[i].y * size);
        v->z = (float)((float)b->z * size + verts[i].z * size);
        tm->keys[n] = interior_key(v, &normals[i]);
        addvectors(v, v, offset);
        addvectors(v, v, &area->origin);
        tm->normals[n] = normals[i];
        tm->tangents[n] = tangents[i];
        tm->uvs[n].x = uvs[i * 2];
        tm->uvs[n].y = uvs[i * 2 + 1];
        tm->colors[n].x = 1.0f;
        tm->colors[n].y = 1.0f;
        tm->colors[n].z = 1.0f;
        tm->num_verts++;
    }
    for (i = 0; i < block->num_faces; i++) {
        int q = tm->num_quads, slot = face_slots[i];
        int *quad = tm->quads[q].verts;

        if (i < block->num_hideable && ((1u << (i & 31)) & hidden) != 0)
            continue;
        for (k = 0; k < 4; k++)
            quad[k] = first + faces[i][k];
        if (slot < OUTER_SLOT)
            tm->quad_slots[q] = textures[slot];
        else
            tm->quad_slots[q] = s->textures[b->index][slot - OUTER_SLOT] & 0xff;
        if (i < block->num_hideable)
            tm->quad_slots[q] = g_materiallist.flagged_material;
        generatetrinormal(&tm->quad_normals[q], &tm->verts[quad[0]], &tm->verts[quad[1]], &tm->verts[quad[2]]);
        if (slot > OUTER_SLOT && slot < OUTER_SLOT + 5) {
            uint32_t colors = s->colors[b->index];
            const vec3 *color = &g_buildingtype.parts[colors & 0x3f].colors[colors >> (4 * (slot - OUTER_SLOT) + 2) & 0xf];

            for (k = 0; k < 4; k++)
                tm->colors[quad[k]] = *color;
        }
        tm->num_quads++;
    }
    for (i = 0; i < block->num_tris; i++) {
        int t = tm->num_tris, slot = tri_slots[i];

        for (k = 0; k < 3; k++)
            tm->tris[t].verts[k] = first + tris[i][k];
        tm->tri_slots[t] = slot < OUTER_SLOT ? textures[slot] : 0;
        tm->num_tris++;
    }
}

/* Add a fixture for a city object. Bits 0 to 9 of the word give the object type, bits 10 and 11
 * a turn of 90 degrees about the y axis and bits 12 to 23 the intersection. */
static void add_fixture(InteriorMesh *mesh, uint32_t word, const FloorBlock *b, const vec3 *offset,
                        const Area *area)
{
    static const float turns[4] = {0.0f, 1.5707963705062866f, 3.1415927410125732f, 4.71238899230957f};
    InteriorFixture *f = &mesh->fixtures[mesh->num_fixtures];
    int type = (int)(word & 0x3ff), turn = (int)(word >> 10 & 3);
    float size = area->block_size;
    vec3 v;

    f->type = type;
    v = g_cityobjecttype[type].block_offset;
    if (turn != 0)
        rotatevector(&v, &g_yaxis, turns[turn]);
    f->pos.x = (float)(size * 0.5 + (float)b->x * size + v.x);
    f->pos.y = (float)((float)b->y * size + v.y);
    f->pos.z = (float)(size * 0.5 + (float)b->z * size + v.z);
    addvectors(&f->pos, &f->pos, offset);
    addvectors(&f->pos, &f->pos, &area->origin);
    resetorientation(f->rot);
    if (turn != 0)
        rotateorientation(f->rot, &g_yaxis, turns[turn]);
    f->intersection = (int)(word >> 12 & 0xfff);
    f->side = (turn + 2) & 3;
    mesh->num_fixtures++;
}

/* Clear the neighbors and the edge normals of a triangle. */
static void clear_edges(int t)
{
    TempMesh *tm = &g_tempmesh;
    int k;

    for (k = 0; k < 3; k++) {
        tm->neighbors[t].verts[k] = -1;
        memset(&tm->edge_normals[t * 3 + k], 0, sizeof(vec3));
    }
}

/* Add triangle a, b, c of quad q. Its normal is the normal of the first quad vertex. */
static void quad_tri(int q, int a, int b, int c)
{
    TempMesh *tm = &g_tempmesh;
    const int *quad = tm->quads[q].verts;
    int t = tm->num_tris;

    tm->tris[t].verts[0] = quad[a];
    tm->tris[t].verts[1] = quad[b];
    tm->tris[t].verts[2] = quad[c];
    tm->tri_normals[t] = tm->normals[quad[0]];
    tm->tri_slots[t] = tm->quad_slots[q];
    clear_edges(t);
}

/* Split each quad into two triangles. The two triangles are neighbors on the diagonal. The loose
 * triangles get no normal. */
static void split_quads(void)
{
    TempMesh *tm = &g_tempmesh;
    int t, q;

    for (t = 0; t < tm->num_tris; t++)
        clear_edges(t);
    for (q = 0; q < tm->num_quads; q++) {
        const vec3 *normal = &tm->normals[tm->quads[q].verts[0]];

        t = tm->num_tris;
        quad_tri(q, 0, 1, 2);
        tm->neighbors[t].verts[2] = t + 1;
        tm->edge_normals[t * 3 + 2] = *normal;
        tm->num_tris++;
        quad_tri(q, 0, 2, 3);
        tm->neighbors[t + 1].verts[0] = t;
        tm->edge_normals[(t + 1) * 3] = *normal;
        tm->num_tris++;
    }
}

/* Remove each vertex that no triangle uses: the last vertex moves into its place. The key list
 * holds 1 for each used vertex. */
static void remove_unused_verts(void)
{
    TempMesh *tm = &g_tempmesh;
    int v, t, k;

    for (v = 0; v < tm->num_verts; v++)
        tm->keys[v] = 0;
    for (t = 0; t < tm->num_tris; t++)
        for (k = 0; k < 3; k++)
            tm->keys[tm->tris[t].verts[k]] = 1;
    v = 0;
    while (v < tm->num_verts) {
        int last;

        if (tm->keys[v] != 0) {
            v++;
            continue;
        }
        last = --tm->num_verts;
        if (v >= last)
            continue;
        tm->verts[v] = tm->verts[last];
        tm->normals[v] = tm->normals[last];
        tm->tangents[v] = tm->tangents[last];
        tm->uvs[v].x = tm->uvs[last].x;
        tm->uvs[v].y = tm->uvs[last].y;
        tm->colors[v] = tm->colors[last];
        tm->keys[v] = tm->keys[last];
        for (t = 0; t < tm->num_tris; t++)
            for (k = 0; k < 3; k++)
                if (tm->tris[t].verts[k] == last)
                    tm->tris[t].verts[k] = v;
    }
}

/* For each edge of each solid triangle, find a later solid triangle that has the same edge in
 * the other direction. A material with flags is not solid. When an edge of the first triangle
 * matches more than one edge of the second triangle, each match writes the result again. */
static void find_neighbors(void)
{
    TempMesh *tm = &g_tempmesh;
    int i, j, a, b;

    for (i = 0; i < tm->num_tris; i++) {
        const int *ti = tm->tris[i].verts;

        if (g_materials[tm->tri_slots[i]].flags != 0)
            continue;
        for (j = i + 1; j < tm->num_tris; j++) {
            const int *tj = tm->tris[j].verts;

            if (g_materials[tm->tri_slots[j]].flags != 0)
                continue;
            for (a = 0; a < 3; a++) {
                if (tm->neighbors[i].verts[a] != -1)
                    continue;
                for (b = 0; b < 3; b++)
                    if (tm->neighbors[j].verts[b] == -1 &&
                        tempmesh_vertnormalequal(ti[a], tj[(b + 1) % 3]) &&
                        tempmesh_vertnormalequal(ti[(a + 1) % 3], tj[b])) {
                        tm->neighbors[i].verts[a] = j;
                        tm->edge_normals[i * 3 + a] = tm->tri_normals[j];
                        tm->neighbors[j].verts[b] = i;
                        tm->edge_normals[j * 3 + b] = tm->tri_normals[i];
                    }
            }
        }
    }
}

/* Add the vertex count of a new interior mesh to the debug values. */
static void count_mesh(int verts)
{
    g_debug.interior_builds++;
    g_debug.interior_vert_sum += verts;
    debug_addataint("interioravg", g_debug.interior_vert_sum / g_debug.interior_builds);
    if (verts > 0 && (g_debug.interior_min_verts == 0 || verts < g_debug.interior_min_verts))
        g_debug.interior_min_verts = verts;
    if (g_debug.interior_max_verts < verts)
        g_debug.interior_max_verts = verts;
    debug_addataint("interiormin", g_debug.interior_min_verts);
    debug_addataint("interiormax", g_debug.interior_max_verts);
}

/* Free the old buffer of a record and give a new one of size bytes. */
static void *replace_buffer(void *old, size_t size)
{
    os_free(old);
    return os_malloc(size);
}

/* Copy the temporary mesh into an interior record. The triangles go into one part for each
 * material, in material order. The neighbor of an edge keeps its index from before the sort.
 * Part 255 uses the last bytes of the record. */
static void store_mesh(InteriorMesh *mesh)
{
    const TempMesh *tm = &g_tempmesh;
    InteriorVertex *verts;
    InteriorPart *parts = mesh->parts;
    int i, mat, n = tm->num_tris;

    mesh->num_verts = tm->num_verts;
    mesh->vertices = replace_buffer(mesh->vertices, (size_t)tm->num_verts * sizeof(InteriorVertex));
    verts = mesh->vertices;
    for (i = 0; i < tm->num_verts; i++) {
        memcpy(&verts[i].pos, &tm->verts[i], sizeof(vec3));
        memcpy(&verts[i].normal, &tm->normals[i], sizeof(vec3));
        memcpy(&verts[i].tangent, &tm->tangents[i], sizeof(vec3));
        memcpy(verts[i].uv, &tm->uvs[i], sizeof verts[i].uv);
        memcpy(&verts[i].color, &tm->colors[i], sizeof(vec3));
    }
    mesh->num_triangles = n;
    mesh->indices = replace_buffer(mesh->indices, (size_t)n * 12);
    mesh->triangle_material = replace_buffer(mesh->triangle_material, (size_t)n * 4);
    mesh->triangle_plane = replace_buffer(mesh->triangle_plane, (size_t)n * 12);
    mesh->neighbors = replace_buffer(mesh->neighbors, (size_t)n * 12);
    mesh->edge_normals = replace_buffer(mesh->edge_normals, (size_t)n * 36);
    resettimer(SORT_TIMER, "interiorsort");
    starttimer(SORT_TIMER);
    mesh->num_triangles = 0;
    mesh->num_parts = 0;
    for (mat = 0; mat < NUM_MATERIALS; mat++) {
        InteriorPart *part = &parts[mesh->num_parts];

        part->material = (uint8_t)mat;
        part->num_triangles = 0;
        part->first_triangle = mesh->num_triangles;
        for (i = 0; i < n; i++) {
            int d = mesh->num_triangles;

            if (tm->tri_slots[i] != mat)
                continue;
            memcpy((int *)mesh->indices + d * 3, &tm->tris[i], 12);
            mesh->triangle_material[d] = tm->tri_slots[i];
            memcpy(&mesh->triangle_plane[d], &tm->tri_normals[i], 12);
            memcpy(&mesh->neighbors[d], &tm->neighbors[i], 12);
            memcpy(&mesh->edge_normals[d * 3], &tm->edge_normals[i * 3], 36);
            mesh->num_triangles++;
            part->num_triangles++;
        }
        if (part->num_triangles > 0)
            mesh->num_parts++;
    }
    stoptimer(SORT_TIMER);
}

/* Build the interior mesh of floors slot * 2 and slot * 2 + 1 of a sector. The result is the
 * interior record, or -1 when the sector is not in use or the floors have nothing to show. A
 * slot with nothing to show gets -2 and is not built again. A new mesh takes the next record in
 * a ring of 4096: the sector that used that record loses it. */
int __cdecl area_interior_buildsectormodel(int sector, int slot)
{
    TempMesh *tm = &g_tempmesh;
    BlockSector *s;
    const Area *area;
    InteriorMesh *mesh;
    FloorBlock b;
    vec3 offset;
    int model;

    if (sector < 0 || sector >= NUM_SECTORS || !g_block_sectors[sector].used)
        return -1;
    s = &g_block_sectors[sector];
    if (s->interior_models[slot] == NO_INTERIOR)
        return -1;
    if (s->interior_models[slot] >= 0 && s->interior_models[slot] < MAX_MODELS)
        return s->interior_models[slot];
    starttimer(g_timers.buildinterior);
    starttimer(g_timers.setupinterior);
    model = g_interior_store.count;
    area = &g_area[s->area];
    offset.x = (float)((float)s->pos[0] * 8.0 * area->block_size);
    offset.y = (float)((float)s->pos[1] * 8.0 * area->block_size);
    offset.z = (float)((float)s->pos[2] * 8.0 * area->block_size);
    tm->num_specials = 0;
    tm->num_verts = 0;
    tm->num_quads = 0;
    tm->num_tris = 0;
    for (b.y = slot * 2; b.y < slot * 2 + 2; b.y++)
        for (b.z = 0; b.z < SECTOR_SIDE; b.z++)
            for (b.x = 0; b.x < SECTOR_SIDE; b.x++) {
                uint32_t word;

                b.index = b.y * 64 + b.x + b.z * 8;
                word = s->interior[b.index];
                if ((word & BLOCK_KIND_MASK) == BLOCK_SPECIAL)
                    add_block_mesh(s, word, &b, &offset, area);
            }
    stoptimer(g_timers.setupinterior);
    mesh = &g_interiors[model];
    mesh->num_fixtures = 0;
    for (b.y = slot * 2; b.y < slot * 2 + 2; b.y++)
        for (b.z = 0; b.z < SECTOR_SIDE; b.z++)
            for (b.x = 0; b.x < SECTOR_SIDE; b.x++) {
                uint32_t word;

                b.index = b.y * 64 + b.x + b.z * 8;
                word = s->cityobjects[b.index];
                if ((int)word > 0)
                    add_fixture(mesh, word, &b, &offset, area);
            }
    if (tm->num_quads == 0 && tm->num_tris == 0 && mesh->num_fixtures == 0) {
        stoptimer(g_timers.buildinterior);
        s->interior_models[slot] = NO_INTERIOR;
        return -1;
    }
    g_interior_model_count++;
    debug_addataint("interiormodels", g_interior_model_count);
    starttimer(g_timers.optimizeinterior);
    tempmesh_optimize2();
    stoptimer(g_timers.optimizeinterior);
    split_quads();
    remove_unused_verts();
    stoptimer(g_timers.buildinterior);
    if (mesh->sector != -1)
        g_block_sectors[mesh->sector].interior_models[mesh->slot] = -1;
    mesh->sector = sector;
    mesh->slot = slot;
    s->interior_models[slot] = model;
    g_interior_store.count = (g_interior_store.count + 1) & (MAX_MODELS - 1);
    debug_addataint("interiormodelloc", g_interior_store.count);
    count_mesh(tm->num_verts);
    starttimer(g_timers.shadowinterior);
    find_neighbors();
    stoptimer(g_timers.shadowinterior);
    starttimer(g_timers.allocinterior);
    store_mesh(mesh);
    stoptimer(g_timers.allocinterior);
    return model;
}
