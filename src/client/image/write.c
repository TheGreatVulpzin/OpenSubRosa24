/* Byte fields for BMP and TGA output.
 * A format selects one, two or four low bytes from each integer.
 * Spaces separate fields and consume no values.
 */
#include "image/write.h"
#include <stddef.h>
#include "image/io.h"

void *sr_import(const char *dll, const char *name);

enum {
    IMAGE_ASSERT_WRITE_FORMAT = 0x1121f4,
    IMAGE_WRITE_SOURCE = 0x1121f8
};

/* Write one byte to the file. */
void write8(void *file, uint8_t value)
{
    static size_t(__cdecl *call)(const void *, size_t, size_t, void *);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "fwrite");
    call(&value, 1, 1, file);
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
