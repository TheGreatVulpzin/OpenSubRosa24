/* Eight-bit PNG images, palette expansion and channel conversion. */
#ifndef CL_IMAGE_PNG_H
#define CL_IMAGE_PNG_H

#include <stddef.h>
#include "sr_image_types.h"

uint64_t get_chunk_header(ImageReader *reader);
int check_png_header(ImageReader *reader);
int paeth(int left, int above, int upper_left);
int compute_y(int red, int green, int blue);
void *convert_format(void *data, int channels, int requested, int width, int height);
int create_png_image_raw(PngImage *image, const uint8_t *raw, unsigned size,
                         int channels, int width, unsigned height);
int create_png_image(PngImage *image, const uint8_t *raw, int size, size_t channels, int interlace);
int compute_transparency(PngImage *image, const uint8_t *color, int channels);
int expand_palette(PngImage *image, const uint8_t *palette, unsigned count, int channels);
int parse_png_file(PngImage *image, int scan, unsigned requested);
void *do_png(PngImage *image, unsigned *width, unsigned *height, int *channels, int requested);

#endif
