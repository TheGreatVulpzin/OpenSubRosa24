/* PNG loader of the client: the helpers that read the file header.
 */
#include <stdint.h>

#include "client.h"

/* Read a big endian 32 bit number from the image reader (still lifted C, 0x407000). */
static uint32_t read_be32(int reader)
{
    return (uint32_t)CL_FN(0x407000, int(__cdecl *)(int))(reader);
}

/* Read the length and the type of a chunk. The result holds the length in the low half and the
 * type in the high half. */
uint64_t __cdecl get_chunk_header(int reader)
{
    uint32_t length = read_be32(reader);
    uint32_t type = read_be32(reader);
    return ((uint64_t)type << 32) | length;
}
