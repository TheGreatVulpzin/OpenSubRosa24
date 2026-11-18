/* Radiance RGBE images. */
#ifndef CL_IMAGE_HDR_H
#define CL_IMAGE_HDR_H
#include "sr_image_types.h"
float *hdr_load(ImageReader *reader, unsigned *width, int *height, int *channels, int requested);
char *hdr_gettoken(ImageReader *reader, char *buffer);
void hdr_convert(float *output, const uint8_t *rgbe, int channels);
#endif
