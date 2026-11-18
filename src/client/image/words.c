/* Image failure text and sixteen-bit byte readers.
 * Memory and file readers share the byte access function.
 * Missing bytes contribute zero to a word.
 */
#include "image/io.h"
#include "sr_runtime.h"

/* Store the failure text and return zero. */
int e(const char *message)
{
    cl_data_image_error = (char *)message;
    return 0;
}

/* Read a big endian word, high byte first. */
unsigned get16(ImageReader *reader)
{
    unsigned high = image_get8(reader);
    unsigned low = image_get8(reader);
    return high * 256 + low;
}

/* Read a little endian word, low byte first. */
unsigned get16le(ImageReader *reader)
{
    unsigned low = image_get8(reader);
    unsigned high = image_get8(reader);
    return high * 256 + low;
}
