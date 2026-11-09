/* Bitmap fonts for text and the mouse cursor.
 * A font stores 256 widths and four texture coordinates per glyph.
 * Font images arrange glyphs in a sixteen by sixteen grid.
 * Built-in fonts use eight by eight monochrome glyphs.
 */
#include "image/fonts.h"
#include <string.h>
#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"

enum { FONT_GLYPHS = 256, FONT_COLUMNS = 16, FONT_CELL = 8, FONT_ATLAS = 128,
       FONT_LARGE_ATLAS = 1024, FONT_TEXTURE_BASE = 1000, FONT_SMALL = 2,
       FONT_CURSOR = 1, FONT_CURSOR_TEXTURE = 1010,
       IMAGE_RGBA = 0x1908, IMAGE_CLAMP = 0x812f, IMAGE_LINEAR = 0x2601,
       IMAGE_NEAREST = 0x2600, IMAGE_MIPMAP_LINEAR = 0x2703 };

/* Game functions of other files. */
static void font_upload(int texture)
{
    CL_FN(0x50ac40, void(__cdecl *)(int))(texture);
}

/* Set texture storage and sampling. Return the pixel buffer. */
static uint32_t *font_texture(int index, int width, int height, int mag, int min)
{
    ImageTexture *texture = &cl_data_image_textures[index];
    texture->width = width;
    texture->height = height;
    texture->levels = 1;
    texture->format = IMAGE_RGBA;
    texture->internal_format = IMAGE_RGBA;
    texture->flags = 0;
    texture->wrap_s = IMAGE_CLAMP;
    texture->wrap_t = IMAGE_CLAMP;
    texture->mag_filter = mag;
    texture->min_filter = min;
    os_free(texture->pixels[0]);
    texture->pixels[0] = os_malloc(width * height * sizeof(uint32_t));
    return texture->pixels[0];
}

/* Map extended glyphs to their bitmap and inverse flag. */
static unsigned font_glyph(unsigned glyph, int *inverse)
{
    *inverse = 0;
    if (glyph >= 128) {
        if (glyph < 160)
            glyph -= 128;
        else {
            glyph -= glyph < 192 ? 160 : 128;
            *inverse = 1;
        }
    }
    switch (glyph) {
    case 28: return 124;
    case 29: return 125;
    case 30: return 96;
    case 31: return 123;
    }
    return glyph;
}

/* Fill a square atlas from the monochrome glyph table. */
static void font_bitmap(uint32_t *pixels, int dimension, int scale)
{
    int x, y, inverse;
    for (y = 0; y < dimension; y++)
        for (x = 0; x < dimension; x++) {
            unsigned glyph = (y / (FONT_CELL * scale)) * FONT_COLUMNS + x / (FONT_CELL * scale);
            unsigned bit = (x / scale) & 7, row = (y / scale) & 7;
            glyph = font_glyph(glyph, &inverse);
            pixels[y * dimension + x] =
                (((cl_data_font_bits[glyph * FONT_CELL + row] >> bit) & 1) != inverse) ? UINT32_MAX : 0;
        }
}

/* Store a rectangle in clockwise order. */
static void font_rectangle(float *coordinates, float left, float top, float right, float bottom)
{
    coordinates[0] = left;
    coordinates[1] = top;
    coordinates[2] = right;
    coordinates[3] = top;
    coordinates[4] = right;
    coordinates[5] = bottom;
    coordinates[6] = left;
    coordinates[7] = bottom;
}

/* Set fixed widths and coordinates. Glyph 127 uses one point in the atlas. */
static void font_fixed_metrics(Font *font, double inset)
{
    int glyph, coordinate;
    for (glyph = 0; glyph < FONT_GLYPHS; glyph++) {
        int x = (glyph & 15) * FONT_CELL, y = (glyph >> 4) * FONT_CELL;
        float *coordinates = font->coordinates + glyph * 8;
        font->width[glyph] = 1.0f;
        font_rectangle(coordinates, (float)(x + inset), (float)(y + inset),
                       (float)(x + FONT_CELL - inset), (float)(y + FONT_CELL - inset));
        for (coordinate = 0; coordinate < 8; coordinate++)
            coordinates[coordinate] = (float)(coordinates[coordinate] / (double)FONT_ATLAS);
        if (glyph == 127) {
            font_rectangle(coordinates, 124.0f, 60.0f, 124.0f, 60.0f);
            for (coordinate = 0; coordinate < 8; coordinate++)
                coordinates[coordinate] = (float)(coordinates[coordinate] / (double)FONT_ATLAS);
        }
    }
}

/* Build the large font, small font and cursor textures. */
void createfont(void)
{
    uint32_t outline = cl_data_font_alpha_low ? 255 : 0xff000000u;
    int texture = cl_data_fontnum + FONT_TEXTURE_BASE, x, y;
    uint32_t *pixels;
    cl_data_fonts[cl_data_fontnum].texture = texture;
    pixels = font_texture(texture, FONT_LARGE_ATLAS, FONT_LARGE_ATLAS, IMAGE_LINEAR, IMAGE_MIPMAP_LINEAR);
    font_bitmap(pixels, FONT_LARGE_ATLAS, 8);
    CL_FN(0x50a9a0, void(__cdecl *)(int))(texture);
    font_upload(texture);
    font_fixed_metrics(&cl_data_fonts[cl_data_fontnum], 0.25);
    cl_data_fontnum = FONT_SMALL;
    texture = FONT_TEXTURE_BASE + FONT_SMALL;
    cl_data_fonts[FONT_SMALL].texture = texture;
    pixels = font_texture(texture, FONT_ATLAS, FONT_ATLAS, IMAGE_NEAREST, IMAGE_NEAREST);
    font_bitmap(pixels, FONT_ATLAS, 1);
    font_upload(texture);
    font_fixed_metrics(&cl_data_fonts[FONT_SMALL], 0.0);
    cl_data_fontnum = FONT_CURSOR;
    pixels = font_texture(FONT_CURSOR_TEXTURE, FONT_ATLAS, FONT_ATLAS, IMAGE_LINEAR, IMAGE_LINEAR);
    for (y = 0; y < FONT_ATLAS; y++)
        for (x = 0; x < FONT_ATLAS; x++) {
            uint32_t value = 0;
            if ((y >> 3) > 7 && (x >> 3) > 7) {
                unsigned bit = (x >> 3) & 7, row = (y >> 3) & 7;
                if (cl_data_cursor_bits[row + 1] & (1 << bit))
                    value = UINT32_MAX;
                else if (row && (cl_data_cursor_bits[row] & (1 << bit)))
                    value = outline;
            }
            pixels[y * FONT_ATLAS + x] = value;
        }
    font_upload(FONT_CURSOR_TEXTURE);
}

/* Find the last occupied column. Stop at the first empty pair of columns. */
static float font_width(const ImageTexture *texture, int cell, int glyph, int start, uint32_t alpha)
{
    int x = (glyph & 15) * cell + start;
    int y = (15 - (glyph >> 4)) * cell;
    float width = 2.0f;
    while (width < (double)(float)cell - 1.0) {
        int row;
        for (row = 0; row < cell; row++) {
            const uint32_t *pixel = texture->pixels[0] + (y + row) * texture->width + x;
            if ((pixel[0] & alpha) || (pixel[1] & alpha))
                break;
        }
        if (row == cell)
            break;
        width = (float)(width + 1.0);
        x++;
    }
    return width;
}

/* Load a font atlas. Failed loads and dimensions that are not powers of two leave metrics unchanged. */
void loadfont(int texture_index, char *path, float size)
{
    Font *font = &cl_data_fonts[cl_data_fontnum];
    ImageTexture *texture = &cl_data_image_textures[texture_index];
    int cell, glyph, coordinate, x, y;
    uint32_t *pixels, alpha;
    font->texture = texture_index;
    if (!CL_FN(0x509f60, int(__cdecl *)(char *))(path) ||
        ((cl_data_image_width - 1) & cl_data_image_width) ||
        ((cl_data_image_height - 1) & cl_data_image_height))
        return;
    pixels = font_texture(texture_index, cl_data_image_width, cl_data_image_height, IMAGE_LINEAR, IMAGE_LINEAR);
    memcpy(pixels, cl_data_image_pixels, texture->width * texture->height * sizeof *pixels);
    if (!texture->width)
        return;
    cell = texture->width >> 4;
    for (y = texture->height - cell; y < texture->height; y++)
        for (x = 0; x < cell; x++)
            pixels[y * texture->width + x] = UINT32_MAX;
    alpha = cl_data_font_alpha_low ? 255 : 0xff000000u;
    for (glyph = 0; glyph < FONT_GLYPHS; glyph++) {
        float *coordinates = font->coordinates + glyph * 8;
        font->width[glyph] = font_width(texture, cell, glyph, size > 32.0 ? 4 : 2, alpha);
        if (glyph == 32)
            font->width[glyph] = (float)((float)cell * 0.375);
        if (glyph >= 48 && glyph < 58)
            font->width[glyph] = (float)((float)cell * 0.3499999940395355);
        font->width[glyph] = (float)(font->width[glyph] / (float)cell);
        x = (glyph & 15) * cell + 1;
        y = (15 - (glyph >> 4)) * cell + 1;
        if (!glyph) {
            font_rectangle(coordinates, (float)((float)cell * 0.5),
                (float)((float)texture->height - (float)cell * 0.5),
                (float)((float)cell * 0.5), (float)((float)texture->height - (float)cell * 0.5));
        } else {
            font_rectangle(coordinates, (float)((float)x + 0.03125),
                (float)(((float)y + (float)cell) - 0.03125),
                (float)(((float)cell * font->width[glyph] + (float)x) - 0.03125),
                (float)(((float)cell - size) + (float)y + 0.03125));
        }
        for (coordinate = 0; coordinate < 8; coordinate++)
            coordinates[coordinate] = (float)(coordinates[coordinate] /
                (float)(coordinate & 1 ? texture->height : texture->width));
        font->width[glyph] = (float)(((double)(float)cell / size) * font->width[glyph]);
    }
    font_upload(texture_index);
}

/* Set the resolution of the selected font. */
void setfontresolution(uint32_t width, uint32_t height)
{
    cl_data_fonts[cl_data_fontnum].resolution_x = width;
    cl_data_fonts[cl_data_fontnum].resolution_y = height;
}
