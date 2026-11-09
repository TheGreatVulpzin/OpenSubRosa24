/* Little endian image header fields. */
#ifndef CL_IMAGE_WRITE_H
#define CL_IMAGE_WRITE_H
#include <stdint.h>
void write8(void *file, uint8_t value);
void writefv(void *file, const char *format, const int *values);
#endif
