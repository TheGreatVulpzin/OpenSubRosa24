/* The debug shapes of the 3D view: a bounding box between two corners and a box at an object. */
#include "render/debug_draw.h"

#include "client.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define GL_LINES 0x0001
/* The eight corners of a unit box: four at the bottom, then four at the top, in order around. */
#define BOX_CORNERS 0x122a30

GL_IMPORT(glPushMatrix, (void), ())
GL_IMPORT(glPopMatrix, (void), ())
GL_IMPORT(glVertex3f, (float x, float y, float z), (x, y, z))
GL_IMPORT(glVertex3fv, (const float *vertex), (vertex))
GL_IMPORT(glColor3f, (float red, float green, float blue), (red, green, blue))

/* Game functions of other files. */
#define setupobjectrender CL_FN(0x5066b0, void(__cdecl *)(const void *placement, const void *rotation))

/* Add the line between two corners of the box given by its smallest and largest corner. */
static void box_line(const float *low, const float *high, int ax, int ay, int az, int bx, int by, int bz)
{
    glVertex3f(ax ? high[0] : low[0], ay ? high[1] : low[1], az ? high[2] : low[2]);
    glVertex3f(bx ? high[0] : low[0], by ? high[1] : low[1], bz ? high[2] : low[2]);
}

/* Draw the edges of the box between two corners in white. */
void __cdecl debug_renderboundingbox(const float *low, const float *high)
{
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    glColor3f(1.0f, 1.0f, 1.0f);
    box_line(low, high, 0, 0, 0, 1, 0, 0);
    box_line(low, high, 1, 0, 0, 1, 1, 0);
    box_line(low, high, 1, 1, 0, 0, 1, 0);
    box_line(low, high, 0, 1, 0, 0, 0, 0);
    box_line(low, high, 0, 0, 0, 0, 0, 1);
    box_line(low, high, 1, 0, 0, 1, 0, 1);
    box_line(low, high, 1, 1, 0, 1, 1, 1);
    box_line(low, high, 0, 1, 0, 0, 1, 1);
    box_line(low, high, 1, 1, 1, 0, 1, 1);
    box_line(low, high, 0, 1, 1, 0, 0, 1);
    box_line(low, high, 0, 0, 1, 1, 0, 1);
    box_line(low, high, 1, 0, 1, 1, 1, 1);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

/* Scale a corner of the unit box and add it as a vertex. */
static void scaled_corner(const float *corner, float x, float y, float z)
{
    float vertex[3];

    vertex[0] = (float)(corner[0] * x);
    vertex[1] = (float)(corner[1] * y);
    vertex[2] = (float)(corner[2] * z);
    glVertex3fv(vertex);
}

/* Draw the edges of a box at an object in red. The size is the scale along each axis. */
void __cdecl debug_renderbox(const void *placement, const void *rotation, float x, float y, float z)
{
    const float *corners = (const float *)(sr_base + BOX_CORNERS);
    int side;

    glPushMatrix();
    setupobjectrender(placement, rotation);
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f);
    for (side = 0; side < 4; ++side) {
        int next = (side + 1) & 3;

        scaled_corner(corners + side * 3, x, y, z);
        scaled_corner(corners + next * 3, x, y, z);
        scaled_corner(corners + (side + 4) * 3, x, y, z);
        scaled_corner(corners + (next + 4) * 3, x, y, z);
        scaled_corner(corners + side * 3, x, y, z);
        scaled_corner(corners + (side + 4) * 3, x, y, z);
    }
    glEnd();
    glPopMatrix();
}
