/* Byte access for image files and memory buffers.
 * Image decoders share one reader with dimensions and channel counts.
 * File reads use the C runtime of the file handle.
 */
#include "image/io.h"
#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The release C runtime has this function, but assert.h declares it only for a debug build. */
void __cdecl _wassert(const wchar_t *expression, const wchar_t *file, unsigned line);

/* Keep the failure message and return zero. */
int stbi_set_error(const char *message)
{
    cl_data_image_error = (char *)message;
    return 0;
}

/* Keep the failure message that the place number names. */
int image_fail(unsigned message_rva)
{
    return stbi_set_error(CL_TEXT(message_rva));
}

/* Stop when a required decoder invariant fails. */
void image_check(int condition, unsigned expression_rva, unsigned source_rva, unsigned line)
{
    if (condition)
        return;
    _wassert(CL_WTEXT(expression_rva), CL_WTEXT(source_rva), line);
}

/* Read one unsigned byte. End of input gives zero. */
unsigned stbi_get8(ImageReader *reader)
{
    if (reader->file != NULL) {
        int c = os_fgetc(reader->file);
        return c == -1 ? 0 : (unsigned)c;
    }
    if (reader->cursor < reader->end)
        return *reader->cursor++;
    return 0;
}

/* Read one byte as an unsigned 8 bit value. */
uint8_t stbi_get8u(ImageReader *reader)
{
    return (uint8_t)stbi_get8(reader);
}

/* Make the reader read from a file. */
void start_file(ImageReader *reader, void *file)
{
    reader->file = file;
}

/* Make the reader read from a memory buffer. */
void start_mem(ImageReader *reader, const uint8_t *buffer, int length)
{
    reader->file = NULL;
    reader->cursor = (uint8_t *)buffer;
    reader->end = (uint8_t *)buffer + length;
}

/* The position of a file. */
long image_tell(void *file)
{
    return os_ftell(file);
}

/* Move a file to a position from its start. */
void image_seek_set(void *file, long position)
{
    os_fseek(file, position, SEEK_SET);
}

/* Read a big endian word. Missing bytes give zero. */
unsigned stbi_get16(ImageReader *reader)
{
    unsigned high = stbi_get8(reader);
    return (high << 8) + stbi_get8(reader);
}

/* Read a big endian double word. Missing bytes give zero. */
unsigned stbi_get32(ImageReader *reader)
{
    unsigned high = stbi_get16(reader);
    return (high << 16) + stbi_get16(reader);
}

/* Read a little endian word. Missing bytes give zero. */
unsigned stbi_get16le(ImageReader *reader)
{
    unsigned low = stbi_get8(reader);
    return low + (stbi_get8(reader) << 8);
}

/* Read a little endian double word. Missing bytes give zero. */
unsigned stbi_get32le(ImageReader *reader)
{
    unsigned low = stbi_get16le(reader);
    return low + (stbi_get16le(reader) << 16);
}

/* Move the cursor by count bytes. The memory cursor has no bounds check. */
void stbi_skip(ImageReader *reader, int count)
{
    if (reader->file == NULL) {
        reader->cursor += count;
        return;
    }
    os_fseek(reader->file, count, SEEK_CUR);
}

/* Read count bytes into data. */
void stbi_getn(ImageReader *reader, void *data, size_t count)
{
    if (reader->file != NULL) {
        image_read(data, count, reader->file);
        return;
    }
    memcpy(data, reader->cursor, count);
    reader->cursor += count;
}

/* Read a byte array from a file. Return the number of bytes read. */
size_t image_read(void *data, size_t count, void *file)
{
    return os_fread(data, 1, count, file);
}

/* Resize an allocation. Return null when memory is full. */
void *image_realloc(void *data, size_t size)
{
    return os_realloc(data, size);
}

/* Return the end flag of a file. */
int image_eof(ImageReader *reader)
{
    return os_feof(reader->file);
}

/* Read big endian items into little endian storage. EOF bytes become 255. */
uint32_t freadswap(void *data, int size, uint32_t count, void *file)
{
    uint8_t *bytes = data;
    uint32_t item;
    int byte;
    if (size != 1 && size != 2 && size != 4)
        return count;
    for (item = 0; item < count; item++)
        for (byte = size - 1; byte >= 0; byte--)
            bytes[item * size + byte] = (uint8_t)os_fgetc(file);
    return count;
}

/* Write big endian items from little endian storage. Item sizes other than 1, 2 and 4 write nothing.
 * Return the item count. */
uint32_t fwriteswap(const void *data, int size, uint32_t count, void *file)
{
    const uint8_t *bytes = data;
    uint32_t item;
    int byte;

    if (size != 1 && size != 2 && size != 4)
        return count;
    for (item = 0; item < count; item++)
        for (byte = size - 1; byte >= 0; byte--)
            os_fputc(bytes[item * size + byte], file);
    return count;
}
