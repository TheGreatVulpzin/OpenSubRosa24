/* Image byte reads and header writes with fresh files for each call.
 * Each comparison checks return values, output bytes and file contents.
 * Supported item sizes include one, two and four bytes.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image/io.h"
#include "image/write.h"
#include "sr_os.h"
#include "plat_test.h"
#include "client.h"

/* Compare the bytes from complete reads, short reads and unsupported item sizes. */
static int check_reads(const char *input)
{
    static const int sizes[] = {1, 2, 4, 3};
    int index, count, side, same = 0;
    for (index = 0; index < 4; index++)
        for (count = 0; count <= 5; count++) {
            uint8_t output[2][32];
            uint32_t result[2];
            char reason[256];
            memset(output, 0xa5, sizeof output);
            for (side = 0; side < 2; side++) {
                void *file;
                plat_test_begin(side);
                file = os_fopen(input, "rb");
                if (!file)
                    return -1;
                result[side] = side ? CL_FN(0x4fdfb0, uint32_t(__cdecl *)(void *, int, unsigned, void *))
                    (output[side], sizes[index], count, file) : freadswap(output[side], sizes[index], count, file);
                os_fclose(file);
                plat_test_end(side);
            }
            if (result[0] != result[1] || memcmp(output[0], output[1], sizeof output[0]) ||
                !plat_test_same(reason, sizeof reason))
                return -1;
            same++;
        }
    return same;
}

/* Compare single-byte writes and multi-byte field formats. */
static int check_writes(int fields)
{
    static const char *formats[] = {"", " ", "1", "2", "4", "1 2 4 1 2"};
    static const int values[] = {0x12345678, -1, (int)0x89abcdefu, 0, 0x7fff};
    int index, side, same = 0;
    for (index = 0; index < (fields ? 6 : 256); index++) {
        char reason[256];
        for (side = 0; side < 2; side++) {
            void *file;
            plat_test_begin(side);
            file = os_fopen("c1-image-output.bin", "wb");
            if (!file)
                return -1;
            if (fields) {
                if (side)
                    CL_FN(0x40ace0, void(__cdecl *)(void *, const char *, const int *))(file, formats[index], values);
                else
                    writefv(file, formats[index], values);
            } else {
                if (side)
                    CL_FN(0x40ae60, void(__cdecl *)(void *, uint8_t))(file, (uint8_t)index);
                else
                    write8(file, (uint8_t)index);
            }
            os_fclose(file);
            plat_test_end(side);
        }
        if (!plat_test_same(reason, sizeof reason))
            return -1;
        same++;
    }
    return same;
}

/* Compare big endian writes of items of one, two, four and three bytes. */
static int check_swap_writes(void)
{
    static const int sizes[] = {1, 2, 4, 3};
    static const uint8_t data[32] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee,
                                     0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
                                     0x0e, 0x0f, 0x10, 0x20};
    int index, count, side, same = 0;
    for (index = 0; index < 4; index++)
        for (count = 0; count <= 5; count++) {
            uint32_t result[2];
            char reason[256];
            for (side = 0; side < 2; side++) {
                void *file;
                plat_test_begin(side);
                file = os_fopen("c1-image-output.bin", "wb");
                if (!file)
                    return -1;
                result[side] = side ? CL_FN(0x4fe0d0, uint32_t(__cdecl *)(const void *, int, uint32_t, void *))
                                          (data, sizes[index], (uint32_t)count, file)
                                    : fwriteswap(data, sizes[index], (uint32_t)count, file);
                os_fclose(file);
                plat_test_end(side);
            }
            if (result[0] != result[1] || !plat_test_same(reason, sizeof reason))
                return -1;
            same++;
        }
    return same;
}

/* Compare the rows that write_pixels writes: every channel count, direction, alpha mode and padding. */
static int check_pixel_writes(void)
{
    uint8_t data[6 * 4];
    int rgb_dir, vdir, comp, alpha, pad, side, index, same = 0;

    for (index = 0; index < (int)sizeof data; index++)
        data[index] = (uint8_t)(index * 37 + 11);
    for (rgb_dir = -1; rgb_dir <= 1; rgb_dir += 2)
        for (vdir = -1; vdir <= 1; vdir += 2)
            for (comp = 1; comp <= 4; comp++)
                for (alpha = -1; alpha <= 1; alpha++)
                    for (pad = 0; pad <= 3; pad += 3) {
                        char reason[256];
                        for (side = 0; side < 2; side++) {
                            void *file;
                            plat_test_begin(side);
                            file = os_fopen("c1-image-output.bin", "wb");
                            if (!file)
                                return -1;
                            if (side)
                                CL_FN(0x40ae90, void(__cdecl *)(void *, int, int, int, int, int, const uint8_t *, int, size_t))
                                    (file, rgb_dir, vdir, 3, 2, comp, data, alpha, (size_t)pad);
                            else
                                write_pixels(file, rgb_dir, vdir, 3, 2, comp, data, alpha, (size_t)pad);
                            os_fclose(file);
                            plat_test_end(side);
                        }
                        if (!plat_test_same(reason, sizeof reason))
                            return -1;
                        same++;
                    }
    return same;
}

/* Compare the file that outfile writes: the header fields after the format, then the rows. */
static int check_outfile(void)
{
    static const uint8_t data[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    int side, comp, same = 0;

    for (comp = 1; comp <= 4; comp++) {
        char reason[256];
        int result[2];
        for (side = 0; side < 2; side++) {
            plat_test_begin(side);
            if (side)
                result[side] = CL_FN(0x40ac50, int(__cdecl *)(const char *, int, int, int, int, int, const uint8_t *, int,
                                                               unsigned, const char *, ...))
                    ("c1-image-output.bin", 1, 1, 2, 1, comp, data, 0, 2u, "1 2 4", 7, 0x1234, 0x56789abc);
            else
                result[side] = outfile("c1-image-output.bin", 1, 1, 2, 1, comp, data, 0, 2u, "1 2 4", 7, 0x1234, 0x56789abc);
            plat_test_end(side);
        }
        if (result[0] != result[1] || !plat_test_same(reason, sizeof reason))
            return -1;
        same++;
    }
    return same;
}

/* Report each file operation and return zero when all comparisons pass. */
int check_image_files(const char *input)
{
    int reads = check_reads(input), bytes = check_writes(0), fields = check_writes(1);
    int swaps = check_swap_writes(), pixels = check_pixel_writes(), outs = check_outfile();
    printf("[ 1/6] freadswap %d same %d different 0 crashed\n", reads < 0 ? 0 : reads, reads < 0);
    printf("[ 2/6] write8 %d same %d different 0 crashed\n", bytes < 0 ? 0 : bytes, bytes < 0);
    printf("[ 3/6] writefv %d same %d different 0 crashed\n", fields < 0 ? 0 : fields, fields < 0);
    printf("[ 4/6] fwriteswap %d same %d different 0 crashed\n", swaps < 0 ? 0 : swaps, swaps < 0);
    printf("[ 5/6] write_pixels %d same %d different 0 crashed\n", pixels < 0 ? 0 : pixels, pixels < 0);
    printf("[ 6/6] outfile %d same %d different 0 crashed\n", outs < 0 ? 0 : outs, outs < 0);
    return reads < 0 || bytes < 0 || fields < 0 || swaps < 0 || pixels < 0 || outs < 0;
}
