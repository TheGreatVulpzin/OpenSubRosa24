/* Drawing of the models with OpenGL: a sphere, a lit model and a wire model.
 *
 * A model keeps its vertices in one array of floats. The position takes 3 floats, the texture
 * coordinates take 2 and the normal takes 3, and the stride of a vertex is vertex_floats * 4
 * bytes. The transform of an object comes from setupobjectrender.
 */
#include "render/models_draw.h"

#include <string.h>

#include "client.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "vectors.h"

#define g_textures cl_data_texture
#define g_models cl_data_model
#define g_sphere cl_data_sphere
#define g_texturelist sr_data_texturelist
#define g_light_position cl_data_light_position
#define g_programs cl_data_programs
#define g_vbo cl_data_vbo
#define g_track_vbo cl_data_track_vbo
#define g_track_vertex_floats cl_data_track_vertex_floats
#define g_track_vertex_count cl_data_track_vertex_count

/* Game functions of other files. */
#define setupobjectrender CL_FN(0x5066b0, void(__cdecl *)(const void *placement, const void *scale))

enum {
    GL_VERTEX_ARRAY = 0x8074,
    GL_NORMAL_ARRAY = 0x8075,
    GL_TEXTURE_COORD_ARRAY = 0x8078,
    GL_LIGHT0 = 0x4000,
    GL_POSITION = 0x1203,
    GL_TEXTURE0 = 0x84c0,
    GL_TEXTURE1 = 0x84c1,
    GL_TEXTURE2 = 0x84c2,
    GL_LINES = 0x0001,
    PROGRAM_MODEL = 3,
    GL_FRONT = 0x0404,
    GL_BACK = 0x0405,
    GL_FILL = 0x1b02,
    TEXCOORD_OFFSET = 0x0c,
    NORMAL_OFFSET = 0x14,
    MODE_LIT = 0,
    MODE_WIRE_COLOR = 2
};

/* The slot of the multitexture extension: the client holds a pointer to it. */
#define SLOT_ACTIVE_TEXTURE 0x115874
#define SLOT_BIND_BUFFER 0x115aa8
#define SLOT_USE_PROGRAM 0x115b18
typedef void(__stdcall *ActiveTextureFn)(uint32_t unit);

GL_IMPORT(glPushMatrix, (void), ())
GL_IMPORT(glPopMatrix, (void), ())
GL_IMPORT(glScalef, (float x, float y, float z), (x, y, z))
GL_IMPORT(glNormal3fv, (const float *normal), (normal))
GL_IMPORT(glVertex3fv, (const float *vertex), (vertex))
GL_IMPORT(glColor3fv, (const float *color), (color))
GL_IMPORT(glLightfv, (uint32_t light, uint32_t name, const float *value), (light, name, value))
GL_IMPORT(glEnableClientState, (uint32_t array), (array))
GL_IMPORT(glDisableClientState, (uint32_t array), (array))
GL_IMPORT(glTexCoordPointer, (int size, uint32_t type, int stride, const void *pointer), (size, type, stride, pointer))
GL_IMPORT(glNormalPointer, (uint32_t type, int stride, const void *pointer), (type, stride, pointer))
GL_IMPORT(glDrawArrays, (uint32_t mode, int first, int count), (mode, first, count))
GL_IMPORT(glPolygonMode, (uint32_t face, uint32_t mode), (face, mode))
GL_IMPORT(glColor3f, (float red, float green, float blue), (red, green, blue))

static void select_texture_unit(uint32_t unit)
{
    (*(ActiveTextureFn *)(sr_base + SLOT_ACTIVE_TEXTURE))(unit);
}

static void bind_buffer(uint32_t target, uint32_t buffer)
{
    (*(void(__stdcall **)(uint32_t, uint32_t))(sr_base + SLOT_BIND_BUFFER))(target, buffer);
}

static void use_program(uint32_t program)
{
    (*(void(__stdcall **)(uint32_t))(sr_base + SLOT_USE_PROGRAM))(program);
}

static void bind_texture(int texture)
{
    glBindTexture(GL_TEXTURE_2D, g_textures[texture].gl_name);
}

/* Draw a level of the sphere mesh at an object's place, scaled. Each corner has the normal of
 * its vertex. */
void __cdecl rendersphere(const void *placement, float scale, int level)
{
    const SphereLevel *mesh = &g_sphere[level];
    int triangle;
    int corner;

    glPushMatrix();
    setupobjectrender(placement, 0);
    glScalef(scale, scale, scale);
    glBegin(GL_TRIANGLES);
    for (triangle = 0; triangle < mesh->num_tris; ++triangle)
        for (corner = 0; corner < 3; ++corner) {
            const float *vertex = (const float *)&mesh->verts[mesh->tris[triangle].verts[corner]];

            glNormal3fv(vertex);
            glVertex3fv(vertex);
        }
    glEnd();
    glPopMatrix();
}

/* Draw one model. Mode 0 uses three textures (color, light map, shadow map) and the light,
 * mode 2 uses the color texture only, and any other mode draws the shape only. */
void __cdecl rendermodel(int model, const void *placement, const void *scale, int mode, const int *textures,
                         const float *color)
{
    const Model *record = &g_models[model];
    int stride = record->vertex_floats << 2;

    if (mode == MODE_LIT) {
        bind_texture(textures[0]);
        select_texture_unit(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        bind_texture(textures[1]);
        select_texture_unit(GL_TEXTURE2);
        glEnable(GL_TEXTURE_2D);
        bind_texture(g_texturelist.shadow_map);
        glLightfv(GL_LIGHT0, GL_POSITION, g_light_position);
    }
    if (mode == MODE_WIRE_COLOR)
        bind_texture(textures[0]);
    glPushMatrix();
    setupobjectrender(placement, scale);
    glColor3fv(color);
    glEnableClientState(GL_VERTEX_ARRAY);
    if (mode == MODE_LIT || mode == MODE_WIRE_COLOR) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
    }
    glVertexPointer(3, GL_FLOAT, stride, record->render_verts);
    if (mode == MODE_LIT || mode == MODE_WIRE_COLOR) {
        glTexCoordPointer(2, GL_FLOAT, stride, (const char *)record->render_verts + TEXCOORD_OFFSET);
        glNormalPointer(GL_FLOAT, stride, (const char *)record->render_verts + NORMAL_OFFSET);
    }
    glDrawArrays(GL_TRIANGLES, 0, record->num_tris * 3);
    if (mode == MODE_LIT || mode == MODE_WIRE_COLOR) {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
    if (mode == MODE_LIT) {
        glDisable(GL_TEXTURE_2D);
        select_texture_unit(GL_TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        select_texture_unit(GL_TEXTURE0);
    }
}

/* Draw a model as flat shapes in a color with half alpha, then set both faces to fill. */
void __cdecl rendermodelwire(int model, const void *placement, const void *scale_place, const unsigned *color,
                             float scale)
{
    const Model *record = &g_models[model];
    float red, green, blue;

    glPushMatrix();
    setupobjectrender(placement, scale_place);
    glScalef(scale, scale, scale);
    memcpy(&red, &color[0], 4);
    memcpy(&green, &color[1], 4);
    memcpy(&blue, &color[2], 4);
    glColor4f(red, green, blue, 0.5f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, record->vertex_floats << 2, record->render_verts);
    glDrawArrays(GL_TRIANGLES, 0, record->num_tris * 3);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
    glPolygonMode(GL_FRONT, GL_FILL);
    glPolygonMode(GL_BACK, GL_FILL);
}

/* Draw the normals and the tangents of a model as short lines at its vertices: red lines for
 * the normals and green lines for the tangents, half a unit long. */
void __cdecl rendermodelnormals(int model, const void *placement, const void *scale)
{
    const Model *record = &g_models[model];
    vec3 end;
    int triangle;
    int corner;
    int pass;

    glPushMatrix();
    setupobjectrender(placement, scale);
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    for (triangle = 0; triangle < record->num_tris; ++triangle)
        for (pass = 0; pass < 2; ++pass) {
            const vec3 *direction = pass == 0 ? record->vert_normals : record->vert_tangents;

            if (pass == 0)
                glColor3f(1.0f, 0.0f, 0.0f);
            else
                glColor3f(0.0f, 1.0f, 0.0f);
            for (corner = 0; corner < 3; ++corner) {
                int vertex = record->tris[triangle].verts[corner];

                glVertex3fv((const float *)&record->verts[vertex]);
                scaleaddvectors(&end, &record->verts[vertex], &direction[vertex], 0.5f);
                glVertex3fv((const float *)&end);
            }
        }
    glEnd();
    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
}

/* Draw the buffer of the track: the shape only (mode not 0) or lit with the model program and
 * the first panel texture (mode 0). */
void __cdecl rendertrackmodel(int mode)
{
    int stride = g_track_vertex_floats << 2;

    bind_buffer(GL_ARRAY_BUFFER, g_vbo[g_track_vbo].gl_name);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, stride, 0);
    if (mode == 0) {
        use_program(g_programs[PROGRAM_MODEL].program);
        bind_texture(g_texturelist.panel[0]);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, stride, (const void *)TEXCOORD_OFFSET);
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, stride, (const void *)NORMAL_OFFSET);
        glLightfv(GL_LIGHT0, GL_POSITION, g_light_position);
    }
    glDrawArrays(GL_QUADS, 0, g_track_vertex_count);
    glDisableClientState(GL_VERTEX_ARRAY);
    if (mode == 0) {
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        use_program(0);
    }
    bind_buffer(GL_ARRAY_BUFFER, 0);
}
