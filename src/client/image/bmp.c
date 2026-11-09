/* BMP decoding for palettes and packed RGB samples.
 * The loader accepts core, information and version-four headers.
 * Row padding uses four-byte boundaries. Positive heights store bottom rows first.
 * Run length compression and monochrome samples are unsupported.
 */
#include "image/bmp.h"
#include <stdlib.h>
#include "client.h"
#include "image/io.h"
#include "image/png.h"
#include "sr_os.h"
#include "sr_runtime.h"

enum { BMP_CORE = 12, BMP_INFO = 40, BMP_EXTENDED = 56, BMP_V4 = 108,
       BMP_FILE_HEADER = 14, BMP_PALETTE_SIZE = 256,
       BMP_RGB = 0, BMP_RLE8 = 1, BMP_RLE4 = 2, BMP_MASKS = 3 };

enum {
    BMP_ERROR_RUNS = 0x11201c,
    BMP_ERROR_PALETTE = 0x1120b4,
    BMP_ERROR_DEPTH = 0x1120bc,
    BMP_ERROR_MASKS = 0x1120c4,
    BMP_ERROR_SIGNATURE = 0x111ff4,
    BMP_ERROR_HEADER = 0x111ffc,
    BMP_ERROR_DATA = 0x112008,
    BMP_ERROR_MONOCHROME = 0x112010,
    BMP_ERROR_MEMORY = 0x112024
};

/* Expand a masked sample to eight bits. */
static int bmp_sample(unsigned value, int shift, int count)
{
    return CL_FN(0x409450, int(__cdecl *)(int, int, int))(value, shift, count);
}

/* Read masks and the palette count. Return zero for unsupported compression. */
static int bmp_header(ImageReader *reader, int header, int offset, int bits, unsigned *masks, int *palette_count)
{
    int compression, index;
    if (header == BMP_CORE) {
        if (bits < 24)
            *palette_count = (offset - 38) / 3;
        return 1;
    }
    compression = image_get32le(reader);
    if (compression == BMP_RLE8 || compression == BMP_RLE4)
        return image_fail(BMP_ERROR_RUNS);
    for (index = 0; index < 5; index++)
        image_get32le(reader);
    if (header == BMP_INFO || header == BMP_EXTENDED) {
        if (header == BMP_EXTENDED)
            for (index = 0; index < 4; index++)
                image_get32le(reader);
        if (bits == 16 || bits == 32) {
            if (compression == BMP_RGB) {
                masks[0] = bits == 32 ? 0xff0000 : 0x7c00;
                masks[1] = bits == 32 ? 0xff00 : 0x3e0;
                masks[2] = bits == 32 ? 0xff : 0x1f;
                masks[3] = bits == 32 ? 0xff000000u : 0;
            } else {
                if (compression != BMP_MASKS)
                    return 0;
                for (index = 0; index < 3; index++)
                    masks[index] = image_get32le(reader);
                if (masks[0] == masks[1] && masks[1] == masks[2])
                    return 0;
            }
        }
    } else {
        for (index = 0; index < 4; index++)
            masks[index] = image_get32le(reader);
        for (index = 0; index < 13; index++)
            image_get32le(reader);
    }
    if (bits < 16)
        *palette_count = (offset - BMP_FILE_HEADER - header) >> 2;
    return 1;
}

/* Read indexed rows. Return zero for a bad palette or sample depth. */
static int bmp_palette(ImageReader *reader, uint8_t *output, int channels, int bits, int count, int header, int offset)
{
    uint8_t palette[BMP_PALETTE_SIZE][4];
    int index, row, column, channel, packed = 0;
    unsigned row_bytes, padding;
    if (!count || count > BMP_PALETTE_SIZE)
        return image_fail(BMP_ERROR_PALETTE);
    for (index = 0; index < count; index++) {
        palette[index][2] = (uint8_t)image_get8(reader);
        palette[index][1] = (uint8_t)image_get8(reader);
        palette[index][0] = (uint8_t)image_get8(reader);
        if (header != BMP_CORE)
            image_get8(reader);
        palette[index][3] = 255;
    }
    image_skip(reader, offset - BMP_FILE_HEADER - header - (3 + (header != BMP_CORE)) * count);
    if (bits != 4 && bits != 8)
        return image_fail(BMP_ERROR_DEPTH);
    row_bytes = bits == 4 ? (reader->width + 1) >> 1 : reader->width;
    padding = -row_bytes & 3;
    for (row = 0; row < (int)reader->height; row++) {
        for (column = 0; column < (int)reader->width; column++) {
            if (bits == 8 || !(column & 1))
                packed = image_get8(reader);
            index = bits == 8 ? packed : column & 1 ? packed & 15 : packed >> 4;
            for (channel = 0; channel < channels; channel++)
                *output++ = palette[index][channel];
        }
        image_skip(reader, padding);
    }
    return 1;
}

/* Read RGB rows. Return zero when a required color mask is zero. */
static int bmp_rgb(ImageReader *reader, uint8_t *output, int channels, int bits, const unsigned *masks)
{
    int shift[4] = {0}, counts[4] = {0}, index, row, column;
    int fast = bits == 24 ? 1 : bits == 32 && masks[2] == 255 && masks[1] == 0xff00 &&
               masks[0] == 0xff000000u && masks[3] == 0xff000000u ? 2 : 0;
    unsigned row_bytes = bits == 24 ? reader->width * 3 : bits == 16 ? reader->width * 2 : 0;
    unsigned padding = -row_bytes & 3;
    if (!fast) {
        if (!masks[0] || !masks[1] || !masks[2])
            return image_fail(BMP_ERROR_MASKS);
        for (index = 0; index < 4; index++) {
            shift[index] = CL_FN(0x409340, int(__cdecl *)(unsigned))(masks[index]) - 7;
            counts[index] = CL_FN(0x4093e0, int(__cdecl *)(unsigned))(masks[0]);
        }
    }
    for (row = 0; row < (int)reader->height; row++) {
        for (column = 0; column < (int)reader->width; column++, output += channels) {
            uint8_t alpha;
            if (fast) {
                output[2] = (uint8_t)image_get8(reader);
                output[1] = (uint8_t)image_get8(reader);
                output[0] = (uint8_t)image_get8(reader);
                alpha = fast == 2 ? (uint8_t)image_get8(reader) : 255;
            } else {
                unsigned value = bits == 16 ? image_get16le(reader) : image_get32le(reader);
                for (index = 0; index < 3; index++)
                    output[index] = (uint8_t)bmp_sample(value & masks[index], shift[index], counts[index]);
                alpha = masks[3] ? (uint8_t)bmp_sample(value & masks[3], shift[3], counts[3]) : 255;
            }
            if (channels == 4)
                output[3] = alpha;
        }
        image_skip(reader, padding);
    }
    return 1;
}

/* Load BMP pixels. Return null for unsupported input or insufficient memory. */
void *bmp_load(ImageReader *reader, unsigned *width, unsigned *height, unsigned *channels, unsigned requested)
{
    unsigned masks[4] = {0}, output_channels;
    int offset, header, bits, palette_count = 0, flip, row;
    uint8_t *output;
    if (image_get8(reader) != 'B' || image_get8(reader) != 'M') {
        image_fail(BMP_ERROR_SIGNATURE);
        return NULL;
    }
    image_get32le(reader);
    image_get16le(reader);
    image_get16le(reader);
    offset = image_get32le(reader);
    header = image_get32le(reader);
    if (header != BMP_CORE && header != BMP_INFO && header != BMP_EXTENDED && header != BMP_V4) {
        image_fail(BMP_ERROR_HEADER);
        return NULL;
    }
    cl_data_image_error = (char *)(sr_base + BMP_ERROR_DATA);
    reader->width = header == BMP_CORE ? image_get16le(reader) : image_get32le(reader);
    reader->height = header == BMP_CORE ? image_get16le(reader) : image_get32le(reader);
    if (image_get16le(reader) != 1)
        return NULL;
    bits = image_get16le(reader);
    if (bits == 1) {
        image_fail(BMP_ERROR_MONOCHROME);
        return NULL;
    }
    flip = (int)reader->height > 0;
    reader->height = abs((int)reader->height);
    if (!bmp_header(reader, header, offset, bits, masks, &palette_count))
        return NULL;
    reader->channels = masks[3] ? 4 : 3;
    output_channels = requested && (int)requested >= 3 ? requested : reader->channels;
    output = os_malloc(output_channels * reader->width * reader->height);
    if (!output) {
        image_fail(BMP_ERROR_MEMORY);
        return NULL;
    }
    if (bits < 16) {
        if (!bmp_palette(reader, output, output_channels, bits, palette_count, header, offset)) {
            os_free(output);
            return NULL;
        }
    } else {
        image_skip(reader, offset - BMP_FILE_HEADER - header);
        if (!bmp_rgb(reader, output, output_channels, bits, masks))
            return NULL;
    }
    if (flip)
        for (row = 0; row < (int)reader->height >> 1; row++) {
            unsigned index, size = reader->width * output_channels;
            uint8_t *top = output + row * size;
            uint8_t *bottom = output + (reader->height - 1 - row) * size;
            for (index = 0; index < size; index++) {
                uint8_t value = top[index];
                top[index] = bottom[index];
                bottom[index] = value;
            }
        }
    if (requested && requested != output_channels) {
        output = convert_format(output, output_channels, requested, reader->width, reader->height);
        if (!output)
            return NULL;
    }
    *width = reader->width;
    *height = reader->height;
    if (channels)
        *channels = output_channels;
    return output;
}
