/* The unit vector that a packet sends: one axis and two scaled values. */
#include "math/unit_vector.h"

#include <stdint.h>

#include "vectors.h"

/* Read a unit vector. packed[0] holds the axis in bits 0 and 1 and the sign of that axis in bit 2.
 * The axis gets 1 or -1. The other two axes follow in packed[1] and packed[2]: bit 0 is the sign and the
 * next bits are a value from 0 to 2^bits - 1 that scales to 0 to 1. The result is made unit length. */
void __cdecl unpackunitvector(const uint32_t *packed, float *out, uint8_t bits)
{
    uint32_t axis = packed[0] & 3;
    int next = 1;
    int i;

    out[axis] = (packed[0] & 4) == 0 ? 1.0f : -1.0f;
    for (i = 0; i < 3; i++) {
        if ((uint32_t)i != axis) {
            float value = (float)(packed[next] >> 1 & (1u << (bits & 0x1f)) - 1u) / (float)((1 << (bits & 0x1f)) - 1);

            if ((packed[next] & 1) != 0)
                value = -value;
            out[i] = value;
            next++;
        }
    }
    normalizevector((vec3 *)out, (vec3 *)out);
}
