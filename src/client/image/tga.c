/* TGA decoding for gray, gray with alpha, RGB and RGBA pixels.
 * Colors can come from a palette. Both plain rows and run length rows are read.
 * The loader gives up on a bad header and returns no pixels and no message.
 */
#include "image/tga.h"
#include "image/io.h"
#include "image/png.h"
#include "sr_os.h"

enum { TGA_RLE_FIRST = 8, TGA_MAX_CHANNELS = 4 };

/* Read the header and test it. Palette or direct color, image types 1, 2, 3, 9, 10 and 11,
 * a width and a height of at least 1 and a pixel size of 8, 16, 24 or 32 bits pass. */
int tga_test(ImageReader *reader)
{
    int value;

    stbi_get8u(reader);
    value = (int)stbi_get8u(reader);
    if (value > 1)
        return 0;
    value = (int)stbi_get8u(reader);
    if (value != 1 && value != 2 && value != 3 && value != 9 && value != 10 && value != 11)
        return 0;
    stbi_get16(reader);
    stbi_get16(reader);
    stbi_get8(reader);
    stbi_get16(reader);
    stbi_get16(reader);
    if ((int)stbi_get16(reader) < 1)
        return 0;
    if ((int)stbi_get16(reader) < 1)
        return 0;
    value = (int)stbi_get8(reader);
    if (value != 8 && value != 16 && value != 24 && value != 32)
        return 0;
    return 1;
}

/* Load the pixels. The result has the requested number of channels, or the number of the file when
 * the request is outside 1 to 4. A bottom up image is turned over. */
uint8_t *tga_load(ImageReader *reader, int *width, int *height, int *channels, int requested)
{
    int offset = (int)stbi_get8u(reader);
    int indexed = (int)stbi_get8u(reader);
    int image_type = (int)stbi_get8u(reader);
    int run_length = 0;
    int palette_start = (int)stbi_get16le(reader);
    int palette_length = (int)stbi_get16le(reader);
    int palette_bits = (int)stbi_get8u(reader);
    int x_origin = (int)stbi_get16le(reader);
    int y_origin = (int)stbi_get16le(reader);
    int image_width = (int)stbi_get16le(reader);
    int image_height = (int)stbi_get16le(reader);
    int bits = (int)stbi_get8u(reader);
    int flags = (int)stbi_get8u(reader);
    uint8_t *pixels;
    uint8_t *palette = NULL;
    int index, byte;
    uint8_t raw[TGA_MAX_CHANNELS];
    uint8_t color[TGA_MAX_CHANNELS];
    int run_count = 0;
    int run_repeating = 0;
    int read_pixel = 1;

    if (image_type >= TGA_RLE_FIRST) {
        image_type -= TGA_RLE_FIRST;
        run_length = 1;
    }
    flags = 1 - ((flags >> 5) & 1);
    if (image_width < 1 || image_height < 1 || image_type < 1 || image_type > 3 ||
        (bits != 8 && bits != 16 && bits != 24 && bits != 32))
        return NULL;
    if (indexed)
        bits = palette_bits;
    *width = image_width;
    *height = image_height;
    if (requested < 1 || requested > TGA_MAX_CHANNELS) {
        requested = bits / 8;
        *channels = requested;
    } else {
        *channels = bits / 8;
    }
    pixels = os_malloc((size_t)(image_width * image_height * requested));
    stbi_skip(reader, offset);
    if (indexed) {
        stbi_skip(reader, palette_start);
        palette = os_malloc((size_t)(palette_length * palette_bits / 8));
        stbi_getn(reader, palette, (size_t)(palette_length * palette_bits / 8));
    }
    for (index = 0; index < image_width * image_height; ++index) {
        if (run_length) {
            if (run_count == 0) {
                int command = (int)stbi_get8u(reader);
                run_count = 1 + (command & 127);
                run_repeating = command >> 7;
                read_pixel = 1;
            } else if (!run_repeating) {
                read_pixel = 1;
            }
        } else {
            read_pixel = 1;
        }
        if (read_pixel) {
            if (indexed) {
                int entry = (int)stbi_get8u(reader);
                if (entry >= palette_length)
                    entry = 0;
                entry *= bits / 8;
                for (byte = 0; byte * 8 < bits; ++byte)
                    raw[byte] = palette[entry + byte];
            } else {
                for (byte = 0; byte * 8 < bits; ++byte)
                    raw[byte] = (uint8_t)stbi_get8u(reader);
            }
            switch (bits) {
            case 8:
                color[0] = raw[0];
                color[1] = raw[0];
                color[2] = raw[0];
                color[3] = 255;
                break;
            case 16:
                color[0] = raw[0];
                color[1] = raw[0];
                color[2] = raw[0];
                color[3] = raw[1];
                break;
            case 24:
                color[0] = raw[2];
                color[1] = raw[1];
                color[2] = raw[0];
                color[3] = 255;
                break;
            case 32:
                color[0] = raw[2];
                color[1] = raw[1];
                color[2] = raw[0];
                color[3] = raw[3];
                break;
            }
            read_pixel = 0;
        }
        switch (requested) {
        case 1:
            pixels[index * requested + 0] = (uint8_t)compute_y(color[0], color[1], color[2]);
            break;
        case 2:
            pixels[index * requested + 0] = (uint8_t)compute_y(color[0], color[1], color[2]);
            pixels[index * requested + 1] = color[3];
            break;
        case 3:
            pixels[index * requested + 0] = color[0];
            pixels[index * requested + 1] = color[1];
            pixels[index * requested + 2] = color[2];
            break;
        case 4:
            pixels[index * requested + 0] = color[0];
            pixels[index * requested + 1] = color[1];
            pixels[index * requested + 2] = color[2];
            pixels[index * requested + 3] = color[3];
            break;
        }
        --run_count;
    }
    if (flags) {
        for (byte = 0; byte * 2 < image_height; ++byte) {
            int top = byte * image_width * requested;
            int bottom = (image_height - 1 - byte) * image_width * requested;
            for (index = image_width * requested; index > 0; --index) {
                uint8_t swap = pixels[top];
                pixels[top] = pixels[bottom];
                pixels[bottom] = swap;
                ++top;
                ++bottom;
            }
        }
    }
    if (palette != NULL)
        os_free(palette);
    (void)x_origin;
    (void)y_origin;
    return pixels;
}
