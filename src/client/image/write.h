/* Little endian image header fields. */
#ifndef CL_IMAGE_WRITE_H
#define CL_IMAGE_WRITE_H
#include <stddef.h>
#include <stdint.h>
void write8(void *file, uint8_t value);
void writefv(void *file, const char *format, const int *values);
void write_pixels(void *file, int rgb_dir, int vdir, int width, int height, int comp, const uint8_t *data,
                  int write_alpha, size_t scanline_pad);
int outfile(const char *filename, int rgb_dir, int vdir, int width, int height, int comp, const uint8_t *data,
            int write_alpha, unsigned scanline_pad, const char *format, ...);
int stbi_write_bmp(const char *filename, int width, int height, int comp, const uint8_t *data);
#endif
