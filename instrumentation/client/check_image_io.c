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

/* Report each file operation and return zero when all comparisons pass. */
int check_image_files(const char *input)
{
    int reads = check_reads(input), bytes = check_writes(0), fields = check_writes(1);
    printf("[ 1/3] freadswap %d same %d different 0 crashed\n", reads < 0 ? 0 : reads, reads < 0);
    printf("[ 2/3] write8 %d same %d different 0 crashed\n", bytes < 0 ? 0 : bytes, bytes < 0);
    printf("[ 3/3] writefv %d same %d different 0 crashed\n", fields < 0 ? 0 : fields, fields < 0);
    return reads < 0 || bytes < 0 || fields < 0;
}
