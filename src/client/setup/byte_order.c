/* The file functions of the game: the client reads and writes files through two pointers. */
#include "setup/byte_order.h"

#include "client.h"
#include "sr_runtime.h"

void *sr_import(const char *dll, const char *name);

/* Set the byte order of the pixels to little endian and point the read and write pointers of the game
 * at the C library functions fread and fwrite. */
void __cdecl checkbigendian(void)
{
    cl_data_bigendian = 0;
    sr_data_file_write = (SrFileTransfer)sr_import("MSVCR100.DLL", "fwrite");
    sr_data_file_read = (SrFileTransfer)sr_import("MSVCR100.DLL", "fread");
}
