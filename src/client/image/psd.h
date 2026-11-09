/* Eight-bit RGB Photoshop images. */
#ifndef CL_IMAGE_PSD_H
#define CL_IMAGE_PSD_H
#include "sr_image_types.h"
void *psd_load(ImageReader *reader, int *width, int *height, int *channels, int requested);
#endif
