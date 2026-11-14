/* The view of a pass: the viewport, the projection and the matrix of the camera or of an object. */
#include "render/view.h"

#include "client.h"
#include "sr_image_types.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define g_width cl_data_render_width
#define g_height cl_data_render_height
#define g_fonts cl_data_fonts
#define g_fontnum cl_data_fontnum

GL_IMPORT(glScissor, (int x, int y, int width, int height), (x, y, width, height))
GL_IMPORT(glFrustum, (double left, double right, double bottom, double top, double z_near, double z_far),
          (left, right, bottom, top, z_near, z_far))
GL_IMPORT(glMultMatrixf, (const float *matrix), (matrix))
GL_IMPORT(glTranslatef, (float x, float y, float z), (x, y, z))

/* The viewport and the scissor box for a rectangle in the units of the font: the size of the
 * window scales it. The rectangle starts at the top of the window, the viewport at the bottom. */
static void set_rectangle(int x, int y, int width, int height)
{
    const Font *font = &g_fonts[g_fontnum];
    int left = (x * g_width) / (int)font->resolution_x;
    int wide = (width * g_width) / (int)font->resolution_x;
    int bottom = (((int)font->resolution_y - (y + height)) * g_height) / (int)font->resolution_y;
    int high = (height * g_height) / (int)font->resolution_y;

    glViewport(left, bottom, wide, high);
    glScissor(left, bottom, wide, high);
}

/* Clear the projection, the texture and the model view matrices after a projection is set. */
static void reset_matrices(void)
{
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* The 2D view for text and bitmaps: blending on, no depth, the whole window, and the coordinates
 * of the font from the top left corner. */
void __cdecl setuptextdisplay(void)
{
    const Font *font = &g_fonts[g_fontnum];

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(0);
    glViewport(0, 0, g_width, g_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)(int)font->resolution_x, (double)(int)font->resolution_y, 0.0, -16.0, 16.0);
    reset_matrices();
}

/* A view with perspective on a rectangle. The three floats are the near plane, the far plane and
 * the scale of the width of the near plane. */
void __cdecl setupperspectiveviewport(int x, int y, int width, int height, float near_plane, float far_plane,
                                      float scale)
{
    set_rectangle(x, y, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum((double)(-scale * near_plane), (double)(scale * near_plane),
              (double)(-scale * ((float)((float)height / (float)width)) * near_plane),
              (double)(scale * ((float)height / (float)width) * near_plane), (double)near_plane, (double)far_plane);
    reset_matrices();
}

/* A view without perspective on a rectangle. The three floats are the half width, the half
 * height and the half depth of the box in view. */
void __cdecl setuporthoviewport(int x, int y, int width, int height, float half_width, float half_height,
                                float half_depth)
{
    set_rectangle(x, y, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho((double)-half_width, (double)half_width, (double)-half_height, (double)half_height, (double)-half_depth,
            (double)half_depth);
    reset_matrices();
}

/* Move the camera: the rotation of the camera (3 rows), then the move by the opposite of its place.
 * A null pointer leaves that part out. */
void __cdecl setupviewpoint(const void *place, const void *turn)
{
    const float *position = place;
    const float *rotation = turn;
    float matrix[16];

    glMatrixMode(GL_MODELVIEW);
    if (rotation != 0) {
        matrix[0] = rotation[0];
        matrix[4] = rotation[1];
        matrix[8] = rotation[2];
        matrix[12] = 0.0f;
        matrix[1] = rotation[3];
        matrix[5] = rotation[4];
        matrix[9] = rotation[5];
        matrix[13] = 0.0f;
        matrix[2] = rotation[6];
        matrix[6] = rotation[7];
        matrix[10] = rotation[8];
        matrix[14] = 0.0f;
        matrix[3] = 0.0f;
        matrix[7] = 0.0f;
        matrix[11] = 0.0f;
        matrix[15] = 1.0f;
        glMultMatrixf(matrix);
    }
    if (position != 0)
        glTranslatef(-position[0], -position[1], -position[2]);
}

/* Place an object: the move to its place, then its rotation (3 rows). A null pointer leaves that part
 * out. */
void __cdecl setupobjectrender(const void *place, const void *turn)
{
    const float *position = place;
    const float *rotation = turn;
    float matrix[16];

    glMatrixMode(GL_MODELVIEW);
    if (position != 0)
        glTranslatef(position[0], position[1], position[2]);
    if (rotation != 0) {
        matrix[0] = rotation[0];
        matrix[1] = rotation[1];
        matrix[2] = rotation[2];
        matrix[3] = 0.0f;
        matrix[4] = rotation[3];
        matrix[5] = rotation[4];
        matrix[6] = rotation[5];
        matrix[7] = 0.0f;
        matrix[8] = rotation[6];
        matrix[9] = rotation[7];
        matrix[10] = rotation[8];
        matrix[11] = 0.0f;
        matrix[12] = 0.0f;
        matrix[13] = 0.0f;
        matrix[14] = 0.0f;
        matrix[15] = 1.0f;
        glMultMatrixf(matrix);
    }
}
