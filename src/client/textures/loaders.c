/* The loaders that read an image file into a texture.
 *
 * The decoder writes the file into the shared image: width, height and one 32 bit word for each pixel.
 * A texture takes the pixels of the shared image. The path of the last image stays in image_name.
 */
#include "textures/loaders.h"

#include <string.h>

#include "client.h"
#include "sr_os.h"
#include "sr_runtime.h"
#include "textures/textures.h"
#include "textures/upload.h"

void *sr_import(const char *dll, const char *name);

#define g_image cl_data_image
#define g_bigendian cl_data_bigendian
#define g_image_name cl_data_image_name
#define g_textures cl_data_texture

enum { GL_RGBA = 0x1908, GL_CLAMP_TO_EDGE = 0x812f, GL_LINEAR = 0x2601 };

/* Select the file transfers for a machine with the low byte first. */
void checkbigendian(void)
{
    cl_data_bigendian = 0;
    sr_data_file_write = (SrFileTransfer)sr_import("MSVCR100.DLL", "fwrite");
    sr_data_file_read = (SrFileTransfer)sr_import("MSVCR100.DLL", "fread");
}

/* Game functions of other files. */
#define image_file_load \
    CL_FN(0x4010a0, unsigned char *(__cdecl *)(const char *, int *, int *, int *, int))
#define image_file_free CL_FN(0x401010, void(__cdecl *)(void *))

/* Return 1 when the value is a power of two. Zero counts as one. */
static int is_power_of_two(int value)
{
    return ((value - 1) & value) == 0;
}

/* Load an image file into the shared image. The rows go upside down. A pixel is the word with
 * alpha, blue, green and red from the top. When the byte order flag is set the word is red,
 * green, blue and alpha. An image with fewer than 4 channels has alpha 255. An image with fewer
 * than 3 channels reads the bytes after its pixel. Return 1 when the file loads. */
int __cdecl loadimage(const char *path)
{
    int channels;
    unsigned char *data = image_file_load(path, &g_image.width, &g_image.height, &channels, 0);
    int row_size;
    int y;
    int x;

    if (data == NULL)
        return 0;
    row_size = g_image.width * channels;
    for (y = 0; y < g_image.height; y++) {
        for (x = 0; x < g_image.width; x++) {
            const unsigned char *pixel = data + y * row_size + x * channels;
            unsigned red = pixel[0];
            unsigned green = pixel[1];
            unsigned blue = pixel[2];
            unsigned alpha = channels == 4 ? pixel[3] : 0xff;
            uint32_t *out = &g_image.pixels[(g_image.height - 1 - y) * g_image.width + x];

            if (g_bigendian == 0)
                *out = green * 0x100 + alpha * 0x1000000 + blue * 0x10000 + red;
            else
                *out = blue * 0x100 + red * 0x1000000 + green * 0x10000 + alpha;
        }
    }
    image_file_free(data);
    return 1;
}

/* Load an image into a texture and upload it. Both sizes must be powers of two. Return 1 on
 * success. */
int __cdecl loadtexture(int texture, const char *path, int mipmap, int wrap_s, int wrap_t, int mag_filter,
                        int min_filter)
{
    if (!loadimage(path))
        return 0;
    if (!is_power_of_two(g_image.width) || !is_power_of_two(g_image.height))
        return 0;
    strcpy(g_image_name, path);
    texture_from_image(texture, wrap_s, wrap_t, mag_filter, min_filter);
    if (mipmap != 0)
        generatemipmap(texture);
    setuptexture(texture);
    return 1;
}

/* Load and upload a texture without a restriction on its dimensions. */
int loadtexture2(int texture, const char *path, int mipmap, int wrap_s, int wrap_t,
                 int mag_filter, int min_filter)
{
    if (!loadimage(path))
        return 0;
    strcpy(g_image_name, path);
    texture_from_image(texture, wrap_s, wrap_t, mag_filter, min_filter);
    if (mipmap)
        generatemipmap(texture);
    setuptexture(texture);
    return 1;
}

/* As loadtexture, and replace the alpha of every pixel with the alpha argument. The shared
 * image changes too. With the byte order flag set, the alpha is the low byte and is not limited
 * to a byte. */
int __cdecl loadtexturesetalpha(int texture, const char *path, int mipmap, int wrap_s, int wrap_t, int mag_filter,
                                int min_filter, unsigned alpha)
{
    int count;
    int i;

    if (!loadimage(path))
        return 0;
    if (!is_power_of_two(g_image.width) || !is_power_of_two(g_image.height))
        return 0;
    strcpy(g_image_name, path);
    count = g_image.width * g_image.height;
    for (i = 0; i < count; i++) {
        if (g_bigendian == 0)
            g_image.pixels[i] = (g_image.pixels[i] & 0xffffff) | alpha << 24;
        else
            g_image.pixels[i] = (g_image.pixels[i] & 0xffffff00u) | alpha;
    }
    texture_from_image(texture, wrap_s, wrap_t, mag_filter, min_filter);
    if (mipmap != 0)
        generatemipmap(texture);
    setuptexture(texture);
    return 1;
}

/* Copy a part of an image into a texture of width by height pixels. The part starts at (x, y)
 * of the image. The image loads again only when its path is not the path of the last image. The
 * texture keeps one level, clamps at the edges and filters with the linear filter. Rows and
 * columns past the image size keep the memory of the new buffer. The start is not tested
 * against the image size. */
void __cdecl loadtexturepartial(int texture, const char *path, int x, int y, int width, int height)
{
    Texture *t = &g_textures[texture];
    int row;
    int column;

    if (strcmp(g_image_name, path) != 0) {
        if (!loadimage(path))
            return;
        strcpy(g_image_name, path);
    }
    t->width = width;
    t->height = height;
    t->num_levels = 1;
    t->format = GL_RGBA;
    t->internal_format = GL_RGBA;
    t->spare = 0;
    t->wrap_s = GL_CLAMP_TO_EDGE;
    t->wrap_t = GL_CLAMP_TO_EDGE;
    t->mag_filter = GL_LINEAR;
    t->min_filter = GL_LINEAR;
    os_free(t->levels[0]);
    t->levels[0] = os_malloc((size_t)(t->width * t->height * 4));
    for (row = 0; row < t->height; row++) {
        if (row < g_image.height) {
            for (column = 0; column < t->width; column++) {
                if (column < g_image.width)
                    t->levels[0][row * t->width + column] =
                        g_image.pixels[(y + row) * g_image.width + x + column];
            }
        }
    }
    setuptexture(texture);
}
