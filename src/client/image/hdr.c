/* Radiance RGBE decoding to floating-point samples.
 * The header must select the 32-bit RGBE run length format.
 * Rows use the negative Y and positive X orientation.
 * Short rows and unmarked rows contain flat four-byte pixels.
 */
#include "image/hdr.h"
#include <stdlib.h>
#include <string.h>
#include "client.h"
#include "image/io.h"
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

/* Read a header line into the supplied buffer. */
static char *hdr_line(ImageReader *reader, char *line)
{
    return CL_FN(0x40a9b0, char *(__cdecl *)(ImageReader *, char *))(reader, line);
}

/* Convert one RGBE pixel to the requested channel count. */
static void hdr_pixel(float *output, const uint8_t *rgbe, int channels)
{
    CL_FN(0x40aa60, void(__cdecl *)(float *, const uint8_t *, int))(output, rgbe, channels);
}

/* Read flat pixels from a position in the image. */
static void hdr_flat(ImageReader *reader, float *output, unsigned width, int height,
                     int channels, int row, unsigned column)
{
    uint8_t rgbe[HDR_COMPONENTS];
    for (; row < height; row++, column = 0)
        for (; column < width; column++) {
            image_getn(reader, rgbe, sizeof rgbe);
            hdr_pixel(output + (row * width + column) * channels, rgbe, channels);
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
    if (strcmp(hdr_line(reader, line), "#?RADIANCE")) {
        image_fail(HDR_ERROR_SIGNATURE);
        return NULL;
    }
    while (*(position = hdr_line(reader, line)))
        if (!strcmp(position, "FORMAT=32-bit_rle_rgbe"))
            format = 1;
    if (!format) {
        image_fail(HDR_ERROR_FORMAT);
        return NULL;
    }
    position = hdr_line(reader, line);
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
        header[0] = (uint8_t)image_get8(reader);
        header[1] = (uint8_t)image_get8(reader);
        header[2] = (uint8_t)image_get8(reader);
        if (header[0] != 2 || header[1] != 2 || (header[2] & HDR_RUN_FLAG)) {
            header[3] = (uint8_t)image_get8(reader);
            hdr_pixel(output, header, requested);
            os_free(scanline);
            hdr_flat(reader, output, columns, rows, requested, 0, 1);
            return output;
        }
        header[3] = (uint8_t)image_get8(reader);
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
                unsigned count = image_get8(reader);
                if (count > HDR_RUN_FLAG) {
                    uint8_t value = (uint8_t)image_get8(reader);
                    count -= HDR_RUN_FLAG;
                    while (count--) {
                        scanline[column * HDR_COMPONENTS + component] = value;
                        column++;
                    }
                } else {
                    while (count--) {
                        scanline[column * HDR_COMPONENTS + component] = (uint8_t)image_get8(reader);
                        column++;
                    }
                }
            }
        }
        for (column = 0; column < columns; column++)
            hdr_pixel(output + (row * columns + column) * requested,
                       scanline + column * HDR_COMPONENTS, requested);
    }
    os_free(scanline);
    return output;
}
