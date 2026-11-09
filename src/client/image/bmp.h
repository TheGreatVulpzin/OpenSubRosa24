/* BMP pixels and palette loading. */
#ifndef CL_IMAGE_BMP_H
#define CL_IMAGE_BMP_H
#include "sr_image_types.h"
void *bmp_load(ImageReader *reader, unsigned *width, unsigned *height, unsigned *channels, unsigned requested);
#endif
