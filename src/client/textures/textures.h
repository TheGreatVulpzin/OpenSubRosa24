/* The textures that the client makes at the start of the game. See textures.c. */
#ifndef CL_TEXTURES_TEXTURES_H
#define CL_TEXTURES_TEXTURES_H

#include <stdint.h>

uint32_t pack_pixel(int red, int green, int blue, int alpha);
void texture_from_image(int texture, int wrap_s, int wrap_t, int mag_filter, int min_filter);
void __cdecl generatemipmap(int texture);
void __cdecl generatetexture_randomvec(int texture);
void __cdecl generatetexture_carpet(int material);
void __cdecl generatetexture_billboard(int material);
void __cdecl generatetexture_newspaper(void);

#endif /* CL_TEXTURES_TEXTURES_H */
