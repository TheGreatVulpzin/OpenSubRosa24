/* Baseline JPEG decoding for one or three image components.
 * Canonical Huffman codes produce quantized eight by eight blocks.
 * An integer inverse cosine transform restores each block.
 * Progressive frames and sixteen-bit quantization tables are unsupported.
 */
#include "image/jpeg.h"
#include <limits.h>
#include <string.h>
#include "client.h"
#include "image/io.h"
#include "sr_os.h"

enum { JPEG_BLOCK = 8, JPEG_COEFFICIENTS = 64, JPEG_FAST_BITS = 9,
       JPEG_FAST_SIZE = 512, JPEG_CODE_BITS = 16, JPEG_NO_MARKER = 255,
       JPEG_SOF0 = 0xc0, JPEG_SOF1 = 0xc1, JPEG_SOF2 = 0xc2, JPEG_DHT = 0xc4,
       JPEG_SOI = 0xd8, JPEG_EOI = 0xd9, JPEG_SOS = 0xda, JPEG_DQT = 0xdb,
       JPEG_DRI = 0xdd, JPEG_APP0 = 0xe0, JPEG_APP15 = 0xef, JPEG_COMMENT = 0xfe,
       JPEG_RESTART0 = 0xd0, JPEG_RESTART7 = 0xd7, JPEG_MAX_BYTES = 1 << 30,
       IDCT_SCALE = 4096, IDCT_C0541 = 2217, IDCT_C1847 = -7567,
       IDCT_C0765 = 3135, IDCT_C1175 = 4816, IDCT_C0899 = -3685,
       IDCT_C2562 = -10497, IDCT_C1961 = -8034, IDCT_C0390 = -1597,
       IDCT_C1501 = 6149, IDCT_C3072 = 12586, IDCT_C2053 = 8410, IDCT_C0298 = 1223 };

static const uint8_t jpeg_zigzag[] = {
    0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,
    14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,
    46,53,60,61,54,47,55,62,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63
};

enum {
    JPEG_ASSERT_CODE = 0x1112b0,
    JPEG_CODE_SOURCE = 0x111230,
    JPEG_ERROR_DC_CODE = 0x11121c,
    JPEG_ERROR_AC_CODE = 0x11129c,
    JPEG_ERROR_CODE_LENGTHS = 0x1111c4,
    JPEG_ERROR_PROGRESSIVE = 0x11135c,
    JPEG_ERROR_MARKER = 0x1113c0,
    JPEG_ERROR_RESTART_LENGTH = 0x1113d0,
    JPEG_ERROR_QUANTIZATION_DEPTH = 0x1113dc,
    JPEG_ERROR_QUANTIZATION_TABLE = 0x1113ec,
    JPEG_ERROR_HUFFMAN_HEADER = 0x1113fc,
    JPEG_ERROR_SCAN_COMPONENTS = 0x11140c,
    JPEG_ERROR_SCAN_LENGTH = 0x111424,
    JPEG_ERROR_DC_TABLE = 0x111430,
    JPEG_ERROR_AC_TABLE = 0x11143c,
    JPEG_ERROR_SPECTRAL_START = 0x111448,
    JPEG_ERROR_SUCCESSIVE_BITS = 0x111450,
    JPEG_ERROR_FRAME_MINIMUM = 0x111458,
    JPEG_ERROR_DEPTH = 0x111464,
    JPEG_ERROR_HEIGHT = 0x111470,
    JPEG_ERROR_WIDTH = 0x111484,
    JPEG_ERROR_COMPONENT_COUNT = 0x11148c,
    JPEG_ERROR_FRAME_LENGTH = 0x1114a0,
    JPEG_ERROR_COMPONENT_ID = 0x1114ac,
    JPEG_ERROR_HORIZONTAL_SAMPLING = 0x1114c0,
    JPEG_ERROR_VERTICAL_SAMPLING = 0x1114c8,
    JPEG_ERROR_QUANTIZATION_INDEX = 0x1114d0,
    JPEG_ERROR_SIZE = 0x1114d8,
    JPEG_ERROR_MEMORY = 0x1114e4,
    JPEG_ERROR_START = 0x1114f0,
    JPEG_ERROR_FRAME = 0x1114f8
};

/* Return one at the end of a memory buffer, or the end flag of a file. */
int at_eof(ImageReader *reader)
{
    return reader->file ? image_eof(reader) : reader->cursor >= reader->end;
}

/* Read the pending marker or the next marker byte. */
static unsigned jpeg_marker(JpegImage *image)
{
    return (uint8_t)CL_FN(0x402280, char(__cdecl *)(JpegImage *))(image);
}

/* Empty the entropy buffer and clear the DC predictors of the first three components. The
 * pending marker is cleared and the restart counter starts again. With no restart interval
 * the counter never reaches zero. */
void __cdecl reset(JpegImage *image)
{
    image->num_bits = 0;
    image->bits = 0;
    image->no_more = 0;
    image->component[0].dc_prediction = 0;
    image->component[1].dc_prediction = 0;
    image->component[2].dc_prediction = 0;
    image->marker = JPEG_NO_MARKER;
    image->restart_left = image->restart_interval ? image->restart_interval : 0x7fffffff;
}

/* Fill the entropy buffer. A marker ends input and leaves its byte pending. */
void grow_buffer_unsafe(JpegImage *image)
{
    do {
        unsigned value = image->no_more ? 0 : image_get8(&image->reader);
        if (value == JPEG_NO_MARKER) {
            unsigned marker = image_get8(&image->reader);
            if (marker) {
                image->marker = (uint8_t)marker;
                image->no_more = 1;
                return;
            }
        }
        image->bits = (image->bits << 8) | value;
        image->num_bits += 8;
    } while (image->num_bits <= 24);
}

/* Decode a Huffman symbol. Return minus one when the code is invalid or short. */
int decode(JpegImage *image, const JpegHuffman *table)
{
    unsigned slot, bits, length;
    if (image->num_bits < JPEG_CODE_BITS)
        grow_buffer_unsafe(image);
    slot = table->fast[(image->bits >> ((image->num_bits - JPEG_FAST_BITS) & 31)) & (JPEG_FAST_SIZE - 1)];
    if (slot < 255) {
        length = table->size[slot];
        if (image->num_bits < (int)length)
            return -1;
    } else {
        bits = image->num_bits < 16 ? image->bits << ((16 - image->num_bits) & 31) :
                                     image->bits >> ((image->num_bits - 16) & 31);
        bits &= 0xffff;
        for (length = JPEG_FAST_BITS + 1; bits >= table->max_code[length]; length++)
            ;
        if (length == 17) {
            image->num_bits -= 16;
            return -1;
        }
        if (image->num_bits < (int)length)
            return -1;
        slot = ((image->bits >> ((image->num_bits - length) & 31)) & ((1u << length) - 1)) + table->delta[length];
        image_check(((image->bits >> ((image->num_bits - table->size[slot]) & 31)) &
                     ((1u << table->size[slot]) - 1)) == table->code[slot], JPEG_ASSERT_CODE, JPEG_CODE_SOURCE, 0x3fa);
    }
    image->num_bits -= length;
    return table->value[slot];
}

/* Consume a signed amplitude whose magnitude occupies count bits. */
unsigned extend_receive(JpegImage *image, int count)
{
    unsigned value;
    if (image->num_bits < count)
        grow_buffer_unsafe(image);
    value = (image->bits >> ((image->num_bits - count) & 31)) & ((1u << (count & 31)) - 1);
    image->num_bits -= count;
    if (value < (1u << ((count - 1) & 31)))
        value += (~0u << (count & 31)) + 1;
    return value;
}

/* Decode one block and update its DC predictor. Return zero for a bad code. */
int decode_block(JpegImage *image, int16_t *block, JpegHuffman *dc, JpegHuffman *ac, int component)
{
    int symbol = decode(image, dc), index = 1;
    int prediction;
    if (symbol < 0)
        return image_fail(JPEG_ERROR_DC_CODE);
    memset(block, 0, JPEG_COEFFICIENTS * sizeof *block);
    prediction = symbol ? (int)extend_receive(image, symbol) : 0;
    prediction += image->component[component].dc_prediction;
    image->component[component].dc_prediction = prediction;
    block[0] = (int16_t)prediction;
    do {
        symbol = decode(image, ac);
        if (symbol < 0)
            return image_fail(JPEG_ERROR_AC_CODE);
        if (!(symbol & 15)) {
            if (symbol != 0xf0)
                break;
            index += 16;
        } else {
            index += symbol >> 4;
            block[jpeg_zigzag[index++]] = (int16_t)extend_receive(image, symbol & 15);
        }
    } while (index < JPEG_COEFFICIENTS);
    return 1;
}

/* Transform eight coefficients with twelve-bit cosine constants. */
static void idct_1d(const int *data, int *output)
{
    int shared = (data[2] + data[6]) * IDCT_C0541;
    int even2 = data[6] * IDCT_C1847 + shared;
    int even3 = data[2] * IDCT_C0765 + shared;
    int even0 = (data[0] + data[4]) * IDCT_SCALE;
    int even1 = (data[0] - data[4]) * IDCT_SCALE;
    int odd7 = data[7], odd5 = data[5], odd3 = data[3], odd1 = data[1];
    int sum = (odd7 + odd3 + odd5 + odd1) * IDCT_C1175;
    int pair1 = (odd7 + odd1) * IDCT_C0899 + sum;
    int pair2 = (odd5 + odd3) * IDCT_C2562 + sum;
    int pair3 = (odd7 + odd3) * IDCT_C1961;
    int pair4 = (odd5 + odd1) * IDCT_C0390;
    int out1 = pair1 + pair4 + odd1 * IDCT_C1501;
    int out3 = pair2 + pair3 + odd3 * IDCT_C3072;
    int out5 = pair2 + pair4 + odd5 * IDCT_C2053;
    int out7 = pair1 + pair3 + odd7 * IDCT_C0298;
    output[0] = even0 + even3 + out1;
    output[7] = even0 + even3 - out1;
    output[1] = even1 + even2 + out3;
    output[6] = even1 + even2 - out3;
    output[2] = even1 - even2 + out5;
    output[5] = even1 - even2 - out5;
    output[3] = even0 - even3 + out7;
    output[4] = even0 - even3 - out7;
}

/* Clamp a sample to the byte range. */
static uint8_t jpeg_clamp(int value)
{
    return (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value);
}

/* Restore one block. Column and row passes round before their right shifts. */
void idct_block(uint8_t *output, int stride, const int16_t *data, const uint8_t *quantization)
{
    int intermediate[JPEG_COEFFICIENTS], input[JPEG_BLOCK], transformed[JPEG_BLOCK];
    int column, row, dc_only;
    for (column = 0; column < JPEG_BLOCK; column++) {
        dc_only = 1;
        for (row = 0; row < JPEG_BLOCK; row++) {
            input[row] = data[row * JPEG_BLOCK + column] * quantization[row * JPEG_BLOCK + column];
            if (row && data[row * JPEG_BLOCK + column])
                dc_only = 0;
        }
        if (dc_only) {
            for (row = 0; row < JPEG_BLOCK; row++)
                intermediate[row * JPEG_BLOCK + column] = input[0] * 4;
        } else {
            idct_1d(input, transformed);
            for (row = 0; row < JPEG_BLOCK; row++)
                intermediate[row * JPEG_BLOCK + column] = (transformed[row] + 512) >> 10;
        }
    }
    for (row = 0; row < JPEG_BLOCK; row++) {
        idct_1d(intermediate + row * JPEG_BLOCK, transformed);
        for (column = 0; column < JPEG_BLOCK; column++)
            output[row * stride + column] = jpeg_clamp(((transformed[column] + 65536) >> 17) + 128);
    }
}

/* Build a canonical table from counts of codes of each length. Return zero for overflow. */
int build_huffman(JpegHuffman *table, const int *counts)
{
    int length, slot = 0, code = 0, count, fast;
    for (length = 1; length <= JPEG_CODE_BITS; length++)
        for (count = 0; count < counts[length - 1]; count++)
            table->size[slot++] = (uint8_t)length;
    table->size[slot] = 0;
    slot = 0;
    for (length = 1; length <= JPEG_CODE_BITS; length++) {
        table->delta[length] = slot - code;
        if (table->size[slot] == length) {
            while (table->size[slot] == length)
                table->code[slot++] = (uint16_t)code++;
            if (code - 1 >= (1 << length))
                return image_fail(JPEG_ERROR_CODE_LENGTHS);
        }
        table->max_code[length] = code << (JPEG_CODE_BITS - length);
        code *= 2;
    }
    table->max_code[17] = 0xffffffff;
    memset(table->fast, 0xff, sizeof table->fast);
    for (count = 0; count < slot; count++) {
        length = table->size[count];
        if (length <= JPEG_FAST_BITS)
            for (fast = 0; fast < (1 << (JPEG_FAST_BITS - length)); fast++)
                table->fast[(table->code[count] << (JPEG_FAST_BITS - length)) + fast] = (uint8_t)count;
    }
    return 1;
}

/* Read quantization, Huffman and restart tables. Return zero for unsupported markers. */
unsigned process_marker(JpegImage *image, int marker)
{
    ImageReader *reader = &image->reader;
    int remaining, header, table, index, count, counts[JPEG_CODE_BITS];
    JpegHuffman *codes;
    switch (marker) {
    case JPEG_SOF2: return image_fail(JPEG_ERROR_PROGRESSIVE);
    case JPEG_NO_MARKER: return image_fail(JPEG_ERROR_MARKER);
    case JPEG_DRI:
        if (image_get16(reader) != 4)
            return image_fail(JPEG_ERROR_RESTART_LENGTH);
        image->restart_interval = image_get16(reader);
        return 1;
    case JPEG_DQT:
        remaining = image_get16(reader) - 2;
        while (remaining > 0) {
            header = image_get8(reader);
            if (header >> 4)
                return image_fail(JPEG_ERROR_QUANTIZATION_DEPTH);
            table = header & 15;
            if (table > 3)
                return image_fail(JPEG_ERROR_QUANTIZATION_TABLE);
            for (index = 0; index < JPEG_COEFFICIENTS; index++)
                image->dequantize[table * JPEG_COEFFICIENTS + jpeg_zigzag[index]] = (uint8_t)image_get8(reader);
            remaining -= JPEG_COEFFICIENTS + 1;
        }
        return remaining == 0;
    case JPEG_DHT:
        remaining = image_get16(reader) - 2;
        while (remaining > 0) {
            header = image_get8(reader);
            table = header & 15;
            if ((header >> 4) > 1 || table > 3)
                return image_fail(JPEG_ERROR_HUFFMAN_HEADER);
            count = 0;
            for (index = 0; index < JPEG_CODE_BITS; index++) {
                counts[index] = image_get8(reader);
                count += counts[index];
            }
            remaining -= JPEG_CODE_BITS + 1;
            codes = header >> 4 ? &image->ac[table] : &image->dc[table];
            if (!build_huffman(codes, counts))
                return 0;
            for (index = 0; index < count; index++)
                codes->value[index] = (uint8_t)image_get8(reader);
            remaining -= count;
        }
        return remaining == 0;
    default:
        if ((marker < JPEG_APP0 || marker > JPEG_APP15) && marker != JPEG_COMMENT)
            return 0;
        remaining = image_get16(reader);
        image_skip(reader, remaining - 2);
        return 1;
    }
}

/* Read scan components and selectors. Return zero for bad counts or coding modes. */
int process_scan_header(JpegImage *image)
{
    ImageReader *reader = &image->reader;
    int length = image_get16(reader), index, component, id, selector;
    image->scan_components = image_get8(reader);
    if (image->scan_components < 1 || image->scan_components > 4 || image->scan_components > reader->channels)
        return image_fail(JPEG_ERROR_SCAN_COMPONENTS);
    if (length != image->scan_components * 2 + 6)
        return image_fail(JPEG_ERROR_SCAN_LENGTH);
    for (index = 0; index < image->scan_components; index++) {
        id = image_get8(reader);
        selector = image_get8(reader);
        for (component = 0; component < reader->channels && image->component[component].id != id; component++)
            ;
        if (component == reader->channels)
            return 0;
        image->component[component].dc_table = selector >> 4;
        if (image->component[component].dc_table > 3)
            return image_fail(JPEG_ERROR_DC_TABLE);
        image->component[component].ac_table = selector & 15;
        if (image->component[component].ac_table > 3)
            return image_fail(JPEG_ERROR_AC_TABLE);
        image->order[index] = component;
    }
    if (image_get8(reader))
        return image_fail(JPEG_ERROR_SPECTRAL_START);
    image_get8(reader);
    if (image_get8(reader))
        return image_fail(JPEG_ERROR_SUCCESSIVE_BITS);
    return 1;
}

/* Read frame dimensions and sampling factors. Allocate components only for a full load. */
int process_frame_header(JpegImage *image, int scan)
{
    ImageReader *reader = &image->reader;
    int length = image_get16(reader), component, sampling, hmax = 1, vmax = 1;
    if (length < 11)
        return image_fail(JPEG_ERROR_FRAME_MINIMUM);
    if (image_get8(reader) != 8)
        return image_fail(JPEG_ERROR_DEPTH);
    reader->height = image_get16(reader);
    if (!reader->height)
        return image_fail(JPEG_ERROR_HEIGHT);
    reader->width = image_get16(reader);
    if (!reader->width)
        return image_fail(JPEG_ERROR_WIDTH);
    sampling = image_get8(reader);
    if (sampling != 1 && sampling != 3)
        return image_fail(JPEG_ERROR_COMPONENT_COUNT);
    reader->channels = sampling;
    for (component = 0; component < reader->channels; component++) {
        image->component[component].data = NULL;
        image->component[component].line = NULL;
    }
    if (length != reader->channels * 3 + 8)
        return image_fail(JPEG_ERROR_FRAME_LENGTH);
    for (component = 0; component < reader->channels; component++) {
        JpegComponent *part = &image->component[component];
        part->id = image_get8(reader);
        if (part->id != component && part->id != component + 1)
            return image_fail(JPEG_ERROR_COMPONENT_ID);
        sampling = image_get8(reader);
        part->h = sampling >> 4;
        if (!part->h || part->h > 4)
            return image_fail(JPEG_ERROR_HORIZONTAL_SAMPLING);
        part->v = sampling & 15;
        if (!part->v || part->v > 4)
            return image_fail(JPEG_ERROR_VERTICAL_SAMPLING);
        part->quantization = image_get8(reader);
        if (part->quantization > 3)
            return image_fail(JPEG_ERROR_QUANTIZATION_INDEX);
    }
    if (scan)
        return 1;
    if ((unsigned)JPEG_MAX_BYTES / reader->width / reader->channels < reader->height)
        return image_fail(JPEG_ERROR_SIZE);
    for (component = 0; component < reader->channels; component++) {
        if (image->component[component].h > hmax)
            hmax = image->component[component].h;
        if (image->component[component].v > vmax)
            vmax = image->component[component].v;
    }
    image->h_max = hmax;
    image->v_max = vmax;
    image->mcu_width = hmax * JPEG_BLOCK;
    image->mcu_height = vmax * JPEG_BLOCK;
    image->mcu_columns = (reader->width - 1 + image->mcu_width) / image->mcu_width;
    image->mcu_rows = (reader->height - 1 + image->mcu_height) / image->mcu_height;
    for (component = 0; component < reader->channels; component++) {
        JpegComponent *part = &image->component[component];
        part->width = (reader->width * part->h - 1 + hmax) / hmax;
        part->height = (reader->height * part->v - 1 + vmax) / vmax;
        part->stride = image->mcu_columns * part->h * JPEG_BLOCK;
        part->buffer_height = image->mcu_rows * part->v * JPEG_BLOCK;
        part->allocation = os_malloc(part->stride * part->buffer_height + 15);
        if (!part->allocation) {
            while (--component >= 0) {
                os_free(image->component[component].allocation);
                image->component[component].data = NULL;
            }
            return image_fail(JPEG_ERROR_MEMORY);
        }
        part->data = (uint8_t *)(((uintptr_t)part->allocation + 15) & ~(uintptr_t)15);
        part->line = NULL;
    }
    return 1;
}

/* Find a baseline frame. Return zero for a bad marker or unsupported frame. */
int decode_jpeg_header(JpegImage *image, int scan)
{
    unsigned marker;
    image->marker = JPEG_NO_MARKER;
    if (jpeg_marker(image) != JPEG_SOI)
        return image_fail(JPEG_ERROR_START);
    if (scan == 1)
        return 1;
    marker = jpeg_marker(image);
    while (marker != JPEG_SOF0 && marker != JPEG_SOF1) {
        if (!process_marker(image, marker))
            return 0;
        marker = jpeg_marker(image);
        while (marker == JPEG_NO_MARKER) {
            if (at_eof(&image->reader))
                return image_fail(JPEG_ERROR_FRAME);
            marker = jpeg_marker(image);
        }
    }
    return process_frame_header(image, scan) != 0;
}

/* Decode and store a component block. Return zero when its entropy code is invalid. */
static int jpeg_store_block(JpegImage *image, int component, int x, int y)
{
    JpegComponent *part = &image->component[component];
    int16_t block[JPEG_COEFFICIENTS];
    if (!decode_block(image, block, &image->dc[part->dc_table], &image->ac[part->ac_table], component))
        return 0;
    idct_block(part->data + (y * part->stride + x) * JPEG_BLOCK, part->stride,
               block, image->dequantize + part->quantization * JPEG_COEFFICIENTS);
    return 1;
}

/* Process a restart interval. Return zero when entropy data ends before a restart marker. */
static int jpeg_restart(JpegImage *image)
{
    if (--image->restart_left > 0)
        return 1;
    if (image->num_bits < 24)
        grow_buffer_unsafe(image);
    if (image->marker < JPEG_RESTART0 || image->marker > JPEG_RESTART7)
        return 0;
    reset(image);
    return 1;
}

/* Decode one scan. Return zero for a bad block. A missing restart ends the scan. */
int parse_entropy_coded_data(JpegImage *image)
{
    int row, column, scan, component, x, y, columns, rows;
    reset(image);
    if (image->scan_components == 1) {
        component = image->order[0];
        columns = (image->component[component].width + 7) >> 3;
        rows = (image->component[component].height + 7) >> 3;
        for (row = 0; row < rows; row++)
            for (column = 0; column < columns; column++) {
                if (!jpeg_store_block(image, component, column, row))
                    return 0;
                if (!jpeg_restart(image))
                    return 1;
            }
    } else {
        for (row = 0; row < image->mcu_rows; row++)
            for (column = 0; column < image->mcu_columns; column++) {
                for (scan = 0; scan < image->scan_components; scan++) {
                    component = image->order[scan];
                    for (y = 0; y < image->component[component].v; y++)
                        for (x = 0; x < image->component[component].h; x++)
                            if (!jpeg_store_block(image, component,
                                    column * image->component[component].h + x,
                                    row * image->component[component].v + y))
                                return 0;
                }
                if (!jpeg_restart(image))
                    return 1;
            }
    }
    return 1;
}

/* Decode all scans of an image. Return zero for a bad header, marker or entropy block. */
int decode_jpeg_image(JpegImage *image)
{
    unsigned marker;
    image->restart_interval = 0;
    if (!decode_jpeg_header(image, 0))
        return 0;
    marker = jpeg_marker(image);
    while (marker != JPEG_EOI) {
        if (marker == JPEG_SOS) {
            if (!process_scan_header(image) || !parse_entropy_coded_data(image))
                return 0;
        } else if (!process_marker(image, marker)) {
            return 0;
        }
        marker = jpeg_marker(image);
    }
    return 1;
}

/* Convert a row to RGB with opaque alpha. Every pixel writes four bytes. */
void YCbCr_to_RGB_row(uint8_t *output, const uint8_t *y, const uint8_t *cb, const uint8_t *cr,
                      int count, int stride)
{
    int index;
    for (index = 0; index < count; index++, output += stride) {
        int luminance = y[index] * 65536 + 32768;
        int red_difference = cr[index] - 128, blue_difference = cb[index] - 128;
        output[0] = jpeg_clamp((luminance + red_difference * 91881) >> 16);
        output[1] = jpeg_clamp((luminance - red_difference * 46802 - blue_difference * 22554) >> 16);
        output[2] = jpeg_clamp((luminance + blue_difference * 116130) >> 16);
        output[3] = 255;
    }
}

/* Release component allocations and line buffers. Clear only their active pointers. */
void cleanup_jpeg(JpegImage *image)
{
    int component;
    for (component = 0; component < image->reader.channels; component++) {
        JpegComponent *part = &image->component[component];
        if (part->data) {
            os_free(part->allocation);
            part->data = NULL;
        }
        if (part->line) {
            os_free(part->line);
            part->line = NULL;
        }
    }
}
