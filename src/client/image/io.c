/* Byte access for image files and memory buffers.
 * Image decoders share one reader with dimensions and channel counts.
 * File reads use the C runtime of the file handle.
 */
#include "image/io.h"
#include "client.h"
#include "sr_os.h"

void *sr_import(const char *dll, const char *name);

/* Store the failure message and return zero. */
int image_fail(unsigned message_rva)
{
    return CL_FN(0x401110, int(__cdecl *)(const char *))((const char *)(sr_base + message_rva));
}

/* Stop when a required decoder invariant fails. */
void image_check(int condition, unsigned expression_rva, unsigned source_rva, unsigned line)
{
    static void(__cdecl *call)(const wchar_t *, const wchar_t *, unsigned);
    if (condition)
        return;
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "_wassert");
    call((const wchar_t *)(sr_base + expression_rva), (const wchar_t *)(sr_base + source_rva), line);
}

/* Read one unsigned byte. End of input gives zero. */
unsigned image_get8(ImageReader *reader)
{
    return CL_FN(0x402320, unsigned(__cdecl *)(ImageReader *))(reader);
}

/* Read a big endian word. Missing bytes give zero. */
unsigned image_get16(ImageReader *reader)
{
    return CL_FN(0x403b50, unsigned(__cdecl *)(ImageReader *))(reader);
}

/* Read a big endian double word. Missing bytes give zero. */
unsigned image_get32(ImageReader *reader)
{
    return CL_FN(0x407000, unsigned(__cdecl *)(ImageReader *))(reader);
}

/* Read a little endian word. Missing bytes give zero. */
unsigned image_get16le(ImageReader *reader)
{
    return CL_FN(0x4083d0, unsigned(__cdecl *)(ImageReader *))(reader);
}

/* Read a little endian double word. Missing bytes give zero. */
unsigned image_get32le(ImageReader *reader)
{
    return CL_FN(0x408400, unsigned(__cdecl *)(ImageReader *))(reader);
}

/* Move the cursor by count bytes. The memory cursor has no bounds check. */
void image_skip(ImageReader *reader, int count)
{
    CL_FN(0x403b10, void(__cdecl *)(ImageReader *, int))(reader, count);
}

/* Read count bytes into data. */
void image_getn(ImageReader *reader, void *data, size_t count)
{
    CL_FN(0x409d10, void(__cdecl *)(ImageReader *, void *, size_t))(reader, data, count);
}

/* Read a byte array from a file. Return the number of bytes read. */
size_t image_read(void *data, size_t count, void *file)
{
    static size_t(__cdecl *call)(void *, size_t, size_t, void *);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "fread");
    return call(data, 1, count, file);
}

/* Resize an allocation. Return null when memory is full. */
void *image_realloc(void *data, size_t size)
{
    static void *(__cdecl *call)(void *, size_t);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "realloc");
    return call(data, size);
}

/* Return the end flag of a file. */
int image_eof(ImageReader *reader)
{
    static int(__cdecl *call)(void *);
    if (!call)
        *(void **)&call = sr_import("MSVCR100.DLL", "feof");
    return call(reader->file);
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
