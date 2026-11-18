/* Radiance RGBE decoding to floating-point samples.
 * The header must select the 32-bit RGBE run length format.
 * Rows use the negative Y and positive X orientation.
 * Short rows and unmarked rows contain flat four-byte pixels.
 */
#include "image/hdr.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "client.h"
#include "image/io.h"
#include "image/jpeg.h"
#include "sr_os.h"

enum { HDR_LINE_SIZE = 1024, HDR_COMPONENTS = 4, HDR_MIN_RUN_WIDTH = 8,
       HDR_MAX_RUN_WIDTH = 32767, HDR_RUN_FLAG = 128 };

enum {
    HDR_ERROR_SIGNATURE = 0x112168,
    HDR_ERROR_FORMAT = 0x112188,
    HDR_ERROR_Y = 0x1121a0,
    HDR_ERROR_X = 0x1121bc,
    HDR_ERROR_ROW_LENGTH = 0x1121d4
};

/* Read a header line into the buffer, without the line end. A line with 1023 bytes or more is
 * cut and the rest of the line is skipped. The line also ends at the end of the input. */
char *hdr_gettoken(ImageReader *reader, char *buffer)
{
    int length = 0;
    char c = (char)stbi_get8(reader);

    while (!at_eof(reader) && c != '\n') {
        buffer[length++] = c;
        if (length == HDR_LINE_SIZE - 1) {
            while (!at_eof(reader) && stbi_get8(reader) != '\n')
                ;
            break;
        }
        c = (char)stbi_get8(reader);
    }
    buffer[length] = 0;
    return buffer;
}

/* Convert one RGBE pixel to the requested number of channels. The exponent byte gives the scale
 * 2^(exponent - 136). A zero exponent gives black. One and two channels get the mean of the
 * colors. Two channels get the alpha 1 in the second place, four channels in the fourth. */
void hdr_convert(float *output, const uint8_t *rgbe, int channels)
{
    if (rgbe[3] != 0) {
        float scale = (float)ldexp(1.0f, rgbe[3] - (128 + 8));
        if (channels <= 2) {
            output[0] = (rgbe[0] + rgbe[1] + rgbe[2]) * scale / 3;
        } else {
            output[0] = rgbe[0] * scale;
            output[1] = rgbe[1] * scale;
            output[2] = rgbe[2] * scale;
        }
        if (channels == 2)
            output[1] = 1;
        if (channels == 4)
            output[3] = 1;
        return;
    }
    switch (channels) {
    case 4:
        output[3] = 1;
        output[0] = output[1] = output[2] = 0;
        break;
    case 3:
        output[0] = output[1] = output[2] = 0;
        break;
    case 2:
        output[1] = 1;
        output[0] = 0;
        break;
    case 1:
        output[0] = 0;
        break;
    }
}

/* Read flat pixels from a position in the image. */
static void hdr_flat(ImageReader *reader, float *output, unsigned width, int height,
                     int channels, int row, unsigned column)
{
    uint8_t rgbe[HDR_COMPONENTS];
    for (; row < height; row++, column = 0)
        for (; column < width; column++) {
            stbi_getn(reader, rgbe, sizeof rgbe);
            hdr_convert(output + (row * width + column) * channels, rgbe, channels);
        }
}

/* Load RGBE pixels. Return null for a bad format, orientation or row length. */
float *hdr_load(ImageReader *reader, unsigned *width, int *height, int *channels, int requested)
{
    char line[HDR_LINE_SIZE], *position;
    int format = 0, rows, row, component;
    unsigned columns, column;
    float *output;
    uint8_t *scanline = NULL;
    if (strcmp(hdr_gettoken(reader, line), "#?RADIANCE")) {
        image_fail(HDR_ERROR_SIGNATURE);
        return NULL;
    }
    while (*(position = hdr_gettoken(reader, line)))
        if (!strcmp(position, "FORMAT=32-bit_rle_rgbe"))
            format = 1;
    if (!format) {
        image_fail(HDR_ERROR_FORMAT);
        return NULL;
    }
    position = hdr_gettoken(reader, line);
    if (strncmp(position, "-Y ", 3)) {
        image_fail(HDR_ERROR_Y);
        return NULL;
    }
    rows = strtol(position + 3, &position, 10);
    while (*position == ' ')
        position++;
    if (strncmp(position, "+X ", 3)) {
        image_fail(HDR_ERROR_X);
        return NULL;
    }
    columns = strtol(position + 3, NULL, 10);
    *width = columns;
    *height = rows;
    *channels = 3;
    if (!requested)
        requested = 3;
    output = os_malloc(rows * columns * requested * sizeof *output);
    if ((int)columns < HDR_MIN_RUN_WIDTH || (int)columns > HDR_MAX_RUN_WIDTH) {
        hdr_flat(reader, output, columns, rows, requested, 0, 0);
        return output;
    }
    for (row = 0; row < rows; row++) {
        uint8_t header[HDR_COMPONENTS];
        header[0] = (uint8_t)stbi_get8(reader);
        header[1] = (uint8_t)stbi_get8(reader);
        header[2] = (uint8_t)stbi_get8(reader);
        if (header[0] != 2 || header[1] != 2 || (header[2] & HDR_RUN_FLAG)) {
            header[3] = (uint8_t)stbi_get8(reader);
            hdr_convert(output, header, requested);
            os_free(scanline);
            hdr_flat(reader, output, columns, rows, requested, 0, 1);
            return output;
        }
        header[3] = (uint8_t)stbi_get8(reader);
        if ((unsigned)(header[2] * 256 + header[3]) != columns) {
            os_free(output);
            os_free(scanline);
            image_fail(HDR_ERROR_ROW_LENGTH);
            return NULL;
        }
        if (!scanline)
            scanline = os_malloc(columns * HDR_COMPONENTS);
        for (component = 0; component < HDR_COMPONENTS; component++) {
            column = 0;
            while (column < columns) {
                unsigned count = stbi_get8(reader);
                if (count > HDR_RUN_FLAG) {
                    uint8_t value = (uint8_t)stbi_get8(reader);
                    count -= HDR_RUN_FLAG;
                    while (count--) {
                        scanline[column * HDR_COMPONENTS + component] = value;
                        column++;
                    }
                } else {
                    while (count--) {
                        scanline[column * HDR_COMPONENTS + component] = (uint8_t)stbi_get8(reader);
                        column++;
                    }
                }
            }
        }
        for (column = 0; column < columns; column++)
            hdr_convert(output + (row * columns + column) * requested,
                       scanline + column * HDR_COMPONENTS, requested);
    }
    os_free(scanline);
    return output;
}
