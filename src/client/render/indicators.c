/* The round indicators of the 2D layer: the pointer of a player and the compass circle. */
#include "render/indicators.h"

#include "client.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define GL_LINES 0x0001

#define CIRCLE_SEGMENTS 64
#define CIRCLE_STEP 0.0981747704246875
#define POINTER_RISE_SEGMENTS 32
#define POINTER_MAX_SCALE 1.75
#define POINTER_SCALE_STEP 0.25
#define POINTER_MIN_SCALE 1.0
#define COMPASS_SHADE_ALPHA 0.25f
#define COMPASS_LINE_WIDTH 2.0f
#define COMPASS_NEEDLE_GREY 0.75f

GL_IMPORT(glVertex2fv, (const float *vertex), (vertex))
GL_IMPORT(glColor3f, (float red, float green, float blue), (red, green, blue))
GL_IMPORT(glLineWidth, (float width), (width))

/* The size of the pointer at a segment: largest at the start and the end, smaller in the middle. */
static float pointer_scale(int segment)
{
    int distance = segment < POINTER_RISE_SEGMENTS ? segment : CIRCLE_SEGMENTS - segment;
    float scale = (float)(POINTER_MAX_SCALE - (float)distance * POINTER_SCALE_STEP);

    if (scale < POINTER_MIN_SCALE)
        scale = (float)POINTER_MIN_SCALE;
    return scale;
}

/* Draw the pointer of a player: a filled ring whose width changes along the circle. The angle
 * is where the ring starts. */
void __cdecl drawplayerindicator(float x, float y, float radius, float red, float green, float blue, float alpha,
                                 float angle)
{
    float vertex[2];
    float scale;
    int segment;

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLES);
    glColor4f(red, green, blue, alpha);
    for (segment = 0; segment < CIRCLE_SEGMENTS; ++segment) {
        scale = pointer_scale(segment);
        vertex[0] = x;
        vertex[1] = y;
        glVertex2fv(vertex);
        vertex[0] = (float)(x87_sin(angle) * radius * scale + x);
        vertex[1] = (float)(y - x87_cos(angle) * radius * scale);
        glVertex2fv(vertex);
        angle = (float)(angle + CIRCLE_STEP);
        scale = pointer_scale(segment + 1);
        vertex[0] = (float)(x87_sin(angle) * radius * scale + x);
        vertex[1] = (float)(y - x87_cos(angle) * radius * scale);
        glVertex2fv(vertex);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

/* Set a vertex on the circle at an angle. */
static void circle_vertex(float x, float y, float radius, float angle)
{
    float across = (float)(y - x87_cos(angle) * radius);

    glVertex2f((float)(x87_sin(angle) * radius + x), across);
}

/* Draw the compass circle: a dark disc, a black outline and a grey needle at the angle. */
void __cdecl drawcircleindicator(float x, float y, float radius, float needle)
{
    float angle = 0.0f;
    int segment;

    glDisable(GL_TEXTURE_2D);
    glColor4f(0.0f, 0.0f, 0.0f, COMPASS_SHADE_ALPHA);
    glBegin(GL_TRIANGLES);
    for (segment = 0; segment < CIRCLE_SEGMENTS; ++segment) {
        glVertex2f(x, y);
        circle_vertex(x, y, radius, angle);
        angle = (float)(angle + CIRCLE_STEP);
        circle_vertex(x, y, radius, angle);
    }
    glEnd();
    glLineWidth(COMPASS_LINE_WIDTH);
    glBegin(GL_LINES);
    glColor3f(0.0f, 0.0f, 0.0f);
    angle = 0.0f;
    for (segment = 0; segment < CIRCLE_SEGMENTS; ++segment) {
        circle_vertex(x, y, radius, angle);
        angle = (float)(angle + CIRCLE_STEP);
        circle_vertex(x, y, radius, angle);
    }
    glColor3f(COMPASS_NEEDLE_GREY, COMPASS_NEEDLE_GREY, COMPASS_NEEDLE_GREY);
    glVertex2f(x, y);
    circle_vertex(x, y, radius, needle);
    glEnd();
    glEnable(GL_TEXTURE_2D);
    glLineWidth(1.0f);
}
