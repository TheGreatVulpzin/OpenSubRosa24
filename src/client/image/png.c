/* PNG decoding for textures and font atlases.
 * Each chunk has a big endian length, a type, a payload and a CRC word.
 * Images contain eight-bit grayscale, RGB, palette or alpha samples.
 * The loader supports all five row filters and Adam7 interlace.
 */
#include "image/png.h"
#include <stdlib.h>
#include <string.h>
#include "client.h"
#include "image/io.h"
#include "sr_os.h"
#include "sr_runtime.h"

enum {
    PNG_IHDR = 0x49484452, PNG_IDAT = 0x49444154, PNG_IEND = 0x49454e44,
    PNG_PLTE = 0x504c5445, PNG_TRNS = 0x74524e53,
    PNG_ANCILLARY = 0x20000000, PNG_PALETTE_SIZE = 256, PNG_ALPHA = 255,
    PNG_LOAD = 0, PNG_TEST = 1, PNG_INFO = 2,
    PNG_MAX_DIMENSION = 1 << 24, PNG_MAX_BYTES = 1 << 30,
    PNG_NONE = 0, PNG_SUB = 1, PNG_UP = 2, PNG_AVERAGE = 3, PNG_PAETH = 4
};

enum {
    PNG_ERROR_SIGNATURE = 0x111c04,
    PNG_ASSERT_CONVERSION_CHANNELS = 0x1110fc,
    PNG_CONVERSION_SOURCE = 0x111090,
    PNG_ERROR_CONVERSION_MEMORY = 0x111084,
    PNG_ASSERT_PIXEL_CHANNELS = 0x111c90,
    PNG_PIXEL_SOURCE = 0x111c18,
    PNG_ERROR_PIXEL_MEMORY = 0x111c84,
    PNG_ERROR_PIXEL_COUNT = 0x111ce4,
    PNG_ERROR_FILTER = 0x111d0c,
    PNG_ASSERT_ALPHA_CHANNELS = 0x111dac,
    PNG_ALPHA_SOURCE = 0x111de0,
    PNG_ERROR_PALETTE_MEMORY = 0x111e4c,
    PNG_ERROR_HEADER_LENGTH = 0x111e78,
    PNG_ERROR_WIDTH = 0x111e88,
    PNG_ERROR_HEIGHT = 0x111e94,
    PNG_ERROR_DEPTH = 0x111ea0,
    PNG_ERROR_COLOR_RANGE = 0x111eac,
    PNG_ERROR_COLOR_TYPE = 0x111eb8,
    PNG_ERROR_COMPRESSION = 0x111ec4,
    PNG_ERROR_FILTER_METHOD = 0x111ed4,
    PNG_ERROR_INTERLACE = 0x111ee8,
    PNG_ERROR_EMPTY = 0x111f00,
    PNG_ERROR_PALETTE_SIZE = 0x111f1c,
    PNG_ERROR_IMAGE_SIZE = 0x111f10,
    PNG_ERROR_FIRST_CHUNK = 0x111e58,
    PNG_ERROR_DUPLICATE_HEADER = 0x111e68,
    PNG_ERROR_PALETTE_LENGTH = 0x111f28,
    PNG_ERROR_PALETTE_TRIPLES = 0x111f38,
    PNG_ERROR_LATE_TRANSPARENCY = 0x111f48,
    PNG_ERROR_TRANSPARENCY_PALETTE = 0x111f58,
    PNG_ERROR_TRANSPARENCY_COUNT = 0x111f6c,
    PNG_ERROR_TRANSPARENCY_ALPHA = 0x111f7c,
    PNG_ERROR_TRANSPARENCY_LENGTH = 0x111f8c,
    PNG_ERROR_MISSING_PALETTE = 0x111f9c,
    PNG_ERROR_COMPRESSED_MEMORY = 0x111fa4,
    PNG_ERROR_INPUT = 0x111fb0,
    PNG_ERROR_MISSING_DATA = 0x111fbc,
    PNG_ERROR_UNKNOWN_CHUNK = 0x111fc4,
    PNG_ERROR_CHANNELS = 0x111fdc
};

/* Read a chunk length and type. The type occupies the high thirty-two bits. */
uint64_t get_chunk_header(ImageReader *reader)
{
    uint32_t length = image_get32(reader);
    uint32_t type = image_get32(reader);
    return ((uint64_t)type << 32) | length;
}

/* Read the PNG signature. Return zero at the first invalid byte. */
int check_png_header(ImageReader *reader)
{
    static const uint8_t signature[] = {137,80,78,71,13,10,26,10};
    unsigned index;
    for (index = 0; index < sizeof signature; index++)
        if (image_get8(reader) != signature[index])
            return image_fail(PNG_ERROR_SIGNATURE);
    return 1;
}

/* Choose the nearest neighbor of the linear estimate. Ties prefer left, then above. */
int paeth(int left, int above, int upper_left)
{
    int estimate = left + above - upper_left;
    int dl = abs(estimate - left), da = abs(estimate - above), du = abs(estimate - upper_left);
    if (dl <= da && dl <= du)
        return left;
    return da <= du ? above : upper_left;
}

/* Compute an eight-bit luminance with integer weights. */
int compute_y(int red, int green, int blue)
{
    return (red * 77 + green * 150 + blue * 29) >> 8;
}

/* Change the channel count and release data. Return null when memory is full. */
void *convert_format(void *data, int channels, int requested, int width, int height)
{
    uint8_t *output, *dst, *src = data;
    int pixel, count = width * height;
    if (channels == requested)
        return data;
    image_check(requested >= 1 && requested <= 4, PNG_ASSERT_CONVERSION_CHANNELS, PNG_CONVERSION_SOURCE, 0x2ef);
    output = os_malloc(count * requested);
    if (!output) {
        os_free(data);
        image_fail(PNG_ERROR_CONVERSION_MEMORY);
        return NULL;
    }
    dst = output;
    for (pixel = 0; pixel < count; pixel++, src += channels, dst += requested) {
        int gray = channels < 3 ? src[0] : compute_y(src[0], src[1], src[2]);
        int alpha = channels == 2 ? src[1] : channels == 4 ? src[3] : PNG_ALPHA;
        if (requested < 3) {
            dst[0] = (uint8_t)gray;
            if (requested == 2)
                dst[1] = (uint8_t)alpha;
        } else {
            dst[0] = src[0];
            dst[1] = channels < 3 ? src[0] : src[1];
            dst[2] = channels < 3 ? src[0] : src[2];
            if (requested == 4)
                dst[3] = (uint8_t)alpha;
        }
    }
    os_free(data);
    return output;
}

/* Restore filtered samples. Return zero for a bad filter, length or allocation. */
int create_png_image_raw(PngImage *image, const uint8_t *raw, unsigned size,
                         int channels, int width, unsigned height)
{
    int source_channels = image->reader.channels;
    unsigned row;
    int pixel, channel, filter;
    image_check(channels == source_channels || channels == source_channels + 1, PNG_ASSERT_PIXEL_CHANNELS, PNG_PIXEL_SOURCE, 0x93b);
    if (cl_data_image_partial)
        height = 1;
    image->pixels = os_malloc(width * height * channels);
    if (!image->pixels)
        return image_fail(PNG_ERROR_PIXEL_MEMORY);
    if (!cl_data_image_partial && image->reader.width == (unsigned)width && image->reader.height == height) {
        if (size != (source_channels * width + 1) * height)
            return image_fail(PNG_ERROR_PIXEL_COUNT);
    }
    for (row = 0; row < height; row++) {
        uint8_t *line = image->pixels + row * width * channels;
        const uint8_t *above = row ? line - width * channels : NULL;
        filter = *raw++;
        if (filter > PNG_PAETH)
            return image_fail(PNG_ERROR_FILTER);
        for (pixel = 0; pixel < width; pixel++) {
            uint8_t *dst = line + pixel * channels;
            for (channel = 0; channel < source_channels; channel++) {
                int left = pixel ? dst[channel - channels] : 0;
                int up = above ? above[pixel * channels + channel] : 0;
                int corner = pixel && above ? above[(pixel - 1) * channels + channel] : 0;
                int prediction = 0;
                switch (filter) {
                case PNG_SUB: prediction = left; break;
                case PNG_UP: prediction = up; break;
                case PNG_AVERAGE: prediction = (left + up) >> 1; break;
                case PNG_PAETH: prediction = paeth(left, up, corner); break;
                }
                dst[channel] = (uint8_t)(*raw++ + prediction);
            }
            if (channels != source_channels)
                dst[source_channels] = PNG_ALPHA;
        }
    }
    return 1;
}

/* Restore full rows or seven interlace passes. Return zero when a pass fails. */
int create_png_image(PngImage *image, const uint8_t *raw, int size, size_t channels, int interlace)
{
    static const unsigned x_start[] = {0,4,0,2,0,1,0};
    static const unsigned y_start[] = {0,0,4,0,2,0,1};
    static const unsigned x_stride[] = {8,8,4,4,2,2,1};
    static const unsigned y_stride[] = {8,8,8,4,4,2,2};
    uint8_t *output;
    unsigned pass, width, height, x, y;
    int partial;
    if (!interlace)
        return create_png_image_raw(image, raw, size, (int)channels, image->reader.width, image->reader.height);
    partial = cl_data_image_partial;
    cl_data_image_partial = 0;
    output = os_malloc(image->reader.width * image->reader.height * channels);
    for (pass = 0; pass < 7; pass++) {
        unsigned used;
        width = (image->reader.width - x_start[pass] - 1 + x_stride[pass]) / x_stride[pass];
        height = (image->reader.height - y_start[pass] - 1 + y_stride[pass]) / y_stride[pass];
        if (!width || !height)
            continue;
        if (!create_png_image_raw(image, raw, size, (int)channels, width, height)) {
            os_free(output);
            return 0;
        }
        for (y = 0; y < height; y++)
            for (x = 0; x < width; x++)
                memcpy(output + ((y * y_stride[pass] + y_start[pass]) * image->reader.width +
                                  x * x_stride[pass] + x_start[pass]) * channels,
                       image->pixels + (y * width + x) * channels, channels);
        os_free(image->pixels);
        used = (unsigned)((width * channels + 1) * height);
        raw += used;
        size -= used;
    }
    image->pixels = output;
    cl_data_image_partial = partial;
    return 1;
}

/* Clear alpha for matching transparent colors. Grayscale also sets nonmatching alpha. */
int compute_transparency(PngImage *image, const uint8_t *color, int channels)
{
    unsigned pixel, count = image->reader.width * image->reader.height;
    uint8_t *data = image->pixels;
    image_check(channels == 2 || channels == 4, PNG_ASSERT_ALPHA_CHANNELS, PNG_ALPHA_SOURCE, 0x9b2);
    for (pixel = 0; pixel < count; pixel++, data += channels) {
        if (channels == 2)
            data[1] = data[0] == color[0] ? 0 : PNG_ALPHA;
        else if (data[0] == color[0] && data[1] == color[1] && data[2] == color[2])
            data[3] = 0;
    }
    return 1;
}

/* Replace palette indices with RGB or RGBA samples. Return zero when memory is full. */
int expand_palette(PngImage *image, const uint8_t *palette, unsigned count, int channels)
{
    unsigned pixel, pixels = image->reader.width * image->reader.height;
    uint8_t *output = os_malloc(pixels * channels);
    uint8_t *dst = output;
    (void)count;
    if (!output)
        return image_fail(PNG_ERROR_PALETTE_MEMORY);
    for (pixel = 0; pixel < pixels; pixel++, dst += channels)
        memcpy(dst, palette + image->pixels[pixel] * 4, channels == 3 ? 3 : 4);
    os_free(image->pixels);
    image->pixels = output;
    return 1;
}

/* Read dimensions and coding methods. Return zero for an unsupported image. */
static int read_png_dimensions(PngImage *image, unsigned length, unsigned *palette_channels, int *interlace)
{
    ImageReader *reader = &image->reader;
    unsigned color;
    if (length != 13)
        return image_fail(PNG_ERROR_HEADER_LENGTH);
    reader->width = image_get32(reader);
    if (reader->width > PNG_MAX_DIMENSION)
        return image_fail(PNG_ERROR_WIDTH);
    reader->height = image_get32(reader);
    if (reader->height > PNG_MAX_DIMENSION)
        return image_fail(PNG_ERROR_HEIGHT);
    if (image_get8(reader) != 8)
        return image_fail(PNG_ERROR_DEPTH);
    color = image_get8(reader);
    if (color > 6)
        return image_fail(PNG_ERROR_COLOR_RANGE);
    if (color == 3)
        *palette_channels = 3;
    else if (color & 1)
        return image_fail(PNG_ERROR_COLOR_TYPE);
    if (image_get8(reader))
        return image_fail(PNG_ERROR_COMPRESSION);
    if (image_get8(reader))
        return image_fail(PNG_ERROR_FILTER_METHOD);
    *interlace = image_get8(reader);
    if (*interlace > 1)
        return image_fail(PNG_ERROR_INTERLACE);
    if (!reader->width || !reader->height)
        return image_fail(PNG_ERROR_EMPTY);
    reader->channels = *palette_channels ? 1 : 1 + ((color & 2) ? 2 : 0) + ((color & 4) != 0);
    if (*palette_channels) {
        if (((unsigned)PNG_MAX_BYTES / reader->width) >> 2 < reader->height)
            return image_fail(PNG_ERROR_PALETTE_SIZE);
    } else if ((unsigned)PNG_MAX_BYTES / reader->width / reader->channels < reader->height) {
        return image_fail(PNG_ERROR_IMAGE_SIZE);
    }
    return 1;
}

/* Parse chunks and decode image samples. Return zero for an invalid chunk or image. */
int parse_png_file(PngImage *image, int scan, unsigned requested)
{
    ImageReader *reader = &image->reader;
    uint8_t palette[PNG_PALETTE_SIZE * 4], transparent[3];
    unsigned palette_channels = 0, palette_count = 0, capacity = 0, used = 0;
    int first = 1, has_transparency = 0, interlace = 0;
    if (!check_png_header(reader))
        return 0;
    if (scan == PNG_TEST)
        return 1;
    for (;;) {
        uint64_t chunk = get_chunk_header(reader);
        unsigned length = (unsigned)chunk, type = (unsigned)(chunk >> 32), index;
        if (first && type != PNG_IHDR)
            return image_fail(PNG_ERROR_FIRST_CHUNK);
        switch (type) {
        case PNG_IHDR:
            if (!first)
                return image_fail(PNG_ERROR_DUPLICATE_HEADER);
            if (!read_png_dimensions(image, length, &palette_channels, &interlace))
                return 0;
            if (!palette_channels && scan == PNG_INFO)
                return 1;
            break;
        case PNG_PLTE:
            if (length > sizeof palette / 4 * 3)
                return image_fail(PNG_ERROR_PALETTE_LENGTH);
            palette_count = length / 3;
            if (palette_count * 3 != length)
                return image_fail(PNG_ERROR_PALETTE_TRIPLES);
            for (index = 0; index < palette_count; index++) {
                palette[index * 4] = (uint8_t)image_get8(reader);
                palette[index * 4 + 1] = (uint8_t)image_get8(reader);
                palette[index * 4 + 2] = (uint8_t)image_get8(reader);
                palette[index * 4 + 3] = PNG_ALPHA;
            }
            break;
        case PNG_TRNS:
            if (image->compressed)
                return image_fail(PNG_ERROR_LATE_TRANSPARENCY);
            if (palette_channels) {
                if (scan == PNG_INFO) {
                    reader->channels = 4;
                    return 1;
                }
                if (!palette_count)
                    return image_fail(PNG_ERROR_TRANSPARENCY_PALETTE);
                if (length > palette_count)
                    return image_fail(PNG_ERROR_TRANSPARENCY_COUNT);
                palette_channels = 4;
                for (index = 0; index < length; index++)
                    palette[index * 4 + 3] = (uint8_t)image_get8(reader);
            } else {
                if (!(reader->channels & 1))
                    return image_fail(PNG_ERROR_TRANSPARENCY_ALPHA);
                if (length != (unsigned)reader->channels * 2)
                    return image_fail(PNG_ERROR_TRANSPARENCY_LENGTH);
                has_transparency = 1;
                for (index = 0; index < (unsigned)reader->channels; index++)
                    transparent[index] = (uint8_t)image_get16(reader);
            }
            break;
        case PNG_IDAT:
            if (palette_channels && !palette_count)
                return image_fail(PNG_ERROR_MISSING_PALETTE);
            if (scan == PNG_INFO) {
                reader->channels = palette_channels;
                return 1;
            }
            if (capacity < used + length) {
                uint8_t *data;
                if (!capacity)
                    capacity = length > 4096 ? length : 4096;
                while (capacity < used + length)
                    capacity *= 2;
                data = image_realloc(image->compressed, capacity);
                if (!data)
                    return image_fail(PNG_ERROR_COMPRESSED_MEMORY);
                image->compressed = data;
            }
            if (reader->file) {
                if (image_read(image->compressed + used, length, reader->file) != length)
                    return image_fail(PNG_ERROR_INPUT);
            } else {
                memcpy(image->compressed + used, reader->cursor, length);
                reader->cursor += length;
            }
            used += length;
            break;
        case PNG_IEND: {
            int expanded_size;
            if (scan != PNG_LOAD)
                return 1;
            if (!image->compressed)
                return image_fail(PNG_ERROR_MISSING_DATA);
            image->expanded = CL_FN(0x405dd0, uint8_t *(__cdecl *)(uint8_t *, int, int *))
                (image->compressed, used, &expanded_size);
            if (!image->expanded)
                return 0;
            os_free(image->compressed);
            image->compressed = NULL;
            reader->output_channels = reader->channels +
                (((requested == (unsigned)reader->channels + 1 && requested != 3 && !palette_channels) || has_transparency) ? 1 : 0);
            if (!create_png_image(image, image->expanded, expanded_size, reader->output_channels, interlace))
                return 0;
            if (has_transparency && !compute_transparency(image, transparent, reader->output_channels))
                return 0;
            if (palette_channels) {
                reader->channels = palette_channels;
                reader->output_channels = requested > 2 ? requested : palette_channels;
                if (!expand_palette(image, palette, palette_count, reader->output_channels))
                    return 0;
            }
            os_free(image->expanded);
            image->expanded = NULL;
            return 1;
        }
        default:
            if (!(type & PNG_ANCILLARY)) {
                char *message = (char *)(sr_base + PNG_ERROR_UNKNOWN_CHUNK);
                for (index = 0; index < 4; index++)
                    message[index] = (char)(type >> (24 - index * 8));
                return image_fail(PNG_ERROR_UNKNOWN_CHUNK);
            }
            image_skip(reader, length);
        }
        image_get32(reader);
        first = 0;
    }
}

/* Load pixels and report dimensions. Return null on a decode or conversion failure. */
void *do_png(PngImage *image, unsigned *width, unsigned *height, int *channels, int requested)
{
    void *output = NULL;
    image->expanded = NULL;
    image->compressed = NULL;
    image->pixels = NULL;
    if (requested < 0 || requested > 4) {
        image_fail(PNG_ERROR_CHANNELS);
        return NULL;
    }
    if (parse_png_file(image, PNG_LOAD, requested)) {
        output = image->pixels;
        image->pixels = NULL;
        if (requested && requested != image->reader.output_channels) {
            output = convert_format(output, image->reader.output_channels, requested,
                                    image->reader.width, image->reader.height);
            image->reader.output_channels = requested;
            if (!output)
                return NULL;
        }
        *width = image->reader.width;
        *height = image->reader.height;
        if (channels)
            *channels = image->reader.channels;
    }
    os_free(image->pixels);
    image->pixels = NULL;
    os_free(image->expanded);
    image->expanded = NULL;
    os_free(image->compressed);
    image->compressed = NULL;
    return output;
}
