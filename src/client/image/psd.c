/* Photoshop image decoding for eight-bit RGB channels.
 * Channels occupy separate planes in the file and interleaved RGBA bytes in memory.
 * The loader accepts raw planes and PackBits runs.
 */
#include "image/psd.h"
#include "image/io.h"
#include "image/png.h"
#include "sr_os.h"

enum { PSD_SIGNATURE = 0x38425053, PSD_VERSION = 1, PSD_CHANNELS = 4,
       PSD_RGB = 3, PSD_NOOP = 128 };

enum {
    PSD_ERROR_SIGNATURE = 0x1120d8,
    PSD_ERROR_VERSION = 0x1120e0,
    PSD_ERROR_CHANNELS = 0x1120f0,
    PSD_ERROR_DEPTH = 0x112104,
    PSD_ERROR_COLOR = 0x11211c,
    PSD_ERROR_COMPRESSION = 0x112130,
    PSD_ERROR_MEMORY = 0x112140
};

/* Decode a PackBits plane. The stream supplies the run lengths. */
static void psd_runs(ImageReader *reader, uint8_t *output, int pixels)
{
    int written = 0;
    while (written < pixels) {
        unsigned command = image_get8(reader), count;
        if (command == PSD_NOOP)
            continue;
        if (command < PSD_NOOP) {
            count = command + 1;
            written += count;
            while (count--) {
                *output = (uint8_t)image_get8(reader);
                output += PSD_CHANNELS;
            }
        } else {
            uint8_t value = (uint8_t)image_get8(reader);
            count = (command ^ 255) + 2;
            written += count;
            while (count--) {
                *output = value;
                output += PSD_CHANNELS;
            }
        }
    }
}

/* Load planar pixels. Return null for an unsupported header or allocation failure. */
void *psd_load(ImageReader *reader, int *width, int *height, int *channels, int requested)
{
    int count, rows, columns, compression, index, channel, pixels;
    uint8_t *output;
    unsigned error = 0;
    if (image_get32(reader) != PSD_SIGNATURE)
        error = PSD_ERROR_SIGNATURE;
    else if (image_get16(reader) != PSD_VERSION)
        error = PSD_ERROR_VERSION;
    if (error) {
        image_fail(error);
        return NULL;
    }
    image_skip(reader, 6);
    count = image_get16(reader);
    if (count < 0 || count > 16) {
        image_fail(PSD_ERROR_CHANNELS);
        return NULL;
    }
    rows = image_get32(reader);
    columns = image_get32(reader);
    if (image_get16(reader) != 8)
        error = PSD_ERROR_DEPTH;
    else if (image_get16(reader) != PSD_RGB)
        error = PSD_ERROR_COLOR;
    if (error) {
        image_fail(error);
        return NULL;
    }
    for (index = 0; index < 3; index++) {
        int length = image_get32(reader);
        image_skip(reader, length);
    }
    compression = image_get16(reader);
    if (compression >= 2) {
        image_fail(PSD_ERROR_COMPRESSION);
        return NULL;
    }
    pixels = columns * rows;
    output = os_malloc(pixels * PSD_CHANNELS);
    if (!output) {
        image_fail(PSD_ERROR_MEMORY);
        return NULL;
    }
    if (compression)
        image_skip(reader, rows * count * 2);
    for (channel = 0; channel < PSD_CHANNELS; channel++) {
        uint8_t *dst = output + channel;
        /* Raw planes include the plane at index count. Runs stop before that index. */
        if (compression ? channel < count : channel <= count) {
            if (compression)
                psd_runs(reader, dst, pixels);
            else
                for (index = 0; index < pixels; index++, dst += PSD_CHANNELS)
                    *dst = (uint8_t)image_get8(reader);
        } else {
            for (index = 0; index < pixels; index++, dst += PSD_CHANNELS)
                *dst = channel == 3 ? 255 : 0;
        }
    }
    if (requested && requested != PSD_CHANNELS) {
        output = convert_format(output, PSD_CHANNELS, requested, columns, rows);
        if (!output)
            return NULL;
    }
    if (channels)
        *channels = count;
    *height = rows;
    *width = columns;
    return output;
}
