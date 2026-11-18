/* Shared byte reads and memory operations for image files. */
#ifndef CL_IMAGE_IO_H
#define CL_IMAGE_IO_H

#include <stddef.h>
#include "sr_image_types.h"

int stbi_set_error(const char *message);
int image_fail(unsigned message_rva);
void image_check(int condition, unsigned expression_rva, unsigned source_rva, unsigned line);
unsigned stbi_get8(ImageReader *reader);
uint8_t stbi_get8u(ImageReader *reader);
void start_file(ImageReader *reader, void *file);
void start_mem(ImageReader *reader, const uint8_t *buffer, int length);
long image_tell(void *file);
void image_seek_set(void *file, long position);
unsigned stbi_get16(ImageReader *reader);
unsigned stbi_get32(ImageReader *reader);
unsigned stbi_get16le(ImageReader *reader);
unsigned stbi_get32le(ImageReader *reader);
void stbi_skip(ImageReader *reader, int count);
void stbi_getn(ImageReader *reader, void *data, size_t count);
size_t image_read(void *data, size_t count, void *file);
void *image_realloc(void *data, size_t size);
int image_eof(ImageReader *reader);
uint32_t freadswap(void *data, int size, uint32_t count, void *file);

#endif
