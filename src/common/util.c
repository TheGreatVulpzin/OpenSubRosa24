/* Small helpers of the game: the radix sort and the number text. */
#include "util.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sr_runtime.h"

#define g_radix (*SR_AT(Radix, SR_RVA_RADIX))

/* Four passes of one byte each, from the low byte up. Each pass is stable. The work arrays
 * keep the values of the last pass. The list ends with -1 after the last item. */
void csradixsort(void)
{
    Radix *r = &g_radix;
    int *order = r->order;
    int *temp = r->temp;
    int *bucket_count = r->bucket_count;
    int *bucket_offset = r->bucket_offset;
    const uint32_t *keys = r->keys;
    int n = r->count;
    int i, b, k, pass;

    for (i = 0; i < n; i++)
        order[i] = i;
    for (pass = 0; pass < 4; pass++) {
        int shift = pass * 8;

        memset(bucket_count, 0, sizeof r->bucket_count);
        for (i = 0; i < n; i++) {
            temp[i] = order[i];
            bucket_count[(keys[order[i]] >> shift) & 0xff]++;
        }
        bucket_offset[0] = 0;
        for (b = 1; b < 256; b++)
            bucket_offset[b] = bucket_offset[b - 1] + bucket_count[b - 1];
        for (i = 0; i < n; i++) {
            k = temp[i];
            b = (keys[k] >> shift) & 0xff;
            order[bucket_offset[b]++] = k;
        }
    }
    order[n] = -1;
}

void sprintf_comma(char *out, int value)
{
    if (value > 999999999)
        sprintf(out, "%d,%03d,%03d,%03d", value / 1000000000, (value / 1000000) % 1000,
                (value / 1000) % 1000, value % 1000);
    else if (value > 999999)
        sprintf(out, "%d,%03d,%03d", value / 1000000, (value / 1000) % 1000, value % 1000);
    else if (value > 999)
        sprintf(out, "%d,%03d", value / 1000, value % 1000);
    else
        sprintf(out, "%d", value);
}
