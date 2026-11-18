/* TGA test and pixel loading. */
#ifndef CL_IMAGE_TGA_H
#define CL_IMAGE_TGA_H
#include "sr_image_types.h"
int tga_test(ImageReader *reader);
uint8_t *tga_load(ImageReader *reader, int *width, int *height, int *channels, int requested);
#endif
