/* JPEG marker access.
 * A cached marker is consumed before the byte reader advances.
 * A marker prefix can contain more than one 0xff byte.
 */
#include "image/jpeg.h"
#include "client.h"

enum { NO_MARKER = 255, RAW_BYTE_READER_VA = 0x402300 };

/* Read the cached marker or consume a marker prefix and its following byte. */
char get_marker(JpegImage *image)
{
    uint8_t marker = image->marker;
    if (marker != NO_MARKER) {
        image->marker = NO_MARKER;
        return (char)marker;
    }
    marker = CL_FN(RAW_BYTE_READER_VA, uint8_t(__cdecl *)(ImageReader *))(&image->reader);
    if (marker != NO_MARKER)
        return (char)NO_MARKER;
    do {
        marker = CL_FN(RAW_BYTE_READER_VA, uint8_t(__cdecl *)(ImageReader *))(&image->reader);
    } while (marker == NO_MARKER);
    return (char)marker;
}
