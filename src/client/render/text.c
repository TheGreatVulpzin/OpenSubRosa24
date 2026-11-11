/* Text on the 2D layer.
 *
 * A text is a list of letters from the font atlas. Each letter is one quad. The text can have
 * a shadow, run top to bottom, and ask for numbers and strings with codes that start with a
 * slash. The extra arguments of drawtext fill the codes, in order:
 *   //  a slash          /c  one letter (nothing for 0)    /i  a signed number
 *   /u  an unsigned number   /x  hexadecimal capitals      /s  a string
 *   /f  a double with three decimals   /I  a number in groups of three with commas
 *   /b  the 32 bits of a number, in groups of eight with commas
 */
#include "render/text.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "client.h"
#include "sr_runtime.h"
#include "textures/opengl.h"

#define g_textures cl_data_texture
#define g_fonts cl_data_fonts
#define g_fontnum cl_data_fontnum
#define g_text cl_data_draw_text
#define g_number cl_data_draw_number

enum {
    TEXT_LIMIT = 1000,
    GROUP_SIZE = 1000,
    BITS = 32,
    FIXED_GLYPH = 1024,
    FLAG_CENTER_X = 0x01,
    FLAG_RIGHT = 0x02,
    FLAG_CENTER_Y = 0x04,
    FLAG_BOTTOM = 0x08,
    FLAG_FIXED_GLYPH = 0x10,
    FLAG_SHADOW = 0x20,
    FLAG_PLAIN = 0x40,
    FLAG_FLIP = 0x80,
    FLAG_VERTICAL = 0x100
};

GL_IMPORT(glVertex2fv, (const float *vertex), (vertex))
GL_IMPORT(glTexCoord2fv, (const float *coord), (coord))

/* Add the text of the number buffer to the text, without the end mark. */
static int append_number(int length)
{
    size_t count = strlen(g_number);

    memcpy(g_text + length, g_number, count);
    return length + (int)strlen(g_number);
}

/* Add a number in groups of three digits, with a comma after each group but the last. */
static int append_grouped(int length, int value)
{
    if (999999 < value) {
        sprintf(g_number, "%d", value / 1000000);
        length = append_number(length);
        g_text[length++] = ',';
    }
    if (999 < value) {
        if (value < 1000000)
            sprintf(g_number, "%d", (value / 1000) % GROUP_SIZE);
        else
            sprintf(g_number, "%.3d", (value / 1000) % GROUP_SIZE);
        length = append_number(length);
        g_text[length++] = ',';
    }
    if (value < GROUP_SIZE)
        sprintf(g_number, "%d", value);
    else
        sprintf(g_number, "%.3d", value % GROUP_SIZE);
    return append_number(length);
}

/* Add the bits of a number, the high bit first, with a comma after each byte but the last. */
static int append_bits(int length, unsigned value)
{
    int bit;

    for (bit = 0; bit < BITS; ++bit) {
        g_text[length++] = (value & 0x80000000u) == 0 ? '0' : '1';
        if (bit < BITS - 1 && (bit & 7) == 7)
            g_text[length++] = ',';
        value <<= 1;
    }
    return length;
}

/* Fill the text buffer from the format and the extra arguments. */
static void build_text(const char *format, va_list args)
{
    int length = 0;
    int i = 0;

    while (format[i] != '\0' && length < TEXT_LIMIT) {
        if (format[i] == '/') {
            switch (format[i + 1]) {
            case '/':
                ++i;
                g_text[length++] = '/';
                break;
            case 'I':
                ++i;
                length = append_grouped(length, va_arg(args, int));
                break;
            case 'b':
                ++i;
                length = append_bits(length, va_arg(args, unsigned));
                break;
            case 'c': {
                unsigned letter;

                ++i;
                letter = va_arg(args, unsigned);
                if (letter != 0)
                    g_text[length++] = (char)letter;
                break;
            }
            case 'f':
                ++i;
                sprintf(g_number, "%.3f", va_arg(args, double));
                length = append_number(length);
                break;
            case 'i':
                ++i;
                sprintf(g_number, "%d", va_arg(args, int));
                length = append_number(length);
                break;
            case 's':
                ++i;
                sprintf(g_number, "%s", va_arg(args, char *));
                length = append_number(length);
                break;
            case 'u':
                ++i;
                sprintf(g_number, "%u", va_arg(args, unsigned));
                length = append_number(length);
                break;
            case 'x':
                ++i;
                sprintf(g_number, "%X", va_arg(args, unsigned));
                length = append_number(length);
                break;
            default:
                break;
            }
        } else
            g_text[length++] = format[i];
        ++i;
    }
    g_text[length] = 0;
}

/* The texture coordinates of one corner of a letter. */
static const float *corner_coord(const Font *font, unsigned char letter, int corner, int fixed)
{
    return &font->coordinates[(fixed ? FIXED_GLYPH : letter * 8) + corner * 2];
}

/* Draw the text with a shadow of one unit down and to the right. */
static void draw_shadow(const Font *font, float x, float y, float size, unsigned flags, float alpha, int fixed)
{
    float advance = 0.0f;
    float vertex[2];
    int i;

    glBegin(GL_QUADS);
    glColor4f(0.0f, 0.0f, 0.0f, alpha);
    for (i = 0; g_text[i] != '\0' && i < TEXT_LIMIT; ++i) {
        unsigned char letter = (unsigned char)g_text[i];
        float width = font->width[letter];

        vertex[0] = (float)(x + advance + 1.0);
        vertex[1] = (flags & FLAG_FLIP) == 0 ? (float)(y + 1.0) : (float)(y + 1.0 + size);
        if ((flags & FLAG_VERTICAL) != 0) {
            vertex[0] = (float)(x + 1.0 + size);
            vertex[1] = (float)(y + advance + 1.0);
        }
        glTexCoord2fv(corner_coord(font, letter, 0, fixed));
        glVertex2fv(vertex);
        vertex[0] = (float)(width * size + (x + advance) + 1.0);
        vertex[1] = (flags & FLAG_FLIP) == 0 ? (float)(y + 1.0) : (float)(y + 1.0 + size);
        if ((flags & FLAG_VERTICAL) != 0) {
            vertex[0] = (float)(x + 1.0 + size);
            vertex[1] = (float)(width * size + (y + advance) + 1.0);
        }
        glTexCoord2fv(corner_coord(font, letter, 1, fixed));
        glVertex2fv(vertex);
        vertex[1] = (flags & FLAG_FLIP) == 0 ? (float)(y + 1.0 + size) : (float)(y + 1.0);
        vertex[0] = width * size + (x + advance);
        if ((flags & FLAG_VERTICAL) != 0) {
            vertex[1] = (float)(width * size + (y + advance) + 1.0);
            vertex[0] = x;
        }
        vertex[0] = (float)(vertex[0] + 1.0);
        glTexCoord2fv(corner_coord(font, letter, 2, fixed));
        glVertex2fv(vertex);
        vertex[1] = (flags & FLAG_FLIP) == 0 ? (float)(y + 1.0 + size) : (float)(y + 1.0);
        vertex[0] = x + advance;
        if ((flags & FLAG_VERTICAL) != 0) {
            vertex[1] = (float)(y + advance + 1.0);
            vertex[0] = x;
        }
        vertex[0] = (float)(vertex[0] + 1.0);
        glTexCoord2fv(corner_coord(font, letter, 3, fixed));
        glVertex2fv(vertex);
        advance = (float)(width * size + advance);
    }
    glEnd();
}

/* Draw the letters of the text in the color. */
static void draw_letters(const Font *font, float x, float y, float size, unsigned flags, float red, float green,
                         float blue, float alpha, int fixed)
{
    float advance = 0.0f;
    float vertex[2];
    int i;

    glBegin(GL_QUADS);
    glColor4f(red, green, blue, alpha);
    for (i = 0; g_text[i] != '\0' && i < TEXT_LIMIT; ++i) {
        unsigned char letter = (unsigned char)g_text[i];
        float width = font->width[letter];

        vertex[1] = (flags & FLAG_FLIP) == 0 ? y : (float)(y + size);
        vertex[0] = advance;
        if ((flags & FLAG_VERTICAL) != 0) {
            vertex[1] = (float)(y + advance);
            vertex[0] = size;
        }
        vertex[0] = (float)(x + vertex[0]);
        glTexCoord2fv(corner_coord(font, letter, 0, fixed));
        glVertex2fv(vertex);
        vertex[0] = (float)(width * size + (x + advance));
        vertex[1] = (flags & FLAG_FLIP) == 0 ? y : (float)(y + size);
        if ((flags & FLAG_VERTICAL) != 0) {
            vertex[0] = (float)(x + size);
            vertex[1] = (float)(width * size + (y + advance));
        }
        glTexCoord2fv(corner_coord(font, letter, 1, fixed));
        glVertex2fv(vertex);
        vertex[0] = (float)(width * size + (x + advance));
        vertex[1] = (flags & FLAG_FLIP) == 0 ? (float)(y + size) : y;
        if ((flags & FLAG_VERTICAL) != 0) {
            vertex[0] = x;
            vertex[1] = (float)(width * size + (y + advance));
        }
        glTexCoord2fv(corner_coord(font, letter, 2, fixed));
        glVertex2fv(vertex);
        vertex[0] = (float)(x + advance);
        vertex[1] = (flags & FLAG_FLIP) == 0 ? (float)(y + size) : y;
        if ((flags & FLAG_VERTICAL) != 0) {
            vertex[0] = x;
            vertex[1] = (float)(y + advance);
        }
        glTexCoord2fv(corner_coord(font, letter, 3, fixed));
        glVertex2fv(vertex);
        advance = (float)(width * size + advance);
    }
    glEnd();
}

/* Draw a text. The place (x, y) is the corner of the first letter. The flags move it: bit 0
 * centers the text along its line, bit 1 puts its end on the place, bit 2 centers it across
 * the line and bit 3 puts its bottom on the place. Bit 4 draws every letter with the one
 * fixed glyph, bit 5 adds a shadow, bit 6 skips the codes, bit 7 flips the letters and bit 8
 * writes the text from top to bottom. */
void __cdecl drawtext(const char *text, float x, float y, float size, unsigned flags, float red, float green,
                      float blue, float alpha, ...)
{
    const Font *font = &g_fonts[g_fontnum];
    float length = 0.0f;
    int fixed = (flags & FLAG_FIXED_GLYPH) != 0;
    va_list args;
    int i;

    va_start(args, alpha);
    if ((flags & FLAG_PLAIN) == 0)
        build_text(text, args);
    else
        strcpy(g_text, text);
    va_end(args);
    for (i = 0; g_text[i] != '\0' && i < TEXT_LIMIT; ++i)
        length = (float)(font->width[(unsigned char)g_text[i]] * size + length);
    if ((flags & FLAG_VERTICAL) == 0) {
        if ((flags & FLAG_CENTER_X) == 0) {
            if ((flags & FLAG_RIGHT) != 0)
                x = (float)(x - length);
        } else
            x = (float)(x - length * 0.5);
        if ((flags & FLAG_CENTER_Y) == 0) {
            if ((flags & FLAG_BOTTOM) != 0)
                y = (float)(y - size);
        } else
            y = (float)(y - size * 0.5);
    } else {
        if ((flags & FLAG_CENTER_X) == 0) {
            if ((flags & FLAG_RIGHT) != 0)
                y = (float)(y - length);
        } else
            y = (float)(y - length * 0.5);
        if ((flags & FLAG_CENTER_Y) == 0) {
            if ((flags & FLAG_BOTTOM) != 0)
                x = (float)(x - size);
        } else
            x = (float)(x - size * 0.5);
    }
    glBindTexture(GL_TEXTURE_2D, g_textures[font->texture].gl_name);
    if ((flags & FLAG_SHADOW) != 0)
        draw_shadow(font, x, y, size, flags, alpha, fixed);
    draw_letters(font, x, y, size, flags, red, green, blue, alpha, fixed);
}
