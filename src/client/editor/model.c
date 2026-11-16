/* The model editor: the player moves the vertices of the model in work with the mouse. */
#include "editor/model.h"

#include "client.h"
#include "intersect.h"
#include "math/small_vectors.h"
#include "sr_runtime.h"
#include "vectors.h"

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
    copyvector((unsigned int *)&from, (const unsigned int *)&cl_data_camera_position);
    scaleaddvectors(&to, &from, &cl_data_camera_rotation[2], -512.0f);
    scaleaddvectors(&to, &to, &cl_data_camera_rotation[0], (float)(mx * RAY_LENGTH));
    scaleaddvectors(&to, &to, &cl_data_camera_rotation[1], (float)(my * RAY_LENGTH));
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
                scaleaddvectors(&cl_data_edit_vertices[i], &cl_data_edit_vertices[i], &cl_data_camera_rotation[0],
                                (float)((mx - cl_data_edit_mouse_before[0]) * 0.5));
                scaleaddvectors(&cl_data_edit_vertices[i], &cl_data_edit_vertices[i], &cl_data_camera_rotation[1],
                                (float)((my - cl_data_edit_mouse_before[1]) * 0.5));
            }
    cl_data_edit_mouse_before[0] = mx;
    cl_data_edit_mouse_before[1] = my;
    for (i = 0; i < model->num_verts; i++)
        copyvector((unsigned int *)&model->verts[i], (const unsigned int *)&cl_data_edit_vertices[i]);
}
