/* Check the storage of the native client against the original executable.
 *
 * The data files (src/client/data) are plain source. This test compares each object with the
 * original in two ways: the size of every C object, and the bytes of every object that starts with
 * data (and holds no pointer) against the .data of the client image (build/client_image/image_client.c).
 * It prints each difference and exits with 1 if there is any. A name that is in the lists and not in
 * the source stops the build. A text pointer table is checked by the smoke test, not here.
 *
 * Build and run: tools/test_client_storage.bat (after tools/re/gen_client_native.py).
 */
#include "sr_runtime.h"
#include "image_client.h"

#include <stdio.h>
#include <string.h>

static const struct {
    const char *name;
    const void *address;
    unsigned rva;
    unsigned bytes;
} regions[] = {
#include "storage_regions.inc"
};

/* Every object: the size of the C object and the size of the datum in the original. A C object may be
 * bigger (room after an array that the original fills past its end), never smaller. */
static const struct {
    const char *name;
    unsigned size;
    unsigned bytes;
} sizes[] = {
#include "storage_sizes.inc"
};

int main(void)
{
    unsigned i;
    int bad = 0;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        if (sizes[i].size < sizes[i].bytes) {
            printf("too small: %s is %u bytes, the original has %u\n", sizes[i].name, sizes[i].size, sizes[i].bytes);
            bad++;
        }
    }
    for (i = 0; i < sizeof regions / sizeof regions[0]; i++) {
        const unsigned char *want = sr_image_data + (regions[i].rva - sr_image_data_rva);
        if (regions[i].rva < sr_image_data_rva || regions[i].rva + regions[i].bytes > sr_image_data_rva + sr_image_data_size) {
            printf("outside .data: %s\n", regions[i].name);
            bad++;
        } else if (memcmp(regions[i].address, want, regions[i].bytes) != 0) {
            printf("different: %s at 0x%x, %u bytes\n", regions[i].name, regions[i].rva, regions[i].bytes);
            bad++;
        }
    }
    printf("%u sizes and %u start values checked, %d different\n", (unsigned)(sizeof sizes / sizeof sizes[0]),
           (unsigned)(sizeof regions / sizeof regions[0]), bad);
    return bad != 0;
}
