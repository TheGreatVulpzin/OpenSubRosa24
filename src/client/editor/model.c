/* The model editor: the player moves the vertices of the model in work with the mouse. */
#include "editor/model.h"

#include "audio/imports.h"
#include "client.h"
#include "input/key_events.h"
#include "textures/head_models.h"
#include "textures/models.h"
#include "intersect.h"
#include "math/small_vectors.h"
#include "menu/draw.h"
#include "menu/items.h"
#include "render/draw2d.h"
#include "render/models_draw.h"
#include "render/view.h"
#include "sr_runtime.h"
#include "textures/loaders.h"
#include "vectors.h"
#include "input/keyboard.h"
#include "input/mouse.h"
#include "render/text.h"

#define VERTEX_RADIUS 0.03125f
#define RAY_LENGTH 512.0

/* Move the vertices of the model with the mouse. A click grabs each vertex near the mouse ray.
 * While the button stays down, the grabbed vertices move with the mouse in the plane of the
 * screen, at half the speed of the mouse. */
void __cdecl model_editor(void)
{
    ModelTemp *model = &cl_data_modeltemp;
    vec3 hit, normal, from, to;
    float t, mx, my;
    int i;

    for (i = 0; i < model->num_verts; i++)
        copyvector((unsigned int *)&cl_data_edit_vertices[i], (const unsigned int *)&model->verts[i]);
    /* The mouse on the screen from -1 to 1 in x, with the same scale in y (y up). */
    mx = (float)(((double)cl_data_input_mouse.x - 512.0) / 512.0);
    my = (float)((320.0 - (double)cl_data_input_mouse.y) / 512.0);
    copyvector((unsigned int *)&from, (const unsigned int *)&cl_data_audio_camera.pos);
    scaleaddvectors(&to, &from, &cl_data_audio_camera.rot[2], -512.0f);
    scaleaddvectors(&to, &to, &cl_data_audio_camera.rot[0], (float)(mx * RAY_LENGTH));
    scaleaddvectors(&to, &to, &cl_data_audio_camera.rot[1], (float)(my * RAY_LENGTH));
    if (cl_data_input_mouse.left != 0 && cl_data_input_mouse_previous.left == 0) {
        for (i = 0; i < model->num_verts; i++) {
            cl_data_edit_grabbed[i] = 0;
            if (lineintersectsphere(&hit, &normal, &t, &from, &to, &cl_data_edit_vertices[i], VERTEX_RADIUS)) {
                cl_data_edit_grabbed[i] = 1;
                subtractvectors(&cl_data_edit_offsets[i], &cl_data_edit_vertices[i], &hit);
            }
        }
    }
    if (cl_data_input_mouse.left != 0)
        for (i = 0; i < model->num_verts; i++)
            if (cl_data_edit_grabbed[i] != 0) {
                scaleaddvectors(&cl_data_edit_vertices[i], &cl_data_edit_vertices[i], &cl_data_audio_camera.rot[0],
                                (float)((mx - cl_data_edit_mouse_before[0]) * 0.5));
                scaleaddvectors(&cl_data_edit_vertices[i], &cl_data_edit_vertices[i], &cl_data_audio_camera.rot[1],
                                (float)((my - cl_data_edit_mouse_before[1]) * 0.5));
            }
    cl_data_edit_mouse_before[0] = mx;
    cl_data_edit_mouse_before[1] = my;
    for (i = 0; i < model->num_verts; i++)
        copyvector((unsigned int *)&model->verts[i], (const unsigned int *)&cl_data_edit_vertices[i]);
}

/* --- model_viewer --- */

#define KEY_ESCAPE 0x29
#define KEY_LEFT_SHIFT 0xe1
#define OPTION_KEY 1
#define VIEW_TURN 3.1415927410125732f
#define SHIFT_TURN -1.5707963705062866f
#define SPHERE_SIZE 0.015625f
#define GL_TEXTURE_2D 0xde1
#define GL_CLAMP_TO_EDGE 0x812f
#define GL_LINEAR 0x2601
#define GL_LIGHT0 0x4000
#define GL_AMBIENT 0x1200
#define GL_DIFFUSE 0x1201
#define GL_POSITION 0x1203
#define GL_FRONT 0x404
#define GL_BACK 0x405
#define GL_LINE 0x1b01
#define GL_FILL 0x1b02

AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glEnable, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glDisable, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glDepthFunc, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glDepthMask, (unsigned char))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glFrontFace, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClearColor, (float, float, float, float))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glClear, (unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glLightfv, (unsigned, unsigned, const float *))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glPolygonMode, (unsigned, unsigned))
AUDIO_IMPORT("OPENGL32.DLL", __stdcall, void, glColor3f, (float, float, float))
AUDIO_IMPORT("SDL2.DLL", __cdecl, void, SDL_GL_SwapWindow, (void *))

/* Place the camera half a unit behind the center and turn it to face the model. Left shift turns
 * it a quarter more. Light 0 shines from the front in white. */
static void viewer_view(void)
{
    zerovector((unsigned int *)&cl_data_audio_camera.pos);
    resetorientation(cl_data_audio_camera.rot);
    rotateorientation(cl_data_audio_camera.rot, &sr_data_yaxis, VIEW_TURN);
    if (cl_data_keyboard[KEY_LEFT_SHIFT] != 0)
        rotateorientation(cl_data_audio_camera.rot, &sr_data_yaxis, SHIFT_TURN);
    scaleaddvectors(&cl_data_audio_camera.pos, &cl_data_audio_camera.pos, &cl_data_audio_camera.rot[2], 0.5f);
    setupperspectiveviewport(0, 0, 0x400, 0x280, 0.125f, 1024.0f, 1.0f);
    setupviewpoint(&cl_data_audio_camera.pos, cl_data_audio_camera.rot);
    cl_data_light_position[0] = 0.0f;
    cl_data_light_position[1] = 0.0f;
    cl_data_light_position[2] = 1.0f;
    cl_data_light_position[3] = 0.0f;
    audio_glLightfv()(GL_LIGHT0, GL_POSITION, cl_data_light_position);
    cl_data_light_ambient[0] = 1.0f;
    cl_data_light_ambient[1] = 1.0f;
    cl_data_light_ambient[2] = 1.0f;
    cl_data_light_diffuse[0] = 1.0f;
    cl_data_light_diffuse[1] = 1.0f;
    cl_data_light_diffuse[2] = 1.0f;
    audio_glLightfv()(GL_LIGHT0, GL_AMBIENT, cl_data_light_ambient);
    audio_glLightfv()(GL_LIGHT0, GL_DIFFUSE, cl_data_light_diffuse);
}

/* Draw the head model as lines, and a small sphere on each vertex of the model in work: white
 * when the mouse holds it, else grey. */
static void viewer_model(void)
{
    static const float white[3] = {1.0f, 1.0f, 1.0f};
    vec3 place, rot[3];
    int textures[2], i;

    zerovector((unsigned int *)&place);
    resetorientation(rot);
    textures[0] = sr_data_texturelist.human[0];
    textures[1] = sr_data_texturelist.human[0];
    audio_glPolygonMode()(GL_FRONT, GL_LINE);
    audio_glPolygonMode()(GL_BACK, GL_LINE);
    rendermodel(sr_data_modellist.head, &place, rot, 2, textures, white);
    audio_glPolygonMode()(GL_FRONT, GL_FILL);
    audio_glPolygonMode()(GL_BACK, GL_FILL);
    audio_glDisable()(GL_TEXTURE_2D);
    for (i = 0; i < cl_data_modeltemp.num_verts; i++) {
        copyvector((unsigned int *)&place, (const unsigned int *)&cl_data_edit_vertices[i]);
        audio_glColor3f()(0.5f, 0.5f, 0.5f);
        if (cl_data_edit_grabbed[i] != 0)
            audio_glColor3f()(1.0f, 1.0f, 1.0f);
        rendersphere(&place, SPHERE_SIZE, 1);
    }
    audio_glEnable()(GL_TEXTURE_2D);
}

/* Build the head model again from the model in work. setupmodel writes over the triangle list,
 * so the triangles go to a copy first and come back after. */
static void viewer_rebuild(void)
{
    ModelTemp *model = &cl_data_modeltemp;
    int i;

    for (i = 0; i < model->num_tris; i++)
        model->tri_backup[i] = model->tris[i];
    setupmodel(sr_data_modellist.head, 1);
    for (i = 0; i < model->num_tris; i++)
        model->tris[i] = model->tri_backup[i];
}

/* The model viewer: it shows the head model and lets the player move the vertices of the model
 * in work with the mouse. The numbers of the held vertices show on the left. */
void __cdecl model_viewer(void)
{
    createheadmodel3(sr_data_modellist.head);
    loadtexture(sr_data_texturelist.test_m16, "test_m16.png", 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_LINEAR,
                GL_LINEAR);
    resetmenuitems();
    while (cl_data_menu_items[0].pressed == 0 && cl_data_audio_menu_back == 0) {
        float y = 64.0f;
        int i;

        cl_data_menu_item_count = 0;
        createmenubutton("Exit", 4.0f, 4.0f, 192.0f, 32.0f, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f);
        setmenuitem(OPTION_KEY, KEY_ESCAPE);
        checksystemmessages();
        checkkeyboard();
        checkmouse();
        checkmenuitems();
        audio_glEnable()(0xb71);    /* depth test */
        audio_glDepthFunc()(0x203); /* less or equal */
        audio_glDepthMask()(1);
        audio_glDisable()(0xbe2); /* blend */
        audio_glEnable()(0xb44);  /* cull face */
        audio_glFrontFace()(0x900);
        audio_glClearColor()(0.25f, 0.25f, 0.25f, 0.0f);
        audio_glClear()(0x4100);
        viewer_view();
        viewer_model();
        setuptextdisplay();
        for (i = 0; i < cl_data_modeltemp.num_verts; i++)
            if (cl_data_edit_grabbed[i] != 0) {
                drawtext("/i", 4.0f, y, 16.0f, 0, 1.0f, 1.0f, 1.0f, 1.0f, i);
                y = (float)(y + 16.0);
            }
        drawmenuitems();
        drawmousecursor(cl_data_input_mouse.x, cl_data_input_mouse.y, 24, 1.0f, 1.0f, 1.0f, 1.0f);
        audio_SDL_GL_SwapWindow()(cl_data_audio_window);
        model_editor();
        viewer_rebuild();
    }
    resetmenuitems();
}
