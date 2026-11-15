/* The loaders that read an image file into a texture. See loaders.c. */
#ifndef CL_TEXTURES_LOADERS_H
#define CL_TEXTURES_LOADERS_H

int __cdecl loadimage(const char *path);
int __cdecl loadtexture(int texture, const char *path, int mipmap, int wrap_s, int wrap_t, int mag_filter,
                        int min_filter);
int __cdecl loadtexturesetalpha(int texture, const char *path, int mipmap, int wrap_s, int wrap_t, int mag_filter,
                                int min_filter, unsigned alpha);
void __cdecl loadtexturepartial(int texture, const char *path, int x, int y, int width, int height);

#endif /* CL_TEXTURES_LOADERS_H */
