/* The depth pass draws opaque geometry and stencil shadow volumes.
 * Ambient occlusion adds a screen pass and two blur passes.
 * Framebuffer support selects texture attachments or screen copies.
 */
#include "render/frame.h"
#include "client.h"
#include "gl_slots.h"
#include "render/gl_state.h"
#include "render/landscape_draw.h"
#include "render/lists.h"
#include "render/models_draw.h"
#include "render/sectorlist.h"
#include "render/sectors.h"
#include "render/shadow.h"
#include "render/sprite_models.h"
#include "setup/timers.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

enum { CULL_FACE = 0xb44, STENCIL_TEST = 0xb90, ARRAY_POSITION = 0x8074,
       DEPTH_LEQUAL = 0x203, FRONT_CW = 0x900, PROGRAM_DEPTH = 5, PROGRAM_SHADOW = 4,
       PROGRAM_OCCLUSION = 6, PROGRAM_BLUR = 8, TEXTURE1 = 0x84c1,
       FRAMEBUFFER = 0x8d40, COLOR_ATTACHMENT = 0x8ce0, CLEAR_COLOR = 0x4000,
       CLEAR_COLOR_DEPTH_STENCIL = 0x4500, STENCIL_KEEP = 0x1e00, STENCIL_EQUAL = 0x202 };

GL_IMPORT(glClearColor, (float r, float g, float b, float a), (r, g, b, a))
GL_IMPORT(glClearStencil, (int value), (value))
GL_IMPORT(glStencilMask, (uint32_t mask), (mask))
GL_IMPORT(glClear, (uint32_t mask), (mask))
GL_IMPORT(glEnableClientState, (uint32_t array), (array))
GL_IMPORT(glDisableClientState, (uint32_t array), (array))
GL_IMPORT(glStencilOp, (uint32_t fail, uint32_t depth_fail, uint32_t pass), (fail, depth_fail, pass))
GL_IMPORT(glStencilFunc, (uint32_t test, int reference, uint32_t mask), (test, reference, mask))
GL_IMPORT(glColor3f, (float r, float g, float b), (r, g, b))
GL_IMPORT(glFrontFace, (uint32_t face), (face))
GL_IMPORT(glDepthFunc, (uint32_t test), (test))
GL_IMPORT(glTexCoord2f, (float u, float v), (u, v))

/* Select a shader program, or clear the program with a negative index. */
static void depth_program(int index)
{
    cl_glUseProgramObjectARB(
        index < 0 ? 0 : cl_data_programs[index].program);
}

/* Bind the image texture on the current texture unit. */
static void depth_texture(int index)
{
    glBindTexture(GL_TEXTURE_2D, cl_data_texture[index].gl_name);
}

/* Attach an image texture to the framebuffer color output. */
static void depth_attachment(int index)
{
    cl_glFramebufferTexture2D(
        FRAMEBUFFER, COLOR_ATTACHMENT, GL_TEXTURE_2D, cl_data_texture[index].gl_name, 0);
}

/* Copy the viewport into the selected image texture. */
static void depth_copy(int texture)
{
    depth_texture(texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
        cl_ftoi(cl_data_viewport_width), cl_ftoi(cl_data_viewport_height));
}

/* Emit the four screen corners in their drawing order. */
static void depth_quad(void)
{
    glVertex2f(-1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glVertex2f(1.0f, -1.0f);
    glVertex2f(-1.0f, -1.0f);
}

/* Draw one blur direction and copy its result to the shadow map. */
static void blur_quad(const float step[2])
{
    ProgramObject *program = &cl_data_programs[PROGRAM_BLUR];
    depth_program(PROGRAM_BLUR);
    cl_glUniform2fvARB(
        program->uniforms[1], 1, step);
    cl_glUniform4fvARB(
        program->uniforms[2], 1, cl_data_blur_params_near);
    cl_glUniform4fvARB(
        program->uniforms[3], 1, (cl_data_blur_params_near + 4));
    depth_texture(sr_data_texturelist.shadow_map);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, -1.0f);
    glEnd();
    depth_program(-1);
    depth_copy(sr_data_texturelist.shadow_map);
}

/* Draw depth, shadow masks and optional ambient occlusion. */
void __cdecl renderframedepth(void)
{
    const ClientRenderOptions *options = (const ClientRenderOptions *)&sr_data_option;
    int debug_shadows = sr_data_debugit && !cl_data_audio_input_blocked && cl_data_keyboard[14];
    int i;
    starttimer(sr_data_timerlist.renderdepth);
    if (options->framebuffer)
        depth_attachment(sr_data_texturelist.sky);
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glStencilMask(UINT32_MAX);
    glClearStencil(0);
    glClear(CLEAR_COLOR_DEPTH_STENCIL);
    glStencilMask(0);
    glDisable(GL_BLEND);
    depth_program(PROGRAM_DEPTH);
    render_sectorlist(1);
    glEnableClientState(ARRAY_POSITION);
    for (i = 0; i < cl_data_sector_count; ++i)
        render_sector_interior(cl_data_sector_list[i], 1);
    glDisableClientState(ARRAY_POSITION);
    renderlandscape(1);
    rendermodellist(1);
    for (i = 0; i < sr_data_numofsprite_models; ++i) {
        const SpriteModel *piece = &cl_data_sprite_models[i];
        if (!piece->active)
            continue;
        if (piece->kind == 0)
            rendermodel(sr_data_modellist.bullet_flat[0], &piece->pos, piece->rot, 1,
                cl_data_frame_models[0].textures, (const float *)&cl_data_frame_models[0].color);
        if (piece->kind == 1)
            rendermodel(sr_data_modellist.sprite[piece->kind], &piece->pos, piece->rot, 1,
                cl_data_frame_models[0].textures, (const float *)&cl_data_frame_models[0].color);
    }
    renderspritemodels(1);
    rendertrackmodel(1);
    depth_program(-1);
    if (!options->framebuffer)
        depth_copy(sr_data_texturelist.sky);
    if (options->framebuffer)
        depth_attachment(sr_data_texturelist.shadow_map);
    if (debug_shadows && options->framebuffer)
        depth_attachment(sr_data_texturelist.frame_color);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(CLEAR_COLOR);
    glstateshadowtwoside();
    if (debug_shadows) {
        glColorMask(1, 1, 1, 0);
        glEnable(GL_BLEND);
        glColor4f(0.0f, 1.0f, 0.0f, 0.125f);
    }
    depth_program(PROGRAM_SHADOW);
    render_areasectorshadow();
    glEnableClientState(ARRAY_POSITION);
    for (i = 0; i < cl_data_sector_count; ++i)
        render_sector_interior_shadow2(cl_data_sector_list[i]);
    if (!sr_data_debug.world_view)
        for (i = 0; i < cl_data_frame_model_count; ++i) {
            const FrameEntry *entry = &cl_data_frame_models[i];
            if (entry->flags & 1)
                rendermodelshadow(entry->model, &entry->pos, entry->rot);
        }
    glDisableClientState(ARRAY_POSITION);
    depth_program(-1);
    disabletwosidedstencil();
    if (debug_shadows) {
        glDisable(STENCIL_TEST);
        stoptimer(sr_data_timerlist.renderdepth);
        return;
    }
    glColorMask(1, 0, 0, 0);
    glDepthMask(0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(CULL_FACE);
    glEnable(STENCIL_TEST);
    glStencilMask(UINT32_MAX);
    glStencilOp(STENCIL_KEEP, STENCIL_KEEP, STENCIL_KEEP);
    glStencilFunc(STENCIL_EQUAL, 0, UINT32_MAX);
    glViewport(0, 0, cl_ftoi(cl_data_viewport_width), cl_ftoi(cl_data_viewport_height));
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -16.0, 16.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glColor3f(1.0f, 1.0f, 1.0f);
    depth_quad();
    glEnd();
    glDisable(STENCIL_TEST);
    glEnable(GL_TEXTURE_2D);
    glColorMask(0, 1, 0, 0);
    depth_texture(sr_data_texturelist.sky);
    if (!options->ambient_occlusion)
        glDisable(GL_TEXTURE_2D);
    else {
        depth_program(PROGRAM_OCCLUSION);
        cl_glActiveTextureARB(TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        depth_texture(sr_data_texturelist.random_vectors);
    }
    glBegin(GL_QUADS);
    glColor3f(0.5f, 0.5f, 0.5f);
    depth_quad();
    glEnd();
    if (!options->ambient_occlusion)
        glEnable(GL_TEXTURE_2D);
    else {
        glDisable(GL_TEXTURE_2D);
        cl_glActiveTextureARB(0x84c0);
        depth_program(-1);
    }
    glColorMask(1, 1, 1, 0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(DEPTH_LEQUAL);
    glDepthMask(1);
    glEnable(CULL_FACE);
    glFrontFace(FRONT_CW);
    if (!options->framebuffer)
        depth_copy(sr_data_texturelist.shadow_map);
    else
        depth_attachment(sr_data_texturelist.frame_aux);
    if (options->ambient_occlusion) {
        float step[2];
        glColorMask(0, 1, 0, 0);
        glDepthMask(0);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        step[0] = (float)(1.125 / (float)cl_data_render_width);
        step[1] = 0.0f;
        blur_quad(step);
        step[0] = 0.0f;
        step[1] = (float)(1.125 / (float)cl_data_render_height);
        blur_quad(step);
        glColorMask(1, 1, 1, 0);
        glDepthMask(1);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
    stoptimer(sr_data_timerlist.renderdepth);
}
