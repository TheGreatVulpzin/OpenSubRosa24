/* The textures that the client makes at the start of the game.
 *
 * A generator fills the shared image (width, height and one word for each pixel), copies it
 * into a texture record and sends the texture to OpenGL. A pixel holds red in its low byte,
 * then green, blue and alpha. The billboard and the newspaper are drawn with the text and
 * bitmap functions and then copied from the screen into the texture.
 */
#include "textures/textures.h"

#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "random.h"
#include "sr_math.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define g_textures cl_data_texture
#define g_texturelist cl_data_texturelist
#define g_image cl_data_image
#define g_bigendian cl_data_bigendian
#define g_fontnum cl_data_fontnum
#define g_fontlist cl_data_fontlist
#define g_newspaper sr_data_newspaper

#define PI 3.14159265359

#define OPAQUE 0xff
#define MAX_ANISOTROPY 16.0f

/* The font of the printed textures (an entry of fontlist) and the font of the menus. */
#define PRINT_FONT 1
#define MENU_FONT 0

/* The size of the text grid while a printed texture is drawn, and the size the game uses. */
#define PRINT_RESOLUTION 0x200
#define SCREEN_FONT_WIDTH 0x400
#define SCREEN_FONT_HEIGHT 0x280

/* Text flags of drawtext. */
#define TEXT_CENTER 1
#define TEXT_BILLBOARD 0x45

#define NO_BITMAP (-1) /* drawbitmap draws a plain rectangle */

/* Game functions of other files. */
#define setuptexture CL_FN(0x50ac40, void(__cdecl *)(int texture))
#define getrandomvector3d CL_FN(0x4c5700, void(__cdecl *)(vec3 *out, float range, float min))
#define drawtext CL_FN(0x506b30, void(__cdecl *)(const char *text, float x, float y, float size, uint32_t flags, \
                                                 float red, float green, float blue, float alpha, ...))
#define drawbitmap CL_FN(0x507ac0, void(__cdecl *)(int texture, float x, float y, float width, float height, \
                                                   uint32_t flags, float red, float green, float blue, float alpha))
#define setfontresolution CL_FN(0x509f30, void(__cdecl *)(int width, int height))

/* The word of a pixel. On a big endian machine the bytes have the other order. */
uint32_t pack_pixel(int red, int green, int blue, int alpha)
{
    uint32_t r = (uint32_t)red, g = (uint32_t)green, b = (uint32_t)blue, a = (uint32_t)alpha;
    if (g_bigendian == 0)
        return g * 0x100u + b * 0x10000u + a * 0x1000000u + r;
    return b * 0x100u + r * 0x1000000u + g * 0x10000u + a;
}

/* Make a texture with one level from the shared image. The old pixels of level 0 are freed. */
void texture_from_image(int texture, int wrap_s, int wrap_t, int mag_filter, int min_filter)
{
    Texture *t = &g_textures[texture];
    size_t bytes;

    t->width = g_image.width;
    t->height = g_image.height;
    t->num_levels = 1;
    t->format = GL_RGBA;
    t->internal_format = GL_RGBA;
    t->spare = 0;
    t->wrap_s = wrap_s;
    t->wrap_t = wrap_t;
    t->mag_filter = mag_filter;
    t->min_filter = min_filter;
    os_free(t->levels[0]);
    bytes = (size_t)(t->width * t->height * 4);
    t->levels[0] = os_malloc(bytes);
    memcpy(t->levels[0], g_image.pixels, bytes);
}

/* Make the smaller levels of a texture. Each pixel is the mean of 4 pixels of the level
 * above. The number of levels comes from the width. Only RGB and RGBA textures get levels. */
void __cdecl generatemipmap(int texture)
{
    Texture *t = &g_textures[texture];
    int levels, level;

    if (t->format != GL_RGBA && t->format != GL_RGB)
        return;
    for (levels = 1; (t->width >> levels) != 0; levels++)
        ;
    t->num_levels = levels;
    for (level = 1; level < levels; level++) {
        int width = t->width >> level;
        int height = t->height >> level;
        int above_width = t->width >> (level - 1);
        const uint32_t *above;
        uint32_t *out;
        int x, y;

        os_free(t->levels[level]);
        t->levels[level] = os_malloc((size_t)(width * height * 4));
        above = t->levels[level - 1];
        out = t->levels[level];
        for (y = 0; y < height; y++) {
            const uint32_t *row0 = &above[(y * 2) * above_width];
            const uint32_t *row1 = &above[(y * 2 + 1) * above_width];
            for (x = 0; x < width; x++) {
                uint32_t p[4];
                uint32_t red = 0, green = 0, blue = 0, alpha = 0;
                int i;
                p[0] = row0[x * 2];
                p[1] = row0[x * 2 + 1];
                p[2] = row1[x * 2];
                p[3] = row1[x * 2 + 1];
                for (i = 0; i < 4; i++) {
                    red += p[i] & 0xff;
                    green += (p[i] >> 8) & 0xff;
                    blue += (p[i] >> 16) & 0xff;
                    alpha += p[i] >> 24;
                }
                out[width * y + x] = (green >> 2) * 0x100u + (alpha >> 2) * 0x1000000u + (blue >> 2) * 0x10000u + (red >> 2);
            }
        }
    }
}

/* Make a texture of 64 x 8 random unit vectors: x, y and z in red, green and blue. */
void __cdecl generatetexture_randomvec(int texture)
{
    int x, y;

    g_image.width = 0x40;
    g_image.height = 8;
    for (y = 0; y < g_image.height; y++) {
        for (x = 0; x < g_image.width; x++) {
            vec3 v;
            int red, green, blue;
            getrandomvector3d(&v, 0.0f, 1.0f);
            red = (int)(long long)(v.x * 127.0 + 128.0);
            green = (int)(long long)(v.y * 127.0 + 128.0);
            blue = (int)(long long)(v.z * 127.0 + 128.0);
            g_image.pixels[y * g_image.width + x] = pack_pixel(red, green, blue, OPAQUE);
        }
    }
    texture_from_image(texture, GL_REPEAT, GL_REPEAT, GL_NEAREST, GL_NEAREST);
    setuptexture(texture);
}

/* Four layers of smooth noise at the point (x, y) of a carpet, each layer half as fine and
 * with a smaller weight. The result is the inverse of the sum, divided by 64. */
static float carpet_grain(int x, int y)
{
    float fine = (float)fabs(cerpnoise2d((float)(x * 0.25), (float)(y * 0.25), 0x1f));
    float medium = (float)fabs(cerpnoise2d((float)(x * 0.25 * 0.5), (float)(y * 0.25 * 0.5), 0xf));
    float coarse = (float)fabs(cerpnoise2d((float)(x * 0.25 * 0.25), (float)(y * 0.25 * 0.25), 7));
    float wide = (float)fabs(cerpnoise2d((float)(x * 0.25 * 0.125), (float)(y * 0.25 * 0.125), 3));
    float sum = (float)(medium * 0.5 + fine);
    sum = (float)(coarse * 0.25 + sum);
    sum = (float)(wide * 0.125 + sum);
    return (float)((float)(1.0 / sum) * 0.125 * 0.125);
}

/* Make the carpet texture of a material: light diamonds, 64 pixels apart, on a dark red
 * ground. A wave of the noise changes the brightness. */
void __cdecl generatetexture_carpet(int material)
{
    int texture = g_texturelist.material[material];
    int x, y;

    g_image.width = 0x80;
    g_image.height = 0x80;
    for (y = 0; y < g_image.height; y++) {
        for (x = 0; x < g_image.width; x++) {
            int from_center = abs(0x20 - (y & 0x3f)) + abs(0x20 - (x & 0x3f));
            float grain = carpet_grain(x, y);
            float wave = (float)(x87_sin((grain + grain) * PI) * 32.0);
            int red, green, blue;
            if (from_center < 0x20) {
                red = green = blue = (int)(long long)(wave + 112.0);
            } else {
                red = (int)(long long)(wave * 0.25 + 64.0);
                green = (int)(long long)(wave * 0.125 + 56.0);
                blue = (int)(long long)(wave * 0.25 + 48.0);
            }
            g_image.pixels[y * g_image.width + x] = pack_pixel(red, green, blue, 0x20);
        }
    }
    texture_from_image(texture, GL_REPEAT, GL_REPEAT, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
    generatemipmap(texture);
    setuptexture(texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, MAX_ANISOTROPY);
}

/* Make a texture of 1024 x 1024 with no pixels of its own. OpenGL gets the pixels from the
 * screen later. */
static void start_printed_texture(int texture)
{
    Texture *t = &g_textures[texture];

    g_image.width = 0x400;
    g_image.height = 0x400;
    t->width = 0x400;
    t->height = g_image.height;
    t->num_levels = 1;
    t->format = GL_RGBA;
    t->internal_format = GL_RGBA;
    t->spare = 0;
    t->wrap_s = GL_REPEAT;
    t->wrap_t = GL_REPEAT;
    t->mag_filter = GL_LINEAR;
    t->min_filter = GL_LINEAR;
    os_free(t->levels[0]);
    t->levels[0] = 0;
    setuptexture(texture);
}

/* Set OpenGL to draw flat on a page of 512 x 512 units, clear the page to white with alpha 0,
 * and choose the print font. The later drawing does not change alpha. */
static void begin_page(int view_width, int view_height)
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, view_width, view_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 512.0, 512.0, 0.0, -16.0, 16.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    setfontresolution(PRINT_RESOLUTION, PRINT_RESOLUTION);
    glColorMask(1, 1, 1, 1);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(512.0f, 0.0f);
    glVertex2f(512.0f, 512.0f);
    glVertex2f(0.0f, 512.0f);
    glEnd();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glColorMask(1, 1, 1, 0);
    g_fontnum = g_fontlist[PRINT_FONT];
}

/* Put the OpenGL state and the font size back to the values of the game. */
static void end_page(void)
{
    glEnable(GL_BLEND);
    glColorMask(1, 1, 1, 0);
    setfontresolution(SCREEN_FONT_WIDTH, SCREEN_FONT_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(1);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, MAX_ANISOTROPY);
}

/* Make the texture of the billboard material: three advertisements. */
void __cdecl generatetexture_billboard(int material)
{
    int texture = g_texturelist.material[material];

    start_printed_texture(texture);
    begin_page(g_image.width, g_image.height);
    drawtext("Cars Cars Cars", 128.0f, 288.0f, 48.0f, TEXT_BILLBOARD, 0.0f, 0.0f, 0.0f, 1.0f);
    drawbitmap(NO_BITMAP, 0.0f, 128.0f, 256.0f, 64.0f, 0, 0.5f, 0.5f, 0.0f, 1.0f);
    drawtext("Goldmen", 128.0f, 160.0f, 88.0f, TEXT_BILLBOARD, 0.0f, 0.0f, 0.0f, 1.0f);
    drawbitmap(NO_BITMAP, 0.0f, 64.0f, 256.0f, 64.0f, 0, 0.0f, 0.0f, 0.5f, 1.0f);
    drawtext("Monsota", 128.0f, 96.0f, 88.0f, TEXT_BILLBOARD, 1.0f, 1.0f, 0.0f, 1.0f);
    g_fontnum = MENU_FONT;
    glBindTexture(GL_TEXTURE_2D, g_textures[texture].gl_name);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_image.width, g_image.height);
    end_page();
}

/* Make the texture of the newspaper from its lines of text. The page is drawn 4 times, each
 * time moved by half a page, and each drawing fills one quarter of the texture. Line 0 is
 * the headline. */
void __cdecl generatetexture_newspaper(void)
{
    int texture = g_texturelist.newspaper;
    int row, column, line;

    start_printed_texture(texture);
    begin_page(PRINT_RESOLUTION, PRINT_RESOLUTION);
    for (row = 0; row < 0x400; row += PRINT_RESOLUTION) {
        for (column = 0; column < 0x400; column += PRINT_RESOLUTION) {
            float y = 120.0f;
            drawbitmap(NO_BITMAP, 0.0f, 0.0f, 512.0f, 512.0f, 0, 0.5f, 0.5f, 0.5f, 1.0f);
            drawtext("The Readme", (float)(512.0 - column), (float)(4.0 - row), 52.0f, TEXT_CENTER, 0.0f, 0.0f, 0.0f, 1.0f);
            drawtext(g_newspaper.lines[0], (float)(512.0 - column), (float)(56.0 - row), 32.0f, TEXT_CENTER, 0.0f, 0.0f,
                     0.0f, 1.0f);
            for (line = 1; line < g_newspaper.num_lines; line++) {
                drawtext(g_newspaper.lines[line], (float)(8.0 - column), (float)(y - row), 36.0f, 0, 0.0f, 0.0f, 0.0f, 1.0f);
                y = (float)(y + 36.0);
            }
            glBindTexture(GL_TEXTURE_2D, g_textures[texture].gl_name);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, column, PRINT_RESOLUTION - row, 0, 0, PRINT_RESOLUTION, PRINT_RESOLUTION);
        }
    }
    g_fontnum = MENU_FONT;
    end_page();
    g_newspaper.texture_done = 1;
}
