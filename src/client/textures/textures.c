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
#include "textures/noise.h"
#include "textures/opengl.h"

#define g_textures cl_data_texture
#define g_texturelist cl_data_texturelist
#define g_image cl_data_image
#define g_bigendian cl_data_bigendian
#define g_fontnum cl_data_fontnum
#define g_fontlist cl_data_fontlist
#define g_newspaper sr_data_newspaper
#define g_windowatlas cl_data_texture_windowatlas
#define g_windowrect cl_data_windowrect
#define g_humanlook cl_data_texture_human
#define g_materiallist sr_data_materiallist

#define PI 3.14159265359

#define OPAQUE 0xff

/* The small plain textures: the size of a side, the radius of a disc, the dark grey level and
 * the alpha of most pixels. */
#define SMALL_SIZE 0x80
#define DISC_RADIUS 48.0
#define DARK 0x20
#define FAINT 0x20
#define MAX_ANISOTROPY 16.0f

/* The window texture: a square image of cells. */
#define WINDOW_IMAGE_SIZE 0x800
#define CELL_SIZE 0x80

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
#define vectorlength2d CL_FN(0x50bb20, double(__cdecl *)(const float *v))
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
            red = cl_ftoi(v.x * 127.0 + 128.0);
            green = cl_ftoi(v.y * 127.0 + 128.0);
            blue = cl_ftoi(v.z * 127.0 + 128.0);
            g_image.pixels[y * g_image.width + x] = pack_pixel(red, green, blue, OPAQUE);
        }
    }
    texture_from_image(texture, GL_REPEAT, GL_REPEAT, GL_NEAREST, GL_NEAREST);
    setuptexture(texture);
}

typedef struct Pixel {
    int red, green, blue, alpha;
} Pixel;

static void set_pixel(Pixel *p, int red, int green, int blue, int alpha)
{
    p->red = red;
    p->green = green;
    p->blue = blue;
    p->alpha = alpha;
}

static void set_color(Pixel *p, int red, int green, int blue)
{
    p->red = red;
    p->green = green;
    p->blue = blue;
}

/* The distance from the point (x, y) to the center of the disc textures. */
static double from_disc_center(int x, int y)
{
    float v[2];
    v[0] = (float)(SMALL_SIZE / 2 - x);
    v[1] = (float)(SMALL_SIZE / 2 - y);
    return vectorlength2d(v);
}

/* A blue sign with a light grey panel. */
static void sign_pixel(Pixel *p, int x, int y)
{
    set_color(p, DARK, DARK, 0xc0);
    if (x > 0x17 && x < 0x68 && y > 0x37 && y < 0x68)
        set_color(p, 0xc0, 0xc0, 0xc0);
}

/* A traffic lamp: one bright rectangle on a dark ground. Red is at the bottom rows, then
 * yellow, then green. */
static void lamp_pixel(Pixel *p, const int *lamps, int texture, int x, int y)
{
    static const struct {
        int first_row, red, green;
    } LAMPS[3] = {{0x54, 0xff, DARK}, {0x34, 0xff, 0xff}, {0x14, DARK, 0xff}};
    int i;

    set_color(p, DARK, DARK, DARK);
    if (x < 0x28 || x > 0x57)
        return;
    for (i = 0; i < 3; i++) {
        if (texture == lamps[i] && y >= LAMPS[i].first_row && y <= LAMPS[i].first_row + 0x17) {
            set_color(p, LAMPS[i].red, LAMPS[i].green, DARK);
            return;
        }
    }
}

/* One of six colors, with a grey frame around a hole at the top of the image. */
static void panel_pixel(Pixel *p, const int *panels, int texture, int x, int y)
{
    static const uint8_t COLORS[6][3] = {{0x20, 0x20, 0x20}, {0x20, 0x50, 0x20}, {0x20, 0x20, 0x80},
                                         {0xc0, 0xc0, 0xc0}, {0xf7, 0xb5, 0x19}, {0x40, 0x10, 0x10}};
    int i;

    for (i = 0; i < 6; i++) {
        if (texture == panels[i])
            set_color(p, COLORS[i][0], COLORS[i][1], COLORS[i][2]);
    }
    p->alpha = FAINT;
    if (y < 0x28 && x > 0x1f && x < 0x60 && (y < 8 || y > 0x23 || x < 0x28 || x > 0x37))
        set_pixel(p, 0x60, 0x60, 0x60, OPAQUE);
}

/* A grey disc on a dark ground, with a small dark mark near the last rows. */
static void grey_disc_pixel(Pixel *p, int x, int y)
{
    float v[2];

    v[0] = (float)((float)x - 64.0);
    v[1] = (float)((float)y - 64.0);
    if (DISC_RADIUS <= vectorlength2d(v))
        set_pixel(p, DARK, DARK, DARK, 4);
    else
        set_pixel(p, 0x6e, 0x6e, 0x70, FAINT);
    if (y > 0x77 && y < 0x7c && x > 0x37 && x < 0x48)
        set_pixel(p, 0x40, 0x40, 0x40, 4);
}

/* A white disc. The noise moves the edge of the disc by up to 6 pixels. */
static void rough_disc_pixel(Pixel *p, int x, int y)
{
    double distance = from_disc_center(x, y);
    float noise = cerpnoise2d((float)((float)x * 0.125 * 0.5), (float)((float)y * 0.125 * 0.5), 0xffffffff);

    set_color(p, 0xff, 0xff, 0xff);
    p->alpha = noise * 6.0 + DISC_RADIUS <= distance ? 0 : OPAQUE;
}

/* A color on the left half and dark grey on the right half. */
static void paint_pixel(Pixel *p, const int *paints, int texture, int x)
{
    set_color(p, DARK, DARK, DARK);
    if (texture == paints[1])
        set_color(p, 0x80, 0x10, 0x10);
    if (texture == paints[2])
        set_color(p, 0x20, 0x20, 0x60);
    if (texture == paints[3])
        set_color(p, 0x80, 0x80, 0x80);
    if (texture == paints[4])
        set_color(p, 0xe0, 0xe0, 0xe0);
    if (texture == paints[5])
        set_color(p, 0xdb, 0xc0, 0x59);
    if (x > 0x3f)
        set_color(p, 0x40, 0x40, 0x40);
    p->alpha = 0x80;
}

/* Not zero when column x is nearer to the center line of the suit than width. */
static int near_suit_center(int x, int width)
{
    return abs(x - 0x20) < width || abs(x - 0x1f) < width;
}

static void tint_pixel(Pixel *p, const float *tint)
{
    p->blue = cl_ftoi(tint[2] * 255.0);
    p->green = cl_ftoi(tint[1] * 255.0);
    p->red = cl_ftoi(tint[0] * 255.0);
    p->alpha = OPAQUE;
}

/* The left half of a human texture: a grey suit with a white shirt. The first two textures
 * have a badge and a bow tie in the tint color. The third has a straight tie. */
static void suit_pixel(Pixel *p, const int *humans, int texture, const float *tint, int x, int y)
{
    int with_bow_tie = humans[0] <= texture && texture <= humans[1];

    set_pixel(p, 0x60, 0x60, 0x60, FAINT);
    if (with_bow_tie && y > 0x5f && y < 100 && x > 0x29 && x < 0x30)
        tint_pixel(p, tint);
    if (!near_suit_center(x, (y - 0x50) >> 2))
        return;
    set_pixel(p, 0xc0, 0xc0, 0xc0, 0);
    if (texture == humans[2]) {
        if ((y < 0x7e && x > 0x1e && x < 0x21) || (y < 0x7c && x > 0x1d && x < 0x22))
            tint_pixel(p, tint);
    }
    if (with_bow_tie && y < 0x7e) {
        if (near_suit_center(x, (y - 0x70) >> 1) || near_suit_center(x, (0x78 - y) >> 2))
            tint_pixel(p, tint);
    }
}

/* A human texture. The right half has the skin color, then the hair color, then an eye: a
 * white ground, a blue iris and a grey pupil. */
static void human_pixel(Pixel *p, const int *humans, int texture, const float *tint, int x, int y)
{
    float v[2];

    if (x < 0x40) {
        suit_pixel(p, humans, texture, tint, x, y);
    } else if (y < 0x40) {
        set_pixel(p, 0xfd, 0xb4, 0x9f, FAINT);
    } else if (y < 0x60) {
        set_pixel(p, 0xa0, 0x80, 0x20, FAINT);
    } else {
        set_pixel(p, 0xfd, 0xfd, 0xfd, FAINT);
        v[0] = (float)(79.5 - (float)x);
        v[1] = (float)(111.5 - (float)y);
        if (vectorlength2d(v) < 6.0)
            set_color(p, 0x38, 0x40, 0xfd);
        if (vectorlength2d(v) < 3.5)
            set_color(p, 0x40, 0x40, 0x40);
    }
}

/* Make one of the small plain textures of 128 x 128. The texture number selects the picture.
 * A number that is in no list gives a faint magenta image. */
void __cdecl generatetexture(int texture, int mipmap, int wrap_s, int wrap_t, int mag_filter, int min_filter)
{
    const TextureList *list = &g_texturelist;
    const float *tint = sr_data_teamcolor;
    int x, y, i;

    for (i = 0; i < 3; i++) {
        if (texture == list->human[i]) {
            tint = &sr_data_teamcolor[i * 3];
            break;
        }
    }
    g_image.width = SMALL_SIZE;
    g_image.height = SMALL_SIZE;
    for (y = 0; y < g_image.height; y++) {
        for (x = 0; x < g_image.width; x++) {
            Pixel p;
            set_pixel(&p, 0xff, 0, 0xff, FAINT);
            if (texture == list->street[4])
                sign_pixel(&p, x, y);
            if (list->street[5] <= texture && texture <= list->street[7])
                lamp_pixel(&p, &list->street[5], texture, x, y);
            if (list->panel[0] <= texture && texture <= list->panel[5])
                panel_pixel(&p, list->panel, texture, x, y);
            if (texture == list->plain_green)
                set_pixel(&p, 0x10, 0x40, 0x10, FAINT);
            if (texture == list->half_grey) {
                if (x < 0x40)
                    set_pixel(&p, 0x80, 0x78, 0x70, 1);
                else
                    set_pixel(&p, DARK, DARK, DARK, FAINT);
            }
            if (texture == list->half_brown) {
                if (x < 0x40)
                    set_pixel(&p, 0x10, 0x10, 0x10, OPAQUE);
                else
                    set_pixel(&p, 0x40, 0x18, 0xc, FAINT);
            }
            if (texture == list->plain_orange)
                set_pixel(&p, 0xa0, 0x60, 0x20, OPAQUE);
            if (texture == list->grey_disc)
                grey_disc_pixel(&p, x, y);
            if (texture == list->disc)
                set_pixel(&p, 0xff, 0xff, 0xff, DISC_RADIUS <= from_disc_center(x, y) ? 0 : OPAQUE);
            if (texture == list->rough_disc)
                rough_disc_pixel(&p, x, y);
            if (list->paint[0] <= texture && texture <= list->paint[7])
                paint_pixel(&p, list->paint, texture, x);
            if (list->human[0] <= texture && texture <= list->human[3])
                human_pixel(&p, list->human, texture, tint, x, y);
            g_image.pixels[y * g_image.width + x] = pack_pixel(p.red, p.green, p.blue, p.alpha);
        }
    }
    texture_from_image(texture, wrap_s, wrap_t, mag_filter, min_filter);
    if (mipmap != 0)
        generatemipmap(texture);
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
                red = green = blue = cl_ftoi(wave + 112.0);
            } else {
                red = cl_ftoi(wave * 0.25 + 64.0);
                green = cl_ftoi(wave * 0.125 + 56.0);
                blue = cl_ftoi(wave * 0.25 + 48.0);
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

/* The texel of a tile of 64 x 64 that gives the pixel p (0 to 127) of a window cell. Pixels 24
 * to 31 use texel 0 and pixels 96 to 103 use texel 63. The other pixels wrap around the tile. */
static int window_texel(int p)
{
    if (p < 0x18)
        return (p + 0x28) & 0x3f;
    if (p < 0x20)
        return 0;
    if (p < 0x60)
        return (p + 0x20) & 0x3f;
    if (p < 0x68)
        return 0x3f;
    return (p + 0x18) & 0x3f;
}

/* Make the window texture of a material: 2048 x 2048 pixels, 16 x 4 cells of 128 x 128. A cell
 * is the second level of the texture of a material, with the lit rectangle of a window drawn
 * over it. Clear windows get grey glass, the others get alpha 255. A source texture with no
 * second level leaves the color of the pixel before. */
void __cdecl generatematerialwindowtexture(int material, int mipmap, int wrap_s, int wrap_t, int mag_filter,
                                           int min_filter)
{
    int texture = g_texturelist.material[material];
    uint32_t red = 0, green = 0, blue = 0, alpha = 0;
    int cell, x, y;

    g_image.width = WINDOW_IMAGE_SIZE;
    g_image.height = WINDOW_IMAGE_SIZE;
    memset(g_image.pixels, 0, (size_t)WINDOW_IMAGE_SIZE * WINDOW_IMAGE_SIZE * 4);
    for (cell = 0; cell < g_windowatlas.num_cells; cell++) {
        const Texture *source = &g_textures[g_texturelist.material[g_windowatlas.cells[cell].material]];
        const WindowRect *window = &g_windowrect[g_windowatlas.cells[cell].window];
        const uint32_t *pixels = source->levels[1];
        int left = cl_ftoi(window->left * 64.0);
        int top = cl_ftoi(window->top * 64.0);
        int right = left + cl_ftoi(window->width * 64.0);
        int bottom = top + cl_ftoi(window->height * 64.0);

        for (y = 0; y < CELL_SIZE; y++) {
            int v = window_texel(y);
            for (x = 0; x < CELL_SIZE; x++) {
                int u = window_texel(x);
                if (pixels != 0) {
                    uint32_t word = pixels[(source->width >> 1) * v + u];
                    red = word & 0xff;
                    green = (word >> 8) & 0xff;
                    blue = (word >> 16) & 0xff;
                    alpha = word >> 24;
                }
                if (v >= top && v < bottom && 0x3f - u >= left && 0x3f - u < right) {
                    if (window->clear == 0) {
                        alpha = OPAQUE;
                    } else {
                        red = green = blue = 0x60;
                        alpha = 0x20;
                    }
                }
                g_image.pixels[(y + (cell >> 4) * CELL_SIZE) * WINDOW_IMAGE_SIZE + x + (cell & 0xf) * CELL_SIZE] =
                    green * 0x100u + alpha * 0x1000000u + blue * 0x10000u + red;
            }
        }
    }
    texture_from_image(texture, wrap_s, wrap_t, mag_filter, min_filter);
    if (mipmap != 0)
        generatemipmap(texture);
    setuptexture(texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, MAX_ANISOTROPY);
}

/* The pixels of the suit half of a human texture (the left half): a grey suit with a light
 * shirt. The grey depends on the team. Teams 0 and 1 have a badge, a bow tie and a tie in the
 * tint color. Team 2 has a straight tie, teams 3 and 4 a wide tie. Team 7 has a light lower half. */
static void player_suit_pixel(Pixel *p, const Human *human, const float *tint, int x, int y)
{
    static const uint8_t SUIT_GREY[4] = {0x60, 0x40, 0x20, 0x80};
    int team = human->team;
    int grey = DARK;

    if (team >= 0 && team < 4)
        grey = SUIT_GREY[team];
    if (team < 5 && human->look_d == 1)
        grey = 0xc0;
    set_pixel(p, grey, grey, grey, FAINT);
    if (team >= 0 && team <= 1 && y > 0x5f && y < 100 && x > 0x29 && x < 0x30)
        tint_pixel(p, tint);
    if (near_suit_center(x, (y - 0x50) >> 2)) {
        grey = team == 4 ? 0x80 : 0xd0;
        set_pixel(p, grey, grey, grey, 0);
        if (team == 2) {
            if ((y < 0x7e && x == 0x1f) || (y < 0x7c && x > 0x1d && x < 0x22))
                tint_pixel(p, tint);
        }
        if (team >= 0 && team <= 1) {
            if (y < 0x7e && (near_suit_center(x, (y - 0x70) >> 1) || near_suit_center(x, (0x78 - y) >> 2)))
                tint_pixel(p, tint);
        }
        if (team == 3 || team == 4) {
            if (y < 0x7e && (near_suit_center(x, (y - 0x70) >> 1) || near_suit_center(x, ((0x78 - y) >> 3) + 2)))
                tint_pixel(p, tint);
        }
    }
    if (y > 0x3f && team == 7)
        set_pixel(p, 0xd0, 0xd0, 0xd0, 0);
}

/* The pixels of the right half of a human texture: skin, then hair, then an eye. */
static void player_face_pixel(Pixel *p, const Human *human, const uint8_t *skin, const uint8_t *hair, int x, int y)
{
    static const uint8_t IRIS[3][3] = {{0x5e, 0x9b, 0x94}, {0x4a, 0x69, 0x1b}, {0x68, 0x47, 0x34}};
    float v[2];
    double distance;

    if (y < 0x40) {
        set_pixel(p, skin[0], skin[1], skin[2], FAINT);
    } else if (y < 0x60) {
        set_pixel(p, hair[0], hair[1], hair[2], FAINT);
    } else {
        set_pixel(p, 0xfd, 0xfd, 0xfd, FAINT);
        v[0] = (float)(79.5 - (float)x);
        v[1] = (float)(111.5 - (float)y);
        distance = vectorlength2d(v);
        if (distance < 6.0 && human->look_a >= 0 && human->look_a < 3)
            set_color(p, IRIS[human->look_a][0], IRIS[human->look_a][1], IRIS[human->look_a][2]);
        if (distance < 3.0)
            set_color(p, 0x40, 0x40, 0x40);
    }
}

/* Make the texture of a human from its team, skin, hair, eye and suit values. The texture is
 * 128 x 128 and the game keeps the values of the last picture: the texture stays as it is
 * when none of them changed. The pixel memory of the texture is made once and then reused.
 * A skin or hair value outside its table gives black. */
void __cdecl generatehumantexture(int human)
{
    static const uint8_t SKIN[8][3] = {{0xf8, 0xd5, 0xc2}, {0xef, 0xbb, 0xa6}, {0xe6, 0xaa, 0x86}, {0xd2, 0x94, 0x6b},
                                       {0xc5, 0x8f, 99},   {0xae, 0x71, 0x42}, {0x84, 0x57, 0x36}, {0x71, 0x50, 0x31}};
    static const uint8_t HAIR[4][3] = {{0xa0, 0x80, 0x20}, {0x50, 0x40, 0x10}, {0x20, 0x18, 0x10}, {0xa0, 0x40, 0x20}};
    static const float WHITE[3] = {1.0f, 1.0f, 1.0f};
    static const uint8_t NONE[3] = {0, 0, 0};
    const Human *h = &sr_data_human[human];
    HumanLook *last = &g_humanlook[human];
    Texture *t = &g_textures[g_texturelist.human_textures[human]];
    const float *tint = h->team == -1 ? WHITE : &sr_data_teamcolor[h->team * 3];
    const uint8_t *skin = h->look_b >= 0 && h->look_b < 8 ? SKIN[h->look_b] : NONE;
    const uint8_t *hair = h->look_c >= 0 && h->look_c < 4 ? HAIR[h->look_c] : NONE;
    size_t bytes;
    int x, y;

    if (last->team == h->team && last->look_b == h->look_b && last->look_c == h->look_c && last->look_a == h->look_a &&
        last->look_d == h->look_d)
        return;
    last->team = h->team;
    last->look_b = h->look_b;
    last->look_c = h->look_c;
    last->look_a = h->look_a;
    last->look_d = h->look_d;
    g_image.width = SMALL_SIZE;
    g_image.height = SMALL_SIZE;
    for (y = 0; y < g_image.height; y++) {
        for (x = 0; x < g_image.width; x++) {
            Pixel p;
            if (x < SMALL_SIZE / 2)
                player_suit_pixel(&p, h, tint, x, y);
            else
                player_face_pixel(&p, h, skin, hair, x, y);
            g_image.pixels[y * g_image.width + x] = pack_pixel(p.red, p.green, p.blue, p.alpha);
        }
    }
    t->width = g_image.width;
    t->height = g_image.height;
    t->num_levels = 1;
    t->format = GL_RGBA;
    t->internal_format = GL_RGBA;
    t->spare = 0;
    t->wrap_s = GL_CLAMP_TO_EDGE;
    t->wrap_t = GL_CLAMP_TO_EDGE;
    t->mag_filter = GL_NEAREST;
    t->min_filter = GL_NEAREST;
    bytes = (size_t)(t->width * t->height * 4);
    if (t->levels[0] == 0)
        t->levels[0] = os_malloc(bytes);
    memcpy(t->levels[0], g_image.pixels, bytes);
    setuptexture(g_texturelist.human_textures[human]);
}

/* Four layers of smooth noise at the point (x, y), each layer finer than the one before. Each
 * layer adds its noise plus 1, twice. The result is a float from 0 to 8. */
static float layered_noise(int x, int y)
{
    float broad = cerpnoise2d((float)(x * 0.25), (float)(y * 0.125), 0xf);
    float middle = cerpnoise2d((float)(x * 0.5), (float)(y * 0.25), 0x1f);
    float fine = cerpnoise2d((float)(x * 1.0), (float)(y * 0.5), 0x3f);
    float finest = cerpnoise2d((float)(x + x + 1235.0), (float)(y * 1.0 + 123.0), 0x7f);
    float sum = (float)((broad + 1.0) + (broad + 1.0));

    sum = (float)((middle + 1.0) + (middle + 1.0) + sum);
    sum = (float)((fine + 1.0) + (fine + 1.0) + sum);
    return (float)((finest + 1.0) + (finest + 1.0) + sum);
}

static void grey(Pixel *p, int level)
{
    set_color(p, level, level, level);
}

/* Make the color darker for each of the three kinds of joint of a floor that the point is on.
 * Each kind halves the color, so a point on two kinds gets a quarter. The kinds are the first
 * 2 rows of every 16, the first 2 columns of the top half of every 32 rows, and the 2 middle
 * columns of the bottom half. */
static void halve_on_joints(Pixel *p, int x, int y)
{
    unsigned row = (unsigned)y, column = (unsigned)x;
    int halvings = ((row & 0xf) < 2) + ((row & 0x1f) < 0x10 && (column & 0x3f) < 2) +
                   ((row & 0x1f) > 0xf && (column & 0x3f) > 0x1f && (column & 0x3f) < 0x22);

    p->red >>= halvings;
    p->green >>= halvings;
    p->blue >>= halvings;
}

/* The same for the mortar of a brick wall: every eighth row, one column of 32 in the top
 * half of every 16 rows, and one column in the middle of the bottom half. */
static void halve_on_mortar(Pixel *p, int x, int y)
{
    unsigned row = (unsigned)y, column = (unsigned)x;
    int halvings = ((row & 7) == 0) + ((row & 0xf) < 8 && (column & 0x1f) == 0) +
                   ((row & 0xf) > 7 && (column & 0x1f) == 0x10);

    p->red >>= halvings;
    p->green >>= halvings;
    p->blue >>= halvings;
}

/* A color from the noise of the point (x, y): each channel adds its base to 8 noise units,
 * alpha is 32 plus 4 noise units. */
static void noise_color(Pixel *p, int x, int y, int red_base, int green_base, int blue_base)
{
    float noise = noise2d(x, y);

    p->red = cl_ftoi(noise * 8.0 + red_base);
    p->green = cl_ftoi(noise * 8.0 + green_base);
    p->blue = cl_ftoi(noise * 8.0 + blue_base);
    p->alpha = cl_ftoi(noise * 4.0 + 32.0);
}

/* Not zero when the number is in the closed range. */
static int between(int value, int low, int high)
{
    return low <= value && value <= high;
}

/* The pixel of a plain wall with a ground made of the layered noise. */
static void grouped_pixel(Pixel *p, int group, int x, int y)
{
    int level = cl_ftoi(layered_noise(x, y) + 192.0);

    set_pixel(p, level, level, level, FAINT);
    if (group == 2)
        halve_on_joints(p, x, y);
    if (group == 1)
        halve_on_mortar(p, x, y);
}

/* A texture with a street marking: noise, and a grey line along one edge of the road. */
static void street_pixel(Pixel *p, const MaterialList *list, int material, int x, int y)
{
    noise_color(p, x, y, 48, 48, 48);
    if (material == list->street_x_right && between(y, 5, 0xa) && between(x, 0x20, 0x5f))
        grey(p, 0xa0);
    if (material == list->street_x_left && between(y, 0x75, 0x7a) && between(x, 0x20, 0x5f))
        grey(p, 0xa0);
    if (material == list->street_z_right && between(x, 5, 0xa) && between(y, 0x20, 0x5f))
        grey(p, 0xa0);
    if (material == list->street_z_left && between(x, 0x75, 0x7a) && between(y, 0x20, 0x5f))
        grey(p, 0xa0);
}

/* Materials 3 to 9: noise, with a yellow line near one edge (4, 5, 7, 8) or two grey bands (6, 9). */
static void lined_pixel(Pixel *p, int material, int x, int y)
{
    noise_color(p, x, y, 48, 48, 48);
    if (material == 4 && between(y, 5, 0xa))
        set_color(p, 0xa0, 0xa0, 0x10);
    if (material == 5 && between(y, 0x75, 0x7a))
        set_color(p, 0xa0, 0xa0, 0x10);
    if (material == 6 && (between(y, 0x10, 0x1f) || between(y, 0x60, 0x6f)))
        grey(p, 0xa0);
    if (material == 7 && between(x, 5, 0xa))
        set_color(p, 0xa0, 0xa0, 0x10);
    if (material == 8 && between(x, 0x75, 0x7a))
        set_color(p, 0xa0, 0xa0, 0x10);
    if (material == 9 && (between(x, 0x10, 0x1f) || between(x, 0x60, 0x6f)))
        grey(p, 0xa0);
}

/* Materials 12 and 17: a brown wall with joints. Material 17 has a dark window with a frame. */
static void window_wall_pixel(Pixel *p, int material, int x, int y)
{
    noise_color(p, x, y, 160, 140, 100);
    halve_on_joints(p, x, y);
    if (material != 0x11)
        return;
    if (between(y, 0x14, 0x6b) && between(x, 0xc, 0x73))
        set_color(p, 0x40, 0x20, 0x20);
    p->alpha = OPAQUE;
    if (between(y, 0x18, 0x67) && between(x, 0x10, 0x6f)) {
        set_color(p, 0x10, 0x10, 0x20);
        p->alpha = 0x40;
        if (between(y & 0x1f, 0xe, 0x11)) {
            set_color(p, 0x40, 0x20, 0x20);
            p->alpha = OPAQUE;
        }
        if (between(x & 0x1f, 0xe, 0x11)) {
            set_color(p, 0x40, 0x20, 0x20);
            p->alpha = OPAQUE;
        }
    }
}

/* The pixel of a material texture. The checks run in this order, and a later check replaces
 * what an earlier one made. The first color is magenta. */
static void material_pixel(Pixel *p, const MaterialList *list, int material, int group, int x, int y)
{
    set_pixel(p, 0xff, 0, 0xff, FAINT);
    if (group != -1)
        grouped_pixel(p, group, x, y);
    if (material == list->tile_material) {
        float shade = (float)((cerpnoise2d((float)(x * 0.25), (float)(y * 0.25), 0x1f) + 1.0) * 0.125 + 0.875);
        set_pixel(p, cl_ftoi(shade * 12.0 * 0.5), cl_ftoi(shade * 72.0 * 0.5), cl_ftoi(shade * 16.0 * 0.5), 0);
    }
    if (material == 0xd)
        set_pixel(p, 0x10, 0x10, 0x10, FAINT);
    if (material == 0x12)
        set_pixel(p, 8, 0x10, 0x80, FAINT);
    if (material == 0x13) {
        set_pixel(p, 0xc0, 0xc0, 0x10, FAINT);
        if (((unsigned)(x + y) & 0x3f) < 0x1f)
            set_pixel(p, 0x20, 0x20, 0x20, FAINT);
    }
    if (material == list->misc_2a0)
        set_pixel(p, 0xe1, 0xe1, 0xd5, FAINT);
    if (material == list->misc_2a4)
        set_pixel(p, 0x41, 0x41, 0x41, FAINT);
    if (material == list->misc_2b0) {
        float broad = cerpnoise2d((float)(x * 0.25), (float)(y * 0.25), 0xf);
        float middle = cerpnoise2d((float)(x * 0.5), (float)(y * 0.5), 0x1f);
        float fine = cerpnoise2d((float)(x * 1.0), (float)(y * 1.0), 0x3f);
        float finest = cerpnoise2d((float)(x + x + 1235.0), (float)(y + y + 123.0), 0x7f);
        float sum = (float)((broad + 1.0) + (broad + 1.0));
        int level;

        sum = (float)((middle + 1.0) + (middle + 1.0) + sum);
        sum = (float)((fine + 1.0) * 8.0 + sum);
        sum = (float)((finest + 1.0) + (finest + 1.0) + sum);
        level = cl_ftoi(sum + 72.0);
        set_pixel(p, level, level, level, 0x10);
    }
    if (material == list->ceiling) {
        grey(p, 0x80);
        if ((y & 0x1f) == 0 || (x & 0x1f) == 0)
            grey(p, 0x60);
        if (y < 0x20 && x < 0x20)
            grey(p, 0xff);
        p->alpha = FAINT;
    }
    if (material == list->misc_2c4) {
        grey(p, 0xff);
        if (y < 8 || x < 8 || y > 0x77 || x > 0x77)
            grey(p, 0xe0);
        p->alpha = FAINT;
    }
    if (material == list->flagged_material) {
        float noise = cerpnoise2d((float)(x * 0.25), (float)(y * 0.25), 0x7f);
        set_pixel(p, 0x30, 0x30, 0x48, 0x60);
        if ((float)(noise - 0.75) > 0.0)
            p->alpha = cl_ftoi((noise - 0.75) * 64.0 + 96.0);
    }
    if (material == 0x14) {
        float sum = layered_noise(x, y);
        set_pixel(p, cl_ftoi(sum + 160.0), cl_ftoi(sum + 160.0), cl_ftoi(sum * 0.875 + 160.0), OPAQUE);
        if (between(y, 4, 0x6f) && between(x, 0x10, 0x6f))
            set_pixel(p, 0x10, 0x10, 0x20, 0x40);
    }
    if (material == 0 || material == 1 || material == 2) {
        float sum = layered_noise(x, y);
        set_color(p, cl_ftoi(sum + 192.0), cl_ftoi(sum + 192.0), cl_ftoi(sum * 0.875 + 192.0));
        if (material == 1) {
            p->alpha = OPAQUE;
            if (between(y, 0x20, 0x5f) && between(x, 4, 0x7b)) {
                float distance = (float)((float)(abs(x - 0x40) + abs(y - 0x40)) / 64.0);
                set_pixel(p, cl_ftoi(distance * 32.0 + 16.0), cl_ftoi(distance * 32.0 + 16.0),
                          cl_ftoi(distance * 64.0 + 32.0), 0x80);
            }
        }
        if (material == 2)
            set_pixel(p, 0x10, 0x10, 0x20, OPAQUE);
    }
    if (list->street_x_left <= material && material <= list->street_z_right)
        street_pixel(p, list, material, x, y);
    if (between(material, 3, 9))
        lined_pixel(p, material, x, y);
    if (material == 0xc || material == 0x11)
        window_wall_pixel(p, material, x, y);
    if (material == 0xf) {
        noise_color(p, x, y, 80, 71, 64);
        p->alpha = cl_ftoi(noise2d(x + 0x80, y + 0x80) * 16.0 + 32.0);
    }
    if (material == 0x10) {
        noise_color(p, x, y, 32, 32, 32);
        p->alpha = cl_ftoi(noise2d(x + 0x80, y + 0x80) * 16.0 + 32.0);
    }
    if (material == 10)
        set_pixel(p, 0xc0, 0x60, 0x30, FAINT);
    if (material == 0xb || material == list->marble) {
        float grain = carpet_grain(x, y);
        float wave = (float)(x87_sin((grain + grain) * PI) * 32.0);
        set_pixel(p, cl_ftoi(wave + 176.0), cl_ftoi(wave + 184.0), cl_ftoi(wave + 192.0), FAINT);
    }
}

/* Make the texture of a material: 128 x 128 pixels. The sign and the carpet have their own
 * generators. Every other material is drawn from its number, or magenta when no picture is
 * defined for it. */
void __cdecl generatematerialtexture(int material, int mipmap, int wrap_s, int wrap_t, int mag_filter, int min_filter)
{
    const MaterialList *list = &g_materiallist;
    int group = -1;
    int texture, x, y, i;

    if (material == list->sign) {
        generatetexture_billboard(material);
        return;
    }
    if (material == list->carpet) {
        generatetexture_carpet(material);
        return;
    }
    texture = g_texturelist.material[material];
    for (i = 0; i < 4; i++) {
        if (list->presets[i * 10] <= material && material <= list->presets[i * 10 + 2])
            group = i;
    }
    g_image.width = SMALL_SIZE;
    g_image.height = SMALL_SIZE;
    for (y = 0; y < g_image.height; y++) {
        for (x = 0; x < g_image.width; x++) {
            Pixel p;
            material_pixel(&p, list, material, group, x, y);
            g_image.pixels[y * g_image.width + x] = pack_pixel(p.red, p.green, p.blue, p.alpha);
        }
    }
    texture_from_image(texture, wrap_s, wrap_t, mag_filter, min_filter);
    if (mipmap != 0)
        generatemipmap(texture);
    setuptexture(texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, MAX_ANISOTROPY);
}
