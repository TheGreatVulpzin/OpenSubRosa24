/* Bitmap font atlases and glyph metrics. */
#ifndef CL_IMAGE_FONTS_H
#define CL_IMAGE_FONTS_H

#include <stdint.h>

void createfont(void);
void loadfont(int texture, char *path, float size);
void setfontresolution(uint32_t width, uint32_t height);

#endif
