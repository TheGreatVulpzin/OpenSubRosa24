/* The flat shapes of the 2D layer: bitmaps, boxes, circles and the mouse cursor.
 *
 * The screen is a canvas of 1024 by 640 units. A shape has a place and a size. The flags tell
 * how the place relates to the shape: bit 0 centers it on x, bit 1 puts the right edge on x,
 * bit 2 centers it on y and bit 3 puts the bottom edge on y.
 */
#include "render/draw2d.h"

#include "client.h"
#include "sr_math.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define g_textures cl_data_texture
#define g_fonts cl_data_fonts
#define g_fontnum cl_data_fontnum

#define NO_BITMAP (-1)
#define GL_LINES 0x0001
#define GL_LINE_SMOOTH 0x0b20
/* The font record holds a number at this offset that the cursor adds to its texture offset. */
#define FONT_CURSOR_FIELD 0x0c
#define CURSOR_TEXTURE_OFFSET 0x3f0
#define CURSOR_SCALE 1.5
#define CIRCLE_SEGMENTS 64
#define CIRCLE_STEP 0.0981747704246875
#define FLAG_CENTER_X 1
#define FLAG_RIGHT 2
#define FLAG_CENTER_Y 4
#define FLAG_BOTTOM 8

GL_IMPORT(glVertex2fv, (const float *vertex), (vertex))
GL_IMPORT(glTexCoord2f, (float s, float t), (s, t))
GL_IMPORT(glTexCoord2fv, (const float *coord), (coord))

/* Move the place of a shape by the flags. */
static void align_shape(float *x, float *y, float width, float height, unsigned flags)
{
    if ((flags & FLAG_CENTER_X) == 0) {
        if ((flags & FLAG_RIGHT) != 0)
            *x = (float)(*x - width);
    } else
        *x = (float)(*x - width * 0.5);
    if ((flags & FLAG_CENTER_Y) == 0) {
        if ((flags & FLAG_BOTTOM) != 0)
            *y = (float)(*y - height);
    } else
        *y = (float)(*y - height * 0.5);
}

/* Draw a rectangle with a texture, or a plain rectangle when the texture is -1. */
void __cdecl drawbitmap(int texture, float x, float y, float width, float height, unsigned flags, float red,
                        float green, float blue, float alpha)
{
    float vertex[2];

    align_shape(&x, &y, width, height, flags);
    if (texture == NO_BITMAP)
        glDisable(GL_TEXTURE_2D);
    else
        glBindTexture(GL_TEXTURE_2D, g_textures[texture].gl_name);
    glBegin(GL_QUADS);
    glColor4f(red, green, blue, alpha);
    vertex[0] = x;
    vertex[1] = y;
    glTexCoord2f(0.0f, 1.0f);
    glVertex2fv(vertex);
    vertex[0] = (float)(x + width);
    vertex[1] = y;
    glTexCoord2f(1.0f, 1.0f);
    glVertex2fv(vertex);
    vertex[0] = (float)(x + width);
    vertex[1] = (float)(y + height);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2fv(vertex);
    vertex[0] = x;
    vertex[1] = (float)(y + height);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2fv(vertex);
    glEnd();
    if (texture == NO_BITMAP)
        glEnable(GL_TEXTURE_2D);
}

/* Draw a rectangle with a texture. The coordinates of the four corners come from a list of
 * four pairs. A pair takes 8 bytes. */
void __cdecl drawbitmaptexcoord(int texture, float x, float y, float width, float height, unsigned flags,
                                float red, float green, float blue, float alpha, const float *coords)
{
    float vertex[2];

    align_shape(&x, &y, width, height, flags);
    glBindTexture(GL_TEXTURE_2D, g_textures[texture].gl_name);
    glBegin(GL_QUADS);
    glColor4f(red, green, blue, alpha);
    vertex[0] = x;
    vertex[1] = y;
    glTexCoord2fv(coords);
    glVertex2fv(vertex);
    vertex[0] = (float)(x + width);
    vertex[1] = y;
    glTexCoord2fv(coords + 2);
    glVertex2fv(vertex);
    vertex[0] = (float)(x + width);
    vertex[1] = (float)(y + height);
    glTexCoord2fv(coords + 4);
    glVertex2fv(vertex);
    vertex[0] = x;
    vertex[1] = (float)(y + height);
    glTexCoord2fv(coords + 6);
    glVertex2fv(vertex);
    glEnd();
}

/* Draw the outline of a rectangle with smooth lines. */
void __cdecl drawlinebox(float x, float y, float width, float height, unsigned flags, float red, float green,
                         float blue, float alpha)
{
    float vertex[2];

    align_shape(&x, &y, width, height, flags);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LINE_SMOOTH);
    glBegin(GL_LINES);
    glColor4f(red, green, blue, alpha);
    vertex[0] = x;
    vertex[1] = y;
    glVertex2fv(vertex);
    vertex[0] = (float)(x + width);
    vertex[1] = y;
    glVertex2fv(vertex);
    glVertex2fv(vertex);
    vertex[0] = (float)(x + width);
    vertex[1] = (float)(y + height);
    glVertex2fv(vertex);
    glVertex2fv(vertex);
    vertex[0] = x;
    vertex[1] = (float)(y + height);
    glVertex2fv(vertex);
    glVertex2fv(vertex);
    vertex[0] = x;
    vertex[1] = y;
    glVertex2fv(vertex);
    glEnd();
    glDisable(GL_LINE_SMOOTH);
    glEnable(GL_TEXTURE_2D);
}

/* Draw the mouse cursor: a square of the cursor texture of the font, around the point. */
void __cdecl drawmousecursor(int x, int y, int size, float red, float green, float blue, float alpha)
{
    float vertex[2];
    int texture = *(int *)((char *)&g_fonts[g_fontnum] + FONT_CURSOR_FIELD) + CURSOR_TEXTURE_OFFSET;

    glBindTexture(GL_TEXTURE_2D, g_textures[texture].gl_name);
    glBegin(GL_QUADS);
    vertex[0] = (float)((float)x - (float)size * CURSOR_SCALE);
    vertex[1] = (float)((float)y - (float)size * CURSOR_SCALE);
    glColor4f(red, green, blue, alpha);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2fv(vertex);
    vertex[0] = (float)((float)size * CURSOR_SCALE + (float)x);
    vertex[1] = (float)((float)y - (float)size * CURSOR_SCALE);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2fv(vertex);
    vertex[0] = (float)((float)size * CURSOR_SCALE + (float)x);
    vertex[1] = (float)((float)size * CURSOR_SCALE + (float)y);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2fv(vertex);
    vertex[0] = (float)((float)x - (float)size * CURSOR_SCALE);
    vertex[1] = (float)((float)size * CURSOR_SCALE + (float)y);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2fv(vertex);
    glEnd();
}

/* Set a vertex on the circle at an angle. */
static void circle_vertex(float x, float y, float radius, float angle)
{
    float vertex[2];

    vertex[0] = (float)(x87_sin(angle) * radius + x);
    vertex[1] = (float)(y - x87_cos(angle) * radius);
    glVertex2fv(vertex);
}

/* Draw the outline of a circle with 64 smooth line segments. */
void __cdecl drawcircle(float x, float y, float radius, float red, float green, float blue, float alpha)
{
    float angle = 0.0f;
    int segment;

    glEnable(GL_LINE_SMOOTH);
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_LINES);
    glColor4f(red, green, blue, alpha);
    for (segment = 0; segment < CIRCLE_SEGMENTS; ++segment) {
        circle_vertex(x, y, radius, angle);
        angle = (float)(angle + CIRCLE_STEP);
        circle_vertex(x, y, radius, angle);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LINE_SMOOTH);
}

/* Draw a filled circle as 64 triangles around the center. */
void __cdecl drawcirclefill(float x, float y, float radius, float red, float green, float blue, float alpha)
{
    float angle = 0.0f;
    float vertex[2];
    int segment;

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLES);
    glColor4f(red, green, blue, alpha);
    for (segment = 0; segment < CIRCLE_SEGMENTS; ++segment) {
        vertex[0] = x;
        vertex[1] = y;
        glVertex2fv(vertex);
        circle_vertex(x, y, radius, angle);
        angle = (float)(angle + CIRCLE_STEP);
        circle_vertex(x, y, radius, angle);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}
