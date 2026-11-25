/* Check the start values of the native client storage.
 *
 * cl_storage_init gives each object its start value. This test compares every object that
 * starts with data (and holds no pointer) with the bytes of the .data of the client image
 * (build/client_image/image_client.c). It prints each difference and exits with 1 if there is any.
 * A text pointer table is checked by the smoke test, not here.
 *
 * Build (see tools/build_client_storage_test.bat): SR_CLIENT_NATIVE, the object of
 * src/client/storage.c and build/client_image/image_client.c.
 */
#include "sr_runtime.h"
#include "image_client.h"

#include <stdio.h>
#include <string.h>

void cl_storage_init(void);

static const struct {
    const char *name;
    const void *address;
    unsigned rva;
    unsigned bytes;
} regions[] = {
#include "storage_regions.inc"
};

int main(void)
{
    unsigned i;
    int bad = 0;
    cl_storage_init();
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
    printf("%u objects checked, %d different\n", (unsigned)(sizeof regions / sizeof regions[0]), bad);
    return bad != 0;
}
