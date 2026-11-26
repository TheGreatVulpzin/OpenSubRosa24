/* Byte fields for BMP and TGA output.
 * A format selects one, two or four low bytes from each integer.
 * Spaces separate fields and consume no values.
 */
#include <stdio.h>
#include "image/write.h"
#include <stddef.h>
#include "image/io.h"
#include "sr_os.h"

enum {
    IMAGE_ASSERT_WRITE_FORMAT = 0x1121f4,
    IMAGE_WRITE_SOURCE = 0x1121f8
};

/* Write one byte to the file. */
void write8(void *file, uint8_t value)
{
    fwrite(&value, 1, 1, file);
}

/* Write fields in little endian order. Stop at an invalid format character. */
void writefv(void *file, const char *format, const int *values)
{
    while (*format) {
        int size, byte;
        uint32_t value;
        char field = *format++;
        if (field == ' ')
            continue;
        if (field != '1' && field != '2' && field != '4') {
            image_check(0, IMAGE_ASSERT_WRITE_FORMAT, IMAGE_WRITE_SOURCE, 0xed7);
            return;
        }
        size = field - '0';
        value = (uint32_t)*values++;
        for (byte = 0; byte < size; byte++)
            write8(file, (uint8_t)(value >> (byte * 8)));
    }
}

/* Write the pixels of an image, row by row, each row followed by pad zero bytes. A negative vdir
 * writes the bottom row first. rgb_dir -1 writes blue, green, red and 1 writes red, green, blue.
 * One and two channels give gray in three bytes. Four channels are mixed with a pink background
 * unless alpha is not zero. A negative write_alpha puts the last channel before the color, a
 * positive one after it. */
void write_pixels(void *file, int rgb_dir, int vdir, int width, int height, int comp, const uint8_t *data,
                  int write_alpha, size_t scanline_pad)
{
    static const uint8_t background[3] = {255, 0, 255};
    uint32_t zero = 0;
    int row, row_end, column, channel;

    if (vdir < 0) {
        row_end = -1;
        row = height - 1;
    } else {
        row_end = height;
        row = 0;
    }
    for (; row != row_end; row += vdir) {
        for (column = 0; column < width; ++column) {
            const uint8_t *pixel = data + (row * width + column) * comp;
            int values[3];
            if (write_alpha < 0)
                fwrite(&pixel[comp - 1], 1, 1, file);
            switch (comp) {
            case 1:
            case 2:
                values[0] = values[1] = values[2] = pixel[0];
                writefv(file, "111", values);
                break;
            case 4:
                if (!write_alpha) {
                    uint8_t mixed[3];
                    for (channel = 0; channel < 3; ++channel)
                        mixed[channel] = (uint8_t)(background[channel] +
                                                   (int)(((unsigned)pixel[channel] - background[channel]) * pixel[3]) / 255);
                    values[0] = mixed[1 - rgb_dir];
                    values[1] = mixed[1];
                    values[2] = mixed[1 + rgb_dir];
                    writefv(file, "111", values);
                    break;
                }
                /* fall through */
            case 3:
                values[0] = pixel[1 - rgb_dir];
                values[1] = pixel[1];
                values[2] = pixel[1 + rgb_dir];
                writefv(file, "111", values);
                break;
            }
            if (write_alpha > 0)
                fwrite(&pixel[comp - 1], 1, 1, file);
        }
        fwrite(&zero, scanline_pad, 1, file);
    }
}

/* Open a file and write the header fields, then the pixels. The fields follow the format and come
 * after the format as integers. Return 1 when the file opens. */
int outfile(const char *filename, int rgb_dir, int vdir, int width, int height, int comp, const uint8_t *data,
            int write_alpha, unsigned scanline_pad, const char *format, ...)
{
    void *file = os_fopen(filename, "wb");

    if (file != NULL) {
        writefv(file, format, (const int *)(&format + 1));
        write_pixels(file, rgb_dir, vdir, width, height, comp, data, write_alpha, scanline_pad);
        os_fclose(file);
    }
    return file != NULL;
}

/* Write a 24 bit BMP file from pixels of 1 to 4 channels. Rows are padded to four bytes. */
int stbi_write_bmp(const char *filename, int width, int height, int comp, const uint8_t *data)
{
    int pad = (-width * 3) & 3;

    return outfile(filename, -1, -1, width, height, comp, data, 0, (unsigned)pad, "11 4 22 44 44 22 444444",
                   'B', 'M', 14 + 40 + (width * 3 + pad) * height, 0, 0, 14 + 40, 40, width, height, 1, 24, 0, 0, 0, 0,
                   0, 0);
}
