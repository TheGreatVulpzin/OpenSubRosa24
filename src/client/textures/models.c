/* The model in work and the builders that fill it.
 *
 * A builder clears the model in work (modeltemp), adds vertices, triangles and quads to it, and
 * then stores it as one of the game models. A triangle has one texture coordinate for each
 * corner and a shading mode. A quad is a face that a builder can pull out of the surface.
 * The builders of the city objects, the human, the hair and the items call these functions.
 */
#include "textures/models.h"

#include <string.h>

#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "vectors.h"

#define g_temp cl_data_modeltemp
#define g_models cl_data_model
#define g_cubequad sr_data_cubequad /* the 4 corners of each of the 6 faces of a cube */
#define g_cubetri sr_data_cubetri   /* the 3 corners of each of the 12 triangles of a cube */

/* Two vertices of the model in work are one point when each axis is closer than this. */
#define SAME_POINT 0.0001f

/* Shading modes of a triangle (ModelTemp.tri_shading). */
#define SHADE_FLAT (-2)   /* all corners use the normal of the triangle */
#define SHADE_SMOOTH (-1) /* each corner uses the normal of its vertex */

/* The types of city object that have a special shape. */
enum {
    CITYOBJECT_ELEVATOR = 2,
    CITYOBJECT_ELEVATOR_DOOR = 3,
    CITYOBJECT_COMPUTER = 4,
    CITYOBJECT_TRAFFIC_LIGHT = 5,
};

/* A box adds 12 triangles, two for each face. */
#define BOX_TRIS 12
#define FACE_TRIS 2

/* Game functions of other files. */
#define setupmodel CL_FN(0x4ac190, void(__cdecl *)(int model, int flat))

/* 1 when a and b differ by less than tolerance. */
static int is_near(float a, float b, float tolerance)
{
    float distance = (float)(a - b);
    if (distance < 0.0f)
        distance = -distance;
    return distance < tolerance;
}

static int points_near(const vec3 *a, const vec3 *b, float tolerance)
{
    return is_near(a->x, b->x, tolerance) && is_near(a->y, b->y, tolerance) && is_near(a->z, b->z, tolerance);
}

/* Remove all the parts of the model in work. */
static void clear_modeltemp(void)
{
    g_temp.num_verts = 0;
    g_temp.num_tris = 0;
    g_temp.num_quads = 0;
    g_temp.num_lines = 0;
}

/* Add a vertex to the model in work with no search for an equal one. The result is its index. */
static int append_vertex(const vec3 *pos)
{
    g_temp.verts[g_temp.num_verts] = *pos;
    return g_temp.num_verts++;
}

/* Add a triangle to the model in work. The result is its index. */
static int append_tri(int a, int b, int c, int shading)
{
    MeshTri *tri = &g_temp.tris[g_temp.num_tris];
    tri->verts[0] = a;
    tri->verts[1] = b;
    tri->verts[2] = c;
    g_temp.tri_shading[g_temp.num_tris] = shading;
    return g_temp.num_tris++;
}

/* 1 when two vertices of the model in work are at the same point. */
int __cdecl modeltempvertsequal(int a, int b)
{
    return points_near(&g_temp.verts[a], &g_temp.verts[b], SAME_POINT);
}

/* Find a vertex of the model in work that is closer to pos than tolerance on each axis. Add a
 * new vertex when there is none. The result is the index of the vertex. */
int __cdecl modeltemp_addvertex(const vec3 *pos, float tolerance)
{
    int i;
    for (i = 0; i < g_temp.num_verts; i++) {
        if (points_near(&g_temp.verts[i], pos, tolerance))
            return i;
    }
    return append_vertex(pos);
}

/* Free an array of a game model and give back a new copy of source. */
static void *replace_copy(void *old, const void *source, int size)
{
    void *copy;
    os_free(old);
    copy = os_malloc((size_t)size);
    memcpy(copy, source, (size_t)size);
    return copy;
}

/* Store the model in work as a game model. The game model gets its own copy of each array. */
void __cdecl copymodel(int model)
{
    Model *m = &g_models[model];
    int verts = g_temp.num_verts;
    int tris = g_temp.num_tris;
    int lines = g_temp.num_lines;

    m->num_verts = verts;
    m->vertex_floats = g_temp.vertex_floats;
    m->verts = replace_copy(m->verts, g_temp.verts, verts * (int)sizeof(vec3));
    m->vert_texcoords = replace_copy(m->vert_texcoords, g_temp.vert_texcoords, verts * (int)sizeof(TexCoord));
    m->vert_normals = replace_copy(m->vert_normals, g_temp.vert_normals, verts * (int)sizeof(vec3));
    m->vert_tangents = replace_copy(m->vert_tangents, g_temp.vert_tangents, verts * (int)sizeof(vec3));
    m->render_verts = replace_copy(m->render_verts, g_temp.render_verts,
                                   tris * 3 * g_temp.vertex_floats * (int)sizeof(float));
    m->num_tris = tris;
    m->tris = replace_copy(m->tris, g_temp.tris, tris * (int)sizeof(MeshTri));
    m->tri_normals = replace_copy(m->tri_normals, g_temp.tri_normals, tris * (int)sizeof(vec3));
    m->tri_neighbors = replace_copy(m->tri_neighbors, g_temp.tri_neighbors, tris * (int)sizeof(MeshTri));
    m->num_lines = lines;
    m->lines = replace_copy(m->lines, g_temp.lines, lines * (int)sizeof(MeshLine));
}

/* Add a box to the model in work: 8 vertices around center and 12 flat triangles. */
void __cdecl addboxtomodel(const vec3 *center, float half_x, float half_y, float half_z)
{
    int first = g_temp.num_verts;
    int i;

    for (i = 0; i < 8; i++) {
        int column = i & 3;
        vec3 corner;
        corner.x = (column == 0 || column == 3) ? -half_x : half_x;
        corner.y = (i < 4) ? -half_y : half_y;
        corner.z = (column == 0 || column == 1) ? -half_z : half_z;
        addvectors(&corner, &corner, center);
        append_vertex(&corner);
    }
    for (i = 0; i < BOX_TRIS; i++)
        append_tri(first + g_cubetri[i * 3], first + g_cubetri[i * 3 + 1], first + g_cubetri[i * 3 + 2], SHADE_FLAT);
}

/* Give the three corners of a triangle of the model in work one texture coordinate. */
void __cdecl model_settritexcoord(int tri, float u, float v)
{
    TexCoord *corners = g_temp.tri_texcoords[tri].corners;
    int i;
    for (i = 0; i < 3; i++) {
        corners[i].u = u;
        corners[i].v = v;
    }
}

/* Give the four corners of a quad of the model in work one texture coordinate. */
void __cdecl model_setquadtexcoord(int quad, float u, float v)
{
    TexCoord *corners = g_temp.quad_texcoords[quad].corners;
    int i;
    for (i = 0; i < 4; i++) {
        corners[i].u = u;
        corners[i].v = v;
    }
}

/* Move a quad to four new vertices and close the gap with four side quads. corners holds the
 * four old vertices of the quad, then the four new ones, as the corners of a cube. */
static void extrude_to(int quad, const vec3 new_pos[4])
{
    int corners[8];
    int i, side;

    for (i = 0; i < 4; i++)
        corners[i] = g_temp.quads[quad].verts[i];
    for (i = 0; i < 4; i++)
        corners[4 + i] = append_vertex(&new_pos[i]);
    for (i = 0; i < 4; i++)
        g_temp.quads[quad].verts[i] = corners[4 + i];
    for (side = 1; side < 5; side++) {
        MeshQuad *q = &g_temp.quads[g_temp.num_quads];
        for (i = 0; i < 4; i++)
            q->verts[i] = corners[g_cubequad[side * 4 + i]];
        g_temp.num_quads++;
    }
}

/* Pull a quad of the model in work out by offset. */
void __cdecl model_extrudequad(int quad, const vec3 *offset)
{
    vec3 new_pos[4];
    int i;
    for (i = 0; i < 4; i++)
        addvectors(&new_pos[i], &g_temp.verts[g_temp.quads[quad].verts[i]], offset);
    extrude_to(quad, new_pos);
}

/* Pull a quad of the model in work out by offset and scale it around its center. Each axis
 * has its own factor. */
void __cdecl model_extrudequadscale(int quad, const vec3 *offset, float scale_x, float scale_y, float scale_z)
{
    const int *v = g_temp.quads[quad].verts;
    vec3 center = {0.0f, 0.0f, 0.0f};
    vec3 new_pos[4];
    int i;

    for (i = 0; i < 4; i++)
        addvectors(&center, &center, &g_temp.verts[v[i]]);
    scalevector(&center, &center, 0.25f);
    for (i = 0; i < 4; i++) {
        vec3 corner;
        subtractvectors(&corner, &g_temp.verts[v[i]], &center);
        corner.x = (float)(corner.x * scale_x);
        corner.y = (float)(corner.y * scale_y);
        corner.z = (float)(corner.z * scale_z);
        addvectors(&corner, &corner, &center);
        addvectors(&new_pos[i], &corner, offset);
    }
    extrude_to(quad, new_pos);
}

/* Replace a quad of the model in work with a smaller quad inside it, in the same plane, and
 * add four side quads. Each new corner moves in by 1/8 of the edge 0 to 1 (or 2 to 3) and by
 * 1/4 of the edge to the corner across. */
void __cdecl model_extrudequad2(int quad)
{
    const int *v = g_temp.quads[quad].verts;
    const vec3 *p0 = &g_temp.verts[v[0]];
    const vec3 *p1 = &g_temp.verts[v[1]];
    const vec3 *p2 = &g_temp.verts[v[2]];
    const vec3 *p3 = &g_temp.verts[v[3]];
    vec3 along, across;
    vec3 new_pos[4];

    subtractvectors(&along, p1, p0);
    subtractvectors(&across, p3, p0);
    scaleaddvectors(&new_pos[0], p0, &along, 0.125f);
    scaleaddvectors(&new_pos[0], &new_pos[0], &across, 0.25f);

    subtractvectors(&across, p2, p1);
    scaleaddvectors(&new_pos[1], p1, &along, -0.125f);
    scaleaddvectors(&new_pos[1], &new_pos[1], &across, 0.25f);

    subtractvectors(&along, p3, p2);
    subtractvectors(&across, p1, p2);
    scaleaddvectors(&new_pos[2], p2, &along, 0.125f);
    scaleaddvectors(&new_pos[2], &new_pos[2], &across, 0.25f);

    subtractvectors(&across, p0, p3);
    scaleaddvectors(&new_pos[3], p3, &along, -0.125f);
    scaleaddvectors(&new_pos[3], &new_pos[3], &across, 0.25f);

    extrude_to(quad, new_pos);
}

/* Make the two triangles of a face show the whole texture. first is the first triangle. */
static void map_face_to_texture(int first)
{
    static const TexCoord whole[FACE_TRIS][3] = {
        {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}},
        {{0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}},
    };
    int tri, corner;
    for (tri = 0; tri < FACE_TRIS; tri++)
        for (corner = 0; corner < 3; corner++)
            g_temp.tri_texcoords[first + tri].corners[corner] = whole[tri][corner];
}

/* Make a game model from the collision mesh of a city object type. Each face with 4 corners
 * becomes two smooth triangles. A face with 3 corners is not in the model. One face of the
 * last box of the computer shows the whole texture. One face of each of the last two boxes of
 * the traffic light does the same. */
void __cdecl createmodel_cityobject_type(int model, int type)
{
    const CityObjectTypeRec *t = &sr_data_cityobjecttype[type];
    int i;

    clear_modeltemp();
    for (i = 0; i < t->num_verts; i++)
        append_vertex(&t->verts[i]);
    for (i = 0; i < t->num_faces; i++) {
        const CityFace *face = &t->faces[i];
        if (face->corners == 4) {
            model_settritexcoord(append_tri(face->verts[0], face->verts[1], face->verts[2], SHADE_SMOOTH), 0.0f, 0.0f);
            model_settritexcoord(append_tri(face->verts[0], face->verts[2], face->verts[3], SHADE_SMOOTH), 0.0f, 0.0f);
        }
    }
    if (type == CITYOBJECT_COMPUTER)
        map_face_to_texture(g_temp.num_tris - BOX_TRIS + FACE_TRIS);
    if (type == CITYOBJECT_TRAFFIC_LIGHT) {
        map_face_to_texture(g_temp.num_tris - BOX_TRIS + 2 * FACE_TRIS);
        map_face_to_texture(g_temp.num_tris - 2 * BOX_TRIS + 2 * FACE_TRIS);
    }
    setupmodel(model, 1);
}

/* Make the game model of the elevator (a floor, a ceiling and a panel) or of its door (one
 * thin box). The texture coordinate of all triangles is one corner of the texture. */
void __cdecl createmodel_cityobject_elevator(int model, int type)
{
    static const vec3 floor_center = {0.0f, -1.875f, 0.0f};
    static const vec3 ceiling_center = {0.0f, 1.0625f, 0.0f};
    static const vec3 panel_center = {1.25f, -0.25f, -1.5f};
    static const vec3 door_center = {0.0f, 0.0f, 0.0f};
    float u;
    int i;

    clear_modeltemp();
    if (type == CITYOBJECT_ELEVATOR) {
        addboxtomodel(&floor_center, 2.0f, 0.125f, 2.0f);
        addboxtomodel(&ceiling_center, 2.0f, 0.0625f, 2.0f);
        addboxtomodel(&panel_center, 0.375f, 0.375f, 0.0625f);
        u = 0.0f;
    } else {
        addboxtomodel(&door_center, 0.5f, 2.0f, 0.0625f);
        u = 1.0f;
    }
    for (i = 0; i < g_temp.num_tris; i++)
        model_settritexcoord(i, u, 0.0f);
    setupmodel(model, 0);
}

/* Make the game model of a city object type. */
void __cdecl createmodel_cityobject(int model, int type)
{
    if (type == CITYOBJECT_ELEVATOR || type == CITYOBJECT_ELEVATOR_DOOR)
        createmodel_cityobject_elevator(model, type);
    else
        createmodel_cityobject_type(model, type);
}
