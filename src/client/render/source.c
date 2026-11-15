/* Read a text file, for the source of a shader. */
#include "render/source.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"

enum { SOURCE_LINE = 128 };

/* Read a whole file into a new block. The block has a zero at its end, and the caller frees it.
 * The path and a line break go to the end of the shader log, also when the file does not open.
 * Return NULL when the file does not open. The file handle is the shared open_file. The file
 * goes through source_buffer first, which holds 0x20000 bytes: a longer file writes past it. */
void *__cdecl loadsource(const char *path)
{
    char line[SOURCE_LINE];
    int length = 0;
    int byte;
    void *copy;

    sprintf(line, "%s\r\n", path);
    strcat(cl_data_shader_log, line);
    cl_data_open_file = (int)(uintptr_t)os_fopen(path, "rb");
    if (cl_data_open_file == 0)
        return NULL;
    byte = os_fgetc((void *)(uintptr_t)cl_data_open_file);
    while (byte != -1) {
        cl_data_source_buffer[length++] = (char)byte;
        byte = os_fgetc((void *)(uintptr_t)cl_data_open_file);
    }
    cl_data_source_buffer[length++] = 0;
    os_fclose((void *)(uintptr_t)cl_data_open_file);
    copy = os_malloc((size_t)length);
    memcpy(copy, cl_data_source_buffer, (size_t)length);
    return copy;
}
