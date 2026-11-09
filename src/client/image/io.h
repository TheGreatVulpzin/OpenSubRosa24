/* Shared byte reads and memory operations for image files. */
#ifndef CL_IMAGE_IO_H
#define CL_IMAGE_IO_H

#include <stddef.h>
#include "sr_image_types.h"

int image_fail(unsigned message_rva);
void image_check(int condition, unsigned expression_rva, unsigned source_rva, unsigned line);
unsigned image_get8(ImageReader *reader);
unsigned image_get16(ImageReader *reader);
unsigned image_get32(ImageReader *reader);
unsigned image_get16le(ImageReader *reader);
unsigned image_get32le(ImageReader *reader);
void image_skip(ImageReader *reader, int count);
void image_getn(ImageReader *reader, void *data, size_t count);
size_t image_read(void *data, size_t count, void *file);
void *image_realloc(void *data, size_t size);
int image_eof(ImageReader *reader);
uint32_t freadswap(void *data, int size, uint32_t count, void *file);

#endif
