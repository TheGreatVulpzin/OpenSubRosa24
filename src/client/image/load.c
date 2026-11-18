/* Image loading by file or memory buffer for every format.
 * The loader tries each format test in turn: JPEG, PNG, BMP, PSD, HDR, the registered loaders and
 * TGA last, because its test accepts many files. A format test reads from the start and puts the
 * file back where it was. HDR files give floats, which the 8 bit loader converts with a gamma.
 */
#include "image/load.h"
#include <math.h>
#include "client.h"
#include "image/bmp.h"
#include "image/hdr.h"
#include "image/io.h"
#include "image/jpeg.h"
#include "image/png.h"
#include "image/psd.h"
#include "image/tga.h"
#include "sr_os.h"
#include "sr_runtime.h"

enum {
    LOAD_MAX_LOADERS = 32,
    LOAD_SCAN_TYPE = 1,
    LOAD_SCAN_HEADER = 2,
    LOAD_ERROR_OPEN = 0x111004,
    LOAD_ERROR_TYPE_FILE = 0x111010,
    LOAD_ERROR_TYPE_MEMORY = 0x111024,
    LOAD_ERROR_OPEN_FLOAT = 0x11103c,
    LOAD_ERROR_TYPE_FLOAT_FILE = 0x111048,
    LOAD_ERROR_TYPE_FLOAT_MEMORY = 0x11105c,
    LOAD_ERROR_HDR_MEMORY = 0x1111ac,
    LOAD_ERROR_LDR_MEMORY = 0x1111b8
};

/* A loader that a program registers: the tests and the loads of one format. */
typedef struct ImageLoader {
    int(__cdecl *test_memory)(const uint8_t *buffer, int length);
    uint8_t *(__cdecl *load_from_memory)(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested);
    int(__cdecl *test_file)(void *file);
    uint8_t *(__cdecl *load_from_file)(void *file, int *x, int *y, int *comp, int requested);
} ImageLoader;

#define image_loader(index) ((const ImageLoader *)cl_data_image_loaders[index])

/* The text of the last failure. */
char *stbi_failure_reason(void)
{
    return cl_data_image_error;
}

/* Free the pixels of an image. */
void stbi_image_free(void *pixels)
{
    os_free(pixels);
}

/* Add a loader to the list. Return 0 when the list is full. */
int stbi_register_loader(void *loader)
{
    int index;

    for (index = 0; index < LOAD_MAX_LOADERS; ++index) {
        if (cl_data_image_loaders[index] == loader)
            return 1;
        if (cl_data_image_loaders[index] == NULL) {
            cl_data_image_loaders[index] = loader;
            cl_data_image_loader_count = index + 1;
            return 1;
        }
    }
    return 0;
}

/* Set the gamma of the conversion from HDR floats to 8 bit pixels. The value kept is its inverse. */
void stbi_hdr_to_ldr_gamma(float gamma)
{
    cl_data_image_hdr_gamma_inverse = (float)(1.0 / gamma);
}

/* Set the scale of the conversion from HDR floats to 8 bit pixels. The value kept is its inverse. */
void stbi_hdr_to_ldr_scale(float scale)
{
    cl_data_image_hdr_scale_inverse = (float)(1.0 / scale);
}

/* Set the gamma of the conversion from 8 bit pixels to HDR floats. */
void stbi_ldr_to_hdr_gamma(float gamma)
{
    cl_data_image_ldr_gamma = gamma;
}

/* Set the scale of the conversion from 8 bit pixels to HDR floats. */
void stbi_ldr_to_hdr_scale(float scale)
{
    cl_data_image_ldr_scale = scale;
}

/* Convert 8 bit pixels to floats and free the 8 bit pixels. A last alpha channel is not changed
 * by the gamma. Return null with a message when memory is full. */
float *ldr_to_hdr(uint8_t *data, int x, int y, unsigned comp)
{
    float *output = os_malloc((size_t)(x * y * (int)comp * 4));
    int pixel, channel;
    unsigned colors;

    if (output == NULL) {
        os_free(data);
        image_fail(LOAD_ERROR_HDR_MEMORY);
        return NULL;
    }
    colors = (comp & 1) == 0 ? comp - 1 : comp;
    for (pixel = 0; pixel < x * y; pixel++) {
        for (channel = 0; channel < (int)colors; channel++) {
            double value = pow((double)data[pixel * comp + channel] / 255.0, (double)cl_data_image_ldr_gamma);
            output[pixel * comp + channel] = (float)(value * cl_data_image_ldr_scale);
        }
        if (channel < (int)comp)
            output[pixel * comp + channel] = (float)((float)data[pixel * comp + channel] / 255.0);
    }
    os_free(data);
    return output;
}

/* Convert HDR floats to 8 bit pixels and free the floats. The values are rounded and cut to 0 and
 * 255. Return null with a message when memory is full. */
uint8_t *hdr_to_ldr(float *data, int x, int y, unsigned comp)
{
    uint8_t *output = os_malloc((size_t)(x * y * (int)comp));
    int pixel, channel;
    unsigned colors;

    if (output == NULL) {
        os_free(data);
        image_fail(LOAD_ERROR_LDR_MEMORY);
        return NULL;
    }
    colors = (comp & 1) == 0 ? comp - 1 : comp;
    for (pixel = 0; pixel < x * y; pixel++) {
        for (channel = 0; channel < (int)colors; channel++) {
            double power = pow((double)(data[pixel * comp + channel] * cl_data_image_hdr_scale_inverse),
                               (double)cl_data_image_hdr_gamma_inverse);
            float value = (float)(power * 255.0 + 0.5);
            if (value < 0)
                value = 0;
            if (value > 255)
                value = 255;
            output[pixel * comp + channel] = (uint8_t)(int)value;
        }
        if (channel < (int)comp) {
            float value = (float)(data[pixel * comp + channel] * 255 + 0.5f);
            if (value < 0)
                value = 0;
            if (value > 255)
                value = 255;
            output[pixel * comp + channel] = (uint8_t)(int)value;
        }
    }
    os_free(data);
    return output;
}

/* Open a file by its name for binary reading. */
static void *open_image_file(const char *filename)
{
    return os_fopen(filename, "rb");
}

/* Test a JPEG file. The file position stays where it was. */
int stbi_jpeg_test_file(void *file)
{
    JpegImage image;
    long position = image_tell(file);
    int result;

    start_file(&image.reader, file);
    result = decode_jpeg_header(&image, LOAD_SCAN_TYPE);
    image_seek_set(file, position);
    return result;
}

/* Test a JPEG buffer. */
int stbi_jpeg_test_memory(const uint8_t *buffer, int length)
{
    JpegImage image;

    start_mem(&image.reader, buffer, length);
    return decode_jpeg_header(&image, LOAD_SCAN_TYPE);
}

/* Load a JPEG file by its name. */
uint8_t *stbi_jpeg_load(const char *filename, int *x, int *y, int *comp, int requested)
{
    uint8_t *data;
    void *file = open_image_file(filename);

    if (!file)
        return NULL;
    data = stbi_jpeg_load_from_file(file, x, y, comp, requested);
    os_fclose(file);
    return data;
}

/* Load a JPEG from an open file. */
uint8_t *stbi_jpeg_load_from_file(void *file, int *x, int *y, int *comp, int requested)
{
    JpegImage image;

    start_file(&image.reader, file);
    return load_jpeg_image(&image, x, y, comp, requested);
}

/* Load a JPEG from a buffer. */
uint8_t *stbi_jpeg_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested)
{
    JpegImage image;

    start_mem(&image.reader, buffer, length);
    return load_jpeg_image(&image, x, y, comp, requested);
}

/* Test a PNG file. The file position stays where it was. */
int stbi_png_test_file(void *file)
{
    PngImage image;
    long position = image_tell(file);
    int result;

    start_file(&image.reader, file);
    result = parse_png_file(&image, LOAD_SCAN_TYPE, 0);
    image_seek_set(file, position);
    return result;
}

/* Test a PNG buffer. */
int stbi_png_test_memory(const uint8_t *buffer, int length)
{
    PngImage image;

    start_mem(&image.reader, buffer, length);
    return parse_png_file(&image, LOAD_SCAN_TYPE, 0);
}

/* Load a PNG file by its name. */
uint8_t *stbi_png_load(const char *filename, int *x, int *y, int *comp, int requested)
{
    uint8_t *data;
    void *file = open_image_file(filename);

    if (!file)
        return NULL;
    data = stbi_png_load_from_file(file, x, y, comp, requested);
    os_fclose(file);
    return data;
}

/* Load a PNG from an open file. */
uint8_t *stbi_png_load_from_file(void *file, int *x, int *y, int *comp, int requested)
{
    PngImage image;

    start_file(&image.reader, file);
    return do_png(&image, (unsigned *)x, (unsigned *)y, comp, requested);
}

/* Load a PNG from a buffer. */
uint8_t *stbi_png_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested)
{
    PngImage image;

    start_mem(&image.reader, buffer, length);
    return do_png(&image, (unsigned *)x, (unsigned *)y, comp, requested);
}

/* Read the size and the channels of a PNG file by its name. Return 0 when the file is not a PNG. */
int stbi_png_info(const char *filename, int *x, int *y, int *comp)
{
    PngImage image;
    void *file = open_image_file(filename);

    if (!file)
        return 0;
    start_file(&image.reader, file);
    if (parse_png_file(&image, LOAD_SCAN_HEADER, 0)) {
        if (x)
            *x = (int)image.reader.width;
        if (y)
            *y = (int)image.reader.height;
        if (comp)
            *comp = image.reader.channels;
        os_fclose(file);
        return 1;
    }
    os_fclose(file);
    return 0;
}

/* Test the start of a BMP file: the letters BM and a header size of 12, 40, 56 or 108. */
int bmp_test(ImageReader *reader)
{
    int size;

    if (stbi_get8(reader) != 'B')
        return 0;
    if (stbi_get8(reader) != 'M')
        return 0;
    stbi_get32le(reader);
    stbi_get16le(reader);
    stbi_get16le(reader);
    stbi_get32le(reader);
    size = (int)stbi_get32le(reader);
    return size == 12 || size == 40 || size == 56 || size == 108;
}

/* Test a BMP file. The file position stays where it was. */
int stbi_bmp_test_file(void *file)
{
    ImageReader reader;
    long position = image_tell(file);
    int result;

    start_file(&reader, file);
    result = bmp_test(&reader);
    image_seek_set(file, position);
    return result;
}

/* Test a BMP buffer. */
int stbi_bmp_test_memory(const uint8_t *buffer, int length)
{
    ImageReader reader;

    start_mem(&reader, buffer, length);
    return bmp_test(&reader);
}

/* Load a BMP file by its name. */
uint8_t *stbi_bmp_load(const char *filename, int *x, int *y, int *comp, int requested)
{
    uint8_t *data;
    void *file = open_image_file(filename);

    if (!file)
        return NULL;
    data = stbi_bmp_load_from_file(file, x, y, comp, requested);
    os_fclose(file);
    return data;
}

/* Load a BMP from an open file. */
uint8_t *stbi_bmp_load_from_file(void *file, int *x, int *y, int *comp, int requested)
{
    ImageReader reader;

    start_file(&reader, file);
    return bmp_load(&reader, (unsigned *)x, (unsigned *)y, (unsigned *)comp, (unsigned)requested);
}

/* Load a BMP from a buffer. */
uint8_t *stbi_bmp_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested)
{
    ImageReader reader;

    start_mem(&reader, buffer, length);
    return bmp_load(&reader, (unsigned *)x, (unsigned *)y, (unsigned *)comp, (unsigned)requested);
}

/* Test the start of a PSD file: the letters 8BPS. */
int psd_test(ImageReader *reader)
{
    return stbi_get32(reader) == 0x38425053;
}

/* Test a PSD file. The file position stays where it was. */
int stbi_psd_test_file(void *file)
{
    ImageReader reader;
    long position = image_tell(file);
    int result;

    start_file(&reader, file);
    result = psd_test(&reader);
    image_seek_set(file, position);
    return result;
}

/* Test a PSD buffer. */
int stbi_psd_test_memory(const uint8_t *buffer, int length)
{
    ImageReader reader;

    start_mem(&reader, buffer, length);
    return psd_test(&reader);
}

/* Load a PSD file by its name. */
uint8_t *stbi_psd_load(const char *filename, int *x, int *y, int *comp, int requested)
{
    uint8_t *data;
    void *file = open_image_file(filename);

    if (!file)
        return NULL;
    data = stbi_psd_load_from_file(file, x, y, comp, requested);
    os_fclose(file);
    return data;
}

/* Load a PSD from an open file. */
uint8_t *stbi_psd_load_from_file(void *file, int *x, int *y, int *comp, int requested)
{
    ImageReader reader;

    start_file(&reader, file);
    return psd_load(&reader, x, y, comp, requested);
}

/* Load a PSD from a buffer. */
uint8_t *stbi_psd_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested)
{
    ImageReader reader;

    start_mem(&reader, buffer, length);
    return psd_load(&reader, x, y, comp, requested);
}

/* Test the start of an HDR file: the text #?RADIANCE and a line end. */
int hdr_test(ImageReader *reader)
{
    static const char signature[] = "#?RADIANCE\n";
    int index;

    for (index = 0; signature[index]; ++index)
        if (stbi_get8(reader) != (unsigned)signature[index])
            return 0;
    return 1;
}

/* Test an HDR file. The file position stays where it was. */
int stbi_hdr_test_file(void *file)
{
    ImageReader reader;
    long position = image_tell(file);
    int result;

    start_file(&reader, file);
    result = hdr_test(&reader);
    image_seek_set(file, position);
    return result;
}

/* Test an HDR buffer. */
int stbi_hdr_test_memory(const uint8_t *buffer, int length)
{
    ImageReader reader;

    start_mem(&reader, buffer, length);
    return hdr_test(&reader);
}

/* Load an HDR from an open file. */
float *stbi_hdr_load_from_file(void *file, int *x, int *y, int *comp, int requested)
{
    ImageReader reader;

    start_file(&reader, file);
    return hdr_load(&reader, (unsigned *)x, y, comp, requested);
}

/* Load an HDR from a buffer. */
float *stbi_hdr_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested)
{
    ImageReader reader;

    start_mem(&reader, buffer, length);
    return hdr_load(&reader, (unsigned *)x, y, comp, requested);
}

/* Test a TGA file. The file position stays where it was. */
int stbi_tga_test_file(void *file)
{
    ImageReader reader;
    long position = image_tell(file);
    int result;

    start_file(&reader, file);
    result = tga_test(&reader);
    image_seek_set(file, position);
    return result;
}

/* Test a TGA buffer. */
int stbi_tga_test_memory(const uint8_t *buffer, int length)
{
    ImageReader reader;

    start_mem(&reader, buffer, length);
    return tga_test(&reader);
}

/* Load a TGA file by its name. */
uint8_t *stbi_tga_load(const char *filename, int *x, int *y, int *comp, int requested)
{
    uint8_t *data;
    void *file = open_image_file(filename);

    if (!file)
        return NULL;
    data = stbi_tga_load_from_file(file, x, y, comp, requested);
    os_fclose(file);
    return data;
}

/* Load a TGA from an open file. */
uint8_t *stbi_tga_load_from_file(void *file, int *x, int *y, int *comp, int requested)
{
    ImageReader reader;

    start_file(&reader, file);
    return tga_load(&reader, x, y, comp, requested);
}

/* Load a TGA from a buffer. */
uint8_t *stbi_tga_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested)
{
    ImageReader reader;

    start_mem(&reader, buffer, length);
    return tga_load(&reader, x, y, comp, requested);
}

/* Load an image file by its name. The format is found from the content. */
uint8_t *stbi_load(const char *filename, int *x, int *y, int *comp, int requested)
{
    uint8_t *result;
    void *file = open_image_file(filename);

    if (!file) {
        image_fail(LOAD_ERROR_OPEN);
        return NULL;
    }
    result = stbi_load_from_file(file, x, y, comp, requested);
    os_fclose(file);
    return result;
}

/* Load an image from an open file. The format is found from the content. */
uint8_t *stbi_load_from_file(void *file, int *x, int *y, int *comp, int requested)
{
    int index;

    if (stbi_jpeg_test_file(file))
        return stbi_jpeg_load_from_file(file, x, y, comp, requested);
    if (stbi_png_test_file(file))
        return stbi_png_load_from_file(file, x, y, comp, requested);
    if (stbi_bmp_test_file(file))
        return stbi_bmp_load_from_file(file, x, y, comp, requested);
    if (stbi_psd_test_file(file))
        return stbi_psd_load_from_file(file, x, y, comp, requested);
    if (stbi_hdr_test_file(file)) {
        float *pixels = stbi_hdr_load_from_file(file, x, y, comp, requested);
        return hdr_to_ldr(pixels, *x, *y, (unsigned)(requested ? requested : *comp));
    }
    for (index = 0; index < cl_data_image_loader_count; index++)
        if (image_loader(index)->test_file(file))
            return image_loader(index)->load_from_file(file, x, y, comp, requested);
    if (stbi_tga_test_file(file))
        return stbi_tga_load_from_file(file, x, y, comp, requested);
    image_fail(LOAD_ERROR_TYPE_FILE);
    return NULL;
}

/* Load an image from a buffer. The format is found from the content. */
uint8_t *stbi_load_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested)
{
    int index;

    if (stbi_jpeg_test_memory(buffer, length))
        return stbi_jpeg_load_from_memory(buffer, length, x, y, comp, requested);
    if (stbi_png_test_memory(buffer, length))
        return stbi_png_load_from_memory(buffer, length, x, y, comp, requested);
    if (stbi_bmp_test_memory(buffer, length))
        return stbi_bmp_load_from_memory(buffer, length, x, y, comp, requested);
    if (stbi_psd_test_memory(buffer, length))
        return stbi_psd_load_from_memory(buffer, length, x, y, comp, requested);
    if (stbi_hdr_test_memory(buffer, length)) {
        float *pixels = stbi_hdr_load_from_memory(buffer, length, x, y, comp, requested);
        return hdr_to_ldr(pixels, *x, *y, (unsigned)(requested ? requested : *comp));
    }
    for (index = 0; index < cl_data_image_loader_count; index++)
        if (image_loader(index)->test_memory(buffer, length))
            return image_loader(index)->load_from_memory(buffer, length, x, y, comp, requested);
    if (stbi_tga_test_memory(buffer, length))
        return stbi_tga_load_from_memory(buffer, length, x, y, comp, requested);
    image_fail(LOAD_ERROR_TYPE_MEMORY);
    return NULL;
}

/* Load an image file by its name as floats. */
float *stbi_loadf(const char *filename, int *x, int *y, int *comp, int requested)
{
    float *result;
    void *file = open_image_file(filename);

    if (!file) {
        image_fail(LOAD_ERROR_OPEN_FLOAT);
        return NULL;
    }
    result = stbi_loadf_from_file(file, x, y, comp, requested);
    os_fclose(file);
    return result;
}

/* Load an image from an open file as floats. An 8 bit image is converted. */
float *stbi_loadf_from_file(void *file, int *x, int *y, int *comp, int requested)
{
    uint8_t *data;

    if (stbi_hdr_test_file(file))
        return stbi_hdr_load_from_file(file, x, y, comp, requested);
    data = stbi_load_from_file(file, x, y, comp, requested);
    if (data)
        return ldr_to_hdr(data, *x, *y, (unsigned)(requested ? requested : *comp));
    image_fail(LOAD_ERROR_TYPE_FLOAT_FILE);
    return NULL;
}

/* Load an image from a buffer as floats. An 8 bit image is converted. */
float *stbi_loadf_from_memory(const uint8_t *buffer, int length, int *x, int *y, int *comp, int requested)
{
    uint8_t *data;

    if (stbi_hdr_test_memory(buffer, length))
        return stbi_hdr_load_from_memory(buffer, length, x, y, comp, requested);
    data = stbi_load_from_memory(buffer, length, x, y, comp, requested);
    if (data)
        return ldr_to_hdr(data, *x, *y, (unsigned)(requested ? requested : *comp));
    image_fail(LOAD_ERROR_TYPE_FLOAT_MEMORY);
    return NULL;
}

/* Tell if a buffer holds an HDR image. */
int stbi_is_hdr_from_memory(const uint8_t *buffer, int length)
{
    return stbi_hdr_test_memory(buffer, length);
}

/* Tell if an open file holds an HDR image. */
int stbi_is_hdr_from_file(void *file)
{
    return stbi_hdr_test_file(file);
}

/* Tell if a file holds an HDR image. */
int stbi_is_hdr(const char *filename)
{
    void *file = open_image_file(filename);
    int result = 0;

    if (file) {
        result = stbi_is_hdr_from_file(file);
        os_fclose(file);
    }
    return result;
}
