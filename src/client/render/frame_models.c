/* The frame list holds model transforms, scales, colors and four texture indices. The draw uses
 * the first two.
 * A sorted index list controls the drawing order.
 * Human models get their positions and normals from the bone transforms.
 */
#include "render/lists.h"
#include "client.h"
#include "gl_slots.h"
#include "math/small_vectors.h"
#include "sr_runtime.h"
#include "setup/timers.h"
#include "textures/opengl.h"
#include "vectors.h"

enum { ARRAY_POSITION = 0x8074, ARRAY_NORMAL = 0x8075, ARRAY_TEXCOORD = 0x8078,
       TEXTURE0 = 0x84c0, TEXTURE1 = 0x84c1, TEXTURE2 = 0x84c2,
       LIGHT0 = 0x4000, LIGHT_POSITION = 0x1203, CULL_FACE = 0xb44, NORMAL_OFFSET = 0x14, TEXCOORD_OFFSET = 0xc,
       FIRST_PART_REFERENCE = 16, ENTRY_HIDDEN = 2, FRAME_MODELS = 4096 };

GL_IMPORT(glEnableClientState, (uint32_t array), (array))
GL_IMPORT(glDisableClientState, (uint32_t array), (array))
GL_IMPORT(glPushMatrix, (void), ())
GL_IMPORT(glPopMatrix, (void), ())
GL_IMPORT(glColor3fv, (const float *color), (color))
GL_IMPORT(glLightfv, (uint32_t light, uint32_t name, const float *value), (light, name, value))
GL_IMPORT(glTexCoordPointer, (int size, uint32_t type, int stride, const void *pointer), (size, type, stride, pointer))
GL_IMPORT(glNormalPointer, (uint32_t type, int stride, const void *pointer), (type, stride, pointer))
GL_IMPORT(glDrawArrays, (uint32_t mode, int first, int count), (mode, first, count))

/* Transform human corners, then fill the face and vertex normals. */
void __cdecl buildhumanmodel(int human_index, int model_index)
{
    Human *human = &sr_data_human[human_index];
    Model *model = &cl_data_model[model_index];
    int stride = model->vertex_floats * 4;
    uint8_t *vertices = (uint8_t *)model->render_verts;
    int v, t;
    for (v = 0; v < model->num_tris * 3; ++v) {
        const BoneVert *reference = &cl_data_cornervert[v];
        const Bone *bone;
        vec3 point;
        if (reference->bone < FIRST_PART_REFERENCE) {
            bone = &human->bones[reference->bone];
            scaleaddvectors(&point, &bone->pos, &bone->rot[0], reference->pos.x);
            scaleaddvectors(&point, &point, &bone->rot[1], reference->pos.y);
            scaleaddvectors(&point, &point, &bone->rot[2], reference->pos.z);
        } else {
            const vec3 *part = human->colors + (reference->bone - FIRST_PART_REFERENCE) * 2;
            vec3 local = reference->pos;
            bone = &human->bones[reference->mode];
            scaleaddvectors(&point, &bone->pos, &bone->rot[0], part[0].x);
            scaleaddvectors(&point, &point, &bone->rot[1], part[0].y);
            scaleaddvectors(&point, &point, &bone->rot[2], part[0].z);
            local.x = (float)(local.x * part[1].x);
            local.y = (float)(local.y * part[1].y);
            scaleaddvectors(&point, &point, &bone->rot[0], local.x);
            scaleaddvectors(&point, &point, &bone->rot[1], local.y);
            scaleaddvectors(&point, &point, &bone->rot[2], local.z);
        }
        *(vec3 *)(vertices + v * stride) = point;
    }
    for (t = 0; t < model->num_tris; ++t) {
        uint8_t *triangle = vertices + t * 3 * stride;
        generatetrinormal(&model->tri_normals[t], (const vec3 *)triangle,
            (const vec3 *)(triangle + stride), (const vec3 *)(triangle + 2 * stride));
        for (v = 0; v < 3; ++v)
            *(vec3 *)(triangle + v * stride + NORMAL_OFFSET) = model->tri_normals[t];
    }
}

/* Select the texture unit through its extension slot. */
static void texture_unit(uint32_t unit)
{
    cl_glActiveTextureARB(unit);
}

/* Draw the sorted entries. The world debug view suppresses the whole pass. */
void __cdecl rendermodellist(int mode)
{
    typedef void(__cdecl *PlacementFn)(const vec3 *, const vec3 *);
    PlacementFn placement;
    int previous = -1, i;
    if (sr_data_debug.world_view)
        return;
    starttimer(cl_data_timerlist.rendermodels);
    glEnableClientState(ARRAY_POSITION);
    if (mode == 0 || mode == 2) {
        glEnableClientState(ARRAY_TEXCOORD);
        glEnableClientState(ARRAY_NORMAL);
    }
    if (mode == 0) {
        texture_unit(TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        texture_unit(TEXTURE2);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, cl_data_texture[cl_data_texturelist.shadow_map].gl_name);
        glLightfv(LIGHT0, LIGHT_POSITION, cl_data_light_position);
    }
    placement = CL_FN(0x5066b0, PlacementFn);
    for (i = 0; i < cl_data_frame_model_count; ++i) {
        const FrameEntry *entry = &cl_data_frame_models[cl_data_frame_model_order[i]];
        const Model *model;
        const uint8_t *vertices;
        int stride;
        if (entry->flags & ENTRY_HIDDEN)
            continue;
        model = &cl_data_model[entry->model];
        if (mode == 0 && entry->textures[0] != previous) {
            texture_unit(TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, cl_data_texture[entry->textures[0]].gl_name);
            texture_unit(TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, cl_data_texture[entry->textures[1]].gl_name);
            previous = entry->textures[0];
        }
        glPushMatrix();
        placement(&entry->pos, entry->rot);
        if (mode == 0)
            glColor3fv((const float *)&entry->color);
        stride = model->vertex_floats * 4;
        vertices = (const uint8_t *)model->render_verts;
        glVertexPointer(3, GL_FLOAT, stride, vertices);
        if (mode == 0 || mode == 2) {
            glTexCoordPointer(2, GL_FLOAT, stride, vertices + TEXCOORD_OFFSET);
            glNormalPointer(GL_FLOAT, stride, vertices + NORMAL_OFFSET);
        }
        glDrawArrays(GL_TRIANGLES, 0, model->num_tris * 3);
        glPopMatrix();
    }
    if (mode == 0) {
        texture_unit(TEXTURE2);
        glDisable(GL_TEXTURE_2D);
        texture_unit(TEXTURE1);
        glDisable(GL_TEXTURE_2D);
        texture_unit(TEXTURE0);
    }
    if (mode == 0 || mode == 2) {
        glDisableClientState(ARRAY_TEXCOORD);
        glDisableClientState(ARRAY_NORMAL);
    }
    glDisableClientState(ARRAY_POSITION);
    stoptimer(cl_data_timerlist.rendermodels);
}

/* Reset culling and depth testing around the message pass. */
void __cdecl frame_rendermessages(void)
{
    glDisable(CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

/* Add one model to the frame list with a scale and a color of 1. A full list ignores the model.
 * The word at +4 of the entry keeps its value. */
void __cdecl frame_addmodel(int model, const int textures[4], uint32_t flags, const vec3 *pos, const vec3 rot[3])
{
    FrameEntry *entry;

    if (cl_data_frame_model_count >= FRAME_MODELS)
        return;
    entry = &cl_data_frame_models[cl_data_frame_model_count];
    entry->model = model;
    entry->textures[0] = textures[0];
    entry->textures[1] = textures[1];
    entry->textures[2] = textures[2];
    entry->textures[3] = textures[3];
    entry->flags = flags;
    /* copyvector moves the bits, so a NaN keeps its bits. */
    copyvector((unsigned int *)&entry->pos, (const unsigned int *)pos);
    copyvector((unsigned int *)&entry->rot[0], (const unsigned int *)&rot[0]);
    copyvector((unsigned int *)&entry->rot[1], (const unsigned int *)&rot[1]);
    copyvector((unsigned int *)&entry->rot[2], (const unsigned int *)&rot[2]);
    entry->scale.x = 1.0f;
    entry->scale.y = 1.0f;
    entry->scale.z = 1.0f;
    entry->color.x = 1.0f;
    entry->color.y = 1.0f;
    entry->color.z = 1.0f;
    ++cl_data_frame_model_count;
}
