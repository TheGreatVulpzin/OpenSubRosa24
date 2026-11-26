/* Particles use camera facing quads or surface aligned quads.
 * Glass uses triangles with a surface normal and a dark translucent color.
 * Landed particles and glass use vertex arrays. Moving particles sort by depth.
 */
#include "render/sprites.h"

#include "client.h"
#include "gl_slots.h"
#include "sr_runtime.h"
#include "textures/opengl.h"
#include "util.h"
#include "vectors.h"

enum { ARRAY_POSITION = 0x8074, ARRAY_NORMAL = 0x8075, ARRAY_TEXCOORD = 0x8078,
       CULL_FACE = 0xb44, LINE_SMOOTH = 0xb20, LINES = 1,
       PROGRAM_REFLECT = 2, UNIFORM_CAMERA = 1, UNIFORM_WORLD = 4, UNIFORM_LIGHT = 5 };

GL_IMPORT(glVertex3fv, (const float *position), (position))
GL_IMPORT(glNormal3fv, (const float *normal), (normal))
GL_IMPORT(glTexCoord2f, (float u, float v), (u, v))
GL_IMPORT(glColor3f, (float red, float green, float blue), (red, green, blue))
GL_IMPORT(glEnableClientState, (uint32_t array), (array))
GL_IMPORT(glDisableClientState, (uint32_t array), (array))
GL_IMPORT(glTexCoordPointer, (int count, uint32_t type, int stride, const void *pointer), (count, type, stride, pointer))
GL_IMPORT(glNormalPointer, (uint32_t type, int stride, const void *pointer), (type, stride, pointer))
GL_IMPORT(glDrawArrays, (uint32_t mode, int first, int count), (mode, first, count))

/* Fade alpha over the last 32 ticks. Negative life gives negative alpha. */
static float sprite_alpha(int life)
{
    float alpha = (float)((float)life / 32.0);

    if (1.0f < alpha)
        alpha = 1.0f;
    return alpha;
}

/* Draw a quad or two triangles after two rounded operations for each corner. */
static void sprite_quad(const ClientSprite *sprite, const vec3 rows[3], int triangles)
{
    static const int quad[4] = {0, 1, 2, 3};
    static const int tris[6] = {0, 1, 2, 0, 2, 3};
    static const float signs[4][2] = {{-1, 1}, {1, 1}, {1, -1}, {-1, -1}};
    static const float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    int index;

    for (index = 0; index < (triangles ? 6 : 4); ++index) {
        int corner = triangles ? tris[index] : quad[index];
        vec3 position;

        scaleaddvectors(&position, &sprite->pos, &rows[0], signs[corner][0] * sprite->size);
        scaleaddvectors(&position, &position, &rows[1], signs[corner][1] * sprite->size);
        glTexCoord2f(triangles ? 0.5f : uv[corner][0], triangles ? 0.5f : uv[corner][1]);
        glVertex3fv(&position.x);
    }
}

/* Send the normal that faces away from the camera toward the shape. */
static void glass_normal(vec3 *normal, const vec3 *position)
{
    vec3 direction;

    subtractvectors(&direction, position, &cl_data_audio_camera.pos);
    if (dotproduct(normal, &direction) < 0.0f) {
        normal->x = -normal->x;
        normal->y = -normal->y;
        normal->z = -normal->z;
    }
    glNormal3fv(&normal->x);
}

/* Draw particles, the faces and edges of glass and the short light traces. */
void __cdecl rendersprites(void)
{
    int index;
    float red = (float)(cl_data_light_ambient[0] * 0.25);
    float green = (float)(cl_data_light_ambient[1] * 0.0);
    float blue = (float)(cl_data_light_ambient[2] * 0.0);
    const ProgramObject *program = &cl_data_programs[PROGRAM_REFLECT];

    for (index = 0; index < sr_data_numofsprites; ++index)
        sr_data_radix.keys[index] = cl_ftoi((double)dotproduct(&cl_data_sprites[index].pos, &cl_data_audio_camera.rot[2]) * 1024.0 + 16777216.0);
    sr_data_radix.count = sr_data_numofsprites;
    csradixsort();
    glDepthMask(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(CULL_FACE);
    glBindTexture(GL_TEXTURE_2D, cl_data_texture[sr_data_texturelist.rough_disc].gl_name);
    glEnableClientState(ARRAY_POSITION);
    glEnableClientState(ARRAY_TEXCOORD);
    glColor3f(red, green, blue);
    glVertexPointer(3, GL_FLOAT, sizeof(SpriteDecalVertex), &cl_data_sprite_decals[0].pos);
    glTexCoordPointer(2, GL_FLOAT, sizeof(SpriteDecalVertex), cl_data_sprite_decals[0].texcoord);
    glDrawArrays(GL_QUADS, 0, sr_data_sprite_points);
    glDisableClientState(ARRAY_TEXCOORD);
    glDisableClientState(ARRAY_POSITION);
    glBindTexture(GL_TEXTURE_2D, cl_data_texture[sr_data_texturelist.rough_disc].gl_name);
    glBegin(GL_QUADS);
    for (index = 0; index < sr_data_numofsprites; ++index) {
        const ClientSprite *sprite = &cl_data_sprites[sr_data_radix.order[index]];
        float alpha = sprite_alpha(sprite->life);

        if (sprite->kind == SPRITE_SPARK) {
            glColor4f(1, 1, 0.75f, alpha);
            sprite_quad(sprite, cl_data_audio_camera.rot, 0);
        }
        if (sprite->kind == SPRITE_STAIN || sprite->kind == SPRITE_MARK) {
            if (sprite->kind == SPRITE_STAIN)
                glColor4f(red, green, blue, alpha);
            else
                glColor4f(0, 0, 0, alpha);
            sprite_quad(sprite, sprite->rot, 0);
        }
    }
    glEnd();
    glBlendFunc(1, GL_ONE_MINUS_SRC_ALPHA);
    cl_glUseProgramObjectARB(program->program);
    cl_glUniform3fvARB(program->uniforms[UNIFORM_CAMERA], 1, &cl_data_audio_camera.pos.x);
    cl_glUniform3fvARB(program->uniforms[UNIFORM_WORLD], 1, &cl_data_world_position.x);
    cl_glUniform3fvARB(program->uniforms[UNIFORM_LIGHT], 1, cl_data_light_position);
    glBindTexture(GL_TEXTURE_2D, cl_data_texture[sr_data_texturelist.material[sr_data_materiallist.flagged_material]].gl_name);
    glEnableClientState(ARRAY_POSITION);
    glEnableClientState(ARRAY_NORMAL);
    glColor4f(0, 0, 0, 0.25f);
    glTexCoord2f(0.5f, 0.5f);
    glVertexPointer(3, GL_FLOAT, sizeof(GlassVertex), &cl_data_glass_vertices[0].pos);
    glNormalPointer(GL_FLOAT, sizeof(GlassVertex), &cl_data_glass_vertices[0].normal);
    glDrawArrays(GL_QUADS, 0, sr_data_sprite_lines);
    glDisableClientState(ARRAY_NORMAL);
    glDisableClientState(ARRAY_POSITION);
    glBegin(GL_TRIANGLES);
    for (index = 0; index < sr_data_numofsprites; ++index) {
        const ClientSprite *sprite = &cl_data_sprites[sr_data_radix.order[index]];
        vec3 normal;
        int corner;

        if (sprite->kind == SPRITE_GLASS) {
            normal = sprite->rot[2];
            glass_normal(&normal, &sprite->pos);
            glColor4f(0, 0, 0, (float)(sprite_alpha(sprite->life) * 0.25));
            sprite_quad(sprite, sprite->rot, 1);
        }
        if (sprite->kind == SPRITE_TRIANGLE) {
            generatetrinormal(&normal, &sprite->rot[0], &sprite->rot[1], &sprite->rot[2]);
            glass_normal(&normal, &sprite->rot[0]);
            glColor4f(0, 0, 0, (float)(sprite_alpha(sprite->life) * 0.25));
            for (corner = 0; corner < 3; ++corner) {
                glTexCoord2f(0.5f, 0.5f);
                glVertex3fv(&sprite->rot[corner].x);
            }
        }
    }
    glEnd();
    cl_glUseProgramObjectARB(0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glBegin(LINES);
    for (index = 0; index < sr_data_numofsprites; ++index) {
        const ClientSprite *sprite = &cl_data_sprites[sr_data_radix.order[index]];
        int corner;

        if (sprite->kind != SPRITE_TRIANGLE)
            continue;
        glColor4f(0.75f, 0.75f, 0.75f, (float)(sprite_alpha(sprite->life) * 0.5));
        for (corner = 0; corner < 3; ++corner) {
            glVertex3fv(&sprite->rot[corner].x);
            glVertex3fv(&sprite->rot[(corner + 1) % 3].x);
        }
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_2D);
    glEnable(LINE_SMOOTH);
    glBegin(LINES);
    for (index = 0; index < sr_data_numofsprites; ++index) {
        const ClientSprite *sprite = &cl_data_sprites[sr_data_radix.order[index]];
        float alpha;

        if (sprite->kind != SPRITE_TRACE)
            continue;
        alpha = (float)((float)(sprite->life - 1) * 0.5);
        if (1.0f < alpha)
            alpha = 1.0f;
        alpha = (float)(alpha * 0.5);
        if (alpha < 0.0f)
            alpha = 0.0f;
        glColor4f(1, 1, 1, alpha);
        glVertex3fv(&sprite->vel.x);
        alpha = (float)((float)sprite->life * 0.5);
        if (1.0f < alpha)
            alpha = 1.0f;
        glColor4f(1, 1, 1, (float)(alpha * 0.5));
        glVertex3fv(&sprite->pos.x);
    }
    glEnd();
    glDisable(LINE_SMOOTH);
    glEnable(GL_TEXTURE_2D);
    glEnable(CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(1);
}
